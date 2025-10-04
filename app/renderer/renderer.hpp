// app/renderer/renderer.hpp
#pragma once
#include "device.hpp"
#include "render-pass/swapchain.hpp"
#include "render-pass/render-pass.hpp"
#include "render-pass/framebuffer.hpp"
#include "render-resource/texture.hpp"
#include "command-execution/command-pool.hpp"
#include "command-execution/command-buffer.hpp"
#include "command-execution/command-queue.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"
#include "manager/scene-graph.hpp"
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
        auto get_render_pass() -> graphics::Render_Pass_Handle { return render_pass_; }
        auto get_width() const -> uint32_t { return width_; }
        auto get_height() const -> uint32_t { return height_; }
        auto get_current_frame_index() const -> uint32_t { return current_frame_; }
        auto get_scene_graph() -> core::Scene_Graph* { return core::Scene_Graph::current_instance(); }

        // Callback for custom rendering
        using RenderCallback = std::function<void(graphics::Command_Buffer_Handle)>;
        void set_render_callback(RenderCallback callback);

    private:
        // Initialization
        void create_device();
        void create_swapchain();
        void create_depth_resources();
        void create_render_pass();
        void create_framebuffers();
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
        void transition_depth_image_layout();

        // Backend-specific device creation
        auto create_vulkan_device() -> graphics::Device_Handle;

        // Rendering state
        graphics::Device_Handle device_;
        graphics::Swapchain_Handle swapchain_;
        graphics::Render_Pass_Handle render_pass_;
        std::vector<graphics::Framebuffer_Handle> framebuffers_;
        graphics::Texture_Handle depth_image_;

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
        RenderCallback render_callback_;

        // Flags
        bool swapchain_needs_recreation_ = false;
    };
}
