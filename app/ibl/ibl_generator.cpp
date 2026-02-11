#include "ibl_generator.hpp"
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include "tinyexr/tinyexr.h"
#include "utils/shader-compiler.hpp"
#include "backends/vulkan/vk-device.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-texture.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-buffer.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-descriptor-set.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-sampler.hpp"
#include "backends/vulkan/vulkan-pipeline-state/vk-compute-pipeline-state.hpp"
#include "backends/vulkan/vulkan-command-execution/vk-command-buffer.hpp"
#include "sync/barrier.hpp"
#include "log/historiographer.hpp"
#include <filesystem>
#include <cmath>

namespace
{
    auto make_barrier(void* resource, mango::graphics::Resource_State before, mango::graphics::Resource_State after) -> mango::graphics::Barrier
    {
        mango::graphics::Barrier b{};
        b.resource = resource;
        b.before = before;
        b.after = after;
        return b;
    }

    auto ibl_shader_path(const char* filename) -> std::string
    {
        auto base = std::filesystem::path(__FILE__).parent_path().parent_path();
        return (base / "shaders" / filename).string();
    }
}

namespace mango::app
{
    auto IBL_Generator::generate_all(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue) -> IBL_Resources
    {
        IBL_Resources res{};

        // Create sampler for IBL textures
        graphics::Sampler_Desc sampler_desc{};
        sampler_desc.minFilter = graphics::Filter_Mode::linear;
        sampler_desc.magFilter = graphics::Filter_Mode::linear;
        sampler_desc.addressU = graphics::Edge_Mode::clamp;
        sampler_desc.addressV = graphics::Edge_Mode::clamp;
        sampler_desc.addressW = graphics::Edge_Mode::clamp;
        res.ibl_sampler = device->create_sampler(sampler_desc);

        if (!res.ibl_sampler) {
            UH_ERROR("Failed to create IBL sampler");
            return res;
        }

        // Step 1: Generate BRDF LUT
        res.brdf_lut = generate_brdf_lut(device, pool, queue, 512);
        if (!res.brdf_lut) {
            UH_ERROR("Failed to generate BRDF LUT");
            return res;
        }

        // Step 2: Generate procedural sky cubemap
        res.env_cubemap = generate_sky_cubemap(device, pool, queue, 256);
        if (!res.env_cubemap) {
            UH_ERROR("Failed to generate sky cubemap");
            return res;
        }

        // Step 3: Generate irradiance map
        res.irradiance_map = generate_irradiance_map(device, pool, queue, res.env_cubemap, res.ibl_sampler, 32);
        if (!res.irradiance_map) {
            UH_ERROR("Failed to generate irradiance map");
            return res;
        }

        // Step 4: Generate prefiltered environment map
        res.prefiltered_env = generate_prefiltered_env(device, pool, queue, res.env_cubemap, res.ibl_sampler, 128, 5);
        if (!res.prefiltered_env) {
            UH_ERROR("Failed to generate prefiltered env map");
            return res;
        }

        // Create IBL descriptor set layout (set=1)
        graphics::Descriptor_Set_Layout_Desc ibl_layout_desc{};
        {
            graphics::Descriptor_Binding b0{};
            b0.binding = 0;
            b0.type = graphics::Descriptor_Type::combined_image_sampler;
            b0.count = 1;
            b0.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b0);

            graphics::Descriptor_Binding b1{};
            b1.binding = 1;
            b1.type = graphics::Descriptor_Type::combined_image_sampler;
            b1.count = 1;
            b1.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b1);

            graphics::Descriptor_Binding b2{};
            b2.binding = 2;
            b2.type = graphics::Descriptor_Type::combined_image_sampler;
            b2.count = 1;
            b2.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b2);
        }

        res.ibl_set_layout = device->create_descriptor_set_layout(ibl_layout_desc);
        res.ibl_set = device->create_descriptor_set(res.ibl_set_layout);

        if (res.ibl_set) {
            graphics::Descriptor_Write irr_write{};
            irr_write.binding = 0;
            irr_write.type = graphics::Descriptor_Type::combined_image_sampler;
            irr_write.textures = { res.irradiance_map };
            irr_write.samplers = { res.ibl_sampler };

            graphics::Descriptor_Write pref_write{};
            pref_write.binding = 1;
            pref_write.type = graphics::Descriptor_Type::combined_image_sampler;
            pref_write.textures = { res.prefiltered_env };
            pref_write.samplers = { res.ibl_sampler };

            graphics::Descriptor_Write brdf_write{};
            brdf_write.binding = 2;
            brdf_write.type = graphics::Descriptor_Type::combined_image_sampler;
            brdf_write.textures = { res.brdf_lut };
            brdf_write.samplers = { res.ibl_sampler };

            res.ibl_set->update({ irr_write, pref_write, brdf_write });
        }

        res.ready = res.brdf_lut && res.irradiance_map && res.prefiltered_env && res.ibl_set;
        if (res.ready) {
            UH_INFO("IBL resources generated successfully");
        }
        return res;
    }

    auto IBL_Generator::generate_brdf_lut(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        uint32_t size) -> graphics::Texture_Handle
    {
        // Create output texture
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_2d;
        tex_desc.format = graphics::Texture_Format::rg16f;
        tex_desc.width = size;
        tex_desc.height = size;
        tex_desc.depth = 1;
        tex_desc.mip_levels = 1;
        tex_desc.arrayLayers = 1;
        tex_desc.sampled = true;
        tex_desc.storage = true;

        auto texture = device->create_texture(tex_desc);
        if (!texture) return nullptr;

        // Compile compute shader
        auto spv = graphics::utils::compile_shader_form_file(ibl_shader_path("brdf_lut.comp"), shaderc_compute_shader);
        if (spv.empty()) {
            UH_ERROR("Failed to compile brdf_lut.comp");
            return nullptr;
        }

        graphics::Shader_Desc shader_desc{};
        shader_desc.type = graphics::Shader_Type::compute;
        shader_desc.bytecode = std::move(spv);
        auto shader = device->create_shader(shader_desc);
        if (!shader) return nullptr;

        // Create descriptor set layout for the compute shader
        graphics::Descriptor_Set_Layout_Desc layout_desc{};
        graphics::Descriptor_Binding binding{};
        binding.binding = 0;
        binding.type = graphics::Descriptor_Type::storage_texture;
        binding.count = 1;
        binding.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_desc.bindings.push_back(binding);

        auto set_layout = device->create_descriptor_set_layout(layout_desc);
        auto desc_set = device->create_descriptor_set(set_layout);

        if (desc_set) {
            graphics::Descriptor_Write write{};
            write.binding = 0;
            write.type = graphics::Descriptor_Type::storage_texture;
            write.textures = { texture };
            desc_set->update({ write });
        }

        // Create compute pipeline
        graphics::Compute_Pipeline_Desc pipe_desc{};
        pipe_desc.compute_shader = shader;
        pipe_desc.descriptor_set_layouts = { set_layout };
        auto pipeline = device->create_compute_pipeline(pipe_desc);
        if (!pipeline) return nullptr;

        // Record and execute command buffer
        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) return nullptr;

        cmd->begin();

        // Transition image to GENERAL for compute write
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::undefined, graphics::Resource_State::unordered_access)); // 0=UNDEFINED, 4=GENERAL

        cmd->bind_pipeline(pipeline);
        cmd->bind_descriptor_set(0, desc_set);
        cmd->dispatch((size + 15) / 16, (size + 15) / 16, 1);

        // Transition to SHADER_READ_ONLY
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource)); // 4=GENERAL, 1=SHADER_READ_ONLY

        cmd->end();

        // Submit and wait
        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        UH_INFO("BRDF LUT generated");
        return texture;
    }

    auto IBL_Generator::generate_sky_cubemap(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        uint32_t size) -> graphics::Texture_Handle
    {
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_cube;
        tex_desc.format = graphics::Texture_Format::rgba16f;
        tex_desc.width = size;
        tex_desc.height = size;
        tex_desc.depth = 1;
        tex_desc.mip_levels = 1;
        tex_desc.arrayLayers = 6;
        tex_desc.sampled = true;
        tex_desc.storage = true;

        auto texture = device->create_texture(tex_desc);
        if (!texture) return nullptr;

        auto spv = graphics::utils::compile_shader_form_file(ibl_shader_path("procedural_sky.comp"), shaderc_compute_shader);
        if (spv.empty()) {
            UH_ERROR("Failed to compile procedural_sky.comp");
            return nullptr;
        }

        graphics::Shader_Desc shader_desc{};
        shader_desc.type = graphics::Shader_Type::compute;
        shader_desc.bytecode = std::move(spv);
        auto shader = device->create_shader(shader_desc);
        if (!shader) return nullptr;

        graphics::Descriptor_Set_Layout_Desc layout_desc{};
        graphics::Descriptor_Binding binding{};
        binding.binding = 0;
        binding.type = graphics::Descriptor_Type::storage_texture;
        binding.count = 1;
        binding.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
        layout_desc.bindings.push_back(binding);

        auto set_layout = device->create_descriptor_set_layout(layout_desc);
        auto desc_set = device->create_descriptor_set(set_layout);

        if (desc_set) {
            graphics::Descriptor_Write write{};
            write.binding = 0;
            write.type = graphics::Descriptor_Type::storage_texture;
            write.textures = { texture };
            desc_set->update({ write });
        }

        graphics::Compute_Pipeline_Desc pipe_desc{};
        pipe_desc.compute_shader = shader;
        pipe_desc.descriptor_set_layouts = { set_layout };
        auto pipeline = device->create_compute_pipeline(pipe_desc);
        if (!pipeline) return nullptr;

        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) return nullptr;

        cmd->begin();
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));
        cmd->bind_pipeline(pipeline);
        cmd->bind_descriptor_set(0, desc_set);
        cmd->dispatch((size + 15) / 16, (size + 15) / 16, 6);
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        cmd->end();

        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        UH_INFO("Procedural sky cubemap generated");
        return texture;
    }

    auto IBL_Generator::generate_irradiance_map(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        graphics::Texture_Handle env_map,
        graphics::Sampler_Handle sampler,
        uint32_t size) -> graphics::Texture_Handle
    {
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_cube;
        tex_desc.format = graphics::Texture_Format::rgba16f;
        tex_desc.width = size;
        tex_desc.height = size;
        tex_desc.depth = 1;
        tex_desc.mip_levels = 1;
        tex_desc.arrayLayers = 6;
        tex_desc.sampled = true;
        tex_desc.storage = true;

        auto texture = device->create_texture(tex_desc);
        if (!texture) return nullptr;

        auto spv = graphics::utils::compile_shader_form_file(ibl_shader_path("irradiance.comp"), shaderc_compute_shader);
        if (spv.empty()) {
            UH_ERROR("Failed to compile irradiance.comp");
            return nullptr;
        }

        graphics::Shader_Desc shader_desc{};
        shader_desc.type = graphics::Shader_Type::compute;
        shader_desc.bytecode = std::move(spv);
        auto shader = device->create_shader(shader_desc);
        if (!shader) return nullptr;

        graphics::Descriptor_Set_Layout_Desc layout_desc{};
        {
            graphics::Descriptor_Binding b0{};
            b0.binding = 0;
            b0.type = graphics::Descriptor_Type::combined_image_sampler;
            b0.count = 1;
            b0.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b0);

            graphics::Descriptor_Binding b1{};
            b1.binding = 1;
            b1.type = graphics::Descriptor_Type::storage_texture;
            b1.count = 1;
            b1.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b1);
        }

        auto set_layout = device->create_descriptor_set_layout(layout_desc);
        auto desc_set = device->create_descriptor_set(set_layout);

        if (desc_set) {
            graphics::Descriptor_Write env_write{};
            env_write.binding = 0;
            env_write.type = graphics::Descriptor_Type::combined_image_sampler;
            env_write.textures = { env_map };
            env_write.samplers = { sampler };

            graphics::Descriptor_Write out_write{};
            out_write.binding = 1;
            out_write.type = graphics::Descriptor_Type::storage_texture;
            out_write.textures = { texture };

            desc_set->update({ env_write, out_write });
        }

        graphics::Compute_Pipeline_Desc pipe_desc{};
        pipe_desc.compute_shader = shader;
        pipe_desc.descriptor_set_layouts = { set_layout };
        auto pipeline = device->create_compute_pipeline(pipe_desc);
        if (!pipeline) return nullptr;

        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) return nullptr;

        cmd->begin();
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));
        cmd->bind_pipeline(pipeline);
        cmd->bind_descriptor_set(0, desc_set);
        cmd->dispatch((size + 15) / 16, (size + 15) / 16, 6);
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        cmd->end();

        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        UH_INFO("Irradiance map generated");
        return texture;
    }

    auto IBL_Generator::generate_prefiltered_env(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        graphics::Texture_Handle env_map,
        graphics::Sampler_Handle sampler,
        uint32_t size,
        uint32_t mip_levels) -> graphics::Texture_Handle
    {
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_cube;
        tex_desc.format = graphics::Texture_Format::rgba16f;
        tex_desc.width = size;
        tex_desc.height = size;
        tex_desc.depth = 1;
        tex_desc.mip_levels = mip_levels;
        tex_desc.arrayLayers = 6;
        tex_desc.sampled = true;
        tex_desc.storage = true;

        auto texture = device->create_texture(tex_desc);
        if (!texture) return nullptr;

        auto spv = graphics::utils::compile_shader_form_file(ibl_shader_path("prefilter.comp"), shaderc_compute_shader);
        if (spv.empty()) {
            UH_ERROR("Failed to compile prefilter.comp");
            return nullptr;
        }

        graphics::Shader_Desc shader_desc{};
        shader_desc.type = graphics::Shader_Type::compute;
        shader_desc.bytecode = std::move(spv);
        auto shader = device->create_shader(shader_desc);
        if (!shader) return nullptr;

        graphics::Descriptor_Set_Layout_Desc layout_desc{};
        {
            graphics::Descriptor_Binding b0{};
            b0.binding = 0;
            b0.type = graphics::Descriptor_Type::combined_image_sampler;
            b0.count = 1;
            b0.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b0);

            graphics::Descriptor_Binding b1{};
            b1.binding = 1;
            b1.type = graphics::Descriptor_Type::storage_texture;
            b1.count = 1;
            b1.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b1);
        }

        graphics::Push_Constant_Range pc_range{};
        pc_range.offset = 0;
        pc_range.size = sizeof(float) * 2; // roughness + resolution
        pc_range.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;

        auto set_layout = device->create_descriptor_set_layout(layout_desc);

        graphics::Compute_Pipeline_Desc pipe_desc{};
        pipe_desc.compute_shader = shader;
        pipe_desc.descriptor_set_layouts = { set_layout };
        pipe_desc.push_constants = { pc_range };
        auto pipeline = device->create_compute_pipeline(pipe_desc);
        if (!pipeline) return nullptr;

        // Get Vulkan handles for per-mip image view creation
        auto vk_texture = std::dynamic_pointer_cast<graphics::vk::Vk_Texture>(texture);
        auto vk_env = std::dynamic_pointer_cast<graphics::vk::Vk_Texture>(env_map);
        auto vk_sampler_ptr = std::dynamic_pointer_cast<graphics::vk::Vk_Sampler>(sampler);
        auto vk_device = std::dynamic_pointer_cast<graphics::vk::Vk_Device>(device);
        if (!vk_texture || !vk_env || !vk_sampler_ptr || !vk_device) return nullptr;

        VkDevice vk_dev = vk_device->get_vk_device();

        // Create per-mip image views for storage access
        std::vector<VkImageView> mip_views(mip_levels);
        for (uint32_t m = 0; m < mip_levels; ++m) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = vk_texture->get_vk_image();
            view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            view_info.format = vk_texture->get_vk_format();
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = m;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 6;
            if (vkCreateImageView(vk_dev, &view_info, nullptr, &mip_views[m]) != VK_SUCCESS) {
                UH_ERROR_FMT("Failed to create per-mip image view for mip {}", m);
                for (uint32_t j = 0; j < m; ++j) vkDestroyImageView(vk_dev, mip_views[j], nullptr);
                return nullptr;
            }
        }

        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) {
            for (auto v : mip_views) vkDestroyImageView(vk_dev, v, nullptr);
            return nullptr;
        }

        // Create one descriptor set per mip level (can't update a bound descriptor set during recording)
        std::vector<graphics::Descriptor_Set_Handle> mip_desc_sets(mip_levels);
        for (uint32_t m = 0; m < mip_levels; ++m) {
            mip_desc_sets[m] = device->create_descriptor_set(set_layout);
            auto vk_ds = std::dynamic_pointer_cast<graphics::vk::Vk_Descriptor_Set>(mip_desc_sets[m]);
            if (!vk_ds) {
                for (auto v : mip_views) vkDestroyImageView(vk_dev, v, nullptr);
                return nullptr;
            }

            VkDescriptorImageInfo env_info{};
            env_info.sampler = vk_sampler_ptr->get_vk_sampler();
            env_info.imageView = vk_env->get_vk_image_view();
            env_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo storage_info{};
            storage_info.imageView = mip_views[m];
            storage_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = vk_ds->get_vk_descriptor_set();
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &env_info;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = vk_ds->get_vk_descriptor_set();
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &storage_info;

            vkUpdateDescriptorSets(vk_dev, 2, writes, 0, nullptr);
        }

        cmd->begin();
        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));
        cmd->bind_pipeline(pipeline);

        for (uint32_t m = 0; m < mip_levels; ++m) {
            cmd->bind_descriptor_set(0, mip_desc_sets[m]);

            uint32_t mip_size = (std::max)(size >> m, 1u);
            float roughness = (mip_levels > 1) ? static_cast<float>(m) / static_cast<float>(mip_levels - 1) : 0.0f;
            float pc_data[2] = { roughness, static_cast<float>(mip_size) };
            cmd->push_constants(0, sizeof(pc_data), pc_data);
            cmd->dispatch((mip_size + 15) / 16, (mip_size + 15) / 16, 6);
        }

        cmd->resource_barrier(make_barrier(texture.get(), graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        cmd->end();

        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        // Cleanup per-mip views
        for (auto v : mip_views) vkDestroyImageView(vk_dev, v, nullptr);

        UH_INFO_FMT("Prefiltered environment map generated ({} mip levels)", mip_levels);
        return texture;
    }

    auto IBL_Generator::load_exr_to_texture(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        const std::string& path) -> graphics::Texture_Handle
    {
        float* rgba = nullptr;
        int width = 0, height = 0;
        const char* err = nullptr;

        int ret = LoadEXR(&rgba, &width, &height, path.c_str(), &err);
        if (ret != TINYEXR_SUCCESS || !rgba) {
            if (err) {
                UH_ERROR_FMT("Failed to load EXR '{}': {}", path, err);
                FreeEXRErrorMessage(err);
            } else {
                UH_ERROR_FMT("Failed to load EXR '{}'", path);
            }
            return nullptr;
        }

        UH_INFO_FMT("Loaded EXR: {}x{} from '{}'", width, height, path);

        // Create rgba32f texture
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_2d;
        tex_desc.format = graphics::Texture_Format::rgba32f;
        tex_desc.width = static_cast<uint32_t>(width);
        tex_desc.height = static_cast<uint32_t>(height);
        tex_desc.depth = 1;
        tex_desc.mip_levels = 1;
        tex_desc.arrayLayers = 1;
        tex_desc.sampled = true;

        auto texture = device->create_texture(tex_desc);
        if (!texture) {
            free(rgba);
            return nullptr;
        }

        // Create staging buffer and keep it alive until after GPU submit
        std::size_t data_size = static_cast<std::size_t>(width) * height * 4 * sizeof(float);

        auto vk_device = std::dynamic_pointer_cast<graphics::vk::Vk_Device>(device);
        auto staging = std::make_shared<graphics::vk::Vk_Buffer>(
            vk_device->get_vk_device(), vk_device->get_vk_physical_device(),
            graphics::Buffer_Desc{ data_size, graphics::Buffer_Type::storage, graphics::Memory_Type::cpu2gpu });
        staging->upload(rgba, data_size);
        free(rgba);
        rgba = nullptr;

        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) {
            return nullptr;
        }

        auto vk_tex = std::dynamic_pointer_cast<graphics::vk::Vk_Texture>(texture);

        cmd->begin();

        // Transition to transfer dst
        vk_tex->transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy staging buffer to texture
        cmd->copy_buffer_to_texture(staging, texture,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height), 0, 0);

        // Transition to shader read
        vk_tex->transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        cmd->end();

        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        // staging buffer destroyed here after GPU is idle
        UH_INFO("EXR texture uploaded to GPU");
        return texture;
    }

    auto IBL_Generator::equirect_to_cubemap(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        graphics::Texture_Handle equirect,
        graphics::Sampler_Handle sampler,
        uint32_t size) -> graphics::Texture_Handle
    {
        // Create output cubemap
        graphics::Texture_Desc tex_desc{};
        tex_desc.dimension = graphics::Texture_Kind::tex_cube;
        tex_desc.format = graphics::Texture_Format::rgba16f;
        tex_desc.width = size;
        tex_desc.height = size;
        tex_desc.depth = 1;
        tex_desc.mip_levels = 1;
        tex_desc.arrayLayers = 6;
        tex_desc.sampled = true;
        tex_desc.storage = true;

        auto cubemap = device->create_texture(tex_desc);
        if (!cubemap) return nullptr;

        // Compile equirect_to_cubemap compute shader
        auto spv = graphics::utils::compile_shader_form_file(
            ibl_shader_path("equirect_to_cubemap.comp"), shaderc_compute_shader);
        if (spv.empty()) {
            UH_ERROR("Failed to compile equirect_to_cubemap.comp");
            return nullptr;
        }

        graphics::Shader_Desc shader_desc{};
        shader_desc.type = graphics::Shader_Type::compute;
        shader_desc.bytecode = std::move(spv);
        auto shader = device->create_shader(shader_desc);
        if (!shader) return nullptr;

        // Descriptor set layout: binding 0 = equirect sampler, binding 1 = cubemap storage
        graphics::Descriptor_Set_Layout_Desc layout_desc{};
        {
            graphics::Descriptor_Binding b0{};
            b0.binding = 0;
            b0.type = graphics::Descriptor_Type::combined_image_sampler;
            b0.count = 1;
            b0.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b0);

            graphics::Descriptor_Binding b1{};
            b1.binding = 1;
            b1.type = graphics::Descriptor_Type::storage_texture;
            b1.count = 1;
            b1.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
            layout_desc.bindings.push_back(b1);
        }

        auto set_layout = device->create_descriptor_set_layout(layout_desc);
        auto desc_set = device->create_descriptor_set(set_layout);

        if (desc_set) {
            graphics::Descriptor_Write eq_write{};
            eq_write.binding = 0;
            eq_write.type = graphics::Descriptor_Type::combined_image_sampler;
            eq_write.textures = { equirect };
            eq_write.samplers = { sampler };

            graphics::Descriptor_Write out_write{};
            out_write.binding = 1;
            out_write.type = graphics::Descriptor_Type::storage_texture;
            out_write.textures = { cubemap };

            desc_set->update({ eq_write, out_write });
        }

        graphics::Compute_Pipeline_Desc pipe_desc{};
        pipe_desc.compute_shader = shader;
        pipe_desc.descriptor_set_layouts = { set_layout };
        auto pipeline = device->create_compute_pipeline(pipe_desc);
        if (!pipeline) return nullptr;

        auto cmd = pool->allocate_command_buffer(graphics::Command_Buffer_Level::primary);
        if (!cmd) return nullptr;

        cmd->begin();
        cmd->resource_barrier(make_barrier(cubemap.get(), graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));
        cmd->bind_pipeline(pipeline);
        cmd->bind_descriptor_set(0, desc_set);
        cmd->dispatch((size + 15) / 16, (size + 15) / 16, 6);
        cmd->resource_barrier(make_barrier(cubemap.get(), graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        cmd->end();

        graphics::Submit_Info submit{};
        submit.command_buffers.push_back(cmd);
        queue->submit(submit, nullptr);
        queue->wait_idle();

        UH_INFO("Equirectangular to cubemap conversion complete");
        return cubemap;
    }

    auto IBL_Generator::generate_all_from_exr(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        const std::string& exr_path) -> IBL_Resources
    {
        IBL_Resources res{};

        // Create sampler
        graphics::Sampler_Desc sampler_desc{};
        sampler_desc.minFilter = graphics::Filter_Mode::linear;
        sampler_desc.magFilter = graphics::Filter_Mode::linear;
        sampler_desc.addressU = graphics::Edge_Mode::clamp;
        sampler_desc.addressV = graphics::Edge_Mode::clamp;
        sampler_desc.addressW = graphics::Edge_Mode::clamp;
        res.ibl_sampler = device->create_sampler(sampler_desc);
        if (!res.ibl_sampler) {
            UH_ERROR("Failed to create IBL sampler");
            return res;
        }

        // Step 1: Generate BRDF LUT (same as procedural path)
        res.brdf_lut = generate_brdf_lut(device, pool, queue, 512);
        if (!res.brdf_lut) {
            UH_ERROR("Failed to generate BRDF LUT");
            return res;
        }

        // Step 2: Load EXR as equirectangular 2D texture
        auto equirect_tex = load_exr_to_texture(device, pool, queue, exr_path);
        if (!equirect_tex) {
            UH_ERROR("Failed to load EXR, falling back to procedural sky");
            return generate_all(device, pool, queue);
        }

        // Step 3: Convert equirect to cubemap
        res.env_cubemap = equirect_to_cubemap(device, pool, queue, equirect_tex, res.ibl_sampler, 512);
        if (!res.env_cubemap) {
            UH_ERROR("Failed to convert equirect to cubemap");
            return res;
        }

        // Step 4: Generate irradiance map from cubemap
        res.irradiance_map = generate_irradiance_map(device, pool, queue, res.env_cubemap, res.ibl_sampler, 32);
        if (!res.irradiance_map) {
            UH_ERROR("Failed to generate irradiance map");
            return res;
        }

        // Step 5: Generate prefiltered environment map
        res.prefiltered_env = generate_prefiltered_env(device, pool, queue, res.env_cubemap, res.ibl_sampler, 128, 5);
        if (!res.prefiltered_env) {
            UH_ERROR("Failed to generate prefiltered env map");
            return res;
        }

        // Step 6: Create IBL descriptor set (same layout as procedural path)
        graphics::Descriptor_Set_Layout_Desc ibl_layout_desc{};
        {
            graphics::Descriptor_Binding b0{};
            b0.binding = 0;
            b0.type = graphics::Descriptor_Type::combined_image_sampler;
            b0.count = 1;
            b0.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b0);

            graphics::Descriptor_Binding b1{};
            b1.binding = 1;
            b1.type = graphics::Descriptor_Type::combined_image_sampler;
            b1.count = 1;
            b1.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b1);

            graphics::Descriptor_Binding b2{};
            b2.binding = 2;
            b2.type = graphics::Descriptor_Type::combined_image_sampler;
            b2.count = 1;
            b2.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
            ibl_layout_desc.bindings.push_back(b2);
        }

        res.ibl_set_layout = device->create_descriptor_set_layout(ibl_layout_desc);
        res.ibl_set = device->create_descriptor_set(res.ibl_set_layout);

        if (res.ibl_set) {
            graphics::Descriptor_Write irr_write{};
            irr_write.binding = 0;
            irr_write.type = graphics::Descriptor_Type::combined_image_sampler;
            irr_write.textures = { res.irradiance_map };
            irr_write.samplers = { res.ibl_sampler };

            graphics::Descriptor_Write pref_write{};
            pref_write.binding = 1;
            pref_write.type = graphics::Descriptor_Type::combined_image_sampler;
            pref_write.textures = { res.prefiltered_env };
            pref_write.samplers = { res.ibl_sampler };

            graphics::Descriptor_Write brdf_write{};
            brdf_write.binding = 2;
            brdf_write.type = graphics::Descriptor_Type::combined_image_sampler;
            brdf_write.textures = { res.brdf_lut };
            brdf_write.samplers = { res.ibl_sampler };

            res.ibl_set->update({ irr_write, pref_write, brdf_write });
        }

        res.ready = res.brdf_lut && res.irradiance_map && res.prefiltered_env && res.ibl_set;
        if (res.ready) {
            UH_INFO("IBL resources generated from EXR successfully");
        }
        return res;
    }
}
