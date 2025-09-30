#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace mango::graphics::vk
{
    struct Pipeline_Layout_Desc
    {
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;

        std::vector<VkPushConstantRange> push_constant_ranges;
    };

    class Vk_Pipeline_Layout
    {
    public:
        Vk_Pipeline_Layout(VkDevice device, const Pipeline_Layout_Desc& desc);
        ~Vk_Pipeline_Layout();

        Vk_Pipeline_Layout(const Vk_Pipeline_Layout&) = delete;
        Vk_Pipeline_Layout& operator=(const Vk_Pipeline_Layout&) = delete;
        Vk_Pipeline_Layout(Vk_Pipeline_Layout&& other) noexcept;
        Vk_Pipeline_Layout& operator=(Vk_Pipeline_Layout&& other) noexcept;

        auto get_vk_pipeline_layout() const -> VkPipelineLayout { return m_pipeline_layout; }

    private:
        void create_pipeline_layout(const Pipeline_Layout_Desc& desc);
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    };

} // namespace mango::graphics::vk
