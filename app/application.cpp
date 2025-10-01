// app/application/application.cpp
#include "application.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <thread>

namespace mango::app
{
    Application::Application(const Application_Desc& desc)
        : desc_(desc)
    {
        UH_INFO("=== Mangifera Starting ===");
        UH_INFO_FMT("Application: {}", desc_.title);

        try {
            init();
            initialized_ = true;
        }
        catch (const std::exception& e) {
            UH_FATAL_FMT("Failed to initialize application: {}", e.what());
            cleanup();
            throw;
        }
    }

    Application::~Application()
    {
        cleanup();
        UH_INFO("=== Mangifera Shutdown ===");
    }

    void Application::init()
    {
        UH_INFO("Initializing application...");

        // Initialize timing
        start_time_ = Clock::now();
        last_frame_time_ = start_time_;
        last_fps_update_time_ = start_time_;

        // Initialize window
        init_window();

        // Initialize renderer
        init_renderer();

        // Call user initialization
        on_init();

        UH_INFO("Application initialized successfully");
    }

    void Application::init_window()
    {
        UH_INFO("Initializing window...");

        Window_Desc window_desc{};
        window_desc.title = desc_.title;
        window_desc.width = desc_.width;
        window_desc.height = desc_.height;
        window_desc.resizable = desc_.resizable;

        window_ = std::make_unique<Window>(window_desc);

        // Set resize callback
        window_->set_resize_callback([this](uint32_t w, uint32_t h) {
            handle_window_resize(w, h);
        });

        UH_INFO("Window initialized");
    }

    void Application::init_renderer()
    {
        UH_INFO("Initializing renderer...");

        Renderer_Desc renderer_desc{};
        renderer_desc.backend = desc_.graphics_backend;
        renderer_desc.width = desc_.width;
        renderer_desc.height = desc_.height;
        renderer_desc.native_window = window_->get_native_window();
        renderer_desc.enable_validation = desc_.enable_validation;
        renderer_desc.enable_vsync = desc_.enable_vsync;
        renderer_desc.max_frames_in_flight = desc_.max_frames_in_flight;

        renderer_ = std::make_unique<Renderer>(renderer_desc);

        UH_INFO("Renderer initialized");
    }

    void Application::run()
    {
        if (!initialized_) {
            UH_ERROR("Cannot run application: not initialized");
            return;
        }

        UH_INFO("Entering main loop...");

        try {
            main_loop();
        }
        catch (const std::exception& e) {
            UH_FATAL_FMT("Fatal error in main loop: {}", e.what());
            throw;
        }

        shutdown();
    }

    void Application::main_loop()
    {
        while (!should_exit_ && !window_->should_close()) {
            // Update timing
            update_time();

            // Process window events
            window_->poll_events();

            // Skip frame if minimized
            if (minimized_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Update application logic
            update(delta_time_);

            // Render frame
            render();

            // Update frame counter
            frame_count_++;
            fps_frame_count_++;

            // Calculate FPS
            calculate_fps();
        }

        // Wait for all rendering to finish
        if (renderer_) {
            renderer_->wait_idle();
        }

        UH_INFO_FMT("Main loop finished (total frames: {})", frame_count_);
    }

    void Application::update(float delta_time)
    {
        // Call user update
        on_update(delta_time);
    }

    void Application::render()
    {
        // Call user render callback
        on_render();

        // Execute frame rendering
        if (renderer_) {
            renderer_->render_frame();
        }
    }

    void Application::update_time()
    {
        auto current_time = Clock::now();

        // Calculate delta time in seconds
        std::chrono::duration<float> delta = current_time - last_frame_time_;
        delta_time_ = delta.count();

        // Clamp delta time to prevent spiral of death
        if (delta_time_ > 0.1f) {
            delta_time_ = 0.1f;
        }

        last_frame_time_ = current_time;
    }

    void Application::calculate_fps()
    {
        auto current_time = Clock::now();
        std::chrono::duration<float> elapsed = current_time - last_fps_update_time_;

        // Update FPS every second
        if (elapsed.count() >= 1.0f) {
            fps_ = static_cast<float>(fps_frame_count_) / elapsed.count();
            fps_frame_count_ = 0;
            last_fps_update_time_ = current_time;

            // Log FPS
            UH_INFO_FMT("FPS: {:.1f} | Frame Time: {:.2f}ms",
                fps_, delta_time_ * 1000.0f);
        }
    }

    void Application::handle_window_resize(uint32_t width, uint32_t height)
    {
        UH_INFO_FMT("Window resize event: {}x{}", width, height);

        // Check if minimized
        if (width == 0 || height == 0) {
            minimized_ = true;
            UH_INFO("Window minimized");
            return;
        }

        minimized_ = false;

        // Update renderer
        if (renderer_) {
            renderer_->handle_resize(width, height);
        }

        // Call user callback
        on_window_resize(width, height);
    }

    void Application::request_exit()
    {
        should_exit_ = true;
        UH_INFO("Exit requested");
    }

    void Application::shutdown()
    {
        UH_INFO("Shutting down application...");

        // Call user shutdown
        on_shutdown();

        // Wait for renderer to finish
        if (renderer_) {
            renderer_->wait_idle();
        }

        UH_INFO("Application shutdown complete");
    }

    void Application::cleanup()
    {
        // Clean up in reverse order of creation
        renderer_.reset();
        window_.reset();

        initialized_ = false;
    }

} // namespace mango::app
