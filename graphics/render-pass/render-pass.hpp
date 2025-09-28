#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "render-resource/texture.hpp"

namespace mango::graphics
{
    struct Attachment_Desc
    {
        std::shared_ptr<Texture> texture; // the image view / texture
        uint32_t load_op = 0;             // 0 = load, 1 = clear
        uint32_t store_op = 0;            // 0 = store, 1 = discard
        uint32_t initial_state = 0;       // Resource state at begin
        uint32_t final_state = 0;         // Resource state at end
    };

    struct Subpass_Desc
    {
        std::vector<uint32_t> color_attachments; // indices into attachments
        int32_t depth_stencil_attachment = -1;   // optional
    };

    struct Render_Pass_Desc
    {
        std::vector<Attachment_Desc> attachments;
        std::vector<Subpass_Desc> subpasses;
    };

    class Render_Pass
    {
    public:
        virtual ~Render_Pass() = default;

        virtual const Render_Pass_Desc& get_desc() const = 0;
    };

    using Render_Pass_Handle = std::shared_ptr<Render_Pass>;

} // namespace mango::graphics
