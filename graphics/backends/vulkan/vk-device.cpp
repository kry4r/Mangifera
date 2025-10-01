#include "vk-device.hpp"
#include "vulkan-render-resource/vk-buffer.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include "vulkan-sync/vk-semaphore.hpp"
#include "vulkan-sync/vk-fence.hpp"
#include "vulkan-render-resource/vk-shader.hpp"
#include "vulkan-render-resource/vk-sampler.hpp"
#include "vulkan-render-pass/vk-framebuffer.hpp"
#include "vulkan-render-pass/vk-render-pass.hpp"
#include "vulkan-render-pass/vk-swapchain.hpp"
#include "vulkan-command-execution/vk-command-buffer.hpp"
#include "vulkan-command-execution/vk-command-pool.hpp"
#include "vulkan-command-execution/vk-command-queue.hpp"
#include "vulkan-pipeline-state/vk-graphics-pipeline-state.hpp"
#include "vulkan-pipeline-state/vk-raytracing-pipeline-state.hpp"
#include "vulkan-pipeline-state/vk-compute-pipeline-state.hpp"
#include "log/historiographer.hpp"
#include <set>
namespace mango::graphics::vk
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data)
    {
        if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            UH_ERROR_FMT("Validation layer: {}", callback_data->pMessage);
        }
        return VK_FALSE;
    }

    // Helper to create debug messenger
    VkResult create_debug_utils_messenger_ext(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* create_info,
        const VkAllocationCallbacks* allocator,
        VkDebugUtilsMessengerEXT* debug_messenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, create_info, allocator, debug_messenger);
        }
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    void destroy_debug_utils_messenger_ext(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debug_messenger,
        const VkAllocationCallbacks* allocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debug_messenger, allocator);
        }
    }

    Vk_Device::Vk_Device(const Device_Desc& desc)
        : m_enable_validation(desc.enable_validation)
        , m_enable_raytracing(desc.enable_raytracing)
    {
        try {
            create_instance(desc);
            setup_debug_messenger();
            pick_physical_device(desc);
            find_queue_families();
            create_logical_device(desc);

            UH_INFO("Vulkan device created successfully");
        }
        catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }

    Vk_Device::~Vk_Device()
    {
        cleanup();
    }

    void Vk_Device::create_instance(const Device_Desc& desc)
    {
        if (m_enable_validation && !check_validation_layer_support()) {
            throw std::runtime_error("Validation layers requested but not available");
        }

        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Mangifera";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Mangifera Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        auto extensions = get_required_extensions();
        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        // macOS: Enable portability enumeration
        #ifdef __APPLE__
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        #endif

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        if (m_enable_validation) {
            create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();

            // Setup debug messenger for instance creation/destruction
            debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_create_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_create_info.pfnUserCallback = debug_callback;

            create_info.pNext = &debug_create_info;
        } else {
            create_info.enabledLayerCount = 0;
            create_info.pNext = nullptr;
        }

        if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        UH_INFO("Vulkan instance created");
    }

    void Vk_Device::setup_debug_messenger()
    {
        if (!m_enable_validation) return;

        VkDebugUtilsMessengerCreateInfoEXT create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = debug_callback;

        if (create_debug_utils_messenger_ext(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger");
        }

        UH_INFO("Debug messenger setup");
    }

    void Vk_Device::pick_physical_device(const Device_Desc& desc)
    {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

        if (device_count == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

        // Try to use preferred adapter index
        if (desc.preferred_adapter_index < device_count) {
            VkPhysicalDevice candidate = devices[desc.preferred_adapter_index];
            if (is_device_suitable(candidate)) {
                m_physical_device = candidate;
            }
        }

        // If preferred device not suitable, find the first suitable one
        if (m_physical_device == VK_NULL_HANDLE) {
            for (const auto& device : devices) {
                if (is_device_suitable(device)) {
                    m_physical_device = device;
                    break;
                }
            }
        }

        if (m_physical_device == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to find a suitable GPU");
        }

        vkGetPhysicalDeviceProperties(m_physical_device, &m_device_properties);
        vkGetPhysicalDeviceFeatures(m_physical_device, &m_device_features);
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);

        UH_INFO_FMT("Selected GPU: {}", m_device_properties.deviceName);
    }

    void Vk_Device::find_queue_families()
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());

        // Find queue families
        for (uint32_t i = 0; i < queue_family_count; i++) {
            const auto& queue_family = queue_families[i];

            // Graphics queue (usually also supports compute and transfer)
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (m_graphics_family == UINT32_MAX) {
                    m_graphics_family = i;
                    m_present_family = i; // Assume graphics queue can present
                }
            }

            // Dedicated compute queue (optional, for better parallelism)
            if ((queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                m_compute_family = i;
            }

            // Dedicated transfer queue (optional, for better parallelism)
            if ((queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                !(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                !(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                m_transfer_family = i;
            }
        }

        // Fallback: use graphics queue for compute and transfer if no dedicated queues
        if (m_compute_family == UINT32_MAX && m_graphics_family != UINT32_MAX) {
            m_compute_family = m_graphics_family;
        }
        if (m_transfer_family == UINT32_MAX && m_graphics_family != UINT32_MAX) {
            m_transfer_family = m_graphics_family;
        }

        if (m_graphics_family == UINT32_MAX) {
            throw std::runtime_error("Failed to find graphics queue family");
        }

        UH_INFO_FMT("Queue families - Graphics: {}, Compute: {}, Transfer: {}",
            m_graphics_family, m_compute_family, m_transfer_family);
    }

    void Vk_Device::create_logical_device(const Device_Desc& desc)
    {
        std::set<uint32_t> unique_queue_families = {
            m_graphics_family,
            m_compute_family,
            m_transfer_family
        };

        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        float queue_priority = 1.0f;

        for (uint32_t queue_family : unique_queue_families) {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = queue_family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        // Query supported features
        VkPhysicalDeviceFeatures supported_features{};
        vkGetPhysicalDeviceFeatures(m_physical_device, &supported_features);

        VkPhysicalDeviceFeatures device_features{};

        if (supported_features.samplerAnisotropy) {
            device_features.samplerAnisotropy = VK_TRUE;
        }

        if (supported_features.fillModeNonSolid) {
            device_features.fillModeNonSolid = VK_TRUE;
        }

        // Enable timeline semaphore feature
        VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features{};
        timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        timeline_features.timelineSemaphore = VK_TRUE;
        timeline_features.pNext = nullptr;

        VkDeviceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext = &timeline_features;  // Chain timeline semaphore features
        create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = &device_features;

        // Device extensions
        std::vector<const char*> device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        #ifdef __APPLE__
        device_extensions.push_back("VK_KHR_portability_subset");
        #endif

        if (m_enable_raytracing) {
            device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        }

        create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
        create_info.ppEnabledExtensionNames = device_extensions.data();

        if (m_enable_validation) {
            create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        } else {
            create_info.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        vkGetDeviceQueue(m_device, m_graphics_family, 0, &m_graphics_queue);
        vkGetDeviceQueue(m_device, m_compute_family, 0, &m_compute_queue);
        vkGetDeviceQueue(m_device, m_transfer_family, 0, &m_transfer_queue);

        UH_INFO("Logical device created and queues retrieved");
    }

    bool Vk_Device::is_device_suitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);

        // Check queue family support
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        bool has_graphics_queue = false;
        for (const auto& queue_family : queue_families) {
            if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                has_graphics_queue = true;
                break;
            }
        }

        // Check extension support
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        for (const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        bool extensions_supported = required_extensions.empty();

        // Prefer discrete GPU
        bool is_discrete_gpu = device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

        return has_graphics_queue && extensions_supported;
    }

    bool Vk_Device::check_validation_layer_support()
    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const char* layer_name : m_validation_layers) {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName) == 0) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) {
                return false;
            }
        }

        return true;
    }

    std::vector<const char*> Vk_Device::get_required_extensions()
    {
        std::vector<const char*> extensions;

        // Add platform-specific surface extensions
        #ifdef _WIN32
        extensions.push_back("VK_KHR_surface");
        extensions.push_back("VK_KHR_win32_surface");
        #elif __linux__
        extensions.push_back("VK_KHR_surface");
        extensions.push_back("VK_KHR_xcb_surface");
        #elif __APPLE__
        extensions.push_back("VK_KHR_surface");
        extensions.push_back("VK_EXT_metal_surface");
        // macOS: Add portability enumeration extension
        extensions.push_back("VK_KHR_portability_enumeration");
        extensions.push_back("VK_KHR_get_physical_device_properties2");
        #endif

        if (m_enable_validation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void Vk_Device::cleanup()
    {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
            UH_INFO("Logical device destroyed");
        }

        if (m_debug_messenger != VK_NULL_HANDLE) {
            destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
            m_debug_messenger = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            UH_INFO("Vulkan instance destroyed");
        }
    }

    //-------Other resource ceate function--------

    Command_Pool_Handle Vk_Device::create_command_pool()
    {
        return create_command_pool_for_queue_family(m_graphics_family);
    }

    Command_Queue_Handle Vk_Device::create_command_queue(Queue_Type type)
    {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queue_family = 0;

        switch (type) {
            case Queue_Type::graphics:
            case Queue_Type::present:
                queue = m_graphics_queue;
                queue_family = m_graphics_family;
                break;

            case Queue_Type::compute:
                queue = m_compute_queue;
                queue_family = m_compute_family;
                break;

            case Queue_Type::transfer:
                queue = m_transfer_queue;
                queue_family = m_transfer_family;
                break;

            default:
                throw std::runtime_error("Unsupported queue type");
        }

        return std::make_shared<Vk_Command_Queue>(m_device, queue, queue_family, type);

    }

    Fence_Handle Vk_Device::create_fence(bool signaled)
    {
        return std::make_shared<Vk_Fence>(m_device, signaled);
    }

    Semaphore_Handle Vk_Device::create_semaphore(bool timeline, uint64_t initial_value)
    {
        Semaphore_Desc desc{};
        desc.type = timeline ? Semaphore_Type::timeline : Semaphore_Type::binary;
        desc.initial_value = initial_value;

        return std::make_shared<Vk_Semaphore>(m_device, desc);
    }

    Buffer_Handle Vk_Device::create_buffer(const Buffer_Desc& desc)
    {
        return std::make_shared<Vk_Buffer>(m_device, m_physical_device, desc);
    }

    Texture_Handle Vk_Device::create_texture(const Texture_Desc& desc)
    {
        return std::make_shared<Vk_Texture>(m_device, m_physical_device, desc);
    }

    Sampler_Handle Vk_Device::create_sampler(const Sampler_Desc& desc)
    {
        return std::make_shared<Vk_Sampler>(m_device, m_physical_device, desc);
    }

    Shader_Handle Vk_Device::create_shader(const Shader_Desc& desc)
    {
        return std::make_shared<Vk_Shader>(m_device, desc);
    }

    Render_Pass_Handle Vk_Device::create_render_pass(const Render_Pass_Desc& desc)
    {
        return std::make_shared<Vk_Render_Pass>(m_device, desc);
    }

    Framebuffer_Handle Vk_Device::create_framebuffer(const Framebuffer_Desc& desc)
    {
        return std::make_shared<Vk_Framebuffer>(m_device, desc);
    }

    Swapchain_Handle Vk_Device::create_swapchain(const Swapchain_Desc& desc)
    {
        return std::make_shared<Vk_Swapchain>(m_device, m_physical_device, m_instance, desc);
    }

    Graphics_Pipeline_Handle Vk_Device::create_graphics_pipeline(const Graphics_Pipeline_Desc& desc)
    {
        if (!desc.render_pass) {
            throw std::runtime_error("Graphics pipeline requires a render pass");
        }

        auto vk_render_pass = std::dynamic_pointer_cast<Vk_Render_Pass>(desc.render_pass);
        if (!vk_render_pass) {
            throw std::runtime_error("Invalid render pass type for Vulkan graphics pipeline");
        }

        return std::make_shared<Vk_Graphics_Pipeline_State>(
            m_device,
            desc,
            vk_render_pass->get_vk_render_pass()
        );
    }

    Compute_Pipeline_Handle Vk_Device::create_compute_pipeline(const Compute_Pipeline_Desc& desc)
    {
        return std::make_shared<Vk_Compute_Pipeline_State>(m_device, desc);
    }

    Raytracing_Pipeline_Handle Vk_Device::create_raytracing_pipeline(const Raytracing_Pipeline_Desc& desc)
    {
        if (!m_enable_raytracing) {
            throw std::runtime_error("Raytracing is not enabled for this device");
        }

        return std::make_shared<Vk_Raytracing_Pipeline_State>(m_device, desc);
    }

    // ========== Device Queries ==========

    uint32_t Vk_Device::get_queue_family_count() const
    {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
        return queue_family_count;
    }

    std::vector<Queue_Type> Vk_Device::get_supported_queues() const
    {
        std::vector<Queue_Type> supported;

        if (m_graphics_family != UINT32_MAX) {
            supported.push_back(Queue_Type::graphics);
            supported.push_back(Queue_Type::present);
        }
        if (m_compute_family != UINT32_MAX) {
            supported.push_back(Queue_Type::compute);
        }
        if (m_transfer_family != UINT32_MAX) {
            supported.push_back(Queue_Type::transfer);
        }

        return supported;
    }

    Command_Pool_Handle Vk_Device::create_command_pool_for_queue_family(
        uint32_t queue_family_index,
        bool transient,
        bool reset_command_buffer)
    {
        return std::make_shared<Vk_Command_Pool>(
            m_device,
            queue_family_index,
            transient,
            reset_command_buffer
        );
    }

    void Vk_Device::wait_idle()
    {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
        }
    }

    void Vk_Device::create_default_descriptor_pool()
    {
        std::vector<Vk_Descriptor_Pool::Pool_Size> pool_sizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        };

        m_descriptor_pool = std::make_unique<Vk_Descriptor_Pool>(
            m_device, 1000, pool_sizes);
    }

    Descriptor_Set_Layout_Handle Vk_Device::create_descriptor_set_layout(
        const Descriptor_Set_Layout_Desc& desc)
    {
        return std::make_shared<Vk_Descriptor_Set_Layout>(m_device, desc);
    }

    Descriptor_Set_Handle Vk_Device::create_descriptor_set(
        std::shared_ptr<Descriptor_Set_Layout> layout)
    {
        auto vk_layout = std::dynamic_pointer_cast<Vk_Descriptor_Set_Layout>(layout);
        if (!vk_layout) {
            throw std::runtime_error("Invalid descriptor set layout type");
        }

        if (!m_descriptor_pool) {
            create_default_descriptor_pool();
        }

        return std::make_shared<Vk_Descriptor_Set>(
            m_device, m_descriptor_pool->get_vk_pool(), vk_layout);
    }
}
