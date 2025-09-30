#include "vk-pipeline-layout.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Pipeline_Layout::Vk_Pipeline_Layout(VkDevice device, const Pipeline_Layout_Desc& desc)
        : m_device(device)
    {
        create_pipeline_layout(desc);
    }

    Vk_Pipeline_Layout::~Vk_Pipeline_Layout()
    {
        cleanup();
    }

    Vk_Pipeline_Layout::Vk_Pipeline_Layout(Vk_Pipeline_Layout&& other) noexcept
        : m_device(other.m_device)
        , m_pipeline_layout(other.m_pipeline_layout)
    {
        other.m_pipeline_layout = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Pipeline_Layout::operator=(Vk_Pipeline_Layout&& other) noexcept -> Vk_Pipeline_Layout&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_pipeline_layout = other.m_pipeline_layout;

            other.m_pipeline_layout = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Pipeline_Layout::create_pipeline_layout(const Pipeline_Layout_Desc& desc)
    {
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = static_cast<uint32_t>(desc.descriptor_set_layouts.size());
        layout_info.pSetLayouts = desc.descriptor_set_layouts.empty() ? nullptr : desc.descriptor_set_layouts.data();
        layout_info.pushConstantRangeCount = static_cast<uint32_t>(desc.push_constant_ranges.size());
        layout_info.pPushConstantRanges = desc.push_constant_ranges.empty() ? nullptr : desc.push_constant_ranges.data();

        if (vkCreatePipelineLayout(m_device, &layout_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        UH_INFO("Pipeline layout created");
    }

    void Vk_Pipeline_Layout::cleanup()
    {
        if (m_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
            m_pipeline_layout = VK_NULL_HANDLE;
        }
    }

} // namespace mango::graphics::vk
