#include "vk-render-pass.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Render_Pass::Vk_Render_Pass(VkDevice device, const Render_Pass_Desc& desc)
        : m_device(device)
        , m_desc(desc)
    {
        create_render_pass();
    }

    Vk_Render_Pass::~Vk_Render_Pass()
    {
        cleanup();
    }

    Vk_Render_Pass::Vk_Render_Pass(Vk_Render_Pass&& other) noexcept
        : m_device(other.m_device)
        , m_render_pass(other.m_render_pass)
        , m_desc(std::move(other.m_desc))
        , m_attachment_formats(std::move(other.m_attachment_formats))
    {
        other.m_render_pass = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Render_Pass::operator=(Vk_Render_Pass&& other) noexcept -> Vk_Render_Pass&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_render_pass = other.m_render_pass;
            m_desc = std::move(other.m_desc);
            m_attachment_formats = std::move(other.m_attachment_formats);

            other.m_render_pass = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Render_Pass::create_render_pass()
    {
        if (m_desc.attachments.empty()) {
            throw std::runtime_error("Render pass must have at least one attachment");
        }

        if (m_desc.subpasses.empty()) {
            throw std::runtime_error("Render pass must have at least one subpass");
        }

        // Create attachment descriptions
        std::vector<VkAttachmentDescription> vk_attachments;
        vk_attachments.reserve(m_desc.attachments.size());
        m_attachment_formats.reserve(m_desc.attachments.size());

        for (const auto& attachment : m_desc.attachments) {
            VkAttachmentDescription vk_attachment{};

            // Get format from texture
            if (attachment.texture) {
                auto vk_texture = std::dynamic_pointer_cast<Vk_Texture>(attachment.texture);
                if (vk_texture) {
                    vk_attachment.format = vk_texture->get_vk_format();
                    m_attachment_formats.push_back(vk_attachment.format);
                } else {
                    throw std::runtime_error("Invalid texture type in render pass attachment");
                }
            } else {
                throw std::runtime_error("Attachment texture is null");
            }

            vk_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            vk_attachment.loadOp = to_vk_load_op(attachment.load_op);
            vk_attachment.storeOp = to_vk_store_op(attachment.store_op);

            // Stencil ops (for depth-stencil attachments)
            if (is_depth_stencil_format(vk_attachment.format)) {
                vk_attachment.stencilLoadOp = vk_attachment.loadOp;
                vk_attachment.stencilStoreOp = vk_attachment.storeOp;
            } else {
                vk_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                vk_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }

            vk_attachment.initialLayout = state_to_layout(attachment.initial_state);
            vk_attachment.finalLayout = state_to_layout(attachment.final_state);

            vk_attachments.push_back(vk_attachment);
        }

        // Create subpass descriptions and attachment references
        std::vector<VkSubpassDescription> vk_subpasses;
        std::vector<std::vector<VkAttachmentReference>> color_attachment_refs(m_desc.subpasses.size());
        std::vector<VkAttachmentReference> depth_attachment_refs(m_desc.subpasses.size());

        for (size_t i = 0; i < m_desc.subpasses.size(); ++i) {
            const auto& subpass = m_desc.subpasses[i];
            VkSubpassDescription vk_subpass{};
            vk_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            // Color attachments
            for (uint32_t attachment_index : subpass.color_attachments) {
                if (attachment_index >= m_desc.attachments.size()) {
                    throw std::runtime_error("Invalid color attachment index in subpass");
                }

                VkAttachmentReference ref{};
                ref.attachment = attachment_index;
                ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color_attachment_refs[i].push_back(ref);
            }

            vk_subpass.colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs[i].size());
            vk_subpass.pColorAttachments = color_attachment_refs[i].empty()
                ? nullptr
                : color_attachment_refs[i].data();

            // Depth-stencil attachment
            if (subpass.depth_stencil_attachment >= 0) {
                uint32_t ds_index = static_cast<uint32_t>(subpass.depth_stencil_attachment);
                if (ds_index >= m_desc.attachments.size()) {
                    throw std::runtime_error("Invalid depth-stencil attachment index in subpass");
                }

                depth_attachment_refs[i].attachment = ds_index;
                depth_attachment_refs[i].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                vk_subpass.pDepthStencilAttachment = &depth_attachment_refs[i];
            } else {
                vk_subpass.pDepthStencilAttachment = nullptr;
            }

            vk_subpasses.push_back(vk_subpass);
        }

        // Create subpass dependencies
        std::vector<VkSubpassDependency> dependencies;

        // Add dependency for external -> first subpass
        if (!vk_subpasses.empty()) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies.push_back(dependency);
        }

        // Add dependencies between subpasses
        for (size_t i = 0; i + 1 < vk_subpasses.size(); ++i) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = static_cast<uint32_t>(i);
            dependency.dstSubpass = static_cast<uint32_t>(i + 1);
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            dependencies.push_back(dependency);
        }

        // Create render pass
        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = static_cast<uint32_t>(vk_attachments.size());
        render_pass_info.pAttachments = vk_attachments.data();
        render_pass_info.subpassCount = static_cast<uint32_t>(vk_subpasses.size());
        render_pass_info.pSubpasses = vk_subpasses.data();
        render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
        render_pass_info.pDependencies = dependencies.data();

        if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan render pass");
        }

        UH_INFO_FMT("Vulkan render pass created with {} attachments and {} subpasses",
            vk_attachments.size(), vk_subpasses.size());
    }

    void Vk_Render_Pass::cleanup()
    {
        if (m_render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_render_pass, nullptr);
            m_render_pass = VK_NULL_HANDLE;
            UH_INFO("Vulkan render pass destroyed");
        }
    }

    VkAttachmentLoadOp Vk_Render_Pass::to_vk_load_op(uint32_t load_op) const
    {
        switch (load_op) {
            case 0: return VK_ATTACHMENT_LOAD_OP_LOAD;     // Load existing contents
            case 1: return VK_ATTACHMENT_LOAD_OP_CLEAR;    // Clear to constant
            case 2: return VK_ATTACHMENT_LOAD_OP_DONT_CARE; // Don't care
            default: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
    }

    VkAttachmentStoreOp Vk_Render_Pass::to_vk_store_op(uint32_t store_op) const
    {
        switch (store_op) {
            case 0: return VK_ATTACHMENT_STORE_OP_STORE;    // Store results
            case 1: return VK_ATTACHMENT_STORE_OP_DONT_CARE; // Discard results
            default: return VK_ATTACHMENT_STORE_OP_STORE;
        }
    }

    VkImageLayout Vk_Render_Pass::state_to_layout(uint32_t state) const
    {
        // Map from Resource_State to VkImageLayout
        switch (state) {
            case 0: return VK_IMAGE_LAYOUT_UNDEFINED;
            case 1: return VK_IMAGE_LAYOUT_GENERAL;
            case 2: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case 3: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case 4: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case 5: return VK_IMAGE_LAYOUT_GENERAL; // unordered_access
            case 6: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case 7: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case 8: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    bool Vk_Render_Pass::is_depth_stencil_format(VkFormat format) const
    {
        return format == VK_FORMAT_D16_UNORM ||
               format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
               format == VK_FORMAT_D32_SFLOAT ||
               format == VK_FORMAT_D16_UNORM_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

} // namespace mango::graphics::vk
