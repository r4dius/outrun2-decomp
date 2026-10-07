#include "metal_d3d9.hpp"
#include "pc_metal_shaders.hpp"
#import <Metal/Metal.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <map>
#include <sstream>
#include <stdexcept>
namespace outrun::mac {
using namespace platform::d3d9;
namespace {
struct alignas(16) Float4 {float v[4]{};};
struct alignas(16) Int4 {std::int32_t v[4]{};};
struct alignas(16) Uniforms {
    Float4 pc[8];Int4 program[4],info,kinds[2];Float4 bump[8],lum[2];
    Int4 op[4],arg[4],misc[4];Float4 konst[4],tfactor;Int4 flags;
    Float4 fog_colour,fog_params,alpha_test;float lod_bias[6]{};
};
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void metal_error(NSError* e,const char* operation){throw std::runtime_error(std::string(operation)+": "+(e?e.localizedDescription.UTF8String:"Metal returned nil"));}
Float4 colour(std::uint32_t c){return {{float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255}};}
MTLCompareFunction compare(unsigned v){require(v>=1&&v<=8,"Invalid D3D9 comparison");return MTLCompareFunction(v-1);}
MTLStencilOperation stencil(unsigned v){
    const MTLStencilOperation ops[]{MTLStencilOperationKeep,MTLStencilOperationKeep,MTLStencilOperationZero,MTLStencilOperationReplace,MTLStencilOperationIncrementClamp,MTLStencilOperationDecrementClamp,MTLStencilOperationInvert,MTLStencilOperationIncrementWrap,MTLStencilOperationDecrementWrap};
    require(v<=8,"Invalid D3D9 stencil operation");return ops[v];
}
MTLBlendFactor blend(unsigned v){
    const MTLBlendFactor factors[]{MTLBlendFactorOne,MTLBlendFactorZero,MTLBlendFactorOne,MTLBlendFactorSourceColor,MTLBlendFactorOneMinusSourceColor,MTLBlendFactorSourceAlpha,MTLBlendFactorOneMinusSourceAlpha,MTLBlendFactorDestinationAlpha,MTLBlendFactorOneMinusDestinationAlpha,MTLBlendFactorDestinationColor,MTLBlendFactorOneMinusDestinationColor,MTLBlendFactorSourceAlphaSaturated};
    require(v>=1&&v<=11,"Unsupported D3D9 blend factor");return factors[v];
}
MTLBlendOperation blend_op(unsigned v){require(v>=1&&v<=5,"Invalid D3D9 blend operation");return MTLBlendOperation(v-1);}
MTLSamplerAddressMode address(unsigned v){
    switch(v){case 1:return MTLSamplerAddressModeRepeat;case 2:return MTLSamplerAddressModeMirrorRepeat;case 3:return MTLSamplerAddressModeClampToEdge;case 4:return MTLSamplerAddressModeClampToBorderColor;case 5:return MTLSamplerAddressModeMirrorClampToEdge;default:throw std::runtime_error("Unknown D3D9 sampler address mode");}
}
}
struct MetalD3D9Device::Impl {
    MetalD3D9Device& owner;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLLibrary> library;
    id<MTLCommandBuffer> command;
    id<MTLRenderCommandEncoder> encoder;
    id<MTLTexture> back,back_depth,dummy2d,dummycube;
    id<MTLTexture> scene_copy;               // FXAA source (port enhancement)
    id<MTLTexture> msaa_colour,msaa_depth;   // multisampled scene targets (port enhancement)
    id<MTLRenderPipelineState> fxaa;
    id<MTLSamplerState> fxaa_sampler;
    id<MTLBuffer> visibility;
    id<MTLBuffer> default_vertex;
    std::map<std::uint32_t,id<MTLBuffer>> buffers;
    std::map<std::uint32_t,id<MTLTexture>> textures,depths;
    std::map<std::string,id<MTLRenderPipelineState>> pipelines;
    std::map<std::string,id<MTLDepthStencilState>> depth_states;
    std::map<std::string,id<MTLSamplerState>> samplers;
    std::vector<id<MTLBuffer>> vertex_buffers;
    NSUInteger vertex_offset{};
    unsigned vertex_buffer_index{};
    unsigned samples{};
    std::uint64_t gpu_ns{};
    explicit Impl(MetalD3D9Device& d):owner(d){}
    id<MTLBuffer> buffer(unsigned h){
        auto* o=owner.object(h);require(o&&o->kind==1&&!o->bytes.empty(),"Invalid Metal geometry buffer");
        auto& b=buffers[h];
        // A buffer changed during command recording needs a new snapshot:
        // earlier GPU draws must retain the bytes they originally referenced.
        if(!b||b.length!=o->bytes.size()||(h<owner.buffer_dirty_.size()&&owner.buffer_dirty_[h])){
            b=[device newBufferWithBytes:o->bytes.data() length:o->bytes.size() options:MTLResourceStorageModeShared];
            require(b!=nil,"Cannot allocate Metal geometry buffer");
        }
        if(h<owner.buffer_dirty_.size())owner.buffer_dirty_[h]=false;return b;
    }
    void finish_encoder(){if(encoder){[encoder endEncoding];encoder=nil;}}
    void start(){if(!command){command=[queue commandBuffer];require(command!=nil,"Cannot allocate Metal command buffer");samples=0;std::memset(visibility.contents,0,visibility.length);vertex_offset=0;vertex_buffer_index=0;}}
    id<MTLBuffer> upload_vertices(const void* source,NSUInteger bytes,NSUInteger& offset){
        const NSUInteger padded=(bytes+255)&~NSUInteger(255);
        if(vertex_buffer_index<vertex_buffers.size()&&vertex_offset+padded>vertex_buffers[vertex_buffer_index].length){++vertex_buffer_index;vertex_offset=0;}
        if(vertex_buffer_index>=vertex_buffers.size()){
            auto b=[device newBufferWithLength:std::max(NSUInteger(4*1024*1024),padded) options:MTLResourceStorageModeShared];
            require(b!=nil,"Cannot allocate Metal vertex arena");vertex_buffers.push_back(b);
        }else if(padded>vertex_buffers[vertex_buffer_index].length){
            auto b=[device newBufferWithLength:padded options:MTLResourceStorageModeShared];require(b!=nil,"Cannot grow Metal vertex arena");vertex_buffers[vertex_buffer_index]=b;
        }
        auto b=vertex_buffers[vertex_buffer_index];offset=vertex_offset;
        std::memcpy(static_cast<char*>(b.contents)+offset,source,bytes);vertex_offset+=padded;return b;
    }
    void finish(){
        finish_encoder();if(!command)return;
        [command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)metal_error(command.error,"D3D9 Metal command");
        const auto* values=static_cast<const std::uint64_t*>(visibility.contents);
        for(unsigned i=0;i<samples;++i)owner.pixels_shaded+=values[i];
        const double elapsed=command.GPUEndTime-command.GPUStartTime;
        if(elapsed>0)gpu_ns+=std::uint64_t(elapsed*1e9);
        command=nil;samples=0;
    }
    id<MTLTexture> texture(std::uint32_t handle){
        auto* o=owner.object(handle);if(!o||o->kind!=5)return nil;
        if(auto it=textures.find(handle);it!=textures.end())return it->second;
        const auto& d=o->texture;require(!d.faces.empty()&&!d.faces[0].empty(),"Empty D3D9 texture");
        auto td=[MTLTextureDescriptor new];td.textureType=d.cube?MTLTextureTypeCube:MTLTextureType2D;
        td.width=d.faces[0][0].width;td.height=d.faces[0][0].height;td.mipmapLevelCount=d.levels;
        td.pixelFormat=MTLPixelFormatRGBA8Unorm;td.storageMode=MTLStorageModeShared;
        td.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;
        id<MTLTexture> t=[device newTextureWithDescriptor:td];require(t!=nil,"Cannot allocate Metal texture");
        for(unsigned face=0;face<o->rgba.size();++face)for(unsigned level=0;level<o->rgba[face].size();++level){
            const auto& size=d.faces[face][level];const auto& data=o->rgba[face][level];
            // Binding a CPU render target moves its storage into the active
            // framebuffer. The GPU attachment owns that level independently.
            if(data.empty())continue;
            require(data.size()==std::size_t(size.width)*size.height*4,"Invalid RGBA texture level");
            [t replaceRegion:MTLRegionMake2D(0,0,size.width,size.height) mipmapLevel:level slice:face withBytes:data.data() bytesPerRow:std::size_t(size.width)*4 bytesPerImage:data.size()];
        }
        textures.emplace(handle,t);return t;
    }
    id<MTLTexture> depth(std::uint32_t handle){
        if(handle==owner.DepthBuffer)return back_depth;if(!handle)return nil;
        if(auto it=depths.find(handle);it!=depths.end())return it->second;
        auto* o=owner.object(handle);require(o&&o->kind==6&&!o->parent,"Invalid Metal depth surface");
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8 width:o->width height:o->height mipmapped:NO];
        td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget;
        auto t=[device newTextureWithDescriptor:td];require(t!=nil,"Cannot allocate depth/stencil texture");depths.emplace(handle,t);return t;
    }
    MTLRenderPassDescriptor* pass(unsigned flags=0,std::uint32_t c=0,float z=1,unsigned s=0){
        auto p=[MTLRenderPassDescriptor renderPassDescriptor];
        const bool msaa=owner.colour_target_==owner.BackBuffer&&owner.msaa_live_;
        id<MTLTexture> target=msaa?msaa_colour:back;unsigned level=0,slice=0;
        if(owner.colour_target_!=owner.BackBuffer){auto* o=owner.object(owner.colour_target_);require(o&&o->parent,"Invalid Metal colour surface");target=texture(o->parent);level=o->level;slice=o->face;}
        require(target!=nil,"Metal colour attachment missing");
        p.colorAttachments[0].texture=target;p.colorAttachments[0].level=level;p.colorAttachments[0].slice=slice;
        p.colorAttachments[0].loadAction=flags&1?MTLLoadActionClear:MTLLoadActionLoad;
        p.colorAttachments[0].storeAction=MTLStoreActionStore;
        const auto col=colour(c);p.colorAttachments[0].clearColor=MTLClearColorMake(col.v[0],col.v[1],col.v[2],col.v[3]);
        auto d=depth(owner.depth_target_);
        if(msaa){require(owner.depth_target_==owner.DepthBuffer||!d,"MSAA scene with a separate depth surface");if(d)d=msaa_depth;}
        if(d){
            require(d.width>=std::max(NSUInteger(1),target.width>>level)&&d.height>=std::max(NSUInteger(1),target.height>>level),"Metal depth attachment smaller than target");
            p.depthAttachment.texture=d;p.depthAttachment.loadAction=flags&2?MTLLoadActionClear:MTLLoadActionLoad;p.depthAttachment.storeAction=MTLStoreActionStore;p.depthAttachment.clearDepth=z;
            p.stencilAttachment.texture=d;p.stencilAttachment.loadAction=flags&4?MTLLoadActionClear:MTLLoadActionLoad;p.stencilAttachment.storeAction=MTLStoreActionStore;p.stencilAttachment.clearStencil=s&255;
        }else require(!(flags&6),"Clear requests missing Metal depth/stencil surface");
        p.visibilityResultBuffer=visibility;return p;
    }
    id<MTLRenderCommandEncoder> render_encoder(){
        start();if(!encoder){encoder=[command renderCommandEncoderWithDescriptor:pass()];require(encoder!=nil,"Cannot create Metal render encoder");}return encoder;
    }
    void allocate_back(unsigned w,unsigned h){
        require(w>0&&h>0&&w<=16384&&h<=16384,"Invalid Metal back buffer size");
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget;back=[device newTextureWithDescriptor:td];
        td.pixelFormat=MTLPixelFormatDepth32Float_Stencil8;td.storageMode=MTLStorageModePrivate;back_depth=[device newTextureWithDescriptor:td];
        require(back&&back_depth,"Cannot allocate Metal back buffers");
        scene_copy=nil;msaa_colour=msaa_depth=nil;
    }
    void ensure_msaa(unsigned samples){
        if(msaa_colour&&msaa_colour.sampleCount==samples&&msaa_colour.width==back.width&&msaa_colour.height==back.height)return;
        require([device supportsTextureSampleCount:samples],"Metal GPU lacks this MSAA sample count");
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:back.width height:back.height mipmapped:NO];
        td.textureType=MTLTextureType2DMultisample;td.sampleCount=samples;td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageRenderTarget;
        msaa_colour=[device newTextureWithDescriptor:td];
        td.pixelFormat=MTLPixelFormatDepth32Float_Stencil8;msaa_depth=[device newTextureWithDescriptor:td];
        require(msaa_colour&&msaa_depth,"Cannot allocate the MSAA scene targets");
    }
    // The multisampled scene into the back buffer, whose depth/stencil is
    // cleared for the 2D layer drawn there next.
    void resolve_msaa(){
        finish_encoder();start();
        auto p=[MTLRenderPassDescriptor renderPassDescriptor];
        p.colorAttachments[0].texture=msaa_colour;p.colorAttachments[0].loadAction=MTLLoadActionLoad;
        p.colorAttachments[0].storeAction=MTLStoreActionMultisampleResolve;p.colorAttachments[0].resolveTexture=back;
        auto e=[command renderCommandEncoderWithDescriptor:p];require(e!=nil,"Cannot encode the MSAA resolve");[e endEncoding];
        p=[MTLRenderPassDescriptor renderPassDescriptor];
        p.colorAttachments[0].texture=back;p.colorAttachments[0].loadAction=MTLLoadActionLoad;p.colorAttachments[0].storeAction=MTLStoreActionStore;
        p.depthAttachment.texture=back_depth;p.depthAttachment.loadAction=MTLLoadActionClear;p.depthAttachment.storeAction=MTLStoreActionStore;p.depthAttachment.clearDepth=1;
        p.stencilAttachment.texture=back_depth;p.stencilAttachment.loadAction=MTLLoadActionClear;p.stencilAttachment.storeAction=MTLStoreActionStore;p.stencilAttachment.clearStencil=0;
        e=[command renderCommandEncoderWithDescriptor:p];require(e!=nil,"Cannot encode the depth clear");[e endEncoding];
    }
};
// Port post-process (not original code): switch/shaders/fxaa_fsh.glsl in MSL,
// over the whole back buffer before the 2D layer.
constexpr const char* FxaaSource=R"(#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
vertex V fxaa_vs(uint i [[vertex_id]]){ float2 p=float2(float((i<<1)&2),float(i&2)); V v; v.p=float4(p*2.0-1.0,0.0,1.0); return v; }
static float4 at(texture2d<float> t,sampler s,float2 q,float2 size){ q=clamp(q,float2(0.5),size-0.5); return t.sample(s,q/size,level(0.0)); }
static float luma(float3 c){ return dot(c,float3(0.299,0.587,0.114)); }
fragment float4 fxaa_fs(V v [[stage_in]],texture2d<float> scene [[texture(0)]],sampler s [[sampler(0)]]){
    const float2 size=float2(scene.get_width(),scene.get_height());
    const float2 q=v.p.xy;
    const float4 cM=at(scene,s,q,size);
    const float lNW=luma(at(scene,s,q+float2(-1.0,-1.0),size).rgb),lNE=luma(at(scene,s,q+float2(1.0,-1.0),size).rgb);
    const float lSW=luma(at(scene,s,q+float2(-1.0,1.0),size).rgb),lSE=luma(at(scene,s,q+float2(1.0,1.0),size).rgb);
    const float lM=luma(cM.rgb);
    const float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE)));
    const float lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    float2 dir=float2(-((lNW+lNE)-(lSW+lSE)),(lNW+lSW)-(lNE+lSE));
    const float reduce=max((lNW+lNE+lSW+lSE)*(0.25/8.0),1.0/128.0);
    const float rcpMin=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
    dir=clamp(dir*rcpMin,float2(-8.0),float2(8.0));
    const float3 a=0.5*(at(scene,s,q+dir*(1.0/3.0-0.5),size).rgb+at(scene,s,q+dir*(2.0/3.0-0.5),size).rgb);
    const float3 b=a*0.5+0.25*(at(scene,s,q+dir*-0.5,size).rgb+at(scene,s,q+dir*0.5,size).rgb);
    const float lB=luma(b);
    const float3 aa=(lB<lMin||lB>lMax)?a:b;
    return float4((lMax-lMin<max(0.0312,lMax*0.125))?cM.rgb:aa,cM.a);
}
)";
MetalD3D9Device::MetalD3D9Device(unsigned w,unsigned h,unsigned scale):PcSoftD3D9Device(w,h),metal_(std::make_unique<Impl>(*this)){
    @autoreleasepool {
        require(scale>=1&&scale<=4,"Metal render scale must be between 1 and 4");
        static_assert(sizeof(Vertex)==180,"MSL packed vertex layout");
        auto& m=*metal_;m.device=MTLCreateSystemDefaultDevice();require(m.device!=nil,"No Metal GPU available");
        m.queue=[m.device newCommandQueue];require(m.queue!=nil,"Cannot create Metal queue");
        NSError* error=nil;auto options=[MTLCompileOptions new];options.fastMathEnabled=NO;
        m.library=[m.device newLibraryWithSource:[NSString stringWithUTF8String:PcMetalSource] options:options error:&error];
        if(!m.library)metal_error(error,"Recovered MSL shader compilation");
        m.allocate_back(w*scale,h*scale);
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
        m.dummy2d=[m.device newTextureWithDescriptor:td];td.textureType=MTLTextureTypeCube;m.dummycube=[m.device newTextureWithDescriptor:td];
        require(m.dummy2d&&m.dummycube,"Cannot allocate Metal placeholder textures");
        const unsigned char black[]{0,0,0,255};
        [m.dummy2d replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:black bytesPerRow:4];
        for(unsigned face=0;face<6;++face)[m.dummycube replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 slice:face withBytes:black bytesPerRow:4 bytesPerImage:4];
        m.visibility=[m.device newBufferWithLength:65536*8 options:MTLResourceStorageModeShared];require(m.visibility!=nil,"Cannot allocate Metal visibility results");
        const float defaults[]{0,0,0,1};m.default_vertex=[m.device newBufferWithBytes:defaults length:sizeof defaults options:MTLResourceStorageModeShared];require(m.default_vertex!=nil,"Cannot allocate default vertex attributes");
        id<MTLLibrary> fxaa=[m.device newLibraryWithSource:[NSString stringWithUTF8String:FxaaSource] options:options error:&error];
        if(!fxaa)metal_error(error,"FXAA shader compilation");
        auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[fxaa newFunctionWithName:@"fxaa_vs"];pd.fragmentFunction=[fxaa newFunctionWithName:@"fxaa_fs"];
        pd.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA8Unorm;
        m.fxaa=[m.device newRenderPipelineStateWithDescriptor:pd error:&error];if(!m.fxaa)metal_error(error,"FXAA pipeline");
        auto sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=MTLSamplerMinMagFilterLinear;sd.sAddressMode=sd.tAddressMode=MTLSamplerAddressModeClampToEdge;
        m.fxaa_sampler=[m.device newSamplerStateWithDescriptor:sd];require(m.fxaa_sampler!=nil,"Cannot create FXAA sampler");
    }
}
MetalD3D9Device::~MetalD3D9Device(){try{metal_->finish();}catch(const std::exception& e){std::fprintf(stderr,"Metal shutdown: %s\n",e.what());}}
void MetalD3D9Device::begin_frame(){
    metal_->finish();metal_->gpu_ns=0;scene_finished_=false;
    msaa_live_=msaa_>1&&!count_only;
    if(msaa_live_)@autoreleasepool {metal_->ensure_msaa(msaa_);}
    if(!count_only)metal_->start();
}
void MetalD3D9Device::resize_back_buffer(unsigned w,unsigned h){
    if(w==presentation_width()&&h==presentation_height())return;
    @autoreleasepool {metal_->finish();metal_->allocate_back(w,h);}
}
void MetalD3D9Device::finish_scene(){
    if(scene_finished_||count_only||colour_target_!=BackBuffer)return;
    if(msaa_live_){@autoreleasepool {metal_->resolve_msaa();}msaa_live_=false;++msaa_resolves;scene_finished_=true;return;}
    if(!fxaa_)return;
    scene_finished_=true;
    @autoreleasepool {
        auto& m=*metal_;m.finish_encoder();m.start();
        if(!m.scene_copy){
            auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:m.back.width height:m.back.height mipmapped:NO];
            td.storageMode=MTLStorageModePrivate;td.usage=MTLTextureUsageShaderRead;
            m.scene_copy=[m.device newTextureWithDescriptor:td];require(m.scene_copy!=nil,"Cannot allocate the FXAA source");
        }
        auto blit=[m.command blitCommandEncoder];require(blit!=nil,"Cannot encode the FXAA copy");
        [blit copyFromTexture:m.back toTexture:m.scene_copy];[blit endEncoding];
        auto p=[MTLRenderPassDescriptor renderPassDescriptor];
        p.colorAttachments[0].texture=m.back;p.colorAttachments[0].loadAction=MTLLoadActionDontCare;p.colorAttachments[0].storeAction=MTLStoreActionStore;
        auto e=[m.command renderCommandEncoderWithDescriptor:p];require(e!=nil,"Cannot encode FXAA");
        [e setRenderPipelineState:m.fxaa];[e setFragmentTexture:m.scene_copy atIndex:0];[e setFragmentSamplerState:m.fxaa_sampler atIndex:0];
        [e setViewport:MTLViewport{0,0,double(m.back.width),double(m.back.height),0,1}];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[e endEncoding];
        ++fxaa_passes;
    }
}
void MetalD3D9Device::end_frame(){
    if(msaa_live_){const auto target=colour_target_;colour_target_=BackBuffer;finish_scene();colour_target_=target;}   // no 2D layer this frame
    metal_->finish();readback();
}
void MetalD3D9Device::readback(){
    metal_->finish();
    auto& destination=colour_target_==BackBuffer?colour_:back_colour_;
    const auto w=presentation_width(),h=presentation_height();
    destination.resize(std::size_t(w)*h*4);
    [metal_->back getBytes:destination.data() bytesPerRow:std::size_t(w)*4 fromRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0];
}
unsigned MetalD3D9Device::presentation_width()const{return unsigned(metal_->back.width);}
unsigned MetalD3D9Device::presentation_height()const{return unsigned(metal_->back.height);}
std::uint64_t MetalD3D9Device::gpu_nanoseconds()const{return metal_->gpu_ns;}
std::string MetalD3D9Device::device_name()const{return metal_->device.name.UTF8String;}
void MetalD3D9Device::set_render_target(std::uint32_t i,std::uint32_t h){metal_->finish_encoder();PcSoftD3D9Device::set_render_target(i,h);}
void MetalD3D9Device::set_depth_stencil_surface(std::uint32_t h){metal_->finish_encoder();PcSoftD3D9Device::set_depth_stencil_surface(h);}
void MetalD3D9Device::release(std::uint32_t h){
    PcSoftD3D9Device::release(h);
    // Most releases only drop a reference (set_stream_source, set_texture): nothing was destroyed.
    if(object(h))return;
    // Releasing a surface can recursively release its parent texture.
    for(auto it=metal_->textures.begin();it!=metal_->textures.end();)if(!object(it->first))it=metal_->textures.erase(it);else ++it;
    for(auto it=metal_->depths.begin();it!=metal_->depths.end();)if(!object(it->first))it=metal_->depths.erase(it);else ++it;
    for(auto it=metal_->buffers.begin();it!=metal_->buffers.end();)if(!object(it->first))it=metal_->buffers.erase(it);else ++it;
}
std::uint8_t* MetalD3D9Device::lock(std::uint32_t h,std::uint32_t offset,std::uint32_t size,std::uint32_t flags){
    auto* p=PcSoftD3D9Device::lock(h,offset,size,flags);
    if(p){if(buffer_dirty_.size()<=h)buffer_dirty_.resize(h+1);buffer_dirty_[h]=true;}return p;
}
void MetalD3D9Device::unlock_rect(std::uint32_t h){
    metal_->finish();PcSoftD3D9Device::unlock_rect(h);auto* s=object(h);if(!s||!s->parent)return;
    auto found=metal_->textures.find(s->parent);if(found==metal_->textures.end())return;
    const auto* parent=object(s->parent);const auto& data=parent->rgba[s->face][s->level];
    [found->second replaceRegion:MTLRegionMake2D(0,0,s->width,s->height) mipmapLevel:s->level slice:s->face withBytes:data.data() bytesPerRow:std::size_t(s->width)*4 bytesPerImage:data.size()];
}
void MetalD3D9Device::clear(std::uint32_t flags,std::uint32_t c,float z,std::uint32_t s){
    if(count_only)return;auto& m=*metal_;m.finish_encoder();m.start();
    auto e=[m.command renderCommandEncoderWithDescriptor:m.pass(flags,c,z,s)];require(e!=nil,"Cannot encode Metal clear");[e endEncoding];
}
void MetalD3D9Device::raster(const Vertex* v,unsigned n){
    if(n<3)return;
    batch_rhw_=pretransformed_;
    auto push=[&](const Vertex& input){auto o=input;
        if(pretransformed_){const float w=1.f/o.pos[3];o.pos[0]=(2*(o.pos[0]+.5f-soft_viewport_.x)/soft_viewport_.w-1)*w;o.pos[1]=(1-2*(o.pos[1]+.5f-soft_viewport_.y)/soft_viewport_.h)*w;o.pos[2]*=w;o.pos[3]=w;}
        else{o.pos[0]+=o.pos[3]/soft_viewport_.w;o.pos[1]-=o.pos[3]/soft_viewport_.h;}
        batch_.push_back(o);};
    for(unsigned i=1;i+1<n;++i){push(v[0]);push(v[i]);push(v[i+1]);++triangles_drawn;}
}
void MetalD3D9Device::draw_primitive_up(std::uint32_t t,std::uint32_t n,const void* d,std::uint32_t s){
    native_vertices_=false;batch_.clear();
    if(t!=1){PcSoftD3D9Device::draw_primitive_up(t,n,d,s);flush();return;}
    if(!d||!n||count_only)return;
    // Retail 41A6F0 / 41AE40: XYZ + D3DCOLOR point sprites (FVF 0x42).
    // Expand in clip space so the existing Metal pixel path preserves all
    // material, alpha-test, blending and depth state. No effect is discarded.
    require(fvf_==0x42&&s>=16&&vertex_shader==0,"Unsupported point sprite vertex layout");
    const auto matrix=transform_wvp();
    auto transform=[](const float* v,const std::array<float,16>& m){
        std::array<float,4> out{};
        for(unsigned k=0;k<4;++k)out[k]=v[0]*m[k]+v[1]*m[4+k]+v[2]*m[8+k]+v[3]*m[12+k];
        return out;
    };
    const auto* bytes=static_cast<const std::uint8_t*>(d);
    const float minimum=render[155]?f(render[155]):1.f;
    const float maximum=render[166]?f(render[166]):64.f;
    require(std::isfinite(minimum)&&std::isfinite(maximum)&&minimum>=0&&maximum>=minimum,"Invalid point sprite size limits");
    pretransformed_=false;
    for(unsigned i=0;i<n;++i){
        float xyz[4]{0,0,0,1};std::uint32_t c;
        std::memcpy(xyz,bytes+std::size_t(i)*s,12);std::memcpy(&c,bytes+std::size_t(i)*s+12,4);
        const auto clip=transform(xyz,matrix);
        // D3D points clip their centre before expanding, including z and w.
        if(!(clip[3]>0)||clip[2]<0||clip[2]>clip[3]||std::abs(clip[0])>clip[3]||std::abs(clip[1])>clip[3])continue;
        float size=render[154]?f(render[154]):1.f;
        if(render[157]){
            const auto world=transform(xyz,transform_world);
            const auto eye=transform(world.data(),transform_view);
            const float distance=std::sqrt(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
            const float denominator=f(render[158])+f(render[159])*distance+f(render[160])*distance*distance;
            size=denominator>0?soft_viewport_.h*size/std::sqrt(denominator):maximum;
        }
        require(std::isfinite(size),"Invalid point sprite size");size=std::clamp(size,minimum,maximum);
        const float dx=size*clip[3]/soft_viewport_.w,dy=size*clip[3]/soft_viewport_.h;
        Vertex q[4]{};
        for(unsigned k=0;k<4;++k){
            auto& v=q[k];std::copy(clip.begin(),clip.end(),v.pos);
            v.pos[0]+=(k&1)?dx:-dx;v.pos[1]+=(k&2)?-dy:dy;
            v.d[0]={float((c>>16)&255)/255.f,float((c>>8)&255)/255.f,float(c&255)/255.f,float(c>>24)/255.f};v.fog=1;
            for(auto& uv:v.t)uv=render[156]?platform::pc_shader::V4{float(k&1),float((k>>1)&1),0,1}:platform::pc_shader::V4{0,0,0,1};
        }
        const Vertex a[]{q[0],q[1],q[2]},b[]{q[2],q[1],q[3]};raster(a,3);raster(b,3);
    }
    const auto cull=render[RS_CULLMODE];render[RS_CULLMODE]=CULL_NONE;
    flush();render[RS_CULLMODE]=cull;
}
void MetalD3D9Device::flush(){
    if((batch_.empty()&&!native_vertices_)||count_only)return;
    @autoreleasepool {
        auto& m=*metal_;const auto* ps=object(pixel_shader);
        unsigned kind=0;
        if(ps&&ps->kind==4){
            require(ps->program.version==0xffff0101u||ps->program.version==0xffff0104u,"Unsupported Metal pixel shader version");
            require(ps->program.blobs.size()<=16,"Metal pixel program exceeds retained 16-blob layout");kind=ps->program.version==0xffff0101u?1:2;
        }
        const bool has_depth=depth_target_!=0;
        const bool separate=render[RS_SEPARATEALPHABLENDENABLE]!=0;
        std::string key;auto word=[&](unsigned v){key.append(reinterpret_cast<const char*>(&v),sizeof v);};
        const unsigned samples=colour_target_==BackBuffer&&msaa_live_?msaa_:1;
        word(kind);word(has_depth);word(samples);
        word(native_vertices_);
        const auto* vs=object(vertex_shader);const auto* decl=object(declaration);
        if(native_vertices_){
            word(unsigned(vs->program.blobs.size()));for(auto blob:vs->program.blobs)word(blob);
            key.append(reinterpret_cast<const char*>(decl->elements.data()),decl->elements.size()*sizeof(platform::PcVertexElement));
            for(const auto& stream:streams)word(stream.stride);
        }
        // Specialise the retained instruction list so Metal can eliminate
        // unreachable interpreter branches, while material state stays dynamic.
        if(kind){word(unsigned(ps->program.blobs.size()));for(auto blob:ps->program.blobs)word(blob);}
        for(auto state:{RS_COLORWRITEENABLE,RS_ALPHABLENDENABLE,RS_SRCBLEND,RS_DESTBLEND,RS_BLENDOP,RS_SEPARATEALPHABLENDENABLE,RS_SRCBLENDALPHA,RS_DESTBLENDALPHA,RS_BLENDOPALPHA})word(render[state]);
        auto pipeline=m.pipelines.find(key);
        if(pipeline==m.pipelines.end()){
            auto d=[MTLRenderPipelineDescriptor new];
            if(!native_vertices_)d.vertexFunction=[m.library newFunctionWithName:@"pc_vertex"];
            else{
                auto constants=[MTLFunctionConstantValues new];
                std::int32_t instructions[32];std::fill(std::begin(instructions),std::end(instructions),-1);
                for(unsigned i=0;i<vs->program.blobs.size();++i)instructions[i]=int(vs->program.blobs[i]);
                for(unsigned i=0;i<8;++i)[constants setConstantValue:instructions+4*i type:MTLDataTypeInt4 atIndex:16+i];
                NSError* error=nil;d.vertexFunction=[m.library newFunctionWithName:@"pc_native_vertex" constantValues:constants error:&error];
                if(!d.vertexFunction)metal_error(error,"Metal vertex program specialization");
                auto layout=[MTLVertexDescriptor vertexDescriptor];
                for(unsigned reg=0;reg<16;++reg){layout.attributes[reg].format=MTLVertexFormatFloat4;layout.attributes[reg].bufferIndex=4;}
                layout.layouts[4].stride=16;layout.layouts[4].stepFunction=MTLVertexStepFunctionConstant;layout.layouts[4].stepRate=0;
                const MTLVertexFormat formats[]{MTLVertexFormatFloat,MTLVertexFormatFloat2,MTLVertexFormatFloat3,MTLVertexFormatFloat4,MTLVertexFormatUChar4Normalized_BGRA,MTLVertexFormatUChar4};
                for(const auto& field:decl->elements){
                    if(field.stream==platform::PcDeclEndStream)break;
                    for(const auto& input:vs->program.inputs)if(field.usage==input.usage&&field.usage_index==input.usage_index){
                        require(input.reg<16&&field.type<6&&field.stream<4,"Unsupported Metal vertex declaration");
                        auto a=layout.attributes[input.reg];a.format=formats[field.type];a.offset=field.offset;a.bufferIndex=field.stream;
                        layout.layouts[field.stream].stride=streams[field.stream].stride;layout.layouts[field.stream].stepFunction=MTLVertexStepFunctionPerVertex;
                    }
                }d.vertexDescriptor=layout;
            }
            if(!kind)d.fragmentFunction=[m.library newFunctionWithName:@"pc_ffp"];
            else{
                auto constants=[MTLFunctionConstantValues new];
                std::int32_t instructions[16];std::fill(std::begin(instructions),std::end(instructions),-1);
                for(unsigned i=0;i<ps->program.blobs.size();++i)instructions[i]=int(ps->program.blobs[i]);
                for(unsigned i=0;i<4;++i)[constants setConstantValue:instructions+4*i type:MTLDataTypeInt4 atIndex:i];
                NSError* error=nil;
                d.fragmentFunction=[m.library newFunctionWithName:kind==1?@"pc_ps11":@"pc_ps14" constantValues:constants error:&error];
                if(!d.fragmentFunction)metal_error(error,"Metal pixel program specialization");
            }
            auto c=d.colorAttachments[0];c.pixelFormat=MTLPixelFormatRGBA8Unorm;
            const auto mask=render[RS_COLORWRITEENABLE];c.writeMask=MTLColorWriteMaskNone;
            if(mask&1)c.writeMask|=MTLColorWriteMaskRed;if(mask&2)c.writeMask|=MTLColorWriteMaskGreen;if(mask&4)c.writeMask|=MTLColorWriteMaskBlue;if(mask&8)c.writeMask|=MTLColorWriteMaskAlpha;
            c.blendingEnabled=render[RS_ALPHABLENDENABLE]!=0;
            if(c.blendingEnabled){
                c.sourceRGBBlendFactor=blend(render[RS_SRCBLEND]);c.destinationRGBBlendFactor=blend(render[RS_DESTBLEND]);c.rgbBlendOperation=blend_op(render[RS_BLENDOP]);
                c.sourceAlphaBlendFactor=blend(render[separate?RS_SRCBLENDALPHA:RS_SRCBLEND]);c.destinationAlphaBlendFactor=blend(render[separate?RS_DESTBLENDALPHA:RS_DESTBLEND]);c.alphaBlendOperation=blend_op(render[separate?RS_BLENDOPALPHA:RS_BLENDOP]);
            }
            if(has_depth){d.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;d.stencilAttachmentPixelFormat=MTLPixelFormatDepth32Float_Stencil8;}
            d.rasterSampleCount=samples;
            NSError* error=nil;auto p=[m.device newRenderPipelineStateWithDescriptor:d error:&error];if(!p)metal_error(error,"Metal D3D9 pipeline");pipeline=m.pipelines.emplace(key,p).first;
        }
        key.clear();word(has_depth);
        for(unsigned state:{7u,14u,23u,52u,53u,54u,55u,56u,58u,59u})word(render[state]);
        auto depth_state=m.depth_states.find(key);
        if(depth_state==m.depth_states.end()){
            auto d=[MTLDepthStencilDescriptor new];d.depthCompareFunction=render[RS_ZENABLE]&&has_depth?compare(render[RS_ZFUNC]):MTLCompareFunctionAlways;
            d.depthWriteEnabled=render[RS_ZENABLE]&&render[RS_ZWRITEENABLE]&&has_depth;
            if(render[52]){
                require(has_depth,"Stencil enabled without a depth/stencil attachment");
                auto s=[MTLStencilDescriptor new];s.stencilCompareFunction=compare(render[56]);s.stencilFailureOperation=stencil(render[53]);s.depthFailureOperation=stencil(render[54]);s.depthStencilPassOperation=stencil(render[55]);s.readMask=render[58]&255;s.writeMask=render[59]&255;d.frontFaceStencil=s;d.backFaceStencil=s;
            }
            auto ds=[m.device newDepthStencilStateWithDescriptor:d];require(ds!=nil,"Cannot create Metal depth/stencil state");depth_state=m.depth_states.emplace(key,ds).first;
        }
        Uniforms u{};std::memcpy(u.pc,ps_constants.data(),sizeof u.pc);
        for(auto& row:u.program)for(auto& value:row.v)value=-1;
        if(kind)for(unsigned i=0;i<ps->program.blobs.size();++i)u.program[i/4].v[i%4]=int(ps->program.blobs[i]);
        if(kind)u.info.v[0]=int(ps->program.blobs.size());
        for(unsigned s=0;s<8;++s){const auto* t=object(textures[s]);u.kinds[s/4].v[s%4]=t&&t->kind==5&&t->texture.cube;
            for(unsigned k=0;k<4;++k)u.bump[s].v[k]=f(stage[s][7+k]);}
        for(unsigned s=0;s<4;++s){const auto& t=stage[s];
            u.lum[s/2].v[(s%2)*2]=f(t[22]);u.lum[s/2].v[(s%2)*2+1]=f(t[23]);
            u.op[s]={{int(t[1]),int(t[2]),int(t[3]),int(t[4])}};
            u.arg[s]={{int(t[5]),int(t[6]),int(t[26]),int(t[27])}};
            const auto* tx=object(textures[s]);u.misc[s]={{int(t[28]),int(t[11]&3),tx&&tx->kind==5&&!tx->texture.cube,tx&&tx->kind==5&&tx->texture.cube}};
            u.konst[s]=colour(t[32]);
        }
        u.tfactor=colour(render[RS_TEXTUREFACTOR]);u.flags.v[0]=render[RS_SPECULARENABLE]!=0;
        u.fog_colour=colour(render[RS_FOGCOLOR]);u.fog_colour.v[3]=f(render[RS_FOGEND]);
        u.fog_params={{render[RS_FOGENABLE]?float(render[RS_FOGTABLEMODE]?render[RS_FOGTABLEMODE]:4):0,f(render[RS_FOGSTART]),f(render[RS_FOGDENSITY]),1}};
        u.alpha_test={{render[RS_ALPHATESTENABLE]?1.f:0.f,float(render[RS_ALPHAREF]&255),float(render[RS_ALPHAFUNC]),0}};
        auto e=m.render_encoder();[e setRenderPipelineState:pipeline->second];[e setDepthStencilState:depth_state->second];[e setStencilReferenceValue:render[57]&255];
        [e setFrontFacingWinding:MTLWindingClockwise];[e setCullMode:render[RS_CULLMODE]==CULL_NONE?MTLCullModeNone:render[RS_CULLMODE]==CULL_CCW?MTLCullModeBack:MTLCullModeFront];
        [e setTriangleFillMode:render[RS_FILLMODE]==2?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
        require(render[RS_FILLMODE]==2||render[RS_FILLMODE]==3,"Metal point fill mode is not implemented");
        // The logical PC target stretched to the back buffer's pixels; with the
        // UI rectangle, pretransformed (2D) draws keep a centred 4:3 area.
        const bool back=colour_target_==BackBuffer;
        double sx=back?double(presentation_width())/back_width_:1,sy=back?double(presentation_height())/back_height_:1,ox=0;
        if(back&&ui_rect_&&batch_rhw_&&!native_vertices_){const double w=double(presentation_height())*4/3;ox=std::floor((presentation_width()-w)/2);sx=w/back_width_;}
        MTLViewport viewport{ox+soft_viewport_.x*sx,soft_viewport_.y*sy,soft_viewport_.w*sx,soft_viewport_.h*sy,soft_viewport_.min_z,soft_viewport_.max_z};[e setViewport:viewport];
        const unsigned left=unsigned(std::max(0.f,soft_viewport_.x)),top=unsigned(std::max(0.f,soft_viewport_.y));
        require(left<width_&&top<height_&&soft_viewport_.w>0&&soft_viewport_.h>0,"Metal viewport outside target");
        const unsigned right=left+std::min(width_-left,unsigned(soft_viewport_.w)),bottom=top+std::min(height_-top,unsigned(soft_viewport_.h));
        const NSUInteger tw=back?presentation_width():width_,th=back?presentation_height():height_;
        const auto px=[&](unsigned v){return std::min(tw,NSUInteger(std::lround(ox+v*sx)));};const auto py=[&](unsigned v){return std::min(th,NSUInteger(std::lround(v*sy)));};
        [e setScissorRect:MTLScissorRect{px(left),py(top),px(right)-px(left),py(bottom)-py(top)}];
        // Constant D3D depth bias is an absolute offset, unlike Metal's
        // format-dependent depthBias. Fold it into clip z; slope is native.
        for(auto& v:batch_)v.pos[2]+=f(render[RS_DEPTHBIAS])*v.pos[3];
        [e setDepthBias:0 slopeScale:f(render[RS_SLOPESCALEDEPTHBIAS]) clamp:0];
        for(unsigned s=0;s<6;++s){
            auto* o=object(textures[s]);auto texture=m.texture(textures[s]);const bool cube=o&&o->kind==5&&o->texture.cube;
            [e setFragmentTexture:texture&&!cube?texture:m.dummy2d atIndex:s];[e setFragmentTexture:texture&&cube?texture:m.dummycube atIndex:6+s];
            const auto& state=sampler[s];std::string sk(reinterpret_cast<const char*>(state.data()),state.size()*sizeof(state[0]));
            const unsigned levels=o&&o->kind==5?o->texture.levels:1;sk.append(reinterpret_cast<const char*>(&levels),sizeof levels);
            auto sample=m.samplers.find(sk);
            if(sample==m.samplers.end()){
                auto d=[MTLSamplerDescriptor new];d.sAddressMode=address(state[SAMP_ADDRESSU]);d.tAddressMode=address(state[SAMP_ADDRESSV]);d.rAddressMode=address(state[SAMP_ADDRESSW]);
                d.minFilter=state[SAMP_MINFILTER]==1?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
                d.magFilter=state[SAMP_MAGFILTER]==1?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
                d.mipFilter=state[SAMP_MIPFILTER]==0?MTLSamplerMipFilterNotMipmapped:state[SAMP_MIPFILTER]==1?MTLSamplerMipFilterNearest:MTLSamplerMipFilterLinear;
                d.lodMinClamp=float(std::min(state[SAMP_MAXMIPLEVEL],levels-1));
                d.maxAnisotropy=std::clamp(state[SAMP_MAXANISOTROPY],1u,16u);
                if(state[SAMP_ADDRESSU]==4||state[SAMP_ADDRESSV]==4||state[SAMP_ADDRESSW]==4){
                    switch(state[SAMP_BORDERCOLOR]){case 0:d.borderColor=MTLSamplerBorderColorTransparentBlack;break;case 0xff000000u:d.borderColor=MTLSamplerBorderColorOpaqueBlack;break;case 0xffffffffu:d.borderColor=MTLSamplerBorderColorOpaqueWhite;break;default:throw std::runtime_error("Custom D3D border colour requires shader emulation");}
                }
                auto value=[m.device newSamplerStateWithDescriptor:d];require(value!=nil,"Cannot create Metal sampler");sample=m.samplers.emplace(sk,value).first;
            }
            [e setFragmentSamplerState:sample->second atIndex:s];u.lod_bias[s]=f(state[SAMP_MIPMAPLODBIAS]);
        }
        // Metal copies set*Bytes immediately; large vertex batches need a buffer.
        if(native_vertices_){
            for(unsigned i=0;i<4;++i)if(streams[i].buffer)[e setVertexBuffer:m.buffer(streams[i].buffer) offset:streams[i].offset atIndex:i];
            [e setVertexBuffer:m.default_vertex offset:0 atIndex:4];
            [e setVertexBytes:vs_constants.data() length:sizeof vs_constants atIndex:5];
            const float viewport[]{soft_viewport_.w,soft_viewport_.h,f(render[RS_DEPTHBIAS]),0};
            [e setVertexBytes:viewport length:sizeof viewport atIndex:6];
        }else{
            NSUInteger vertex_offset=0;
            id<MTLBuffer> vertex_buffer=m.upload_vertices(batch_.data(),batch_.size()*sizeof(Vertex),vertex_offset);
            [e setVertexBuffer:vertex_buffer offset:vertex_offset atIndex:0];
        }
        [e setFragmentBytes:&u length:sizeof u atIndex:0];
        require(m.samples<65536,"Metal visibility query capacity exceeded");
        [e setVisibilityResultMode:MTLVisibilityResultModeCounting offset:std::size_t(m.samples++)*8];
        if(native_vertices_)[e drawIndexedPrimitives:native_type_==5?MTLPrimitiveTypeTriangleStrip:MTLPrimitiveTypeTriangle indexCount:native_count_ indexType:MTLIndexTypeUInt16 indexBuffer:m.buffer(indices) indexBufferOffset:std::size_t(native_start_)*2 instanceCount:1 baseVertex:native_base_ baseInstance:0];
        else [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:batch_.size()];batch_.clear();batch_rhw_=false;
    }
}
}
