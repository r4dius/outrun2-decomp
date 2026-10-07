#include "gl_d3d9.hpp"
#include "pc_gl_shaders.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace outrun::ps5_runtime {
using namespace platform::d3d9;
namespace {
constexpr unsigned KTexture=5,KSurface=6;
std::string vertex(bool ffp){
    std::string s="#version 450\nlayout(location=0) in vec4 p;layout(location=1) in vec4 d0;layout(location=2) in vec4 d1;\n";
    for(unsigned k=0;k<8;++k)s+="layout(location="+std::to_string(3+k)+") in vec4 t"+std::to_string(k)+";\n";
    s+="layout(location=11) in float fog;layout(location=0) out vec4 od0;layout(location=1) out vec4 od1;\n";
    for(unsigned k=0;k<(ffp?4u:8u);++k)s+="layout(location="+std::to_string(2+k)+") out vec4 ot"+std::to_string(k)+";\n";
    s+="layout(location="+std::to_string(ffp?6:10)+") out float ofog;void main(){gl_Position=p;od0=d0;od1=d1;ofog=fog;";
    for(unsigned k=0;k<(ffp?4u:8u);++k)s+="ot"+std::to_string(k)+"=t"+std::to_string(k)+";";
    return s+"}\n";
}
GLenum comparison(unsigned f){return GL_NEVER+std::clamp(f,1u,8u)-1u;}
GLenum stencil_op(unsigned v){const GLenum ops[]{GL_KEEP,GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};return ops[std::min(v,8u)];}
GLenum blend_factor(unsigned v){const GLenum factors[]{GL_ONE,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};return factors[std::min(v,11u)];}
GLenum blend_op(unsigned v){const GLenum ops[]{GL_FUNC_ADD,GL_FUNC_ADD,GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT,GL_MIN,GL_MAX};return ops[std::min(v,5u)];}
GLenum address(unsigned v){return v==2?GL_MIRRORED_REPEAT:v==3?GL_CLAMP_TO_EDGE:v==4?GL_CLAMP_TO_BORDER:v==5?GL_MIRROR_CLAMP_TO_EDGE:GL_REPEAT;}
// Port post-process (not original code): switch/shaders/fxaa_fsh.glsl over the
// whole back buffer before the 2D layer (full-screen triangle, no vertex buffer).
constexpr const char* FxaaVertex="#version 450\nvoid main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,0.0,1.0);}\n";
constexpr const char* FxaaFragment=R"(#version 450
layout(location=0) out vec4 outColor;
layout(binding=0) uniform sampler2D scene;
vec4 at(vec2 q){vec2 size=vec2(textureSize(scene,0));q=clamp(q,vec2(0.5),size-0.5);return textureLod(scene,q/size,0.0);}
float luma(vec3 c){return dot(c,vec3(0.299,0.587,0.114));}
void main(){
    vec2 q=gl_FragCoord.xy;
    vec4 cM=at(q);
    float lNW=luma(at(q+vec2(-1.0,-1.0)).rgb),lNE=luma(at(q+vec2(1.0,-1.0)).rgb);
    float lSW=luma(at(q+vec2(-1.0,1.0)).rgb),lSE=luma(at(q+vec2(1.0,1.0)).rgb);
    float lM=luma(cM.rgb);
    float lMin=min(lM,min(min(lNW,lNE),min(lSW,lSE)));
    float lMax=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    vec2 dir=vec2(-((lNW+lNE)-(lSW+lSE)),(lNW+lSW)-(lNE+lSE));
    float reduce=max((lNW+lNE+lSW+lSE)*(0.25/8.0),1.0/128.0);
    float rcpMin=1.0/(min(abs(dir.x),abs(dir.y))+reduce);
    dir=clamp(dir*rcpMin,vec2(-8.0),vec2(8.0));
    vec3 a=0.5*(at(q+dir*(1.0/3.0-0.5)).rgb+at(q+dir*(2.0/3.0-0.5)).rgb);
    vec3 b=a*0.5+0.25*(at(q+dir*-0.5).rgb+at(q+dir*0.5).rgb);
    float lB=luma(b);
    vec3 aa=(lB<lMin||lB>lMax)?a:b;
    outColor=vec4((lMax-lMin<max(0.0312,lMax*0.125))?cM.rgb:aa,cM.a);
}
)";
}
GlD3D9Device::GlD3D9Device(unsigned w,unsigned h,bool frame_queries):PcSoftD3D9Device(w,h),frame_queries_(frame_queries){
    programs_[0]=gl_program(vertex(true),gl_shaders::ffp);
    glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);glGenBuffers(3,ubo_);
    glGenSamplers(6,samplers_);
    if(frame_queries_){glGenQueries(1,&query_);glGenQueries(1,&timer_);}
    glGenFramebuffers(1,&framebuffer_);glGenTextures(1,&back_texture_);glGenRenderbuffers(1,&back_depth_);
    allocate_back(w,h);
    fxaa_program_=gl_program(FxaaVertex,FxaaFragment);glGenVertexArrays(1,&fxaa_vao_);
    const unsigned char black[4]{0,0,0,0};glGenTextures(1,&dummy2d_);glBindTexture(GL_TEXTURE_2D,dummy2d_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,black);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glGenTextures(1,&dummycube_);glBindTexture(GL_TEXTURE_CUBE_MAP,dummycube_);
    for(unsigned face=0;face<6;++face)glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,black);
    glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    bind_target();gl_require("D3D9 GPU initialization");
}
GlD3D9Device::~GlD3D9Device(){
    if(query_live_){glEndQuery(GL_SAMPLES_PASSED);glEndQuery(GL_TIME_ELAPSED);}
    for(auto& [_,t]:textures_gl_)glDeleteTextures(1,&t.id);for(auto& [_,d]:depth_gl_)glDeleteRenderbuffers(1,&d);
    for(auto p:programs_)glDeleteProgram(p);glDeleteBuffers(3,ubo_);glDeleteBuffers(1,&vbo_);glDeleteVertexArrays(1,&vao_);glDeleteQueries(1,&query_);
    for(const auto& [_,p]:shader_programs_)glDeleteProgram(p);
    glDeleteFramebuffers(1,&framebuffer_);glDeleteTextures(1,&back_texture_);glDeleteRenderbuffers(1,&back_depth_);
    glDeleteTextures(1,&dummy2d_);glDeleteTextures(1,&dummycube_);
    glDeleteSamplers(6,samplers_);
    glDeleteProgram(fxaa_program_);glDeleteVertexArrays(1,&fxaa_vao_);if(fxaa_copy_)glDeleteTextures(1,&fxaa_copy_);
    if(msaa_fbo_){glDeleteFramebuffers(1,&msaa_fbo_);glDeleteRenderbuffers(1,&msaa_colour_);glDeleteRenderbuffers(1,&msaa_depth_);}
    glDeleteQueries(1,&timer_);
}
void GlD3D9Device::allocate_back(unsigned w,unsigned h){
    if(!w||!h||w>16384||h>16384)throw std::runtime_error("invalid GPU back buffer size");
    glBindTexture(GL_TEXTURE_2D,back_texture_);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,int(w),int(h),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindRenderbuffer(GL_RENDERBUFFER,back_depth_);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,int(w),int(h));
    back_w_=w;back_h_=h;gl_require("GPU back buffer");
}
void GlD3D9Device::resize_back_buffer(unsigned w,unsigned h){if(w!=back_w_||h!=back_h_)allocate_back(w,h);}
void GlD3D9Device::finish_scene(){
    if(scene_finished_||count_only||colour_target_!=BackBuffer)return;
    if(msaa_live_){   // resolve into the back buffer, whose depth/stencil is cleared for the 2D layer
        msaa_live_=false;scene_finished_=true;bind_target();
        glBindFramebuffer(GL_READ_FRAMEBUFFER,msaa_fbo_);
        glBlitFramebuffer(0,0,int(back_w_),int(back_h_),0,0,int(back_w_),int(back_h_),GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
        glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glStencilMask(0xff);glClearDepth(1.0);glClearStencil(0);
        glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
        ++msaa_resolves;gl_require("MSAA resolve");return;
    }
    if(!fxaa_)return;
    scene_finished_=true;
    if(!fxaa_copy_||fxaa_copy_w_!=back_w_||fxaa_copy_h_!=back_h_){
        if(!fxaa_copy_)glGenTextures(1,&fxaa_copy_);
        glBindTexture(GL_TEXTURE_2D,fxaa_copy_);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,int(back_w_),int(back_h_),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        fxaa_copy_w_=back_w_;fxaa_copy_h_=back_h_;
    }
    glCopyImageSubData(back_texture_,GL_TEXTURE_2D,0,0,0,0,fxaa_copy_,GL_TEXTURE_2D,0,0,0,0,int(back_w_),int(back_h_),1);
    bind_target();
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glViewport(0,0,int(back_w_),int(back_h_));
    glUseProgram(fxaa_program_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,fxaa_copy_);glBindSampler(0,0);
    glBindVertexArray(fxaa_vao_);glDrawArrays(GL_TRIANGLES,0,3);
    ++fxaa_passes;gl_require("FXAA");
}
void GlD3D9Device::begin_frame(){
    scene_finished_=false;msaa_live_=msaa_>1&&!count_only;
    if(msaa_live_){   // (re)allocated for a new sample count or back buffer size
        GLint w=0,h=0;
        if(msaa_fbo_){glBindRenderbuffer(GL_RENDERBUFFER,msaa_colour_);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&w);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&h);}
        if(!msaa_fbo_||msaa_samples_!=msaa_||unsigned(w)!=back_w_||unsigned(h)!=back_h_){
            if(!msaa_fbo_){glGenFramebuffers(1,&msaa_fbo_);glGenRenderbuffers(1,&msaa_colour_);glGenRenderbuffers(1,&msaa_depth_);}
            glBindRenderbuffer(GL_RENDERBUFFER,msaa_colour_);glRenderbufferStorageMultisample(GL_RENDERBUFFER,int(msaa_),GL_RGBA8,int(back_w_),int(back_h_));
            glBindRenderbuffer(GL_RENDERBUFFER,msaa_depth_);glRenderbufferStorageMultisample(GL_RENDERBUFFER,int(msaa_),GL_DEPTH24_STENCIL8,int(back_w_),int(back_h_));
            glBindFramebuffer(GL_FRAMEBUFFER,msaa_fbo_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,msaa_colour_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,msaa_depth_);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("MSAA render target incomplete");
            msaa_samples_=msaa_;gl_require("MSAA targets");
        }
    }
    // Frame-wide sample counting is a desktop pixel-test diagnostic. On PS5 it
    // allocates an occlusion buffer per draw, and the SDK timer measures CPU wall
    // time with submission drains rather than hardware GPU timestamps.
    if(!count_only&&frame_queries_){glBeginQuery(GL_SAMPLES_PASSED,query_);glBeginQuery(GL_TIME_ELAPSED,timer_);query_live_=true;}
}
void GlD3D9Device::end_frame(){
    if(msaa_live_){const auto target=colour_target_;colour_target_=BackBuffer;finish_scene();colour_target_=target;}   // no 2D layer this frame
    if(query_live_){glEndQuery(GL_SAMPLES_PASSED);glEndQuery(GL_TIME_ELAPSED);query_live_=false;GLuint64 passed=0;glGetQueryObjectui64v(query_,GL_QUERY_RESULT,&passed);pixels_shaded+=passed;glGetQueryObjectui64v(timer_,GL_QUERY_RESULT,&gpu_ns_);}
    gl_require("D3D9 GPU frame");
}
unsigned GlD3D9Device::texture(std::uint32_t handle){
    auto* o=object(handle);if(!o||o->kind!=KTexture)return 0;
    if(auto it=textures_gl_.find(handle);it!=textures_gl_.end())return it->second.id;
    Texture t{};t.target=o->texture.cube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;glGenTextures(1,&t.id);glBindTexture(t.target,t.id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    for(unsigned face=0;face<o->rgba.size();++face)for(unsigned level=0;level<o->rgba[face].size();++level){
        const auto& size=o->texture.faces[face][level];const auto& rgba=o->rgba[face][level];
        glTexImage2D(o->texture.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+face:GL_TEXTURE_2D,int(level),GL_RGBA8,int(size.width),int(size.height),0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.empty()?nullptr:rgba.data());
    }
    glTexParameteri(t.target,GL_TEXTURE_MAX_LEVEL,int(o->texture.levels)-1);glTexParameteri(t.target,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(t.target,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    textures_gl_[handle]=t;gl_require("D3D9 texture upload");return t.id;
}
unsigned GlD3D9Device::depth(std::uint32_t handle){
    if(handle==DepthBuffer)return back_depth_;if(!handle)return 0;
    if(auto it=depth_gl_.find(handle);it!=depth_gl_.end())return it->second;
    auto* o=object(handle);if(!o||o->kind!=KSurface||o->parent)throw std::runtime_error("invalid GPU depth surface");
    unsigned id=0;glGenRenderbuffers(1,&id);glBindRenderbuffer(GL_RENDERBUFFER,id);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,int(o->width),int(o->height));
    depth_gl_[handle]=id;return id;
}
void GlD3D9Device::bind_target(){
    if(colour_target_==BackBuffer&&msaa_live_){   // the multisampled scene (its own depth/stencil)
        if(depth_target_!=DepthBuffer&&depth_target_)throw std::runtime_error("MSAA scene with a separate depth surface");
        glBindFramebuffer(GL_FRAMEBUFFER,msaa_fbo_);return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
    if(colour_target_==BackBuffer)glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,back_texture_,0);
    else{auto* s=object(colour_target_);if(!s||!s->parent)throw std::runtime_error("invalid GPU colour surface");
        auto* parent=object(s->parent);if(!parent)throw std::runtime_error("GPU surface has no parent");
        const auto id=texture(s->parent);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,parent->texture.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+s->face:GL_TEXTURE_2D,id,int(s->level));}
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth(depth_target_));
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU render target incomplete");
}
void GlD3D9Device::set_render_target(std::uint32_t index,std::uint32_t handle){PcSoftD3D9Device::set_render_target(index,handle);/* attachments bound once both surfaces agree */}
void GlD3D9Device::set_depth_stencil_surface(std::uint32_t handle){PcSoftD3D9Device::set_depth_stencil_surface(handle);}
void GlD3D9Device::release(std::uint32_t handle){
    PcSoftD3D9Device::release(handle);if(object(handle))return;
    if(auto it=textures_gl_.find(handle);it!=textures_gl_.end()){glDeleteTextures(1,&it->second.id);textures_gl_.erase(it);}
    if(auto it=depth_gl_.find(handle);it!=depth_gl_.end()){glDeleteRenderbuffers(1,&it->second);depth_gl_.erase(it);}
    if(auto it=shader_programs_.find(handle);it!=shader_programs_.end()){glDeleteProgram(it->second);shader_programs_.erase(it);}
}
void GlD3D9Device::unlock_rect(std::uint32_t handle){
    auto* s=object(handle);const auto parent=s?s->parent:0;PcSoftD3D9Device::unlock_rect(handle);
    if(!parent||!textures_gl_.count(parent))return;
    s=object(handle);auto* p=object(parent);auto& t=textures_gl_.at(parent);glBindTexture(t.target,t.id);
    glTexSubImage2D(p->texture.cube?GL_TEXTURE_CUBE_MAP_POSITIVE_X+s->face:GL_TEXTURE_2D,int(s->level),0,0,int(s->width),int(s->height),GL_RGBA,GL_UNSIGNED_BYTE,p->rgba[s->face][s->level].data());
    gl_require("D3D9 surface unlock");
}
void GlD3D9Device::clear(std::uint32_t flags,std::uint32_t colour,float z,std::uint32_t stencil){
    if(count_only)return;bind_target();glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glStencilMask(0xff);
    glClearColor(float((colour>>16)&255)/255,float((colour>>8)&255)/255,float(colour&255)/255,float(colour>>24)/255);glClearDepth(z);glClearStencil(int(stencil));
    glClear((flags&1?GL_COLOR_BUFFER_BIT:0)|(flags&2?GL_DEPTH_BUFFER_BIT:0)|(flags&4?GL_STENCIL_BUFFER_BIT:0));gl_require("D3D9 clear");
}
void GlD3D9Device::raster(const Vertex* vertices,unsigned count){
    if(count<3)return;
    batch_rhw_=pretransformed_;
    auto push=[&](const Vertex& source){auto v=source;
        if(pretransformed_){const float w=1.f/v.pos[3];v.pos[0]=(2*(v.pos[0]+.5f-soft_viewport_.x)/soft_viewport_.w-1)*w;
            v.pos[1]=(1-2*(v.pos[1]+.5f-soft_viewport_.y)/soft_viewport_.h)*w;v.pos[2]=v.pos[2]*w;v.pos[3]=w;}
        else{v.pos[0]+=v.pos[3]/soft_viewport_.w;v.pos[1]-=v.pos[3]/soft_viewport_.h;}
        batch_.push_back(v);};
    for(unsigned k=1;k+1<count;++k){push(vertices[0]);push(vertices[k]);push(vertices[k+1]);++triangles_drawn;}
}
std::uint32_t GlD3D9Device::draw_indexed_primitive(std::uint32_t type,std::int32_t base,std::uint32_t min,std::uint32_t vertices,std::uint32_t start,std::uint32_t count){
    batch_.clear();const auto result=PcSoftD3D9Device::draw_indexed_primitive(type,base,min,vertices,start,count);flush();return result;
}
void GlD3D9Device::draw_primitive_up(std::uint32_t type,std::uint32_t count,const void* data,std::uint32_t stride){
    batch_.clear();PcSoftD3D9Device::draw_primitive_up(type,count,data,stride);flush();
}
void GlD3D9Device::uniform(unsigned binding,const void* data,std::size_t bytes){glBindBuffer(GL_UNIFORM_BUFFER,ubo_[binding]);glBufferData(GL_UNIFORM_BUFFER,GLsizeiptr(bytes),data,GL_STREAM_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,binding,ubo_[binding]);}
void GlD3D9Device::apply_state(){
    // The logical PC target stretched to the back buffer's pixels; with the UI
    // rectangle, pretransformed (2D) draws keep a centred 4:3 area.
    const bool back=colour_target_==BackBuffer;
    double sx=back?double(back_w_)/back_width_:1,sy=back?double(back_h_)/back_height_:1,ox=0;
    if(back&&ui_rect_&&batch_rhw_){const double w=double(back_h_)*4/3;ox=std::floor((back_w_-w)/2);sx=w/back_width_;}
    glClipControl(GL_UPPER_LEFT,GL_ZERO_TO_ONE);
    glViewportIndexedf(0,float(ox+soft_viewport_.x*sx),float(soft_viewport_.y*sy),float(soft_viewport_.w*sx),float(soft_viewport_.h*sy));glDepthRange(soft_viewport_.min_z,soft_viewport_.max_z);
    glEnable(GL_SCISSOR_TEST);glScissor(int(std::lround(ox+soft_viewport_.x*sx)),int(std::lround(soft_viewport_.y*sy)),int(std::lround(soft_viewport_.w*sx)),int(std::lround(soft_viewport_.h*sy)));
    if(render[RS_CULLMODE]==CULL_NONE)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glFrontFace(GL_CW);glCullFace(render[RS_CULLMODE]==CULL_CCW?GL_BACK:GL_FRONT);}
    if(render[RS_ZENABLE])glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);glDepthMask(render[RS_ZWRITEENABLE]?GL_TRUE:GL_FALSE);glDepthFunc(comparison(render[RS_ZFUNC]));
    if(render[52])glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);glStencilFunc(comparison(render[56]),int(render[57]&255),render[58]);glStencilMask(render[59]);glStencilOp(stencil_op(render[53]),stencil_op(render[54]),stencil_op(render[55]));
    const auto mask=render[RS_COLORWRITEENABLE];glColorMask(mask&1?GL_TRUE:GL_FALSE,mask&2?GL_TRUE:GL_FALSE,mask&4?GL_TRUE:GL_FALSE,mask&8?GL_TRUE:GL_FALSE);
    if(render[RS_ALPHABLENDENABLE])glEnable(GL_BLEND);else glDisable(GL_BLEND);
    const bool separate=render[RS_SEPARATEALPHABLENDENABLE]!=0;
    glBlendFuncSeparate(blend_factor(render[RS_SRCBLEND]),blend_factor(render[RS_DESTBLEND]),blend_factor(render[separate?RS_SRCBLENDALPHA:RS_SRCBLEND]),blend_factor(render[separate?RS_DESTBLENDALPHA:RS_DESTBLEND]));
    glBlendEquationSeparate(blend_op(render[RS_BLENDOP]),blend_op(render[separate?RS_BLENDOPALPHA:RS_BLENDOP]));
    const float bias=f(render[RS_DEPTHBIAS]),slope=f(render[RS_SLOPESCALEDEPTHBIAS]);if(bias!=0||slope!=0){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(slope,bias*16777215.f);}else glDisable(GL_POLYGON_OFFSET_FILL);
}
void GlD3D9Device::bind_textures(){
    for(unsigned s=0;s<6;++s){auto* o=object(textures[s]);const auto id=texture(textures[s]);const bool cube=o&&o->kind==KTexture&&o->texture.cube;
        glActiveTexture(GL_TEXTURE0+s);glBindTexture(GL_TEXTURE_2D,id&&!cube?id:dummy2d_);
        glActiveTexture(GL_TEXTURE0+6+s);glBindTexture(GL_TEXTURE_CUBE_MAP,id&&cube?id:dummycube_);
        glBindSampler(s,samplers_[s]);glBindSampler(6+s,samplers_[s]);
        if(!id)continue;const auto target=cube?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;glActiveTexture(GL_TEXTURE0+s+(cube?6:0));
        const auto& p=sampler[s];const bool linear=p[SAMP_MINFILTER]!=1;const auto mip=p[SAMP_MIPFILTER];
        const auto sm=samplers_[s];
        glSamplerParameteri(sm,GL_TEXTURE_MIN_FILTER,mip==0?(linear?GL_LINEAR:GL_NEAREST):mip==1?(linear?GL_LINEAR_MIPMAP_NEAREST:GL_NEAREST_MIPMAP_NEAREST):(linear?GL_LINEAR_MIPMAP_LINEAR:GL_NEAREST_MIPMAP_LINEAR));
        glSamplerParameteri(sm,GL_TEXTURE_MAG_FILTER,p[SAMP_MAGFILTER]==1?GL_NEAREST:GL_LINEAR);
        glSamplerParameteri(sm,GL_TEXTURE_WRAP_S,address(p[SAMP_ADDRESSU]));glSamplerParameteri(sm,GL_TEXTURE_WRAP_T,address(p[SAMP_ADDRESSV]));glSamplerParameteri(sm,GL_TEXTURE_WRAP_R,address(p[SAMP_ADDRESSW]));
        glSamplerParameterf(sm,GL_TEXTURE_LOD_BIAS,f(p[SAMP_MIPMAPLODBIAS]));glSamplerParameterf(sm,GL_TEXTURE_MIN_LOD,float(std::min(p[SAMP_MAXMIPLEVEL],o->texture.levels-1)));
        const auto c=p[SAMP_BORDERCOLOR];const float border[]{float((c>>16)&255)/255,float((c>>8)&255)/255,float(c&255)/255,float(c>>24)/255};glSamplerParameterfv(sm,GL_TEXTURE_BORDER_COLOR,border);
    }
}
void GlD3D9Device::flush(){
    if(batch_.empty()||count_only)return;bind_target();apply_state();bind_textures();
    auto* ps=object(pixel_shader);const unsigned kind=ps&&ps->kind==4?(ps->program.version==0xffff0101u?1:2):0;
    unsigned executable=programs_[0];
    if(kind){
        auto found=shader_programs_.find(pixel_shader);
        if(found==shader_programs_.end()){
            // Specialize the recovered instruction sequence, leaving material,
            // constants and textures dynamic. A giant runtime switch would
            // compile every shader in the game for each first draw.
            if(ps->program.blobs.size()>16)throw std::runtime_error("PS program exceeds 16 blobs");
            std::string source=kind==1?gl_shaders::ps11:gl_shaders::ps14;
            const std::string declaration="ivec4 program[4];ivec4 info;";
            const auto at=source.find(declaration);if(at==std::string::npos)throw std::runtime_error("PS uniform layout mismatch");
            source.replace(at,declaration.size(),"ivec4 unused_program[4];ivec4 unused_info;");
            const auto insert=source.find('\n',source.find("uniform PsProgram"))+1;
            std::string fixed="const ivec4 program[4]=ivec4[4](";
            for(unsigned row=0;row<4;++row){if(row)fixed+=",";fixed+="ivec4(";
                for(unsigned col=0;col<4;++col){if(col)fixed+=",";const auto k=row*4+col;fixed+=k<ps->program.blobs.size()?std::to_string(ps->program.blobs[k]):"-1";}fixed+=")";}
            fixed+=");const ivec4 info=ivec4("+std::to_string(ps->program.blobs.size())+",0,0,0);\n";
            source.insert(insert,fixed);
            found=shader_programs_.emplace(pixel_shader,gl_program(vertex(false),source)).first;
        }
        executable=found->second;
    }
    glUseProgram(executable);
    uniform(0,ps_constants.data(),8*4*sizeof(float));
    if(kind){struct {std::int32_t program[16],info[4],kinds[8];float bump[8][4],lum[2][4];} p{};
        std::fill(std::begin(p.program),std::end(p.program),-1);if(ps->program.blobs.size()>16)throw std::runtime_error("PS program exceeds 16 blobs");
        for(unsigned k=0;k<ps->program.blobs.size();++k)p.program[k]=int(ps->program.blobs[k]);p.info[0]=int(ps->program.blobs.size());
        for(unsigned s=0;s<8;++s){auto* t=object(textures[s]);p.kinds[s]=t&&t->kind==KTexture&&t->texture.cube?1:0;for(unsigned k=0;k<4;++k)p.bump[s][k]=f(stage[s][7+k]);}
        for(unsigned s=0;s<4;++s){p.lum[s>>1][(s&1)*2]=f(stage[s][22]);p.lum[s>>1][(s&1)*2+1]=f(stage[s][23]);}uniform(1,&p,sizeof p);
    }else{struct {std::int32_t op[4][4],arg[4][4],misc[4][4];float konst[4][4],tfactor[4];std::int32_t flags[4];} p{};
        for(unsigned s=0;s<4;++s){const auto& t=stage[s];p.op[s][0]=int(t[1]);p.op[s][1]=int(t[2]);p.op[s][2]=int(t[3]);p.op[s][3]=int(t[4]);
            p.arg[s][0]=int(t[5]);p.arg[s][1]=int(t[6]);p.arg[s][2]=int(t[26]);p.arg[s][3]=int(t[27]);auto* tx=object(textures[s]);
            p.misc[s][0]=int(t[28]);p.misc[s][1]=int(t[11]&3);p.misc[s][2]=tx&&tx->kind==KTexture&&!tx->texture.cube;p.misc[s][3]=tx&&tx->kind==KTexture&&tx->texture.cube;
            const auto c=t[32];p.konst[s][0]=float((c>>16)&255)/255;p.konst[s][1]=float((c>>8)&255)/255;p.konst[s][2]=float(c&255)/255;p.konst[s][3]=float(c>>24)/255;}
        const auto c=render[RS_TEXTUREFACTOR];p.tfactor[0]=float((c>>16)&255)/255;p.tfactor[1]=float((c>>8)&255)/255;p.tfactor[2]=float(c&255)/255;p.tfactor[3]=float(c>>24)/255;p.flags[0]=render[RS_SPECULARENABLE]!=0;uniform(1,&p,sizeof p);}
    float fixed[12]{};const auto c=render[RS_FOGCOLOR];fixed[0]=float((c>>16)&255)/255;fixed[1]=float((c>>8)&255)/255;fixed[2]=float(c&255)/255;fixed[3]=f(render[RS_FOGEND]);
    if(render[RS_FOGENABLE])fixed[4]=render[RS_FOGTABLEMODE]?float(render[RS_FOGTABLEMODE]):4.f;
    fixed[5]=f(render[RS_FOGSTART]);fixed[6]=f(render[RS_FOGDENSITY]);fixed[7]=1;fixed[8]=render[RS_ALPHATESTENABLE]?1.f:0.f;fixed[9]=float(render[RS_ALPHAREF]&255);fixed[10]=float(render[RS_ALPHAFUNC]);uniform(2,fixed,sizeof fixed);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(batch_.size()*sizeof(Vertex)),batch_.data(),GL_STREAM_DRAW);
    for(unsigned a=0;a<12;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,a==11?1:4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(std::uintptr_t(a*4*sizeof(float))));}
    glDrawArrays(GL_TRIANGLES,0,GLsizei(batch_.size()));++submitted_draws;gl_require("D3D9 draw");batch_.clear();batch_rhw_=false;
}
}
