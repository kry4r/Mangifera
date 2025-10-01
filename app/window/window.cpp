#include "window.hpp"
#include "log/historiographer.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>

namespace mango::app
{
    static void glfw_error_callback(int error_code, const char* description)
    {
        UH_ERROR_FMT("GLFW Error {}: {}", error_code, description);
    }

    Window::Window(const Window_Desc& desc)
        : width_(desc.width)
        , height_(desc.height)
    {
        init_glfw();
        create_window(desc);

        UH_INFO_FMT("Window created: {}x{} - {}", width_, height_, desc.title);
    }

    Window::~Window()
    {
        cleanup();
    }

    void Window::init_glfw()
    {
        glfwSetErrorCallback(glfw_error_callback);

        if (!glfwInit()) {
            UH_FATAL("Failed to initialize GLFW");
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        UH_INFO("GLFW initialized successfully");
    }

    void Window::create_window(const Window_Desc& desc)
    {
        glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

        native_window_ = glfwCreateWindow(
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            desc.title.c_str(),
            nullptr,  // monitor (nullptr = windowed mode)
            nullptr   // share (nullptr = no sharing)
        );

        if (!native_window_) {
            glfwTerminate();
            UH_FATAL("Failed to create GLFW window");
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(native_window_, this);

        glfwSetFramebufferSizeCallback(native_window_, framebuffer_size_callback);

        int fb_width, fb_height;
        glfwGetFramebufferSize(native_window_, &fb_width, &fb_height);
        width_ = static_cast<uint32_t>(fb_width);
        height_ = static_cast<uint32_t>(fb_height);

        UH_INFO_FMT("Window framebuffer size: {}x{}", width_, height_);
    }

    bool Window::should_close() const
    {
        return glfwWindowShouldClose(native_window_);
    }

    void Window::poll_events()
    {
        glfwPollEvents();
    }

    void Window::set_resize_callback(ResizeCallback callback)
    {
        resize_callback_ = std::move(callback);
    }

    void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        auto* app_window = static_cast<Window*>(glfwGetWindowUserPointer(window));

        if (!app_window) {
            UH_ERROR("Window user pointer is null in framebuffer size callback");
            return;
        }

        app_window->width_ = static_cast<uint32_t>(width);
        app_window->height_ = static_cast<uint32_t>(height);

        UH_INFO_FMT("Window resized: {}x{}", width, height);

        if (app_window->resize_callback_) {
            app_window->resize_callback_(app_window->width_, app_window->height_);
        }
    }

    void Window::cleanup()
    {
        if (native_window_) {
            glfwDestroyWindow(native_window_);
            native_window_ = nullptr;
            UH_INFO("Window destroyed");
        }

        glfwTerminate();
        UH_INFO("GLFW terminated");
    }

} // namespace mango::app
