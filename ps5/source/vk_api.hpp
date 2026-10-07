#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// PS5 links a static RADV ICD. Desktop validation uses the Vulkan loader.
// Keep the same explicit dispatch path on both platforms.
#define OR2_VK_INSTANCE(X) \
 X(DestroyInstance) X(EnumeratePhysicalDevices) X(GetPhysicalDeviceProperties) \
 X(GetPhysicalDeviceFeatures) X(GetPhysicalDeviceFeatures2) X(GetPhysicalDeviceMemoryProperties) \
 X(GetPhysicalDeviceQueueFamilyProperties) X(GetPhysicalDeviceFormatProperties) \
 X(EnumerateDeviceExtensionProperties) X(CreateDevice) X(GetDeviceProcAddr)
#define OR2_VK_DISPLAY(X) \
 X(GetPhysicalDeviceDisplayPropertiesKHR) X(GetPhysicalDeviceDisplayPlanePropertiesKHR) \
 X(GetDisplayPlaneSupportedDisplaysKHR) X(GetDisplayModePropertiesKHR) \
 X(GetDisplayPlaneCapabilitiesKHR) X(CreateDisplayPlaneSurfaceKHR) X(DestroySurfaceKHR) \
 X(GetPhysicalDeviceSurfaceSupportKHR) X(GetPhysicalDeviceSurfaceCapabilitiesKHR) \
 X(GetPhysicalDeviceSurfaceFormatsKHR) X(GetPhysicalDeviceSurfacePresentModesKHR)
#define OR2_VK_DEVICE(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(DeviceWaitIdle) X(QueueSubmit) X(QueueWaitIdle) \
 X(CreateCommandPool) X(DestroyCommandPool) X(ResetCommandPool) X(AllocateCommandBuffers) \
 X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(WaitForFences) X(ResetFences) X(GetFenceStatus) \
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) \
 X(BindBufferMemory) X(MapMemory) X(UnmapMemory) X(CreateImage) X(DestroyImage) \
 X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView) X(DestroyImageView) \
 X(CreateSampler) X(DestroySampler) X(CreateDescriptorSetLayout) X(DestroyDescriptorSetLayout) \
 X(CreateDescriptorPool) X(DestroyDescriptorPool) X(ResetDescriptorPool) X(AllocateDescriptorSets) \
 X(UpdateDescriptorSets) X(CreatePipelineLayout) X(DestroyPipelineLayout) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreateGraphicsPipelines) X(DestroyPipeline) \
 X(CreatePipelineCache) X(DestroyPipelineCache) X(GetPipelineCacheData) \
 X(CreateRenderPass) X(DestroyRenderPass) X(CreateFramebuffer) X(DestroyFramebuffer) \
 X(CmdPipelineBarrier) X(CmdCopyBufferToImage) X(CmdCopyImageToBuffer) X(CmdCopyImage) X(CmdResolveImage) \
 X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdClearAttachments) X(CmdBindPipeline) \
 X(CmdBindDescriptorSets) X(CmdBindVertexBuffers) X(CmdSetViewport) X(CmdSetScissor) \
 X(CmdSetDepthBias) X(CmdSetBlendConstants) X(CmdSetStencilReference) X(CmdDraw) X(CmdBindIndexBuffer) X(CmdDrawIndexed) \
 X(CreateQueryPool) X(DestroyQueryPool) X(GetQueryPoolResults) X(CmdResetQueryPool) \
 X(CmdWriteTimestamp) X(CmdBeginQuery) X(CmdEndQuery)
#define OR2_VK_SWAPCHAIN(X) \
 X(CreateSwapchainKHR) X(DestroySwapchainKHR) X(GetSwapchainImagesKHR) X(AcquireNextImageKHR) \
 X(QueuePresentKHR) X(CreateSemaphore) X(DestroySemaphore)
namespace outrun::ps5_runtime::vulkan {
#define OR2_VK_DECLARE(name) inline PFN_vk##name vk##name{};
OR2_VK_DECLARE(CreateInstance)
OR2_VK_INSTANCE(OR2_VK_DECLARE)
OR2_VK_DISPLAY(OR2_VK_DECLARE)
OR2_VK_DEVICE(OR2_VK_DECLARE)
OR2_VK_SWAPCHAIN(OR2_VK_DECLARE)
#undef OR2_VK_DECLARE
void load_global();
void load_instance(VkInstance instance, bool display);
void load_device(VkDevice device, bool display);
void require(VkResult result, const char* operation);
}
