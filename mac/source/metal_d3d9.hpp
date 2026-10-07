#pragma once
#include "platform/pc_soft_d3d9.hpp"
#include <memory>
namespace outrun::mac {
// Translates the recovered vertex/pixel programs to Metal, retaining the
// CPU resource ownership and an explicit reference vertex path for audits.
class MetalD3D9Device final : public platform::PcSoftD3D9Device {
public:
    MetalD3D9Device(unsigned width,unsigned height,unsigned render_scale=1);
    ~MetalD3D9Device() override;
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) override;
    void draw_primitive_up(std::uint32_t,std::uint32_t,const void*,std::uint32_t) override;
    void clear(std::uint32_t,std::uint32_t,float,std::uint32_t) override;
    void set_render_target(std::uint32_t,std::uint32_t) override;
    void set_depth_stencil_surface(std::uint32_t) override;
    void unlock_rect(std::uint32_t) override;
    void release(std::uint32_t) override;
    std::uint8_t* lock(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t) override;
    void begin_frame();
    void end_frame();
    void readback();
    std::uint64_t gpu_nanoseconds() const;
    std::string device_name() const;
    unsigned presentation_width() const;
    unsigned presentation_height() const;
    // Port enhancements (Options > Settings): the back buffer at any pixel
    // size (the logical PC screen is stretched to it), the 2D layer kept in the
    // centred 4:3 rectangle of a wider back buffer (pretransformed draws, as
    // the Switch device's UI rectangle), and the 3D scene's antialiasing:
    // MSAA (the back buffer drawn multisampled, resolved when the 2D layer
    // starts) or FXAA over it then (finish_scene, PC 42D710; end_frame when no
    // 2D layer came). The setters apply between frames.
    void resize_back_buffer(unsigned width,unsigned height);
    void set_ui_rect(bool on){ui_rect_=on;}
    void set_fxaa(bool on){fxaa_=on;}
    void set_msaa(unsigned samples){msaa_=samples==2||samples==4?samples:1;}   // 1, 2 or 4
    void finish_scene();
    std::uint32_t fxaa_passes{},msaa_resolves{};
private:
    bool shade_cached_vertex(std::uint32_t,std::int32_t,const platform::pc_shader::V4*,Vertex&);
    void raster(const Vertex*,unsigned) override;
    void flush();
    struct Impl;
    std::unique_ptr<Impl> metal_;
    std::vector<Vertex> batch_;
    std::vector<Vertex> vertex_cache_;
    std::vector<std::uint32_t> vertex_generation_;
    std::uint32_t generation_{};
    bool native_vertices_{};
    std::uint32_t native_type_{},native_start_{},native_count_{};
    std::int32_t native_base_{};
    std::vector<bool> buffer_dirty_;
    unsigned msaa_{1};
    bool msaa_live_{};   // the back buffer is the multisampled scene target (begin_frame .. finish_scene)
    bool ui_rect_{},fxaa_{},scene_finished_{},batch_rhw_{};   // batch_rhw_: the batch came from pretransformed vertices
};
}
