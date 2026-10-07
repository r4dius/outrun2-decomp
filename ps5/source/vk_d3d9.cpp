#include "vk_d3d9.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <chrono>
#include <cstdlib>
#include <cstdio>
namespace outrun::ps5_runtime {
using namespace vulkan;
using namespace platform::d3d9;
VulkanD3D9Device::VulkanD3D9Device(Context& context,unsigned w,unsigned h,bool frame_queries):PcSoftD3D9Device(w,h),context_(context),frame_queries_(frame_queries){
    static_assert(sizeof(Vertex)==180,"Vulkan vertex layout must match recovered vertex records");
    retire_listener_=context_.add_retire_listener([this](std::uint64_t epoch,const std::vector<std::uint64_t>& samples){retire_queries(epoch,samples);});
    resize_back_buffer(w,h);dummy2d_=context_.image(1,1);dummycube_=context_.image(1,1,1,6);
    const unsigned char black[4]{0,0,0,255};context_.upload(dummy2d_,0,0,black,4);
    for(unsigned f=0;f<6;++f)context_.upload(dummycube_,f,0,black,4);
    context_.clear({back_,back_depth_},7,0xff000000,1,0);
}
VulkanD3D9Device::~VulkanD3D9Device(){try{context_.finish();}catch(...){}context_.remove_retire_listener(retire_listener_);}
void VulkanD3D9Device::resize_back_buffer(unsigned w,unsigned h){
    if(back_&&back_->width==w&&back_->height==h)return;
    context_.end_pass();back_=context_.image(w,h);back_depth_=context_.image(w,h,1,1,context_.depth_format());
    msaa_colour_.reset();msaa_depth_.reset();fxaa_copy_.reset();
}
Texture VulkanD3D9Device::texture(std::uint32_t handle){
    auto* o=object(handle);if(!o||o->kind!=5)return {};
    if(auto it=images_.find(handle);it!=images_.end())return it->second;
    auto t=context_.image(o->texture.width,o->texture.height,o->texture.levels,o->texture.cube?6:1);
    for(unsigned face=0;face<o->rgba.size();++face)for(unsigned level=0;level<o->rgba[face].size();++level){
        const auto& rgba=o->rgba[face][level];if(!rgba.empty())context_.upload(t,face,level,rgba.data(),rgba.size());
    }
    images_[handle]=t;return t;
}
Texture VulkanD3D9Device::depth(std::uint32_t handle){
    if(handle==DepthBuffer)return back_depth_;if(!handle)return {};
    if(auto it=depths_.find(handle);it!=depths_.end())return it->second;
    auto* o=object(handle);if(!o||o->kind!=6||o->parent)throw std::runtime_error("Invalid Vulkan depth surface");
    return depths_[handle]=context_.image(o->width,o->height,1,1,context_.depth_format());
}
Target VulkanD3D9Device::target(){
    if(colour_target_==BackBuffer){
        if(msaa_live_){if(depth_target_!=DepthBuffer&&depth_target_)throw std::runtime_error("Multisampled back buffer with separate depth surface");
            return {msaa_colour_,depth_target_?msaa_depth_:Texture{}};}
        return {back_,depth(depth_target_)};
    }
    auto* surface=object(colour_target_);if(!surface||!surface->parent)throw std::runtime_error("Invalid Vulkan colour surface");
    return {texture(surface->parent),depth(depth_target_),surface->face,surface->level};
}
void VulkanD3D9Device::release(std::uint32_t handle){
    PcSoftD3D9Device::release(handle);if(object(handle))return;
    // Context retains every image referenced by commands until the fence.
    images_.erase(handle);depths_.erase(handle);geometry_.erase(handle);index_ranges_.erase(handle);
}
void VulkanD3D9Device::unlock_rect(std::uint32_t handle){
    auto* s=object(handle);const auto parent=s?s->parent:0;PcSoftD3D9Device::unlock_rect(handle);
    if(!parent||!images_.count(parent))return;s=object(handle);auto* p=object(parent);
    const auto& rgba=p->rgba[s->face][s->level];context_.upload(images_.at(parent),s->face,s->level,rgba.data(),rgba.size());
}
void VulkanD3D9Device::clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil){if(!count_only)context_.clear(target(),flags,colour,z,stencil);}
void VulkanD3D9Device::raster(const Vertex* vertices,unsigned count){
    if(count<3)return;batch_rhw_=pretransformed_;
    auto push=[&](const Vertex& source){auto v=source;
        if(pretransformed_){const float w=1.f/v.pos[3];v.pos[0]=(2*(v.pos[0]+.5f-soft_viewport_.x)/soft_viewport_.w-1)*w;
            v.pos[1]=(1-2*(v.pos[1]+.5f-soft_viewport_.y)/soft_viewport_.h)*w;v.pos[2]*=w;v.pos[3]=w;}
        else{v.pos[0]+=v.pos[3]/soft_viewport_.w;v.pos[1]-=v.pos[3]/soft_viewport_.h;}
        batch_.push_back(v);};
    for(unsigned i=1;i+1<count;++i){push(vertices[0]);push(vertices[i]);push(vertices[i+1]);++triangles_drawn;}
}
std::uint32_t VulkanD3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t min,std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    const auto begin=sample_draw_times_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    batch_.clear();native_vertices_=false;
    if(menu_trace_draws_){--menu_trace_draws_;auto* ib=object(indices);auto* vs=object(vertex_shader);auto* ps=object(pixel_shader);
        std::fprintf(stdout,"[vk-menu-draw] type=%u count=%u VS=%u PS=%u target=%x depth=%x z=%u/%u/%u cull=%u colour=%x blend=%u/%u/%u alpha=%u/%u/%u viewport=%.0f,%.0f,%.0f,%.0f\n",
            type,count,vs&&!vs->program.blobs.empty()?vs->program.blobs[0]:999u,ps&&!ps->program.blobs.empty()?ps->program.blobs[0]:999u,
            colour_target_,depth_target_,render[RS_ZENABLE],render[RS_ZWRITEENABLE],render[RS_ZFUNC],render[RS_CULLMODE],render[RS_COLORWRITEENABLE],
            render[RS_ALPHABLENDENABLE],render[RS_SRCBLEND],render[RS_DESTBLEND],render[RS_ALPHATESTENABLE],render[RS_ALPHAREF],render[RS_ALPHAFUNC],
            float(soft_viewport_.x),float(soft_viewport_.y),float(soft_viewport_.w),float(soft_viewport_.h));
        if(ib&&count)for(unsigned k=0;k<3&&2*(std::size_t(start)+k+1)<=ib->bytes.size();++k){std::uint16_t i;std::memcpy(&i,ib->bytes.data()+2*(std::size_t(start)+k),2);Vertex v{};
            if(shade_vertex(i,base,v))std::fprintf(stdout,"[vk-menu-vertex] i=%u clip=%.6g,%.6g,%.6g,%.6g diffuse=%.3g,%.3g,%.3g,%.3g\n",unsigned(i),v.pos[0],v.pos[1],v.pos[2],v.pos[3],v.d[0].x,v.d[0].y,v.d[0].z,v.d[0].w);
        }
    }
    std::uint32_t result;
    if(!count_only&&prepare_native(type,base,start,count)){
        note_dip_state(count);if(hash_draws)hash_draw(1,{type,std::uint32_t(base),min,vertices,start,count});result=0;
    }else result=prepare_indexed(type,base,min,vertices,start,count);
    const auto prepared=sample_draw_times_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};flush();
    if(sample_draw_times_){const auto recorded=std::chrono::steady_clock::now();
        vertex_prepare_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(prepared-begin).count();
        command_record_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(recorded-prepared).count();}return result;
}
bool VulkanD3D9Device::prepare_native(std::uint32_t type,std::int32_t base,std::uint32_t start,std::uint32_t count){
    static const bool cpu=std::getenv("OR2_PS5_CPU_VERTEX")||std::getenv("OR2_DBG_DRAWSTATS")||std::getenv("OR2_DBG_ONLY");
    if(cpu||!count||!targets_consistent())return false;
    auto* ib=object(indices);auto* decl=object(declaration);auto* vs=object(vertex_shader);
    const std::size_t total=type==5?std::size_t(count)+2:type==4?std::size_t(count)*3:0;
    if(!ib||!decl||!vs||vs->kind!=3||vs->program.blobs.size()>32||!total||(std::size_t(start)+total)*2>ib->bytes.size())return false;
    // The index contents change only at Unlock. Keep bounds per submitted
    // range, while validating the current base/streams on every draw below.
    // MinVertexIndex/NumVertices are advisory D3D parameters, not trusted bounds.
    auto& ranges=index_ranges_[indices];const auto key=(std::uint64_t(start)<<32)|total;
    auto range=ranges.find(key);
    if(range==ranges.end()){
        IndexRange bounds{65535,0};
        for(std::size_t k=start;k<std::size_t(start)+total;++k){std::uint16_t i;std::memcpy(&i,ib->bytes.data()+k*2,2);bounds.low=std::min(bounds.low,unsigned(i));bounds.high=std::max(bounds.high,unsigned(i));}
        index_scan_count+=total;
        if(ranges.size()>=256)ranges.clear();
        range=ranges.emplace(key,bounds).first;
    }else ++index_range_hits;
    const auto low=range->second.low,high=range->second.high;
    if(std::int64_t(base)+low<0)return false;
    native_state_={};native_state_.layout=3;native_state_.primitive=type;native_state_.vertex_blobs.fill(-1);native_state_.vertex_blobs[32]=0;
    for(unsigned i=0;i<vs->program.blobs.size();++i)native_state_.vertex_blobs[i]=int(vs->program.blobs[i]);
    for(auto& attribute:native_state_.attributes)attribute={4,VK_FORMAT_R32G32B32A32_SFLOAT,0};
    constexpr VkFormat formats[]{VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32_SFLOAT,VK_FORMAT_R32G32B32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_R8G8B8A8_USCALED};
    for(const auto& e:decl->elements){
        if(e.stream==platform::PcDeclEndStream)break;
        if(e.type>5||e.stream>=streams.size())return false;
        const auto& stream=streams[e.stream];auto* buffer=object(stream.buffer);if(!buffer)return false;
        const unsigned need=e.type<=3?4u*(e.type+1):4u;
        if(std::uint64_t(stream.offset)+std::uint64_t(std::int64_t(base)+high)*stream.stride+e.offset+need>buffer->bytes.size())return false;
        native_state_.strides[e.stream]=stream.stride;
        for(const auto& input:vs->program.inputs)if(input.usage==e.usage&&input.usage_index==e.usage_index){
            if(input.reg>=16)return false;
            native_state_.attributes[input.reg]={e.stream,formats[e.type],e.offset};
            native_state_.vertex_blobs[32]&=~(1<<input.reg);
            if(e.type==4)native_state_.vertex_blobs[32]|=1<<input.reg;
        }
    }
    native_vertices_=true;native_base_=base;native_start_=start;native_count_=unsigned(total);pretransformed_=false;return true;
}
std::shared_ptr<Buffer> VulkanD3D9Device::geometry(std::uint32_t handle){
    auto found=geometry_.find(handle);
    if(found==geometry_.end()){auto* buffer=object(handle);if(!buffer||buffer->bytes.empty())throw std::runtime_error("Invalid Vulkan geometry buffer");
        found=geometry_.emplace(handle,context_.geometry(buffer->bytes.data(),buffer->bytes.size())).first;}
    context_.retain(found->second);return found->second;
}
std::uint32_t VulkanD3D9Device::prepare_indexed(std::uint32_t type,std::int32_t base,std::uint32_t min,std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    // Keep the reference renderer's optional draw filtering/statistics intact.
    static const bool debug=std::getenv("OR2_DBG_DRAWSTATS")||std::getenv("OR2_DBG_ONLY");
    if(debug)return PcSoftD3D9Device::draw_indexed_primitive(type,base,min,vertices,start,count);
    auto* ib=object(indices);auto* ps=object(pixel_shader);if(!ib)return 1;
    ps_=ps&&ps->kind==4?&ps->program:nullptr;
    const std::size_t total=type==5?std::size_t(count)+2:type==4?std::size_t(count)*3:0;
    if(!total||(std::size_t(start)+total)*2>ib->bytes.size())return 1;
    note_dip_state(count);if(hash_draws)hash_draw(1,{type,std::uint32_t(base),min,vertices,start,count});
    if(count_only)return 0;
    if(!targets_consistent()){errors.push_back("draw with the depth surface smaller than the render target");return 1;}
    if(!count)return 0;
    auto* decl=object(declaration);auto* vs=object(vertex_shader);if(!decl||!vs||vs->kind!=3)return 1;
    // Resolve declaration streams/input registers and copy constants once per
    // draw, rather than walking bindings and copying 4 KiB for every vertex.
    inputs_.clear();
    for(const auto& e:decl->elements){
        if(e.stream==platform::PcDeclEndStream)break;
        const auto& stream=streams.at(e.stream);auto* buffer=object(stream.buffer);if(!buffer)return 1;
        unsigned registers=0;
        for(const auto& input:vs->program.inputs)if(input.usage==e.usage&&input.usage_index==e.usage_index){
            if(input.reg>=16)return 1;
            registers|=1u<<input.reg;
        }
        inputs_.push_back({&buffer->bytes,std::size_t(stream.offset)+e.offset,stream.stride,e.type,registers});
    }
    std::array<platform::pc_shader::V4,256> constants;
    for(unsigned i=0;i<256;++i)constants[i]={vs_constants[i][0],vs_constants[i][1],vs_constants[i][2],vs_constants[i][3]};
    // All supported game index buffers contain uint16 indices. Allocating the
    // bounded cache once also keeps pointers stable when a new index is shaded.
    if(indexed_vertices_.empty()){indexed_vertices_.resize(65536);vertex_epochs_.resize(65536);}
    if(++vertex_epoch_==0){std::fill(vertex_epochs_.begin(),vertex_epochs_.end(),0);vertex_epoch_=1;}
    batch_.reserve(std::max(batch_.capacity(),std::size_t(count)*3));
    auto index=[&](std::size_t k){std::uint16_t i;std::memcpy(&i,ib->bytes.data()+k*2,2);return i;};
    auto vertex=[&](std::uint16_t i)->const Vertex*{
        if(vertex_epochs_[i]!=vertex_epoch_){
            if(!prepare_vertex(i,base,vs->program,constants.data(),indexed_vertices_[i]))return nullptr;
            vertex_epochs_[i]=vertex_epoch_;
        }return &indexed_vertices_[i];
    };
    for(std::uint32_t p=0;p<count;++p){
        const auto at=std::size_t(start)+(type==5?p:std::size_t(p)*3);
        auto a=index(at),b=index(at+1),c=index(at+2);if(type==5&&(p&1))std::swap(a,b);
        const auto* va=vertex(a);const auto* vb=vertex(b);const auto* vc=vertex(c);
        if(!va||!vb||!vc)return 1;
        clipped_triangle(*va,*vb,*vc);
    }return 0;
}
bool VulkanD3D9Device::prepare_vertex(std::uint16_t index,std::int32_t base,const platform::PcShaderProgram& program,const platform::pc_shader::V4* constants,Vertex& out){
    using namespace platform::pc_shader;
    VsState state;state.C=constants;for(auto& v:state.V)v={0,0,0,1};
    const auto address=std::int64_t(base)+index;if(address<0)return false;
    for(const auto& binding:inputs_){
        const auto at=std::uint64_t(address)*binding.stride+binding.offset;
        const unsigned need=binding.type<=3?4u*(binding.type+1):4u;
        if(at>binding.bytes->size()||need>binding.bytes->size()-at)return false;
        if(!binding.registers)continue;
        const auto* data=binding.bytes->data()+std::size_t(at);V4 value{0,0,0,1};
        if(binding.type<=3)std::memcpy(&value,data,need);
        else if(binding.type==4)value={data[2]/255.f,data[1]/255.f,data[0]/255.f,data[3]/255.f};
        else if(binding.type==5)value={float(data[0]),float(data[1]),float(data[2]),float(data[3])};
        for(unsigned mask=binding.registers;mask;mask&=mask-1)state.V[unsigned(__builtin_ctz(mask))]=value;
    }
    platform::pc_shader_run_vs(program,state);std::memcpy(out.pos,&state.oPos,16);
    out.d[0]=sat(state.oD[0]);out.d[1]=sat(state.oD[1]);
    for(unsigned i=0;i<8;++i)out.t[i]=state.oT[i];
    out.fog=clamp01(state.oFog.x);return true;
}
void VulkanD3D9Device::clipped_triangle(const Vertex& a,const Vertex& b,const Vertex& c){
    // Same D3D depth clipping/order/interpolation as the reference, with bounded
    // stack polygons instead of three heap-allocated vectors per triangle.
    auto inside=[](const Vertex& v){return v.pos[2]>=0.f&&v.pos[3]-v.pos[2]>=0.f&&v.pos[3]>1e-6f;};
    if(inside(a)&&inside(b)&&inside(c)){const Vertex vertices[]{a,b,c};raster(vertices,3);return;}
    Vertex first[8],second[8];first[0]=a;first[1]=b;first[2]=c;
    auto* polygon=first;auto* output=second;unsigned count=3;
    auto lerp=[](const Vertex& p,const Vertex& q,float t){Vertex r;
        for(unsigned i=0;i<4;++i)r.pos[i]=p.pos[i]+(q.pos[i]-p.pos[i])*t;
        for(unsigned i=0;i<2;++i)r.d[i]=platform::pc_shader::lrp(platform::pc_shader::v4(t),q.d[i],p.d[i]);
        for(unsigned i=0;i<8;++i)r.t[i]=platform::pc_shader::lrp(platform::pc_shader::v4(t),q.t[i],p.t[i]);
        r.fog=p.fog+(q.fog-p.fog)*t;return r;
    };
    for(unsigned plane=0;plane<2;++plane){
        unsigned written=0;
        for(unsigned i=0;i<count;++i){const auto& p=polygon[i];const auto& q=polygon[(i+1)%count];
            const float dp=plane==0?p.pos[2]:p.pos[3]-p.pos[2],dq=plane==0?q.pos[2]:q.pos[3]-q.pos[2];
            if(dp>=0.f)output[written++]=p;
            if((dp>=0.f)!=(dq>=0.f))output[written++]=lerp(p,q,dp/(dp-dq));
        }
        count=written;std::swap(polygon,output);if(count<3)return;
    }
    for(unsigned i=0;i<count;++i)if(!(polygon[i].pos[3]>1e-6f))return;
    raster(polygon,count);
}
void VulkanD3D9Device::draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
    const auto begin=sample_draw_times_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    batch_.clear();native_vertices_=false;
    if(type==1)prepare_points(count,data,stride);
    else PcSoftD3D9Device::draw_primitive_up(type,count,data,stride);
    const auto prepared=sample_draw_times_?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
    // Point sprites are independent of triangle winding. Restore game state
    // even when a driver error interrupts recording.
    const auto cull=render[RS_CULLMODE];if(type==1)render[RS_CULLMODE]=CULL_NONE;
    try{flush();}catch(...){render[RS_CULLMODE]=cull;throw;}
    render[RS_CULLMODE]=cull;
    if(sample_draw_times_){const auto recorded=std::chrono::steady_clock::now();
        vertex_prepare_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(prepared-begin).count();
        command_record_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(recorded-prepared).count();}
}
void VulkanD3D9Device::prepare_points(std::uint32_t count,const void* data,std::uint32_t stride){
    if(!data||!count)return;
    if(hash_draws){hash_draw(2,{1,count,stride});const auto* b=static_cast<const std::uint8_t*>(data);
        for(std::size_t i=0;i+4<=std::size_t(count)*stride;i+=4){std::uint32_t v;std::memcpy(&v,b+i,4);draw_hash=(draw_hash^v)*0x100000001b3ull;}}
    if(count_only)return;
    // Same retail 41A6F0 / 41AE40 path as the Metal port. The small particle
    // batches become quads; ordinary indexed geometry keeps the GPU VS path.
    if(fvf_!=0x42||stride<16||vertex_shader){errors.push_back("Vulkan point sprite: unsupported vertex layout");return;}
    if(!targets_consistent()){errors.push_back("draw with the depth surface smaller than the render target");return;}
    const auto wvp=transform_wvp();
    auto transform=[](const float* v,const std::array<float,16>& m){std::array<float,4> out{};
        for(unsigned k=0;k<4;++k)out[k]=v[0]*m[k]+v[1]*m[4+k]+v[2]*m[8+k]+v[3]*m[12+k];return out;};
    const float minimum=f(render[155]),maximum=render[166]?f(render[166]):64.f;
    if(!std::isfinite(minimum)||!std::isfinite(maximum)||minimum<0||maximum<minimum){errors.push_back("Vulkan point sprite: invalid size limits");return;}
    const auto* bytes=static_cast<const std::uint8_t*>(data);pretransformed_=false;
    for(unsigned i=0;i<count;++i){
        float xyz[4]{0,0,0,1};std::uint32_t colour;
        std::memcpy(xyz,bytes+std::size_t(i)*stride,12);std::memcpy(&colour,bytes+std::size_t(i)*stride+12,4);
        const auto clip=transform(xyz,wvp);
        if(!(clip[3]>0)||clip[2]<0||clip[2]>clip[3]||std::abs(clip[0])>clip[3]||std::abs(clip[1])>clip[3])continue;
        float size=render[154]?f(render[154]):1.f;
        if(render[157]){const auto world=transform(xyz,transform_world);const auto eye=transform(world.data(),transform_view);
            const float distance=std::sqrt(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
            const float denominator=f(render[158])+f(render[159])*distance+f(render[160])*distance*distance;
            size=denominator>0?soft_viewport_.h*size/std::sqrt(denominator):maximum;}
        if(!std::isfinite(size)){errors.push_back("Vulkan point sprite: invalid size");return;}
        size=std::clamp(size,minimum,maximum);
        const float dx=size*clip[3]/soft_viewport_.w,dy=size*clip[3]/soft_viewport_.h;
        Vertex q[4]{};
        for(unsigned k=0;k<4;++k){auto& v=q[k];std::copy(clip.begin(),clip.end(),v.pos);
            v.pos[0]+=(k&1)?dx:-dx;v.pos[1]+=(k&2)?-dy:dy;
            v.d[0]={float((colour>>16)&255)/255.f,float((colour>>8)&255)/255.f,float(colour&255)/255.f,float(colour>>24)/255.f};v.fog=1;
            for(auto& uv:v.t)uv=render[156]?platform::pc_shader::V4{float(k&1),float((k>>1)&1),0,1}:platform::pc_shader::V4{0,0,0,1};}
        const Vertex a[]{q[0],q[1],q[2]},b[]{q[2],q[1],q[3]};raster(a,3);raster(b,3);
    }
}
void VulkanD3D9Device::begin_frame(){
#ifdef OR2_PS5_PAYLOAD
    // Sample one frame per report window instead of reading the native clock
    // three times for every draw of every frame. Frame/CPU/GPU/wait totals
    // remain measured every frame independently of these diagnostic samples.
    sample_draw_times_=++diagnostic_frames_%120==60;
#endif
    if(sample_draw_times_)++timed_frames;
    frame_samples_=context_.samples_passed();scene_finished_=false;msaa_live_=msaa_>1&&!count_only;
    if(msaa_live_){
        if(msaa_>context_.max_samples())throw std::runtime_error("Requested MSAA count unsupported by Vulkan device");
        if(!msaa_colour_||msaa_colour_->samples!=VkSampleCountFlagBits(msaa_)){
            msaa_colour_=context_.image(back_->width,back_->height,1,1,back_->format,msaa_);
            msaa_depth_=context_.image(back_->width,back_->height,1,1,context_.depth_format(),msaa_);}
    }
}
void VulkanD3D9Device::end_frame(){
    finish_scene();
    // Native presentation submits scene and compositor together. Diagnostics
    // still retire here, and explicit game queries retain their own fences.
    if(frame_queries_){context_.finish();pixels_shaded+=context_.samples_passed()-frame_samples_;}
}
void VulkanD3D9Device::finish_scene(){
    if(scene_finished_||count_only||colour_target_!=BackBuffer)return;
    scene_finished_=true;
    if(msaa_live_){context_.resolve(msaa_colour_,back_);msaa_live_=false;
        context_.clear({back_,back_depth_},6,0,1,0);++msaa_resolves;return;}
    if(!fxaa_)return;
    if(!fxaa_copy_)fxaa_copy_=context_.image(back_->width,back_->height);
    context_.copy(back_,fxaa_copy_);
    std::array<Texture,12> textures{};textures[0]=fxaa_copy_;std::array<VkSampler,12> samplers{};
    std::array<std::uint32_t,14> state{};state[1]=state[2]=state[3]=3;state[5]=state[6]=2;samplers[0]=context_.sampler(state,1);
    const auto descriptor=context_.descriptors({},textures,samplers);context_.target({back_,{}});
    PipelineState p{};p.layout=2;p.format=back_->format;p.render[RS_CULLMODE]=CULL_NONE;p.render[RS_COLORWRITEENABLE]=15;
    const auto command=context_.commands();vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,context_.pipeline(p));
    VkViewport viewport{0,0,float(back_->width),float(back_->height),0,1};VkRect2D rect{{0,0},{back_->width,back_->height}};
    vkCmdSetViewport(command,0,1,&viewport);vkCmdSetScissor(command,0,1,&rect);vkCmdSetDepthBias(command,0,0,0);
    const float blend[4]{};vkCmdSetBlendConstants(command,blend);vkCmdSetStencilReference(command,VK_STENCIL_FACE_FRONT_AND_BACK,0);
    const auto offsets=Context::uniform_offsets({});
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,context_.pipeline_layout(),0,1,&descriptor,unsigned(offsets.size()),offsets.data());vkCmdDraw(command,3,1,0,0);++fxaa_passes;
}
void VulkanD3D9Device::flush(){
    if((batch_.empty()&&!native_vertices_)||count_only)return;
    if(pretransformed_&&msaa_live_&&colour_target_==BackBuffer)finish_scene();
    auto* ps=object(pixel_shader);const unsigned kind=ps&&ps->kind==4?(ps->program.version==0xffff0101u?1:2):0;
    std::array<Slice,3> uniforms{};uniforms[0]=context_.uniform(0,ps_constants.data(),8*4*sizeof(float));
    PipelineState state=native_vertices_?native_state_:PipelineState{};state.shader=kind;state.blobs.fill(-1);state.blobs[16]=0;
    if(kind){
        if(ps->program.blobs.size()>16)throw std::runtime_error("Vulkan PS program exceeds 16 blobs");
        struct {std::int32_t program[16],info[4],kinds[8];float bump[8][4],lum[2][4];} p{};
        std::fill(std::begin(p.program),std::end(p.program),-1);p.info[0]=int(ps->program.blobs.size());state.blobs[16]=p.info[0];
        for(unsigned i=0;i<ps->program.blobs.size();++i)state.blobs[i]=p.program[i]=int(ps->program.blobs[i]);
        for(unsigned s=0;s<8;++s){auto* t=object(textures[s]);p.kinds[s]=t&&t->kind==5&&t->texture.cube;for(unsigned k=0;k<4;++k)p.bump[s][k]=f(stage[s][7+k]);}
        for(unsigned s=0;s<4;++s){p.lum[s>>1][(s&1)*2]=f(stage[s][22]);p.lum[s>>1][(s&1)*2+1]=f(stage[s][23]);}uniforms[1]=context_.uniform(1,&p,sizeof p);
    }else{
        struct {std::int32_t op[4][4],arg[4][4],misc[4][4];float konst[4][4],tfactor[4];std::int32_t flags[4];} p{};
        for(unsigned s=0;s<4;++s){const auto& t=stage[s];p.op[s][0]=int(t[1]);p.op[s][1]=int(t[2]);p.op[s][2]=int(t[3]);p.op[s][3]=int(t[4]);
            p.arg[s][0]=int(t[5]);p.arg[s][1]=int(t[6]);p.arg[s][2]=int(t[26]);p.arg[s][3]=int(t[27]);auto* tx=object(textures[s]);
            p.misc[s][0]=int(t[28]);p.misc[s][1]=int(t[11]&3);p.misc[s][2]=tx&&tx->kind==5&&!tx->texture.cube;p.misc[s][3]=tx&&tx->kind==5&&tx->texture.cube;
            const auto c=t[32];p.konst[s][0]=float((c>>16)&255)/255;p.konst[s][1]=float((c>>8)&255)/255;p.konst[s][2]=float(c&255)/255;p.konst[s][3]=float(c>>24)/255;}
        const auto c=render[RS_TEXTUREFACTOR];p.tfactor[0]=float((c>>16)&255)/255;p.tfactor[1]=float((c>>8)&255)/255;p.tfactor[2]=float(c&255)/255;p.tfactor[3]=float(c>>24)/255;p.flags[0]=render[RS_SPECULARENABLE]!=0;uniforms[1]=context_.uniform(1,&p,sizeof p);
    }
    float fixed[12]{};const auto c=render[RS_FOGCOLOR];fixed[0]=float((c>>16)&255)/255;fixed[1]=float((c>>8)&255)/255;fixed[2]=float(c&255)/255;fixed[3]=f(render[RS_FOGEND]);
    if(render[RS_FOGENABLE])fixed[4]=render[RS_FOGTABLEMODE]?float(render[RS_FOGTABLEMODE]):4.f;
    fixed[5]=f(render[RS_FOGSTART]);fixed[6]=f(render[RS_FOGDENSITY]);fixed[7]=1;fixed[8]=render[RS_ALPHATESTENABLE]?1.f:0.f;fixed[9]=float(render[RS_ALPHAREF]&255);fixed[10]=float(render[RS_ALPHAFUNC]);uniforms[2]=context_.uniform(2,fixed,sizeof fixed);
    std::array<Texture,12> images{};std::array<VkSampler,12> samplers{};
    for(unsigned s=0;s<6;++s){auto tex=texture(textures[s]);const bool cube=tex&&tex->layers==6;
        images[s]=tex&&!cube?tex:dummy2d_;images[s+6]=tex&&cube?tex:dummycube_;
        samplers[s]=samplers[s+6]=context_.sampler(sampler[s],tex?tex->levels:1);
    }
    const auto target=this->target();
    for(const auto& image:images)if(image==target.colour)throw std::runtime_error("D3D texture feedback into its Vulkan render target");
    std::array<Slice,2> vertex_uniforms{};Slice vertices{};
    std::array<std::shared_ptr<Buffer>,5> buffers{};std::array<VkBuffer,5> vertex_buffers{};std::array<VkDeviceSize,5> offsets{};
    std::shared_ptr<Buffer> index_buffer;
    if(native_vertices_){
        vertex_uniforms[0]=context_.uniform(3,vs_constants.data(),sizeof vs_constants);
        const float fix[]{1.f/soft_viewport_.w,1.f/soft_viewport_.h,0,0};vertex_uniforms[1]=context_.uniform(4,fix,sizeof fix);
        const float defaults[]{0,0,0,1};const auto fallback=context_.uniform(5,defaults,sizeof defaults);
        for(unsigned i=0;i<5;++i){vertex_buffers[i]=fallback.buffer;offsets[i]=fallback.offset;}
        for(unsigned i=0;i<4;++i)if(streams[i].buffer){buffers[i]=geometry(streams[i].buffer);vertex_buffers[i]=buffers[i]->buffer;offsets[i]=streams[i].offset;}
        index_buffer=geometry(indices);
    }else vertices=context_.allocate(batch_.data(),batch_.size()*sizeof(Vertex));
    const auto descriptors=context_.descriptors(uniforms,images,samplers,vertex_uniforms);context_.target(target);
    // Only pipeline state belongs in the cache key; colours, fog, constants,
    // sampler state and texture identities are dynamic draw data.
    for(unsigned i:std::initializer_list<unsigned>{RS_ZENABLE,RS_FILLMODE,RS_ZWRITEENABLE,RS_SRCBLEND,RS_DESTBLEND,RS_CULLMODE,RS_ZFUNC,
        RS_ALPHABLENDENABLE,52u,53u,54u,55u,56u,58u,59u,RS_COLORWRITEENABLE,RS_BLENDOP,
        RS_SEPARATEALPHABLENDENABLE,RS_SRCBLENDALPHA,RS_DESTBLENDALPHA,RS_BLENDOPALPHA})state.render[i]=render[i];
    state.samples=unsigned(target.colour->samples);state.format=target.colour->format;state.depth=bool(target.depth);
    DrawBindings bindings{};bindings.pipeline=context_.pipeline(state);bindings.descriptors=descriptors;bindings.uniforms=Context::uniform_offsets(uniforms,vertex_uniforms);
    const bool back=colour_target_==BackBuffer;double sx=back?double(back_->width)/back_width_:1,sy=back?double(back_->height)/back_height_:1,ox=0;
    if(back&&ui_rect_&&batch_rhw_){const double w=double(back_->height)*4/3;ox=std::floor((back_->width-w)/2);sx=w/back_width_;}
    VkViewport viewport{float(ox+soft_viewport_.x*sx),float(soft_viewport_.y*sy),float(soft_viewport_.w*sx),float(soft_viewport_.h*sy),soft_viewport_.min_z,soft_viewport_.max_z};
    const int x=std::max(0,int(std::lround(viewport.x))),y=std::max(0,int(std::lround(viewport.y)));
    const int width=std::max(0,std::min(int(std::max(1u,target.colour->width>>target.level)),int(std::lround(viewport.x+viewport.width)))-x);
    const int height=std::max(0,std::min(int(std::max(1u,target.colour->height>>target.level)),int(std::lround(viewport.y+viewport.height)))-y);
    bindings.viewport=viewport;bindings.scissor={{x,y},{unsigned(width),unsigned(height)}};
    bindings.depth_bias=f(render[RS_DEPTHBIAS])*16777215.f;bindings.slope_bias=f(render[RS_SLOPESCALEDEPTHBIAS]);bindings.stencil=render[57]&255;
    const auto factor=render[193];bindings.blend={float((factor>>16)&255)/255,float((factor>>8)&255)/255,float(factor&255)/255,float(factor>>24)/255};
    if(native_vertices_){bindings.vertex_count=5;bindings.vertices=vertex_buffers;bindings.offsets=offsets;bindings.index=index_buffer->buffer;}
    else{bindings.vertex_count=1;bindings.vertices[0]=vertices.buffer;bindings.offsets[0]=vertices.offset;}
    const auto command=context_.bind_draw(bindings);
    const bool samples=frame_queries_||gpu_active_queries_!=0;
    if(samples)context_.begin_occlusion();
    if(native_vertices_){vkCmdDrawIndexed(command,native_count_,1,native_start_,native_base_,0);++native_vertex_draws;
        triangles_drawn+=native_state_.primitive==5?native_count_-2:native_count_/3;
        if(native_vertex_draws==1)context_.trace_next_submission();}
    else vkCmdDraw(command,unsigned(batch_.size()),1,0,0);
    ++submitted_draws;
    if(samples){const auto index=context_.end_occlusion();const auto epoch=context_.recording_epoch();
        for(auto& [handle,q]:gpu_queries_)if(q.active){query_draws_.push_back({epoch,index,handle,q.generation});++q.outstanding;}}
    batch_.clear();batch_rhw_=false;native_vertices_=false;
}
void VulkanD3D9Device::retire_queries(std::uint64_t epoch,const std::vector<std::uint64_t>& samples){
    std::size_t kept=0;
    for(const auto& d:query_draws_){
        if(d.epoch!=epoch){query_draws_[kept++]=d;continue;}
        auto it=gpu_queries_.find(d.handle);if(it==gpu_queries_.end()||it->second.generation!=d.generation)continue;
        auto& q=it->second;if(d.index<samples.size())q.total+=samples[d.index];
        if(q.outstanding&&--q.outstanding==0&&q.ended){q.result=std::uint32_t(std::min<std::uint64_t>(q.total,std::numeric_limits<std::uint32_t>::max()));q.ready=true;}
    }
    query_draws_.resize(kept);
}
void VulkanD3D9Device::query_issue(std::uint32_t handle,std::uint32_t flags){
    if(handle<QueryBase||handle-QueryBase>=queries_.size())return;
    if(count_only){PcSoftD3D9Device::query_issue(handle,flags);return;}
    auto& q=gpu_queries_[handle];
    // BEGIN restarts the query; draws still in flight for the previous
    // generation no longer count towards it.
    if(flags&2){if(!q.active)++gpu_active_queries_;q.active=true;q.ended=false;q.ready=false;++q.generation;q.outstanding=0;q.total=0;}
    if((flags&1)&&q.active){q.active=false;q.ended=true;--gpu_active_queries_;
        if(!q.outstanding){q.result=std::uint32_t(std::min<std::uint64_t>(q.total,std::numeric_limits<std::uint32_t>::max()));q.ready=true;}}
}
std::uint32_t VulkanD3D9Device::query_get_data(std::uint32_t handle,std::uint32_t& samples){
    if(count_only)return PcSoftD3D9Device::query_get_data(handle,samples);
    samples=0;if(handle<QueryBase||handle-QueryBase>=queries_.size())return 1;
    // Like the reference device, a query never issued is complete with 0 samples.
    auto it=gpu_queries_.find(handle);if(it==gpu_queries_.end())return 0;if(it->second.active)return 1;
    if(!it->second.ready)context_.poll();
    if(!it->second.ready)return 1;
    samples=it->second.result;return 0;
}
}
