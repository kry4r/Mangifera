#include "vk-compute-pipeline-state.hpp"
#include "vulkan-render-resource/vk-shader.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Compute_Pipeline_State::Vk_Compute_Pipeline_State(VkDevice device, const Compute_Pipeline_Desc& desc)
        : Compute_Pipeline_State(desc)
        , m_device(device)
    {
        create_pipeline_layout();
        create_pipeline();
    }

    Vk_Compute_Pipeline_State::~Vk_Compute_Pipeline_State()
    {
        cleanup();
    }

    Vk_Compute_Pipeline_State::Vk_Compute_Pipeline_State(Vk_Compute_Pipeline_State&& other) noexcept
        : Compute_Pipeline_State(std::move(other))
        , m_device(other.m_device)
        , m_pipeline(other.m_pipeline)
        , m_pipeline_layout(std::move(other.m_pipeline_layout))
    {
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Compute_Pipeline_State::operator=(Vk_Compute_Pipeline_State&& other) noexcept -> Vk_Compute_Pipeline_State&
    {
        if (this != &other) {
            cleanup();

            Compute_Pipeline_State::operator=(std::move(other));

            m_device = other.m_device;
            m_pipeline = other.m_pipeline;
            m_pipeline_layout = std::move(other.m_pipeline_layout);

            other.m_pipeline = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Compute_Pipeline_State::create_pipeline_layout()
    {
        Pipeline_Layout_Desc layout_desc{};
        //TODO:RTemporarily simplified, reflection or explicit specification can be implemented from shaders later

        m_pipeline_layout = std::make_unique<Vk_Pipeline_Layout>(m_device, layout_desc);
    }

    void Vk_Compute_Pipeline_State::create_pipeline()
    {
        const auto& desc = get_desc();

        if (!desc.compute_shader) {
            throw std::runtime_error("Compute pipeline requires compute shader");
        }

        auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(desc.compute_shader);
        if (!vk_shader) {
            throw std::runtime_error("Invalid shader type for compute pipeline");
        }

        // Compute shader stage
        VkPipelineShaderStageCreateInfo shader_stage{};
        shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage.stage = vk_shader->get_vk_stage_flags();
        shader_stage.module = vk_shader->get_vk_shader_module();
        shader_stage.pName = vk_shader->get_entry_point();
        shader_stage.pSpecializationInfo = nullptr;

        // Create compute pipeline
        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = shader_stage;
        pipeline_info.layout = m_pipeline_layout->get_vk_pipeline_layout();
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline");
        }

        UH_INFO("Compute pipeline created");
    }

    void Vk_Compute_Pipeline_State::cleanup()
    {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
            UH_INFO("Compute pipeline destroyed");
        }

        m_pipeline_layout.reset();
    }

} // namespace mango::graphics::vk
