#pragma once
#include "device.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace mango::graphics::vk
{
    class Vk_Device : public Device
    {
    public:
        explicit Vk_Device(const Device_Desc& desc);
        ~Vk_Device() override;

        Vk_Device(const Vk_Device&) = delete;
        Vk_Device& operator=(const Vk_Device&) = delete;
        Vk_Device(Vk_Device&&) noexcept = default;
        Vk_Device& operator=(Vk_Device&&) noexcept = default;

        // ========== Resource creation ==========
        Command_Pool_Handle create_command_pool() override;
        Command_Queue_Handle create_command_queue(Queue_Type type) override;

        Fence_Handle create_fence(bool signaled) override;
        Semaphore_Handle create_semaphore(bool timeline, uint64_t initial_value) override;

        Buffer_Handle create_buffer(const Buffer_Desc& desc) override;
        Texture_Handle create_texture(const Texture_Desc& desc) override;
        Sampler_Handle create_sampler(const Sampler_Desc& desc) override;
        Shader_Handle create_shader(const Shader_Desc& desc) override;

        Render_Pass_Handle create_render_pass(const Render_Pass_Desc& desc) override;
        Framebuffer_Handle create_framebuffer(const Framebuffer_Desc& desc) override;
        Swapchain_Handle create_swapchain(const Swapchain_Desc& desc) override;

        Graphics_Pipeline_Handle create_graphics_pipeline(const Graphics_Pipeline_Desc& desc) override;
        Compute_Pipeline_Handle create_compute_pipeline(const Compute_Pipeline_Desc& desc) override;
        Raytracing_Pipeline_Handle create_raytracing_pipeline(const Raytracing_Pipeline_Desc& desc) override;

        // ========== Device queries ==========
        uint32_t get_queue_family_count() const override;
        std::vector<Queue_Type> get_supported_queues() const override;

        void wait_idle() override;

        // ========== Vulkan specific getters ==========
        // only for vulkan subsytem
        auto get_vk_device() const -> VkDevice { return m_device; }
        auto get_vk_physical_device() const -> VkPhysicalDevice { return m_physical_device; }
        auto get_vk_instance() const -> VkInstance { return m_instance; }

        // get queue index
        auto get_graphics_queue_family() const -> uint32_t { return m_graphics_family; }
        auto get_compute_queue_family() const -> uint32_t { return m_compute_family; }
        auto get_transfer_queue_family() const -> uint32_t { return m_transfer_family; }

        Command_Pool_Handle create_command_pool_for_queue_family(uint32_t queue_family_index,
                                                          bool transient = false,
                                                          bool reset_command_buffer = true);

    private:
        // ========== Initialization methods ==========
        void create_instance(const Device_Desc& desc);
        void setup_debug_messenger();
        void pick_physical_device(const Device_Desc& desc);
        void create_logical_device(const Device_Desc& desc);
        void find_queue_families();

        // ========== Helper methods ==========
        bool is_device_suitable(VkPhysicalDevice device);
        bool check_validation_layer_support();
        std::vector<const char*> get_required_extensions();

        // ========== Cleanup ==========
        void cleanup();

        // ========== Vulkan handles ==========
        VkInstance m_instance = VK_NULL_HANDLE;
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;

        // ========== Queue family indices ==========
        uint32_t m_graphics_family = UINT32_MAX;
        uint32_t m_compute_family = UINT32_MAX;
        uint32_t m_transfer_family = UINT32_MAX;
        uint32_t m_present_family = UINT32_MAX;

        // ========== Device properties ==========
        VkPhysicalDeviceProperties m_device_properties{};
        VkPhysicalDeviceFeatures m_device_features{};
        VkPhysicalDeviceMemoryProperties m_memory_properties{};

        // ========== Configuration ==========
        bool m_enable_validation = false;
        bool m_enable_raytracing = false;

        // Validation layers
        const std::vector<const char*> m_validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };
    };

} // namespace mango::graphics::vk
