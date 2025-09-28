#pragma once
#include "pipeline-state.hpp"
#include "render-resource/shader.hpp"

namespace mango::graphics
{

    // 计算管线描述
    struct Compute_Pipeline_Desc
    {
        std::shared_ptr<Shader> compute_shader;
    };

    struct Compute_Pipeline_State: public Pipeline_State
    {
    public:
        explicit Compute_Pipeline_State(const Compute_Pipeline_Desc& desc)
            : m_desc(desc)
        {
        }

        Pipeline_Type get_type() const override { return Pipeline_Type::compute; }

        const Compute_Pipeline_Desc& get_desc() const { return m_desc; }

    private:
        Compute_Pipeline_Desc m_desc;
    };

    using Compute_Pipeline_Handle = std::shared_ptr<Compute_Pipeline_State>;

} // namespace mango::graphics
