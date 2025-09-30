#include "vk-graphics-pipeline-state.hpp"
#include "vulkan-render-resource/vk-shader.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <vector>

namespace mango::graphics::vk
{
    Vk_Graphics_Pipeline_State::Vk_Graphics_Pipeline_State(VkDevice device, const Graphics_Pipeline_Desc& desc, VkRenderPass render_pass)
        : Graphics_Pipeline_State(desc)
        , m_device(device)
    {
        create_pipeline_layout();
        create_pipeline(render_pass);
    }

    Vk_Graphics_Pipeline_State::~Vk_Graphics_Pipeline_State()
    {
        cleanup();
    }

    Vk_Graphics_Pipeline_State::Vk_Graphics_Pipeline_State(Vk_Graphics_Pipeline_State&& other) noexcept
        : Graphics_Pipeline_State(std::move(other))
        , m_device(other.m_device)
        , m_pipeline(other.m_pipeline)
        , m_pipeline_layout(std::move(other.m_pipeline_layout))
    {
        other.m_pipeline = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Graphics_Pipeline_State::operator=(Vk_Graphics_Pipeline_State&& other) noexcept -> Vk_Graphics_Pipeline_State&
    {
        if (this != &other) {
            cleanup();

            Graphics_Pipeline_State::operator=(std::move(other));

            m_device = other.m_device;
            m_pipeline = other.m_pipeline;
            m_pipeline_layout = std::move(other.m_pipeline_layout);

            other.m_pipeline = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Graphics_Pipeline_State::create_pipeline_layout()
    {
        Pipeline_Layout_Desc layout_desc{};
        //TODO:RTemporarily simplified, reflection or explicit specification can be implemented from shaders later

        m_pipeline_layout = std::make_unique<Vk_Pipeline_Layout>(m_device, layout_desc);
    }

    void Vk_Graphics_Pipeline_State::create_pipeline(VkRenderPass render_pass)
    {
        const auto& desc = get_desc();

        // Shader stages
        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

        if (desc.vertex_shader) {
            auto vk_vs = std::dynamic_pointer_cast<Vk_Shader>(desc.vertex_shader);
            if (vk_vs) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_vs->get_vk_stage_flags();
                stage_info.module = vk_vs->get_vk_shader_module();
                stage_info.pName = vk_vs->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        if (desc.fragment_shader) {
            auto vk_fs = std::dynamic_pointer_cast<Vk_Shader>(desc.fragment_shader);
            if (vk_fs) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_fs->get_vk_stage_flags();
                stage_info.module = vk_fs->get_vk_shader_module();
                stage_info.pName = vk_fs->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        // Geometry shader
        if (desc.geometry_shader) {
            auto vk_gs = std::dynamic_pointer_cast<Vk_Shader>(desc.geometry_shader);
            if (vk_gs) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_gs->get_vk_stage_flags();
                stage_info.module = vk_gs->get_vk_shader_module();
                stage_info.pName = vk_gs->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        // Tessellation shaders
        if (desc.tess_control_shader) {
            auto vk_tcs = std::dynamic_pointer_cast<Vk_Shader>(desc.tess_control_shader);
            if (vk_tcs) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_tcs->get_vk_stage_flags();
                stage_info.module = vk_tcs->get_vk_shader_module();
                stage_info.pName = vk_tcs->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        if (desc.tess_eval_shader) {
            auto vk_tes = std::dynamic_pointer_cast<Vk_Shader>(desc.tess_eval_shader);
            if (vk_tes) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_tes->get_vk_stage_flags();
                stage_info.module = vk_tes->get_vk_shader_module();
                stage_info.pName = vk_tes->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        // Mesh shaders
        if (desc.mesh_shader) {
            auto vk_ms = std::dynamic_pointer_cast<Vk_Shader>(desc.mesh_shader);
            if (vk_ms) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = vk_ms->get_vk_stage_flags();
                stage_info.module = vk_ms->get_vk_shader_module();
                stage_info.pName = vk_ms->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        if (desc.task_shader) {
            auto vk_ts = std::dynamic_pointer_cast<Vk_Shader>(desc.task_shader);
            if (vk_ts) {
                VkPipelineShaderStageCreateInfo stage_info{};
                stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage_info.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
                stage_info.module = vk_ts->get_vk_shader_module();
                stage_info.pName = vk_ts->get_entry_point();
                shader_stages.push_back(stage_info);
            }
        }

        // Vertex input state
        std::vector<VkVertexInputBindingDescription> binding_descriptions;
        std::vector<VkVertexInputAttributeDescription> attribute_descriptions;

        for (const auto& attr : desc.vertex_attributes) {
            VkVertexInputAttributeDescription vk_attr{};
            vk_attr.location = attr.location;
            vk_attr.binding = 0;
            vk_attr.format = VK_FORMAT_R32G32B32_SFLOAT; // TODO:Simplified implementation, requires judgment based on actual circumstances
            vk_attr.offset = attr.offset;
            attribute_descriptions.push_back(vk_attr);
        }

        if (!attribute_descriptions.empty()) {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = desc.vertex_attributes[0].stride;
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            binding_descriptions.push_back(binding);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = static_cast<uint32_t>(binding_descriptions.size());
        vertex_input_info.pVertexBindingDescriptions = binding_descriptions.data();
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        // Viewport state (dynamic)
        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        // Rasterization state
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = desc.rasterizer_state.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = desc.rasterizer_state.cull_enable ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        rasterizer.frontFace = desc.rasterizer_state.front_ccw ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        // Multisample state
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Depth-stencil state
        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = desc.depth_stencil_state.depth_test_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthWriteEnable = desc.depth_stencil_state.depth_write_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
        depth_stencil.stencilTestEnable = desc.depth_stencil_state.stencil_enable ? VK_TRUE : VK_FALSE;

        // Color blend state
        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment.blendEnable = desc.blend_state.blend_enable ? VK_TRUE : VK_FALSE;

        if (desc.blend_state.blend_enable) {
            color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.attachmentCount = 1;
        color_blending.pAttachments = &color_blend_attachment;

        // Dynamic state
        std::vector<VkDynamicState> dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        // Create graphics pipeline
        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = m_pipeline_layout->get_vk_pipeline_layout();
        pipeline_info.renderPass = render_pass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }

        UH_INFO("Graphics pipeline created");
    }

    void Vk_Graphics_Pipeline_State::cleanup()
    {
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        m_pipeline_layout.reset();
    }

} // namespace mango::graphics::vk
