#include "switch_d3d9.hpp"
#include "frame_profile.hpp"
#include "platform/pc_screen.hpp"
#include <map>
#include <cmath>
#include <cstdio>
#ifdef __SWITCH__
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace outrun::switch_runtime {
using namespace outrun::platform;
namespace {
enum Kind : std::uint32_t { KBuffer=1,KDecl,KVs,KPs,KTexture,KSurface };
constexpr std::uint32_t MaxImages=4096,MaxSamplers=1024;
// Uniform blocks (std140), see the generated ubershaders.
struct SpecEntry { std::uint32_t kind,count;std::uint32_t blobs[16];const char* path; };
const SpecEntry SpecTable[]={
#include "pc_spec_shaders.inc"
};
constexpr std::uint32_t UboVsConst=0,UboVsProgram=0x1000,UboVsFix=0x1100,UboPsConst=0x1200,UboPsProgram=0x1300,UboPsFixed=0x1500,UboHalf=0x1600,UboSize=0x1700;
std::uint32_t align(std::uint32_t v,std::uint32_t a){return (v+a-1)&~(a-1);}
DkPrimitive up_primitive(std::uint32_t type){
    switch(type){case 1:return DkPrimitive_Points;case 2:return DkPrimitive_Lines;case 3:return DkPrimitive_LineStrip;
        case 4:return DkPrimitive_Triangles;case 5:return DkPrimitive_TriangleStrip;default:return DkPrimitive_TriangleFan;}
}
DkWrapMode wrap(std::uint32_t d3d){
    switch(d3d){case 2:return DkWrapMode_MirroredRepeat;case 3:return DkWrapMode_ClampToEdge;case 4:return DkWrapMode_ClampToBorder;
        case 5:return DkWrapMode_MirrorClampToEdge;default:return DkWrapMode_Repeat;}
}
}
bool SwitchD3D9Device::allocate(std::uint32_t size,std::uint32_t al,DkGpuAddr& gpu,std::uint8_t*& cpu){
    data_used_=align(data_used_,al);
    if(data_blocks_.empty()||data_used_+size>data_block_size_){
        data_block_size_=std::max<std::uint32_t>(32u<<20,align(size,DK_MEMBLOCK_ALIGNMENT));
        DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,device_,data_block_size_);m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
        auto b=dkMemBlockCreate(&m);if(!b)return false;data_blocks_.push_back(b);data_used_=0;
    }
    auto b=data_blocks_.back();gpu=dkMemBlockGetGpuAddr(b)+data_used_;cpu=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(b))+data_used_;
    data_used_+=size;return true;
}
bool SwitchD3D9Device::load_shader(DkShader& shader,const char* path,std::string& error){
    std::FILE* f=std::fopen(path,"rb");if(!f){error=std::string("missing shader ")+path;return false;}
    std::fseek(f,0,SEEK_END);const long n=std::ftell(f);std::rewind(f);
    const std::uint32_t off=align(code_used_,DK_SHADER_CODE_ALIGNMENT);
    if(n<=0||off+std::uint32_t(n)+DK_SHADER_CODE_UNUSABLE_SIZE>dkMemBlockGetSize(code_)){std::fclose(f);error="shader code memory";return false;}
    const auto read=std::fread(static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(code_))+off,1,std::size_t(n),f);std::fclose(f);
    if(read!=std::size_t(n)){error="shader read";return false;}
    DkShaderMaker sm;dkShaderMakerDefaults(&sm,code_,off);dkShaderInitialize(&shader,&sm);code_used_=off+std::uint32_t(n);return true;
}
bool SwitchD3D9Device::initialize(DkDevice device,DkQueue queue,std::string& error){
    device_=device;queue_=queue;
    DkMemBlockMaker m;
    // The code block is sized from the shader files themselves (the unrolled
    // uber programs are ~1 MB together; a fixed 256 KB block made the whole
    // PC device fail on the console with "shader code memory").
    static const char* const Shaders[]{"romfs:/shaders/pc_vs_uber_vsh.dksh","romfs:/shaders/pc_ps14_uber_fsh.dksh",
        "romfs:/shaders/pc_ps11_uber_fsh.dksh","romfs:/shaders/pc_ffp_vsin_fsh.dksh","romfs:/shaders/pc_ffp_vsh.dksh","romfs:/shaders/pc_ffp_fsh.dksh",
        "romfs:/shaders/pc_ffp_vsps_vsh.dksh","romfs:/shaders/pc_particle_lean_fsh.dksh",
        "romfs:/shaders/pc_particle_half_fsh.dksh","romfs:/shaders/pc_particle_comp_vsh.dksh",
        "romfs:/shaders/pc_particle_comp_fsh.dksh","romfs:/shaders/pc_particle_depth_fsh.dksh"};
    std::uint32_t code_size=0;
    std::vector<const char*> paths(std::begin(Shaders),std::end(Shaders));
    for(const auto& e:SpecTable)paths.push_back(e.path);
    for(const char* path:paths){
        std::FILE* f=std::fopen(path,"rb");if(!f){error=std::string("missing shader ")+path;return false;}
        std::fseek(f,0,SEEK_END);const long n=std::ftell(f);std::fclose(f);
        if(n<=0){error=std::string("empty shader ")+path;return false;}
        code_size=align(code_size,DK_SHADER_CODE_ALIGNMENT)+std::uint32_t(n);
    }
    code_size=align(code_size+DK_SHADER_CODE_UNUSABLE_SIZE,DK_MEMBLOCK_ALIGNMENT);
    dkMemBlockMakerDefaults(&m,device_,code_size);m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code;
    code_=dkMemBlockCreate(&m);if(!code_){error="d3d9 code memory";return false;}
    if(!load_shader(vs_,Shaders[0],error)||!load_shader(ps14_,Shaders[1],error)||!load_shader(ps11_,Shaders[2],error)||
       !load_shader(ffp_vsin_fs_,Shaders[3],error)||!load_shader(ffp_vs_,Shaders[4],error)||!load_shader(ffp_fs_,Shaders[5],error)||
       !load_shader(ffp_vsps_,Shaders[6],error)||!load_shader(particle_fs_,Shaders[7],error)||
       !load_shader(particle_half_fs_,Shaders[8],error)||!load_shader(particle_comp_vs_,Shaders[9],error)||
       !load_shader(particle_comp_fs_,Shaders[10],error)||!load_shader(particle_depth_fs_,Shaders[11],error))return false;
    for(const auto& e:SpecTable){
        Spec sp;sp.kind=e.kind;sp.blobs.assign(e.blobs,e.blobs+e.count);
        if(!load_shader(sp.shader,e.path,error))return false;
        spec_.push_back(std::move(sp));
    }
    dkMemBlockMakerDefaults(&m,device_,2u*UpRingSize);m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    up_ring_=dkMemBlockCreate(&m);if(!up_ring_){error="d3d9 DrawPrimitiveUP ring";return false;}
    dkMemBlockMakerDefaults(&m,device_,align(MaxImages*sizeof(DkImageDescriptor)+MaxSamplers*sizeof(DkSamplerDescriptor),DK_MEMBLOCK_ALIGNMENT));
    m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    descriptors_=dkMemBlockCreate(&m);if(!descriptors_){error="d3d9 descriptor memory";return false;}
    dkMemBlockMakerDefaults(&m,device_,align(UboSize,DK_MEMBLOCK_ALIGNMENT));m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    uniforms_=dkMemBlockCreate(&m);if(!uniforms_){error="d3d9 uniform memory";return false;}
    std::memset(dkMemBlockGetCpuAddr(uniforms_),0,UboSize);ubo_shadow_.assign(UboSize,0u);
    dkMemBlockMakerDefaults(&m,device_,64u*1024u);m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    upload_cmd_mem_=dkMemBlockCreate(&m);if(!upload_cmd_mem_){error="d3d9 upload command memory";return false;}
    DkCmdBufMaker cm;dkCmdBufMakerDefaults(&cm,device_);upload_cmd_=dkCmdBufCreate(&cm);
    dkCmdBufAddMemory(upload_cmd_,upload_cmd_mem_,0,64u*1024u);
    dkMemBlockMakerDefaults(&m,device_,DK_MEMBLOCK_ALIGNMENT);m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    reports_=dkMemBlockCreate(&m);if(!reports_){error="d3d9 query report memory";return false;}
    std::memset(dkMemBlockGetCpuAddr(reports_),0,DK_MEMBLOCK_ALIGNMENT);
    dkMemBlockMakerDefaults(&m,device_,align(2u*MaxProbes*32u,DK_MEMBLOCK_ALIGNMENT));m.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    probes_=dkMemBlockCreate(&m);   // optional (overlay measurement only)
    images_.reserve(MaxImages); // DkImage objects stay at fixed addresses
    // Unbound texture stages: D3D9 pixel shaders read (0, 0, 0, 1) (as
    // PcSoftD3D9Device::sample); without these, handle 0 sampled the first
    // texture ever created.
    if(!solid_image(false,dummy2d_)||!solid_image(true,dummycube_)){error="d3d9 unbound texture images";return false;}
    return true;
}
// A 1x1 opaque black RGBA8 image (2D, or a cube with six faces).
bool SwitchD3D9Device::solid_image(bool cube,std::uint32_t& index){
    if(!create_image(cube?DkImageType_Cubemap:DkImageType_2D,DkImageFormat_RGBA8_Unorm,1,1,cube?6u:1u,1,0,index))return false;
    DkMemBlockMaker sm;dkMemBlockMakerDefaults(&sm,device_,DK_MEMBLOCK_ALIGNMENT);sm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    auto stage=dkMemBlockCreate(&sm);if(!stage)return false;
    auto* cpu=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(stage));
    for(unsigned f=0;f<6u;++f){const std::uint8_t px[4]{0,0,0,255};std::memcpy(cpu+f*DK_IMAGE_LINEAR_STRIDE_ALIGNMENT,px,4);}
    DkImageView view;dkImageViewDefaults(&view,&images_[index]);if(cube)view.type=DkImageType_2DArray;
    for(unsigned f=0;f<(cube?6u:1u);++f){
        const DkCopyBuf src{dkMemBlockGetGpuAddr(stage)+f*DK_IMAGE_LINEAR_STRIDE_ALIGNMENT,0u,0u};const DkImageRect rect{0u,0u,f,1u,1u,1u};
        dkCmdBufCopyBufferToImage(upload_cmd_,&src,&view,&rect,0u);
    }
    dkQueueSubmitCommands(queue_,dkCmdBufFinishList(upload_cmd_));dkQueueWaitIdle(queue_);dkCmdBufClear(upload_cmd_);
    dkCmdBufAddMemory(upload_cmd_,upload_cmd_mem_,0,64u*1024u);
    dkMemBlockDestroy(stage);
    return true;
}
void SwitchD3D9Device::shutdown(){
    if(queue_)dkQueueWaitIdle(queue_);
    if(upload_cmd_)dkCmdBufDestroy(upload_cmd_);upload_cmd_=nullptr;
    for(auto b:data_blocks_)dkMemBlockDestroy(b);data_blocks_.clear();
    for(auto b:image_blocks_)dkMemBlockDestroy(b);image_blocks_.clear();
    for(auto* b:{&code_,&descriptors_,&uniforms_,&upload_cmd_mem_,&up_ring_,&reports_,&probes_})if(*b){dkMemBlockDestroy(*b);*b=nullptr;}
}
std::uint32_t SwitchD3D9Device::create_vertex_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t){
    Object o;o.kind=KBuffer;o.size=n;if(!allocate(std::max(n,4u),16,o.gpu,o.cpu)){errors.push_back("vertex buffer memory");return 0;}
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t SwitchD3D9Device::create_index_buffer(std::uint32_t n,std::uint32_t,std::uint32_t,std::uint32_t){
    return create_vertex_buffer(n,0,0,0);}
std::uint8_t* SwitchD3D9Device::lock(std::uint32_t h,std::uint32_t off,std::uint32_t size,std::uint32_t){
    auto* o=object(h);if(!o||o->kind!=KBuffer||off>o->size||size>o->size-off)return nullptr;return o->cpu+off;}
std::uint32_t SwitchD3D9Device::buffer_size(std::uint32_t h){auto* o=object(h);return o&&o->kind==KBuffer?o->size:0u;}
std::uint32_t SwitchD3D9Device::create_vertex_declaration(const PcVertexElement* e){
    Object o;o.kind=KDecl;for(;;++e){o.elements.push_back(*e);if(e->stream==PcDeclEndStream)break;}
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t SwitchD3D9Device::get_declaration(std::uint32_t h,PcVertexElement* out){
    auto* o=object(h);if(!o||o->kind!=KDecl)return 0;if(out)std::copy(o->elements.begin(),o->elements.end(),out);return std::uint32_t(o->elements.size());}
std::uint32_t SwitchD3D9Device::create_vertex_shader(const std::uint32_t* t){
    Object o;o.kind=KVs;if(!pc_shader_split(t,o.program)||o.program.blobs.size()>16){errors.push_back("vertex shader not made of EXE blobs");return 0;}
    o.spec=find_spec(0u,o.program.blobs);
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t SwitchD3D9Device::create_pixel_shader(const std::uint32_t* t){
    Object o;o.kind=KPs;if(!pc_shader_split(t,o.program)||o.program.blobs.size()>16){errors.push_back("pixel shader not made of EXE blobs");return 0;}
    o.spec=find_spec(o.program.version==0xffff0101u?1u:2u,o.program.blobs);
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());}
std::uint32_t SwitchD3D9Device::create_texture_from_file(const PcTextureFileRequest& q){
    PcDdsTexture t;std::string error;
    if(!pc_d3dx_texture_from_file(q,t,error)){errors.push_back(error);return 0;}
    // Narrow BC images (one side <= 32 texels, e.g. the 256x32 SUMO_FE strip with the
    // NOT AVAILABLE label and the grey menu dashes) sample corrupted on the console when
    // uploaded compressed; they go up as RGBA8 (checked on hardware 2026-10-07 with
    // decompress_textures=1, which forces it for every BC file).
    const bool narrow=t.width<=32u||t.height<=32u;
    if((g_pc_decompress_bc||narrow)&&t.format!=PcDdsTexture::Format::rgba8){
        for(unsigned f=0;f<t.faces.size();++f)for(unsigned l=0;l<t.faces[f].size();++l)t.faces[f][l].data=pc_dds_rgba(t,f,l);
        t.format=PcDdsTexture::Format::rgba8;
    }
    if(images_.size()>=MaxImages){errors.push_back("image descriptor pool full");return 0;}
    std::uint32_t index=0;
    if(!create_image(t.cube?DkImageType_Cubemap:DkImageType_2D,
        t.format==PcDdsTexture::Format::bc1?DkImageFormat_RGBA_BC1:t.format==PcDdsTexture::Format::bc2?DkImageFormat_RGBA_BC2:
        t.format==PcDdsTexture::Format::bc3?DkImageFormat_RGBA_BC3:DkImageFormat_RGBA8_Unorm,
        t.width,t.height,t.cube?6u:1u,t.levels,0,index))return 0;
    auto& image=images_[index];
    // Staging copy of every face/level, then GPU copies.
    std::size_t staging=0;for(const auto& f:t.faces)for(const auto& l:f)staging+=align(std::uint32_t(l.data.size()),DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    DkMemBlockMaker sm;dkMemBlockMakerDefaults(&sm,device_,align(std::uint32_t(staging),DK_MEMBLOCK_ALIGNMENT));sm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    auto stage=dkMemBlockCreate(&sm);if(!stage){errors.push_back("texture staging");return 0;}
    auto* cpu=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(stage));const auto gpu=dkMemBlockGetGpuAddr(stage);
    std::uint32_t at=0;
    for(unsigned f=0;f<t.faces.size();++f)for(unsigned l=0;l<t.faces[f].size();++l){
        const auto& lv=t.faces[f][l];std::memcpy(cpu+at,lv.data.data(),lv.data.size());
        DkImageView view;dkImageViewDefaults(&view,&image);view.mipLevelOffset=std::uint8_t(l);view.mipLevelCount=1;
        if(t.cube){view.type=DkImageType_2DArray;}
        const DkCopyBuf src{gpu+at,0u,0u};const DkImageRect rect{0u,0u,f,lv.width,lv.height,1u};
        dkCmdBufCopyBufferToImage(upload_cmd_,&src,&view,&rect,0u);
        at+=align(std::uint32_t(lv.data.size()),DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    }
    dkQueueSubmitCommands(queue_,dkCmdBufFinishList(upload_cmd_));dkQueueWaitIdle(queue_);dkCmdBufClear(upload_cmd_);
    dkCmdBufAddMemory(upload_cmd_,upload_cmd_mem_,0,64u*1024u);
    dkMemBlockDestroy(stage);
    Object o;o.kind=KTexture;o.image=index;o.cube=t.cube;o.width=t.width;o.height=t.height;o.levels=t.levels;
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());
}
bool SwitchD3D9Device::create_image(DkImageType type,DkImageFormat format,std::uint32_t w,std::uint32_t h,std::uint32_t layers,
                                    std::uint32_t levels,std::uint32_t flags,std::uint32_t& index){
    if(images_.size()>=MaxImages){errors.push_back("image descriptor pool full");return false;}
    DkImageLayoutMaker lm;dkImageLayoutMakerDefaults(&lm,device_);
    lm.type=type;lm.format=format;lm.flags=flags;
    lm.dimensions[0]=w;lm.dimensions[1]=h;lm.dimensions[2]=layers;lm.mipLevels=levels;
    DkImageLayout layout;dkImageLayoutInitialize(&layout,&lm);
    const auto size=std::uint32_t(dkImageLayoutGetSize(&layout)),al=dkImageLayoutGetAlignment(&layout);
    image_used_=align(image_used_,al);
    if(image_blocks_.empty()||image_used_+size>image_block_size_){
        image_block_size_=std::max<std::uint32_t>(32u<<20,align(size,DK_MEMBLOCK_ALIGNMENT));
        DkMemBlockMaker m;dkMemBlockMakerDefaults(&m,device_,image_block_size_);m.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;
        auto b=dkMemBlockCreate(&m);if(!b){errors.push_back("texture memory");return false;}image_blocks_.push_back(b);image_used_=0;
    }
    images_.emplace_back();auto& image=images_.back();
    dkImageInitialize(&image,&layout,image_blocks_.back(),image_used_);image_used_+=size;
    index=std::uint32_t(images_.size()-1);
    DkImageView view;dkImageViewDefaults(&view,&image);
    auto* desc=static_cast<DkImageDescriptor*>(dkMemBlockGetCpuAddr(descriptors_));
    dkImageDescriptorInitialize(&desc[index],&view,false,false);
    return true;
}
// ---- renderer-init objects (404250) ----
namespace {
std::uint32_t texel_bytes(std::uint32_t format){
    switch(format){case 21:case 22:return 4;case 23:return 2;case 75:return 4;default:return 0;}
}
}
std::uint32_t SwitchD3D9Device::empty_texture(bool cube,std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t format,std::uint32_t usage){
    if(!texel_bytes(format)||!w||!h||format==75u){errors.push_back("empty texture format");return 0;}
    if(levels==0){levels=1;for(std::uint32_t m=std::max(w,h);m>1;m>>=1)++levels;}
    const auto fmt=format==23u?DkImageFormat_RGB565_Unorm:DkImageFormat_RGBA8_Unorm;
    std::uint32_t index=0;
    if(!create_image(cube?DkImageType_Cubemap:DkImageType_2D,fmt,w,h,cube?6u:1u,levels,(usage&1u)?DkImageFlags_UsageRender:0u,index))return 0;
    Object o;o.kind=KTexture;o.image=index;o.cube=cube;o.format=format;o.width=w;o.height=h;o.levels=levels;
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());
}
std::uint32_t SwitchD3D9Device::create_cube_texture(std::uint32_t edge,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t){
    return empty_texture(true,edge,edge,levels,format,usage);}
std::uint32_t SwitchD3D9Device::create_texture(std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t){
    return empty_texture(false,w,h,levels,format,usage);}
std::uint32_t SwitchD3D9Device::create_depth_stencil_surface(std::uint32_t w,std::uint32_t h,std::uint32_t format,std::uint32_t,std::uint32_t,std::uint32_t){
    if(format!=75u){errors.push_back("depth-stencil format");return 0;}
    std::uint32_t index=0;
    if(!create_image(DkImageType_2D,DkImageFormat_Z24S8,w,h,1,1,DkImageFlags_UsageRender,index))return 0;
    Object o;o.kind=KSurface;o.image=index;o.format=format;o.width=w;o.height=h;
    objects_.push_back(std::move(o));return std::uint32_t(objects_.size());
}
std::uint32_t SwitchD3D9Device::surface_of(std::uint32_t t,std::uint32_t face,std::uint32_t level){
    auto* o=object(t);if(!o||o->kind!=KTexture||!o->format)return 0;
    if(face>=(o->cube?6u:1u)||level>=o->levels)return 0;
    // One surface object per (texture, face, level), as D3D9 returns the same surface.
    for(std::size_t k=0;k<objects_.size();++k){const auto& x=objects_[k];
        if(x.kind==KSurface&&x.parent==t&&x.face==face&&x.level==level)return std::uint32_t(k+1);}
    Object s;s.kind=KSurface;s.parent=t;s.face=face;s.level=level;s.format=o->format;s.image=o->image;s.cube=o->cube;
    s.width=std::max(1u,o->width>>level);s.height=std::max(1u,o->height>>level);
    objects_.push_back(std::move(s));return std::uint32_t(objects_.size());
}
std::uint32_t SwitchD3D9Device::get_cube_map_surface(std::uint32_t c,std::uint32_t face,std::uint32_t level){
    auto* o=object(c);if(!o||!o->cube)return 0;return surface_of(c,face,level);}
std::uint32_t SwitchD3D9Device::get_surface_level(std::uint32_t t,std::uint32_t level){
    auto* o=object(t);if(!o||o->cube)return 0;return surface_of(t,0,level);}
std::uint8_t* SwitchD3D9Device::lock_rect(std::uint32_t s,std::uint32_t& pitch){
    auto* o=object(s);pitch=0;if(!o||o->kind!=KSurface||!o->parent)return nullptr;
    o->texels.assign(std::size_t(o->width)*o->height*texel_bytes(o->format),0);
    pitch=o->width*texel_bytes(o->format);return o->texels.data();}
void SwitchD3D9Device::unlock_rect(std::uint32_t s){
    auto* o=object(s);if(!o||o->kind!=KSurface||!o->parent||o->texels.empty())return;
    // D3D memory layout to the image format: X8R8G8B8/A8R8G8B8 (B,G,R,A
    // bytes) become RGBA8 (X8 reads as opaque); R5G6B5 is uploaded as is.
    std::vector<std::uint8_t> data(o->texels.size());
    const std::size_t n=std::size_t(o->width)*o->height;
    if(o->format==23u)data=o->texels;
    else for(std::size_t k=0;k<n;++k){data[k*4]=o->texels[k*4+2];data[k*4+1]=o->texels[k*4+1];data[k*4+2]=o->texels[k*4];
        data[k*4+3]=o->format==22u?0xffu:o->texels[k*4+3];}
    DkMemBlockMaker sm;dkMemBlockMakerDefaults(&sm,device_,align(std::uint32_t(data.size()),DK_MEMBLOCK_ALIGNMENT));
    sm.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    auto stage=dkMemBlockCreate(&sm);if(!stage){errors.push_back("surface upload staging");return;}
    std::memcpy(dkMemBlockGetCpuAddr(stage),data.data(),data.size());
    auto& image=images_[o->image];
    DkImageView view;dkImageViewDefaults(&view,&image);view.mipLevelOffset=std::uint8_t(o->level);view.mipLevelCount=1;
    if(o->cube)view.type=DkImageType_2DArray;
    const DkCopyBuf src{dkMemBlockGetGpuAddr(stage),0u,0u};const DkImageRect rect{0u,0u,o->face,o->width,o->height,1u};
    dkCmdBufCopyBufferToImage(upload_cmd_,&src,&view,&rect,0u);
    dkQueueSubmitCommands(queue_,dkCmdBufFinishList(upload_cmd_));dkQueueWaitIdle(queue_);dkCmdBufClear(upload_cmd_);
    dkCmdBufAddMemory(upload_cmd_,upload_cmd_mem_,0,64u*1024u);
    dkMemBlockDestroy(stage);
    o->texels.clear();
}
std::uint32_t SwitchD3D9Device::sampler_index(std::uint32_t st){
    const auto& s=sampler[st];
    const std::array<std::uint32_t,9> key{s[d3d9::SAMP_ADDRESSU],s[d3d9::SAMP_ADDRESSV],s[d3d9::SAMP_ADDRESSW],s[d3d9::SAMP_BORDERCOLOR],
        s[d3d9::SAMP_MAGFILTER],s[d3d9::SAMP_MINFILTER],s[d3d9::SAMP_MIPFILTER],s[d3d9::SAMP_MIPMAPLODBIAS],s[d3d9::SAMP_MAXMIPLEVEL]};
    auto& memo=sampler_memo_[st];                      // [0..8] the tuple, [9] its index
    if(g_pc_state_cache&&sampler_memo_valid_[st]&&std::equal(key.begin(),key.end(),memo.begin()))return memo[9];
    const std::uint32_t index=sampler_lookup(st,key);
    std::copy(key.begin(),key.end(),memo.begin());memo[9]=index;sampler_memo_valid_[st]=true;
    return index;
}
std::uint32_t SwitchD3D9Device::sampler_lookup(std::uint32_t st,const std::array<std::uint32_t,9>& key){
    const auto& s=sampler[st];
    for(std::uint32_t k=0;k<samplers_.size();++k)if(samplers_[k]==key)return k;
    if(samplers_.size()>=MaxSamplers)return 0;
    DkSampler d;dkSamplerDefaults(&d);
    d.wrapMode[0]=wrap(key[0]);d.wrapMode[1]=wrap(key[1]);d.wrapMode[2]=wrap(key[2]);
    const auto b=key[3];d.borderColor[0].value_f=((b>>16)&255)/255.f;d.borderColor[1].value_f=((b>>8)&255)/255.f;
    d.borderColor[2].value_f=(b&255)/255.f;d.borderColor[3].value_f=(b>>24)/255.f;
    d.magFilter=key[4]>=2?DkFilter_Linear:DkFilter_Nearest;d.minFilter=key[5]>=2?DkFilter_Linear:DkFilter_Nearest;
    d.mipFilter=key[6]==0?DkMipFilter_None:key[6]==1?DkMipFilter_Nearest:DkMipFilter_Linear;
    d.lodBias=d3d9::f(key[7]);d.lodClampMin=float(key[8]);
    if(key[5]==3)d.maxAnisotropy=float(std::max<std::uint32_t>(1,s[d3d9::SAMP_MAXANISOTROPY]));
    auto* base=static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(descriptors_))+MaxImages*sizeof(DkImageDescriptor);
    dkSamplerDescriptorInitialize(reinterpret_cast<DkSamplerDescriptor*>(base)+samplers_.size(),&d);
    samplers_.push_back(key);return std::uint32_t(samplers_.size()-1);
}
void SwitchD3D9Device::apply_viewport(){
    // D3D9 viewport (PC pixels) -> framebuffer pixels of the PC screen rectangle.
    const float sx=float(frame_w_)/float(pc_w_),sy=float(frame_h_)/float(pc_h_);
    const float x=float(frame_x_)+float(viewport_[0])*sx,y=float(frame_y_)+float(viewport_[1])*sy;
    const float w=float(viewport_[2])*sx,h=float(viewport_[3])*sy;
    const DkViewport vp{x,y,w,h,d3d9::f(viewport_[4]),d3d9::f(viewport_[5])};
    const DkScissor sc{std::uint32_t(x),std::uint32_t(y),std::uint32_t(w),std::uint32_t(h)};
    dkCmdBufSetViewports(cmd_,0,&vp,1);dkCmdBufSetScissors(cmd_,0,&sc,1);
    const float fix[4]{1.f/w,1.f/h,0.f,float(diag_mode)};
    push_ubo(UboVsFix,0x100,0,16,fix);
}
void SwitchD3D9Device::push_ps_fixed(){
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    {   float f[12]{};
        const auto fc=render[d3d9::RS_FOGCOLOR];
        f[0]=((fc>>16)&255)/255.f;f[1]=((fc>>8)&255)/255.f;f[2]=(fc&255)/255.f;f[3]=d3d9::f(render[d3d9::RS_FOGEND]);
        if(render[d3d9::RS_FOGENABLE]){const auto mode=render[d3d9::RS_FOGTABLEMODE];f[4]=mode?float(mode):4.f;}
        f[5]=d3d9::f(render[d3d9::RS_FOGSTART]);f[6]=d3d9::f(render[d3d9::RS_FOGDENSITY]);f[7]=1.f; // w-based fog
        f[8]=render[d3d9::RS_ALPHATESTENABLE]?1.f:0.f;f[9]=float(render[d3d9::RS_ALPHAREF]&255);f[10]=float(render[d3d9::RS_ALPHAFUNC]);
        f[11]=float(diag_mode);
        push_ubo(UboPsFixed,0x100,0,sizeof(f),f);}
}
void SwitchD3D9Device::apply_render_states(){
    {   DkRasterizerState rs;dkRasterizerStateDefaults(&rs);
        // D3D9 culls by on-screen winding. The 2026-09-28 hardware test that
        // chose CCW ran with the old y flip in pc_vs_uber; without it (clip +y
        // at the top, upper-left origin) the winding is D3D's own: with CCW
        // every culled draw (course, cars, robots) vanished on hardware
        // (2026-09-29 log: 510418 of 587946 first vertices on screen on the
        // CPU, only cull-none sky/sea/flag visible).
        rs.frontFace=DkFrontFace_CW;
        const auto cull=render[d3d9::RS_CULLMODE];
        const std::array<std::uint32_t,5> raster_key{cull,render[d3d9::RS_DEPTHBIAS],render[d3d9::RS_SLOPESCALEDEPTHBIAS],diag_mode,1u};
        if(bound_.raster&&bound_.raster_key==raster_key)++binds_skipped;else{bound_.raster=true;bound_.raster_key=raster_key;
        rs.cullMode=cull==d3d9::CULL_CCW?DkFace_Back:cull==d3d9::CULL_CW?DkFace_Front:DkFace_None;
        if(diag_mode>=5u)rs.cullMode=DkFace_None;                         // console diagnostic 5..6
        const float bias=d3d9::f(render[d3d9::RS_DEPTHBIAS]),slope=d3d9::f(render[d3d9::RS_SLOPESCALEDEPTHBIAS]);
        rs.depthBiasEnableMask=(bias!=0.f||slope!=0.f)?DkPolygonFlag_All:0;
        dkCmdBufBindRasterizerState(cmd_,&rs);
        if(rs.depthBiasEnableMask)dkCmdBufSetDepthBias(cmd_,bias*16777215.f,0.f,slope);}
        const std::array<std::uint32_t,14> depth_key{render[d3d9::RS_ZENABLE],render[d3d9::RS_ZWRITEENABLE],render[d3d9::RS_ZFUNC],
            render[52],render[53],render[54],render[55],render[56],render[57],render[58],render[59],diag_mode,1u,0u};
        if(bound_.depth&&bound_.depth_key==depth_key)++binds_skipped;else{bound_.depth=true;bound_.depth_key=depth_key;
        DkDepthStencilState ds;dkDepthStencilStateDefaults(&ds);
        ds.depthTestEnable=render[d3d9::RS_ZENABLE]!=0;ds.depthWriteEnable=render[d3d9::RS_ZWRITEENABLE]!=0;
        ds.depthCompareOp=DkCompareOp(std::clamp<std::uint32_t>(render[d3d9::RS_ZFUNC],1,8));
        // Stencil (RS 52..59): D3DSTENCILOP / D3DCMP share deko3d's numbering.
        ds.stencilTestEnable=render[52]!=0;
        const auto sop=[](std::uint32_t o){return DkStencilOp(std::clamp<std::uint32_t>(o,1,8));};
        ds.stencilFrontFailOp=ds.stencilBackFailOp=sop(render[53]);
        ds.stencilFrontDepthFailOp=ds.stencilBackDepthFailOp=sop(render[54]);
        ds.stencilFrontPassOp=ds.stencilBackPassOp=sop(render[55]);
        ds.stencilFrontCompareOp=ds.stencilBackCompareOp=DkCompareOp(std::clamp<std::uint32_t>(render[56],1,8));
        if(diag_mode>=4u)ds.depthTestEnable=false;                   // console diagnostic 4..6
        dkCmdBufBindDepthStencilState(cmd_,&ds);
        if(ds.stencilTestEnable)dkCmdBufSetStencil(cmd_,DkFace_FrontAndBack,std::uint8_t(render[59]),std::uint8_t(render[57]),std::uint8_t(render[58]));}
        const std::uint32_t colour_key=render[d3d9::RS_ALPHABLENDENABLE]!=0?1u:0u;
        if(bound_.colour&&bound_.colour_key==colour_key)++binds_skipped;else{bound_.colour=true;bound_.colour_key=colour_key;
        DkColorState cs;dkColorStateDefaults(&cs);dkColorStateSetBlendEnable(&cs,0,colour_key!=0u);
        dkCmdBufBindColorState(cmd_,&cs);}
        const std::uint32_t write_key=render[d3d9::RS_COLORWRITEENABLE]&0xf;
        if(bound_.write&&bound_.write_key==write_key)++binds_skipped;else{bound_.write=true;bound_.write_key=write_key;
        DkColorWriteState ws;dkColorWriteStateDefaults(&ws);dkColorWriteStateSetMask(&ws,0,write_key);
        dkCmdBufBindColorWriteState(cmd_,&ws);}
        const std::array<std::uint32_t,3> blend_key{render[d3d9::RS_SRCBLEND],render[d3d9::RS_DESTBLEND],render[d3d9::RS_BLENDOP]};
        if(bound_.blend&&bound_.blend_key==blend_key){++binds_skipped;return;}
        bound_.blend=true;bound_.blend_key=blend_key;
        DkBlendState bs;dkBlendStateDefaults(&bs);
        auto factor=[](std::uint32_t f){return f>=1&&f<=11?DkBlendFactor(f):DkBlendFactor_One;};
        auto op=[](std::uint32_t o){return o>=1&&o<=5?DkBlendOp(o):DkBlendOp_Add;};
        dkBlendStateSetFactors(&bs,factor(render[d3d9::RS_SRCBLEND]),factor(render[d3d9::RS_DESTBLEND]),
                               factor(render[d3d9::RS_SRCBLEND]),factor(render[d3d9::RS_DESTBLEND]));
        dkBlendStateSetOps(&bs,op(render[d3d9::RS_BLENDOP]),op(render[d3d9::RS_BLENDOP]));
        dkCmdBufBindBlendState(cmd_,0,&bs);}
}
bool SwitchD3D9Device::texture_level_size(std::uint32_t texture,std::uint32_t level,std::uint32_t& w,std::uint32_t& h){
    w=h=0;auto* o=object(texture);
    if(!o||o->kind!=KTexture||!o->width||level>=std::max(1u,o->levels))return false;
    w=std::max(1u,o->width>>level);h=std::max(1u,o->height>>level);return true;
}
void SwitchD3D9Device::draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
    if(!cmd_||!data||!count)return;
    ProfileScope prof(profile_key(ProfileRenderer,0,ProfDeviceDraw));
    // Binds the vertex input and shaders itself (not through the cache).
    if(!g_pc_state_cache)invalidate_bound();
    struct Unbind{Bound& b;explicit Unbind(Bound& bb):b(bb){b.vertex=false;b.shaders=false;}~Unbind(){b.vertex=false;b.shaders=false;}} unbind(bound_);
    const std::uint32_t fvf=fvf_;
    const bool rhw=(fvf&0x400eu)==0x004u,xyz=(fvf&0x400eu)==0x002u;
    if(!rhw&&!xyz){errors.push_back("draw_primitive_up: only XYZRHW / XYZ FVFs are supported");return;}
    // D3DPRIMITIVETYPE: 1 points, 2 lines, 3 line strip, 4 triangles, 5 strip, 6 fan.
    const std::uint32_t vertices=type==1?count:type==2?count*2u:type==3?count+1u:type==4?count*3u:(type==5||type==6)?count+2u:0u;
    if(!vertices){errors.push_back("draw_primitive_up: unsupported primitive type");return;}
    // Widescreen: the 2D layer keeps the PC's 4:3 placement, centred.
    struct UiRect{SwitchD3D9Device& d;bool on;std::uint32_t x,w;
        UiRect(SwitchD3D9Device& dd,bool o):d(dd),on(o),x(dd.frame_x_),w(dd.frame_w_){if(on){d.frame_x_=d.ui_x_;d.frame_w_=d.ui_w_;d.apply_viewport();}}
        ~UiRect(){if(on){d.frame_x_=x;d.frame_w_=w;d.apply_viewport();}}}
        ui_rect(*this,rhw&&ui_rect_on_&&colour_target_==BackBuffer);
    // FVF layout: position (16), [normal 12], [psize 4], [diffuse], [specular], texture sets.
    std::uint32_t off=rhw?16u:12u;if(fvf&0x010u)off+=12;if(fvf&0x020u)off+=4;
    const std::uint32_t diffuse_at=off;if(fvf&0x040u)off+=4;
    const std::uint32_t specular_at=off;if(fvf&0x080u)off+=4;
    const unsigned sets=std::min(4u,(fvf>>8)&0xfu);std::uint32_t tex_at[4]{};unsigned tex_n[4]{};
    for(unsigned k=0;k<sets;++k){const unsigned f=(fvf>>(16+2*k))&3u;tex_n[k]=f==0?2:f==1?3:f==2?4:1;tex_at[k]=off;off+=4u*tex_n[k];}
    if(stride<off){errors.push_back("draw_primitive_up: stride smaller than the FVF vertex");return;}
    const std::uint32_t bytes=vertices*stride,at=align(up_used_,16u);
    if(at+bytes>UpRingSize){errors.push_back("draw_primitive_up: ring full");return;}
    std::memcpy(static_cast<std::uint8_t*>(dkMemBlockGetCpuAddr(up_ring_))+slot_*UpRingSize+at,data,bytes);up_used_=at+bytes;
    const auto gpu=dkMemBlockGetGpuAddr(up_ring_)+slot_*UpRingSize+at;
    if(auto* vs=object(vertex_shader);xyz&&vs&&vs->kind==KVs){
        // XYZ vertices through the bound vertex shader: POSITION 0, NORMAL 3,
        // COLOR 10 (0 diffuse, 1 specular), TEXCOORD 5 (set).
        std::array<DkVtxAttribState,16> va{};
        for(auto& a:va){a={};a.isFixed=1;a.size=DkVtxAttribSize_4x32;a.type=DkVtxAttribType_Float;}
        auto input=[&](std::uint8_t usage,std::uint8_t index,std::uint32_t offset,DkVtxAttribSize size,DkVtxAttribType type,bool bgra){
            for(const auto& in:vs->program.inputs){
                if(in.usage!=usage||in.usage_index!=index||in.reg>=16)continue;
                auto& a=va[in.reg];a={};a.bufferId=0;a.offset=offset;a.size=size;a.type=type;a.isBgra=bgra?1:0;}};
        input(0,0,0,DkVtxAttribSize_3x32,DkVtxAttribType_Float,false);
        if(fvf&0x010u)input(3,0,12,DkVtxAttribSize_3x32,DkVtxAttribType_Float,false);
        if(fvf&0x040u)input(10,0,diffuse_at,DkVtxAttribSize_4x8,DkVtxAttribType_Unorm,true);
        if(fvf&0x080u)input(10,1,specular_at,DkVtxAttribSize_4x8,DkVtxAttribType_Unorm,true);
        for(unsigned k=0;k<sets;++k)input(5,std::uint8_t(k),tex_at[k],tex_n[k]==1?DkVtxAttribSize_1x32:tex_n[k]==2?DkVtxAttribSize_2x32:
            tex_n[k]==3?DkVtxAttribSize_3x32:DkVtxAttribSize_4x32,DkVtxAttribType_Float,false);
        const DkVtxBufferState vbuf{stride,0};
        dkCmdBufBindVtxBuffer(cmd_,0,gpu,bytes);
        dkCmdBufBindVtxAttribState(cmd_,va.data(),16);
        dkCmdBufBindVtxBufferState(cmd_,&vbuf,1);
        bind_programmable(vs,object(pixel_shader));
        apply_render_states();
        const int probe=probe_begin();
        dkCmdBufDraw(cmd_,up_primitive(type),vertices,1,0,0);
        probe_end(probe);
        ++draws;
        return;
    }
    std::array<DkVtxAttribState,7> attribs{};
    for(auto& a:attribs){a={};a.isFixed=1;a.size=DkVtxAttribSize_4x32;a.type=DkVtxAttribType_Float;}
    attribs[0]={};attribs[0].bufferId=0;attribs[0].offset=0;attribs[0].size=rhw?DkVtxAttribSize_4x32:DkVtxAttribSize_3x32;attribs[0].type=DkVtxAttribType_Float;
    if(fvf&0x040u){auto& a=attribs[1];a={};a.offset=diffuse_at;a.size=DkVtxAttribSize_4x8;a.type=DkVtxAttribType_Unorm;a.isBgra=1;}
    if(fvf&0x080u){auto& a=attribs[2];a={};a.offset=specular_at;a.size=DkVtxAttribSize_4x8;a.type=DkVtxAttribType_Unorm;a.isBgra=1;}
    for(unsigned k=0;k<sets;++k){auto& a=attribs[3+k];a={};a.offset=tex_at[k];a.type=DkVtxAttribType_Float;
        a.size=tex_n[k]==1?DkVtxAttribSize_1x32:tex_n[k]==2?DkVtxAttribSize_2x32:tex_n[k]==3?DkVtxAttribSize_3x32:DkVtxAttribSize_4x32;}
    const DkVtxBufferState buf{stride,0};
    dkCmdBufBindVtxBuffer(cmd_,0,gpu,bytes);
    dkCmdBufBindVtxAttribState(cmd_,attribs.data(),std::uint32_t(attribs.size()));
    dkCmdBufBindVtxBufferState(cmd_,&buf,1);
    // D3D9 applies a bound pixel shader to fixed-function vertices too (the
    // particles draw with 623830, PcSoftD3D9Device::draw_primitive_up runs it):
    // the transform of pc_ffp_vsh in the pixel programs' layout, then the
    // program. Without one, the texture stage cascade.
    auto* ps=object(pixel_shader);if(ps&&ps->kind!=KPs)ps=nullptr;
    const DkShader* shaders[2]{ps?&ffp_vsps_:&ffp_vs_,ps?bind_pixel_program(ps):&ffp_fs_};
    bool half=false;
    if(ps&&g_pc_particle_mode!=0u&&diag_mode==0u&&ps->program.blobs.size()==1u&&ps->program.blobs[0]==54u){
        const unsigned mode=std::min(g_pc_particle_mode,2u);
        const bool noop_zero=render[d3d9::RS_SRCBLEND]==2u&&render[d3d9::RS_DESTBLEND]==6u&&render[d3d9::RS_BLENDOP]==1u&&
            render[d3d9::RS_ALPHABLENDENABLE]!=0u&&(render[d3d9::RS_COLORWRITEENABLE]&0xfu)==0xfu;
        half=mode==2u&&noop_zero&&xyz;
        shaders[1]=&particle_fs_;
    }
    dkCmdBufBindShaders(cmd_,DkStageFlag_GraphicsMask,shaders,2);
    if(ps)++dip.up_ps;
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    // Screen: the whole PC screen is the viewport of pretransformed vertices;
    // the D3D viewport rectangle only clips (scissor). D3D9 pixel centres are
    // integral: +0.5 framebuffer pixel, in PC pixels.
    {   struct {float screen[4];std::int32_t present[4];float fix[4];float wvp[16];} v{};
        v.screen[0]=2.f/float(pc_w_);v.screen[1]=2.f/float(pc_h_);
        v.screen[2]=0.5f*float(pc_w_)/float(frame_w_);v.screen[3]=0.5f*float(pc_h_)/float(frame_h_);
        v.present[0]=(fvf&0x040u)?1:0;v.present[1]=(fvf&0x080u)?1:0;v.present[2]=xyz?1:0;
        if(xyz){const auto m=transform_wvp();for(unsigned k=0;k<16;++k)v.wvp[k]=m[k];
            // D3D9 viewport transform through the deko3d viewport (apply_viewport): pixel-centre fix only.
            const float sx=float(frame_w_)/float(pc_w_),sy=float(frame_h_)/float(pc_h_);
            v.fix[0]=1.f/(float(viewport_[2])*sx);v.fix[1]=1.f/(float(viewport_[3])*sy);}
        push_ubo(UboVsFix,0x100,0,sizeof(v),&v);
        dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,0,u+UboVsFix,0x100);}
    if(!ps){
        push_ffp_stages();
        push_ps_fixed();
        bind_pixel_textures();}                                    // 2D 0..5, cube 6..11
    apply_render_states();
    if(half&&draw_particles_half(up_primitive(type),vertices)){
        ++draws;
        apply_viewport();
        dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,0,u+UboVsConst,0x1100);
        dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,1,u+UboPsProgram,0x200);
        return;
    }
    // Pretransformed: full PC-screen viewport, the D3D viewport rectangle as
    // the scissor. Transformed vertices keep the D3D viewport (apply_viewport).
    if(rhw){   const float sx=float(frame_w_)/float(pc_w_),sy=float(frame_h_)/float(pc_h_);
        const DkViewport vp{float(frame_x_),float(frame_y_),float(frame_w_),float(frame_h_),0.f,1.f};
        const DkScissor sc{std::uint32_t(float(frame_x_)+float(viewport_[0])*sx),std::uint32_t(float(frame_y_)+float(viewport_[1])*sy),
                           std::uint32_t(float(viewport_[2])*sx),std::uint32_t(float(viewport_[3])*sy)};
        dkCmdBufSetViewports(cmd_,0,&vp,1);dkCmdBufSetScissors(cmd_,0,&sc,1);}
    const int probe=probe_begin();
    dkCmdBufDraw(cmd_,up_primitive(type),vertices,1,0,0);
    probe_end(probe);
    ++draws;
    // Back to the indexed-draw bindings: the D3D viewport and the uber uniforms.
    apply_viewport();
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,0,u+UboVsConst,0x1100);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,1,u+UboPsProgram,0x200);
}
void SwitchD3D9Device::Sprite::draw(std::uint32_t texture,const std::int32_t* rect,const float* center,const float* position,std::uint32_t colour){
    auto& d=d_;
    std::uint32_t tw=0,th=0;
    if(texture&&!d.texture_level_size(texture,0,tw,th))return;
    float l=0,t=0,r=float(tw),b=float(th);
    if(rect){l=float(rect[0]);t=float(rect[1]);r=float(rect[2]);b=float(rect[3]);}
    const float w=r-l,h=b-t;
    const float cx=center?center[0]:0.f,cy=center?center[1]:0.f,cz=center?center[2]:0.f;
    const float px=position?position[0]:0.f,py=position?position[1]:0.f,pz=position?position[2]:0.f;
    const float* M=m_;
    struct V { float x,y,z,rhw; std::uint32_t c; float u,v; } q[4];
    static_assert(sizeof(V)==28,"FVF 0x144 vertex");
    const float cu[4]{0,w,0,w},cv[4]{0,0,h,h};
    for(unsigned k=0;k<4;++k){
        const float x=cu[k]-cx+px,y=cv[k]-cy+py,z=pz-cz;
        q[k].x=x*M[0]+y*M[4]+z*M[8]+M[12];
        q[k].y=x*M[1]+y*M[5]+z*M[9]+M[13];
        q[k].z=x*M[2]+y*M[6]+z*M[10]+M[14];
        q[k].rhw=1.f;q[k].c=colour;
        q[k].u=tw?(l+cu[k])/float(tw):0.f;q[k].v=th?(t+cv[k])/float(th):0.f;
    }
    const auto saved_stage0=d.stage[0],saved_stage1=d.stage[1];
    const std::uint32_t saved_tex=d.textures[0],saved_ps=d.pixel_shader,saved_vs=d.vertex_shader,saved_fvf=d.fvf_;
    const std::uint32_t saved_cull=d.render[d3d9::RS_CULLMODE],saved_ab=d.render[d3d9::RS_ALPHABLENDENABLE],
        saved_src=d.render[d3d9::RS_SRCBLEND],saved_dst=d.render[d3d9::RS_DESTBLEND];
    if(!(flags_&2u)){
        d.render[d3d9::RS_CULLMODE]=d3d9::CULL_NONE;
        if(flags_&0x10u){d.render[d3d9::RS_ALPHABLENDENABLE]=1;d.render[d3d9::RS_SRCBLEND]=5;d.render[d3d9::RS_DESTBLEND]=6;}
    }
    d.stage[0][1]=4;d.stage[0][2]=2;d.stage[0][3]=0;d.stage[0][4]=4;d.stage[0][5]=2;d.stage[0][6]=0;d.stage[0][11]=0;d.stage[0][28]=1;
    d.stage[1][1]=1;d.stage[1][4]=1;
    d.textures[0]=texture;d.pixel_shader=0;d.vertex_shader=0;d.fvf_=0x144u;
    d.draw_primitive_up(5,2,q,sizeof(V));
    d.stage[0]=saved_stage0;d.stage[1]=saved_stage1;d.textures[0]=saved_tex;d.pixel_shader=saved_ps;d.vertex_shader=saved_vs;d.fvf_=saved_fvf;
    d.render[d3d9::RS_CULLMODE]=saved_cull;d.render[d3d9::RS_ALPHABLENDENABLE]=saved_ab;d.render[d3d9::RS_SRCBLEND]=saved_src;d.render[d3d9::RS_DESTBLEND]=saved_dst;
}
void SwitchD3D9Device::set_viewport(const std::uint32_t v[6]){
    PcD3D9Device::set_viewport(v);
    if(cmd_)apply_viewport();
}
void SwitchD3D9Device::clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil){
    if(!cmd_)return;
    clear(colour,z,(flags&1u)!=0u,false);
    if(flags&6u)dkCmdBufClearDepthStencil(cmd_,(flags&2u)!=0u,z,(flags&4u)?0xffu:0u,std::uint8_t(stencil));
}
bool SwitchD3D9Device::bind_targets(){
    if(!cmd_)return false;
    DkImageView colour,depth;const DkImageView* colours[1]{&colour};
    std::uint32_t w,h;
    if(colour_target_==BackBuffer){
        if(!fb_colour_){errors.push_back("back buffer image not set");return false;}
        dkImageViewDefaults(&colour,fb_colour_);
        frame_x_=fb_x_;frame_y_=fb_y_;frame_w_=fb_w_;frame_h_=fb_h_;pc_w_=fb_pc_w_;pc_h_=fb_pc_h_;w=fb_pc_w_;h=fb_pc_h_;
    }else{
        auto* o=object(colour_target_);auto* t=o?object(o->parent):nullptr;
        if(!o||!t||t->image>=images_.size()){errors.push_back("render target surface");return false;}
        dkImageViewDefaults(&colour,&images_[t->image]);
        if(t->cube){colour.type=DkImageType_2DArray;colour.layerOffset=std::uint16_t(o->face);colour.layerCount=1;}
        colour.mipLevelOffset=std::uint8_t(o->level);colour.mipLevelCount=1;
        frame_x_=0;frame_y_=0;frame_w_=o->width;frame_h_=o->height;pc_w_=o->width;pc_h_=o->height;w=o->width;h=o->height;
    }
    const DkImageView* depth_view=nullptr;
    if(depth_target_==DepthBuffer){if(fb_depth_){dkImageViewDefaults(&depth,fb_depth_);depth_view=&depth;}}
    else if(auto* d=object(depth_target_);d&&d->kind==KSurface&&!d->parent&&d->image<images_.size()){dkImageViewDefaults(&depth,&images_[d->image]);depth_view=&depth;}
    dkCmdBufBindRenderTargets(cmd_,colours,1u,depth_view);
    {DkMultisampleState ms;dkMultisampleStateDefaults(&ms);
     if(colour_target_==BackBuffer){ms.mode=fb_ms_;ms.rasterizerMode=fb_ms_;}
     dkCmdBufBindMultisampleState(cmd_,&ms);}
    const std::uint32_t full[6]{0,0,w,h,0,0x3f800000u};PcD3D9Device::set_viewport(full);
    apply_viewport();
    ++target_switches;
    return true;
}
void SwitchD3D9Device::retarget_back_buffer(const DkImage* colour,const DkImage* depth,std::uint32_t x,std::uint32_t y,std::uint32_t w,std::uint32_t h,
                                             std::uint32_t ui_x,std::uint32_t ui_w){
    fb_colour_=colour;fb_depth_=depth;fb_ms_=DkMsMode_1x;fb_x_=x;fb_y_=y;fb_w_=w;fb_h_=h;ui_x_=ui_x;ui_w_=ui_w;
    invalidate_bound();
    if(!cmd_||colour_target_!=BackBuffer||depth_target_!=DepthBuffer)return;
    frame_x_=x;frame_y_=y;frame_w_=w;frame_h_=h;
    DkImageView c,d;dkImageViewDefaults(&c,colour);dkImageViewDefaults(&d,depth);
    const DkImageView* colours[1]{&c};
    dkCmdBufBindRenderTargets(cmd_,colours,1u,&d);
    DkMultisampleState ms;dkMultisampleStateDefaults(&ms);dkCmdBufBindMultisampleState(cmd_,&ms);
    apply_viewport();
    dkCmdBufClearDepthStencil(cmd_,true,1.0f,0xffu,0u);
    ++target_switches;
}
// Particle mode 2 (the 623830 smoke quads of 419DC0, blending ONE / INVSRCALPHA,
// no depth writes, an alpha test that rejects nothing): the draw goes to a
// half-resolution target cleared to (0,0,0,1) with the alpha channel blended
// ZERO / INVSRCALPHA, so rgb holds the smoke colour and a the transmittance. Its
// depth test runs in hardware against a half-resolution depth (the farthest of
// each 2x2 block, pc_particle_depth_fsh). One triangle then adds it to the scene:
// rgb + dst * a. About a quarter of the fill of the full-resolution draw (up to
// 22 screens of smoke on the beach courses). False: the draw is left to the caller.
bool SwitchD3D9Device::draw_particles_half(DkPrimitive prim,std::uint32_t vertices){
    if(colour_target_!=BackBuffer||depth_target_!=DepthBuffer||!fb_colour_||!fb_depth_||fb_ms_!=DkMsMode_1x)return false;
    if(render[d3d9::RS_ZWRITEENABLE]!=0u||render[d3d9::RS_CULLMODE]!=d3d9::CULL_NONE)return false;
    const std::uint32_t af=render[d3d9::RS_ALPHAFUNC];
    if(render[d3d9::RS_ALPHATESTENABLE]!=0u&&af!=8u&&!(af==7u&&render[d3d9::RS_ALPHAREF]==0u))return false;
    const bool ztest=render[d3d9::RS_ZENABLE]!=0u;const std::uint32_t zf=render[d3d9::RS_ZFUNC];
    if(ztest&&(zf<1u||zf>8u))return false;
    constexpr std::uint32_t FullW=1280u,FullH=720u,HalfW=FullW/2u,HalfH=FullH/2u;   // the frame and its half
    const std::uint32_t fx=frame_x_,fy=frame_y_,fw=frame_w_,fh=frame_h_;
    if(fw/2u>HalfW||fh/2u>HalfH||fw<2u||fh<2u)return false;
    if(half_image_==~0u&&!create_image(DkImageType_2D,DkImageFormat_RGBA8_Unorm,HalfW,HalfH,1,1,DkImageFlags_UsageRender,half_image_)){
        half_image_=~0u;return false;}
    if(half_depth_==~0u&&!create_image(DkImageType_2D,DkImageFormat_Z24S8,HalfW,HalfH,1,1,DkImageFlags_UsageRender|DkImageFlags_HwCompression,half_depth_)){
        half_depth_=~0u;return false;}
    auto* desc=static_cast<DkImageDescriptor*>(dkMemBlockGetCpuAddr(descriptors_));
    bool written=false;
    if(depth_desc_==~0u){if(images_.size()>=MaxImages)return false;images_.emplace_back();depth_desc_=std::uint32_t(images_.size()-1);}
    if(depth_desc_image_!=fb_depth_){DkImageView v;dkImageViewDefaults(&v,fb_depth_);dkImageDescriptorInitialize(&desc[depth_desc_],&v,false,false);
        depth_desc_image_=fb_depth_;written=true;}
    const std::array<std::uint32_t,9> linear_clamp{3u,3u,3u,0u,2u,2u,0u,0u,0u};
    const std::uint32_t smp=sampler_lookup(0,linear_clamp);
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    const int probe=probe_begin();
    const float hp[12]{float(fx),float(fy),0.f,0.f, float(fx),float(fy),0.5f/float(HalfW),0.5f/float(HalfH),
                       float(fx),float(fy),1.f/float(FullW),1.f/float(FullH)};
    push_ubo(UboHalf,0x100,0,sizeof(hp),hp);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,3,u+UboHalf,0x100);
    {   DkImageView hv,hd;dkImageViewDefaults(&hv,&images_[half_image_]);dkImageViewDefaults(&hd,&images_[half_depth_]);const DkImageView* hc[1]{&hv};
        dkCmdBufBarrier(cmd_,DkBarrier_Fragments,written?DkInvalidateFlags_Image|DkInvalidateFlags_Descriptors:DkInvalidateFlags_Image);
        dkCmdBufBindRenderTargets(cmd_,hc,1u,&hd);
        DkMultisampleState ms;dkMultisampleStateDefaults(&ms);dkCmdBufBindMultisampleState(cmd_,&ms);
        const DkScissor all{0u,0u,HalfW,HalfH};dkCmdBufSetScissors(cmd_,0,&all,1);
        dkCmdBufClearColorFloat(cmd_,0,DkColorMask_RGBA,0.f,0.f,0.f,1.f);
        frame_x_=0u;frame_y_=0u;frame_w_=fw/2u;frame_h_=fh/2u;apply_viewport();
        frame_x_=fx;frame_y_=fy;frame_w_=fw;frame_h_=fh;
        DkRasterizerState rs;dkRasterizerStateDefaults(&rs);rs.cullMode=DkFace_None;dkCmdBufBindRasterizerState(cmd_,&rs);
        // Half-resolution depth (colour writes off, depth always written).
        if(ztest){
            DkColorWriteState ws;dkColorWriteStateDefaults(&ws);dkColorWriteStateSetMask(&ws,0,0);dkCmdBufBindColorWriteState(cmd_,&ws);
            DkDepthStencilState ds;dkDepthStencilStateDefaults(&ds);ds.depthTestEnable=true;ds.depthWriteEnable=true;ds.depthCompareOp=DkCompareOp_Always;
            dkCmdBufBindDepthStencilState(cmd_,&ds);
            dkCmdBufBindTexture(cmd_,DkStage_Fragment,12,dkMakeTextureHandle(depth_desc_,smp));
            const DkShader* shaders[2]{&particle_comp_vs_,&particle_depth_fs_};
            dkCmdBufBindShaders(cmd_,DkStageFlag_GraphicsMask,shaders,2);
            dkCmdBufDraw(cmd_,DkPrimitive_Triangles,3,1,0,0);
        }
        // The smoke: the game's depth test, no depth writes.
        DkDepthStencilState ds;dkDepthStencilStateDefaults(&ds);ds.depthTestEnable=ztest;ds.depthWriteEnable=false;
        ds.depthCompareOp=DkCompareOp(ztest?zf:8u);
        dkCmdBufBindDepthStencilState(cmd_,&ds);
        DkColorState cs;dkColorStateDefaults(&cs);dkColorStateSetBlendEnable(&cs,0,true);dkCmdBufBindColorState(cmd_,&cs);
        DkColorWriteState ws;dkColorWriteStateDefaults(&ws);dkColorWriteStateSetMask(&ws,0,DkColorMask_RGBA);dkCmdBufBindColorWriteState(cmd_,&ws);
        DkBlendState bs;dkBlendStateDefaults(&bs);
        dkBlendStateSetFactors(&bs,DkBlendFactor_One,DkBlendFactor_InvSrcAlpha,DkBlendFactor_Zero,DkBlendFactor_InvSrcAlpha);
        dkBlendStateSetOps(&bs,DkBlendOp_Add,DkBlendOp_Add);dkCmdBufBindBlendState(cmd_,0,&bs);
        // The vertex input of the caller (dkCmdBufBindVtx*) is still bound.
        const DkShader* shaders[2]{&ffp_vsps_,&particle_half_fs_};
        dkCmdBufBindShaders(cmd_,DkStageFlag_GraphicsMask,shaders,2);
        dkCmdBufDraw(cmd_,prim,vertices,1,0,0);
    }
    // Composite over the scene (no depth test, the D3D viewport rectangle).
    {   DkImageView c,d;dkImageViewDefaults(&c,fb_colour_);dkImageViewDefaults(&d,fb_depth_);const DkImageView* cc[1]{&c};
        dkCmdBufBarrier(cmd_,DkBarrier_Fragments,DkInvalidateFlags_Image);
        dkCmdBufBindRenderTargets(cmd_,cc,1u,&d);
        apply_viewport();
        DkDepthStencilState ds;dkDepthStencilStateDefaults(&ds);ds.depthTestEnable=false;ds.depthWriteEnable=false;dkCmdBufBindDepthStencilState(cmd_,&ds);
        DkBlendState bs;dkBlendStateDefaults(&bs);
        dkBlendStateSetFactors(&bs,DkBlendFactor_One,DkBlendFactor_SrcAlpha,DkBlendFactor_Zero,DkBlendFactor_One);
        dkBlendStateSetOps(&bs,DkBlendOp_Add,DkBlendOp_Add);dkCmdBufBindBlendState(cmd_,0,&bs);
        dkCmdBufBindVtxAttribState(cmd_,nullptr,0);
        dkCmdBufBindTexture(cmd_,DkStage_Fragment,0,dkMakeTextureHandle(half_image_,smp));
        const DkShader* shaders[2]{&particle_comp_vs_,&particle_comp_fs_};
        dkCmdBufBindShaders(cmd_,DkStageFlag_GraphicsMask,shaders,2);
        dkCmdBufDraw(cmd_,DkPrimitive_Triangles,3,1,0,0);
    }
    probe_end(probe);
    invalidate_bound();
    return true;
}
// Occlusion queries: SamplesPassed reported before (BEGIN) and after (END)
// the query's draws into reports_ (32 bytes per query: two {u64 counter,
// u64 timestamp} reports). The result is read once the frame that issued it
// has completed on the GPU, i.e. two begin_frame calls later (the frame slots
// alternate and each waits for its previous use).
// Stencil draw probes (g_pc_stencil_probe): a SamplesPassed report {counter,
// timestamp} before and after each draw with the stencil test on. Read back when
// the frame slot comes round again (its fence has been waited on).
int SwitchD3D9Device::probe_begin(){
    const unsigned zone=std::min(platform::g_pc_gpu_zone,6u);
    if(!probes_||!cmd_||(render[52]==0u&&zone==0u)||probe_n_[slot_]>=MaxProbes)return -1;
    const unsigned n=probe_n_[slot_]++;
    probe_kind_[slot_][n]=std::uint8_t(zone?zone+1u:(render[d3d9::RS_COLORWRITEENABLE]&0xfu)==0u?0u:1u);
    dkCmdBufReportCounter(cmd_,DkCounter_SamplesPassed,dkMemBlockGetGpuAddr(probes_)+(slot_*MaxProbes+n)*32u);
    return int(n);
}
void SwitchD3D9Device::probe_end(int probe){
    if(probe<0)return;
    dkCmdBufReportCounter(cmd_,DkCounter_SamplesPassed,dkMemBlockGetGpuAddr(probes_)+(slot_*MaxProbes+unsigned(probe))*32u+16u);
}
void SwitchD3D9Device::collect_probes(){
    if(!probes_)return;
    PcStencilProbe p;
    const auto* base=static_cast<const std::uint8_t*>(dkMemBlockGetCpuAddr(probes_))+slot_*MaxProbes*32u;
    for(unsigned k=0;k<probe_n_[slot_];++k){
        std::uint64_t c0,t0,c1,t1;const auto* r=base+k*32u;
        std::memcpy(&c0,r,8);std::memcpy(&t0,r+8,8);std::memcpy(&c1,r+16,8);std::memcpy(&t1,r+24,8);
        const unsigned kind=probe_kind_[slot_][k];
        if(t1>t0)p.ms[kind]+=double(dkTimestampToNs(t1-t0))/1e6;
        if(c1>c0)p.samples[kind]+=c1-c0;
        ++p.draws[kind];
    }
    if(probe_n_[slot_])g_pc_stencil_probe=p;
    probe_n_[slot_]=0;
}
std::uint32_t SwitchD3D9Device::create_occlusion_query(){
    if(!reports_||queries_.size()>=MaxQueries)return 0;
    queries_.push_back({});return QueryBase+std::uint32_t(queries_.size()-1u);
}
void SwitchD3D9Device::query_issue(std::uint32_t handle,std::uint32_t flags){
    if(handle<QueryBase||handle-QueryBase>=queries_.size()||!cmd_)return;
    auto& q=queries_[handle-QueryBase];
    const auto gpu=dkMemBlockGetGpuAddr(reports_)+(handle-QueryBase)*32u;
    if(flags&2u){dkCmdBufReportCounter(cmd_,DkCounter_SamplesPassed,gpu);q.begun=true;}
    if((flags&1u)&&q.begun){dkCmdBufReportCounter(cmd_,DkCounter_SamplesPassed,gpu+16u);q.begun=false;q.pending=true;q.issued=frame_serial_;}
}
std::uint32_t SwitchD3D9Device::query_get_data(std::uint32_t handle,std::uint32_t& samples){
    samples=0;
    if(handle<QueryBase||handle-QueryBase>=queries_.size())return 1u;
    auto& q=queries_[handle-QueryBase];
    if(q.begun||(q.pending&&frame_serial_<q.issued+2u))return 1u;
    if(q.pending){
        const auto* r=static_cast<const std::uint8_t*>(dkMemBlockGetCpuAddr(reports_))+(handle-QueryBase)*32u;
        std::uint64_t a,b;std::memcpy(&a,r,8);std::memcpy(&b,r+16,8);
        q.result=std::uint32_t(b-a);q.pending=false;
    }
    samples=q.result;return 0u;
}
void SwitchD3D9Device::set_render_target(std::uint32_t index,std::uint32_t surface){
    if(index!=0u||surface==0u){errors.push_back("SetRenderTarget: index 0 and a surface only");return;}
    if(surface!=BackBuffer){
        auto* o=object(surface);auto* t=o?object(o->parent):nullptr;
        if(!o||o->kind!=KSurface||!t||t->kind!=KTexture){errors.push_back("SetRenderTarget: not a texture surface");return;}
    }
    const bool to_back=surface==BackBuffer&&colour_target_!=BackBuffer;
    colour_target_=surface;
    if(to_back&&cmd_)dkCmdBufBarrier(cmd_,DkBarrier_Fragments,DkInvalidateFlags_Image);   // the offscreen target is sampled next
    bind_targets();
}
void SwitchD3D9Device::set_depth_stencil_surface(std::uint32_t surface){
    if(surface==depth_target_)return;
    if(surface!=DepthBuffer&&surface!=0u){
        auto* d=object(surface);
        if(!d||d->kind!=KSurface||d->parent){errors.push_back("SetDepthStencilSurface: not a depth surface");return;}
    }
    depth_target_=surface;
    bind_targets();
}
void SwitchD3D9Device::begin_frame(DkCmdBuf cmd,std::uint32_t vx,std::uint32_t vy,std::uint32_t vw,std::uint32_t vh,
                                   std::uint32_t pc_w,std::uint32_t pc_h){
    cmd_=cmd;draws=0;up_used_=0;++frame_serial_;
    frame_x_=vx;frame_y_=vy;frame_w_=vw;frame_h_=vh;pc_w_=pc_w?pc_w:640u;pc_h_=pc_h?pc_h:480u;
    fb_x_=frame_x_;fb_y_=frame_y_;fb_w_=frame_w_;fb_h_=frame_h_;fb_pc_w_=pc_w_;fb_pc_h_=pc_h_;
    colour_target_=BackBuffer;depth_target_=DepthBuffer;
    const std::uint32_t full[6]{0,0,pc_w_,pc_h_,0,0x3f800000u};PcD3D9Device::set_viewport(full);
    rebind_frame_resources();
    // Viewport, scissor and the D3D9 half-pixel fix of the full PC screen;
    // the back buffer (the frame, or the scaled offscreen image) is bound here.
    apply_viewport();
    bind_targets();
}
void SwitchD3D9Device::rebind_frame_resources(){
    invalidate_bound();
    if(!cmd_)return;
    const auto gpu=dkMemBlockGetGpuAddr(descriptors_);
    dkCmdBufBindImageDescriptorSet(cmd_,gpu,MaxImages);
    dkCmdBufBindSamplerDescriptorSet(cmd_,gpu+MaxImages*sizeof(DkImageDescriptor),MaxSamplers);
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,0,u+UboVsConst,0x1100);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,1,u+UboVsProgram,0x100);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Vertex,2,u+UboVsFix,0x100);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,0,u+UboPsConst,0x100);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,1,u+UboPsProgram,0x200);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,2,u+UboPsFixed,0x100);
}
void SwitchD3D9Device::clear(std::uint32_t argb,float depth,bool colour,bool zbuffer){
    if(colour)dkCmdBufClearColorFloat(cmd_,0,DkColorMask_RGBA,((argb>>16)&255)/255.f,((argb>>8)&255)/255.f,(argb&255)/255.f,(argb>>24)/255.f);
    if(zbuffer)dkCmdBufClearDepthStencil(cmd_,true,depth,0xff,0);
}
void SwitchD3D9Device::end_frame(){
    invalidate_bound();
    // Leave default states for the command lists that follow (UI).
    DkRasterizerState rs;dkRasterizerStateDefaults(&rs);rs.cullMode=DkFace_None;dkCmdBufBindRasterizerState(cmd_,&rs);
    DkColorState cs;dkColorStateDefaults(&cs);dkCmdBufBindColorState(cmd_,&cs);
    DkColorWriteState ws;dkColorWriteStateDefaults(&ws);dkCmdBufBindColorWriteState(cmd_,&ws);
    DkDepthStencilState ds;dkDepthStencilStateDefaults(&ds);dkCmdBufBindDepthStencilState(cmd_,&ds);
    dkCmdBufSetDepthBias(cmd_,0.f,0.f,0.f);
    cmd_=nullptr;
}
std::uint32_t SwitchD3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t,std::uint32_t,std::uint32_t start,std::uint32_t count){
    if(!cmd_)return 1;
    ProfileScope prof(profile_key(ProfileRenderer,0,ProfDeviceDraw));
    auto* decl=object(declaration);auto* vs=object(vertex_shader);auto* ps=object(pixel_shader);auto* ib=object(indices);
    ++dip.calls;
    if(!decl){++dip.no_decl;return 1;}
    if(!vs||vs->kind!=KVs){++dip.no_vs;return 1;}
    if(!ib){++dip.no_ib;return 1;}
    // Console diagnostic 5..6: only the opaque alpha-tested draws (ALPHAREF 0x80), painted red.
    if(diag_mode>=5u&&!(render[d3d9::RS_ALPHATESTENABLE]&&(render[d3d9::RS_ALPHAREF]&0xffu)==0x80u))return 0;
    if(!ps||ps->kind!=KPs)++dip.ffp_ps;
    if(render[d3d9::RS_ALPHATESTENABLE]){auto* t=object(textures[0]);if(!t||t->kind!=KTexture||!t->image)++dip.alpha_no_image;}
    if(diag_mode!=0u){   // CPU check of the first vertex of the draw (console diagnostic modes only: it costs a CPU vertex shader run per draw).
        pc_shader::VsState s;std::array<pc_shader::V4,256> cst;
        for(unsigned k=0;k<256;++k)cst[k]={vs_constants[k][0],vs_constants[k][1],vs_constants[k][2],vs_constants[k][3]};
        s.C=cst.data();for(auto& v:s.V)v={0,0,0,1};
        bool ok=std::size_t(start)*2u+2u<=ib->size;
        std::uint16_t index=0;if(ok)std::memcpy(&index,ib->cpu+std::size_t(start)*2u,2);
        for(const auto& e:decl->elements){
            if(!ok||e.stream==platform::PcDeclEndStream)break;
            auto* b=e.stream<4?object(streams[e.stream].buffer):nullptr;if(!b||!b->cpu){ok=false;break;}
            const std::size_t at=std::size_t(streams[e.stream].offset)+std::size_t(std::int64_t(base)+index)*streams[e.stream].stride+e.offset;
            if(at+16u>b->size&&e.type<=3){ok=false;break;}
            pc_shader::V4 v{0,0,0,1};float f[4]{0,0,0,1};
            if(e.type<=3){std::memcpy(f,b->cpu+at,4u*(e.type+1u));v={f[0],f[1],f[2],e.type==3?f[3]:1.f};}
            for(const auto& in:vs->program.inputs)if(in.usage==e.usage&&in.usage_index==e.usage_index&&in.reg<16)s.V[in.reg]=v;
        }
        if(!ok)++dip.cpu_unread;
        else{
            platform::pc_shader_run_vs(vs->program,s);
            const auto& p=s.oPos;
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.w))++dip.cpu_bad;
            else if(!(p.w>0.f))++dip.cpu_behind;
            else if(std::fabs(p.x)<=p.w*1.2f&&std::fabs(p.y)<=p.w*1.2f&&p.z>=0.f&&p.z<=p.w)++dip.cpu_on;
            else ++dip.cpu_off;
        }
    }
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    // Vertex input: declaration elements -> v registers declared by the shader.
    std::array<DkVtxBufferState,4> bufs{};
    if(!g_pc_state_cache)invalidate_bound();
    if(vs->attr_decl!=declaration||!g_pc_state_cache){   // declaration elements -> the shader's inputs, once per (shader, declaration)
    std::array<DkVtxAttribState,16> attribs{};
    for(auto& a:attribs){a={};a.isFixed=1;a.size=DkVtxAttribSize_4x32;a.type=DkVtxAttribType_Float;}
    std::uint32_t max_stream=0;
    for(const auto& e:decl->elements){
        if(e.stream==PcDeclEndStream)break;
        for(const auto& in:vs->program.inputs){
            if(in.usage!=e.usage||in.usage_index!=e.usage_index||in.reg>=16)continue;
            auto& a=attribs[in.reg];a={};a.bufferId=e.stream;a.offset=e.offset;
            switch(e.type){
            case 0:a.size=DkVtxAttribSize_1x32;a.type=DkVtxAttribType_Float;break;
            case 1:a.size=DkVtxAttribSize_2x32;a.type=DkVtxAttribType_Float;break;
            case 2:a.size=DkVtxAttribSize_3x32;a.type=DkVtxAttribType_Float;break;
            case 3:a.size=DkVtxAttribSize_4x32;a.type=DkVtxAttribType_Float;break;
            case 4:a.size=DkVtxAttribSize_4x8;a.type=DkVtxAttribType_Unorm;a.isBgra=1;break;
            default:a.size=DkVtxAttribSize_4x8;a.type=DkVtxAttribType_Uscaled;break;
            }
        }
        max_stream=std::max<std::uint32_t>(max_stream,e.stream+1u);
    }
    vs->attr=attribs;vs->attr_streams=max_stream;vs->attr_decl=declaration;
    }
    const auto& attribs=vs->attr;const std::uint32_t max_stream=vs->attr_streams;
    if(!bound_.vertex){bound_.vb.fill(~DkGpuAddr(0));bound_.buf_count=0;}
    for(std::uint32_t s=0;s<max_stream&&s<4;++s){
        bufs[s]={streams[s].stride,0};
        if(auto* b=object(streams[s].buffer)){
            const DkGpuAddr at=b->gpu+streams[s].offset;const std::uint32_t size=b->size-streams[s].offset;
            if(bound_.vb[s]==at&&bound_.vb_size[s]==size)++binds_skipped;
            else{dkCmdBufBindVtxBuffer(cmd_,s,at,size);bound_.vb[s]=at;bound_.vb_size[s]=size;}
        }
    }
    const std::uint32_t buf_count=std::max(1u,max_stream);
    if(bound_.vertex&&!std::memcmp(bound_.attribs.data(),attribs.data(),sizeof attribs))++binds_skipped;
    else{dkCmdBufBindVtxAttribState(cmd_,attribs.data(),16);std::memcpy(bound_.attribs.data(),attribs.data(),sizeof attribs);}
    if(bound_.vertex&&bound_.buf_count==buf_count&&!std::memcmp(bound_.bufs.data(),bufs.data(),sizeof(DkVtxBufferState)*buf_count))++binds_skipped;
    else{dkCmdBufBindVtxBufferState(cmd_,bufs.data(),buf_count);std::memcpy(bound_.bufs.data(),bufs.data(),sizeof bufs);bound_.buf_count=buf_count;}
    bound_.vertex=true;
    if(bound_.index&&bound_.ib==ib->gpu)++binds_skipped;
    else{dkCmdBufBindIdxBuffer(cmd_,DkIdxFormat_Uint16,ib->gpu);bound_.index=true;bound_.ib=ib->gpu;}
    bind_programmable(vs,ps);
    apply_render_states();
    const std::uint32_t indices_count=type==5?count+2:type==4?count*3:0;
    if(!indices_count)return 1;
    dkCmdBufDrawIndexed(cmd_,type==5?DkPrimitive_TriangleStrip:DkPrimitive_Triangles,indices_count,1,start,base,0);
    ++draws;++dip.drawn;note_dip_state(count);
    return 0;
}
std::string SwitchD3D9Device::diagnostics()const{
    char t[600];
    std::snprintf(t,sizeof t,"diag_mode=%u dip calls=%u ffp_ps=%u up_ps=%u drawn=%u no_decl=%u no_vs=%u no_ib=%u alpha_no_image=%u cpu_first_vertex on=%u off=%u behind=%u bad=%u unread=%u "
        "shaders spec=%zu vs spec/uber=%u/%u ps spec/uber=%u/%u vs_const_bytes=%u push bytes=%u skipped=%u binds_skipped=%u errors:",
        diag_mode,dip.calls,dip.ffp_ps,dip.up_ps,dip.drawn,dip.no_decl,dip.no_vs,dip.no_ib,dip.alpha_no_image,dip.cpu_on,dip.cpu_off,dip.cpu_behind,dip.cpu_bad,dip.cpu_unread,
        spec_.size(),dip.spec_vs,dip.uber_vs,dip.spec_ps,dip.uber_ps,dip.const_bytes,dip.push_bytes,dip.push_skipped,binds_skipped);
    std::string out=t;
    out+=" states:"+dip_state_summary(12);
    std::map<std::string,std::uint32_t> counts;for(const auto& e:errors)++counts[e];
    for(const auto& [e,n]:counts){out+=" ["+e+"] x"+std::to_string(n);}
    return out;
}
void SwitchD3D9Device::push_ffp_stages(){
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    struct {std::int32_t op[4][4],arg[4][4],misc[4][4];float konst[4][4],tfactor[4];std::int32_t flags[4];} f{};
    for(unsigned st=0;st<4;++st){
        const auto& t=stage[st];
        f.op[st][0]=std::int32_t(t[1]);f.op[st][1]=std::int32_t(t[2]);f.op[st][2]=std::int32_t(t[3]);f.op[st][3]=std::int32_t(t[4]);
        f.arg[st][0]=std::int32_t(t[5]);f.arg[st][1]=std::int32_t(t[6]);f.arg[st][2]=std::int32_t(t[26]);f.arg[st][3]=std::int32_t(t[27]);
        auto* tx=object(textures[st]);
        f.misc[st][0]=std::int32_t(t[28]);f.misc[st][1]=std::int32_t(t[11]&7u);f.misc[st][2]=tx&&tx->kind==KTexture&&!tx->cube?1:0;f.misc[st][3]=tx&&tx->kind==KTexture&&tx->cube?1:0;
        const auto k=t[32];f.konst[st][0]=((k>>16)&255)/255.f;f.konst[st][1]=((k>>8)&255)/255.f;f.konst[st][2]=(k&255)/255.f;f.konst[st][3]=(k>>24)/255.f;
    }
    const auto tf=render[d3d9::RS_TEXTUREFACTOR];
    f.tfactor[0]=((tf>>16)&255)/255.f;f.tfactor[1]=((tf>>8)&255)/255.f;f.tfactor[2]=(tf&255)/255.f;f.tfactor[3]=(tf>>24)/255.f;
    f.flags[0]=render[d3d9::RS_SPECULARENABLE]?1:0;
    push_ubo(UboPsProgram,0x200,0,sizeof(f),&f);
    dkCmdBufBindUniformBuffer(cmd_,DkStage_Fragment,1,u+UboPsProgram,0x200);
}
// Shaders, their programs and constants, the pixel stage textures.
// dkCmdBufPushConstants into the device uniform block, skipped when the
// bytes equal what the earlier pushes left there (ubo_shadow_ follows the
// block in command order; it starts zeroed like the block).
void SwitchD3D9Device::push_ubo(std::uint32_t at,std::uint32_t range,std::uint32_t off,std::uint32_t size,const void* data){
    if(at+off+size<=ubo_shadow_.size()&&!std::memcmp(ubo_shadow_.data()+at+off,data,size)){dip.push_skipped+=size;return;}
    dkCmdBufPushConstants(cmd_,dkMemBlockGetGpuAddr(uniforms_)+at,range,off,size,data);
    if(at+off+size<=ubo_shadow_.size())std::memcpy(ubo_shadow_.data()+at+off,data,size);
    dip.push_bytes+=size;
}
std::int32_t SwitchD3D9Device::find_spec(std::uint32_t kind,const std::vector<std::uint32_t>& blobs){
    if(g_pc_spec_disabled)return -1;
    for(std::size_t i=0;i<spec_.size();++i)if(spec_[i].kind==kind&&spec_[i].blobs==blobs)return std::int32_t(i);
    // Reported once per created shader: add the line to tools/pc_spec_programs.txt.
    std::string line=std::string("uber fallback ")+(kind==0u?"vs":kind==1u?"ps11":"ps14");
    for(auto b:blobs)line+=" "+std::to_string(b);
    errors.push_back(line);
    return -1;
}
void SwitchD3D9Device::bind_programmable(Object* vs,Object* ps){
    const auto u=dkMemBlockGetGpuAddr(uniforms_);
    if(ps&&ps->kind!=KPs)ps=nullptr;
    // SetPixelShader(NULL): the fixed-function texture stage cascade (an
    // empty ps14 program would output black with alpha 0 and fail the
    // alpha test of every opaque material).
    // Specialised shaders unless a console diagnostic mode needs the uber ones.
    const bool spec_vs=vs->spec>=0&&diag_mode==0u,spec_ps=ps&&ps->spec>=0&&diag_mode==0u;
    const DkShader* shaders[2]{spec_vs?&spec_[std::size_t(vs->spec)].shader:&vs_,
        !ps?&ffp_vsin_fs_:spec_ps?&spec_[std::size_t(ps->spec)].shader:ps->program.version==0xffff0101u?&ps11_:&ps14_};
    if(spec_vs)++dip.spec_vs;else ++dip.uber_vs;
    if(ps){if(spec_ps)++dip.spec_ps;else ++dip.uber_ps;}
    if(bound_.shaders&&bound_.shader[0]==shaders[0]&&bound_.shader[1]==shaders[1])++binds_skipped;
    else{dkCmdBufBindShaders(cmd_,DkStageFlag_GraphicsMask,shaders,2);bound_.shaders=true;bound_.shader[0]=shaders[0];bound_.shader[1]=shaders[1];}
    {   float prog[20]{};for(int k=0;k<16;++k)prog[k]=-1.f;for(std::size_t k=0;k<vs->program.blobs.size()&&k<16;++k)prog[k]=float(vs->program.blobs[k]);
        prog[16]=std::int32_t(vs->program.blobs.size());
        push_ubo(UboVsConst,0x1100,UboVsProgram,sizeof(prog),prog);}   // same buffer as the binding (u, 0x1100)
    // Only the registers set since the last push: push constants update the
    // buffer in command order, so the rest already holds the current values
    // (it was cleared with vs_constants at initialisation).
    if(vs_dirty_hi>vs_dirty_lo){
        const std::uint32_t bytes=(vs_dirty_hi-vs_dirty_lo)*16u;
        push_ubo(UboVsConst,0x1100,vs_dirty_lo*16u,bytes,vs_constants[vs_dirty_lo].data());
        dip.const_bytes+=bytes;vs_dirty_lo=256u;vs_dirty_hi=0u;
    }
    if(ps)(void)bind_pixel_program(ps);
    else{
        push_ffp_stages();
        push_ubo(UboPsConst,0x100,0,sizeof(float)*32,ps_constants.data());
        push_ps_fixed();
        bind_pixel_textures();
    }
}
// The pixel program words, constants, alpha test / fog and textures of a
// bound pixel shader; returns its shader (specialised unless a diagnostic mode).
const DkShader* SwitchD3D9Device::bind_pixel_program(Object* ps){
    struct { std::int32_t program[16];std::int32_t info[4];std::int32_t kind[8];float bump[8][4];float lum[2][4]; } p{};
    for(int k=0;k<16;++k)p.program[k]=-1;                              // list terminator (see the uber shaders)
    for(std::size_t k=0;k<ps->program.blobs.size();++k)p.program[k]=std::int32_t(ps->program.blobs[k]);
    p.info[0]=std::int32_t(ps->program.blobs.size());
    for(unsigned s=0;s<8;++s){
        auto* t=object(textures[s]);p.kind[s]=t&&t->kind==KTexture&&t->cube?1:0;
        for(unsigned k=0;k<4;++k)p.bump[s][k]=d3d9::f(stage[s][7+k]);
    }
    for(unsigned s=0;s<4;++s){p.lum[s>>1][(s&1)*2]=d3d9::f(stage[s][22]);p.lum[s>>1][(s&1)*2+1]=d3d9::f(stage[s][23]);}
    push_ubo(UboPsProgram,0x200,0,sizeof(p),&p);
    push_ubo(UboPsConst,0x100,0,sizeof(float)*32,ps_constants.data());
    push_ps_fixed();
    bind_pixel_textures();
    const bool spec=ps->spec>=0&&diag_mode==0u;
    return spec?&spec_[std::size_t(ps->spec)].shader:ps->program.version==0xffff0101u?&ps11_:&ps14_;
}
// Textures: 2D bindings 0..5, cube bindings 6..11.
void SwitchD3D9Device::bind_pixel_textures(){
    std::array<DkResHandle,12> h{};
    for(unsigned s=0;s<6;++s){
        auto* t=object(textures[s]);const auto smp=sampler_index(s);
        if(t&&t->kind!=KTexture)t=nullptr;
        h[s]=dkMakeTextureHandle(t&&!t->cube?t->image:dummy2d_,smp);h[6+s]=dkMakeTextureHandle(t&&t->cube?t->image:dummycube_,smp);
    }
    if(bound_.textures&&bound_.texture==h){++binds_skipped;return;}
    dkCmdBufBindTextures(cmd_,DkStage_Fragment,0,h.data(),12);bound_.textures=true;bound_.texture=h;
}
}
#endif
