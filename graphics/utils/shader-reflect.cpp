#include "shader-reflect.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <algorithm>

namespace mango::graphics::vk
{
    Shader_Reflection_Data Shader_Reflector::reflect(const std::vector<uint32_t>& spirv_code, VkShaderStageFlags stage)
    {
        Shader_Reflection_Data result;
        result.stage = stage;

        // Create SPIRV-Reflect module
        SpvReflectShaderModule module;
        SpvReflectResult spv_result = spvReflectCreateShaderModule(
            spirv_code.size() * sizeof(uint32_t),
            spirv_code.data(),
            &module
        );

        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to create SPIRV-Reflect module");
        }

        // Entry point name
        result.entry_point = module.entry_point_name;

        // Reflect descriptor bindings (common for all stages)
        uint32_t descriptor_set_count = 0;
        spv_result = spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, nullptr);
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            spvReflectDestroyShaderModule(&module);
            throw std::runtime_error("Failed to enumerate descriptor sets");
        }

        std::vector<SpvReflectDescriptorSet*> descriptor_sets(descriptor_set_count);
        spv_result = spvReflectEnumerateDescriptorSets(&module, &descriptor_set_count, descriptor_sets.data());
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            spvReflectDestroyShaderModule(&module);
            throw std::runtime_error("Failed to enumerate descriptor sets");
        }

        // Process each descriptor set
        for (uint32_t i = 0; i < descriptor_set_count; ++i) {
            const SpvReflectDescriptorSet* set = descriptor_sets[i];

            Reflected_Descriptor_Set reflected_set;
            reflected_set.set = set->set;

            for (uint32_t j = 0; j < set->binding_count; ++j) {
                const SpvReflectDescriptorBinding* binding = set->bindings[j];

                Reflected_Descriptor_Binding reflected_binding;
                reflected_binding.set = set->set;
                reflected_binding.binding = binding->binding;
                reflected_binding.type = spirv_descriptor_type_to_vk(binding->descriptor_type);
                reflected_binding.count = binding->count;
                reflected_binding.stage_flags = stage;
                reflected_binding.name = binding->name ? binding->name : "";

                reflected_set.bindings.push_back(reflected_binding);
            }

            result.descriptor_sets.push_back(reflected_set);
        }

        // Reflect push constants (common for all stages)
        uint32_t push_constant_count = 0;
        spv_result = spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, nullptr);
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            spvReflectDestroyShaderModule(&module);
            throw std::runtime_error("Failed to enumerate push constants");
        }

        std::vector<SpvReflectBlockVariable*> push_constants(push_constant_count);
        spv_result = spvReflectEnumeratePushConstantBlocks(&module, &push_constant_count, push_constants.data());
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            spvReflectDestroyShaderModule(&module);
            throw std::runtime_error("Failed to enumerate push constants");
        }

        for (uint32_t i = 0; i < push_constant_count; ++i) {
            const SpvReflectBlockVariable* pc = push_constants[i];

            Reflected_Push_Constant reflected_pc;
            reflected_pc.offset = pc->offset;
            reflected_pc.size = pc->size;
            reflected_pc.stage_flags = stage;
            reflected_pc.name = pc->name ? pc->name : "";

            result.push_constants.push_back(reflected_pc);
        }

        // Stage-specific reflections
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            reflect_vertex_inputs(module, result);
        } else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            reflect_fragment_outputs(module, result);
        } else if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
            reflect_compute_workgroup_size(module, result);
        } else if (stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
            reflect_geometry_info(module, result);
        } else if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
            stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
            reflect_tessellation_info(module, result);
        }

        spvReflectDestroyShaderModule(&module);

        UH_INFO_FMT("Shader reflection complete: stage={}, {} descriptor sets, {} push constants",
            stage, result.descriptor_sets.size(), result.push_constants.size());

        return result;
    }

    void Shader_Reflector::reflect_vertex_inputs(SpvReflectShaderModule& module, Shader_Reflection_Data& result)
    {
        uint32_t input_count = 0;
        SpvReflectResult spv_result = spvReflectEnumerateInputVariables(&module, &input_count, nullptr);
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to enumerate vertex input variables");
        }

        std::vector<SpvReflectInterfaceVariable*> inputs(input_count);
        spv_result = spvReflectEnumerateInputVariables(&module, &input_count, inputs.data());
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to enumerate vertex input variables");
        }

        for (uint32_t i = 0; i < input_count; ++i) {
            const SpvReflectInterfaceVariable* input = inputs[i];

            // Skip built-in variables (like gl_VertexIndex, gl_InstanceIndex)
            if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
                continue;
            }

            Reflected_Vertex_Input reflected_input;
            reflected_input.location = input->location;
            reflected_input.format = spirv_format_to_vk(input->format);
            reflected_input.name = input->name ? input->name : "";

            result.vertex_inputs.push_back(reflected_input);
        }

        UH_INFO_FMT("Vertex shader: {} input attributes", result.vertex_inputs.size());
    }

    void Shader_Reflector::reflect_fragment_outputs(SpvReflectShaderModule& module, Shader_Reflection_Data& result)
    {
        uint32_t output_count = 0;
        SpvReflectResult spv_result = spvReflectEnumerateOutputVariables(&module, &output_count, nullptr);
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to enumerate fragment output variables");
        }

        std::vector<SpvReflectInterfaceVariable*> outputs(output_count);
        spv_result = spvReflectEnumerateOutputVariables(&module, &output_count, outputs.data());
        if (spv_result != SPV_REFLECT_RESULT_SUCCESS) {
            throw std::runtime_error("Failed to enumerate fragment output variables");
        }

        for (uint32_t i = 0; i < output_count; ++i) {
            const SpvReflectInterfaceVariable* output = outputs[i];

            // Skip built-in variables (like gl_FragDepth)
            if (output->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
                continue;
            }

            Reflected_Fragment_Output reflected_output;
            reflected_output.location = output->location;
            reflected_output.format = spirv_format_to_vk(output->format);
            reflected_output.name = output->name ? output->name : "";

            result.fragment_outputs.push_back(reflected_output);
        }

        UH_INFO_FMT("Fragment shader: {} output attachments", result.fragment_outputs.size());
    }

    void Shader_Reflector::reflect_compute_workgroup_size(SpvReflectShaderModule& module, Shader_Reflection_Data& result)
    {
        // Get workgroup size from entry point
        const SpvReflectEntryPoint* entry_point = spvReflectGetEntryPoint(&module, module.entry_point_name);
        if (!entry_point) {
            throw std::runtime_error("Failed to get compute shader entry point");
        }

        result.workgroup_size.x = entry_point->local_size.x;
        result.workgroup_size.y = entry_point->local_size.y;
        result.workgroup_size.z = entry_point->local_size.z;

        UH_INFO_FMT("Compute shader: workgroup size = ({}, {}, {})",
            result.workgroup_size.x, result.workgroup_size.y, result.workgroup_size.z);
    }

    void Shader_Reflector::reflect_geometry_info(SpvReflectShaderModule& module, Shader_Reflection_Data& result)
    {
        const SpvReflectEntryPoint* entry_point = spvReflectGetEntryPoint(&module, module.entry_point_name);
        if (!entry_point) {
            throw std::runtime_error("Failed to get geometry shader entry point");
        }

        // Default values
        result.geometry_info.input_primitive = SpvExecutionModeMax;
        result.geometry_info.output_primitive = SpvExecutionModeMax;
        result.geometry_info.max_output_vertices = 0;
        result.geometry_info.invocations = 1;

        // Parse execution modes - they are just enum values
        for (uint32_t i = 0; i < entry_point->execution_mode_count; ++i) {
            SpvExecutionMode mode = entry_point->execution_modes[i];

            switch (mode) {
                case SpvExecutionModeInputPoints:
                case SpvExecutionModeInputLines:
                case SpvExecutionModeInputLinesAdjacency:
                case SpvExecutionModeTriangles:
                case SpvExecutionModeInputTrianglesAdjacency:
                    result.geometry_info.input_primitive = mode;
                    break;

                case SpvExecutionModeOutputPoints:
                case SpvExecutionModeOutputLineStrip:
                case SpvExecutionModeOutputTriangleStrip:
                    result.geometry_info.output_primitive = mode;
                    break;

                case SpvExecutionModeOutputVertices:
                    // Get the actual vertex count from local_size field or other source
                    // SPIRV-Reflect doesn't directly expose operands in execution_modes array
                    // You may need to parse it from the shader code or use shader properties
                    result.geometry_info.max_output_vertices = entry_point->local_size.x; // Placeholder
                    break;

                case SpvExecutionModeInvocations:
                    result.geometry_info.invocations = 1; // Default, may need to parse from code
                    break;

                default:
                    break;
            }
        }

        UH_INFO_FMT("Geometry shader: max_vertices={}, invocations={}",
            result.geometry_info.max_output_vertices, result.geometry_info.invocations);
    }

    void Shader_Reflector::reflect_tessellation_info(SpvReflectShaderModule& module, Shader_Reflection_Data& result)
    {
        const SpvReflectEntryPoint* entry_point = spvReflectGetEntryPoint(&module, module.entry_point_name);
        if (!entry_point) {
            throw std::runtime_error("Failed to get tessellation shader entry point");
        }

        // Default values
        result.tessellation_info.partition_mode = SpvExecutionModeMax;
        result.tessellation_info.spacing_mode = SpvExecutionModeMax;
        result.tessellation_info.vertex_order = SpvExecutionModeMax;
        result.tessellation_info.output_vertices = 0;

        // Parse execution modes
        for (uint32_t i = 0; i < entry_point->execution_mode_count; ++i) {
            SpvExecutionMode mode = entry_point->execution_modes[i];

            switch (mode) {
                case SpvExecutionModeTriangles:
                case SpvExecutionModeQuads:
                case SpvExecutionModeIsolines:
                    result.tessellation_info.partition_mode = mode;
                    break;

                case SpvExecutionModeSpacingEqual:
                case SpvExecutionModeSpacingFractionalEven:
                case SpvExecutionModeSpacingFractionalOdd:
                    result.tessellation_info.spacing_mode = mode;
                    break;

                case SpvExecutionModeVertexOrderCw:
                case SpvExecutionModeVertexOrderCcw:
                    result.tessellation_info.vertex_order = mode;
                    break;

                case SpvExecutionModeOutputVertices:
                    // For tessellation control shaders, output vertices might be in local_size
                    result.tessellation_info.output_vertices = entry_point->local_size.x; // Placeholder
                    break;

                default:
                    break;
            }
        }

        UH_INFO_FMT("Tessellation shader: output_vertices={}", result.tessellation_info.output_vertices);
    }

    Shader_Reflection_Data Shader_Reflector::merge_reflection_data(
        const std::vector<Shader_Reflection_Data>& reflections)
    {
        Shader_Reflection_Data merged;

        // Map for merging descriptor sets
        std::unordered_map<uint32_t, Reflected_Descriptor_Set> set_map;

        // Merge all shader reflection data
        for (const auto& reflection : reflections) {
            // Merge descriptor sets
            for (const auto& set : reflection.descriptor_sets) {
                auto it = set_map.find(set.set);
                if (it == set_map.end()) {
                    set_map[set.set] = set;
                } else {
                    // Merge bindings in the same set
                    for (const auto& binding : set.bindings) {
                        auto binding_it = std::find_if(
                            it->second.bindings.begin(),
                            it->second.bindings.end(),
                            [&] (const Reflected_Descriptor_Binding& b) {
                                return b.binding == binding.binding;
                            }
                        );

                        if (binding_it == it->second.bindings.end()) {
                            it->second.bindings.push_back(binding);
                        } else {
                            // Merge stage flags
                            binding_it->stage_flags |= binding.stage_flags;
                        }
                    }
                }
            }

            // Merge push constants
            for (const auto& pc : reflection.push_constants) {
                auto pc_it = std::find_if(
                    merged.push_constants.begin(),
                    merged.push_constants.end(),
                    [&] (const Reflected_Push_Constant& p) {
                        return p.offset == pc.offset && p.size == pc.size;
                    }
                );

                if (pc_it == merged.push_constants.end()) {
                    merged.push_constants.push_back(pc);
                } else {
                    // Merge stage flags
                    pc_it->stage_flags |= pc.stage_flags;
                }
            }

            // Copy stage-specific data
            if (reflection.stage == VK_SHADER_STAGE_VERTEX_BIT) {
                merged.vertex_inputs = reflection.vertex_inputs;
            } else if (reflection.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
                merged.fragment_outputs = reflection.fragment_outputs;
            } else if (reflection.stage == VK_SHADER_STAGE_COMPUTE_BIT) {
                merged.workgroup_size = reflection.workgroup_size;
            } else if (reflection.stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
                merged.geometry_info = reflection.geometry_info;
            } else if (reflection.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
                reflection.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
                merged.tessellation_info = reflection.tessellation_info;
            }
        }

        // Convert map back to vector
        for (auto& pair : set_map) {
            merged.descriptor_sets.push_back(pair.second);
        }

        // Sort descriptor sets by set number
        std::sort(merged.descriptor_sets.begin(), merged.descriptor_sets.end(),
            [] (const Reflected_Descriptor_Set& a, const Reflected_Descriptor_Set& b) {
                return a.set < b.set;
            }
        );

        return merged;
    }

    VkDescriptorType Shader_Reflector::spirv_descriptor_type_to_vk(SpvReflectDescriptorType type) const
    {
        switch (type) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            default:
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    VkFormat Shader_Reflector::spirv_format_to_vk(SpvReflectFormat format) const
    {
        switch (format) {
            case SPV_REFLECT_FORMAT_R32_SFLOAT:           return VK_FORMAT_R32_SFLOAT;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:        return VK_FORMAT_R32G32_SFLOAT;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:     return VK_FORMAT_R32G32B32_SFLOAT;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:  return VK_FORMAT_R32G32B32A32_SFLOAT;
            case SPV_REFLECT_FORMAT_R32_SINT:             return VK_FORMAT_R32_SINT;
            case SPV_REFLECT_FORMAT_R32G32_SINT:          return VK_FORMAT_R32G32_SINT;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:       return VK_FORMAT_R32G32B32_SINT;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:    return VK_FORMAT_R32G32B32A32_SINT;
            case SPV_REFLECT_FORMAT_R32_UINT:             return VK_FORMAT_R32_UINT;
            case SPV_REFLECT_FORMAT_R32G32_UINT:          return VK_FORMAT_R32G32_UINT;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:       return VK_FORMAT_R32G32B32_UINT;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:    return VK_FORMAT_R32G32B32A32_UINT;
            case SPV_REFLECT_FORMAT_R16_SFLOAT:           return VK_FORMAT_R16_SFLOAT;
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT:        return VK_FORMAT_R16G16_SFLOAT;
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:     return VK_FORMAT_R16G16B16_SFLOAT;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:  return VK_FORMAT_R16G16B16A16_SFLOAT;
            default:                                       return VK_FORMAT_UNDEFINED;
        }
    }

} // namespace mango::graphics::vk
