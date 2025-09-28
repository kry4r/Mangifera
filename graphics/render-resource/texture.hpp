#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace mango::graphics
{
    enum struct Texture_Kind {
        none,
        window,         // This is a marker used only by Framebuffer.
        tex_2d,
        tex_3d,
        tex_cube,
        tex_2d_array,
    };

    enum struct Texture_Format {
        invalid,

        r8,         // color-renderable, texture-filterable.
        r16f,       // color-renderable1, texture-filterable.
        r32f,       // color-renderable1, texture-filterable1.
        r8u,        // color-renderable. Store untegers without normalization.
        r16u,       // color-renderable. Store untegers without normalization.
        r32u,       // color-renderable. Store untegers without normalization.
        r8i,        // color-renderable. Store integers without normalization.
        r16i,       // color-renderable. Store integers without normalization.
        r32i,       // color-renderable. Store integers without normalization.

        rg8,        // color-renderable, texture-filterable.
        rg16f,      // color-renderable1, texture-filterable.
        rg32f,      // color-renderable1, texture-filterable1.
        rg8u,       // color-renderable. Store untegers without normalization.
        rg16u,      // color-renderable. Store untegers without normalization.
        rg32u,      // color-renderable. Store untegers without normalization.
        rg8i,       // color-renderable. Store integers without normalization.
        rg16i,      // color-renderable. Store integers without normalization.
        rg32i,      // color-renderable. Store integers without normalization.

        rgb8,       // color-renderable, texture-filterable.
        sRGB,       // texture-filterable. "r", "g", "b" is 8-bit sRGB values.

        rgba8,      // color-renderable, texture-filterable.
        rgba16f,    // color-renderable1, texture-filterable.
        rgba32f,    // color-renderable1, texture-filterable1.
        sRGB_alpha8,    // color-renderable, texture-filterable. "r", "g", "b" is 8-bit sRGB values. "alpha8" is a linear 8-bit value.
        rgb10_alpha2,   // color-renderable, texture-filterable.
        rgba8u,     // color-renderable. Store untegers without normalization.
        rgba16u,    // color-renderable. Store untegers without normalization.
        rgba32u,    // color-renderable. Store untegers without normalization.
        rgba8i,     // color-renderable. Store integers without normalization.
        rgba16i,    // color-renderable. Store integers without normalization.
        rgba32i,    // color-renderable. Store integers without normalization.

        // These are ALL the depth/stencil formats that WebGL2 supports.
        depth24,    // color-renderable.
        depth32f,   // color-renderable.

        depth24_stencil8,   // color-renderable.
        depth32f_stencil8,  // color-renderable.
    };

    struct Texture_Desc {
        Texture_Kind dimension = Texture_Kind::tex_2d;
        Texture_Format format = Texture_Format::rgba8;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mip_levels = 1;
        uint32_t arrayLayers = 1;
        bool sampled = true;
        bool render_target = false;
    };

    class Texture {
    public:
        virtual ~Texture() = default;
        virtual auto getDesc() const -> Texture_Desc& = 0;
    };

    using Texture_Handle = std::shared_ptr<Texture>;
}
