#pragma once
#include "vk_api.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace outrun::ps5_runtime::vulkan {
struct Image {
    VkDevice device{}; VkImage image{}; VkDeviceMemory memory{}; VkImageView view{};
    unsigned width{}, height{}, levels{1}, layers{1}; VkFormat format{};
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED}; bool owned{true};
    const void* retained_by{};std::uint64_t retained_epoch{};
    std::map<std::pair<unsigned,unsigned>,VkImageView> attachments;
    ~Image();
    VkImageView attachment(unsigned face=0, unsigned level=0);
};
using Texture = std::shared_ptr<Image>;
struct Buffer {
    VkDevice device{}; VkBuffer buffer{}; VkDeviceMemory memory{}; void* mapped{};
    VkDeviceSize size{}, used{};
    const void* retained_by{};std::uint64_t retained_epoch{};
    ~Buffer();
};
struct Slice { VkBuffer buffer{}; VkDeviceSize offset{}, size{}; void* mapped{}; };
struct Target { Texture colour, depth; unsigned face{}, level{}; };
struct DrawBindings {
    VkPipeline pipeline{};VkDescriptorSet descriptors{};
    std::array<std::uint32_t,5> uniforms{};
    VkViewport viewport{};VkRect2D scissor{};
    float depth_bias{},slope_bias{};std::uint32_t stencil{};
    std::array<float,4> blend{};
    unsigned vertex_count{};
    std::array<VkBuffer,5> vertices{};std::array<VkDeviceSize,5> offsets{};
    VkBuffer index{};
};
struct PipelineState {
    // No padding in the key: recovered D3D state, shader sequence and vertex layout.
    std::array<std::uint32_t,256> render{};
    std::array<std::int32_t,17> blobs{};
    std::array<std::int32_t,33> vertex_blobs{};
    std::array<std::array<std::uint32_t,3>,16> attributes{}; // binding, format, offset
    std::array<std::uint32_t,5> strides{};
    unsigned primitive{4};
    unsigned shader{}, layout{}, samples{1}, depth{}; VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    bool operator<(const PipelineState& b)const;
};
// Choose a scanout for the compositor: the smallest that fits, then the
// refresh nearest `refresh_mhz` (millihertz).
VkDisplayModePropertiesKHR select_display_mode(const std::vector<VkDisplayModePropertiesKHR>&,unsigned width,unsigned height,unsigned refresh_mhz=60000);
class Context {
public:
    Context(unsigned width, unsigned height, bool native_display);
    // A larger native scanout than the compositor (the launch 3D resolution),
    // set before the display opens.
    static void prefer_scanout(unsigned width,unsigned height);
    // The native display's refresh in Hz (60 or 120; 120 for the host test
    // display), known once the display is open.
    static unsigned display_refresh();
    // Presents `frames_per_second` (60 or 120) frames a second on a 120 Hz
    // display: each flip shows for one or two vblanks (VideoOut flip rate).
    static void pace_frames(unsigned frames_per_second);
    ~Context();
    Context(const Context&)=delete; Context& operator=(const Context&)=delete;
    Texture image(unsigned width,unsigned height,unsigned levels=1,unsigned layers=1,
                  VkFormat format=VK_FORMAT_R8G8B8A8_UNORM,unsigned samples=1);
    void upload(const Texture&,unsigned face,unsigned level,const void* rgba,std::size_t size);
    void transition(const Texture&,VkImageLayout);
    void target(const Target&);
    void end_pass();
    void clear(const Target&,unsigned flags,std::uint32_t argb,float depth,unsigned stencil);
    Slice allocate(const void*,std::size_t,VkDeviceSize alignment=16);
    Slice uniform(unsigned slot,const void*,std::size_t);
    std::shared_ptr<Buffer> geometry(const void*,std::size_t);
    void retain(const std::shared_ptr<Buffer>&);
    VkDescriptorSet descriptors(const std::array<Slice,3>&,const std::array<Texture,12>&,
                                const std::array<VkSampler,12>&,const std::array<Slice,2>& vertex_uniforms={});
    static std::array<std::uint32_t,5> uniform_offsets(const std::array<Slice,3>&,const std::array<Slice,2>& vertex_uniforms={});
    std::uint64_t descriptor_sets_allocated{},descriptor_sets_reused{};
    VkSampler sampler(const std::array<std::uint32_t,14>& state,unsigned levels);
    VkPipeline pipeline(const PipelineState&);
    std::size_t pipeline_count()const{return pipelines_.size();}
    unsigned warmed_pipelines()const{return warmed_pipelines_;}
    std::uint64_t prepared_shader_uses()const{return prepared_shader_uses_;}
    std::uint64_t linked_shader_uses()const{return linked_shader_uses_;}
    VkCommandBuffer commands();
    VkCommandBuffer bind_draw(const DrawBindings&);
    std::uint64_t state_commands{},state_commands_saved{};
    void finish();
    void trace_next_submission(){trace_submission_=true;}
    std::vector<std::uint8_t> readback(const Texture&,unsigned face=0,unsigned level=0);
    void resolve(const Texture& source,const Texture& destination);
    void copy(const Texture& source,const Texture& destination);
    Texture display_target();
    void present();
    VkPipelineLayout pipeline_layout()const{return pipeline_layout_;}
    VkDevice device()const{return device_;}
    unsigned display_width()const{return display_width_;}
    unsigned display_height()const{return display_height_;}
    unsigned max_samples()const{return max_samples_;}
    VkFormat depth_format()const{return depth_format_;}
    std::uint64_t gpu_nanoseconds()const{return gpu_ns_;}
    std::uint64_t samples_passed()const{return samples_passed_;}
    // Occlusion results arrive when the recording that holds them retires
    // (its fence signals), like D3D9 GetData returning S_FALSE meanwhile.
    void begin_occlusion(); unsigned end_occlusion();
    std::uint64_t recording_epoch()const{return recording_epoch_;}
    using RetireListener=std::function<void(std::uint64_t epoch,const std::vector<std::uint64_t>& samples)>;
    unsigned add_retire_listener(RetireListener);
    void remove_retire_listener(unsigned id);
    // Submit the current recording without waiting; finish() also waits for
    // every recording in flight. poll() retires completed ones, never blocks.
    void submit();
    void poll();
    static constexpr unsigned FramesInFlight=2;
    const char* device_name()const{return properties_.deviceName;}
private:
    unsigned memory_type(unsigned bits,VkMemoryPropertyFlags flags);
    std::unique_ptr<Buffer> buffer(VkDeviceSize size,VkBufferUsageFlags usage);
    VkRenderPass render_pass(VkFormat,unsigned samples,bool depth);
    VkDescriptorPool descriptor_pool();
    void start(); void setup_display(unsigned width,unsigned height); void destroy();
    void load_pipeline_cache(); void save_pipeline_cache();
    VkShaderModule linked_shader(unsigned,const std::array<std::int32_t,33>&);
    VkPipelineCache pipeline_cache_{};
    std::string cache_path_;
    std::vector<PipelineState> cached_states_;
    std::uint64_t shader_hash_{};
    bool cache_dirty_{};
    unsigned cache_frames_{},warmed_pipelines_{};
    VkInstance instance_{}; VkPhysicalDevice physical_{}; VkDevice device_{}; VkQueue queue_{};
    VkPhysicalDeviceProperties properties_{}; VkPhysicalDeviceMemoryProperties memory_{};
    VkPhysicalDeviceFeatures features_{}; unsigned family_{},max_samples_{1};
    bool mirror_clamp_{},custom_border_{};
    VkFormat depth_format_{VK_FORMAT_D24_UNORM_S8_UINT};
    // One slot per recording in flight: its commands, fence and everything the
    // GPU may still read (uploads, descriptors, framebuffers, retained images).
    struct Frame {
        VkCommandPool pool{}; VkCommandBuffer command{}; VkFence fence{}; VkSemaphore acquired{};
        bool submitted{}; std::uint64_t epoch{};
        std::vector<Texture> retained; std::vector<VkFramebuffer> framebuffers;
        std::vector<std::unique_ptr<Buffer>> buffers;
        std::vector<std::shared_ptr<Buffer>> retained_buffers;
        std::vector<VkDescriptorPool> pools; unsigned pool_index{},descriptor_count{};
        VkQueryPool occlusion{},timestamps{}; unsigned queries{};
    };
    void retire(Frame&);
    std::array<Frame,FramesInFlight> frames_{}; unsigned frame_{};
    std::uint64_t retired_epoch_{};
    std::vector<std::pair<unsigned,RetireListener>> listeners_; unsigned next_listener_{1};
    VkCommandBuffer command_{};
    bool recording_{},pass_live_{}; Target active_{}; VkRenderPass active_pass_{};
    std::uint64_t recording_epoch_{};
    std::array<Slice,6> last_uniforms_{};
    // CPU copies of the last uniforms: mapped GPU memory can be uncached, so
    // never read it back to detect unchanged constants.
    std::array<std::vector<std::uint8_t>,6> last_uniform_bytes_{};
    Slice empty_uniform_{};
    std::array<std::uint64_t,39> last_descriptors_{};
    VkDescriptorSet last_descriptor_set_{};
    struct DescriptorHash {
        std::size_t operator()(const std::array<std::uint64_t,39>& key)const noexcept {
            std::uint64_t hash=0xcbf29ce484222325ull;
            for(auto word:key)hash=(hash^word)*0x100000001b3ull;
            return std::size_t(hash^(hash>>32));
        }
    };
    std::unordered_map<std::array<std::uint64_t,39>,VkDescriptorSet,DescriptorHash> descriptor_sets_;
    DrawBindings last_bindings_{};bool bindings_valid_{};
    bool trace_submission_{};
    VkDescriptorSetLayout descriptor_layout_{}; VkPipelineLayout pipeline_layout_{};
    std::map<std::array<unsigned,3>,VkRenderPass> passes_;
    std::map<PipelineState,VkPipeline> pipelines_;
    std::map<std::array<std::uint32_t,15>,VkSampler> samplers_;
    std::array<VkShaderModule,9> shaders_{};
    std::map<std::pair<unsigned,std::array<std::int32_t,33>>,VkShaderModule> prepared_shaders_;
    std::map<std::pair<unsigned,std::array<std::int32_t,33>>,VkShaderModule> linked_shaders_;
    std::uint64_t prepared_shader_uses_{};
    std::uint64_t linked_shader_uses_{};
    bool query_live_{};
    std::uint64_t gpu_ns_{},samples_passed_{};
    bool native_{},splash_hidden_{}; VkSurfaceKHR surface_{}; VkSwapchainKHR swapchain_{};
    std::vector<Texture> swap_images_; Texture host_display_;
    // Present waits per swapchain image; acquires use the frame slot's semaphore.
    std::vector<VkSemaphore> rendered_; bool acquired_live_{},present_pending_{}; unsigned swap_index_{};
    unsigned display_width_{},display_height_{};
};
}
