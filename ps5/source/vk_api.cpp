#include "vk_api.hpp"
#include <stdexcept>
#include <string>
#ifdef OR2_PS5_PAYLOAD
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance, const char*);
#else
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char*);
#endif
namespace outrun::ps5_runtime::vulkan {
namespace {
PFN_vkVoidFunction instance_proc(VkInstance instance, const char* name) {
#ifdef OR2_PS5_PAYLOAD
    return ::vk_icdGetInstanceProcAddr(instance, name);
#else
    return ::vkGetInstanceProcAddr(instance, name);
#endif
}
template<class T> T checked(PFN_vkVoidFunction function, const char* name) {
    if (!function) throw std::runtime_error(std::string("Missing Vulkan entry point: ") + name);
    return reinterpret_cast<T>(function);
}
}
void require(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
void load_global() { vkCreateInstance = checked<PFN_vkCreateInstance>(instance_proc(VK_NULL_HANDLE, "vkCreateInstance"), "vkCreateInstance"); }
void load_instance(VkInstance instance, bool display) {
#define LOAD(name) vk##name = checked<PFN_vk##name>(instance_proc(instance, "vk" #name), "vk" #name);
    OR2_VK_INSTANCE(LOAD)
    if (display) { OR2_VK_DISPLAY(LOAD) }
#undef LOAD
}
void load_device(VkDevice device, bool display) {
#define LOAD(name) vk##name = checked<PFN_vk##name>(vkGetDeviceProcAddr(device, "vk" #name), "vk" #name);
    OR2_VK_DEVICE(LOAD)
    if (display) { OR2_VK_SWAPCHAIN(LOAD) }
#undef LOAD
}
}
