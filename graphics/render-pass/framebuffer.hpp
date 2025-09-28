#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "render-pass.hpp"
#include "render-resource/texture.hpp"

namespace mango::graphics
{
    struct Framebuffer_Desc
    {
        std::shared_ptr<Render_Pass> render_pass;
        std::vector<std::shared_ptr<Texture>> attachments;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t layers = 1;
    };

    class Framebuffer
    {
    public:
        virtual ~Framebuffer() = default;

        virtual const Framebuffer_Desc& get_desc() const = 0;
    };

    using Framebuffer_Handle = std::shared_ptr<Framebuffer>;

} // namespace mango::graphics
