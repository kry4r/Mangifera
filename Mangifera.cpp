// app/main.cpp
#include "app/application.hpp"
#include "log/historiographer.hpp"
#include <exception>
#include <iostream>

using namespace mango;

class Test_Application : public app::Application
{
public:
    explicit Test_Application(const app::Application_Desc& desc)
        : Application(desc)
    {
    }

protected:
    void on_init() override
    {
        UH_INFO("Test application initialized");
        // Base class init_renderer() already sets the correct render callback
        // that calls render_scene() + render_imgui() with proper ImGui frame lifecycle.
    }

    void on_update(float delta_time) override
    {
        // Log FPS occasionally (every 5 seconds)
        static float time_accumulator = 0.0f;
        time_accumulator += delta_time;

        if (time_accumulator >= 5.0f) {
            UH_INFO_FMT("Application running: {:.1f} FPS, {} frames",
                get_fps(), get_frame_count());
            time_accumulator = 0.0f;
        }
    }

    void on_render() override
    {
    }

    void on_window_resize(uint32_t width, uint32_t height) override
    {
        UH_INFO_FMT("Application received resize: {}x{}", width, height);
    }

    void on_shutdown() override
    {
        UH_INFO("Test application shutting down");
    }
};

int main(int argc, char** argv)
{
    // Configure logger
    auto& logger = core::UkaLogger::instance();
    logger.set_level(core::LogLevel::UKA_INFO);
    logger.set_console_output(true);
    logger.set_file_output(true);
    logger.set_color_output(true);
    logger.set_async_mode(false); // Synchronous for debugging

    UH_INFO("=== Mango Engine - Black Window Test ===");

    try {
        // Create application descriptor
        app::Application_Desc app_desc{};
        app_desc.title = "Mango Engine - Test Window";
        app_desc.width = 1280;
        app_desc.height = 720;
        app_desc.enable_validation = true;
        app_desc.enable_vsync = true;
        app_desc.resizable = true;
        app_desc.graphics_backend = app::Graphics_Backend::Vulkan;
        app_desc.max_frames_in_flight = 2;

        // Create and run application
        Test_Application app(app_desc);
        app.run();

        UH_INFO("Application exited successfully");
        logger.flush();

        return 0;
    }
    catch (const std::exception& e) {
        UH_FATAL_FMT("Unhandled exception: {}", e.what());
        logger.flush();

        std::cerr << "\n=== FATAL ERROR ===\n"
                  << e.what() << "\n"
                  << "==================\n" << std::endl;

        return -1;
    }
    catch (...) {
        UH_FATAL("Unknown exception occurred");
        logger.flush();

        std::cerr << "\n=== FATAL ERROR ===\n"
                  << "Unknown exception\n"
                  << "==================\n" << std::endl;

        return -1;
    }
}
