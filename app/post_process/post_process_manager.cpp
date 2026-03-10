#include "post_process_manager.hpp"
#include "render_features/passes/post/bloom_pass.hpp"
#include "render_features/passes/post/tonemap_pass.hpp"
#include "render_features/passes/sensor_export_pass.hpp"
#include "utils/shader-compiler.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-buffer.hpp"
#include "backends/vulkan/vk-device.hpp"
#include "sync/barrier.hpp"
#include "log/historiographer.hpp"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cstring>

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

    auto post_shader_path(const char* filename) -> std::string
    {
        auto base = std::filesystem::path(__FILE__).parent_path().parent_path();
        return (base / "shaders" / "post" / filename).string();
    }

    constexpr float MIN_LOG_LUM = -10.0f;
    constexpr float MAX_LOG_LUM = 2.0f;
    constexpr float LOG_LUM_RANGE = MAX_LOG_LUM - MIN_LOG_LUM;

    // Helper to create a compute pipeline from a shader file
    struct Pipeline_Builder
    {
        mango::graphics::Device_Handle device;

        struct Binding_Info {
            uint32_t binding;
            mango::graphics::Descriptor_Type type;
        };

        auto build(const char* shader_file,
                    const std::vector<Binding_Info>& bindings,
                    uint32_t pc_size)
            -> std::tuple<mango::graphics::Compute_Pipeline_Handle,
                          mango::graphics::Descriptor_Set_Layout_Handle,
                          mango::graphics::Descriptor_Set_Handle>
        {
            auto spv = mango::graphics::utils::compile_shader_form_file(
                post_shader_path(shader_file), shaderc_compute_shader);
            if (spv.empty()) {
                UH_ERROR_FMT("Failed to compile {}", shader_file);
                return {};
            }

            mango::graphics::Shader_Desc sd{};
            sd.type = mango::graphics::Shader_Type::compute;
            sd.bytecode = std::move(spv);
            auto shader = device->create_shader(sd);
            if (!shader) return {};

            mango::graphics::Descriptor_Set_Layout_Desc ld{};
            for (auto& b : bindings) {
                mango::graphics::Descriptor_Binding db{};
                db.binding = b.binding;
                db.type = b.type;
                db.count = 1;
                db.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
                ld.bindings.push_back(db);
            }

            auto layout = device->create_descriptor_set_layout(ld);
            auto desc_set = device->create_descriptor_set(layout);

            mango::graphics::Compute_Pipeline_Desc pd{};
            pd.compute_shader = shader;
            pd.descriptor_set_layouts = { layout };
            if (pc_size > 0) {
                mango::graphics::Push_Constant_Range pc{};
                pc.offset = 0;
                pc.size = pc_size;
                pc.shader_stages = VK_SHADER_STAGE_COMPUTE_BIT;
                pd.push_constants = { pc };
            }

            auto pipeline = device->create_compute_pipeline(pd);
            return { pipeline, layout, desc_set };
        }
    };

    using DT = mango::graphics::Descriptor_Type;
}

namespace mango::app
{
    void Post_Process_Manager::init(
        graphics::Device_Handle device,
        graphics::Command_Pool_Handle pool,
        graphics::Command_Queue_Handle queue,
        uint32_t width, uint32_t height)
    {
        device_ = device;
        pool_ = pool;
        queue_ = queue;
        width_ = width;
        height_ = height;

        graphics::Sampler_Desc sd{};
        sd.minFilter = graphics::Filter_Mode::linear;
        sd.magFilter = graphics::Filter_Mode::linear;
        sd.addressU = graphics::Edge_Mode::clamp;
        sd.addressV = graphics::Edge_Mode::clamp;
        linear_sampler_ = device_->create_sampler(sd);

        // Comparison sampler for shadow map sampling in volumetric pass
        graphics::Sampler_Desc csd{};
        csd.minFilter = graphics::Filter_Mode::linear;
        csd.magFilter = graphics::Filter_Mode::linear;
        csd.addressU = graphics::Edge_Mode::clamp;
        csd.addressV = graphics::Edge_Mode::clamp;
        csd.comparison_enable = true;
        csd.border_color = graphics::Border_Color::float_opaque_white;
        comparison_sampler_ = device_->create_sampler(csd);

        create_textures();
        create_lut_texture();
        create_exposure_resources();
        create_pipelines();

        ready_ = tonemap_pipeline_ && output_texture_;
        UH_INFO_FMT("Post-process manager initialized ({}x{}), ready={}", width, height, ready_);
    }

    void Post_Process_Manager::resize(uint32_t width, uint32_t height)
    {
        if (width == width_ && height == height_) return;
        width_ = width;
        height_ = height;
        create_textures();
        bound_hdr_input_ = nullptr;
        UH_INFO_FMT("Post-process manager resized to {}x{}", width, height);
    }

    void Post_Process_Manager::create_textures()
    {
        auto make_tex = [&](uint32_t w, uint32_t h, graphics::Texture_Format fmt) -> graphics::Texture_Handle {
            graphics::Texture_Desc td{};
            td.dimension = graphics::Texture_Kind::tex_2d;
            td.format = fmt;
            td.width = w;
            td.height = h;
            td.depth = 1;
            td.mip_levels = 1;
            td.arrayLayers = 1;
            td.sampled = true;
            td.storage = true;
            return device_->create_texture(td);
        };

        output_texture_ = make_tex(width_, height_, graphics::Texture_Format::rgba16f);
        post_a_ = make_tex(width_, height_, graphics::Texture_Format::rgba16f);
        post_b_ = make_tex(width_, height_, graphics::Texture_Format::rgba16f);

        uint32_t hw = (std::max)(width_ / 2, 1u);
        uint32_t hh = (std::max)(height_ / 2, 1u);
        ssao_half_ = make_tex(hw, hh, graphics::Texture_Format::r16f);
        ssao_full_ = make_tex(width_, height_, graphics::Texture_Format::r16f);

        // Bloom mip chain
        uint32_t bw = hw, bh = hh;
        for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
            bloom_chain_[i] = make_tex(bw, bh, graphics::Texture_Format::rgba16f);
            bw = (std::max)(bw / 2, 1u);
            bh = (std::max)(bh / 2, 1u);
        }

        // SSR textures
        ssr_half_ = make_tex(hw, hh, graphics::Texture_Format::rgba16f);
        ssr_full_ = make_tex(width_, height_, graphics::Texture_Format::rgba16f);

        // Volumetric light (quarter-res)
        uint32_t qw = (std::max)(width_ / 4, 1u);
        uint32_t qh = (std::max)(height_ / 4, 1u);
        volumetric_ = make_tex(qw, qh, graphics::Texture_Format::rgba16f);

        // Hi-Z mip chain (r32f)
        uint32_t hzw = hw, hzh = hh;
        for (uint32_t i = 0; i < HIZ_MIP_COUNT; i++) {
            hiz_mips_[i] = make_tex(hzw, hzh, graphics::Texture_Format::r32f);
            hzw = (std::max)(hzw / 2, 1u);
            hzh = (std::max)(hzh / 2, 1u);
        }
    }

    void Post_Process_Manager::create_lut_texture()
    {
        graphics::Texture_Desc td{};
        td.dimension = graphics::Texture_Kind::tex_3d;
        td.format = graphics::Texture_Format::rgba16f;
        td.width = 32;
        td.height = 32;
        td.depth = 32;
        td.mip_levels = 1;
        td.arrayLayers = 1;
        td.sampled = true;
        td.storage = true;
        lut_3d_ = device_->create_texture(td);
    }

    void Post_Process_Manager::generate_lut(graphics::Command_Buffer_Handle cmd)
    {
        if (!lut_gen_pipeline_ || !lut_gen_set_ || !lut_3d_) return;

        // Check if LUT params changed
        bool dirty = lut_dirty_ ||
            settings_.color_temperature != prev_temperature_ ||
            settings_.color_contrast != prev_contrast_ ||
            settings_.color_saturation != prev_saturation_ ||
            settings_.color_preset != prev_preset_;

        if (!dirty) return;

        prev_temperature_ = settings_.color_temperature;
        prev_contrast_ = settings_.color_contrast;
        prev_saturation_ = settings_.color_saturation;
        prev_preset_ = settings_.color_preset;
        lut_dirty_ = false;

        cmd->resource_barrier(make_barrier(lut_3d_.get(),
            graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

        cmd->bind_pipeline(lut_gen_pipeline_);
        cmd->bind_descriptor_set(0, lut_gen_set_);

        LUT_Gen_PC pc{};
        pc.temperature = settings_.color_temperature;
        pc.contrast = settings_.color_contrast;
        pc.saturation = settings_.color_saturation;
        pc.preset = static_cast<uint32_t>(settings_.color_preset);
        cmd->push_constants(0, sizeof(pc), &pc);
        // 32/8=4, 32/8=4, 32/4=8 workgroups
        cmd->dispatch(4, 4, 8);

        cmd->resource_barrier(make_barrier(lut_3d_.get(),
            graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
    }

    void Post_Process_Manager::create_exposure_resources()
    {
        graphics::Buffer_Desc ed{};
        ed.size = sizeof(float) * 2;
        ed.usage = graphics::Buffer_Type::storage;
        ed.memory = graphics::Memory_Type::cpu2gpu;
        exposure_buffer_ = device_->create_buffer(ed);

        if (exposure_buffer_) {
            float init[2] = { 0.8f, 0.8f };
            auto vk = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(exposure_buffer_);
            if (vk) vk->upload(init, sizeof(init));
        }

        graphics::Buffer_Desc hd{};
        hd.size = sizeof(uint32_t) * 256;
        hd.usage = graphics::Buffer_Type::storage;
        hd.memory = graphics::Memory_Type::gpu_only;
        histogram_buffer_ = device_->create_buffer(hd);
    }

    void Post_Process_Manager::create_pipelines()
    {
        Pipeline_Builder pb{ device_ };

        // SSAO
        {
            auto [p, l, s] = pb.build("ssao.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::storage_texture}
            }, sizeof(SSAO_PC));
            ssao_pipeline_ = p; ssao_set_layout_ = l; ssao_set_ = s;
            if (p) UH_INFO("SSAO pipeline created");
        }

        // SSAO Upsample
        {
            auto [p, l, s] = pb.build("ssao_upsample.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::storage_texture}
            }, sizeof(SSAO_Up_PC));
            ssao_up_pipeline_ = p; ssao_up_set_layout_ = l; ssao_up_set_ = s;
            if (p) UH_INFO("SSAO upsample pipeline created");
        }

        // Composite
        {
            auto [p, l, s] = pb.build("composite.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::combined_image_sampler},
                {3, DT::storage_texture}
            }, sizeof(Composite_PC));
            composite_pipeline_ = p; composite_set_layout_ = l; composite_set_ = s;
            if (p) UH_INFO("Composite pipeline created");
        }

        // Bloom downsample
        {
            auto [p, l, s] = pb.build("bloom_downsample.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::storage_texture}
            }, sizeof(Bloom_Down_PC));
            bloom_down_pipeline_ = p; bloom_down_set_layout_ = l;
            // Create per-mip descriptor sets
            for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
                bloom_down_sets_[i] = device_->create_descriptor_set(l);
            }
            if (p) UH_INFO("Bloom downsample pipeline created");
        }

        // Bloom upsample
        {
            auto [p, l, s] = pb.build("bloom_upsample.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::storage_texture}
            }, sizeof(Bloom_Up_PC));
            bloom_up_pipeline_ = p; bloom_up_set_layout_ = l;
            for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
                bloom_up_sets_[i] = device_->create_descriptor_set(l);
            }
            if (p) UH_INFO("Bloom upsample pipeline created");
        }

        // Bloom composite
        {
            auto [p, l, s] = pb.build("bloom_composite.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::storage_texture}
            }, sizeof(Bloom_Comp_PC));
            bloom_comp_pipeline_ = p; bloom_comp_set_layout_ = l; bloom_comp_set_ = s;
            if (p) UH_INFO("Bloom composite pipeline created");
        }

        // Histogram
        {
            auto [p, l, s] = pb.build("luminance_histogram.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::storage_buffer}
            }, sizeof(Histogram_PC));
            histogram_pipeline_ = p; histogram_set_layout_ = l; histogram_set_ = s;
            if (p) UH_INFO("Histogram pipeline created");
        }

        // Histogram average
        {
            auto [p, l, s] = pb.build("histogram_average.comp", {
                {0, DT::storage_buffer},
                {1, DT::storage_buffer}
            }, sizeof(Histogram_Avg_PC));
            histogram_avg_pipeline_ = p; histogram_avg_set_layout_ = l; histogram_avg_set_ = s;

            if (s && histogram_buffer_ && exposure_buffer_) {
                graphics::Descriptor_Write hw{};
                hw.binding = 0;
                hw.type = DT::storage_buffer;
                hw.buffers = { histogram_buffer_ };
                hw.buffer_offsets = { 0 };
                hw.buffer_ranges = { sizeof(uint32_t) * 256 };

                graphics::Descriptor_Write ew{};
                ew.binding = 1;
                ew.type = DT::storage_buffer;
                ew.buffers = { exposure_buffer_ };
                ew.buffer_offsets = { 0 };
                ew.buffer_ranges = { sizeof(float) * 2 };

                s->update({ hw, ew });
            }
            if (p) UH_INFO("Histogram average pipeline created");
        }

        // Tone mapping (binding 3 = color_lut sampler3D)
        {
            auto [p, l, s] = pb.build("tonemapping.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::storage_texture},
                {2, DT::storage_buffer},
                {3, DT::combined_image_sampler}
            }, sizeof(Tonemap_PC));
            tonemap_pipeline_ = p; tonemap_set_layout_ = l; tonemap_set_ = s;
            if (p) UH_INFO("Tone mapping pipeline created");
        }

        // LUT generate
        {
            auto [p, l, s] = pb.build("lut_generate.comp", {
                {0, DT::storage_texture}
            }, sizeof(LUT_Gen_PC));
            lut_gen_pipeline_ = p; lut_gen_set_layout_ = l; lut_gen_set_ = s;

            // Bind lut_3d_ to the descriptor set
            if (s && lut_3d_) {
                graphics::Descriptor_Write w0{};
                w0.binding = 0;
                w0.type = DT::storage_texture;
                w0.textures = { lut_3d_ };
                s->update({ w0 });
            }
            if (p) UH_INFO("LUT generate pipeline created");
        }

        // Hi-Z generate
        {
            auto [p, l, s] = pb.build("hiz_generate.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::storage_texture}
            }, sizeof(HiZ_PC));
            hiz_pipeline_ = p; hiz_set_layout_ = l;
            for (uint32_t i = 0; i < HIZ_MIP_COUNT; i++) {
                hiz_sets_[i] = device_->create_descriptor_set(l);
            }
            if (p) UH_INFO("Hi-Z pipeline created");
        }

        // SSR trace
        {
            auto [p, l, s] = pb.build("ssr_trace.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::combined_image_sampler},
                {3, DT::combined_image_sampler},
                {4, DT::storage_texture}
            }, sizeof(SSR_Trace_PC));
            ssr_trace_pipeline_ = p; ssr_trace_set_layout_ = l; ssr_trace_set_ = s;
            if (p) UH_INFO("SSR trace pipeline created");
        }

        // SSR upsample
        {
            auto [p, l, s] = pb.build("ssr_upsample.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::storage_texture}
            }, sizeof(SSR_Up_PC));
            ssr_up_pipeline_ = p; ssr_up_set_layout_ = l; ssr_up_set_ = s;
            if (p) UH_INFO("SSR upsample pipeline created");
        }

        // Volumetric light
        {
            auto [p, l, s] = pb.build("volumetric_light.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},  // sampler2DShadow
                {2, DT::storage_texture}
            }, sizeof(Volumetric_PC));
            vol_pipeline_ = p; vol_set_layout_ = l; vol_set_ = s;
            if (p) UH_INFO("Volumetric light pipeline created");
        }

        // Volumetric upsample
        {
            auto [p, l, s] = pb.build("volumetric_upsample.comp", {
                {0, DT::combined_image_sampler},
                {1, DT::combined_image_sampler},
                {2, DT::storage_texture}
            }, sizeof(Volumetric_Up_PC));
            vol_up_pipeline_ = p; vol_up_set_layout_ = l; vol_up_set_ = s;
            if (p) UH_INFO("Volumetric upsample pipeline created");
        }

        // Light probe bake
        {
            auto [p, l, s] = pb.build("sh_probe_bake.comp", {
                {0, DT::storage_buffer},
                {1, DT::combined_image_sampler}
            }, sizeof(Probe_Bake_PC));
            probe_pipeline_ = p; probe_set_layout_ = l; probe_set_ = s;
            if (p) UH_INFO("Light probe bake pipeline created");
        }
    }

    void Post_Process_Manager::update_descriptors(
        graphics::Texture_Handle hdr_input,
        graphics::Texture_Handle depth,
        graphics::Texture_Handle normals)
    {
        bool shadow_changed = (bound_shadow_map_ != shadow_map_);
        if (bound_hdr_input_ == hdr_input && !shadow_changed) return;
        bound_hdr_input_ = hdr_input;
        bound_shadow_map_ = shadow_map_;

        auto sam = linear_sampler_;
        uint32_t hw = (std::max)(width_ / 2, 1u);
        uint32_t hh = (std::max)(height_ / 2, 1u);

        // SSAO: depth, normals → ssao_half_
        if (ssao_set_ && depth && normals && ssao_half_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { depth }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { normals }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { ssao_half_ };
            ssao_set_->update({ w0, w1, w2 });
        }

        // SSAO Upsample: ssao_half_, depth → ssao_full_
        if (ssao_up_set_ && ssao_half_ && depth && ssao_full_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { ssao_half_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { depth }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { ssao_full_ };
            ssao_up_set_->update({ w0, w1, w2 });
        }

        // Composite: hdr, ssao_full_, ssr_full_ → post_a_
        if (composite_set_ && hdr_input && ssao_full_ && post_a_) {
            auto ssr_tex = ssr_full_ ? ssr_full_ : ssao_full_; // fallback if SSR not created
            graphics::Descriptor_Write w0{}, w1{}, w2{}, w3{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { hdr_input }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { ssao_full_ }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::combined_image_sampler;
            w2.textures = { ssr_tex }; w2.samplers = { sam };
            w3.binding = 3; w3.type = DT::storage_texture;
            w3.textures = { post_a_ };
            composite_set_->update({ w0, w1, w2, w3 });
        }

        // Hi-Z sets: [0] reads from depth, [1..N] reads from hiz_mips_[i-1]
        for (uint32_t i = 0; i < HIZ_MIP_COUNT; i++) {
            if (!hiz_sets_[i] || !hiz_mips_[i]) continue;
            auto src = (i == 0) ? depth : hiz_mips_[i - 1];
            if (!src) continue;
            graphics::Descriptor_Write w0{}, w1{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { src }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::storage_texture;
            w1.textures = { hiz_mips_[i] };
            hiz_sets_[i]->update({ w0, w1 });
        }

        // SSR trace: hdr, depth, normals, hiz_mips_[0] → ssr_half_
        if (ssr_trace_set_ && hdr_input && depth && normals && ssr_half_) {
            auto hiz_src = hiz_mips_[0] ? hiz_mips_[0] : depth; // fallback
            graphics::Descriptor_Write w0{}, w1{}, w2{}, w3{}, w4{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { hdr_input }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { depth }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::combined_image_sampler;
            w2.textures = { normals }; w2.samplers = { sam };
            w3.binding = 3; w3.type = DT::combined_image_sampler;
            w3.textures = { hiz_src }; w3.samplers = { sam };
            w4.binding = 4; w4.type = DT::storage_texture;
            w4.textures = { ssr_half_ };
            ssr_trace_set_->update({ w0, w1, w2, w3, w4 });
        }

        // SSR upsample: ssr_half_, depth → ssr_full_
        if (ssr_up_set_ && ssr_half_ && depth && ssr_full_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { ssr_half_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { depth }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { ssr_full_ };
            ssr_up_set_->update({ w0, w1, w2 });
        }

        // Volumetric light: depth + shadow_map → volumetric_
        if (vol_set_ && depth && volumetric_ && shadow_map_) {
            auto comp_sam = comparison_sampler_ ? comparison_sampler_ : sam;
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { depth }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { shadow_map_ }; w1.samplers = { comp_sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { volumetric_ };
            vol_set_->update({ w0, w1, w2 });
        }

        // Volumetric upsample: volumetric_ + post_a_ → post_b_
        if (vol_up_set_ && volumetric_ && post_a_ && post_b_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { volumetric_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { post_a_ }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { post_b_ };
            vol_up_set_->update({ w0, w1, w2 });
        }

        // Bloom downsample sets: [0] reads from post_a_, [1..N] reads from bloom_chain_[i-1]
        for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
            if (!bloom_down_sets_[i] || !bloom_chain_[i]) continue;
            auto src = (i == 0) ? post_a_ : bloom_chain_[i - 1];
            if (!src) continue;
            graphics::Descriptor_Write w0{}, w1{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { src }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::storage_texture;
            w1.textures = { bloom_chain_[i] };
            bloom_down_sets_[i]->update({ w0, w1 });
        }

        // Bloom upsample sets: [i] reads from bloom_chain_[i+1], writes to bloom_chain_[i]
        for (int i = BLOOM_MIP_COUNT - 2; i >= 0; i--) {
            if (!bloom_up_sets_[i] || !bloom_chain_[i] || !bloom_chain_[i + 1]) continue;
            graphics::Descriptor_Write w0{}, w1{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { bloom_chain_[i + 1] }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::storage_texture;
            w1.textures = { bloom_chain_[i] };
            bloom_up_sets_[i]->update({ w0, w1 });
        }

        // Bloom composite: post_a_ + bloom_chain_[0] → post_b_
        if (bloom_comp_set_ && post_a_ && bloom_chain_[0] && post_b_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { post_a_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::combined_image_sampler;
            w1.textures = { bloom_chain_[0] }; w1.samplers = { sam };
            w2.binding = 2; w2.type = DT::storage_texture;
            w2.textures = { post_b_ };
            bloom_comp_set_->update({ w0, w1, w2 });
        }

        // Histogram: reads from tone-map input (post_b_ or post_a_)
        // We'll use post_a_ as default; will be re-bound in execute if bloom is active
        if (histogram_set_ && histogram_buffer_ && post_a_) {
            graphics::Descriptor_Write w0{}, w1{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { post_a_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::storage_buffer;
            w1.buffers = { histogram_buffer_ };
            w1.buffer_offsets = { 0 };
            w1.buffer_ranges = { sizeof(uint32_t) * 256 };
            histogram_set_->update({ w0, w1 });
        }

        // Tone mapping: reads from tone-map input → output_texture_
        if (tonemap_set_ && post_a_ && output_texture_ && exposure_buffer_) {
            graphics::Descriptor_Write w0{}, w1{}, w2{}, w3{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { post_a_ }; w0.samplers = { sam };
            w1.binding = 1; w1.type = DT::storage_texture;
            w1.textures = { output_texture_ };
            w2.binding = 2; w2.type = DT::storage_buffer;
            w2.buffers = { exposure_buffer_ };
            w2.buffer_offsets = { 0 };
            w2.buffer_ranges = { sizeof(float) * 2 };
            w3.binding = 3; w3.type = DT::combined_image_sampler;
            w3.textures = { lut_3d_ ? lut_3d_ : output_texture_ };
            w3.samplers = { sam };
            tonemap_set_->update({ w0, w1, w2, w3 });
        }
    }

    void Post_Process_Manager::execute(
        graphics::Command_Buffer_Handle cmd,
        graphics::Texture_Handle hdr_color,
        graphics::Texture_Handle depth,
        graphics::Texture_Handle gbuffer_normal)
    {
        if (!ready_ || !cmd || !tonemap_pipeline_ || !output_texture_) return;

        update_descriptors(hdr_color, depth, gbuffer_normal);

        uint32_t hw = (std::max)(width_ / 2, 1u);
        uint32_t hh = (std::max)(height_ / 2, 1u);

        // === Step 1: SSAO ===
        if (settings_.ssao_enabled && ssao_pipeline_ && ssao_up_pipeline_ &&
            ssao_set_ && ssao_up_set_ && ssao_half_ && ssao_full_) {

            cmd->resource_barrier(make_barrier(ssao_half_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(ssao_pipeline_);
            cmd->bind_descriptor_set(0, ssao_set_);

            SSAO_PC pc{};
            memcpy(pc.projection, projection_, sizeof(float) * 16);
            pc.full_res[0] = width_; pc.full_res[1] = height_;
            pc.half_res[0] = hw; pc.half_res[1] = hh;
            pc.radius = settings_.ssao_radius;
            pc.strength = settings_.ssao_strength;
            pc.num_samples = static_cast<uint32_t>(settings_.ssao_samples);
            pc.frame_idx = frame_index_;
            cmd->push_constants(0, sizeof(pc), &pc);
            cmd->dispatch((hw + 15) / 16, (hh + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(ssao_half_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

            // Upsample
            cmd->resource_barrier(make_barrier(ssao_full_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(ssao_up_pipeline_);
            cmd->bind_descriptor_set(0, ssao_up_set_);

            SSAO_Up_PC up_pc{};
            up_pc.full_res[0] = width_; up_pc.full_res[1] = height_;
            up_pc.half_res[0] = hw; up_pc.half_res[1] = hh;
            cmd->push_constants(0, sizeof(up_pc), &up_pc);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(ssao_full_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        }

        // === Step 1b: Hi-Z Pyramid ===
        if (settings_.ssr_enabled && hiz_pipeline_) {
            uint32_t hzw = hw, hzh = hh;
            uint32_t hzs_w = width_, hzs_h = height_;
            for (uint32_t i = 0; i < HIZ_MIP_COUNT; i++) {
                if (!hiz_sets_[i] || !hiz_mips_[i]) break;

                cmd->resource_barrier(make_barrier(hiz_mips_[i].get(),
                    graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

                cmd->bind_pipeline(hiz_pipeline_);
                cmd->bind_descriptor_set(0, hiz_sets_[i]);

                HiZ_PC hpc{};
                hpc.src_res[0] = hzs_w; hpc.src_res[1] = hzs_h;
                hpc.dst_res[0] = hzw; hpc.dst_res[1] = hzh;
                cmd->push_constants(0, sizeof(hpc), &hpc);
                cmd->dispatch((hzw + 15) / 16, (hzh + 15) / 16, 1);

                cmd->resource_barrier(make_barrier(hiz_mips_[i].get(),
                    graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

                hzs_w = hzw; hzs_h = hzh;
                hzw = (std::max)(hzw / 2, 1u);
                hzh = (std::max)(hzh / 2, 1u);
            }
        }

        // === Step 1c: SSR Trace + Upsample ===
        if (settings_.ssr_enabled && ssr_trace_pipeline_ && ssr_up_pipeline_ &&
            ssr_trace_set_ && ssr_up_set_ && ssr_half_ && ssr_full_) {

            // SSR trace (half-res)
            cmd->resource_barrier(make_barrier(ssr_half_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(ssr_trace_pipeline_);
            cmd->bind_descriptor_set(0, ssr_trace_set_);

            SSR_Trace_PC spc{};
            memcpy(spc.proj, projection_, sizeof(float) * 16);
            memcpy(spc.inv_proj, inv_projection_, sizeof(float) * 16);
            memcpy(spc.view, view_, sizeof(float) * 16);
            spc.full_res[0] = width_; spc.full_res[1] = height_;
            spc.half_res[0] = hw; spc.half_res[1] = hh;
            spc.max_steps = static_cast<uint32_t>(settings_.ssr_max_steps);
            spc.max_distance = settings_.ssr_max_distance;
            spc.thickness = settings_.ssr_thickness;
            cmd->push_constants(0, sizeof(spc), &spc);
            cmd->dispatch((hw + 15) / 16, (hh + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(ssr_half_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

            // SSR upsample
            cmd->resource_barrier(make_barrier(ssr_full_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(ssr_up_pipeline_);
            cmd->bind_descriptor_set(0, ssr_up_set_);

            SSR_Up_PC sup{};
            sup.full_res[0] = width_; sup.full_res[1] = height_;
            sup.half_res[0] = hw; sup.half_res[1] = hh;
            cmd->push_constants(0, sizeof(sup), &sup);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(ssr_full_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        }

        // === Step 2: Composite (HDR * SSAO + SSR → post_a_) ===
        if (composite_pipeline_ && composite_set_ && post_a_) {
            cmd->resource_barrier(make_barrier(post_a_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(composite_pipeline_);
            cmd->bind_descriptor_set(0, composite_set_);

            Composite_PC cpc{};
            cpc.resolution[0] = width_; cpc.resolution[1] = height_;
            cpc.ssao_enabled = settings_.ssao_enabled ? 1u : 0u;
            cpc.ssr_enabled = settings_.ssr_enabled ? 1u : 0u;
            cmd->push_constants(0, sizeof(cpc), &cpc);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(post_a_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
        }

        // Track which buffer holds the current scene
        // After composite, scene is in post_a_
        auto current_scene = post_a_;
        auto other_buffer = post_b_;

        // === Step 2b: Volumetric Light ===
        if (settings_.volumetric_enabled && vol_pipeline_ && vol_up_pipeline_ &&
            vol_set_ && vol_up_set_ && volumetric_ && shadow_map_) {

            uint32_t qw = (std::max)(width_ / 4, 1u);
            uint32_t qh = (std::max)(height_ / 4, 1u);

            // Volumetric light (quarter-res)
            cmd->resource_barrier(make_barrier(volumetric_.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(vol_pipeline_);
            cmd->bind_descriptor_set(0, vol_set_);

            Volumetric_PC vpc{};
            memcpy(vpc.inv_view_proj, inv_view_proj_, sizeof(float) * 16);
            memcpy(vpc.shadow_view_proj, shadow_view_proj_, sizeof(float) * 16);
            memcpy(vpc.light_pos, light_pos_, sizeof(float) * 4);
            memcpy(vpc.light_color, light_color_, sizeof(float) * 4);
            vpc.quarter_res[0] = qw; vpc.quarter_res[1] = qh;
            vpc.density = settings_.volumetric_density;
            vpc.attenuation = settings_.volumetric_attenuation;
            vpc.num_steps = 64;
            vpc.max_distance = 5.0f;
            cmd->push_constants(0, sizeof(vpc), &vpc);
            cmd->dispatch((qw + 15) / 16, (qh + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(volumetric_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

            // Volumetric upsample: volumetric_ + current_scene → other_buffer
            // Dynamically rebind upsample set to read from current scene
            {
                graphics::Descriptor_Write w0{}, w1{}, w2{};
                w0.binding = 0; w0.type = DT::combined_image_sampler;
                w0.textures = { volumetric_ }; w0.samplers = { linear_sampler_ };
                w1.binding = 1; w1.type = DT::combined_image_sampler;
                w1.textures = { current_scene }; w1.samplers = { linear_sampler_ };
                w2.binding = 2; w2.type = DT::storage_texture;
                w2.textures = { other_buffer };
                vol_up_set_->update({ w0, w1, w2 });
            }

            cmd->resource_barrier(make_barrier(other_buffer.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(vol_up_pipeline_);
            cmd->bind_descriptor_set(0, vol_up_set_);

            Volumetric_Up_PC vupc{};
            vupc.full_res[0] = width_; vupc.full_res[1] = height_;
            vupc.quarter_res[0] = qw; vupc.quarter_res[1] = qh;
            cmd->push_constants(0, sizeof(vupc), &vupc);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(other_buffer.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

            // Swap buffers
            std::swap(current_scene, other_buffer);
        }

        // === Step 3: Bloom ===
        if (settings_.bloom_enabled && bloom_down_pipeline_ && bloom_up_pipeline_ &&
            bloom_comp_pipeline_ && bloom_comp_set_) {

            uint32_t bw = hw, bh = hh;
            uint32_t src_w = width_, src_h = height_;

            // Rebind bloom_down_sets_[0] to read from current_scene (may be post_b_ if volumetric ran)
            if (bloom_down_sets_[0] && bloom_chain_[0]) {
                graphics::Descriptor_Write w0{}, w1{};
                w0.binding = 0; w0.type = DT::combined_image_sampler;
                w0.textures = { current_scene }; w0.samplers = { linear_sampler_ };
                w1.binding = 1; w1.type = DT::storage_texture;
                w1.textures = { bloom_chain_[0] };
                bloom_down_sets_[0]->update({ w0, w1 });
            }

            // Downsample chain
            for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
                if (!bloom_down_sets_[i] || !bloom_chain_[i]) break;

                cmd->resource_barrier(make_barrier(bloom_chain_[i].get(),
                    graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

                cmd->bind_pipeline(bloom_down_pipeline_);
                cmd->bind_descriptor_set(0, bloom_down_sets_[i]);

                Bloom_Down_PC bdpc{};
                bdpc.src_res[0] = src_w; bdpc.src_res[1] = src_h;
                bdpc.dst_res[0] = bw; bdpc.dst_res[1] = bh;
                bdpc.threshold = settings_.bloom_threshold;
                bdpc.is_first_pass = (i == 0) ? 1u : 0u;
                cmd->push_constants(0, sizeof(bdpc), &bdpc);
                cmd->dispatch((bw + 15) / 16, (bh + 15) / 16, 1);

                cmd->resource_barrier(make_barrier(bloom_chain_[i].get(),
                    graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

                src_w = bw; src_h = bh;
                bw = (std::max)(bw / 2, 1u);
                bh = (std::max)(bh / 2, 1u);
            }

            // Upsample chain (from smallest to largest, additive)
            bw = hw; bh = hh;
            // Compute sizes for each mip
            uint32_t mip_w[BLOOM_MIP_COUNT], mip_h[BLOOM_MIP_COUNT];
            {
                uint32_t tw = hw, th = hh;
                for (uint32_t i = 0; i < BLOOM_MIP_COUNT; i++) {
                    mip_w[i] = tw; mip_h[i] = th;
                    tw = (std::max)(tw / 2, 1u);
                    th = (std::max)(th / 2, 1u);
                }
            }

            for (int i = static_cast<int>(BLOOM_MIP_COUNT) - 2; i >= 0; i--) {
                if (!bloom_up_sets_[i] || !bloom_chain_[i]) break;

                // dst needs to be read+write for additive blend
                cmd->resource_barrier(make_barrier(bloom_chain_[i].get(),
                    graphics::Resource_State::shader_resource, graphics::Resource_State::unordered_access));

                cmd->bind_pipeline(bloom_up_pipeline_);
                cmd->bind_descriptor_set(0, bloom_up_sets_[i]);

                Bloom_Up_PC bupc{};
                bupc.src_res[0] = mip_w[i + 1]; bupc.src_res[1] = mip_h[i + 1];
                bupc.dst_res[0] = mip_w[i]; bupc.dst_res[1] = mip_h[i];
                bupc.filter_radius = 1.0f;
                cmd->push_constants(0, sizeof(bupc), &bupc);
                cmd->dispatch((mip_w[i] + 15) / 16, (mip_h[i] + 15) / 16, 1);

                cmd->resource_barrier(make_barrier(bloom_chain_[i].get(),
                    graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
            }

            // Bloom composite: current_scene + bloom_chain_[0] → other_buffer
            // Rebind bloom comp set dynamically
            {
                graphics::Descriptor_Write w0{}, w1{}, w2{};
                w0.binding = 0; w0.type = DT::combined_image_sampler;
                w0.textures = { current_scene }; w0.samplers = { linear_sampler_ };
                w1.binding = 1; w1.type = DT::combined_image_sampler;
                w1.textures = { bloom_chain_[0] }; w1.samplers = { linear_sampler_ };
                w2.binding = 2; w2.type = DT::storage_texture;
                w2.textures = { other_buffer };
                bloom_comp_set_->update({ w0, w1, w2 });
            }

            cmd->resource_barrier(make_barrier(other_buffer.get(),
                graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(bloom_comp_pipeline_);
            cmd->bind_descriptor_set(0, bloom_comp_set_);

            Bloom_Comp_PC bcpc{};
            bcpc.resolution[0] = width_; bcpc.resolution[1] = height_;
            bcpc.intensity = settings_.bloom_intensity;
            cmd->push_constants(0, sizeof(bcpc), &bcpc);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(other_buffer.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));

            std::swap(current_scene, other_buffer);
        }

        // Determine tone mapping input
        auto tonemap_input = current_scene;

        // Update tonemap + histogram descriptors to read from correct input
        if (tonemap_set_ && tonemap_input && output_texture_ && exposure_buffer_) {
            graphics::Descriptor_Write w0{};
            w0.binding = 0; w0.type = DT::combined_image_sampler;
            w0.textures = { tonemap_input }; w0.samplers = { linear_sampler_ };
            graphics::Descriptor_Write w1{};
            w1.binding = 1; w1.type = DT::storage_texture;
            w1.textures = { output_texture_ };
            graphics::Descriptor_Write w2{};
            w2.binding = 2; w2.type = DT::storage_buffer;
            w2.buffers = { exposure_buffer_ };
            w2.buffer_offsets = { 0 };
            w2.buffer_ranges = { sizeof(float) * 2 };
            // Binding 3: color LUT (use lut_3d_ if available, else a dummy)
            graphics::Descriptor_Write w3{};
            w3.binding = 3; w3.type = DT::combined_image_sampler;
            w3.textures = { lut_3d_ ? lut_3d_ : output_texture_ }; // fallback
            w3.samplers = { linear_sampler_ };
            tonemap_set_->update({ w0, w1, w2, w3 });
        }

        // === Step 4: Auto-exposure histogram ===
        if (settings_.auto_exposure && histogram_pipeline_ && histogram_avg_pipeline_ &&
            histogram_set_ && histogram_avg_set_) {

            // Update histogram input
            if (histogram_set_ && tonemap_input) {
                graphics::Descriptor_Write w0{};
                w0.binding = 0; w0.type = DT::combined_image_sampler;
                w0.textures = { tonemap_input }; w0.samplers = { linear_sampler_ };
                graphics::Descriptor_Write w1{};
                w1.binding = 1; w1.type = DT::storage_buffer;
                w1.buffers = { histogram_buffer_ };
                w1.buffer_offsets = { 0 };
                w1.buffer_ranges = { sizeof(uint32_t) * 256 };
                histogram_set_->update({ w0, w1 });
            }

            cmd->bind_pipeline(histogram_pipeline_);
            cmd->bind_descriptor_set(0, histogram_set_);

            Histogram_PC hpc{};
            hpc.resolution[0] = width_; hpc.resolution[1] = height_;
            hpc.min_log_lum = MIN_LOG_LUM;
            hpc.inv_log_lum_range = 1.0f / LOG_LUM_RANGE;
            cmd->push_constants(0, sizeof(hpc), &hpc);
            cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

            cmd->resource_barrier(make_barrier(histogram_buffer_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::unordered_access));

            cmd->bind_pipeline(histogram_avg_pipeline_);
            cmd->bind_descriptor_set(0, histogram_avg_set_);

            Histogram_Avg_PC apc{};
            apc.pixel_count = width_ * height_;
            apc.min_log_lum = MIN_LOG_LUM;
            apc.log_lum_range = LOG_LUM_RANGE;
            apc.time_delta = delta_time_;
            apc.adaptation_speed = 1.5f;
            cmd->push_constants(0, sizeof(apc), &apc);
            cmd->dispatch(1, 1, 1);

            cmd->resource_barrier(make_barrier(exposure_buffer_.get(),
                graphics::Resource_State::unordered_access, graphics::Resource_State::unordered_access));
        }

        // === Step 4b: Generate LUT if needed ===
        bool color_grading_active = lut_3d_ && lut_gen_pipeline_ &&
            (settings_.color_temperature != 0.0f || settings_.color_contrast != 1.0f ||
             settings_.color_saturation != 1.0f || settings_.color_preset != 0);
        if (color_grading_active) {
            generate_lut(cmd);
        }

        // === Step 5: Tone mapping ===
        cmd->resource_barrier(make_barrier(output_texture_.get(),
            graphics::Resource_State::undefined, graphics::Resource_State::unordered_access));

        cmd->bind_pipeline(tonemap_pipeline_);
        cmd->bind_descriptor_set(0, tonemap_set_);

        Tonemap_PC tpc{};
        tpc.resolution[0] = width_; tpc.resolution[1] = height_;
        tpc.tone_map_mode = static_cast<uint32_t>(settings_.tone_map_mode);
        tpc.manual_exposure = settings_.manual_exposure;
        tpc.auto_exposure_on = settings_.auto_exposure ? 1u : 0u;
        tpc.gamma_debug_mode = static_cast<uint32_t>(settings_.gamma_debug_mode);
        tpc.color_grading_enabled = color_grading_active ? 1u : 0u;
        cmd->push_constants(0, sizeof(tpc), &tpc);
        cmd->dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);

        cmd->resource_barrier(make_barrier(output_texture_.get(),
            graphics::Resource_State::unordered_access, graphics::Resource_State::shader_resource));
    }

    auto Post_Process_Manager::get_output_texture() -> graphics::Texture_Handle
    {
        return (ready_ && output_texture_) ? output_texture_ : nullptr;
    }

    void Post_Process_Manager::register_graph_passes(Render_Graph& graph, const Frame_Context& context) const
    {
        Bloom_Pass bloom{};
        Tonemap_Pass tonemap{};
        Sensor_Export_Pass sensor_export{};

        bloom.add_to_graph(graph);

        if (context.outputs.depth
            || context.outputs.normal
            || context.outputs.segmentation
            || context.outputs.instance_id
            || context.outputs.motion_vector) {
            sensor_export.add_to_graph(graph);
        }

        if (context.outputs.rgb) {
            tonemap.add_to_graph(graph);
        }
    }

    void Post_Process_Manager::render_settings_ui()
    {
        if (!ImGui::Begin("Post Processing")) {
            ImGui::End();
            return;
        }

        if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* modes[] = { "ACES Filmic", "Reinhard" };
            ImGui::Combo("Tone Map", &settings_.tone_map_mode, modes, 2);
            ImGui::SliderFloat("Exposure", &settings_.manual_exposure, 0.1f, 5.0f);
            ImGui::Checkbox("Auto Exposure", &settings_.auto_exposure);
        }

        if (ImGui::CollapsingHeader("Gamma Debug")) {
            const char* dm[] = { "Off", "Linear", "sRGB", "Albedo Range" };
            ImGui::Combo("Debug Mode", &settings_.gamma_debug_mode, dm, 4);
        }

        if (ImGui::CollapsingHeader("SSAO")) {
            ImGui::Checkbox("Enabled##ssao", &settings_.ssao_enabled);
            if (settings_.ssao_enabled) {
                ImGui::SliderFloat("Radius", &settings_.ssao_radius, 0.1f, 2.0f);
                ImGui::SliderFloat("Strength", &settings_.ssao_strength, 0.1f, 3.0f);
                const char* samples[] = { "16", "32" };
                int idx = settings_.ssao_samples == 32 ? 1 : 0;
                if (ImGui::Combo("Samples", &idx, samples, 2)) {
                    settings_.ssao_samples = idx == 1 ? 32 : 16;
                }
            }
        }

        if (ImGui::CollapsingHeader("Bloom")) {
            ImGui::Checkbox("Enabled##bloom", &settings_.bloom_enabled);
            if (settings_.bloom_enabled) {
                ImGui::SliderFloat("Threshold", &settings_.bloom_threshold, 0.0f, 5.0f);
                ImGui::SliderFloat("Intensity##bloom", &settings_.bloom_intensity, 0.0f, 2.0f);
            }
        }

        if (ImGui::CollapsingHeader("Color Grading")) {
            ImGui::SliderFloat("Temperature", &settings_.color_temperature, -1.0f, 1.0f);
            ImGui::SliderFloat("Contrast", &settings_.color_contrast, 0.5f, 2.0f);
            ImGui::SliderFloat("Saturation", &settings_.color_saturation, 0.0f, 2.0f);
            const char* presets[] = { "Neutral", "Cinematic", "Natural" };
            ImGui::Combo("Preset", &settings_.color_preset, presets, 3);
        }

        if (ImGui::CollapsingHeader("SSR")) {
            ImGui::Checkbox("Enabled##ssr", &settings_.ssr_enabled);
            if (settings_.ssr_enabled) {
                ImGui::SliderInt("Max Steps", &settings_.ssr_max_steps, 32, 64);
                ImGui::SliderFloat("Thickness", &settings_.ssr_thickness, 0.01f, 0.5f);
                ImGui::SliderFloat("Max Distance", &settings_.ssr_max_distance, 1.0f, 50.0f);
            }
        }

        if (ImGui::CollapsingHeader("Volumetric Light")) {
            ImGui::Checkbox("Enabled##vol", &settings_.volumetric_enabled);
            if (settings_.volumetric_enabled) {
                ImGui::SliderFloat("Density", &settings_.volumetric_density, 0.01f, 1.0f);
                ImGui::SliderFloat("Attenuation", &settings_.volumetric_attenuation, 0.1f, 5.0f);
            }
        }

        if (ImGui::CollapsingHeader("Light Probes")) {
            ImGui::Checkbox("Enabled##probes", &settings_.probes_enabled);
        }

        ImGui::End();
    }

} // namespace mango::app
