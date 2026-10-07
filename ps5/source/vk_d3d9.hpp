#pragma once
#include "vk_context.hpp"
#include "platform/pc_soft_d3d9.hpp"

namespace outrun::ps5_runtime {
// Recovered vertex/pixel programs run on Vulkan. The CPU interpreter remains
// a fallback/reference; game-facing handles/state stay with the common device.
class VulkanD3D9Device final: public platform::PcSoftD3D9Device {
public:
#ifdef OR2_PS5_PAYLOAD
    static constexpr bool DefaultFrameQueries=false;
#else
    static constexpr bool DefaultFrameQueries=true;
#endif
    VulkanD3D9Device(vulkan::Context&,unsigned width,unsigned height,bool frame_queries=DefaultFrameQueries);
    ~VulkanD3D9Device()override;
    std::uint32_t draw_indexed_primitive(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void draw_primitive_up(std::uint32_t,std::uint32_t,const void*,std::uint32_t)override;
    void clear(std::uint32_t,std::uint32_t,float,std::uint32_t)override;
    void unlock_rect(std::uint32_t)override;
    void release(std::uint32_t)override;
    void unlock(std::uint32_t handle)override{geometry_.erase(handle);index_ranges_.erase(handle);}
    void query_issue(std::uint32_t,std::uint32_t)override;
    std::uint32_t query_get_data(std::uint32_t,std::uint32_t&)override;
    void begin_frame(); void end_frame(); void finish_scene();
    void resize_back_buffer(unsigned width,unsigned height);
    void set_ui_rect(bool on){ui_rect_=on;}
    void set_fxaa(bool on){fxaa_=on;}
    void set_msaa(unsigned samples){msaa_=samples==2||samples==4?samples:1;}
    vulkan::Texture back_texture()const{return back_;}
    std::uint64_t gpu_nanoseconds()const{return context_.gpu_nanoseconds();}
    unsigned presentation_width()const{return back_->width;}
    unsigned presentation_height()const{return back_->height;}
    std::uint32_t fxaa_passes{},msaa_resolves{};
    std::uint64_t submitted_draws{};
    std::uint64_t native_vertex_draws{};
    std::uint64_t vertex_prepare_ns{},command_record_ns{};
    std::uint64_t timed_frames{};
    std::uint64_t index_range_hits{},index_scan_count{};
    void trace_menu_vertices(unsigned draws){menu_trace_draws_=draws;}
private:
#ifdef OR2_PS5_HOST_TEST
    friend struct VulkanVertexTestAccess;
#endif
    // CPU vertex translation is local to this backend. Reuse an index cache
    // across draws, but invalidate its entries on every draw/state change.
    std::uint32_t prepare_indexed(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
    bool prepare_vertex(std::uint16_t,std::int32_t,const platform::PcShaderProgram&,const platform::pc_shader::V4*,Vertex&);
    void clipped_triangle(const Vertex&,const Vertex&,const Vertex&);
    bool prepare_native(std::uint32_t,std::int32_t,std::uint32_t,std::uint32_t);
    void prepare_points(std::uint32_t,const void*,std::uint32_t);
    std::shared_ptr<vulkan::Buffer> geometry(std::uint32_t);
    std::map<std::uint32_t,std::shared_ptr<vulkan::Buffer>> geometry_;
    struct IndexRange {unsigned low{},high{};};
    std::map<std::uint32_t,std::map<std::uint64_t,IndexRange>> index_ranges_;
    vulkan::PipelineState native_state_{};
    std::int32_t native_base_{};
    std::uint32_t native_start_{},native_count_{};
    bool native_vertices_{};
#ifdef OR2_PS5_PAYLOAD
    bool sample_draw_times_{};unsigned diagnostic_frames_{};
#else
    bool sample_draw_times_{true};
#endif
    unsigned menu_trace_draws_{};
    struct InputBinding {const std::vector<std::uint8_t>* bytes{};std::size_t offset{};std::uint32_t stride{};unsigned type{},registers{};};
    std::vector<InputBinding> inputs_;
    std::vector<Vertex> indexed_vertices_;
    std::vector<std::uint32_t> vertex_epochs_;
    std::uint32_t vertex_epoch_{};
    void raster(const Vertex*,unsigned)override;
    void flush(); vulkan::Texture texture(std::uint32_t);vulkan::Texture depth(std::uint32_t);
    vulkan::Target target();
    vulkan::Context& context_;
    vulkan::Texture back_,back_depth_,msaa_colour_,msaa_depth_,fxaa_copy_,dummy2d_,dummycube_;
    std::map<std::uint32_t,vulkan::Texture> images_,depths_;
    std::vector<Vertex> batch_; bool batch_rhw_{},ui_rect_{},fxaa_{},scene_finished_{},msaa_live_{};
    unsigned msaa_{1},gpu_active_queries_{};std::uint64_t frame_samples_{};bool frame_queries_{};
    // D3D9 occlusion queries over per-draw Vulkan queries. Results arrive when
    // the recordings holding their draws retire; GetData reports S_FALSE until
    // then, as on the PC, so the scene never waits for the GPU mid-frame.
    struct Query {bool active{},ended{};std::uint32_t generation{},outstanding{};std::uint64_t total{};std::uint32_t result{};bool ready{};};
    std::map<std::uint32_t,Query> gpu_queries_;
    struct QueryDraw {std::uint64_t epoch{};unsigned index{};std::uint32_t handle{},generation{};};
    std::vector<QueryDraw> query_draws_;
    unsigned retire_listener_{};
    void retire_queries(std::uint64_t epoch,const std::vector<std::uint64_t>& samples);
};
}
