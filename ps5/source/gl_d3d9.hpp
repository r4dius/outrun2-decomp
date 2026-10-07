#pragma once
#include "gl_api.hpp"
#include "platform/pc_soft_d3d9.hpp"
#include <map>
namespace outrun::ps5_runtime {
// Keep the recovered vertex programs and D3D9 resource/state ownership. Raster,
// pixel shaders, depth/stencil, blend, render targets and reflections use GL.
class GlD3D9Device final:public platform::PcSoftD3D9Device {
public:
#ifdef OR2_PS5_PAYLOAD
    static constexpr bool DefaultFrameQueries=false;
#else
    static constexpr bool DefaultFrameQueries=true;
#endif
    GlD3D9Device(unsigned width,unsigned height,bool frame_queries=DefaultFrameQueries);
    ~GlD3D9Device()override;
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void draw_primitive_up(std::uint32_t,std::uint32_t,const void*,std::uint32_t)override;
    void clear(std::uint32_t,std::uint32_t,float,std::uint32_t)override;
    void set_render_target(std::uint32_t,std::uint32_t)override;
    void set_depth_stencil_surface(std::uint32_t)override;
    void unlock_rect(std::uint32_t)override;
    void release(std::uint32_t)override;
    void begin_frame();void end_frame();
    unsigned back_texture()const{return back_texture_;}
    std::uint64_t gpu_nanoseconds()const{return gpu_ns_;}
    // Port enhancements (Options > Settings), as the macOS Metal device: the
    // back buffer at any pixel size (the logical PC screen is stretched to it),
    // the 2D layer kept in the centred 4:3 rectangle of a wider back buffer
    // (pretransformed draws), and the 3D scene's antialiasing: MSAA (drawn
    // multisampled, resolved when the 2D layer starts) or FXAA over it then
    // (finish_scene, PC 42D710; end_frame when no 2D layer came). The setters
    // apply between frames.
    void resize_back_buffer(unsigned width,unsigned height);
    void set_ui_rect(bool on){ui_rect_=on;}
    void set_fxaa(bool on){fxaa_=on;}
    void set_msaa(unsigned samples){msaa_=samples==2||samples==4?samples:1;}
    void finish_scene();
    unsigned presentation_width()const{return back_w_;}
    unsigned presentation_height()const{return back_h_;}
    std::uint32_t fxaa_passes{},msaa_resolves{};
    std::uint64_t submitted_draws{};
private:
    void raster(const Vertex*,unsigned)override;
    void flush();void bind_target();void apply_state();void bind_textures();
    unsigned texture(std::uint32_t);unsigned depth(std::uint32_t);
    void uniform(unsigned binding,const void*,std::size_t);
    std::vector<Vertex> batch_;
    struct Texture {unsigned id{},target{};};std::map<std::uint32_t,Texture> textures_gl_;
    std::map<std::uint32_t,unsigned> depth_gl_;
    unsigned framebuffer_{},back_texture_{},back_depth_{},vao_{},vbo_{},ubo_[3]{},query_{},dummy2d_{},dummycube_{};
    unsigned programs_[3]{};bool query_live_{},frame_queries_{};
    unsigned samplers_[6]{};
    std::map<std::uint32_t,unsigned> shader_programs_;
    unsigned timer_{};std::uint64_t gpu_ns_{};
    void allocate_back(unsigned width,unsigned height);
    unsigned back_w_{},back_h_{};
    unsigned msaa_{1},msaa_samples_{},msaa_fbo_{},msaa_colour_{},msaa_depth_{};
    unsigned fxaa_program_{},fxaa_vao_{},fxaa_copy_{},fxaa_copy_w_{},fxaa_copy_h_{};
    bool msaa_live_{},ui_rect_{},fxaa_{},scene_finished_{},batch_rhw_{};
};
}
