#include <shaderc/shaderc.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace mango::graphics::utils
{
    std::vector<uint32_t> compile_shader_form_string(const std::string& source, shaderc_shader_kind kind, const std::string& source_name = "shader.glsl",
                                                bool optimize = true)
    {
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        if(optimize) {
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
        }

        shaderc::SpvCompilationResult module =
            compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

        if(module.GetCompilationStatus() != shaderc_compilation_status_success) {
            std::cerr << "Shader compilation failed: " << module.GetErrorMessage() << std::endl;
            return {};
        }

        return {module.cbegin(), module.cend()};
    }

    inline std::vector<uint32_t> compile_shader_form_file(const std::string& filepath, shaderc_shader_kind kind,
        bool optimize = true)
    {
        std::ifstream file(filepath, std::ios::in);
        if(!file.is_open()) {
            std::cerr << "Failed to open shader file: " << filepath << std::endl;
            return {};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        return compile_shader_form_string(source, kind, filepath, optimize);
    }
}
