// app/renderer/renderer.hpp
#pragma once
#include "device.hpp"
#include "render-pass/swapchain.hpp"
#include "render-pass/render-pass.hpp"
#include "render-pass/framebuffer.hpp"
#include "render-resource/texture.hpp"
#include "render-resource/descriptor-set.hpp"
#include "render-resource/sampler.hpp"
#include "pipeline-state/graphics-pipeline-state.hpp"
#include "command-execution/command-pool.hpp"
#include "command-execution/command-buffer.hpp"
#include "command-execution/command-queue.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"
#include "manager/scene-graph.hpp"
#include "render_core/frame_pipeline.hpp"
#include "render_core/render_targets.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace mango::app
{
    enum class Graphics_Backend
    {
        Vulkan,
        // D3D12,    // Reserved for future
        // Metal,    // Reserved for future
        // OpenGL    // Reserved for
    };

    struct Renderer_Desc
    {
        Graphics_Backend backend = Graphics_Backend::Vulkan;
        uint32_t width = 1280;
        uint32_t height = 720;
        void* native_window = nullptr;
        bool enable_validation = true;
        bool enable_vsync = true;
        uint32_t max_frames_in_flight = 2;
    };

    class Renderer
    {
    public:
        explicit Renderer(const Renderer_Desc& desc);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        // Core rendering interface
        void begin_frame();
        void end_frame();
        void render_frame();

        // Resource management
        void wait_idle();
        void handle_resize(uint32_t width, uint32_t height);

        // Getters
        auto get_current_command_buffer() -> graphics::Command_Buffer_Handle;
        auto get_device() -> graphics::Device_Handle { return device_; }
        auto get_render_pass() -> graphics::Render_Pass_Handle { return blit_render_pass_; }
        auto get_scene_render_pass() -> graphics::Render_Pass_Handle { return scene_render_pass_; }
        auto get_width() const -> uint32_t { return width_; }
        auto get_height() const -> uint32_t { return height_; }
        auto get_current_frame_index() const -> uint32_t { return current_frame_; }
        auto get_swapchain_image_count() const -> uint32_t;
        auto get_scene_graph() -> core::Scene_Graph* { return core::Scene_Graph::current_instance(); }
        auto get_command_pool() -> graphics::Command_Pool_Handle { return command_pool_; }
        auto get_graphics_queue() -> graphics::Command_Queue_Handle { return graphics_queue_; }
        auto get_hdr_color_texture() -> graphics::Texture_Handle { return hdr_color_; }
        auto get_gbuffer_normal_texture() -> graphics::Texture_Handle { return gbuffer_normal_; }
        auto get_depth_texture() -> graphics::Texture_Handle { return depth_image_; }

        // Update the blit descriptor to sample from a different texture (e.g. post-processed output)
        void set_blit_source(graphics::Texture_Handle source);
        void set_blit_passthrough(bool passthrough) { blit_passthrough_ = passthrough; }

        // Callbacks for rendering
        using RenderCallback = std::function<void(graphics::Command_Buffer_Handle)>;
        void set_render_callback(RenderCallback callback);
        void set_pre_render_callback(RenderCallback callback);
        void set_post_process_callback(RenderCallback callback);
        void set_imgui_render_callback(RenderCallback callback);

    private:
        // Initialization
        void create_device();
        void create_swapchain();
        void create_depth_resources();
        void create_offscreen_resources();
        void create_scene_render_pass();
        void create_scene_framebuffer();
        void create_blit_render_pass();
        void create_blit_framebuffers();
        void create_blit_pipeline();
        void create_command_resources();
        void create_sync_objects();

        // Cleanup
        void cleanup_swapchain();
        void cleanup();

        // Swapchain recreation
        void recreate_swapchain();
        void wait_for_window_size();

        // Helper methods
        auto choose_depth_format() -> graphics::Texture_Format;

        // Backend-specific device creation
        auto create_vulkan_device() -> graphics::Device_Handle;

        // Rendering state
        graphics::Device_Handle device_;
        graphics::Swapchain_Handle swapchain_;

        // Offscreen scene rendering
        graphics::Texture_Handle hdr_color_;          // rgba16f, scene HDR output
        graphics::Texture_Handle gbuffer_normal_;      // rgba16f, normal.xyz + roughness
        graphics::Texture_Handle depth_image_;         // depth, sampled for post-processing
        graphics::Render_Pass_Handle scene_render_pass_;
        graphics::Framebuffer_Handle scene_framebuffer_;

        // Final blit to swapchain
        graphics::Render_Pass_Handle blit_render_pass_;
        std::vector<graphics::Framebuffer_Handle> blit_framebuffers_;
        graphics::Graphics_Pipeline_Handle blit_pipeline_;
        graphics::Descriptor_Set_Layout_Handle blit_set_layout_;
        graphics::Descriptor_Set_Handle blit_set_;
        graphics::Sampler_Handle blit_sampler_;

        // Command execution
        graphics::Command_Pool_Handle command_pool_;
        std::vector<graphics::Command_Buffer_Handle> command_buffers_;
        graphics::Command_Queue_Handle graphics_queue_;

        // Synchronization
        std::vector<graphics::Fence_Handle> in_flight_fences_;
        std::vector<graphics::Semaphore_Handle> image_available_semaphores_;
        std::vector<graphics::Semaphore_Handle> render_finished_semaphores_;
        std::vector<uint64_t> fence_values_;

        // Frame tracking
        uint32_t current_frame_ = 0;
        uint32_t current_image_index_ = 0;
        bool frame_started_ = false;

        // Configuration
        Renderer_Desc desc_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        void* native_window_ = nullptr;

        // Custom rendering
        RenderCallback render_callback_;       // Scene rendering (PBR + skybox)
        RenderCallback pre_render_callback_;   // Shadow pass
        RenderCallback post_process_callback_; // Compute post-processing
        RenderCallback imgui_render_callback_; // ImGui overlay

        // Frame orchestration
        Frame_Pipeline frame_pipeline_{};
        Render_Targets render_targets_{};

        // Flags
        bool swapchain_needs_recreation_ = false;
        bool blit_passthrough_ = false;

        // Blit push constants
        struct Blit_Push_Constants
        {
            float exposure = 0.8f;
            uint32_t tone_map_mode = 0; // 0=ACES, 1=Reinhard
        };

        Blit_Push_Constants blit_pc_;
    };
}
