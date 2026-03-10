// app/application/application.hpp
#pragma once
#include "window/window.hpp"
#include "renderer/renderer.hpp"
#include "manager/scene-graph.hpp"
#include "render-resource/buffer.hpp"
#include "render-resource/descriptor-set.hpp"
#include "pipeline-state/graphics-pipeline-state.hpp"
#include "resource/model.hpp"
#include "resource/mesh.hpp"
#include "resource/camera.hpp"
#include "resource/transform.hpp"
#include "ibl/ibl_generator.hpp"
#include "post_process/post_process_manager.hpp"
#include "physics/physics_world.hpp"
#include "render_core/run_mode.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <array>
#include <cstdint>

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
        Run_Mode run_mode = Run_Mode::runtime;
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

        auto scene_tree_window() -> void;
        auto display_scene_node(const std::shared_ptr<core::Scene_Node>& node) -> void;
        auto add_child_node(const std::shared_ptr<core::Scene_Node>& node) -> void;
        auto attach_twig_to_node(const std::shared_ptr<core::Scene_Node>& node) -> void;
        auto get_entity_from_scene_node(const std::shared_ptr<core::Scene_Node>& node) -> core::Entity;
        auto resource_window() -> void;
        auto render_ui() -> void;
        auto ensure_pbr_resources() -> void;
        auto render_scene(graphics::Command_Buffer_Handle cmd) -> void;
        auto create_default_camera_if_needed() -> void;
        auto create_default_scene() -> void;
        auto properties_window() -> void;
        auto edit_transform_component(core::Entity entity) -> void;
        auto edit_material_component(core::Entity entity) -> void;
        auto edit_camera_component(core::Entity entity) -> void;
        auto edit_light_component(core::Entity entity) -> void;
        auto edit_physics_body_component(core::Entity entity) -> void;
        auto ensure_shadow_resources() -> void;
        auto render_shadow_pass(graphics::Command_Buffer_Handle cmd) -> void;
        auto init_imgui() -> void;
        auto shutdown_imgui() -> void;
        auto render_imgui(graphics::Command_Buffer_Handle cmd) -> void;

        // Physics
        void set_physics_world(std::shared_ptr<physics::Physics_World> world);
        auto get_physics_world() -> physics::Physics_World* { return physics_world_.get(); }

    private:
        void physics_step(float delta_time);
        void sync_physics_transforms();
        void update_orbit_camera(float delta_time);

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
        bool imgui_initialized_ = false;

        struct Pbr_State
        {
            graphics::Graphics_Pipeline_Handle pipeline;
            graphics::Graphics_Pipeline_Handle skybox_pipeline;
            graphics::Descriptor_Set_Layout_Handle set_layout;
            graphics::Descriptor_Set_Handle set;
            graphics::Buffer_Handle camera_buffer;
            graphics::Buffer_Handle lights_buffer;
            bool ready = false;
        };

        struct Gpu_Mesh
        {
            graphics::Buffer_Handle vertex_buffer;
            graphics::Buffer_Handle index_buffer;
            uint32_t index_count = 0;
            bool indexed = false;
        };

        struct Shadow_State
        {
            graphics::Texture_Handle shadow_map;
            graphics::Render_Pass_Handle shadow_pass;
            graphics::Framebuffer_Handle shadow_framebuffer;
            graphics::Graphics_Pipeline_Handle shadow_pipeline;
            graphics::Descriptor_Set_Layout_Handle shadow_ubo_layout;  // set 0 for shadow pass
            graphics::Descriptor_Set_Handle shadow_ubo_set;
            graphics::Buffer_Handle shadow_ubo_buffer;
            graphics::Descriptor_Set_Layout_Handle shadow_sample_layout; // set 2 for PBR pass
            graphics::Descriptor_Set_Handle shadow_sample_set;
            graphics::Sampler_Handle shadow_sampler;
            math::Mat4 light_view_proj{1.0f};
            bool ready = false;
        };

        Pbr_State pbr_state_;
        Shadow_State shadow_state_;
        IBL_Resources ibl_resources_;
        std::unordered_map<std::size_t, Gpu_Mesh> mesh_cache_;
        std::unordered_map<std::uint32_t, Gpu_Mesh> entity_mesh_cache_;
        std::array<char, 260> model_path_input_{};
        std::array<char, 64> node_name_input_{};
        VkDescriptorPool imgui_descriptor_pool_ = VK_NULL_HANDLE;

        // Post-processing
        Post_Process_Manager post_process_manager_;

        // Physics
        std::shared_ptr<physics::Physics_World> physics_world_;

        // Debug visualization mode (0=RGB, 1=Normals, 2=Depth)
        int debug_mode_ = 0;

        // Feature toggles
        bool shadow_enabled_ = true;
        bool skybox_enabled_ = true;

        // Orbit camera
        float orbit_yaw_ = 0.0f;
        float orbit_pitch_ = 0.0f;
        float orbit_distance_ = 0.85f;
        math::Vec3 orbit_target_{0.276f, 0.275f, 0.10f};
        double last_mouse_x_ = 0.0;
        double last_mouse_y_ = 0.0;
        bool orbit_active_ = false;
        bool pan_active_ = false;

        // Scroll wheel zoom
        static inline float scroll_offset_y_ = 0.0f;
        static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    };
}
