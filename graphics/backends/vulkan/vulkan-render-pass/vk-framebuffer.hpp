#pragma once
#include "render-pass/framebuffer.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Framebuffer : public Framebuffer
    {
    public:
        Vk_Framebuffer(VkDevice device, const Framebuffer_Desc& desc);
        ~Vk_Framebuffer() override;

        Vk_Framebuffer(const Vk_Framebuffer&) = delete;
        Vk_Framebuffer& operator=(const Vk_Framebuffer&) = delete;
        Vk_Framebuffer(Vk_Framebuffer&& other) noexcept;
        Vk_Framebuffer& operator=(Vk_Framebuffer&& other) noexcept;

        // Framebuffer interface
        const Framebuffer_Desc& get_desc() const override { return m_desc; }

        // Vulkan specific
        auto get_vk_framebuffer() const -> VkFramebuffer { return m_framebuffer; }

    private:
        void create_framebuffer();
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
        Framebuffer_Desc m_desc;

        // Cache image views for lifetime management
        std::vector<VkImageView> m_image_views;
    };

} // namespace mango::graphics::vk
