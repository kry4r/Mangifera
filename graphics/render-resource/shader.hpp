#pragma once
#include <string>
#include <vector>
#include <memory>
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
        std::string entry_point = "main";
        std::vector<uint32_t> bytecode; // SPIR-V binary
    };

    class Shader {
    public:
        virtual ~Shader() = default;
        virtual const Shader_Desc& getDesc() const = 0;
    };

    using Shader_Handle = std::shared_ptr<Shader>;
}
