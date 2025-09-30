#include "vk-framebuffer.hpp"
#include "vk-render-pass.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Framebuffer::Vk_Framebuffer(VkDevice device, const Framebuffer_Desc& desc)
        : m_device(device)
        , m_desc(desc)
    {
        create_framebuffer();
    }

    Vk_Framebuffer::~Vk_Framebuffer()
    {
        cleanup();
    }

    Vk_Framebuffer::Vk_Framebuffer(Vk_Framebuffer&& other) noexcept
        : m_device(other.m_device)
        , m_framebuffer(other.m_framebuffer)
        , m_desc(std::move(other.m_desc))
        , m_image_views(std::move(other.m_image_views))
    {
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Framebuffer::operator=(Vk_Framebuffer&& other) noexcept -> Vk_Framebuffer&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_framebuffer = other.m_framebuffer;
            m_desc = std::move(other.m_desc);
            m_image_views = std::move(other.m_image_views);

            other.m_framebuffer = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Framebuffer::create_framebuffer()
    {
        if (!m_desc.render_pass) {
            throw std::runtime_error("Framebuffer must have a render pass");
        }

        if (m_desc.attachments.empty()) {
            throw std::runtime_error("Framebuffer must have at least one attachment");
        }

        if (m_desc.width == 0 || m_desc.height == 0) {
            throw std::runtime_error("Framebuffer dimensions must be non-zero");
        }

        // Get Vulkan render pass
        auto vk_render_pass = std::dynamic_pointer_cast<Vk_Render_Pass>(m_desc.render_pass);
        if (!vk_render_pass) {
            throw std::runtime_error("Invalid render pass type for Vulkan framebuffer");
        }

        // Collect image views from attachments
        m_image_views.reserve(m_desc.attachments.size());

        for (const auto& attachment : m_desc.attachments) {
            if (!attachment) {
                throw std::runtime_error("Framebuffer attachment is null");
            }

            auto vk_texture = std::dynamic_pointer_cast<Vk_Texture>(attachment);
            if (!vk_texture) {
                throw std::runtime_error("Invalid texture type in framebuffer attachment");
            }

            VkImageView image_view = vk_texture->get_vk_image_view();
            if (image_view == VK_NULL_HANDLE) {
                throw std::runtime_error("Texture image view is invalid");
            }

            m_image_views.push_back(image_view);
        }

        // Create framebuffer
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = vk_render_pass->get_vk_render_pass();
        framebuffer_info.attachmentCount = static_cast<uint32_t>(m_image_views.size());
        framebuffer_info.pAttachments = m_image_views.data();
        framebuffer_info.width = m_desc.width;
        framebuffer_info.height = m_desc.height;
        framebuffer_info.layers = m_desc.layers;

        if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &m_framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan framebuffer");
        }

        UH_INFO_FMT("Vulkan framebuffer created ({}x{}, {} layers, {} attachments)",
            m_desc.width, m_desc.height, m_desc.layers, m_image_views.size());
    }

    void Vk_Framebuffer::cleanup()
    {
        if (m_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
            UH_INFO("Vulkan framebuffer destroyed");
        }

        m_image_views.clear();
    }

} // namespace mango::graphics::vk
