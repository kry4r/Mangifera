#pragma once
#include "pipeline-state.hpp"
#include "render-resource/buffer.hpp"
#include "render-resource/sampler.hpp"
#include "render-resource/shader.hpp"
#include "render-resource/texture.hpp"
#include "render-pass/render-pass.hpp"

namespace mango::graphics
{
    struct Vertex_Attribute
    {
        std::string semantic;   // POSITION, NORMAL, TEXCOORD0
        uint32_t    location;
        uint32_t    offset;
        uint32_t    stride;
    };

    struct Rasterizer_State
    {
        bool    cull_enable = true;
        bool    wireframe = false;
        bool    front_ccw = true;
    };

    struct Depth_Stencil_State
    {
        bool    depth_test_enable = true;
        bool    depth_write_enable = true;
        bool    stencil_enable = false;
    };

    struct Blend_State
    {
        bool    blend_enable = false;
    };

    struct Render_Target_Desc
    {
        uint32_t format; // via VkFormat / DXGI_FORMAT
    };

    struct Graphics_Pipeline_Desc
    {
        // optional shader stages
        std::shared_ptr<Shader>   vertex_shader;      // VS
        std::shared_ptr<Shader>   tess_control_shader; // HS
        std::shared_ptr<Shader>   tess_eval_shader;    // DS
        std::shared_ptr<Shader>   geometry_shader;     // GS
        std::shared_ptr<Shader>   task_shader;         // Task
        std::shared_ptr<Shader>   mesh_shader;         // Mesh
        std::shared_ptr<Shader>   fragment_shader;     // FS

        std::vector<Vertex_Attribute> vertex_attributes;
        Rasterizer_State              rasterizer_state;
        Depth_Stencil_State           depth_stencil_state;
        Blend_State                   blend_state;

        std::vector<Render_Target_Desc> render_targets;
        uint32_t depth_stencil_format = 0; // optional

        std::shared_ptr<Render_Pass> render_pass;
        uint32_t subpass = 0;
    };

    struct Graphics_Pipeline_State: public Pipeline_State
    {
    public:
        explicit Graphics_Pipeline_State(const Graphics_Pipeline_Desc& desc)
            : m_desc(desc)
        {
        }

        Pipeline_Type get_type() const override { return Pipeline_Type::graphics; }

        const Graphics_Pipeline_Desc& get_desc() const { return m_desc; }

    private:
        Graphics_Pipeline_Desc m_desc;
    };

    using Graphics_Pipeline_Handle = std::shared_ptr<Graphics_Pipeline_State>;

} // namespace mango::graphics
