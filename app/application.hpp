// app/application/application.hpp
#pragma once
#include "window/window.hpp"
#include "renderer/renderer.hpp"
#include <memory>
#include <chrono>

namespace mango::app
{
    struct Application_Desc
    {
        std::string title = "Mangifera";
        uint32_t width = 1280;
        uint32_t height = 720;
        bool enable_validation = true;
        bool enable_vsync = true;
        bool resizable = true;
        Graphics_Backend graphics_backend = Graphics_Backend::Vulkan;
        uint32_t target_fps = 60;
        uint32_t max_frames_in_flight = 2;
    };

    class Application
    {
    public:
        explicit Application(const Application_Desc& desc);
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // Main application entry point
        void run();

        // Application control
        void request_exit();
        bool should_exit() const { return should_exit_; }

        // Getters
        auto get_window() -> Window* { return window_.get(); }
        auto get_renderer() -> Renderer* { return renderer_.get(); }
        auto get_delta_time() const -> float { return delta_time_; }
        auto get_fps() const -> float { return fps_; }
        auto get_frame_count() const -> uint64_t { return frame_count_; }

    protected:
        // Virtual methods for customization
        virtual void on_init() {}
        virtual void on_update(float delta_time) {}
        virtual void on_render() {}
        virtual void on_shutdown() {}
        virtual void on_window_resize(uint32_t width, uint32_t height) {}

    private:
        // Initialization
        void init();
        void init_window();
        void init_renderer();

        // Main loop
        void main_loop();
        void update(float delta_time);
        void render();

        // Cleanup
        void shutdown();
        void cleanup();

        // Event handlers
        void handle_window_resize(uint32_t width, uint32_t height);

        // Time management
        void update_time();
        void calculate_fps();

        // Core systems
        std::unique_ptr<Window> window_;
        std::unique_ptr<Renderer> renderer_;

        // Configuration
        Application_Desc desc_;

        // Time tracking
        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TimePoint start_time_;
        TimePoint last_frame_time_;
        TimePoint last_fps_update_time_;

        float delta_time_ = 0.0f;
        float fps_ = 0.0f;
        uint64_t frame_count_ = 0;
        uint32_t fps_frame_count_ = 0;

        // State
        bool initialized_ = false;
        bool should_exit_ = false;
        bool minimized_ = false;
    };
}
