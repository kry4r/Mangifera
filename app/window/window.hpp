// window.hpp
#pragma once
#include <string>
#include <functional>

struct GLFWwindow;

namespace mango::app
{
    struct Window_Desc
    {
        std::string title = "Mangifera";
        uint32_t width = 1280;
        uint32_t height = 720;
        bool resizable = true;
    };

    class Window
    {
    public:
        explicit Window(const Window_Desc& desc);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool should_close() const;
        void poll_events();

        auto get_width() const -> uint32_t { return width_; }
        auto get_height() const -> uint32_t { return height_; }
        auto get_native_window() const -> void* { return native_window_; }
        auto get_glfw_window() const -> GLFWwindow* { return native_window_; }  // 添加这个

        using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
        void set_resize_callback(ResizeCallback callback);

    private:
        void init_glfw();
        void create_window(const Window_Desc& desc);
        void cleanup();

        static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

        GLFWwindow* native_window_ = nullptr;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        ResizeCallback resize_callback_;
    };
}
