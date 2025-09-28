
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

    struct Sampler_Desc {
        Filter_Mode minFilter = Filter_Mode::linear;
        Filter_Mode magFilter = Filter_Mode::linear;
        Edge_Mode addressU = Edge_Mode::clamp;
        Edge_Mode addressV = Edge_Mode::clamp;
        Edge_Mode addressW = Edge_Mode::clamp;
    };

    class Sampler {
    public:
        virtual ~Sampler() = default;
        virtual auto getDesc() const -> Sampler_Desc& = 0;
    };
} // namespace mango::graphics
