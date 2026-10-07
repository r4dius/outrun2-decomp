#include "vk_context.hpp"
#include "pc_vk_shaders.hpp"
#include "platform/pc_d3d9_state.hpp"
#include "enhancements/frame_rate.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <chrono>
#ifdef OR2_PS5_PAYLOAD
#include "frame_profile.hpp"
extern "C" int sceSystemServiceHideSplashScreen(void);
#endif

namespace outrun::ps5_runtime::vulkan {
using namespace platform::d3d9;
namespace {
constexpr unsigned QueryCapacity=32768, SetsPerPool=1024;
constexpr std::size_t MaxCacheBytes=64*1024*1024,MaxCachedStates=8192;
// Only fixed-width scalar/array fields, no handles or pointers are persisted.
static_assert(sizeof(PipelineState)==1460,"Bump cache version if pipeline key changes");
struct CacheHeader {char magic[8];std::uint32_t version,key_size,keys,bytes;std::uint64_t shaders,checksum;};
std::uint64_t hash_bytes(const void* data,std::size_t size,std::uint64_t hash=0xcbf29ce484222325ull){
    const auto* bytes=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i)hash=(hash^bytes[i])*0x100000001b3ull;return hash;
}
VkCompareOp comparison(unsigned value){return VkCompareOp(std::clamp(value,1u,8u)-1);}
VkStencilOp stencil(unsigned value){
    constexpr VkStencilOp ops[]{VK_STENCIL_OP_KEEP,VK_STENCIL_OP_KEEP,VK_STENCIL_OP_ZERO,VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_INCREMENT_AND_CLAMP,VK_STENCIL_OP_DECREMENT_AND_CLAMP,VK_STENCIL_OP_INVERT,
        VK_STENCIL_OP_INCREMENT_AND_WRAP,VK_STENCIL_OP_DECREMENT_AND_WRAP};
    return ops[std::min(value,8u)];
}
VkBlendFactor blend(unsigned value){
    constexpr VkBlendFactor factors[]{VK_BLEND_FACTOR_ONE,VK_BLEND_FACTOR_ZERO,VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR,VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_FACTOR_DST_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_DST_COLOR,VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
        VK_BLEND_FACTOR_SRC_ALPHA,VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,VK_BLEND_FACTOR_CONSTANT_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR};
    if(value>=std::size(factors))throw std::runtime_error("Unsupported D3D blend factor");
    return factors[value];
}
VkSamplerAddressMode address(unsigned value){
    return value==5?VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:value==2?VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:value==3?
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:value==4?VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:VK_SAMPLER_ADDRESS_MODE_REPEAT;
}
}
Image::~Image(){
    for(auto& [_,v]:attachments)vkDestroyImageView(device,v,nullptr);
    if(view)vkDestroyImageView(device,view,nullptr);
    if(owned&&image)vkDestroyImage(device,image,nullptr);
    if(memory)vkFreeMemory(device,memory,nullptr);
}
VkImageView Image::attachment(unsigned face,unsigned level){
    if(face>=layers||level>=levels)throw std::runtime_error("Vulkan attachment subresource outside image");
    const auto key=std::make_pair(face,level);
    if(auto it=attachments.find(key);it!=attachments.end())return it->second;
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    info.image=image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=format;
    info.subresourceRange={aspect,level,1,face,1};VkImageView result{};
    require(vkCreateImageView(device,&info,nullptr,&result),"Create attachment view");
    attachments[key]=result;return result;
}
Buffer::~Buffer(){
    if(mapped)vkUnmapMemory(device,memory);
    if(buffer)vkDestroyBuffer(device,buffer,nullptr);
    if(memory)vkFreeMemory(device,memory,nullptr);
}
bool PipelineState::operator<(const PipelineState& b)const{
    // Fixed-width fields with no padding (asserted above). Byte order need not
    // be numeric: map ordering only requires a stable strict total order.
    return std::memcmp(this,&b,sizeof(*this))<0;
}
namespace {unsigned preferred_width{},preferred_height{},native_refresh{120};}
#ifdef OR2_PS5_PAYLOAD
extern "C" int wsi_videoout_set_flip_rate(int rate);   // PS5 RADV VideoOut WSI
#endif
void Context::prefer_scanout(unsigned width,unsigned height){preferred_width=width;preferred_height=height;}
unsigned Context::display_refresh(){return native_refresh;}
void Context::pace_frames(unsigned frames_per_second){
#ifdef OR2_PS5_PAYLOAD
    if(native_refresh>=120u){const int result=wsi_videoout_set_flip_rate(frames_per_second>=120u?0:1);
        std::fprintf(stdout,"[vk-display] %u fps on %u Hz (flip rate %d)\n",frames_per_second,native_refresh,result);}
#else
    (void)frames_per_second;
#endif
}
VkDisplayModePropertiesKHR select_display_mode(const std::vector<VkDisplayModePropertiesKHR>& modes,unsigned width,unsigned height,unsigned refresh_mhz){
    const VkDisplayModePropertiesKHR* best=nullptr;
    auto fits=[&](const auto& m){return m.parameters.visibleRegion.width>=width&&m.parameters.visibleRegion.height>=height;};
    auto area=[](const auto& m){return std::uint64_t(m.parameters.visibleRegion.width)*m.parameters.visibleRegion.height;};
    auto refresh=[&](const auto& m){return std::abs(std::int64_t(m.parameters.refreshRate)-std::int64_t(refresh_mhz));};
    for(const auto& mode:modes){
        if(!mode.parameters.visibleRegion.width||!mode.parameters.visibleRegion.height)continue;
        if(!best||
           (fits(mode)!=fits(*best)?fits(mode):
            area(mode)!=area(*best)?(fits(mode)?area(mode)<area(*best):area(mode)>area(*best)):
            refresh(mode)<refresh(*best)))best=&mode;
    }
    if(!best)throw std::runtime_error("No valid native display mode");
    return *best;
}
Context::Context(unsigned width,unsigned height,bool native_display):native_(native_display){
    try{
        load_global();
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="OutRun2006 PS5";app.apiVersion=VK_API_VERSION_1_1;
        const char* extensions[]{VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_DISPLAY_EXTENSION_NAME};
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;
        if(native_){info.enabledExtensionCount=2;info.ppEnabledExtensionNames=extensions;}
        require(vkCreateInstance(&info,nullptr,&instance_),"Create Vulkan instance");load_instance(instance_,native_);
        unsigned count=0;require(vkEnumeratePhysicalDevices(instance_,&count,nullptr),"Enumerate Vulkan devices");
        if(!count)throw std::runtime_error("No Vulkan physical device");
        std::vector<VkPhysicalDevice> devices(count);require(vkEnumeratePhysicalDevices(instance_,&count,devices.data()),"Get Vulkan devices");
        bool found=false;
        for(auto candidate:devices){
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,families.data());
            for(unsigned i=0;i<count;++i)if(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){
                physical_=candidate;family_=i;found=true;break;
            }
            if(found)break;
        }
        if(!found)throw std::runtime_error("No graphics queue");
        vkGetPhysicalDeviceProperties(physical_,&properties_);vkGetPhysicalDeviceMemoryProperties(physical_,&memory_);
        vkGetPhysicalDeviceFeatures(physical_,&features_);
        const auto samples=properties_.limits.framebufferColorSampleCounts&properties_.limits.framebufferDepthSampleCounts;
        max_samples_=(samples&VK_SAMPLE_COUNT_4_BIT)?4:(samples&VK_SAMPLE_COUNT_2_BIT)?2:1;
        VkFormatProperties format{};vkGetPhysicalDeviceFormatProperties(physical_,depth_format_,&format);
        if(!(format.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))depth_format_=VK_FORMAT_D32_SFLOAT_S8_UINT;
        vkGetPhysicalDeviceFormatProperties(physical_,depth_format_,&format);
        if(!(format.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))throw std::runtime_error("No Vulkan depth/stencil format");
        const float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex=family_;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{};enabled.fillModeNonSolid=features_.fillModeNonSolid;
        enabled.samplerAnisotropy=features_.samplerAnisotropy;enabled.occlusionQueryPrecise=features_.occlusionQueryPrecise;
        require(vkEnumerateDeviceExtensionProperties(physical_,nullptr,&count,nullptr),"Enumerate device extensions");
        std::vector<VkExtensionProperties> advertised(count);
        require(vkEnumerateDeviceExtensionProperties(physical_,nullptr,&count,advertised.data()),"Get device extensions");
        auto extension=[&](const char* name){return std::any_of(advertised.begin(),advertised.end(),[&](const auto& e){return std::strcmp(e.extensionName,name)==0;});};
        std::vector<const char*> device_extensions;
        if(native_)device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        mirror_clamp_=extension(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
        if(mirror_clamp_)device_extensions.push_back(VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);
        VkPhysicalDeviceCustomBorderColorFeaturesEXT borders{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT};
        if(extension(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME)){
            VkPhysicalDeviceFeatures2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};queried.pNext=&borders;
            vkGetPhysicalDeviceFeatures2(physical_,&queried);custom_border_=borders.customBorderColors!=0;
            if(custom_border_)device_extensions.push_back(VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
        }
        VkDeviceCreateInfo dev{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dev.queueCreateInfoCount=1;dev.pQueueCreateInfos=&queue;dev.pEnabledFeatures=&enabled;
        dev.enabledExtensionCount=unsigned(device_extensions.size());dev.ppEnabledExtensionNames=device_extensions.data();
        if(custom_border_)dev.pNext=&borders;
        require(vkCreateDevice(physical_,&dev,nullptr,&device_),"Create Vulkan device");load_device(device_,native_);
        vkGetDeviceQueue(device_,family_,0,&queue_);
        for(auto& f:frames_){
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=family_;
            require(vkCreateCommandPool(device_,&pool,nullptr,&f.pool),"Create command pool");
            VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commands.commandPool=f.pool;
            commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=1;
            require(vkAllocateCommandBuffers(device_,&commands,&f.command),"Allocate command buffer");
            VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};require(vkCreateFence(device_,&fence,nullptr,&f.fence),"Create frame fence");
            VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if(native_)require(vkCreateSemaphore(device_,&semaphore,nullptr,&f.acquired),"Create acquire semaphore");
        }
        std::array<VkDescriptorSetLayoutBinding,17> bindings{};
        for(unsigned i=0;i<bindings.size();++i){bindings[i].binding=i;bindings[i].descriptorCount=1;
            bindings[i].descriptorType=i<3||i>=15?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].stageFlags=i>=15?VK_SHADER_STAGE_VERTEX_BIT:VK_SHADER_STAGE_FRAGMENT_BIT;}
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=unsigned(bindings.size());layout.pBindings=bindings.data();
        require(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&descriptor_layout_),"Create descriptor layout");
        VkPipelineLayoutCreateInfo pipeline{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pipeline.setLayoutCount=1;pipeline.pSetLayouts=&descriptor_layout_;
        require(vkCreatePipelineLayout(device_,&pipeline,nullptr,&pipeline_layout_),"Create pipeline layout");
        auto shader=[&](unsigned i,const auto& words){VkShaderModuleCreateInfo s{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};s.codeSize=sizeof(words);s.pCode=words;
            require(vkCreateShaderModule(device_,&s,nullptr,&shaders_[i]),"Create SPIR-V module");};
        shader(0,shaders::pc_vert);shader(1,shaders::ffp_frag);shader(2,shaders::ps11_frag);shader(3,shaders::ps14_frag);
        shader(4,shaders::display_vert);shader(5,shaders::display_frag);shader(6,shaders::full_vert);shader(7,shaders::fxaa_frag);
        shader(8,shaders::native_vert);
        for(const auto& prepared:shaders::prepared_shaders){VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize=prepared.bytes;info.pCode=prepared.words;VkShaderModule module{};
            require(vkCreateShaderModule(device_,&info,nullptr,&module),"Create offline prepared shader");prepared_shaders_[{prepared.kind,prepared.key}]=module;}
        if(!prepared_shaders_.empty())std::fprintf(stdout,"[vk-shaders] %zu offline prepared programs\n",prepared_shaders_.size());
        shader_hash_=hash_bytes(shaders::pc_vert,sizeof shaders::pc_vert);
        for(const auto bytes:{std::make_pair(shaders::ffp_frag,sizeof shaders::ffp_frag),std::make_pair(shaders::ps11_frag,sizeof shaders::ps11_frag),
            std::make_pair(shaders::ps14_frag,sizeof shaders::ps14_frag),std::make_pair(shaders::display_vert,sizeof shaders::display_vert),
            std::make_pair(shaders::display_frag,sizeof shaders::display_frag),std::make_pair(shaders::full_vert,sizeof shaders::full_vert),
            std::make_pair(shaders::fxaa_frag,sizeof shaders::fxaa_frag),std::make_pair(shaders::native_vert,sizeof shaders::native_vert)})
            shader_hash_=hash_bytes(bytes.first,bytes.second,shader_hash_);
        if(const auto* path=std::getenv("OR2_PS5_VK_CACHE"))cache_path_=path;
#ifdef OR2_PS5_PAYLOAD
        else cache_path_="/download0/OutRunPS5/vulkan-pipelines.cache";
#endif
        load_pipeline_cache();
        const auto warm_begin=std::chrono::steady_clock::now();
        for(const auto& state:cached_states_){this->pipeline(state);++warmed_pipelines_;}
        cached_states_.clear();
        if(warmed_pipelines_)std::fprintf(stdout,"[vk-cache] warmed %u pipelines in %.3f ms\n",warmed_pipelines_,
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-warm_begin).count());
        cache_dirty_=false;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_,&count,families.data());
        for(auto& f:frames_){
            VkQueryPoolCreateInfo q{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};q.queryType=VK_QUERY_TYPE_OCCLUSION;q.queryCount=QueryCapacity;
            require(vkCreateQueryPool(device_,&q,nullptr,&f.occlusion),"Create occlusion pool");
            if(families[family_].timestampValidBits){q.queryType=VK_QUERY_TYPE_TIMESTAMP;q.queryCount=2;
                require(vkCreateQueryPool(device_,&q,nullptr,&f.timestamps),"Create timestamps");}
        }
        if(native_)setup_display(width,height);
        else{display_width_=width;display_height_=height;host_display_=image(width,height);}
        std::fprintf(stdout,"PS5 Vulkan GPU: %s, API %u.%u.%u, display %ux%u\n",properties_.deviceName,
            VK_API_VERSION_MAJOR(properties_.apiVersion),VK_API_VERSION_MINOR(properties_.apiVersion),VK_API_VERSION_PATCH(properties_.apiVersion),display_width_,display_height_);
    }catch(...){destroy();throw;}
}
Context::~Context(){destroy();}
void Context::destroy(){
    if(device_){vkDeviceWaitIdle(device_);
        try{save_pipeline_cache();}catch(...){}
        for(auto& f:frames_){
            for(auto framebuffer:f.framebuffers)vkDestroyFramebuffer(device_,framebuffer,nullptr);f.framebuffers.clear();
            f.retained.clear();f.retained_buffers.clear();f.buffers.clear();
        }
        active_={};host_display_.reset();swap_images_.clear();
        for(auto& [_,p]:pipelines_)vkDestroyPipeline(device_,p,nullptr);
        if(pipeline_cache_)vkDestroyPipelineCache(device_,pipeline_cache_,nullptr);
        for(auto& [_,p]:passes_)vkDestroyRenderPass(device_,p,nullptr);
        for(auto& [_,s]:samplers_)vkDestroySampler(device_,s,nullptr);
        for(auto s:shaders_)if(s)vkDestroyShaderModule(device_,s,nullptr);
        for(const auto& entry:prepared_shaders_)vkDestroyShaderModule(device_,entry.second,nullptr);
        for(const auto& entry:linked_shaders_)vkDestroyShaderModule(device_,entry.second,nullptr);
        for(auto& f:frames_){
            for(auto p:f.pools)vkDestroyDescriptorPool(device_,p,nullptr);
            if(f.timestamps)vkDestroyQueryPool(device_,f.timestamps,nullptr);if(f.occlusion)vkDestroyQueryPool(device_,f.occlusion,nullptr);
        }
        if(pipeline_layout_)vkDestroyPipelineLayout(device_,pipeline_layout_,nullptr);
        if(descriptor_layout_)vkDestroyDescriptorSetLayout(device_,descriptor_layout_,nullptr);
        for(auto& f:frames_){
            if(f.fence)vkDestroyFence(device_,f.fence,nullptr);if(f.pool)vkDestroyCommandPool(device_,f.pool,nullptr);
            if(f.acquired)vkDestroySemaphore(device_,f.acquired,nullptr);
        }
        for(auto s:rendered_)vkDestroySemaphore(device_,s,nullptr);rendered_.clear();
        if(swapchain_)vkDestroySwapchainKHR(device_,swapchain_,nullptr);vkDestroyDevice(device_,nullptr);device_=VK_NULL_HANDLE;
    }
    if(surface_)vkDestroySurfaceKHR(instance_,surface_,nullptr);if(instance_)vkDestroyInstance(instance_,nullptr);
}
void Context::load_pipeline_cache(){
    std::vector<std::uint8_t> data;
    if(!cache_path_.empty())if(auto* file=std::fopen(cache_path_.c_str(),"rb")){
        CacheHeader header{};
        bool valid=std::fread(&header,sizeof header,1,file)==1&&std::memcmp(header.magic,"OR2VKPC2",8)==0&&header.version==2&&
            header.key_size==sizeof(PipelineState)&&header.keys<=MaxCachedStates&&header.bytes>=32&&header.bytes<=MaxCacheBytes&&header.shaders==shader_hash_;
        if(valid){cached_states_.resize(header.keys);data.resize(header.bytes);
            valid=std::fread(cached_states_.data(),sizeof(PipelineState),header.keys,file)==header.keys&&
                std::fread(data.data(),1,data.size(),file)==data.size()&&std::fgetc(file)==EOF;
            if(valid){std::uint32_t vk_header[4]{};std::memcpy(vk_header,data.data(),16);
                valid=vk_header[0]>=32&&vk_header[0]<=data.size()&&vk_header[1]==VK_PIPELINE_CACHE_HEADER_VERSION_ONE&&
                    vk_header[2]==properties_.vendorID&&vk_header[3]==properties_.deviceID&&
                    std::memcmp(data.data()+16,properties_.pipelineCacheUUID,VK_UUID_SIZE)==0&&
                    hash_bytes(data.data(),data.size(),hash_bytes(cached_states_.data(),cached_states_.size()*sizeof(PipelineState)))==header.checksum;}
            for(const auto& key:cached_states_)if(key.layout>3||key.shader>2||(key.samples!=1&&key.samples!=2&&key.samples!=4)||
                key.samples>max_samples_||key.depth>1||(key.format!=VK_FORMAT_R8G8B8A8_UNORM&&key.format!=VK_FORMAT_B8G8R8A8_UNORM))valid=false;
        }
        std::fclose(file);
        if(!valid){data.clear();cached_states_.clear();std::fprintf(stdout,"[vk-cache] ignored incompatible/corrupt cache\n");}
        else std::fprintf(stdout,"[vk-cache] loaded %zu bytes, %zu pipeline keys\n",data.size(),cached_states_.size());
    }
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};info.initialDataSize=data.size();info.pInitialData=data.data();
    auto result=vkCreatePipelineCache(device_,&info,nullptr,&pipeline_cache_);
    if(result!=VK_SUCCESS&&!data.empty()){cached_states_.clear();info.initialDataSize=0;info.pInitialData=nullptr;result=vkCreatePipelineCache(device_,&info,nullptr,&pipeline_cache_);}
    require(result,"Create Vulkan pipeline cache");
}
void Context::save_pipeline_cache(){
    if(!pipeline_cache_||!cache_dirty_||cache_path_.empty()||pipelines_.size()>MaxCachedStates)return;
    const auto begin=std::chrono::steady_clock::now();std::size_t size=0;
    if(vkGetPipelineCacheData(device_,pipeline_cache_,&size,nullptr)!=VK_SUCCESS||size<32||size>MaxCacheBytes)return;
    std::vector<std::uint8_t> data(size);
    if(vkGetPipelineCacheData(device_,pipeline_cache_,&size,data.data())!=VK_SUCCESS)return;data.resize(size);
    std::vector<PipelineState> keys;keys.reserve(pipelines_.size());for(const auto& entry:pipelines_)keys.push_back(entry.first);
    CacheHeader header{};std::memcpy(header.magic,"OR2VKPC2",8);header.version=2;header.key_size=sizeof(PipelineState);
    header.keys=unsigned(keys.size());header.bytes=unsigned(size);header.shaders=shader_hash_;
    header.checksum=hash_bytes(data.data(),data.size(),hash_bytes(keys.data(),keys.size()*sizeof(PipelineState)));
    const auto temporary=cache_path_+".tmp";auto* file=std::fopen(temporary.c_str(),"wb");if(!file)return;
    bool ok=std::fwrite(&header,sizeof header,1,file)==1&&std::fwrite(keys.data(),sizeof(PipelineState),keys.size(),file)==keys.size()&&
        std::fwrite(data.data(),1,data.size(),file)==data.size();
    if(std::fclose(file)!=0)ok=false;
    if(ok&&std::rename(temporary.c_str(),cache_path_.c_str())==0){cache_dirty_=false;
        std::fprintf(stdout,"[vk-cache] saved %zu bytes, %zu keys in %.3f ms\n",size,keys.size(),
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
    else std::remove(temporary.c_str());
}
unsigned Context::memory_type(unsigned bits,VkMemoryPropertyFlags flags){
    for(unsigned i=0;i<memory_.memoryTypeCount;++i)if((bits&(1u<<i))&&(memory_.memoryTypes[i].propertyFlags&flags)==flags)return i;
    throw std::runtime_error("No compatible Vulkan memory type");
}
std::unique_ptr<Buffer> Context::buffer(VkDeviceSize size,VkBufferUsageFlags usage){
    auto b=std::make_unique<Buffer>();b->device=device_;b->size=size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=size;info.usage=usage;
    require(vkCreateBuffer(device_,&info,nullptr,&b->buffer),"Create buffer");VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_,b->buffer,&requirements);VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memory.allocationSize=requirements.size;memory.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    require(vkAllocateMemory(device_,&memory,nullptr,&b->memory),"Allocate upload memory");require(vkBindBufferMemory(device_,b->buffer,b->memory,0),"Bind buffer");
    require(vkMapMemory(device_,b->memory,0,VK_WHOLE_SIZE,0,&b->mapped),"Map buffer");return b;
}
Texture Context::image(unsigned w,unsigned h,unsigned levels,unsigned layers,VkFormat format,unsigned samples){
    if(!w||!h||w>properties_.limits.maxImageDimension2D||h>properties_.limits.maxImageDimension2D||!levels||(layers!=1&&layers!=6))throw std::runtime_error("Invalid Vulkan image size");
    auto t=std::make_shared<Image>();t->device=device_;t->width=w;t->height=h;t->levels=levels;t->layers=layers;t->format=format;t->samples=VkSampleCountFlagBits(samples);
    const bool depth=format==VK_FORMAT_D24_UNORM_S8_UINT||format==VK_FORMAT_D32_SFLOAT_S8_UINT;
    if(depth)t->aspect=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={w,h,1};
    info.mipLevels=levels;info.arrayLayers=layers;info.samples=t->samples;info.tiling=VK_IMAGE_TILING_OPTIMAL;
    info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|(depth?VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT:VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    if(layers==6)info.flags=VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    require(vkCreateImage(device_,&info,nullptr,&t->image),"Create image");VkMemoryRequirements requirements{};vkGetImageMemoryRequirements(device_,t->image,&requirements);
    VkMemoryAllocateInfo memory{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};memory.allocationSize=requirements.size;memory.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    require(vkAllocateMemory(device_,&memory,nullptr,&t->memory),"Allocate image memory");require(vkBindImageMemory(device_,t->image,t->memory,0),"Bind image");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=t->image;view.viewType=layers==6?VK_IMAGE_VIEW_TYPE_CUBE:VK_IMAGE_VIEW_TYPE_2D;
    view.format=format;view.subresourceRange={depth?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,levels,0,layers};
    require(vkCreateImageView(device_,&view,nullptr,&t->view),"Create image view");return t;
}
void Context::start(){
    if(recording_)return;
    poll();
    auto& f=frames_[frame_];
    if(f.submitted){
        // The slot's previous recording (FramesInFlight submissions ago) must
        // be complete before its uploads, descriptors and images are reused.
#ifdef OR2_PS5_PAYLOAD
        const runtime::ProfileScope wait(runtime::profile_key(runtime::ProfileRenderer,0,runtime::ProfPcFence));
#endif
        require(vkWaitForFences(device_,1,&f.fence,VK_TRUE,std::numeric_limits<std::uint64_t>::max()),"Wait Vulkan frame");
        retire(f);
    }
    for(auto framebuffer:f.framebuffers)vkDestroyFramebuffer(device_,framebuffer,nullptr);f.framebuffers.clear();active_={};f.retained.clear();f.retained_buffers.clear();
    for(auto& b:f.buffers)b->used=0;for(auto p:f.pools)require(vkResetDescriptorPool(device_,p,0),"Reset descriptors");
    f.pool_index=f.descriptor_count=f.queries=0;
    f.epoch=++recording_epoch_;last_uniforms_={};empty_uniform_={};last_descriptor_set_={};descriptor_sets_.clear();bindings_valid_=false;
    require(vkResetCommandPool(device_,f.pool,0),"Reset commands");
    command_=f.command;
    VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    require(vkBeginCommandBuffer(command_,&info),"Begin commands");recording_=true;
    if(f.timestamps){vkCmdResetQueryPool(command_,f.timestamps,0,2);vkCmdWriteTimestamp(command_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,f.timestamps,0);}
}
void Context::retire(Frame& f){
    if(!f.submitted)return;
    f.submitted=false;
    if(f.timestamps){
        std::uint64_t times[2]{};
        if(vkGetQueryPoolResults(device_,f.timestamps,0,2,sizeof(times),times,8,VK_QUERY_RESULT_64_BIT)==VK_SUCCESS)
            gpu_ns_=std::uint64_t(double(times[1]-times[0])*properties_.limits.timestampPeriod);
    }
    std::vector<std::uint64_t> samples(f.queries);
    if(f.queries){require(vkGetQueryPoolResults(device_,f.occlusion,0,f.queries,samples.size()*8,samples.data(),8,VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"Read occlusion samples");
        for(auto n:samples)samples_passed_+=n;}
    retired_epoch_=std::max(retired_epoch_,f.epoch);
    for(auto& [_,listener]:listeners_)listener(f.epoch,samples);
}
void Context::poll(){
    // Retire in submission order so listeners see epochs ascending.
    // frame_ is the oldest slot: the next to record, after the newer ones.
    for(unsigned k=0;k<FramesInFlight;++k){auto& f=frames_[(frame_+k)%FramesInFlight];
        if(!f.submitted)continue;
        const auto status=vkGetFenceStatus(device_,f.fence);
        if(status==VK_NOT_READY)break;
        require(status,"Poll Vulkan frame");retire(f);
    }
}
unsigned Context::add_retire_listener(RetireListener listener){listeners_.emplace_back(next_listener_,std::move(listener));return next_listener_++;}
void Context::remove_retire_listener(unsigned id){
    listeners_.erase(std::remove_if(listeners_.begin(),listeners_.end(),[&](const auto& l){return l.first==id;}),listeners_.end());
}
// Raw users (compositor/FXAA/tests) may alter bindings without going through
// bind_draw. Invalidate before returning the command buffer to those callers.
VkCommandBuffer Context::commands(){start();bindings_valid_=false;return command_;}
VkCommandBuffer Context::bind_draw(const DrawBindings& next){
    start();const auto& previous=last_bindings_;const bool fresh=!bindings_valid_;
    auto changed=[&](bool change){if(change)++state_commands;else ++state_commands_saved;return change;};
    if(changed(fresh||next.pipeline!=previous.pipeline))vkCmdBindPipeline(command_,VK_PIPELINE_BIND_POINT_GRAPHICS,next.pipeline);
    if(changed(fresh||std::memcmp(&next.viewport,&previous.viewport,sizeof(VkViewport))!=0))vkCmdSetViewport(command_,0,1,&next.viewport);
    if(changed(fresh||std::memcmp(&next.scissor,&previous.scissor,sizeof(VkRect2D))!=0))vkCmdSetScissor(command_,0,1,&next.scissor);
    if(changed(fresh||next.depth_bias!=previous.depth_bias||next.slope_bias!=previous.slope_bias))vkCmdSetDepthBias(command_,next.depth_bias,0,next.slope_bias);
    if(changed(fresh||next.stencil!=previous.stencil))vkCmdSetStencilReference(command_,VK_STENCIL_FACE_FRONT_AND_BACK,next.stencil);
    if(changed(fresh||next.blend!=previous.blend))vkCmdSetBlendConstants(command_,next.blend.data());
    if(changed(fresh||next.descriptors!=previous.descriptors||next.uniforms!=previous.uniforms))vkCmdBindDescriptorSets(command_,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout_,0,1,&next.descriptors,unsigned(next.uniforms.size()),next.uniforms.data());
    bool vertices=fresh||next.vertex_count!=previous.vertex_count;
    for(unsigned i=0;i<next.vertex_count&&!vertices;++i)vertices=next.vertices[i]!=previous.vertices[i]||next.offsets[i]!=previous.offsets[i];
    if(changed(vertices))vkCmdBindVertexBuffers(command_,0,next.vertex_count,next.vertices.data(),next.offsets.data());
    if(next.index&&changed(fresh||next.index!=previous.index))vkCmdBindIndexBuffer(command_,next.index,0,VK_INDEX_TYPE_UINT16);
    last_bindings_=next;bindings_valid_=true;return command_;
}
void Context::end_pass(){if(pass_live_){if(query_live_)throw std::runtime_error("End render pass inside occlusion query");vkCmdEndRenderPass(command_);pass_live_=false;}}
void Context::transition(const Texture& t,VkImageLayout layout){
    if(!t)return;start();
    if(t->retained_by!=this||t->retained_epoch!=recording_epoch_){
        frames_[frame_].retained.push_back(t);t->retained_by=this;t->retained_epoch=recording_epoch_;
    }
    if(t->layout==layout)return;
    end_pass();VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout=t->layout;barrier.newLayout=layout;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=t->image;barrier.subresourceRange={t->aspect,0,t->levels,0,t->layers};
    const bool undefined=t->layout==VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.srcAccessMask=undefined?0:VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(command_,undefined?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&barrier);t->layout=layout;
}
Slice Context::allocate(const void* data,std::size_t size,VkDeviceSize alignment){
    start();alignment=std::max(alignment,properties_.limits.minUniformBufferOffsetAlignment);
    auto& buffers=frames_[frame_].buffers;
    for(auto& b:buffers){const auto offset=(b->used+alignment-1)/alignment*alignment;
        if(offset+size<=b->size){b->used=offset+size;auto* mapped=static_cast<unsigned char*>(b->mapped)+offset;
            if(data)std::memcpy(mapped,data,size);return {b->buffer,offset,size,mapped};}}
    buffers.push_back(buffer(std::max<VkDeviceSize>(4*1024*1024,size+alignment),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    return allocate(data,size,alignment);
}
void Context::upload(const Texture& t,unsigned face,unsigned level,const void* data,std::size_t size){
    const unsigned w=std::max(1u,t->width>>level),h=std::max(1u,t->height>>level);
    if(level>=t->levels||face>=t->layers||size!=std::size_t(w)*h*4)throw std::runtime_error("Vulkan texture upload size mismatch");
    const auto slice=allocate(data,size);transition(t,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);end_pass();
    VkBufferImageCopy copy{};copy.bufferOffset=slice.offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,face,1};copy.imageExtent={w,h,1};
    vkCmdCopyBufferToImage(command_,slice.buffer,t->image,t->layout,1,&copy);
    // Make repeated uploads to one subresource ordered even without a layout change.
    transition(t,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
Slice Context::uniform(unsigned slot,const void* data,std::size_t size){
    start();auto& previous=last_uniforms_.at(slot);auto& bytes=last_uniform_bytes_[slot];
    if(previous.buffer&&previous.size==size&&std::memcmp(bytes.data(),data,size)==0)return previous;
    bytes.assign(static_cast<const std::uint8_t*>(data),static_cast<const std::uint8_t*>(data)+size);
    return previous=allocate(data,size);
}
VkRenderPass Context::render_pass(VkFormat format,unsigned samples,bool depth){
    const std::array<unsigned,3> key{unsigned(format),samples,unsigned(depth)};if(auto it=passes_.find(key);it!=passes_.end())return it->second;
    VkAttachmentDescription attachments[2]{};attachments[0].format=format;attachments[0].samples=VkSampleCountFlagBits(samples);
    attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout=attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].format=depth_format_;attachments[1].samples=VkSampleCountFlagBits(samples);
    attachments[1].loadOp=attachments[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp=attachments[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout=attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colour{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},z{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&colour;if(depth)subpass.pDepthStencilAttachment=&z;
    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;dependencies[0].dstSubpass=0;dependencies[1].srcSubpass=0;dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    for(auto& d:dependencies){d.srcStageMask=d.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;d.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;d.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;}
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};info.attachmentCount=depth?2:1;info.pAttachments=attachments;info.subpassCount=1;info.pSubpasses=&subpass;info.dependencyCount=2;info.pDependencies=dependencies;
    VkRenderPass result{};require(vkCreateRenderPass(device_,&info,nullptr,&result),"Create render pass");passes_[key]=result;return result;
}
void Context::target(const Target& target){
    if(!target.colour)throw std::runtime_error("Missing Vulkan colour target");start();
    if(pass_live_&&target.colour==active_.colour&&target.depth==active_.depth&&target.face==active_.face&&target.level==active_.level)return;
    end_pass();transition(target.colour,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);transition(target.depth,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    const auto w=std::max(1u,target.colour->width>>target.level),h=std::max(1u,target.colour->height>>target.level);
    if(target.depth&&(target.depth->width!=w||target.depth->height!=h||target.depth->samples!=target.colour->samples))throw std::runtime_error("Vulkan depth/colour target mismatch");
    active_=target;active_pass_=render_pass(target.colour->format,unsigned(target.colour->samples),bool(target.depth));
    VkImageView views[]{target.colour->attachment(target.face,target.level),target.depth?target.depth->attachment():VK_NULL_HANDLE};
    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};framebuffer.renderPass=active_pass_;framebuffer.attachmentCount=target.depth?2:1;framebuffer.pAttachments=views;framebuffer.width=w;framebuffer.height=h;framebuffer.layers=1;
    VkFramebuffer result{};require(vkCreateFramebuffer(device_,&framebuffer,nullptr,&result),"Create framebuffer");frames_[frame_].framebuffers.push_back(result);
    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};info.renderPass=active_pass_;info.framebuffer=result;info.renderArea.extent={w,h};
    vkCmdBeginRenderPass(command_,&info,VK_SUBPASS_CONTENTS_INLINE);pass_live_=true;
}
void Context::clear(const Target& target,unsigned flags,std::uint32_t argb,float depth,unsigned stencil_value){
    this->target(target);VkClearAttachment a[2]{};unsigned count=0;
    if(flags&1){a[count].aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;auto& c=a[count++].clearValue.color.float32;
        c[0]=float((argb>>16)&255)/255;c[1]=float((argb>>8)&255)/255;c[2]=float(argb&255)/255;c[3]=float(argb>>24)/255;}
    if(target.depth&&(flags&6)){a[count].aspectMask=(flags&2?VK_IMAGE_ASPECT_DEPTH_BIT:0)|(flags&4?VK_IMAGE_ASPECT_STENCIL_BIT:0);a[count++].clearValue.depthStencil={depth,stencil_value&255};}
    VkClearRect rect{};rect.rect.extent={std::max(1u,target.colour->width>>target.level),std::max(1u,target.colour->height>>target.level)};rect.layerCount=1;
    if(count)vkCmdClearAttachments(command_,count,a,1,&rect);
}
std::shared_ptr<Buffer> Context::geometry(const void* data,std::size_t size){
#ifdef OR2_PS5_PAYLOAD
    static unsigned uploads=0;const bool trace=++uploads<=4;
    if(trace)std::fprintf(stdout,"[vk-geometry] %u allocate %zu bytes\n",uploads,size);
#endif
    auto result=std::shared_ptr<Buffer>(buffer(size,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
    std::memcpy(result->mapped,data,size);
#ifdef OR2_PS5_PAYLOAD
    if(trace)std::fprintf(stdout,"[vk-geometry] %u ready\n",uploads);
#endif
    return result;
}
void Context::retain(const std::shared_ptr<Buffer>& buffer){
    start();if(buffer->retained_by==this&&buffer->retained_epoch==recording_epoch_)return;
    frames_[frame_].retained_buffers.push_back(buffer);buffer->retained_by=this;buffer->retained_epoch=recording_epoch_;
}
VkDescriptorPool Context::descriptor_pool(){
    auto& f=frames_[frame_];
    if(f.descriptor_count==SetsPerPool){++f.pool_index;f.descriptor_count=0;}
    if(f.pool_index==f.pools.size()){
        const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,SetsPerPool*5},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,SetsPerPool*12}};
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};info.maxSets=SetsPerPool;info.poolSizeCount=2;info.pPoolSizes=sizes;VkDescriptorPool pool{};
        require(vkCreateDescriptorPool(device_,&info,nullptr,&pool),"Create descriptor pool");f.pools.push_back(pool);
    }
    ++f.descriptor_count;return f.pools[f.pool_index];
}
std::array<std::uint32_t,5> Context::uniform_offsets(const std::array<Slice,3>& uniforms,const std::array<Slice,2>& vertex_uniforms){
    std::array<std::uint32_t,5> offsets{};
    for(unsigned i=0;i<5;++i){const auto& slice=i<3?uniforms[i]:vertex_uniforms[i-3];
        if(slice.offset>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Vulkan dynamic uniform offset exceeds 32 bits");
        offsets[i]=std::uint32_t(slice.offset);}
    return offsets;
}
VkDescriptorSet Context::descriptors(const std::array<Slice,3>& uniforms,const std::array<Texture,12>& textures,const std::array<VkSampler,12>& samplers,const std::array<Slice,2>& vertex_uniforms){
    start();for(auto& t:textures)if(t)transition(t,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // Reuse only within this recording epoch; pool reset invalidates all sets.
    // Image transitions still run before a hit, including sampled render targets.
    std::array<std::uint64_t,39> key{};unsigned at=0;
    auto handle=[](auto h){std::uint64_t value=0;static_assert(sizeof(h)<=sizeof(value));std::memcpy(&value,&h,sizeof(h));return value;};
    // Offsets are dynamic: changed matrices/constants do not require another
    // descriptor allocation/update. Buffer identities and ranges still do.
    for(const auto& u:uniforms){key[at++]=handle(u.buffer);key[at++]=0;key[at++]=u.size;}
    for(const auto& u:vertex_uniforms){key[at++]=handle(u.buffer);key[at++]=0;key[at++]=u.size;}
    for(unsigned i=0;i<12;++i){key[at++]=textures[i]?handle(textures[i]->view):0;key[at++]=textures[i]?handle(samplers[i]):0;}
    if(last_descriptor_set_&&key==last_descriptors_){++descriptor_sets_reused;return last_descriptor_set_;}
    // Real race draws interleave materials (A, B, A), so a last-set-only
    // cache misses most reusable sets. Keep immutable sets for the entire
    // recording, retaining sampled images and their transitions above.
    if(auto it=descriptor_sets_.find(key);it!=descriptor_sets_.end()){
        ++descriptor_sets_reused;last_descriptors_=key;return last_descriptor_set_=it->second;
    }
    VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};info.descriptorPool=descriptor_pool();info.descriptorSetCount=1;info.pSetLayouts=&descriptor_layout_;
    VkDescriptorSet result{};require(vkAllocateDescriptorSets(device_,&info,&result),"Allocate draw descriptors");++descriptor_sets_allocated;
    VkDescriptorBufferInfo buffers[5]{};VkDescriptorImageInfo images[12]{};VkWriteDescriptorSet writes[17]{};unsigned count=0;
    if(!empty_uniform_.buffer){const float zero[4]{};empty_uniform_=allocate(zero,sizeof zero);}
    // All five dynamic bindings are initialized, including those absent from
    // this shader. Their caller-supplied dynamic offsets remain zero.
    for(unsigned i=0;i<5;++i){const auto& u=i<3?uniforms[i]:vertex_uniforms[i-3];
        buffers[i]=u.buffer?VkDescriptorBufferInfo{u.buffer,0,u.size}:VkDescriptorBufferInfo{empty_uniform_.buffer,empty_uniform_.offset,empty_uniform_.size};
        auto& w=writes[count++];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=result;w.dstBinding=i<3?i:i+12;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;w.pBufferInfo=&buffers[i];}
    for(unsigned i=0;i<12;++i)if(textures[i]){images[i]={samplers[i],textures[i]->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};auto& w=writes[count++];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=result;w.dstBinding=i+3;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.pImageInfo=&images[i];}
    vkUpdateDescriptorSets(device_,count,writes,0,nullptr);
    // Bound cache memory even for an unusually large submission. Existing
    // entries remain valid and uncached sets still belong to the same pools.
    if(descriptor_sets_.size()<4096)descriptor_sets_.emplace(key,result);
    last_descriptors_=key;last_descriptor_set_=result;return result;
}
VkSampler Context::sampler(const std::array<std::uint32_t,14>& state,unsigned levels){
    std::array<std::uint32_t,15> key{};std::copy(state.begin(),state.end(),key.begin());key[14]=levels;
    if(auto it=samplers_.find(key);it!=samplers_.end())return it->second;
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};info.magFilter=state[SAMP_MAGFILTER]==1?VK_FILTER_NEAREST:VK_FILTER_LINEAR;
    info.minFilter=state[SAMP_MINFILTER]==1?VK_FILTER_NEAREST:VK_FILTER_LINEAR;
    info.mipmapMode=state[SAMP_MIPFILTER]==2?VK_SAMPLER_MIPMAP_MODE_LINEAR:VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU=address(state[1]);info.addressModeV=address(state[2]);info.addressModeW=address(state[3]);
    if(!mirror_clamp_&&(state[1]==5||state[2]==5||state[3]==5))throw std::runtime_error("Vulkan device lacks D3D MIRRORONCE sampling");
    info.mipLodBias=std::clamp(f(state[SAMP_MIPMAPLODBIAS]),-properties_.limits.maxSamplerLodBias,properties_.limits.maxSamplerLodBias);
    info.minLod=float(std::min(state[SAMP_MAXMIPLEVEL],levels-1));info.maxLod=state[SAMP_MIPFILTER]?float(levels-1):info.minLod;
    if(features_.samplerAnisotropy&&state[SAMP_MAXANISOTROPY]>1){info.anisotropyEnable=VK_TRUE;info.maxAnisotropy=std::min(float(state[SAMP_MAXANISOTROPY]),properties_.limits.maxSamplerAnisotropy);}
    info.borderColor=state[SAMP_BORDERCOLOR]==0xffffffff?VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:state[SAMP_BORDERCOLOR]==0xff000000?VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    VkSamplerCustomBorderColorCreateInfoEXT border{VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT};
    if(state[SAMP_ADDRESSU]==4||state[SAMP_ADDRESSV]==4||state[SAMP_ADDRESSW]==4){const auto c=state[SAMP_BORDERCOLOR];if(c!=0&&c!=0xffffffff&&c!=0xff000000){
        if(!custom_border_)throw std::runtime_error("Vulkan device lacks D3D custom sampler border colours");
        info.borderColor=VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;info.pNext=&border;border.format=VK_FORMAT_R8G8B8A8_UNORM;
        border.customBorderColor.float32[0]=float((c>>16)&255)/255;border.customBorderColor.float32[1]=float((c>>8)&255)/255;
        border.customBorderColor.float32[2]=float(c&255)/255;border.customBorderColor.float32[3]=float(c>>24)/255;
    }}
    VkSampler result{};require(vkCreateSampler(device_,&info,nullptr,&result),"Create sampler");samplers_[key]=result;return result;
}
VkShaderModule Context::linked_shader(unsigned kind,const std::array<std::int32_t,33>& key){
    const auto id=std::make_pair(kind,key);
    if(auto it=linked_shaders_.find(id);it!=linked_shaders_.end())return it->second;
    const auto& source=kind==3?shaders::link_native_vert_template:
        kind==1?shaders::link_ps11_frag_template:shaders::link_ps14_frag_template;
    // Pixel dispatch checks each entry independently, just like the recovered
    // shader; count is not an early-termination condition in that code.
    const auto words=link_shader(source,key.data(),kind==3?32:16);
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize=words.size()*sizeof(words[0]);info.pCode=words.data();VkShaderModule module{};
    require(vkCreateShaderModule(device_,&info,nullptr,&module),"Create directly linked shader");
    linked_shaders_[id]=module;return module;
}
VkPipeline Context::pipeline(const PipelineState& requested){
    auto state=requested;
    // D3D keeps the last values of disabled tests/blending. They cannot alter
    // pixels and must not trigger another shader compilation for that draw.
    auto& rkey=state.render;
    if(!state.depth){rkey[RS_ZENABLE]=0;rkey[52]=0;}
    if(!rkey[RS_ZENABLE]){rkey[RS_ZWRITEENABLE]=0;rkey[RS_ZFUNC]=0;}
    if(!rkey[52])for(unsigned i:{53u,54u,55u,56u,58u,59u})rkey[i]=0;
    if(!rkey[RS_ALPHABLENDENABLE])for(unsigned i:{RS_SRCBLEND,RS_DESTBLEND,RS_BLENDOP,RS_SEPARATEALPHABLENDENABLE})rkey[i]=0;
    if(!rkey[RS_SEPARATEALPHABLENDENABLE])for(unsigned i:{RS_SRCBLENDALPHA,RS_DESTBLENDALPHA,RS_BLENDOPALPHA})rkey[i]=0;
    if(auto it=pipelines_.find(state);it!=pipelines_.end())return it->second;
    const auto compile_begin=std::chrono::steady_clock::now();
#ifdef OR2_PS5_PAYLOAD
    static unsigned vertex_pipelines=0;const bool trace=state.layout==3&&++vertex_pipelines<=4;
    if(trace)std::fprintf(stdout,"[vk-pipeline] native %u begin VS=%d,%d,%d,%d PS=%u\n",vertex_pipelines,state.vertex_blobs[0],state.vertex_blobs[1],state.vertex_blobs[2],state.vertex_blobs[3],state.shader);
#endif
    const auto& r=state.render;
    VkPipelineShaderStageCreateInfo stages[2]{};for(auto& stage:stages){stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=shaders_[state.layout==1?4:state.layout==2?6:state.layout==3?8:0];
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=shaders_[state.layout==1?5:state.layout==2?7:1+state.shader];
    std::array<VkSpecializationMapEntry,17> entries{};for(unsigned i=0;i<entries.size();++i)entries[i]={i,i*4,4};
    VkSpecializationInfo specialization{unsigned(entries.size()),entries.data(),sizeof(state.blobs),state.blobs.data()};
    if((state.layout==0||state.layout==3)&&state.shader)stages[1].pSpecializationInfo=&specialization;
    std::array<VkSpecializationMapEntry,33> vertex_entries{};for(unsigned i=0;i<vertex_entries.size();++i)vertex_entries[i]={i,i*4,4};
    VkSpecializationInfo vertex_specialization{unsigned(vertex_entries.size()),vertex_entries.data(),sizeof(state.vertex_blobs),state.vertex_blobs.data()};
    if(state.layout==3)stages[0].pSpecializationInfo=&vertex_specialization;
    bool prepared_vertex=false,prepared_pixel=false;
    bool linked_vertex=false,linked_pixel=false;
    const bool link_only=std::getenv("OR2_PS5_VK_LINK_ONLY")!=nullptr;
    if(state.layout==3){auto it=prepared_shaders_.find({3,state.vertex_blobs});
        if(!link_only&&it!=prepared_shaders_.end()){
            stages[0].module=it->second;stages[0].pSpecializationInfo=nullptr;prepared_vertex=true;++prepared_shader_uses_;
        }else{
            stages[0].module=linked_shader(3,state.vertex_blobs);
            vertex_entries[0]={32,32*4,4};vertex_specialization.mapEntryCount=1;
            stages[0].pSpecializationInfo=&vertex_specialization;linked_vertex=true;++linked_shader_uses_;
        }
    }
    if((state.layout==0||state.layout==3)&&state.shader){std::array<std::int32_t,33> key{};std::copy(state.blobs.begin(),state.blobs.end(),key.begin());
        auto it=prepared_shaders_.find({state.shader,key});
        if(!link_only&&it!=prepared_shaders_.end()){
            stages[1].module=it->second;stages[1].pSpecializationInfo=nullptr;prepared_pixel=true;++prepared_shader_uses_;
        }else{
            stages[1].module=linked_shader(state.shader,key);stages[1].pSpecializationInfo=nullptr;linked_pixel=true;++linked_shader_uses_;
        }
    }
    VkVertexInputBindingDescription binding{0,state.layout==1?32u:180u,VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attributes[12]{};
    for(unsigned i=0;i<12;++i)attributes[i]={i,0,i==11?VK_FORMAT_R32_SFLOAT:VK_FORMAT_R32G32B32A32_SFLOAT,i*16};
    if(state.layout==1){attributes[0]={0,0,VK_FORMAT_R32G32_SFLOAT,0};attributes[1]={1,0,VK_FORMAT_R32G32B32A32_SFLOAT,8};attributes[2]={2,0,VK_FORMAT_R32G32_SFLOAT,24};}
    VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if(state.layout!=2){vertex.vertexBindingDescriptionCount=1;vertex.pVertexBindingDescriptions=&binding;vertex.vertexAttributeDescriptionCount=state.layout==1?3:12;vertex.pVertexAttributeDescriptions=attributes;}
    VkVertexInputBindingDescription native_bindings[5]{};VkVertexInputAttributeDescription native_attributes[16]{};
    if(state.layout==3){
        for(unsigned i=0;i<5;++i)native_bindings[i]={i,state.strides[i],i==4?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX};
        for(unsigned i=0;i<16;++i){const auto& a=state.attributes[i];native_attributes[i]={i,a[0],VkFormat(a[1]),a[2]};}
        vertex.vertexBindingDescriptionCount=5;vertex.pVertexBindingDescriptions=native_bindings;
        vertex.vertexAttributeDescriptionCount=16;vertex.pVertexAttributeDescriptions=native_attributes;
    }
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if(state.layout==3&&state.primitive==5)assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.lineWidth=1;
    raster.frontFace=VK_FRONT_FACE_CLOCKWISE;raster.cullMode=r[RS_CULLMODE]==CULL_NONE?VK_CULL_MODE_NONE:r[RS_CULLMODE]==CULL_CCW?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_FRONT_BIT;
    raster.polygonMode=r[RS_FILLMODE]==2?VK_POLYGON_MODE_LINE:r[RS_FILLMODE]==1?VK_POLYGON_MODE_POINT:VK_POLYGON_MODE_FILL;
    if(raster.polygonMode!=VK_POLYGON_MODE_FILL&&!features_.fillModeNonSolid)throw std::runtime_error("Vulkan device lacks non-solid rasterization");
    raster.depthBiasEnable=VK_TRUE;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};multisample.rasterizationSamples=VkSampleCountFlagBits(state.samples);
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};depth.depthTestEnable=r[RS_ZENABLE]!=0;depth.depthWriteEnable=r[RS_ZWRITEENABLE]!=0;depth.depthCompareOp=comparison(r[RS_ZFUNC]);
    depth.stencilTestEnable=r[52]!=0;depth.front={stencil(r[53]),stencil(r[55]),stencil(r[54]),comparison(r[56]),r[58],r[59],0};depth.back=depth.front;
    VkPipelineColorBlendAttachmentState colour{};colour.colorWriteMask=r[RS_COLORWRITEENABLE]&15;colour.blendEnable=r[RS_ALPHABLENDENABLE]!=0;
    const bool separate=r[RS_SEPARATEALPHABLENDENABLE]!=0;colour.srcColorBlendFactor=blend(r[RS_SRCBLEND]);colour.dstColorBlendFactor=blend(r[RS_DESTBLEND]);
    colour.srcAlphaBlendFactor=blend(r[separate?RS_SRCBLENDALPHA:RS_SRCBLEND]);colour.dstAlphaBlendFactor=blend(r[separate?RS_DESTBLENDALPHA:RS_DESTBLEND]);
    colour.colorBlendOp=VkBlendOp(std::clamp(r[RS_BLENDOP],1u,5u)-1);colour.alphaBlendOp=VkBlendOp(std::clamp(r[separate?RS_BLENDOPALPHA:RS_BLENDOP],1u,5u)-1);
    VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blending.attachmentCount=1;blending.pAttachments=&colour;
    const VkDynamicState dynamics[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS,VK_DYNAMIC_STATE_BLEND_CONSTANTS,VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=std::size(dynamics);dynamic.pDynamicStates=dynamics;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=stages;
    info.pVertexInputState=&vertex;info.pInputAssemblyState=&assembly;info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&multisample;
    info.pDepthStencilState=&depth;info.pColorBlendState=&blending;info.pDynamicState=&dynamic;info.layout=pipeline_layout_;info.renderPass=render_pass(state.format,state.samples,state.depth!=0);
    VkPipeline result{};require(vkCreateGraphicsPipelines(device_,pipeline_cache_,1,&info,nullptr,&result),"Create D3D Vulkan pipeline");pipelines_[state]=result;cache_dirty_=true;
    const auto compile_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-compile_begin).count();
    if(compile_ms>=10)std::fprintf(stdout,"[vk-pipeline] compiled key=%zu layout=%u VS=%d,%d,%d PS=%u prepared=%u/%u linked=%u/%u samples=%u in %.3f ms\n",
        pipelines_.size(),state.layout,state.vertex_blobs[0],state.vertex_blobs[1],state.vertex_blobs[2],state.shader,unsigned(prepared_vertex),unsigned(prepared_pixel),unsigned(linked_vertex),unsigned(linked_pixel),state.samples,compile_ms);
#ifdef OR2_PS5_PAYLOAD
    if(trace)std::fprintf(stdout,"[vk-pipeline] native %u ready\n",vertex_pipelines);
#endif
    return result;
}
void Context::begin_occlusion(){
    start();auto& f=frames_[frame_];
    if(f.queries==QueryCapacity)throw std::runtime_error("Vulkan per-submission draw limit exceeded");
    if(!pass_live_||query_live_)throw std::runtime_error("Invalid Vulkan occlusion begin");
    // Reset only a block that will be used, outside the render pass. Ordinary
    // native draws use no diagnostic queries and do not clear 32768 slots.
    constexpr unsigned ResetBatch=256;
    if(f.queries%ResetBatch==0){const auto current=active_;end_pass();
        vkCmdResetQueryPool(command_,f.occlusion,f.queries,ResetBatch);target(current);}
    vkCmdBeginQuery(command_,f.occlusion,f.queries,features_.occlusionQueryPrecise?VK_QUERY_CONTROL_PRECISE_BIT:0);query_live_=true;
}
unsigned Context::end_occlusion(){auto& f=frames_[frame_];vkCmdEndQuery(command_,f.occlusion,f.queries);query_live_=false;return f.queries++;}
void Context::submit(){
    if(!recording_)return;end_pass();auto& f=frames_[frame_];
    if(f.timestamps)vkCmdWriteTimestamp(command_,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,f.timestamps,1);
#ifdef OR2_PS5_PAYLOAD
    static unsigned submissions=0;++submissions;const bool trace=submissions<=4||trace_submission_;trace_submission_=false;
    if(trace)std::fprintf(stdout,"[vk-submit] %u begin acquired=%u queries=%u\n",submissions,unsigned(acquired_live_),f.queries);
#endif
    require(vkEndCommandBuffer(command_),"End commands");require(vkResetFences(device_,1,&f.fence),"Reset frame fence");
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command_;
    // Acquired swapchain images are first touched at the colour-output stage.
    VkPipelineStageFlags wait=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if(acquired_live_){info.waitSemaphoreCount=1;info.pWaitSemaphores=&f.acquired;info.pWaitDstStageMask=&wait;
        info.signalSemaphoreCount=1;info.pSignalSemaphores=&rendered_.at(swap_index_);}
    require(vkQueueSubmit(queue_,1,&info,f.fence),"Submit Vulkan frame");
    f.submitted=true;recording_=false;frame_=(frame_+1)%FramesInFlight;
    if(acquired_live_){VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&rendered_[swap_index_];p.swapchainCount=1;p.pSwapchains=&swapchain_;p.pImageIndices=&swap_index_;
#ifdef OR2_PS5_PAYLOAD
        if(trace)std::fprintf(stdout,"[vk-present] image=%u layout=%u\n",swap_index_,unsigned(swap_images_[swap_index_]->layout));
#endif
        // The present is queued behind the frame; no CPU wait here. The slot
        // fence (waited before the slot records again) bounds frames in flight.
        require(vkQueuePresentKHR(queue_,&p),"Present Vulkan frame");
        acquired_live_=false;
#ifdef OR2_PS5_PAYLOAD
        // VideoOut flips do not dismiss the Shell's launch image. EGL does
        // this internally; a native Vulkan title must do it explicitly once
        // a frame has been rendered and queued for display.
        if(!splash_hidden_){const int result=sceSystemServiceHideSplashScreen();
            std::fprintf(stdout,"[vk-display] hide-splash=%08x\n",unsigned(result));
            splash_hidden_=result==0;}
#endif
    }
}
void Context::finish(){
    submit();
    {
#ifdef OR2_PS5_PAYLOAD
        const runtime::ProfileScope wait(runtime::profile_key(runtime::ProfileRenderer,0,runtime::ProfPcFence));
#endif
        for(unsigned k=0;k<FramesInFlight;++k){auto& f=frames_[(frame_+k)%FramesInFlight];
            if(!f.submitted)continue;
            require(vkWaitForFences(device_,1,&f.fence,VK_TRUE,std::numeric_limits<std::uint64_t>::max()),"Wait Vulkan frame");
            retire(f);
        }
    }
}
std::vector<std::uint8_t> Context::readback(const Texture& t,unsigned face,unsigned level){
    if(t->aspect!=VK_IMAGE_ASPECT_COLOR_BIT||t->samples!=VK_SAMPLE_COUNT_1_BIT||face>=t->layers||level>=t->levels)throw std::runtime_error("Invalid colour readback");
    const unsigned w=std::max(1u,t->width>>level),h=std::max(1u,t->height>>level);auto b=buffer(std::size_t(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    transition(t,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);end_pass();VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,face,1};copy.imageExtent={w,h,1};
    vkCmdCopyImageToBuffer(command_,t->image,t->layout,b->buffer,1,&copy);finish();std::vector<std::uint8_t> result(std::size_t(w)*h*4);
    std::memcpy(result.data(),b->mapped,result.size());if(t->format==VK_FORMAT_B8G8R8A8_UNORM)for(std::size_t i=0;i<result.size();i+=4)std::swap(result[i],result[i+2]);return result;
}
void Context::copy(const Texture& source,const Texture& destination){
    transition(source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);transition(destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);end_pass();
    VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={source->width,source->height,1};
    vkCmdCopyImage(command_,source->image,source->layout,destination->image,destination->layout,1,&copy);
}
void Context::resolve(const Texture& source,const Texture& destination){
    transition(source,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);transition(destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);end_pass();
    VkImageResolve resolve{};resolve.srcSubresource=resolve.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};resolve.extent={source->width,source->height,1};
    vkCmdResolveImage(command_,source->image,source->layout,destination->image,destination->layout,1,&resolve);
}
void Context::setup_display(unsigned width,unsigned height){
    unsigned count=0;require(vkGetPhysicalDeviceDisplayPropertiesKHR(physical_,&count,nullptr),"Enumerate PS5 displays");
    if(!count)throw std::runtime_error("RADV has no native display");std::vector<VkDisplayPropertiesKHR> displays(count);
    require(vkGetPhysicalDeviceDisplayPropertiesKHR(physical_,&count,displays.data()),"Get PS5 display");const auto display=displays[0].display;
    require(vkGetDisplayModePropertiesKHR(physical_,display,&count,nullptr),"Enumerate PS5 display modes");std::vector<VkDisplayModePropertiesKHR> modes(count);
    require(vkGetDisplayModePropertiesKHR(physical_,display,&count,modes.data()),"Get PS5 display modes");if(!count)throw std::runtime_error("No native display mode");
    // RADV reports 4K first, independently of the attached screen. Use the
    // smallest scanout that fits the compositor and the launch 3D resolution,
    // at the refresh nearest the display rate (Options > Settings FRAME RATE).
    // The 3D render resolution is scaled into it.
    for(const auto& m:modes)std::fprintf(stdout,"[vk-display] mode %ux%u %.3fHz\n",m.parameters.visibleRegion.width,m.parameters.visibleRegion.height,m.parameters.refreshRate/1000.0);
    // The 120 Hz mode whenever VideoOut offers it: FRAME RATE 60 then shows
    // each frame for two vblanks (pace_frames), so the row applies at once.
    const auto mode=select_display_mode(modes,std::max(width,preferred_width),std::max(height,preferred_height),120000u);
    display_width_=mode.parameters.visibleRegion.width;display_height_=mode.parameters.visibleRegion.height;
    std::fprintf(stdout,"[vk-display] compositor=%ux%u scanout=%ux%u refresh=%.3fHz\n",width,height,display_width_,display_height_,mode.parameters.refreshRate/1000.0);
    native_refresh=mode.parameters.refreshRate>=100000u?120u:60u;
    require(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_,&count,nullptr),"Enumerate display planes");std::vector<VkDisplayPlanePropertiesKHR> planes(count);
    require(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_,&count,planes.data()),"Get display planes");bool found=false;unsigned plane_index=0;
    for(unsigned i=0;i<planes.size();++i){unsigned n=0;require(vkGetDisplayPlaneSupportedDisplaysKHR(physical_,i,&n,nullptr),"Get plane displays");std::vector<VkDisplayKHR> supported(n);
        require(vkGetDisplayPlaneSupportedDisplaysKHR(physical_,i,&n,supported.data()),"Read plane displays");if(std::find(supported.begin(),supported.end(),display)!=supported.end()){plane_index=i;found=true;break;}}
    if(!found)throw std::runtime_error("No display plane supports VideoOut");
    VkDisplaySurfaceCreateInfoKHR surface{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};surface.displayMode=mode.displayMode;surface.planeIndex=plane_index;surface.planeStackIndex=planes[plane_index].currentStackIndex;
    surface.transform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;surface.alphaMode=VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;surface.imageExtent={display_width_,display_height_};
    require(vkCreateDisplayPlaneSurfaceKHR(instance_,&surface,nullptr,&surface_),"Create native display surface");VkBool32 supported=0;
    require(vkGetPhysicalDeviceSurfaceSupportKHR(physical_,family_,surface_,&supported),"Check display queue");if(!supported)throw std::runtime_error("Graphics queue cannot present");
    VkSurfaceCapabilitiesKHR caps{};require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_,surface_,&caps),"Get display capabilities");
    require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_,surface_,&count,nullptr),"Enumerate display formats");std::vector<VkSurfaceFormatKHR> formats(count);
    require(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_,surface_,&count,formats.data()),"Get display formats");
    auto format=std::find_if(formats.begin(),formats.end(),[](auto f){return f.format==VK_FORMAT_B8G8R8A8_UNORM||f.format==VK_FORMAT_R8G8B8A8_UNORM;});
    if(format==formats.end())throw std::runtime_error("No native RGBA8 swapchain format");
    unsigned images=std::max(3u,caps.minImageCount);if(caps.maxImageCount)images=std::min(images,caps.maxImageCount);
    VkSwapchainCreateInfoKHR swap{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};swap.surface=surface_;swap.minImageCount=images;swap.imageFormat=format->format;swap.imageColorSpace=format->colorSpace;
    swap.imageExtent={display_width_,display_height_};swap.imageArrayLayers=1;swap.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;swap.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    swap.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;swap.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;swap.presentMode=VK_PRESENT_MODE_FIFO_KHR;swap.clipped=VK_TRUE;
    require(vkCreateSwapchainKHR(device_,&swap,nullptr,&swapchain_),"Create VideoOut swapchain");require(vkGetSwapchainImagesKHR(device_,swapchain_,&count,nullptr),"Enumerate swapchain images");std::vector<VkImage> handles(count);
    require(vkGetSwapchainImagesKHR(device_,swapchain_,&count,handles.data()),"Get swapchain images");
    for(auto handle:handles){auto image=std::make_shared<Image>();image->device=device_;image->image=handle;image->width=display_width_;image->height=display_height_;image->format=format->format;image->owned=false;swap_images_.push_back(image);}
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    rendered_.resize(swap_images_.size());for(auto& s:rendered_)require(vkCreateSemaphore(device_,&semaphore,nullptr,&s),"Create present semaphore");
}
Texture Context::display_target(){
    if(!native_)return host_display_;
    // Start (or continue) the recording first: it waits for the slot whose
    // acquire semaphore is reused below.
    start();
    if(!acquired_live_){
#ifdef OR2_PS5_PAYLOAD
        static unsigned acquires=0;const bool trace=++acquires<=4;
        if(trace)std::fprintf(stdout,"[vk-acquire] %u begin\n",acquires);
#endif
        {
#ifdef OR2_PS5_PAYLOAD
            const runtime::ProfileScope wait(runtime::profile_key(runtime::ProfileRenderer,0,runtime::ProfAcquire));
#endif
            require(vkAcquireNextImageKHR(device_,swapchain_,std::numeric_limits<std::uint64_t>::max(),frames_[frame_].acquired,VK_NULL_HANDLE,&swap_index_),"Acquire VideoOut image");
        }
        acquired_live_=true;
#ifdef OR2_PS5_PAYLOAD
        if(trace)std::fprintf(stdout,"[vk-acquire] %u image=%u\n",acquires,swap_index_);
#endif
    }
    return swap_images_[swap_index_];
}
void Context::present(){if(native_)transition(display_target(),VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);submit();
    // Checkpoint once per 120 displayed frames, outside command recording.
    // Empty/unchanged caches do no IO. Closing an app also saves new keys.
    if(++cache_frames_==120){cache_frames_=0;save_pipeline_cache();}}
}
