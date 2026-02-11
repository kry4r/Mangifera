#pragma once
#include "device.hpp"
#include "command-execution/command-pool.hpp"
#include "command-execution/command-queue.hpp"
#include "render-resource/descriptor-set.hpp"
#include "render-resource/sampler.hpp"
#include <string>

namespace mango::app
{
    struct IBL_Resources
    {
        graphics::Texture_Handle brdf_lut;
        graphics::Texture_Handle irradiance_map;
        graphics::Texture_Handle prefiltered_env;
        graphics::Texture_Handle env_cubemap;
        graphics::Sampler_Handle ibl_sampler;
        graphics::Descriptor_Set_Layout_Handle ibl_set_layout;
        graphics::Descriptor_Set_Handle ibl_set;
        bool ready = false;
    };

    class IBL_Generator
    {
    public:
        static auto generate_all(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue) -> IBL_Resources;

        static auto generate_all_from_exr(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            const std::string& exr_path) -> IBL_Resources;

    private:
        static auto load_exr_to_texture(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            const std::string& path) -> graphics::Texture_Handle;

        static auto equirect_to_cubemap(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            graphics::Texture_Handle equirect,
            graphics::Sampler_Handle sampler,
            uint32_t size) -> graphics::Texture_Handle;
        static auto generate_brdf_lut(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            uint32_t size = 512) -> graphics::Texture_Handle;

        static auto generate_sky_cubemap(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            uint32_t size = 256) -> graphics::Texture_Handle;

        static auto generate_irradiance_map(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            graphics::Texture_Handle env_map,
            graphics::Sampler_Handle sampler,
            uint32_t size = 32) -> graphics::Texture_Handle;

        static auto generate_prefiltered_env(
            graphics::Device_Handle device,
            graphics::Command_Pool_Handle pool,
            graphics::Command_Queue_Handle queue,
            graphics::Texture_Handle env_map,
            graphics::Sampler_Handle sampler,
            uint32_t size = 128,
            uint32_t mip_levels = 5) -> graphics::Texture_Handle;
    };
}
