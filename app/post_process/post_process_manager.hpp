#pragma once
#include "device.hpp"
#include "command-execution/command-pool.hpp"
#include "command-execution/command-buffer.hpp"
#include "command-execution/command-queue.hpp"
#include "render-resource/texture.hpp"
#include "render-resource/descriptor-set.hpp"
#include "render-resource/sampler.hpp"
#include "render-resource/buffer.hpp"
#include "pipeline-state/compute-pipeline-state.hpp"
#include <memory>
#include <cstdint>
#include <vector>

namespace mango::app
{
    struct Post_Process_Settings
    {
        // Tone mapping
        int tone_map_mode = 0;       // 0=ACES, 1=Reinhard
        float manual_exposure = 0.8f;
        bool auto_exposure = false;

        // Gamma debug
        int gamma_debug_mode = 0;    // 0=off, 1=linear, 2=sRGB, 3=albedo range

        // SSAO
        bool ssao_enabled = false;
        float ssao_radius = 0.5f;
        float ssao_strength = 1.5f;
        int ssao_samples = 16;

        // Bloom
        bool bloom_enabled = false;
        float bloom_threshold = 1.0f;
        float bloom_intensity = 0.5f;
        bool bloom_lens_dirt = false;

        // Color grading
        float color_temperature = 0.0f;
        float color_contrast = 1.0f;
        float color_saturation = 1.0f;
        int color_preset = 0;        // 0=Neutral, 1=Cinematic, 2=Natural

        // SSR
        bool ssr_enabled = false;
        int ssr_max_steps = 48;
        float ssr_thickness = 0.1f;
        float ssr_max_distance = 10.0f;

        // Volumetric light
        bool volumetric_enabled = false;
        float volumetric_density = 0.1f;
        float volumetric_attenuation = 1.0f;

        // Light probes
        bool probes_enabled = false;
    };

    class Post_Process_Manager
    {
    public:
        Post_Process_Manager() = default;
        ~Post_Process_Manager() = default;

        void init(graphics::Device_Handle device,
                  graphics::Command_Pool_Handle pool,
                  graphics::Command_Queue_Handle queue,
                  uint32_t width, uint32_t height);

        void resize(uint32_t width, uint32_t height);

        void execute(graphics::Command_Buffer_Handle cmd,
                     graphics::Texture_Handle hdr_color,
                     graphics::Texture_Handle depth,
                     graphics::Texture_Handle gbuffer_normal);

        void render_settings_ui();

        auto get_settings() -> Post_Process_Settings& { return settings_; }
        auto get_settings() const -> const Post_Process_Settings& { return settings_; }

        bool is_ready() const { return ready_; }
        auto get_output_texture() -> graphics::Texture_Handle;
        void set_delta_time(float dt) { delta_time_ = dt; }
        void set_frame_index(uint32_t idx) { frame_index_ = idx; }
        void set_projection_matrix(const float* mat) { memcpy(projection_, mat, sizeof(float) * 16); }

        void set_view_matrix(const float* mat) { memcpy(view_, mat, sizeof(float) * 16); }
        void set_inv_projection_matrix(const float* mat) { memcpy(inv_projection_, mat, sizeof(float) * 16); }

        // Volumetric light data
        void set_shadow_map(graphics::Texture_Handle tex) { shadow_map_ = tex; }
        void set_shadow_sampler(graphics::Sampler_Handle s) { shadow_sampler_ = s; }
        void set_shadow_view_proj(const float* mat) { memcpy(shadow_view_proj_, mat, sizeof(float) * 16); }
        void set_inv_view_proj(const float* mat) { memcpy(inv_view_proj_, mat, sizeof(float) * 16); }
        void set_light_position(float x, float y, float z) { light_pos_[0]=x; light_pos_[1]=y; light_pos_[2]=z; light_pos_[3]=0.0f; }
        void set_light_color(float r, float g, float b, float intensity) { light_color_[0]=r; light_color_[1]=g; light_color_[2]=b; light_color_[3]=intensity; }

    private:
        void create_textures();
        void create_exposure_resources();
        void create_pipelines();
        void create_lut_texture();
        void generate_lut(graphics::Command_Buffer_Handle cmd);
        void update_descriptors(graphics::Texture_Handle hdr_input,
                                graphics::Texture_Handle depth,
                                graphics::Texture_Handle normals);

        graphics::Device_Handle device_;
        graphics::Command_Pool_Handle pool_;
        graphics::Command_Queue_Handle queue_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        bool ready_ = false;
        float delta_time_ = 0.016f;
        uint32_t frame_index_ = 0;
        float projection_[16] = {};
        float view_[16] = {};
        float inv_projection_[16] = {};
        float shadow_view_proj_[16] = {};
        float inv_view_proj_[16] = {};
        float light_pos_[4] = {};
        float light_color_[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        graphics::Texture_Handle shadow_map_;
        graphics::Sampler_Handle shadow_sampler_;

        Post_Process_Settings settings_;
        // Track LUT dirty state
        float prev_temperature_ = -999.0f;
        float prev_contrast_ = -999.0f;
        float prev_saturation_ = -999.0f;
        int prev_preset_ = -1;
        bool lut_dirty_ = true;

        // Sampler
        graphics::Sampler_Handle linear_sampler_;

        // Intermediate textures
        graphics::Texture_Handle output_texture_;    // final tone-mapped (rgba16f)
        graphics::Texture_Handle post_a_;            // composite output (rgba16f)
        graphics::Texture_Handle post_b_;            // bloom composite output (rgba16f)
        graphics::Texture_Handle ssao_half_;         // half-res AO (r16f)
        graphics::Texture_Handle ssao_full_;         // full-res AO (r16f)

        static constexpr uint32_t BLOOM_MIP_COUNT = 5;
        graphics::Texture_Handle bloom_chain_[BLOOM_MIP_COUNT]; // progressive mips (rgba16f)

        // SSR textures
        graphics::Texture_Handle ssr_half_;          // half-res SSR (rgba16f)
        graphics::Texture_Handle ssr_full_;          // full-res SSR (rgba16f)

        // Hi-Z pyramid
        static constexpr uint32_t HIZ_MIP_COUNT = 8;
        graphics::Texture_Handle hiz_mips_[HIZ_MIP_COUNT]; // r32f per mip

        // Volumetric light
        graphics::Texture_Handle volumetric_;        // quarter-res rgba16f

        // Color grading 3D LUT
        graphics::Texture_Handle lut_3d_;            // 32^3 rgba16f

        // Buffers
        graphics::Buffer_Handle exposure_buffer_;    // 2 floats
        graphics::Buffer_Handle histogram_buffer_;   // 256 uints

        // ---- Pipelines & descriptor sets ----

        // SSAO
        graphics::Compute_Pipeline_Handle ssao_pipeline_;
        graphics::Descriptor_Set_Layout_Handle ssao_set_layout_;
        graphics::Descriptor_Set_Handle ssao_set_;

        // SSAO Upsample
        graphics::Compute_Pipeline_Handle ssao_up_pipeline_;
        graphics::Descriptor_Set_Layout_Handle ssao_up_set_layout_;
        graphics::Descriptor_Set_Handle ssao_up_set_;

        // Composite
        graphics::Compute_Pipeline_Handle composite_pipeline_;
        graphics::Descriptor_Set_Layout_Handle composite_set_layout_;
        graphics::Descriptor_Set_Handle composite_set_;

        // Bloom downsample (one pipeline, per-mip descriptor sets)
        graphics::Compute_Pipeline_Handle bloom_down_pipeline_;
        graphics::Descriptor_Set_Layout_Handle bloom_down_set_layout_;
        graphics::Descriptor_Set_Handle bloom_down_sets_[BLOOM_MIP_COUNT];

        // Bloom upsample
        graphics::Compute_Pipeline_Handle bloom_up_pipeline_;
        graphics::Descriptor_Set_Layout_Handle bloom_up_set_layout_;
        graphics::Descriptor_Set_Handle bloom_up_sets_[BLOOM_MIP_COUNT];

        // Bloom composite
        graphics::Compute_Pipeline_Handle bloom_comp_pipeline_;
        graphics::Descriptor_Set_Layout_Handle bloom_comp_set_layout_;
        graphics::Descriptor_Set_Handle bloom_comp_set_;

        // Histogram
        graphics::Compute_Pipeline_Handle histogram_pipeline_;
        graphics::Descriptor_Set_Layout_Handle histogram_set_layout_;
        graphics::Descriptor_Set_Handle histogram_set_;

        // Histogram average
        graphics::Compute_Pipeline_Handle histogram_avg_pipeline_;
        graphics::Descriptor_Set_Layout_Handle histogram_avg_set_layout_;
        graphics::Descriptor_Set_Handle histogram_avg_set_;

        // Tone mapping
        graphics::Compute_Pipeline_Handle tonemap_pipeline_;
        graphics::Descriptor_Set_Layout_Handle tonemap_set_layout_;
        graphics::Descriptor_Set_Handle tonemap_set_;

        // LUT generate
        graphics::Compute_Pipeline_Handle lut_gen_pipeline_;
        graphics::Descriptor_Set_Layout_Handle lut_gen_set_layout_;
        graphics::Descriptor_Set_Handle lut_gen_set_;

        // Hi-Z generate
        graphics::Compute_Pipeline_Handle hiz_pipeline_;
        graphics::Descriptor_Set_Layout_Handle hiz_set_layout_;
        graphics::Descriptor_Set_Handle hiz_sets_[HIZ_MIP_COUNT];

        // SSR trace
        graphics::Compute_Pipeline_Handle ssr_trace_pipeline_;
        graphics::Descriptor_Set_Layout_Handle ssr_trace_set_layout_;
        graphics::Descriptor_Set_Handle ssr_trace_set_;

        // SSR upsample
        graphics::Compute_Pipeline_Handle ssr_up_pipeline_;
        graphics::Descriptor_Set_Layout_Handle ssr_up_set_layout_;
        graphics::Descriptor_Set_Handle ssr_up_set_;

        // Volumetric light
        graphics::Compute_Pipeline_Handle vol_pipeline_;
        graphics::Descriptor_Set_Layout_Handle vol_set_layout_;
        graphics::Descriptor_Set_Handle vol_set_;

        // Volumetric upsample
        graphics::Compute_Pipeline_Handle vol_up_pipeline_;
        graphics::Descriptor_Set_Layout_Handle vol_up_set_layout_;
        graphics::Descriptor_Set_Handle vol_up_set_;

        // Light probe bake
        graphics::Compute_Pipeline_Handle probe_pipeline_;
        graphics::Descriptor_Set_Layout_Handle probe_set_layout_;
        graphics::Descriptor_Set_Handle probe_set_;
        graphics::Buffer_Handle probe_buffer_;

        // Comparison sampler for shadow map
        graphics::Sampler_Handle comparison_sampler_;

        // Track bound inputs
        graphics::Texture_Handle bound_hdr_input_;
        graphics::Texture_Handle bound_shadow_map_;

        // Push constant structs
        struct SSAO_PC {
            float projection[16];
            uint32_t full_res[2];
            uint32_t half_res[2];
            float radius;
            float strength;
            uint32_t num_samples;
            uint32_t frame_idx;
        };

        struct SSAO_Up_PC {
            uint32_t full_res[2];
            uint32_t half_res[2];
        };

        struct Composite_PC {
            uint32_t resolution[2];
            uint32_t ssao_enabled;
            uint32_t ssr_enabled;
        };

        struct Bloom_Down_PC {
            uint32_t src_res[2];
            uint32_t dst_res[2];
            float threshold;
            uint32_t is_first_pass;
        };

        struct Bloom_Up_PC {
            uint32_t src_res[2];
            uint32_t dst_res[2];
            float filter_radius;
        };

        struct Bloom_Comp_PC {
            uint32_t resolution[2];
            float intensity;
            float _pad;
        };

        struct Tonemap_PC {
            uint32_t resolution[2];
            uint32_t tone_map_mode;
            float manual_exposure;
            uint32_t auto_exposure_on;
            uint32_t gamma_debug_mode;
            uint32_t color_grading_enabled;
            float _pad1;
        };

        struct LUT_Gen_PC {
            float temperature;
            float contrast;
            float saturation;
            uint32_t preset;
        };

        struct HiZ_PC {
            uint32_t src_res[2];
            uint32_t dst_res[2];
        };

        struct SSR_Trace_PC {
            float proj[16];
            float inv_proj[16];
            float view[16];
            uint32_t full_res[2];
            uint32_t half_res[2];
            uint32_t max_steps;
            float max_distance;
            float thickness;
            float _pad;
        };

        struct SSR_Up_PC {
            uint32_t full_res[2];
            uint32_t half_res[2];
        };

        struct Histogram_PC {
            uint32_t resolution[2];
            float min_log_lum;
            float inv_log_lum_range;
        };

        struct Histogram_Avg_PC {
            uint32_t pixel_count;
            float min_log_lum;
            float log_lum_range;
            float time_delta;
            float adaptation_speed;
        };

        struct Volumetric_PC {
            float inv_view_proj[16];
            float shadow_view_proj[16];
            float light_pos[4];       // xyz=pos, w=unused
            float light_color[4];     // xyz=color, w=intensity
            uint32_t quarter_res[2];
            float density;
            float attenuation;
            uint32_t num_steps;
            float max_distance;
            float _pad[2];
        };

        struct Volumetric_Up_PC {
            uint32_t full_res[2];
            uint32_t quarter_res[2];
        };

        struct Probe_Bake_PC {
            uint32_t grid_dim[4];  // xyz=dim, w=total
            float grid_min[4];
            float grid_max[4];
            uint32_t num_samples;
            float _pad[3];
        };
    };

} // namespace mango::app
