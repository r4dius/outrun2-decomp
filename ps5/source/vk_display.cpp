#include "vk_display.hpp"
#include "platform/pc_dds.hpp"
#include "platform/pc_d3d9_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
namespace outrun::ps5_runtime {
using namespace vulkan;
using namespace platform::d3d9;
VulkanDisplay::~VulkanDisplay(){if(context_){try{context_->finish();}catch(...){}images_.clear();output_.reset();}}
bool VulkanDisplay::open(unsigned w,unsigned h,std::string& error){
    try{
#ifdef OR2_PS5_PAYLOAD
        constexpr bool native=true;
#else
        constexpr bool native=false;
#endif
        context_=std::make_unique<Context>(w,h,native);width_=w;height_=h;return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void VulkanDisplay::clear(){output_=context_->display_target();context_->clear({output_,{}},1,0xff000000,1,0);}
Texture VulkanDisplay::upload(const void* key,const std::uint8_t* rgba,unsigned w,unsigned h,bool update){
    if(!rgba||!w||!h)throw std::runtime_error("Invalid Vulkan display image");
    auto& t=images_[key];const bool fresh=!t||t->width!=w||t->height!=h;
    if(fresh)t=context_->image(w,h);if(fresh||update)context_->upload(t,0,0,rgba,std::size_t(w)*h*4);return t;
}
Texture VulkanDisplay::texels(const void* key,platform::MeshPreviewTextureFormat format,unsigned w,unsigned h,const std::vector<std::uint8_t>& bytes){
    if(auto it=images_.find(key);it!=images_.end())return it->second;
    platform::PcDdsTexture t{};t.width=w;t.height=h;t.levels=1;
    t.format=format==platform::MeshPreviewTextureFormat::bc1?platform::PcDdsTexture::Format::bc1:
        format==platform::MeshPreviewTextureFormat::bc2?platform::PcDdsTexture::Format::bc2:
        format==platform::MeshPreviewTextureFormat::bc3?platform::PcDdsTexture::Format::bc3:platform::PcDdsTexture::Format::rgba8;
    t.faces.resize(1);t.faces[0].push_back({w,h,bytes});const auto rgba=platform::pc_dds_rgba(t,0,0);
    if(rgba.size()!=std::size_t(w)*h*4)throw std::runtime_error("Vulkan display DDS size mismatch");return upload(key,rgba.data(),w,h,false);
}
void VulkanDisplay::draw(const std::array<platform::MeshPreviewVertex,4>& q,const Texture& image,bool blend){
    if(!output_)throw std::runtime_error("Clear Vulkan display before drawing");
    float vertices[6][8];const unsigned order[]{0,1,2,2,3,0};
    for(unsigned k=0;k<6;++k){const auto& v=q[order[k]];vertices[k][0]=v.position[0];vertices[k][1]=v.position[1];
        for(unsigned c=0;c<4;++c)vertices[k][2+c]=v.color[c];vertices[k][6]=v.uv[0];vertices[k][7]=v.uv[1];}
    std::array<Texture,12> textures{};textures[0]=image;std::array<VkSampler,12> samplers{};
    std::array<std::uint32_t,14> sampler{};sampler[1]=sampler[2]=sampler[3]=3;sampler[5]=sampler[6]=2;
    samplers[0]=context_->sampler(sampler,image->levels);const auto descriptors=context_->descriptors({},textures,samplers);
    const auto vertex=context_->allocate(vertices,sizeof vertices);context_->target({output_,{}});
    PipelineState p{};p.layout=1;p.format=output_->format;p.render[RS_CULLMODE]=CULL_NONE;p.render[RS_COLORWRITEENABLE]=15;
    p.render[RS_ALPHABLENDENABLE]=blend;p.render[RS_SRCBLEND]=5;p.render[RS_DESTBLEND]=6;p.render[RS_BLENDOP]=1;
    p.render[RS_SEPARATEALPHABLENDENABLE]=1;p.render[RS_SRCBLENDALPHA]=2;p.render[RS_DESTBLENDALPHA]=6;p.render[RS_BLENDOPALPHA]=1;
    DrawBindings bindings{};bindings.pipeline=context_->pipeline(p);bindings.descriptors=descriptors;
    bindings.viewport={0,0,float(output_->width),float(output_->height),0,1};bindings.scissor={{0,0},{output_->width,output_->height}};
    bindings.vertex_count=1;bindings.vertices[0]=vertex.buffer;bindings.offsets[0]=vertex.offset;
    const auto command=context_->bind_draw(bindings);vkCmdDraw(command,6,1,0,0);
}
void VulkanDisplay::quad(const std::array<platform::MeshPreviewVertex,4>& q,const Texture& texture,unsigned,unsigned){draw(q,texture,true);}
void VulkanDisplay::rectangle(const Texture& texture,int x,int y,int w,int h,bool blend){
    std::array<platform::MeshPreviewVertex,4> q{};const float px[]{float(x),float(x+w),float(x+w),float(x)},py[]{float(y),float(y),float(y+h),float(y+h)};
    for(unsigned k=0;k<4;++k){q[k].position={2*px[k]/width_-1,1-2*py[k]/height_,0};q[k].color={1,1,1,1};q[k].uv={k==1||k==2?1.f:0.f,k>=2?1.f:0.f};}draw(q,texture,blend);
}
void VulkanDisplay::rgba(const void* key,const std::uint8_t* rgba,unsigned w,unsigned h,int x,int y,int ow,int oh){rectangle(upload(key,rgba,w,h,true),x,y,ow,oh,true);}
void VulkanDisplay::pc_frame(const Texture& texture,int x,int y,int w,int h){rectangle(texture,x,y,w,h,false);}
bool VulkanDisplay::present(std::string& error){
    try{
#ifdef OR2_PS5_HOST_TEST
        if(const char* directory=std::getenv("OR2_PS5_SCREENSHOT_DIR")){
            std::filesystem::create_directories(directory);static unsigned frame=0;char name[48];std::snprintf(name,sizeof name,"/vulkan-%04u.ppm",++frame);
            const auto pixels=context_->readback(output_);auto* file=std::fopen((std::string(directory)+name).c_str(),"wb");
            if(!file)throw std::runtime_error("Cannot write Vulkan screenshot");std::fprintf(file,"P6\n%u %u\n255\n",output_->width,output_->height);
            for(std::size_t i=0;i<pixels.size();i+=4)std::fwrite(pixels.data()+i,3,1,file);std::fclose(file);
        }
#endif
        context_->present();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
