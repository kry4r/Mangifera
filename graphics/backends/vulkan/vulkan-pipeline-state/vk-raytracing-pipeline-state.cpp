#include "vk-raytracing-pipeline-state.hpp"
#include "vulkan-render-resource/vk-shader.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <vector>

namespace mango::graphics::vk
{
    Vk_Raytracing_Pipeline_State::Vk_Raytracing_Pipeline_State(VkDevice device, const Raytracing_Pipeline_Desc& desc)
        : Raytracing_Pipeline_State(desc)
        , m_device(device)
    {
        create_pipeline_layout();
        create_pipeline();
    }

    Vk_Raytracing_Pipeline_State::~Vk_Raytracing_Pipeline_State()
    {
        cleanup();
    }

    Vk_Raytracing_Pipeline_State::Vk_Raytracing_Pipeline_State(Vk_Raytracing_Pipeline_State&& other) noexcept
        : Raytracing_Pipeline_State(std::move(other))
        , m_device(other.m_device)
        , m_pipeline(other.m_pipeline)
        , m_pipeline_layout(std::move(other.m_pipeline_layout))
    {
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Raytracing_Pipeline_State::operator=(Vk_Raytracing_Pipeline_State&& other) noexcept -> Vk_Raytracing_Pipeline_State&
    {
        if (this != &other) {
            cleanup();

            Raytracing_Pipeline_State::operator=(std::move(other));

            m_device = other.m_device;
            m_pipeline = other.m_pipeline;
            m_pipeline_layout = std::move(other.m_pipeline_layout);

            other.m_pipeline = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Raytracing_Pipeline_State::create_pipeline_layout()
    {
        Pipeline_Layout_Desc layout_desc{};
        //TODO:RTemporarily simplified, reflection or explicit specification can be implemented from shaders later

        m_pipeline_layout = std::make_unique<Vk_Pipeline_Layout>(m_device, layout_desc);
    }

    void Vk_Raytracing_Pipeline_State::create_pipeline()
    {
        const auto& desc = get_desc();

        if (desc.shader_groups.empty()) {
            throw std::runtime_error("Raytracing pipeline requires at least one shader group");
        }

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;

        for (const auto& group : desc.shader_groups) {
            VkRayTracingShaderGroupCreateInfoKHR group_info{};
            group_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            group_info.generalShader = VK_SHADER_UNUSED_KHR;
            group_info.closestHitShader = VK_SHADER_UNUSED_KHR;
            group_info.anyHitShader = VK_SHADER_UNUSED_KHR;
            group_info.intersectionShader = VK_SHADER_UNUSED_KHR;

            uint32_t current_shader_index = static_cast<uint32_t>(shader_stages.size());

            // Ray generation shader (general shader)
            if (group.raygen_shader) {
                auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(group.raygen_shader);
                if (vk_shader) {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = vk_shader->get_vk_stage_flags();
                    stage.module = vk_shader->get_vk_shader_module();
                    stage.pName = vk_shader->get_entry_point();
                    shader_stages.push_back(stage);

                    group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    group_info.generalShader = current_shader_index++;
                }
            }

            // Miss shader (general shader)
            if (group.miss_shader) {
                auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(group.miss_shader);
                if (vk_shader) {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = vk_shader->get_vk_stage_flags();
                    stage.module = vk_shader->get_vk_shader_module();
                    stage.pName = vk_shader->get_entry_point();
                    shader_stages.push_back(stage);

                    if (group_info.type != VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR) {
                        group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                        group_info.generalShader = current_shader_index++;
                    }
                }
            }

            // Closest hit shader (hit group)
            if (group.closesthit_shader) {
                auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(group.closesthit_shader);
                if (vk_shader) {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = vk_shader->get_vk_stage_flags();
                    stage.module = vk_shader->get_vk_shader_module();
                    stage.pName = vk_shader->get_entry_point();
                    shader_stages.push_back(stage);

                    group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                    group_info.closestHitShader = current_shader_index++;
                }
            }

            // Any hit shader (hit group)
            if (group.anyhit_shader) {
                auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(group.anyhit_shader);
                if (vk_shader) {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = vk_shader->get_vk_stage_flags();
                    stage.module = vk_shader->get_vk_shader_module();
                    stage.pName = vk_shader->get_entry_point();
                    shader_stages.push_back(stage);

                    group_info.anyHitShader = current_shader_index++;
                }
            }

            // Callable shader (general shader)
            if (group.callable_shader) {
                auto vk_shader = std::dynamic_pointer_cast<Vk_Shader>(group.callable_shader);
                if (vk_shader) {
                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
                    stage.module = vk_shader->get_vk_shader_module();
                    stage.pName = vk_shader->get_entry_point();
                    shader_stages.push_back(stage);

                    if (group_info.type != VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR) {
                        group_info.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                        group_info.generalShader = current_shader_index++;
                    }
                }
            }

            shader_groups.push_back(group_info);
        }

        // Create raytracing pipeline
        VkRayTracingPipelineCreateInfoKHR pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.groupCount = static_cast<uint32_t>(shader_groups.size());
        pipeline_info.pGroups = shader_groups.data();
        pipeline_info.maxPipelineRayRecursionDepth = desc.max_recursion_depth;
        pipeline_info.layout = m_pipeline_layout->get_vk_pipeline_layout();
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;

        auto vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)
            vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR");

        if (!vkCreateRayTracingPipelinesKHR) {
            throw std::runtime_error("vkCreateRayTracingPipelinesKHR not available - raytracing extension may not be enabled");
        }

        if (vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                          1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create raytracing pipeline");
        }

        UH_INFO_FMT("Raytracing pipeline created ({} shader groups, max recursion depth: {})",
            shader_groups.size(), desc.max_recursion_depth);
    }

    void Vk_Raytracing_Pipeline_State::cleanup()
    {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
            UH_INFO("Raytracing pipeline destroyed");
        }

        m_pipeline_layout.reset();
    }

} // namespace mango::graphics::vk
