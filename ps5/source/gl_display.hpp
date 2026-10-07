#pragma once
#include "gl_api.hpp"
#include "platform/mesh_preview_pack.hpp"
#include <array>
#include <map>
#include <string>
#include <vector>
namespace outrun::ps5_runtime {
class GlDisplay {
public:
    ~GlDisplay();
    bool open(unsigned width,unsigned height,std::string& error);
    void clear();
    unsigned texels(const void* key,platform::MeshPreviewTextureFormat,unsigned,unsigned,const std::vector<std::uint8_t>&);
    void quad(const std::array<platform::MeshPreviewVertex,4>&,unsigned texture,unsigned,unsigned);
    void rgba(const void* key,const std::uint8_t* bytes,unsigned,unsigned,int,int,int,int);
    void pc_frame(unsigned texture,int x,int y,int width,int height);
    bool present(std::string& error);
private:
    unsigned upload(const void*,const std::uint8_t*,unsigned,unsigned,bool);
    void rectangle(unsigned,int,int,int,int);
    struct Image {unsigned texture{},width{},height{};};
    std::map<const void*,Image> images_;
    EGLDisplay display_{EGL_NO_DISPLAY};EGLSurface surface_{EGL_NO_SURFACE};EGLContext context_{EGL_NO_CONTEXT};
    unsigned width_{},height_{},actual_width_{},actual_height_{},program_{},vao_{},vbo_{};
};
}
