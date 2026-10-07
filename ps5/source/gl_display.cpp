#include "gl_display.hpp"
#include "platform/pc_dds.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
namespace outrun::ps5_runtime {
namespace {
constexpr const char* vertex=R"GLSL(#version 450
layout(location=0) in vec2 position;layout(location=1) in vec4 colour;layout(location=2) in vec2 texcoord;
out vec4 c;out vec2 uv;void main(){gl_Position=vec4(position,0,1);c=colour;uv=texcoord;})GLSL";
constexpr const char* fragment=R"GLSL(#version 450
in vec4 c;in vec2 uv;layout(binding=0) uniform sampler2D image;layout(location=0) out vec4 pixel;
void main(){pixel=texture(image,uv)*c;})GLSL";
void egl_require(bool ok,const char* op){if(!ok)throw std::runtime_error(std::string(op)+": EGL error "+std::to_string(eglGetError()));}
}
GlDisplay::~GlDisplay(){
    if(context_!=EGL_NO_CONTEXT){
        for(auto& [_,i]:images_)glDeleteTextures(1,&i.texture);
        if(program_)glDeleteProgram(program_);if(vbo_)glDeleteBuffers(1,&vbo_);if(vao_)glDeleteVertexArrays(1,&vao_);
        eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display_,context_);
    }
    if(surface_!=EGL_NO_SURFACE)eglDestroySurface(display_,surface_);
    if(display_!=EGL_NO_DISPLAY)eglTerminate(display_);
}
bool GlDisplay::open(unsigned w,unsigned h,std::string& error){
    try{
        width_=w;height_=h;display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);
        egl_require(display_!=EGL_NO_DISPLAY,"eglGetDisplay");egl_require(eglInitialize(display_,nullptr,nullptr),"eglInitialize");
        egl_require(eglBindAPI(EGL_OPENGL_API),"eglBindAPI");
        const EGLint attrs[]{EGL_SURFACE_TYPE,
#ifdef OR2_PS5_PAYLOAD
            EGL_WINDOW_BIT,
#else
            EGL_PBUFFER_BIT,
#endif
            EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
        EGLConfig config{};EGLint found=0;egl_require(eglChooseConfig(display_,attrs,&config,1,&found)&&found==1,"eglChooseConfig");
#ifdef OR2_PS5_PAYLOAD
        surface_=eglCreateWindowSurface(display_,config,0,nullptr);
#else
        const EGLint size[]{EGL_WIDTH,int(w),EGL_HEIGHT,int(h),EGL_NONE};surface_=eglCreatePbufferSurface(display_,config,size);
#endif
        egl_require(surface_!=EGL_NO_SURFACE,"eglCreateSurface");
        const EGLint ctx[]{EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,
#ifdef OR2_PS5_PAYLOAD
            6,
#else
            5,
#endif
            EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE};
        context_=eglCreateContext(display_,config,EGL_NO_CONTEXT,ctx);egl_require(context_!=EGL_NO_CONTEXT,"eglCreateContext");
        egl_require(eglMakeCurrent(display_,surface_,surface_,context_),"eglMakeCurrent");
        EGLint aw=0,ah=0;egl_require(eglQuerySurface(display_,surface_,EGL_WIDTH,&aw)&&eglQuerySurface(display_,surface_,EGL_HEIGHT,&ah),"eglQuerySurface");
        actual_width_=unsigned(aw);actual_height_=unsigned(ah);
        program_=gl_program(vertex,fragment);glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);
        std::fprintf(stdout,"PS5 GPU display: %s / %s / %s, EGL %ux%u\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION),actual_width_,actual_height_);
        gl_require("display initialization");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void GlDisplay::clear(){
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,int(actual_width_),int(actual_height_));glClipControl(GL_LOWER_LEFT,GL_ZERO_TO_ONE);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBlendEquation(GL_FUNC_ADD);
    gl_require("display clear");
}
unsigned GlDisplay::upload(const void* key,const std::uint8_t* pixels,unsigned w,unsigned h,bool update){
    if(!pixels||!w||!h)throw std::runtime_error("invalid GL display texture");
    auto& i=images_[key];const bool fresh=!i.texture||i.width!=w||i.height!=h;
    if(fresh){if(i.texture)glDeleteTextures(1,&i.texture);glGenTextures(1,&i.texture);i.width=w;i.height=h;}
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,i.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(fresh){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,int(w),int(h),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    else if(update)glTexSubImage2D(GL_TEXTURE_2D,0,0,0,int(w),int(h),GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    gl_require("display texture upload");return i.texture;
}
unsigned GlDisplay::texels(const void* key,platform::MeshPreviewTextureFormat f,unsigned w,unsigned h,const std::vector<std::uint8_t>& bytes){
    if(auto it=images_.find(key);it!=images_.end())return it->second.texture;
    platform::PcDdsTexture t{};t.width=w;t.height=h;t.levels=1;
    t.format=f==platform::MeshPreviewTextureFormat::bc1?platform::PcDdsTexture::Format::bc1:
        f==platform::MeshPreviewTextureFormat::bc2?platform::PcDdsTexture::Format::bc2:
        f==platform::MeshPreviewTextureFormat::bc3?platform::PcDdsTexture::Format::bc3:platform::PcDdsTexture::Format::rgba8;
    t.faces.resize(1);t.faces[0].push_back({w,h,bytes});const auto rgba=platform::pc_dds_rgba(t,0,0);
    if(rgba.size()!=std::size_t(w)*h*4)throw std::runtime_error("GL DDS size mismatch");return upload(key,rgba.data(),w,h,false);
}
void GlDisplay::quad(const std::array<platform::MeshPreviewVertex,4>& q,unsigned image,unsigned,unsigned){
    float vertices[6][8];const unsigned order[]{0,1,2,2,3,0};
    for(unsigned k=0;k<6;++k){const auto& v=q[order[k]];vertices[k][0]=v.position[0];vertices[k][1]=v.position[1];
        for(unsigned c=0;c<4;++c)vertices[k][2+c]=v.color[c];vertices[k][6]=v.uv[0];vertices[k][7]=v.uv[1];}
    glUseProgram(program_);glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STREAM_DRAW);
    for(unsigned a=0;a<3;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,a==1?4:2,GL_FLOAT,GL_FALSE,8*sizeof(float),reinterpret_cast<void*>(std::uintptr_t((a==0?0:a==1?2:6)*sizeof(float))));}
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,image);glDrawArrays(GL_TRIANGLES,0,6);gl_require("display quad");
}
void GlDisplay::rectangle(unsigned texture,int x,int y,int w,int h){
    std::array<platform::MeshPreviewVertex,4> q{};
    const float px[]{float(x),float(x+w),float(x+w),float(x)},py[]{float(y),float(y),float(y+h),float(y+h)};
    for(unsigned k=0;k<4;++k){q[k].position={2*px[k]/width_-1,1-2*py[k]/height_,0};q[k].color={1,1,1,1};q[k].uv={k==1||k==2?1.f:0.f,k>=2?1.f:0.f};}
    quad(q,texture,unsigned(w),unsigned(h));
}
void GlDisplay::rgba(const void* key,const std::uint8_t* bytes,unsigned w,unsigned h,int x,int y,int ow,int oh){rectangle(upload(key,bytes,w,h,true),x,y,ow,oh);}
void GlDisplay::pc_frame(unsigned texture,int x,int y,int w,int h){glDisable(GL_BLEND);rectangle(texture,x,y,w,h);glEnable(GL_BLEND);}
bool GlDisplay::present(std::string& error){
    try{
#ifdef OR2_PS5_HOST_TEST
        if(const char* dir=std::getenv("OR2_PS5_SCREENSHOT_DIR")){
            // PPM is portable and preserves the GPU's actual composed pixels.
            std::filesystem::create_directories(dir);static unsigned frame=0;char name[48];std::snprintf(name,sizeof name,"/gpu-%04u.ppm",++frame);
            std::vector<std::uint8_t> pixels(std::size_t(actual_width_)*actual_height_*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(0,0,int(actual_width_),int(actual_height_),GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
            auto* file=std::fopen((std::string(dir)+name).c_str(),"wb");if(!file)throw std::runtime_error("cannot write GL screenshot");
            std::fprintf(file,"P6\n%u %u\n255\n",actual_width_,actual_height_);
            for(unsigned y=actual_height_;y-->0;)std::fwrite(pixels.data()+std::size_t(y)*actual_width_*3,actual_width_*3,1,file);std::fclose(file);
        }
#endif
        gl_require("frame presentation");egl_require(eglSwapBuffers(display_,surface_),"eglSwapBuffers");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
