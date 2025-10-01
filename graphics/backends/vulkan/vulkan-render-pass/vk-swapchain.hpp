#pragma once
#include "render-pass/swapchain.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace mango::graphics::vk
{
    struct Vk_Swapchain_Texture: public Vk_Texture
    {
    public:
        Vk_Swapchain_Texture(VkDevice device,
            VkPhysicalDevice physical_device,
            VkImage image,
            VkImageView image_view,
            VkFormat format,
            uint32_t width,
            uint32_t height)
            : Vk_Texture(device, physical_device, make_swapchain_desc(format, width, height))
        {
            // Override the created resources with swapchain-provided ones
            // The base class constructor will try to create image/view,
            // but we'll replace them with swapchain's

            // Clean up what base class created (if any)
            if (m_image != VK_NULL_HANDLE) {
                // Base class created an image, we need to destroy it
                // since we'll use swapchain's image instead
                vkDestroyImage(m_device, m_image, nullptr);
            }
            if (m_image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, m_image_view, nullptr);
            }
            if (m_memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, m_memory, nullptr);
            }

            // Use swapchain's resources
            m_image = image;
            m_image_view = image_view;
            m_vk_format = format;
            m_memory = VK_NULL_HANDLE; // Swapchain images don't have separate memory
            m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        ~Vk_Swapchain_Texture() override
        {
            // CRITICAL: Prevent base class from destroying swapchain resources
            m_image = VK_NULL_HANDLE;
            m_image_view = VK_NULL_HANDLE;
            m_memory = VK_NULL_HANDLE;
            // Base class destructor will now be safe
        }

    private:
        static Texture_Desc make_swapchain_desc(VkFormat format, uint32_t width, uint32_t height)
        {
            Texture_Desc desc{};
            desc.dimension = Texture_Kind::tex_2d;
            desc.format = vk_format_to_texture_format(format);
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mip_levels = 1;
            desc.arrayLayers = 1;
            desc.sampled = false;
            desc.render_target = true;
            return desc;
        }

        static Texture_Format vk_format_to_texture_format(VkFormat format)
        {
            switch (format) {
                case VK_FORMAT_B8G8R8A8_SRGB: return Texture_Format::sRGB_alpha8;
                case VK_FORMAT_B8G8R8A8_UNORM: return Texture_Format::rgba8;
                case VK_FORMAT_R8G8B8A8_SRGB: return Texture_Format::sRGB_alpha8;
                case VK_FORMAT_R8G8B8A8_UNORM: return Texture_Format::rgba8;
                default: return Texture_Format::rgba8;
            }
        }
    };

    class Vk_Swapchain: public Swapchain
    {
    public:
        Vk_Swapchain(VkDevice device, VkPhysicalDevice physical_device,
            VkInstance instance, const Swapchain_Desc& desc);
        ~Vk_Swapchain() override;

        Vk_Swapchain(const Vk_Swapchain&) = delete;
        Vk_Swapchain& operator=(const Vk_Swapchain&) = delete;
        Vk_Swapchain(Vk_Swapchain&& other) noexcept;
        Vk_Swapchain& operator=(Vk_Swapchain&& other) noexcept;

        // Swapchain interface
        int32_t acquire_next_image(std::shared_ptr<Semaphore> wait_semaphore = nullptr) override;

        void present(uint32_t image_index,
            const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores = {}) override;

        const Swapchain_Desc& get_desc() const override { return m_desc; }

        const std::vector<std::shared_ptr<Texture>>& get_images() const override { return m_images; }

        // Vulkan specific
        auto get_vk_swapchain() const -> VkSwapchainKHR { return m_swapchain; }
        auto get_vk_surface() const -> VkSurfaceKHR { return m_surface; }

    private:
        void create_surface();
        void create_swapchain();
        void create_image_views();
        void cleanup();

        struct SwapchainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> present_modes;
        };

        SwapchainSupportDetails query_swapchain_support();
        VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
        VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_modes);
        VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);

        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkInstance m_instance = VK_NULL_HANDLE;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

        Swapchain_Desc m_desc;
        std::vector<VkImage> m_vk_images;
        std::vector<VkImageView> m_image_views;
        std::vector<std::shared_ptr<Texture>> m_images;

        VkFormat m_image_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
    };

} // namespace mango::graphics::vk
