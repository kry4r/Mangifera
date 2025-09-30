#pragma once
#include "render-pass/render-pass.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Render_Pass : public Render_Pass
    {
    public:
        Vk_Render_Pass(VkDevice device, const Render_Pass_Desc& desc);
        ~Vk_Render_Pass() override;

        Vk_Render_Pass(const Vk_Render_Pass&) = delete;
        Vk_Render_Pass& operator=(const Vk_Render_Pass&) = delete;
        Vk_Render_Pass(Vk_Render_Pass&& other) noexcept;
        Vk_Render_Pass& operator=(Vk_Render_Pass&& other) noexcept;

        // Render_Pass interface
        const Render_Pass_Desc& get_desc() const override { return m_desc; }

        // Vulkan specific
        auto get_vk_render_pass() const -> VkRenderPass { return m_render_pass; }

    private:
        void create_render_pass();
        void cleanup();

        // Helper functions
        VkAttachmentLoadOp to_vk_load_op(uint32_t load_op) const;
        VkAttachmentStoreOp to_vk_store_op(uint32_t store_op) const;
        VkImageLayout state_to_layout(uint32_t state) const;
        bool is_depth_stencil_format(VkFormat format) const;

        VkDevice m_device = VK_NULL_HANDLE;
        VkRenderPass m_render_pass = VK_NULL_HANDLE;
        Render_Pass_Desc m_desc;

        // Cache attachment formats for validation
        std::vector<VkFormat> m_attachment_formats;
    };

} // namespace mango::graphics::vk
