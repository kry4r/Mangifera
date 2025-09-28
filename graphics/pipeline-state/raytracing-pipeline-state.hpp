#pragma once
#include "pipeline-state.hpp"
#include "render-resource/shader.hpp"

namespace mango::graphics
{
    struct Raytracing_Shader_Group
    {
        std::shared_ptr<Shader> raygen_shader;     // required
        std::shared_ptr<Shader> miss_shader;       // required
        std::shared_ptr<Shader> closesthit_shader; // optional
        std::shared_ptr<Shader> anyhit_shader;     // optional
        std::shared_ptr<Shader> callable_shader;   // optional
    };

    struct Raytracing_Pipeline_Desc
    {
        std::vector<Raytracing_Shader_Group> shader_groups;
        uint32_t max_recursion_depth = 1;
    };

    struct Raytracing_Pipeline_State : public Pipeline_State
    {
    public:
        explicit Raytracing_Pipeline_State(const Raytracing_Pipeline_Desc& desc)
            : m_desc(desc)
        {
        }

        Pipeline_Type get_type() const override { return Pipeline_Type::raytracing; }

        const Raytracing_Pipeline_Desc& get_desc() const { return m_desc; }

    private:
        Raytracing_Pipeline_Desc m_desc;
    };

    using Raytracing_Pipeline_Handle = std::shared_ptr<Raytracing_Pipeline_State>;

} // namespace mango::graphics
