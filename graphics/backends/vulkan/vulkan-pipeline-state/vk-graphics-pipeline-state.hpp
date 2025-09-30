#pragma once
#include "pipeline-state/graphics-pipeline-state.hpp"
#include "vk-pipeline-layout.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace mango::graphics::vk
{
    class Vk_Graphics_Pipeline_State : public Graphics_Pipeline_State
    {
    public:
        Vk_Graphics_Pipeline_State(VkDevice device, const Graphics_Pipeline_Desc& desc, VkRenderPass render_pass);
        ~Vk_Graphics_Pipeline_State();

        Vk_Graphics_Pipeline_State(const Vk_Graphics_Pipeline_State&) = delete;
        Vk_Graphics_Pipeline_State& operator=(const Vk_Graphics_Pipeline_State&) = delete;
        Vk_Graphics_Pipeline_State(Vk_Graphics_Pipeline_State&& other) noexcept;
        Vk_Graphics_Pipeline_State& operator=(Vk_Graphics_Pipeline_State&& other) noexcept;

        // Vulkan specific
        auto get_vk_pipeline() const -> VkPipeline { return m_pipeline; }
        auto get_vk_pipeline_layout() const -> VkPipelineLayout { return m_pipeline_layout->get_vk_pipeline_layout(); }

    private:
        void create_pipeline(VkRenderPass render_pass);
        void create_pipeline_layout();
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        std::unique_ptr<Vk_Pipeline_Layout> m_pipeline_layout;
    };

} // namespace mango::graphics::vk
