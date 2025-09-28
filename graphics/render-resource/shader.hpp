#include <string>
#include <vector>
namespace mango::graphics
{
    enum class Shader_Type {
        vertex,
        fragment,
        compute,
        geometry,
        mesh,
        ray_generate,
        ray_hit,
        ray_miss
    };

    struct Shader_Desc {
        Shader_Type type;
        std::string entryPoint = "main";
        std::vector<uint32_t> bytecode; // SPIR-V binary
    };

    class Shader {
    public:
        virtual ~Shader() = default;
        virtual const Shader_Desc& getDesc() const = 0;
    };
}
