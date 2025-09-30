#pragma once
#include "render-pass/swapchain.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace mango::graphics::vk
{
    class Vk_Swapchain : public Swapchain
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

        struct SwapchainSupportDetails {
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
