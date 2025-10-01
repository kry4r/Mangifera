#pragma once
#include "render-resource/shader.hpp"
#include "utils/shader-reflect.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Shader : public Shader
    {
    public:
        Vk_Shader(VkDevice device, const Shader_Desc& desc);
        ~Vk_Shader() override;

        Vk_Shader(const Vk_Shader&) = delete;
        Vk_Shader& operator=(const Vk_Shader&) = delete;
        Vk_Shader(Vk_Shader&& other) noexcept;
        Vk_Shader& operator=(Vk_Shader&& other) noexcept;

        // Shader interface
        const Shader_Desc& getDesc() const override { return m_desc; }

        // Vulkan specific
        auto get_vk_shader_module() const -> VkShaderModule { return m_shader_module; }
        auto get_vk_stage_flags() const -> VkShaderStageFlagBits { return m_stage_flags; }
        auto get_entry_point() const -> const char* { return m_desc.entry_point.c_str(); }

        const Shader_Reflection_Data& get_reflection_data() const { return m_reflection_data; }

    private:
        void create_shader_module();
        void reflect_shader();
        void cleanup();

        VkShaderStageFlagBits shader_type_to_stage_flags(Shader_Type type) const;
        VkShaderStageFlags shader_type_to_vk_stage(Shader_Type type) const;

        VkDevice m_device = VK_NULL_HANDLE;
        VkShaderModule m_shader_module = VK_NULL_HANDLE;
        VkShaderStageFlagBits m_stage_flags = VK_SHADER_STAGE_ALL;
        Shader_Desc m_desc;
        Shader_Reflection_Data m_reflection_data;
    };

} // namespace mango::graphics::vk
