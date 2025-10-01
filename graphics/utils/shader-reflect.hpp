#pragma once
#include <vulkan/vulkan.h>
#include "spirv_reflect.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace mango::graphics::vk
{
    // Descriptor binding info
    struct Reflected_Descriptor_Binding
    {
        uint32_t set;
        uint32_t binding;
        VkDescriptorType type;
        uint32_t count;
        VkShaderStageFlags stage_flags;
        std::string name;
    };

    // Descriptor set info
    struct Reflected_Descriptor_Set
    {
        uint32_t set;
        std::vector<Reflected_Descriptor_Binding> bindings;
    };

    // Push constant info
    struct Reflected_Push_Constant
    {
        uint32_t offset;
        uint32_t size;
        VkShaderStageFlags stage_flags;
        std::string name;
    };

    // Vertex input attribute (vertex shader only)
    struct Reflected_Vertex_Input
    {
        uint32_t location;
        VkFormat format;
        std::string name;
    };

    // Fragment shader output (fragment shader only)
    struct Reflected_Fragment_Output
    {
        uint32_t location;
        VkFormat format;
        std::string name;
    };

    // Compute shader workgroup size (compute shader only)
    struct Reflected_Workgroup_Size
    {
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };

    // Geometry shader info (geometry shader only)
    struct Reflected_Geometry_Info
    {
        SpvExecutionMode input_primitive;   // Point, Line, Triangle
        SpvExecutionMode output_primitive;  // PointList, LineStrip, TriangleStrip
        uint32_t max_output_vertices;
        uint32_t invocations;
    };

    // Tessellation shader info (tessellation shader only)
    struct Reflected_Tessellation_Info
    {
        SpvExecutionMode partition_mode;    // Triangles, Quads, Isolines
        SpvExecutionMode spacing_mode;      // Equal, FractionalEven, FractionalOdd
        SpvExecutionMode vertex_order;      // Cw, Ccw
        uint32_t output_vertices;           // For control shader
    };

    // Shader reflection result
    struct Shader_Reflection_Data
    {
        std::vector<Reflected_Descriptor_Set> descriptor_sets;
        std::vector<Reflected_Push_Constant> push_constants;
        VkShaderStageFlags stage;
        std::string entry_point;

        // Stage-specific data
        std::vector<Reflected_Vertex_Input> vertex_inputs;           // Vertex shader
        std::vector<Reflected_Fragment_Output> fragment_outputs;     // Fragment shader
        Reflected_Workgroup_Size workgroup_size;                     // Compute shader
        Reflected_Geometry_Info geometry_info;                       // Geometry shader
        Reflected_Tessellation_Info tessellation_info;               // Tessellation shader
    };

    class Shader_Reflector
    {
    public:
        Shader_Reflector() = default;
        ~Shader_Reflector() = default;

        // Reflect from SPIR-V bytecode
        Shader_Reflection_Data reflect(const std::vector<uint32_t>& spirv_code, VkShaderStageFlags stage);

        // Merge multiple shader reflections
        static Shader_Reflection_Data merge_reflection_data(
            const std::vector<Shader_Reflection_Data>& reflections);

    private:
        void reflect_vertex_inputs(SpvReflectShaderModule& module, Shader_Reflection_Data& result);
        void reflect_fragment_outputs(SpvReflectShaderModule& module, Shader_Reflection_Data& result);
        void reflect_compute_workgroup_size(SpvReflectShaderModule& module, Shader_Reflection_Data& result);
        void reflect_geometry_info(SpvReflectShaderModule& module, Shader_Reflection_Data& result);
        void reflect_tessellation_info(SpvReflectShaderModule& module, Shader_Reflection_Data& result);

        VkDescriptorType spirv_descriptor_type_to_vk(SpvReflectDescriptorType type) const;
        VkFormat spirv_format_to_vk(SpvReflectFormat format) const;
    };

} // namespace mango::graphics::vk
