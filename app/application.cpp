// app/application/application.cpp
#include "application.hpp"
#include "log/historiographer.hpp"
#include "manager/world.hpp"
#include "resource/transform.hpp"
#include "resource/camera.hpp"
#include "resource/mesh.hpp"
#include "resource/model.hpp"
#include "resource/material.hpp"
#include "resource/primitives.hpp"
#include "resource/light.hpp"
#include "resource/physics_body.hpp"
#include "utils/shader-compiler.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-buffer.hpp"
#include "backends/vulkan/vk-device.hpp"
#include "vulkan-render-pass/vk-render-pass.hpp"
#include "vulkan-command-execution/vk-command-buffer.hpp"
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <thread>
#include <cstring>
#include <cstddef>
#include <filesystem>

namespace
{
    struct Camera_UBO
    {
        mango::math::Mat4 view;
        mango::math::Mat4 proj;
        mango::math::Mat4 view_proj;
        mango::math::Vec4 camera_pos;
    };

    struct Push_Constants
    {
        mango::math::Mat4 model;
        mango::math::Vec4 base_color;
        mango::math::Vec4 params;
    };

    static constexpr int MAX_LIGHTS = 8;

    struct Light_Data
    {
        mango::math::Vec4 position_type;    // xyz=position/direction, w=type (0=dir,1=point,2=spot)
        mango::math::Vec4 color_intensity;  // xyz=color, w=intensity
        mango::math::Vec4 params;           // xyz=spot_direction, w=range
        mango::math::Vec4 spot_params;      // x=inner_cos, y=outer_cos, zw=unused
    };

    struct Shadow_UBO
    {
        mango::math::Mat4 light_vp;
    };

    struct Lights_UBO
    {
        Light_Data lights[MAX_LIGHTS];
        mango::math::Vec4 light_count; // x=count, y=ibl_intensity, z=debug_mode
        mango::math::Mat4 shadow_view_proj;
    };

    auto pbr_shader_path(const char* filename) -> std::string
    {
        auto base = std::filesystem::path(__FILE__).parent_path();
        return (base / "shaders" / filename).string();
    }
}

namespace mango::app
{
    void Application::glfw_scroll_callback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset)
    {
        scroll_offset_y_ += static_cast<float>(yoffset);
    }

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
        if (desc_.run_mode == Run_Mode::headless) {
            UH_INFO("Application configured for headless compatibility mode");
        }

        // Initialize timing
        start_time_ = Clock::now();
        last_frame_time_ = start_time_;
        last_fps_update_time_ = start_time_;

        // Initialize window
        init_window();

        // Initialize renderer
        init_renderer();

        init_imgui();

        // Call user initialization
        on_init();

        // Create default scene if nothing was set up by user
        create_default_scene();

        // Initialize PBR pipeline and IBL resources up-front
        // (must happen outside the render callback to avoid submitting
        //  compute commands while a frame command buffer is recording)
        create_default_camera_if_needed();
        ensure_pbr_resources();

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
        window_->set_resize_callback([this] (uint32_t w, uint32_t h) {
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

        renderer_->set_pre_render_callback([this](graphics::Command_Buffer_Handle cmd) {
            render_shadow_pass(cmd);
        });

        renderer_->set_render_callback([this](graphics::Command_Buffer_Handle cmd) {
            render_scene(cmd);
        });

        renderer_->set_post_process_callback([this](graphics::Command_Buffer_Handle cmd) {
            if (post_process_manager_.is_ready()) {
                post_process_manager_.set_delta_time(delta_time_);
                post_process_manager_.set_frame_index(static_cast<uint32_t>(frame_count_));

                // Pass projection matrix to post-process (needed for SSAO)
                auto* world = core::World::current_instance();
                if (world) {
                    auto* cam_store = world->get_twig_storage<resource::Camera>();
                    auto* t_store = world->get_twig_storage<resource::Transform>();
                    if (cam_store && !cam_store->data.empty() && t_store) {
                        auto& cam_pair = *cam_store->data.begin();
                        auto& cam = cam_pair.second;
                        auto proj = cam.get_projection_matrix();
                        post_process_manager_.set_projection_matrix(&proj[0][0]);
                        auto inv_proj = glm::inverse(proj);
                        post_process_manager_.set_inv_projection_matrix(&inv_proj[0][0]);
                        auto t_it = t_store->data.find(cam_pair.first);
                        if (t_it != t_store->data.end()) {
                            auto view = cam.get_view_matrix(t_it->second);
                            post_process_manager_.set_view_matrix(&view[0][0]);

                            // Inverse view-projection for volumetric light
                            auto inv_vp = glm::inverse(proj * view);
                            post_process_manager_.set_inv_view_proj(&inv_vp[0][0]);
                        }
                    }

                    // Find first point/spot light for volumetric
                    auto* light_store = world->get_twig_storage<resource::Light>();
                    if (light_store && t_store) {
                        for (auto& [entity, light] : light_store->data) {
                            if (light.type == resource::Light_Type::point || light.type == resource::Light_Type::spot) {
                                auto lt_it = t_store->data.find(entity);
                                if (lt_it != t_store->data.end()) {
                                    auto& pos = lt_it->second.position;
                                    post_process_manager_.set_light_position(pos.x, pos.y, pos.z);
                                    post_process_manager_.set_light_color(light.color.x, light.color.y, light.color.z, light.intensity);
                                    break;
                                }
                            }
                        }
                    }
                }

                // Pass shadow map and light data for volumetric light
                if (shadow_state_.ready && shadow_state_.shadow_map) {
                    post_process_manager_.set_shadow_map(shadow_state_.shadow_map);
                    post_process_manager_.set_shadow_sampler(shadow_state_.shadow_sampler);
                    post_process_manager_.set_shadow_view_proj(&shadow_state_.light_view_proj[0][0]);
                }

                post_process_manager_.execute(cmd,
                    renderer_->get_hdr_color_texture(),
                    renderer_->get_depth_texture(),
                    renderer_->get_gbuffer_normal_texture());

                // Switch blit to read from post-processed output
                auto output = post_process_manager_.get_output_texture();
                if (output) {
                    renderer_->set_blit_source(output);
                    renderer_->set_blit_passthrough(true);
                } else {
                    renderer_->set_blit_source(renderer_->get_hdr_color_texture());
                }
            } else {
                // Fallback: blit reads raw HDR with built-in tone mapping
                renderer_->set_blit_source(renderer_->get_hdr_color_texture());
            }
        });

        renderer_->set_imgui_render_callback([this](graphics::Command_Buffer_Handle cmd) {
            render_imgui(cmd);
        });

        // Initialize post-process manager
        post_process_manager_.init(
            renderer_->get_device(),
            renderer_->get_command_pool(),
            renderer_->get_graphics_queue(),
            desc_.width, desc_.height);

        UH_INFO("Renderer initialized");
    }

    void Application::run()
    {
        if (!initialized_) {
            UH_ERROR("Cannot run application: not initialized");
            return;
        }

        UH_INFO("Entering main loop...");
        if (desc_.run_mode == Run_Mode::headless) {
            UH_INFO("Application main loop is running in headless compatibility mode");
        }

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
        // Step physics before game logic
        physics_step(delta_time);

        // Update camera from input
        update_orbit_camera(delta_time);

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

        // Update post-process manager
        post_process_manager_.resize(width, height);

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
        // Wait for all GPU work to finish BEFORE destroying any resources
        if (renderer_) {
            renderer_->wait_idle();
        }

        shutdown_imgui();
        pbr_state_ = {};
        ibl_resources_ = {};
        mesh_cache_.clear();
        entity_mesh_cache_.clear();

        // Clean up in reverse order of creation
        renderer_.reset();
        window_.reset();

        initialized_ = false;
    }

    auto Application::scene_tree_window() -> void
    {
        if (!ImGui::Begin("Scene Tree")) {
            ImGui::End();
            return;
        }

        auto scene_graph = get_renderer()->get_scene_graph();
        if (scene_graph) {
            display_scene_node(scene_graph->get_root_node());
        }

        ImGui::End();
    }

    auto Application::display_scene_node(const std::shared_ptr<core::Scene_Node>& node) -> void
    {
        if (!node) {
            return;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool is_open = ImGui::TreeNodeEx((void*) (uintptr_t) node->id, flags, "%s", node->name.c_str());

        if (ImGui::IsItemClicked()) {
            auto scene_graph = get_renderer()->get_scene_graph();
            if (scene_graph) {
                scene_graph->set_current_selected_node(node);
            }
        }

        if (ImGui::IsItemClicked(1)) {
            std::string menu_id = "NodeMenu##" + std::to_string(node->id);
            ImGui::OpenPopup(menu_id.c_str());
        }

        std::string menu_id = "NodeMenu##" + std::to_string(node->id);
        if (ImGui::BeginPopup(menu_id.c_str())) {
            if (ImGui::MenuItem("Add Child")) {
                add_child_node(node);
            }
            if (ImGui::MenuItem("Attach Twig")) {
                std::string twig_menu_id = "TwigMenu##" + std::to_string(node->id);
                ImGui::OpenPopup(twig_menu_id.c_str());
            }
            ImGui::EndPopup();
        }

        attach_twig_to_node(node);

        if (is_open) {
            auto child = node->children;
            while (child) {
                display_scene_node(child);
                child = child->next;
            }
            ImGui::TreePop();
        }
    }

    auto Application::add_child_node(const std::shared_ptr<core::Scene_Node>& node) -> void
    {
        auto world = core::World::current_instance();

        core::Entity new_entity = world->create_entity();

        auto scene_graph = get_renderer()->get_scene_graph();
        if (scene_graph) {
            scene_graph->add_entity_to_scene(new_entity, node);
        }

        world->attach_twig(new_entity, resource::Transform());
    }

    auto Application::get_entity_from_scene_node(const std::shared_ptr<core::Scene_Node>& node) -> core::Entity
    {
        if (!node) {
            return core::INVALID_ENTITY;
        }

        for (const auto& pair : renderer_->get_scene_graph()->get_entities_mapping()) {
            if (pair.second && pair.second->id == node->id) {
                return pair.first;
            }
        }
        return core::INVALID_ENTITY;
    }

    auto Application::attach_twig_to_node(const std::shared_ptr<core::Scene_Node>& node) -> void
    {
        auto world = core::World::current_instance();

        core::Entity entity = get_entity_from_scene_node(node);
        if (!world->is_entity_valid(entity)) {
            return;
        }

        std::string menu_id = "TwigMenu##" + std::to_string(node->id);
        if (ImGui::BeginPopup(menu_id.c_str())) {
            if (ImGui::MenuItem("Transform")) {
                if (!world->has_twig<resource::Transform>(entity)) {
                    world->attach_twig(entity, resource::Transform());
                }
            }
            if (ImGui::MenuItem("Mesh")) {
                if (!world->has_twig<resource::Mesh>(entity)) {
                    resource::Mesh mesh;
                    std::vector<resource::Vertex> verts = {
                        { { -0.5f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
                        { {  0.5f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
                        { {  0.0f, 0.0f, 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f } }
                    };
                    std::vector<std::uint32_t> inds = { 0, 1, 2 };
                    mesh.set_vertices(verts);
                    mesh.set_indices(inds);
                    world->attach_twig(entity, mesh);
                }
            }
            if (ImGui::MenuItem("PBR Material")) {
                if (!world->has_twig<resource::Pbr_Material>(entity)) {
                    world->attach_twig(entity, resource::Pbr_Material());
                }
            }
            if (ImGui::MenuItem("Light")) {
            }
            ImGui::EndPopup();
        }
    }

    auto Application::resource_window() -> void
    {
        if (!ImGui::Begin("Resources")) {
            ImGui::End();
            return;
        }

        ImGui::InputText("Node Name", node_name_input_.data(), node_name_input_.size());
        ImGui::InputText("Model Path", model_path_input_.data(), model_path_input_.size());

        auto scene_graph = get_renderer()->get_scene_graph();
        auto selected = scene_graph ? scene_graph->get_current_selected_node() : nullptr;

        if (ImGui::Button("Add Node")) {
            auto world = core::World::current_instance();
            core::Entity entity = world->create_entity();
            world->attach_twig(entity, resource::Transform());

            std::string name = node_name_input_.data();
            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, selected, name);
            }
        }

        if (ImGui::Button("Add Camera")) {
            auto world = core::World::current_instance();
            core::Entity entity = world->create_entity();
            world->attach_twig(entity, resource::Transform());
            world->attach_twig(entity, resource::Camera());

            std::string name = node_name_input_.data();
            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, selected, name.empty() ? "Camera" : name);
            }
        }

        if (ImGui::Button("Load Model")) {
            std::string path = model_path_input_.data();
            if (!path.empty()) {
                auto model = resource::load_model_from_obj(path);
                if (model) {
                    auto world = core::World::current_instance();
                    core::Entity entity = get_entity_from_scene_node(selected);
                    if (!world->is_entity_valid(entity)) {
                        entity = world->create_entity();
                        world->attach_twig(entity, resource::Transform());
                        if (scene_graph) {
                            std::string name = node_name_input_.data();
                            scene_graph->add_entity_to_scene(entity, selected, name.empty() ? "Model" : name);
                        }
                    }
                    world->attach_twig(entity, *model);
                }
            }
        }

        ImGui::End();
    }

    auto Application::render_ui() -> void
    {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        scene_tree_window();
        resource_window();
        properties_window();

        // Post-processing settings
        post_process_manager_.render_settings_ui();

        // Debug visualization window
        if (ImGui::Begin("Render Debug")) {
            const char* modes[] = {"RGB", "Normals", "Depth"};
            ImGui::Combo("View Mode", &debug_mode_, modes, 3);
            ImGui::Separator();
            ImGui::Checkbox("Shadows", &shadow_enabled_);
            ImGui::Checkbox("Skybox", &skybox_enabled_);
        }
        ImGui::End();
    }

    auto Application::init_imgui() -> void
    {
        if (imgui_initialized_) {
            return;
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        auto glfw_window = window_->get_glfw_window();
        // Set scroll callback before ImGui (ImGui chains to PrevUserCallback)
        glfwSetScrollCallback(glfw_window, glfw_scroll_callback);
        ImGui_ImplGlfw_InitForVulkan(glfw_window, true);

        auto device = std::dynamic_pointer_cast<graphics::vk::Vk_Device>(renderer_->get_device());
        auto vk_render_pass = std::dynamic_pointer_cast<graphics::vk::Vk_Render_Pass>(renderer_->get_render_pass());
        if (!device || !vk_render_pass) {
            UH_ERROR("Failed to initialize ImGui: Vulkan device or render pass is invalid");
            return;
        }

        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(device->get_vk_device(), &pool_info, nullptr, &imgui_descriptor_pool_) != VK_SUCCESS) {
            UH_ERROR("Failed to create ImGui descriptor pool");
            return;
        }

        uint32_t image_count = renderer_->get_swapchain_image_count();
        if (image_count < 2) {
            image_count = 2;
        }

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = VK_API_VERSION_1_3;
        init_info.Instance = device->get_vk_instance();
        init_info.PhysicalDevice = device->get_vk_physical_device();
        init_info.Device = device->get_vk_device();
        init_info.QueueFamily = device->get_graphics_queue_family();
        init_info.Queue = device->get_graphics_queue();
        init_info.DescriptorPool = imgui_descriptor_pool_;
        init_info.MinImageCount = image_count;
        init_info.ImageCount = image_count;
        init_info.PipelineInfoMain.RenderPass = vk_render_pass->get_vk_render_pass();
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.CheckVkResultFn = [](VkResult err) {
            if (err != VK_SUCCESS) {
                UH_ERROR_FMT("ImGui Vulkan error: {}", (int)err);
            }
        };

        ImGui_ImplVulkan_Init(&init_info);

        imgui_initialized_ = true;
        UH_INFO("ImGui initialized");
    }

    auto Application::shutdown_imgui() -> void
    {
        if (!imgui_initialized_) {
            return;
        }
        auto device = std::dynamic_pointer_cast<graphics::vk::Vk_Device>(renderer_ ? renderer_->get_device() : nullptr);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (device && imgui_descriptor_pool_) {
            vkDestroyDescriptorPool(device->get_vk_device(), imgui_descriptor_pool_, nullptr);
            imgui_descriptor_pool_ = VK_NULL_HANDLE;
        }
        imgui_initialized_ = false;
        UH_INFO("ImGui shutdown complete");
    }

    auto Application::render_imgui(graphics::Command_Buffer_Handle cmd) -> void
    {
        if (!imgui_initialized_) {
            return;
        }
        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();
        render_ui();
        ImGui::Render();
        auto vk_cmd = std::dynamic_pointer_cast<graphics::vk::Vk_Command_Buffer>(cmd);
        if (!vk_cmd) {
            return;
        }
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vk_cmd->get_vk_command_buffer());
    }

    auto Application::properties_window() -> void
    {
        if (!ImGui::Begin("Properties")) {
            ImGui::End();
            return;
        }

        auto scene_graph = get_renderer()->get_scene_graph();
        auto selected = scene_graph ? scene_graph->get_current_selected_node() : nullptr;

        if (!selected) {
            ImGui::TextDisabled("No node selected");
            ImGui::End();
            return;
        }

        auto world = core::World::current_instance();
        core::Entity entity = get_entity_from_scene_node(selected);

        if (!world->is_entity_valid(entity)) {
            ImGui::TextDisabled("Invalid entity");
            ImGui::End();
            return;
        }

        ImGui::Text("Node: %s", selected->name.c_str());
        ImGui::Separator();

        edit_transform_component(entity);
        edit_material_component(entity);
        edit_camera_component(entity);
        edit_light_component(entity);
        edit_physics_body_component(entity);

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Add Component...")) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!world->has_twig<resource::Transform>(entity) && ImGui::MenuItem("Transform")) {
                world->attach_twig(entity, resource::Transform());
            }
            if (!world->has_twig<resource::Pbr_Material>(entity) && ImGui::MenuItem("PBR Material")) {
                world->attach_twig(entity, resource::Pbr_Material());
            }
            if (!world->has_twig<resource::Light>(entity) && ImGui::MenuItem("Light")) {
                world->attach_twig(entity, resource::Light());
            }
            if (!world->has_twig<resource::Camera>(entity) && ImGui::MenuItem("Camera")) {
                world->attach_twig(entity, resource::Camera());
            }
            if (!world->has_twig<resource::Physics_Body>(entity) && ImGui::MenuItem("Physics Body")) {
                world->attach_twig(entity, resource::Physics_Body());
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    auto Application::edit_transform_component(core::Entity entity) -> void
    {
        auto world = core::World::current_instance();
        if (!world->has_twig<resource::Transform>(entity)) return;
        auto& transform = world->get_twig<resource::Transform>(entity);

        bool header_open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);

        if (header_open) {
            ImGui::DragFloat3("Position", &transform.position.x, 0.1f);

            glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
            if (ImGui::DragFloat3("Rotation", &euler.x, 1.0f)) {
                transform.rotation = glm::quat(glm::radians(euler));
            }

            ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.001f, 100.0f);
        }
    }

    auto Application::edit_material_component(core::Entity entity) -> void
    {
        auto world = core::World::current_instance();
        if (!world->has_twig<resource::Pbr_Material>(entity)) return;
        auto& material = world->get_twig<resource::Pbr_Material>(entity);

        bool header_open = ImGui::CollapsingHeader("PBR Material", ImGuiTreeNodeFlags_DefaultOpen);

        if (header_open) {
            ImGui::ColorEdit4("Base Color", &material.base_color.x);
            bool changed = false;
            changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.05f, 1.0f);
            changed |= ImGui::SliderFloat("AO", &material.ao, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Emissive", &material.emissive_strength, 0.0f, 10.0f);
            if (material.emissive_strength > 0.0f) {
                ImGui::ColorEdit3("Emissive Color", &material.emissive_color.x);
            }
            if (changed) {
                material.sync_params();
            }
        }
    }

    auto Application::edit_camera_component(core::Entity entity) -> void
    {
        auto world = core::World::current_instance();
        if (!world->has_twig<resource::Camera>(entity)) return;
        auto& camera = world->get_twig<resource::Camera>(entity);

        bool header_open = ImGui::CollapsingHeader("Camera");

        if (header_open) {
            ImGui::SliderFloat("FOV", &camera.fov, 10.0f, 120.0f);
            ImGui::DragFloat("Near Plane", &camera.near_plane, 0.01f, 0.001f, 10.0f);
            ImGui::DragFloat("Far Plane", &camera.far_plane, 1.0f, 1.0f, 10000.0f);
        }
    }

    auto Application::edit_light_component(core::Entity entity) -> void
    {
        auto world = core::World::current_instance();
        if (!world->has_twig<resource::Light>(entity)) return;
        auto& light = world->get_twig<resource::Light>(entity);

        bool header_open = ImGui::CollapsingHeader("Light");

        if (header_open) {
            const char* types[] = {"Directional", "Point", "Spot"};
            int type_idx = static_cast<int>(light.type);
            if (ImGui::Combo("Type", &type_idx, types, 3)) {
                light.type = static_cast<resource::Light_Type>(type_idx);
            }
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

            if (light.type == resource::Light_Type::directional) {
                ImGui::DragFloat3("Direction", &light.direction.x, 0.01f);
            }
            if (light.type == resource::Light_Type::point || light.type == resource::Light_Type::spot) {
                ImGui::DragFloat("Range", &light.range, 0.1f, 0.1f, 1000.0f);
            }
            if (light.type == resource::Light_Type::spot) {
                ImGui::SliderFloat("Inner Angle", &light.inner_angle, 1.0f, 90.0f);
                ImGui::SliderFloat("Outer Angle", &light.outer_angle, 1.0f, 90.0f);
            }
        }
    }

    auto Application::edit_physics_body_component(core::Entity entity) -> void
    {
        auto world = core::World::current_instance();
        if (!world->has_twig<resource::Physics_Body>(entity)) return;
        auto& body = world->get_twig<resource::Physics_Body>(entity);

        bool header_open = ImGui::CollapsingHeader("Physics Body");

        if (header_open) {
            const char* body_types[] = {"Static", "Dynamic", "Kinematic"};
            int bt = static_cast<int>(body.type);
            if (ImGui::Combo("Body Type", &bt, body_types, 3)) {
                body.type = static_cast<physics::Body_Type>(bt);
            }

            const char* shape_types[] = {"Box", "Sphere", "Capsule"};
            int st = static_cast<int>(body.shape_type);
            if (ImGui::Combo("Shape", &st, shape_types, 3)) {
                body.shape_type = static_cast<physics::Shape_Type>(st);
            }

            if (body.shape_type == physics::Shape_Type::box) {
                ImGui::DragFloat3("Half Extents", &body.shape_half_extents.x, 0.01f, 0.01f, 100.0f);
            }
            if (body.shape_type == physics::Shape_Type::sphere) {
                ImGui::DragFloat("Radius", &body.shape_radius, 0.01f, 0.01f, 100.0f);
            }
            if (body.shape_type == physics::Shape_Type::capsule) {
                ImGui::DragFloat("Radius##cap", &body.shape_radius, 0.01f, 0.01f, 100.0f);
                ImGui::DragFloat("Half Height", &body.shape_half_height, 0.01f, 0.01f, 100.0f);
            }

            if (body.type != physics::Body_Type::static_body) {
                ImGui::DragFloat("Mass", &body.mass, 0.1f, 0.001f, 10000.0f);
            }
            ImGui::DragFloat("Friction", &body.friction, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Restitution", &body.restitution, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Linear Damping", &body.linear_damping, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Angular Damping", &body.angular_damping, 0.001f, 0.0f, 1.0f);
        }
    }

    auto Application::ensure_pbr_resources() -> void
    {
        if (pbr_state_.ready || !renderer_) {
            return;
        }

        auto device = renderer_->get_device();
        if (!device) {
            return;
        }

        auto vs_spv = graphics::utils::compile_shader_form_file(pbr_shader_path("pbr.vert"), shaderc_vertex_shader);
        auto fs_spv = graphics::utils::compile_shader_form_file(pbr_shader_path("pbr.frag"), shaderc_fragment_shader);
        if (vs_spv.empty() || fs_spv.empty()) {
            return;
        }

        graphics::Shader_Desc vs_desc{};
        vs_desc.type = graphics::Shader_Type::vertex;
        vs_desc.bytecode = std::move(vs_spv);

        graphics::Shader_Desc fs_desc{};
        fs_desc.type = graphics::Shader_Type::fragment;
        fs_desc.bytecode = std::move(fs_spv);

        auto vs = device->create_shader(vs_desc);
        auto fs = device->create_shader(fs_desc);
        if (!vs || !fs) {
            return;
        }

        graphics::Descriptor_Set_Layout_Desc set_layout_desc{};
        graphics::Descriptor_Binding camera_binding{};
        camera_binding.binding = 0;
        camera_binding.type = graphics::Descriptor_Type::uniform_buffer;
        camera_binding.count = 1;
        camera_binding.shader_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        set_layout_desc.bindings.push_back(camera_binding);

        graphics::Descriptor_Binding lights_binding{};
        lights_binding.binding = 1;
        lights_binding.type = graphics::Descriptor_Type::uniform_buffer;
        lights_binding.count = 1;
        lights_binding.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
        set_layout_desc.bindings.push_back(lights_binding);

        pbr_state_.set_layout = device->create_descriptor_set_layout(set_layout_desc);
        pbr_state_.set = device->create_descriptor_set(pbr_state_.set_layout);

        graphics::Buffer_Desc camera_desc{};
        camera_desc.size = sizeof(Camera_UBO);
        camera_desc.usage = graphics::Buffer_Type::uniform;
        camera_desc.memory = graphics::Memory_Type::cpu2gpu;
        pbr_state_.camera_buffer = device->create_buffer(camera_desc);

        graphics::Buffer_Desc lights_desc{};
        lights_desc.size = sizeof(Lights_UBO);
        lights_desc.usage = graphics::Buffer_Type::uniform;
        lights_desc.memory = graphics::Memory_Type::cpu2gpu;
        pbr_state_.lights_buffer = device->create_buffer(lights_desc);

        if (pbr_state_.set && pbr_state_.camera_buffer && pbr_state_.lights_buffer) {
            graphics::Descriptor_Write cam_write{};
            cam_write.binding = 0;
            cam_write.type = graphics::Descriptor_Type::uniform_buffer;
            cam_write.buffers = { pbr_state_.camera_buffer };
            cam_write.buffer_offsets = { 0 };
            cam_write.buffer_ranges = { sizeof(Camera_UBO) };

            graphics::Descriptor_Write light_write{};
            light_write.binding = 1;
            light_write.type = graphics::Descriptor_Type::uniform_buffer;
            light_write.buffers = { pbr_state_.lights_buffer };
            light_write.buffer_offsets = { 0 };
            light_write.buffer_ranges = { sizeof(Lights_UBO) };

            pbr_state_.set->update({ cam_write, light_write });
        }

        graphics::Graphics_Pipeline_Desc pipeline_desc{};
        pipeline_desc.vertex_shader = vs;
        pipeline_desc.fragment_shader = fs;
        pipeline_desc.render_pass = renderer_->get_scene_render_pass();
        pipeline_desc.subpass = 0;
        pipeline_desc.color_attachment_count = 2; // HDR color + G-buffer normal
        pipeline_desc.rasterizer_state.cull_enable = false;
        pipeline_desc.rasterizer_state.front_ccw = true;
        pipeline_desc.depth_stencil_state.depth_test_enable = true;
        pipeline_desc.depth_stencil_state.depth_write_enable = true;
        pipeline_desc.blend_state.blend_enable = false;
        // Generate IBL resources
        if (!ibl_resources_.ready) {
            auto cmd_pool = renderer_->get_command_pool();
            auto cmd_queue = renderer_->get_graphics_queue();
            if (cmd_pool && cmd_queue) {
                // Try EXR-based IBL first, fall back to procedural
                auto exr_path = std::filesystem::path(__FILE__).parent_path().parent_path() / "asset" / "texture" / "brown_photostudio.exr";
                if (std::filesystem::exists(exr_path)) {
                    ibl_resources_ = IBL_Generator::generate_all_from_exr(device, cmd_pool, cmd_queue, exr_path.string());
                } else {
                    ibl_resources_ = IBL_Generator::generate_all(device, cmd_pool, cmd_queue);
                }
            }
        }

        // Shadow map descriptor set layout (set 2): sampler2DShadow
        graphics::Descriptor_Set_Layout_Desc shadow_sample_layout_desc{};
        graphics::Descriptor_Binding shadow_tex_binding{};
        shadow_tex_binding.binding = 0;
        shadow_tex_binding.type = graphics::Descriptor_Type::combined_image_sampler;
        shadow_tex_binding.count = 1;
        shadow_tex_binding.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
        shadow_sample_layout_desc.bindings.push_back(shadow_tex_binding);
        shadow_state_.shadow_sample_layout = device->create_descriptor_set_layout(shadow_sample_layout_desc);

        // Set up pipeline with IBL (set 1) + shadow (set 2) descriptor sets
        if (ibl_resources_.ready && ibl_resources_.ibl_set_layout && shadow_state_.shadow_sample_layout) {
            pipeline_desc.descriptor_set_layouts = { pbr_state_.set_layout, ibl_resources_.ibl_set_layout, shadow_state_.shadow_sample_layout };
        } else if (ibl_resources_.ready && ibl_resources_.ibl_set_layout) {
            pipeline_desc.descriptor_set_layouts = { pbr_state_.set_layout, ibl_resources_.ibl_set_layout };
        } else {
            pipeline_desc.descriptor_set_layouts = { pbr_state_.set_layout };
        }

        graphics::Push_Constant_Range pc_range{};
        pc_range.offset = 0;
        pc_range.size = sizeof(Push_Constants);
        pc_range.shader_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_desc.push_constants.push_back(pc_range);

        graphics::Vertex_Attribute pos{};
        pos.semantic = "POSITION";
        pos.location = 0;
        pos.offset = offsetof(resource::Vertex, position);
        pos.stride = sizeof(resource::Vertex);
        pipeline_desc.vertex_attributes.push_back(pos);

        graphics::Vertex_Attribute normal{};
        normal.semantic = "NORMAL";
        normal.location = 1;
        normal.offset = offsetof(resource::Vertex, normal);
        normal.stride = sizeof(resource::Vertex);
        pipeline_desc.vertex_attributes.push_back(normal);

        graphics::Vertex_Attribute uv{};
        uv.semantic = "TEXCOORD0";
        uv.location = 2;
        uv.offset = offsetof(resource::Vertex, uv);
        uv.stride = sizeof(resource::Vertex);
        pipeline_desc.vertex_attributes.push_back(uv);

        pbr_state_.pipeline = device->create_graphics_pipeline(pipeline_desc);

        // Create skybox pipeline
        if (ibl_resources_.ready) {
            auto skybox_vs_spv = graphics::utils::compile_shader_form_file(pbr_shader_path("skybox.vert"), shaderc_vertex_shader);
            auto skybox_fs_spv = graphics::utils::compile_shader_form_file(pbr_shader_path("skybox.frag"), shaderc_fragment_shader);
            if (!skybox_vs_spv.empty() && !skybox_fs_spv.empty()) {
                graphics::Shader_Desc skybox_vs_desc{};
                skybox_vs_desc.type = graphics::Shader_Type::vertex;
                skybox_vs_desc.bytecode = std::move(skybox_vs_spv);
                auto skybox_vs = device->create_shader(skybox_vs_desc);

                graphics::Shader_Desc skybox_fs_desc{};
                skybox_fs_desc.type = graphics::Shader_Type::fragment;
                skybox_fs_desc.bytecode = std::move(skybox_fs_spv);
                auto skybox_fs = device->create_shader(skybox_fs_desc);

                if (skybox_vs && skybox_fs) {
                    graphics::Graphics_Pipeline_Desc skybox_desc{};
                    skybox_desc.vertex_shader = skybox_vs;
                    skybox_desc.fragment_shader = skybox_fs;
                    skybox_desc.render_pass = renderer_->get_scene_render_pass();
                    skybox_desc.subpass = 0;
                    skybox_desc.color_attachment_count = 2; // HDR color + G-buffer normal
                    skybox_desc.rasterizer_state.cull_enable = false;
                    skybox_desc.depth_stencil_state.depth_test_enable = false;
                    skybox_desc.depth_stencil_state.depth_write_enable = false;
                    skybox_desc.blend_state.blend_enable = false;
                    skybox_desc.descriptor_set_layouts = { pbr_state_.set_layout, ibl_resources_.ibl_set_layout };
                    // No vertex attributes (fullscreen triangle generated in shader)
                    // No push constants needed

                    pbr_state_.skybox_pipeline = device->create_graphics_pipeline(skybox_desc);
                    if (pbr_state_.skybox_pipeline) {
                        UH_INFO("Skybox pipeline created");
                    }
                }
            }
        }

        pbr_state_.ready = pbr_state_.pipeline && pbr_state_.camera_buffer && pbr_state_.lights_buffer && pbr_state_.set;

        // Initialize shadow mapping resources
        if (pbr_state_.ready) {
            ensure_shadow_resources();
        }
    }

    auto Application::ensure_shadow_resources() -> void
    {
        if (shadow_state_.ready || !renderer_) return;

        auto device = renderer_->get_device();
        if (!device) return;

        // 1. Create shadow map texture (depth-only, 2048x2048)
        graphics::Texture_Desc shadow_tex_desc{};
        shadow_tex_desc.dimension = graphics::Texture_Kind::tex_2d;
        shadow_tex_desc.format = graphics::Texture_Format::depth32f;
        shadow_tex_desc.width = 2048;
        shadow_tex_desc.height = 2048;
        shadow_tex_desc.depth = 1;
        shadow_tex_desc.mip_levels = 1;
        shadow_tex_desc.arrayLayers = 1;
        shadow_tex_desc.sampled = true;
        shadow_tex_desc.render_target = true;
        shadow_state_.shadow_map = device->create_texture(shadow_tex_desc);
        if (!shadow_state_.shadow_map) {
            UH_ERROR("Failed to create shadow map texture");
            return;
        }

        // 2. Create shadow render pass (depth-only, no color attachment)
        graphics::Render_Pass_Desc shadow_rp_desc{};
        graphics::Attachment_Desc shadow_depth_att{};
        shadow_depth_att.texture = shadow_state_.shadow_map;
        shadow_depth_att.load_op = 1;  // Clear
        shadow_depth_att.store_op = 0; // Store
        shadow_depth_att.initial_state = 0; // Undefined
        shadow_depth_att.final_state = 4;   // Shader resource (for sampling in PBR)
        shadow_rp_desc.attachments.push_back(shadow_depth_att);

        graphics::Subpass_Desc shadow_subpass{};
        shadow_subpass.depth_stencil_attachment = 0;
        shadow_rp_desc.subpasses.push_back(shadow_subpass);

        shadow_state_.shadow_pass = device->create_render_pass(shadow_rp_desc);
        if (!shadow_state_.shadow_pass) {
            UH_ERROR("Failed to create shadow render pass");
            return;
        }

        // 3. Create shadow framebuffer
        graphics::Framebuffer_Desc shadow_fb_desc{};
        shadow_fb_desc.render_pass = shadow_state_.shadow_pass;
        shadow_fb_desc.attachments.push_back(shadow_state_.shadow_map);
        shadow_fb_desc.width = 2048;
        shadow_fb_desc.height = 2048;
        shadow_fb_desc.layers = 1;
        shadow_state_.shadow_framebuffer = device->create_framebuffer(shadow_fb_desc);
        if (!shadow_state_.shadow_framebuffer) {
            UH_ERROR("Failed to create shadow framebuffer");
            return;
        }

        // 4. Shadow UBO (light VP matrix) — descriptor set 0 for shadow pipeline
        graphics::Descriptor_Set_Layout_Desc shadow_ubo_layout_desc{};
        graphics::Descriptor_Binding shadow_ubo_binding{};
        shadow_ubo_binding.binding = 0;
        shadow_ubo_binding.type = graphics::Descriptor_Type::uniform_buffer;
        shadow_ubo_binding.count = 1;
        shadow_ubo_binding.shader_stages = VK_SHADER_STAGE_VERTEX_BIT;
        shadow_ubo_layout_desc.bindings.push_back(shadow_ubo_binding);
        shadow_state_.shadow_ubo_layout = device->create_descriptor_set_layout(shadow_ubo_layout_desc);
        shadow_state_.shadow_ubo_set = device->create_descriptor_set(shadow_state_.shadow_ubo_layout);

        graphics::Buffer_Desc shadow_buf_desc{};
        shadow_buf_desc.size = sizeof(Shadow_UBO);
        shadow_buf_desc.usage = graphics::Buffer_Type::uniform;
        shadow_buf_desc.memory = graphics::Memory_Type::cpu2gpu;
        shadow_state_.shadow_ubo_buffer = device->create_buffer(shadow_buf_desc);

        if (shadow_state_.shadow_ubo_set && shadow_state_.shadow_ubo_buffer) {
            graphics::Descriptor_Write ubo_write{};
            ubo_write.binding = 0;
            ubo_write.type = graphics::Descriptor_Type::uniform_buffer;
            ubo_write.buffers = { shadow_state_.shadow_ubo_buffer };
            ubo_write.buffer_offsets = { 0 };
            ubo_write.buffer_ranges = { sizeof(Shadow_UBO) };
            shadow_state_.shadow_ubo_set->update({ ubo_write });
        }

        // 5. Create shadow pipeline (depth-only, vertex shader only)
        auto shadow_vs_spv = graphics::utils::compile_shader_form_file(pbr_shader_path("shadow.vert"), shaderc_vertex_shader);
        if (shadow_vs_spv.empty()) {
            UH_ERROR("Failed to compile shadow vertex shader");
            return;
        }

        graphics::Shader_Desc shadow_vs_desc{};
        shadow_vs_desc.type = graphics::Shader_Type::vertex;
        shadow_vs_desc.bytecode = std::move(shadow_vs_spv);
        auto shadow_vs = device->create_shader(shadow_vs_desc);
        if (!shadow_vs) return;

        graphics::Graphics_Pipeline_Desc shadow_pipe_desc{};
        shadow_pipe_desc.vertex_shader = shadow_vs;
        shadow_pipe_desc.render_pass = shadow_state_.shadow_pass;
        shadow_pipe_desc.subpass = 0;
        shadow_pipe_desc.rasterizer_state.cull_enable = false;  // two-sided for Cornell box
        shadow_pipe_desc.rasterizer_state.depth_bias_enable = true;
        shadow_pipe_desc.rasterizer_state.depth_bias_constant = 4.0f;
        shadow_pipe_desc.rasterizer_state.depth_bias_slope = 4.0f;
        shadow_pipe_desc.depth_stencil_state.depth_test_enable = true;
        shadow_pipe_desc.depth_stencil_state.depth_write_enable = true;
        shadow_pipe_desc.blend_state.blend_enable = false;
        shadow_pipe_desc.descriptor_set_layouts = { shadow_state_.shadow_ubo_layout };

        graphics::Push_Constant_Range shadow_pc_range{};
        shadow_pc_range.offset = 0;
        shadow_pc_range.size = sizeof(Push_Constants);
        shadow_pc_range.shader_stages = VK_SHADER_STAGE_VERTEX_BIT;
        shadow_pipe_desc.push_constants.push_back(shadow_pc_range);

        // Same vertex layout as PBR
        graphics::Vertex_Attribute spos{};
        spos.semantic = "POSITION";
        spos.location = 0;
        spos.offset = offsetof(resource::Vertex, position);
        spos.stride = sizeof(resource::Vertex);
        shadow_pipe_desc.vertex_attributes.push_back(spos);

        graphics::Vertex_Attribute snorm{};
        snorm.semantic = "NORMAL";
        snorm.location = 1;
        snorm.offset = offsetof(resource::Vertex, normal);
        snorm.stride = sizeof(resource::Vertex);
        shadow_pipe_desc.vertex_attributes.push_back(snorm);

        graphics::Vertex_Attribute suv{};
        suv.semantic = "TEXCOORD0";
        suv.location = 2;
        suv.offset = offsetof(resource::Vertex, uv);
        suv.stride = sizeof(resource::Vertex);
        shadow_pipe_desc.vertex_attributes.push_back(suv);

        shadow_state_.shadow_pipeline = device->create_graphics_pipeline(shadow_pipe_desc);

        // 6. Create comparison sampler for shadow sampling
        graphics::Sampler_Desc shadow_sampler_desc{};
        shadow_sampler_desc.minFilter = graphics::Filter_Mode::linear;
        shadow_sampler_desc.magFilter = graphics::Filter_Mode::linear;
        shadow_sampler_desc.addressU = graphics::Edge_Mode::clamp;
        shadow_sampler_desc.addressV = graphics::Edge_Mode::clamp;
        shadow_sampler_desc.comparison_enable = true;
        shadow_sampler_desc.border_color = graphics::Border_Color::float_opaque_white;
        shadow_state_.shadow_sampler = device->create_sampler(shadow_sampler_desc);

        // 7. Create descriptor set for shadow map sampling (set 2 in PBR pipeline)
        if (shadow_state_.shadow_sample_layout) {
            shadow_state_.shadow_sample_set = device->create_descriptor_set(shadow_state_.shadow_sample_layout);
            if (shadow_state_.shadow_sample_set && shadow_state_.shadow_map && shadow_state_.shadow_sampler) {
                graphics::Descriptor_Write shadow_write{};
                shadow_write.binding = 0;
                shadow_write.type = graphics::Descriptor_Type::combined_image_sampler;
                shadow_write.textures = { shadow_state_.shadow_map };
                shadow_write.samplers = { shadow_state_.shadow_sampler };
                shadow_state_.shadow_sample_set->update({ shadow_write });
            }
        }

        shadow_state_.ready = shadow_state_.shadow_pipeline && shadow_state_.shadow_pass &&
                              shadow_state_.shadow_framebuffer && shadow_state_.shadow_sample_set;

        if (shadow_state_.ready) {
            UH_INFO("Shadow mapping resources created (2048x2048)");
        }
    }

    auto Application::create_default_camera_if_needed() -> void
    {
        auto world = core::World::current_instance();
        auto camera_store = world->get_twig_storage<resource::Camera>();
        if (camera_store && !camera_store->data.empty()) {
            return;
        }

        core::Entity entity = world->create_entity();

        resource::Transform cam_transform;
        cam_transform.position = {0.276f, 0.275f, -0.75f};
        world->attach_twig(entity, cam_transform);

        resource::Camera cam;
        cam.fov = 40.0f;
        world->attach_twig(entity, cam);

        auto scene_graph = get_renderer()->get_scene_graph();
        if (scene_graph) {
            scene_graph->add_entity_to_scene(entity, scene_graph->get_root_node(), "Camera");
        }
    }

    auto Application::create_default_scene() -> void
    {
        auto world = core::World::current_instance();

        // Only create default scene if there are no mesh/model entities yet
        auto mesh_store = world->get_twig_storage<resource::Mesh>();
        auto model_store = world->get_twig_storage<resource::Model>();
        if ((mesh_store && !mesh_store->data.empty()) || (model_store && !model_store->data.empty())) {
            return;
        }

        auto scene_graph = get_renderer()->get_scene_graph();
        auto root = scene_graph ? scene_graph->get_root_node() : nullptr;

        // Asset directory
        auto asset_dir = std::filesystem::path(__FILE__).parent_path().parent_path() / "asset";
        auto cornell_dir = asset_dir / "cornell_box";

        // Helper to load a Cornell box mesh part
        struct CornellPart {
            const char* obj_file;
            const char* name;
            math::Vec3 position;
            math::Vec4 color;
        };

        CornellPart parts[] = {
            {"cbox_ceiling.obj",   "Ceiling",    {0.278f, 0.5488f, 0.27955f}, {0.725f, 0.71f, 0.68f, 1.0f}},
            {"cbox_floor.obj",     "Floor",      {0.2756f, 0.0f, 0.2796f},    {0.725f, 0.71f, 0.68f, 1.0f}},
            {"cbox_back.obj",      "Back Wall",  {0.2764f, 0.2744f, 0.5592f}, {0.725f, 0.71f, 0.68f, 1.0f}},
            {"cbox_smallbox.obj",  "Small Box",  {0.1855f, 0.0835f, 0.169f},  {0.725f, 0.71f, 0.68f, 1.0f}},
            {"cbox_largebox.obj",  "Large Box",  {0.3685f, 0.166f, 0.35125f}, {0.725f, 0.71f, 0.68f, 1.0f}},
            {"cbox_greenwall.obj", "Green Wall", {0.0f, 0.2744f, 0.2796f},    {0.14f, 0.45f, 0.091f, 1.0f}},
            {"cbox_redwall.obj",   "Red Wall",   {0.5536f, 0.2744f, 0.2796f}, {0.63f, 0.065f, 0.05f, 1.0f}},
        };

        for (auto& part : parts) {
            auto obj_path = (cornell_dir / part.obj_file).string();
            auto model = resource::load_model_from_obj(obj_path);
            if (!model) {
                UH_ERROR_FMT("Failed to load Cornell box part: {}", part.obj_file);
                continue;
            }

            core::Entity entity = world->create_entity();

            resource::Transform t;
            t.position = part.position;
            t.scale = {0.01f, 0.01f, 0.01f};
            world->attach_twig(entity, t);
            world->attach_twig(entity, *model);

            resource::Pbr_Material mat;
            mat.base_color = part.color;
            mat.metallic = 0.0f;
            mat.roughness = 0.9f;
            mat.sync_params();
            world->attach_twig(entity, mat);

            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, root, part.name);
            }
        }

        // Ceiling light (approximating the quad area light from the scene file)
        {
            core::Entity entity = world->create_entity();
            resource::Transform t;
            t.position = {0.278f, 0.548f, 0.280f};
            world->attach_twig(entity, t);

            resource::Light light;
            light.type = resource::Light_Type::point;
            light.color = {1.0f, 0.85f, 0.7f};
            light.intensity = 3.0f;
            light.range = 2.0f;
            world->attach_twig(entity, light);

            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, root, "Ceiling Light");
            }
        }

        // GI bounce lights — simulate first-bounce color bleeding from walls
        // Red wall bounce (reddish fill near red wall, pointing inward)
        {
            core::Entity entity = world->create_entity();
            resource::Transform t;
            t.position = {0.45f, 0.274f, 0.280f}; // near red wall, mid height
            world->attach_twig(entity, t);

            resource::Light light;
            light.type = resource::Light_Type::point;
            light.color = {0.63f, 0.065f, 0.05f}; // red wall color
            light.intensity = 0.6f;
            light.range = 0.5f;
            world->attach_twig(entity, light);

            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, root, "Red Bounce");
            }
        }
        // Green wall bounce (greenish fill near green wall, pointing inward)
        {
            core::Entity entity = world->create_entity();
            resource::Transform t;
            t.position = {0.10f, 0.274f, 0.280f}; // near green wall, mid height
            world->attach_twig(entity, t);

            resource::Light light;
            light.type = resource::Light_Type::point;
            light.color = {0.14f, 0.45f, 0.091f}; // green wall color
            light.intensity = 0.6f;
            light.range = 0.5f;
            world->attach_twig(entity, light);

            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, root, "Green Bounce");
            }
        }
        // Floor bounce (warm upward fill simulating floor reflection)
        {
            core::Entity entity = world->create_entity();
            resource::Transform t;
            t.position = {0.278f, 0.05f, 0.280f}; // near floor center
            world->attach_twig(entity, t);

            resource::Light light;
            light.type = resource::Light_Type::point;
            light.color = {0.725f, 0.71f, 0.68f}; // floor color
            light.intensity = 0.3f;
            light.range = 0.4f;
            world->attach_twig(entity, light);

            if (scene_graph) {
                scene_graph->add_entity_to_scene(entity, root, "Floor Bounce");
            }
        }

        UH_INFO("Cornell box scene created");
    }

    auto Application::render_shadow_pass(graphics::Command_Buffer_Handle cmd) -> void
    {
        if (!cmd || !shadow_state_.ready || !pbr_state_.ready || !shadow_enabled_) return;

        auto world = core::World::current_instance();
        auto transform_store = world->get_twig_storage<resource::Transform>();
        auto light_store = world->get_twig_storage<resource::Light>();

        // Find the first point/spot light for shadow casting
        math::Vec3 light_pos{0.278f, 0.548f, 0.280f};
        if (light_store && transform_store) {
            for (auto& [entity, light] : light_store->data) {
                if (light.type == resource::Light_Type::point || light.type == resource::Light_Type::spot) {
                    auto t_it = transform_store->data.find(entity);
                    if (t_it != transform_store->data.end()) {
                        light_pos = t_it->second.position;
                        break;
                    }
                }
            }
        }

        // Compute light view-projection matrix (looking downward from ceiling light)
        auto light_view = glm::lookAt(
            glm::vec3(light_pos.x, light_pos.y, light_pos.z),
            glm::vec3(light_pos.x, 0.0f, light_pos.z),  // look at floor
            glm::vec3(0.0f, 0.0f, -1.0f)                 // up = -Z
        );
        auto light_proj = glm::perspective(glm::radians(120.0f), 1.0f, 0.01f, 2.0f);
        light_proj[1][1] *= -1.0f; // Vulkan Y-flip
        shadow_state_.light_view_proj = light_proj * light_view;

        // Upload shadow UBO
        Shadow_UBO shadow_ubo{};
        shadow_ubo.light_vp = shadow_state_.light_view_proj;
        auto vk_shadow_buf = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(shadow_state_.shadow_ubo_buffer);
        if (vk_shadow_buf) {
            vk_shadow_buf->upload(&shadow_ubo, sizeof(shadow_ubo));
        }

        // Begin shadow render pass
        cmd->begin_render_pass(shadow_state_.shadow_pass, shadow_state_.shadow_framebuffer, 2048, 2048);
        cmd->set_viewport(0.0f, 0.0f, 2048.0f, 2048.0f);
        cmd->set_scissor(0, 0, 2048, 2048);
        cmd->bind_pipeline(shadow_state_.shadow_pipeline);
        cmd->bind_descriptor_set(0, shadow_state_.shadow_ubo_set);

        // Helper lambda to draw a gpu mesh with a given model matrix
        auto draw_shadow_mesh = [&](const Gpu_Mesh& gpu_mesh, const math::Mat4& model_mat) {
            if (!gpu_mesh.vertex_buffer) return;
            Push_Constants pc{};
            pc.model = model_mat;
            cmd->push_constants(0, sizeof(Push_Constants), &pc);
            cmd->bind_vertex_buffer(0, gpu_mesh.vertex_buffer, 0);
            if (gpu_mesh.indexed && gpu_mesh.index_buffer) {
                cmd->bind_index_buffer(gpu_mesh.index_buffer, 0, 1);
                cmd->draw_indexed(gpu_mesh.index_count);
            } else {
                cmd->draw(gpu_mesh.index_count, 1, 0, 0);
            }
        };

        // Render Model-based entities (Cornell box parts use Model component)
        auto model_store = world->get_twig_storage<resource::Model>();
        if (model_store && transform_store) {
            for (auto& pair : model_store->data) {
                auto t_it = transform_store->data.find(pair.first);
                if (t_it == transform_store->data.end()) continue;

                auto& model = pair.second;
                for (auto& instance : model.get_instances()) {
                    auto mesh = instance.get_mesh();
                    if (!mesh) continue;

                    std::size_t key = reinterpret_cast<std::size_t>(mesh.get());
                    auto it = mesh_cache_.find(key);
                    if (it == mesh_cache_.end()) continue; // not yet uploaded

                    draw_shadow_mesh(it->second, t_it->second.get_matrix());
                }
            }
        }

        // Render Mesh-based entities
        auto mesh_store = world->get_twig_storage<resource::Mesh>();
        if (mesh_store && transform_store) {
            for (auto& pair : mesh_store->data) {
                auto t_it = transform_store->data.find(pair.first);
                if (t_it == transform_store->data.end()) continue;

                auto cache_it = entity_mesh_cache_.find(pair.first.id);
                if (cache_it == entity_mesh_cache_.end()) continue;

                draw_shadow_mesh(cache_it->second, t_it->second.get_matrix());
            }
        }

        cmd->end_render_pass();
    }

    auto Application::render_scene(graphics::Command_Buffer_Handle cmd) -> void
    {
        if (!cmd) {
            return;
        }

        if (!pbr_state_.ready) {
            return;
        }

        auto world = core::World::current_instance();
        auto camera_store = world->get_twig_storage<resource::Camera>();
        auto transform_store = world->get_twig_storage<resource::Transform>();
        auto material_store = world->get_twig_storage<resource::Pbr_Material>();

        resource::Camera* camera = nullptr;
        resource::Transform* camera_transform = nullptr;
        if (camera_store && !camera_store->data.empty()) {
            auto& pair = *camera_store->data.begin();
            camera = &pair.second;
            if (transform_store) {
                auto it = transform_store->data.find(pair.first);
                if (it != transform_store->data.end()) {
                    camera_transform = &it->second;
                }
            }
        }

        if (camera && camera_transform) {
            if (renderer_) {
                camera->aspect = static_cast<float>(renderer_->get_width()) / static_cast<float>(renderer_->get_height());
            }

            Camera_UBO ubo{};
            ubo.view = camera->get_view_matrix(*camera_transform);
            ubo.proj = camera->get_projection_matrix();
            ubo.view_proj = camera->get_view_projection_matrix(*camera_transform);
            ubo.camera_pos = mango::math::Vec4(camera_transform->position, 0.8f); // w = exposure

            auto vk_buffer = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(pbr_state_.camera_buffer);
            if (vk_buffer) {
                vk_buffer->upload(&ubo, sizeof(ubo));
            }
        }

        // Update lights UBO
        {
            Lights_UBO lights_ubo{};
            int light_count = 0;

            auto light_store = world->get_twig_storage<resource::Light>();
            if (light_store && transform_store) {
                for (auto& [entity, light] : light_store->data) {
                    if (light_count >= MAX_LIGHTS) break;
                    auto& ld = lights_ubo.lights[light_count];

                    if (light.type == resource::Light_Type::directional) {
                        auto dir = glm::normalize(light.direction);
                        ld.position_type = {dir.x, dir.y, dir.z, 0.0f};
                    } else {
                        auto t_it = transform_store->data.find(entity);
                        math::Vec3 pos = (t_it != transform_store->data.end()) ? t_it->second.position : math::Vec3(0);
                        ld.position_type = {pos.x, pos.y, pos.z, static_cast<float>(static_cast<int>(light.type))};
                    }

                    ld.color_intensity = {light.color.x, light.color.y, light.color.z, light.intensity};
                    ld.params = {light.direction.x, light.direction.y, light.direction.z, light.range};

                    float inner_cos = std::cos(glm::radians(light.inner_angle));
                    float outer_cos = std::cos(glm::radians(light.outer_angle));
                    ld.spot_params = {inner_cos, outer_cos, 0.0f, 0.0f};

                    light_count++;
                }
            }

            // Fallback: if no lights, add a default directional light
            if (light_count == 0) {
                auto& ld = lights_ubo.lights[0];
                auto dir = glm::normalize(math::Vec3(-0.4f, -1.0f, -0.2f));
                ld.position_type = {dir.x, dir.y, dir.z, 0.0f};
                ld.color_intensity = {1.0f, 1.0f, 1.0f, 3.5f};
                ld.params = {0.0f, 0.0f, 0.0f, 0.0f};
                ld.spot_params = {0.0f, 0.0f, 0.0f, 0.0f};
                light_count = 1;
            }

            lights_ubo.light_count = {static_cast<float>(light_count), 0.15f, static_cast<float>(debug_mode_), shadow_enabled_ ? 1.0f : 0.0f}; // y=ibl_intensity, z=debug_mode, w=shadow_enable
            lights_ubo.shadow_view_proj = shadow_state_.light_view_proj;

            auto vk_lb = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(pbr_state_.lights_buffer);
            if (vk_lb) {
                vk_lb->upload(&lights_ubo, sizeof(lights_ubo));
            }
        }

        // Draw skybox first (no depth test, scene objects render on top)
        if (skybox_enabled_ && pbr_state_.skybox_pipeline && ibl_resources_.ready && ibl_resources_.ibl_set) {
            cmd->bind_pipeline(pbr_state_.skybox_pipeline);
            cmd->bind_descriptor_set(0, pbr_state_.set);
            cmd->bind_descriptor_set(1, ibl_resources_.ibl_set);
            cmd->draw(3, 1, 0, 0); // Fullscreen triangle
        }

        // Draw PBR scene objects
        cmd->bind_pipeline(pbr_state_.pipeline);
        cmd->bind_descriptor_set(0, pbr_state_.set);
        if (ibl_resources_.ready && ibl_resources_.ibl_set) {
            cmd->bind_descriptor_set(1, ibl_resources_.ibl_set);
        }
        if (shadow_state_.ready && shadow_state_.shadow_sample_set) {
            cmd->bind_descriptor_set(2, shadow_state_.shadow_sample_set);
        }

        auto device = renderer_->get_device();
        if (!device) {
            return;
        }

        auto create_gpu_mesh = [&](const std::shared_ptr<resource::Mesh>& mesh) -> Gpu_Mesh {
            Gpu_Mesh gpu{};
            if (!mesh) {
                return gpu;
            }

            const auto& vertices = mesh->get_vertices();
            const auto& indices = mesh->get_indices();

            if (vertices.empty()) {
                return gpu;
            }

            graphics::Buffer_Desc vdesc{};
            vdesc.size = vertices.size() * sizeof(resource::Vertex);
            vdesc.usage = graphics::Buffer_Type::vertex;
            vdesc.memory = graphics::Memory_Type::cpu2gpu;
            gpu.vertex_buffer = device->create_buffer(vdesc);

            auto vk_vb = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(gpu.vertex_buffer);
            if (vk_vb) {
                vk_vb->upload(vertices.data(), vdesc.size);
            }

            if (!indices.empty()) {
                graphics::Buffer_Desc idesc{};
                idesc.size = indices.size() * sizeof(std::uint32_t);
                idesc.usage = graphics::Buffer_Type::index;
                idesc.memory = graphics::Memory_Type::cpu2gpu;
                gpu.index_buffer = device->create_buffer(idesc);
                auto vk_ib = std::dynamic_pointer_cast<graphics::vk::Vk_Buffer>(gpu.index_buffer);
                if (vk_ib) {
                    vk_ib->upload(indices.data(), idesc.size);
                }
                gpu.index_count = static_cast<uint32_t>(indices.size());
                gpu.indexed = true;
            } else {
                gpu.index_count = static_cast<uint32_t>(vertices.size());
                gpu.indexed = false;
            }

            return gpu;
        };

        if (transform_store) {
            auto model_store = world->get_twig_storage<resource::Model>();
            if (model_store) {
                for (auto& pair : model_store->data) {
                    auto transform_it = transform_store->data.find(pair.first);
                    if (transform_it == transform_store->data.end()) {
                        continue;
                    }

                    auto& model = pair.second;
                    for (auto& instance : model.get_instances()) {
                        auto mesh = instance.get_mesh();
                        if (!mesh) {
                            continue;
                        }

                        std::size_t key = reinterpret_cast<std::size_t>(mesh.get());
                        auto it = mesh_cache_.find(key);
                        if (it == mesh_cache_.end()) {
                            auto gpu = create_gpu_mesh(mesh);
                            it = mesh_cache_.emplace(key, std::move(gpu)).first;
                        }

                        auto& gpu = it->second;
                        if (!gpu.vertex_buffer) {
                            continue;
                        }

                        resource::Pbr_Material material{};
                        if (material_store) {
                            auto mat_it = material_store->data.find(pair.first);
                            if (mat_it != material_store->data.end()) {
                                material = mat_it->second;
                            }
                        }
                        Push_Constants pc{};
                        pc.model = transform_it->second.get_matrix();
                        pc.base_color = material.base_color;
                        pc.params = material.params;
                        cmd->push_constants(0, sizeof(Push_Constants), &pc);

                        cmd->bind_vertex_buffer(0, gpu.vertex_buffer, 0);
                        if (gpu.indexed && gpu.index_buffer) {
                            cmd->bind_index_buffer(gpu.index_buffer, 0, 1);
                            cmd->draw_indexed(gpu.index_count);
                        } else {
                            cmd->draw(gpu.index_count);
                        }
                    }
                }
            }

            auto mesh_store = world->get_twig_storage<resource::Mesh>();
            if (mesh_store) {
                for (auto& pair : mesh_store->data) {
                    auto transform_it = transform_store->data.find(pair.first);
                    if (transform_it == transform_store->data.end()) {
                        continue;
                    }

                    auto cache_it = entity_mesh_cache_.find(pair.first.id);
                    if (cache_it == entity_mesh_cache_.end()) {
                        auto mesh_ptr = std::make_shared<resource::Mesh>(pair.second);
                        auto gpu = create_gpu_mesh(mesh_ptr);
                        cache_it = entity_mesh_cache_.emplace(pair.first.id, std::move(gpu)).first;
                    }

                    auto& gpu = cache_it->second;
                    if (!gpu.vertex_buffer) {
                        continue;
                    }

                    resource::Pbr_Material material{};
                    if (material_store) {
                        auto mat_it = material_store->data.find(pair.first);
                        if (mat_it != material_store->data.end()) {
                            material = mat_it->second;
                        }
                    }
                    Push_Constants pc{};
                    pc.model = transform_it->second.get_matrix();
                    pc.base_color = material.base_color;
                    pc.params = material.params;
                    cmd->push_constants(0, sizeof(Push_Constants), &pc);

                    cmd->bind_vertex_buffer(0, gpu.vertex_buffer, 0);
                    if (gpu.indexed && gpu.index_buffer) {
                        cmd->bind_index_buffer(gpu.index_buffer, 0, 1);
                        cmd->draw_indexed(gpu.index_count);
                    } else {
                        cmd->draw(gpu.index_count);
                    }
                }
            }
        }
    }
    // ---- Physics integration ----

    void Application::set_physics_world(std::shared_ptr<physics::Physics_World> world)
    {
        physics_world_ = std::move(world);
    }

    void Application::physics_step(float delta_time)
    {
        if (!physics_world_)
            return;

        physics_world_->step(delta_time);
        sync_physics_transforms();
    }

    void Application::sync_physics_transforms()
    {
        if (!physics_world_)
            return;

        auto* world = core::World::current_instance();
        if (!world)
            return;

        auto* storage = world->get_twig_storage<resource::Physics_Body>();
        if (!storage)
            return;

        for (auto& [entity, body] : storage->data)
        {
            if (body.runtime_handle == physics::INVALID_BODY)
                continue;

            // Only sync dynamic/kinematic bodies
            if (body.type == physics::Body_Type::static_body)
                continue;

            if (!world->has_twig<resource::Transform>(entity))
                continue;

            auto& transform = world->get_twig<resource::Transform>(entity);
            transform.position = physics_world_->get_body_position(body.runtime_handle);
            transform.rotation = physics_world_->get_body_rotation(body.runtime_handle);
        }
    }
    void Application::update_orbit_camera(float delta_time)
    {
        auto* glfw_win = window_ ? window_->get_glfw_window() : nullptr;
        if (!glfw_win) return;

        // Skip camera input when ImGui wants the mouse
        if (imgui_initialized_) {
            auto& io = ImGui::GetIO();
            if (io.WantCaptureMouse) {
                orbit_active_ = false;
                pan_active_ = false;
                return;
            }
        }

        double mx, my;
        glfwGetCursorPos(glfw_win, &mx, &my);

        float dx = static_cast<float>(mx - last_mouse_x_);
        float dy = static_cast<float>(my - last_mouse_y_);
        last_mouse_x_ = mx;
        last_mouse_y_ = my;

        // Right mouse button: orbit
        bool rmb = glfwGetMouseButton(glfw_win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (rmb) {
            if (!orbit_active_) {
                orbit_active_ = true;
                dx = dy = 0.0f; // skip first-frame jump
            }
            orbit_yaw_ -= dx * 0.3f;
            orbit_pitch_ += dy * 0.3f;
            orbit_pitch_ = glm::clamp(orbit_pitch_, -89.0f, 89.0f);
        } else {
            orbit_active_ = false;
        }

        // Middle mouse button: pan
        bool mmb = glfwGetMouseButton(glfw_win, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        if (mmb) {
            if (!pan_active_) {
                pan_active_ = true;
                dx = dy = 0.0f;
            }
            float pan_speed = orbit_distance_ * 0.002f;
            float yaw_rad = glm::radians(orbit_yaw_);
            math::Vec3 right = math::Vec3(std::cos(yaw_rad), 0.0f, -std::sin(yaw_rad));
            math::Vec3 up = math::Vec3(0.0f, 1.0f, 0.0f);
            orbit_target_ -= right * dx * pan_speed;
            orbit_target_ += up * dy * pan_speed;
        } else {
            pan_active_ = false;
        }

        // Scroll wheel zoom
        if (scroll_offset_y_ != 0.0f) {
            orbit_distance_ -= scroll_offset_y_ * orbit_distance_ * 0.1f;
            scroll_offset_y_ = 0.0f;
        }
        // Key fallback zoom
        if (glfwGetKey(glfw_win, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(glfw_win, GLFW_KEY_UP) == GLFW_PRESS) {
            orbit_distance_ -= orbit_distance_ * 2.0f * delta_time;
        }
        if (glfwGetKey(glfw_win, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(glfw_win, GLFW_KEY_DOWN) == GLFW_PRESS) {
            orbit_distance_ += orbit_distance_ * 2.0f * delta_time;
        }
        orbit_distance_ = glm::clamp(orbit_distance_, 0.01f, 200.0f);

        // WASD: move target (forward = camera's visual forward direction)
        float move_speed = 3.0f * delta_time;
        float yaw_rad = glm::radians(orbit_yaw_);
        math::Vec3 forward = math::Vec3(std::sin(yaw_rad), 0.0f, std::cos(yaw_rad));
        math::Vec3 right = math::Vec3(-std::cos(yaw_rad), 0.0f, std::sin(yaw_rad));
        if (glfwGetKey(glfw_win, GLFW_KEY_W) == GLFW_PRESS) orbit_target_ += forward * move_speed;
        if (glfwGetKey(glfw_win, GLFW_KEY_S) == GLFW_PRESS) orbit_target_ -= forward * move_speed;
        if (glfwGetKey(glfw_win, GLFW_KEY_A) == GLFW_PRESS) orbit_target_ -= right * move_speed;
        if (glfwGetKey(glfw_win, GLFW_KEY_D) == GLFW_PRESS) orbit_target_ += right * move_speed;
        if (glfwGetKey(glfw_win, GLFW_KEY_Q) == GLFW_PRESS) orbit_target_.y -= move_speed;
        if (glfwGetKey(glfw_win, GLFW_KEY_E) == GLFW_PRESS) orbit_target_.y += move_speed;

        // Compute camera position from orbit params
        float pitch_rad = glm::radians(orbit_pitch_);
        math::Vec3 offset{
            orbit_distance_ * std::cos(pitch_rad) * (-std::sin(yaw_rad)),
            orbit_distance_ * std::sin(pitch_rad),
            orbit_distance_ * std::cos(pitch_rad) * (-std::cos(yaw_rad))
        };
        math::Vec3 cam_pos = orbit_target_ + offset;

        // Update camera entity transform
        auto* world = core::World::current_instance();
        if (!world) return;

        auto* camera_storage = world->get_twig_storage<resource::Camera>();
        if (!camera_storage) return;

        for (auto& [entity, cam] : camera_storage->data) {
            if (!world->has_twig<resource::Transform>(entity)) continue;
            auto& transform = world->get_twig<resource::Transform>(entity);
            transform.position = cam_pos;

            // Compute look-at rotation
            math::Vec3 dir = glm::normalize(orbit_target_ - cam_pos);
            // lookAt-style quaternion
            math::Mat4 view = glm::lookAt(cam_pos, orbit_target_, math::Vec3(0, 1, 0));
            transform.rotation = glm::conjugate(glm::quat_cast(view));
            break; // only update first camera
        }
    }
} // namespace mango::app
