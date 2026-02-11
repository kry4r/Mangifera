#pragma once
#include <memory>
namespace mango::graphics
{
    enum struct Filter_Mode {
        nearest,
        linear,
    };

    enum struct Edge_Mode {
        repeat,
        clamp,
    };

    enum struct Border_Color {
        float_opaque_black,
        float_opaque_white,
    };

    struct Sampler_Desc {
        Filter_Mode minFilter = Filter_Mode::linear;
        Filter_Mode magFilter = Filter_Mode::linear;
        Edge_Mode addressU = Edge_Mode::clamp;
        Edge_Mode addressV = Edge_Mode::clamp;
        Edge_Mode addressW = Edge_Mode::clamp;
        bool comparison_enable = false;
        Border_Color border_color = Border_Color::float_opaque_black;
    };

    class Sampler {
    public:
        virtual ~Sampler() = default;
        virtual auto getDesc() const -> const Sampler_Desc& = 0;
    };

    using Sampler_Handle = std::shared_ptr<Sampler>;
} // namespace mango::graphics
