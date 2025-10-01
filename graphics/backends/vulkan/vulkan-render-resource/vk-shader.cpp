#include "vk-shader.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Shader::Vk_Shader(VkDevice device, const Shader_Desc& desc)
        : m_device(device)
        , m_desc(desc)
    {
        if (desc.bytecode.empty()) {
            throw std::runtime_error("Shader bytecode is empty");
        }

        if (desc.entry_point.empty()) {
            throw std::runtime_error("Shader entry point is empty");
        }

        m_stage_flags = shader_type_to_stage_flags(desc.type);
        create_shader_module();
        reflect_shader();
    }

    Vk_Shader::~Vk_Shader()
    {
        cleanup();
    }

    Vk_Shader::Vk_Shader(Vk_Shader&& other) noexcept
        : m_device(other.m_device)
        , m_shader_module(other.m_shader_module)
        , m_stage_flags(other.m_stage_flags)
        , m_desc(std::move(other.m_desc))
    {
        other.m_shader_module = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Shader::operator=(Vk_Shader&& other) noexcept -> Vk_Shader&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_shader_module = other.m_shader_module;
            m_stage_flags = other.m_stage_flags;
            m_desc = std::move(other.m_desc);

            other.m_shader_module = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Shader::create_shader_module()
    {
        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = m_desc.bytecode.size() * sizeof(uint32_t);
        create_info.pCode = m_desc.bytecode.data();

        if (vkCreateShaderModule(m_device, &create_info, nullptr, &m_shader_module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan shader module");
        }

        UH_INFO_FMT("Vulkan shader module created (type: {}, size: {} bytes)",
            static_cast<int>(m_desc.type), create_info.codeSize);
    }

    void Vk_Shader::cleanup()
    {
        if (m_shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_shader_module, nullptr);
            m_shader_module = VK_NULL_HANDLE;
            UH_INFO("Vulkan shader module destroyed");
        }
    }
    void Vk_Shader::reflect_shader()
    {
        if (m_desc.bytecode.empty()) {
            UH_WARN("Cannot reflect shader: bytecode is empty");
            return;
        }

        VkShaderStageFlags stage = shader_type_to_vk_stage(m_desc.type);

        Shader_Reflector reflector;
        m_reflection_data = reflector.reflect(m_desc.bytecode, stage);

        UH_INFO_FMT("Shader reflection complete: entry point '{}', {} descriptor sets",
            m_reflection_data.entry_point, m_reflection_data.descriptor_sets.size());
    }

    VkShaderStageFlags Vk_Shader::shader_type_to_vk_stage(Shader_Type type) const
    {
        switch (type) {
            case Shader_Type::vertex:       return VK_SHADER_STAGE_VERTEX_BIT;
            case Shader_Type::fragment:     return VK_SHADER_STAGE_FRAGMENT_BIT;
            case Shader_Type::compute:      return VK_SHADER_STAGE_COMPUTE_BIT;
            case Shader_Type::geometry:     return VK_SHADER_STAGE_GEOMETRY_BIT;
            case Shader_Type::mesh:         return VK_SHADER_STAGE_MESH_BIT_EXT;
            case Shader_Type::ray_generate: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            case Shader_Type::ray_hit:      return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            case Shader_Type::ray_miss:     return VK_SHADER_STAGE_MISS_BIT_KHR;
            default:
                throw std::runtime_error("Unknown shader type");
        }
    }

    VkShaderStageFlagBits Vk_Shader::shader_type_to_stage_flags(Shader_Type type) const
    {
        switch (type) {
            case Shader_Type::vertex:
                return VK_SHADER_STAGE_VERTEX_BIT;

            case Shader_Type::fragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;

            case Shader_Type::compute:
                return VK_SHADER_STAGE_COMPUTE_BIT;

            case Shader_Type::geometry:
                return VK_SHADER_STAGE_GEOMETRY_BIT;

            case Shader_Type::mesh:
                return VK_SHADER_STAGE_MESH_BIT_EXT;

            case Shader_Type::ray_generate:
                return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

            case Shader_Type::ray_hit:
                return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            case Shader_Type::ray_miss:
                return VK_SHADER_STAGE_MISS_BIT_KHR;

            default:
                throw std::runtime_error("Unsupported shader type");
        }
    }

} // namespace mango::graphics::vk
