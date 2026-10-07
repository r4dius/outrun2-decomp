#pragma once
#include "vk_context.hpp"
#include "platform/mesh_preview_pack.hpp"
#include <string>
namespace outrun::ps5_runtime {
class VulkanDisplay {
public:
    ~VulkanDisplay();
    bool open(unsigned width,unsigned height,std::string& error);
    void clear();
    vulkan::Texture texels(const void*,platform::MeshPreviewTextureFormat,unsigned,unsigned,const std::vector<std::uint8_t>&);
    void quad(const std::array<platform::MeshPreviewVertex,4>&,const vulkan::Texture&,unsigned,unsigned);
    void rgba(const void*,const std::uint8_t*,unsigned,unsigned,int,int,int,int);
    void pc_frame(const vulkan::Texture&,int,int,int,int);
    bool present(std::string& error);
    vulkan::Context& context(){return *context_;}
private:
    vulkan::Texture upload(const void*,const std::uint8_t*,unsigned,unsigned,bool);
    void draw(const std::array<platform::MeshPreviewVertex,4>&,const vulkan::Texture&,bool blend);
    void rectangle(const vulkan::Texture&,int,int,int,int,bool);
    std::unique_ptr<vulkan::Context> context_;
    std::map<const void*,vulkan::Texture> images_;
    vulkan::Texture output_; unsigned width_{},height_{};
};
}
