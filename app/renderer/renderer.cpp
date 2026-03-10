// app/renderer/renderer.cpp
#include "renderer.hpp"
#include "backends/vulkan/vk-device.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-texture.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-buffer.hpp"
#include "utils/shader-compiler.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <algorithm>
#include <filesystem>

namespace mango::app
{
    static auto blit_shader_path(const char* filename) -> std::string
    {
        auto base = std::filesystem::path(__FILE__).parent_path().parent_path();
        return (base / "shaders" / "post" / filename).string();
    }

    Renderer::Renderer(const Renderer_Desc& desc)
        : desc_(desc)
        , width_(desc.width)
        , height_(desc.height)
        , native_window_(desc.native_window)
    {
        if (!native_window_) {
            throw std::runtime_error("Native window handle is null");
        }

        UH_INFO("Initializing Renderer...");

        try {
            create_device();
            create_swapchain();
            create_depth_resources();
            create_offscreen_resources();
            create_scene_render_pass();
            create_scene_framebuffer();
            create_blit_render_pass();
            create_blit_framebuffers();
            create_blit_pipeline();
            create_command_resources();
            create_sync_objects();

            UH_INFO_FMT("Renderer initialized successfully ({}x{})", width_, height_);
        }
        catch (const std::exception& e) {
            UH_FATAL_FMT("Failed to initialize renderer: {}", e.what());
            cleanup();
            throw;
        }
    }

    Renderer::~Renderer()
    {
        cleanup();
    }

    void Renderer::create_device()
    {
        UH_INFO_FMT("Creating graphics device (backend: {})",
            static_cast<int>(desc_.backend));

        switch (desc_.backend) {
            case Graphics_Backend::Vulkan:
                device_ = create_vulkan_device();
                break;
            default:
                throw std::runtime_error("Unsupported graphics backend");
        }

        if (!device_) {
            throw std::runtime_error("Failed to create graphics device");
        }

        UH_INFO("Graphics device created successfully");
    }

    auto Renderer::create_vulkan_device() -> graphics::Device_Handle
    {
        graphics::Device_Desc device_desc{};
        device_desc.enable_validation = desc_.enable_validation;
        device_desc.enable_raytracing = false;
        device_desc.preferred_adapter_index = 0;

        return std::make_shared<graphics::vk::Vk_Device>(device_desc);
    }

    void Renderer::create_swapchain()
    {
        UH_INFO("Creating swapchain...");

        graphics::Swapchain_Desc swapchain_desc{};
        swapchain_desc.width = width_;
        swapchain_desc.height = height_;
        swapchain_desc.image_count = desc_.enable_vsync ? 3 : 2;
        swapchain_desc.native_window = native_window_;
        swapchain_desc.format = 0; // Let implementation choose

        swapchain_ = device_->create_swapchain(swapchain_desc);

        if (!swapchain_) {
            throw std::runtime_error("Failed to create swapchain");
        }

        const auto& actual_desc = swapchain_->get_desc();
        width_ = actual_desc.width;
        height_ = actual_desc.height;

        UH_INFO_FMT("Swapchain created: {}x{}, {} images",
            width_, height_, actual_desc.image_count);
    }

    auto Renderer::choose_depth_format() -> graphics::Texture_Format
    {
        std::vector<graphics::Texture_Format> candidates;

        #ifdef __APPLE__
        candidates = {
            graphics::Texture_Format::depth32f_stencil8,
            graphics::Texture_Format::depth32f
        };
        #else
        candidates = {
            graphics::Texture_Format::depth24_stencil8,
            graphics::Texture_Format::depth32f_stencil8,
            graphics::Texture_Format::depth32f,
            graphics::Texture_Format::depth24
        };
        #endif

        UH_INFO_FMT("Selected depth format: {}", static_cast<int>(candidates[0]));
        return candidates[0];
    }

    void Renderer::create_depth_resources()
    {
        UH_INFO("Creating depth resources...");

        graphics::Texture_Desc depth_desc{};
        depth_desc.dimension = graphics::Texture_Kind::tex_2d;
        depth_desc.format = choose_depth_format();
        depth_desc.width = width_;
        depth_desc.height = height_;
        depth_desc.depth = 1;
        depth_desc.mip_levels = 1;
        depth_desc.arrayLayers = 1;
        depth_desc.sampled = true;   // Enable sampling for SSAO/SSR post-processing
        depth_desc.render_target = true;

        depth_image_ = device_->create_texture(depth_desc);

        if (!depth_image_) {
            throw std::runtime_error("Failed to create depth image");
        }

        UH_INFO("Depth resources created (sampled=true for post-processing)");
    }

    void Renderer::create_offscreen_resources()
    {
        UH_INFO("Creating offscreen HDR resources...");

        // HDR color buffer (rgba16f)
        graphics::Texture_Desc hdr_desc{};
        hdr_desc.dimension = graphics::Texture_Kind::tex_2d;
        hdr_desc.format = graphics::Texture_Format::rgba16f;
        hdr_desc.width = width_;
        hdr_desc.height = height_;
        hdr_desc.depth = 1;
        hdr_desc.mip_levels = 1;
        hdr_desc.arrayLayers = 1;
        hdr_desc.sampled = true;
        hdr_desc.render_target = true;
        hdr_desc.storage = true; // For compute shader read/write in post-processing

        hdr_color_ = device_->create_texture(hdr_desc);
        if (!hdr_color_) {
            throw std::runtime_error("Failed to create HDR color buffer");
        }

        // G-buffer normal (rgba16f): normal.xyz + roughness
        graphics::Texture_Desc normal_desc = hdr_desc;
        gbuffer_normal_ = device_->create_texture(normal_desc);
        if (!gbuffer_normal_) {
            throw std::runtime_error("Failed to create G-buffer normal texture");
        }

        UH_INFO_FMT("Offscreen HDR resources created ({}x{}, rgba16f)", width_, height_);
    }

    void Renderer::create_scene_render_pass()
    {
        UH_INFO("Creating scene render pass...");

        graphics::Render_Pass_Desc rp_desc{};

        // Attachment 0: HDR color (rgba16f)
        graphics::Attachment_Desc hdr_attachment{};
        hdr_attachment.texture = hdr_color_;
        hdr_attachment.load_op = 1;  // Clear
        hdr_attachment.store_op = 0; // Store
        hdr_attachment.initial_state = 0; // Undefined
        hdr_attachment.final_state = 4;   // Shader resource (ready for blit sampling)
        rp_desc.attachments.push_back(hdr_attachment);

        // Attachment 1: Depth
        graphics::Attachment_Desc depth_attachment{};
        depth_attachment.texture = depth_image_;
        depth_attachment.load_op = 1;  // Clear
        depth_attachment.store_op = 0; // Store (needed for post-processing)
        depth_attachment.initial_state = 0; // Undefined
        depth_attachment.final_state = 4;   // Shader resource (for SSAO/SSR)
        rp_desc.attachments.push_back(depth_attachment);

        // Attachment 2: G-buffer normal (rgba16f)
        graphics::Attachment_Desc normal_attachment{};
        normal_attachment.texture = gbuffer_normal_;
        normal_attachment.load_op = 1;  // Clear
        normal_attachment.store_op = 0; // Store
        normal_attachment.initial_state = 0; // Undefined
        normal_attachment.final_state = 4;   // Shader resource
        rp_desc.attachments.push_back(normal_attachment);

        // Subpass 0: color=[0, 2], depth=1
        graphics::Subpass_Desc subpass{};
        subpass.color_attachments.push_back(0); // hdr_color at location 0
        subpass.color_attachments.push_back(2); // gbuffer_normal at location 1
        subpass.depth_stencil_attachment = 1;
        rp_desc.subpasses.push_back(subpass);

        scene_render_pass_ = device_->create_render_pass(rp_desc);
        if (!scene_render_pass_) {
            throw std::runtime_error("Failed to create scene render pass");
        }

        UH_INFO("Scene render pass created (HDR color + depth + G-buffer normal)");
    }

    void Renderer::create_scene_framebuffer()
    {
        UH_INFO("Creating scene framebuffer...");

        graphics::Framebuffer_Desc fb_desc{};
        fb_desc.render_pass = scene_render_pass_;
        fb_desc.attachments.push_back(hdr_color_);
        fb_desc.attachments.push_back(depth_image_);
        fb_desc.attachments.push_back(gbuffer_normal_);
        fb_desc.width = width_;
        fb_desc.height = height_;
        fb_desc.layers = 1;

        scene_framebuffer_ = device_->create_framebuffer(fb_desc);
        if (!scene_framebuffer_) {
            throw std::runtime_error("Failed to create scene framebuffer");
        }

        UH_INFO("Scene framebuffer created");
    }

    void Renderer::create_blit_render_pass()
    {
        UH_INFO("Creating blit render pass...");

        graphics::Render_Pass_Desc rp_desc{};

        // Attachment 0: Swapchain color (sRGB)
        graphics::Attachment_Desc color_attachment{};
        color_attachment.texture = swapchain_->get_images()[0];
        color_attachment.load_op = 1;  // Clear
        color_attachment.store_op = 0; // Store
        color_attachment.initial_state = 0; // Undefined
        color_attachment.final_state = 8;  // Present
        rp_desc.attachments.push_back(color_attachment);

        // Subpass 0: color only, no depth
        graphics::Subpass_Desc subpass{};
        subpass.color_attachments.push_back(0);
        rp_desc.subpasses.push_back(subpass);

        blit_render_pass_ = device_->create_render_pass(rp_desc);
        if (!blit_render_pass_) {
            throw std::runtime_error("Failed to create blit render pass");
        }

        UH_INFO("Blit render pass created");
    }

    void Renderer::create_blit_framebuffers()
    {
        UH_INFO("Creating blit framebuffers...");

        const auto& swapchain_images = swapchain_->get_images();
        blit_framebuffers_.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_images.size(); i++) {
            graphics::Framebuffer_Desc fb_desc{};
            fb_desc.render_pass = blit_render_pass_;
            fb_desc.attachments.push_back(swapchain_images[i]);
            fb_desc.width = width_;
            fb_desc.height = height_;
            fb_desc.layers = 1;

            blit_framebuffers_[i] = device_->create_framebuffer(fb_desc);

            if (!blit_framebuffers_[i]) {
                throw std::runtime_error("Failed to create blit framebuffer");
            }
        }

        UH_INFO_FMT("Created {} blit framebuffers", blit_framebuffers_.size());
    }

    void Renderer::create_blit_pipeline()
    {
        UH_INFO("Creating blit pipeline...");

        // Compile blit shaders
        auto vs_spv = graphics::utils::compile_shader_form_file(
            blit_shader_path("fullscreen_blit.vert"), shaderc_vertex_shader);
        auto fs_spv = graphics::utils::compile_shader_form_file(
            blit_shader_path("fullscreen_blit.frag"), shaderc_fragment_shader);

        if (vs_spv.empty() || fs_spv.empty()) {
            UH_ERROR("Failed to compile blit shaders");
            return;
        }

        graphics::Shader_Desc vs_desc{};
        vs_desc.type = graphics::Shader_Type::vertex;
        vs_desc.bytecode = std::move(vs_spv);

        graphics::Shader_Desc fs_desc{};
        fs_desc.type = graphics::Shader_Type::fragment;
        fs_desc.bytecode = std::move(fs_spv);

        auto vs = device_->create_shader(vs_desc);
        auto fs = device_->create_shader(fs_desc);
        if (!vs || !fs) {
            UH_ERROR("Failed to create blit shader objects");
            return;
        }

        // Descriptor set layout: binding 0 = combined_image_sampler (HDR texture)
        graphics::Descriptor_Set_Layout_Desc set_layout_desc{};
        graphics::Descriptor_Binding hdr_binding{};
        hdr_binding.binding = 0;
        hdr_binding.type = graphics::Descriptor_Type::combined_image_sampler;
        hdr_binding.count = 1;
        hdr_binding.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
        set_layout_desc.bindings.push_back(hdr_binding);

        blit_set_layout_ = device_->create_descriptor_set_layout(set_layout_desc);
        blit_set_ = device_->create_descriptor_set(blit_set_layout_);

        // Create sampler for HDR texture
        graphics::Sampler_Desc sampler_desc{};
        sampler_desc.minFilter = graphics::Filter_Mode::linear;
        sampler_desc.magFilter = graphics::Filter_Mode::linear;
        sampler_desc.addressU = graphics::Edge_Mode::clamp;
        sampler_desc.addressV = graphics::Edge_Mode::clamp;
        blit_sampler_ = device_->create_sampler(sampler_desc);

        // Update descriptor set with HDR color texture
        if (blit_set_ && hdr_color_ && blit_sampler_) {
            graphics::Descriptor_Write hdr_write{};
            hdr_write.binding = 0;
            hdr_write.type = graphics::Descriptor_Type::combined_image_sampler;
            hdr_write.textures = { hdr_color_ };
            hdr_write.samplers = { blit_sampler_ };
            blit_set_->update({ hdr_write });
        }

        // Create pipeline
        graphics::Graphics_Pipeline_Desc pipe_desc{};
        pipe_desc.vertex_shader = vs;
        pipe_desc.fragment_shader = fs;
        pipe_desc.render_pass = blit_render_pass_;
        pipe_desc.subpass = 0;
        pipe_desc.rasterizer_state.cull_enable = false;
        pipe_desc.depth_stencil_state.depth_test_enable = false;
        pipe_desc.depth_stencil_state.depth_write_enable = false;
        pipe_desc.blend_state.blend_enable = false;
        pipe_desc.descriptor_set_layouts = { blit_set_layout_ };
        // No vertex attributes (fullscreen triangle generated in shader)

        graphics::Push_Constant_Range pc_range{};
        pc_range.offset = 0;
        pc_range.size = sizeof(Blit_Push_Constants);
        pc_range.shader_stages = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipe_desc.push_constants.push_back(pc_range);

        blit_pipeline_ = device_->create_graphics_pipeline(pipe_desc);

        if (blit_pipeline_) {
            UH_INFO("Blit pipeline created successfully");
        } else {
            UH_ERROR("Failed to create blit pipeline");
        }
    }

    void Renderer::create_command_resources()
    {
        UH_INFO("Creating command resources...");

        // Create command pool
        command_pool_ = device_->create_command_pool();
        if (!command_pool_) {
            throw std::runtime_error("Failed to create command pool");
        }

        // Create graphics queue
        graphics_queue_ = device_->create_command_queue(graphics::Queue_Type::graphics);
        if (!graphics_queue_) {
            throw std::runtime_error("Failed to create graphics queue");
        }

        // Allocate command buffers
        command_buffers_.resize(desc_.max_frames_in_flight);
        for (uint32_t i = 0; i < desc_.max_frames_in_flight; i++) {
            command_buffers_[i] = command_pool_->allocate_command_buffer(
                graphics::Command_Buffer_Level::primary
            );

            if (!command_buffers_[i]) {
                throw std::runtime_error("Failed to allocate command buffer");
            }
        }

        UH_INFO_FMT("Created command pool and {} command buffers",
            command_buffers_.size());
    }

    void Renderer::create_sync_objects()
    {
        UH_INFO("Creating synchronization objects...");

        in_flight_fences_.resize(desc_.max_frames_in_flight);
        image_available_semaphores_.resize(desc_.max_frames_in_flight);
        render_finished_semaphores_.resize(desc_.max_frames_in_flight);
        fence_values_.resize(desc_.max_frames_in_flight, 0);

        for (uint32_t i = 0; i < desc_.max_frames_in_flight; i++) {
            in_flight_fences_[i] = device_->create_fence(false);

            if (!in_flight_fences_[i]) {
                throw std::runtime_error("Failed to create fence");
            }

            image_available_semaphores_[i] = device_->create_semaphore(false, 0);
            render_finished_semaphores_[i] = device_->create_semaphore(false, 0);

            if (!image_available_semaphores_[i] || !render_finished_semaphores_[i]) {
                throw std::runtime_error("Failed to create semaphores");
            }
        }

        UH_INFO_FMT("Created {} sync object sets", desc_.max_frames_in_flight);
    }

    void Renderer::begin_frame()
    {
        if (frame_started_) {
            UH_WARN("begin_frame called but frame already started");
            return;
        }

        if (swapchain_needs_recreation_) {
            recreate_swapchain();
            swapchain_needs_recreation_ = false;
            return;
        }

        // Wait for this frame's fence if it was used before
        auto& fence = in_flight_fences_[current_frame_];
        uint64_t wait_value = fence_values_[current_frame_];

        if (wait_value > 0) {
            fence->wait(wait_value, UINT64_MAX);
        }

        // Acquire next swapchain image
        auto& image_available = image_available_semaphores_[current_frame_];
        int32_t image_index = swapchain_->acquire_next_image(image_available);

        if (image_index < 0) {
            UH_WARN("Swapchain out of date, scheduling recreation");
            swapchain_needs_recreation_ = true;
            return;
        }

        current_image_index_ = static_cast<uint32_t>(image_index);

        auto& cmd = command_buffers_[current_frame_];
        cmd->reset();
        cmd->begin();

        frame_started_ = true;
    }

    void Renderer::end_frame()
    {
        if (!frame_started_) {
            UH_WARN("end_frame called but frame not started");
            return;
        }

        auto& cmd = command_buffers_[current_frame_];
        cmd->end();

        graphics::Submit_Info submit_info{};
        submit_info.command_buffers.push_back(cmd);
        submit_info.wait_semaphores.push_back(image_available_semaphores_[current_frame_]);
        submit_info.wait_stage_masks.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        submit_info.signal_semaphores.push_back(render_finished_semaphores_[current_frame_]);

        // Increment and signal fence
        fence_values_[current_frame_]++;
        auto& fence = in_flight_fences_[current_frame_];

        graphics_queue_->submit(submit_info, fence);

        graphics_queue_->present(
            swapchain_,
            current_image_index_,
            {render_finished_semaphores_[current_frame_]}
        );

        current_frame_ = (current_frame_ + 1) % desc_.max_frames_in_flight;
        frame_started_ = false;
    }

    void Renderer::render_frame()
    {
        begin_frame();

        if (!frame_started_) {
            return;
        }

        auto& cmd = command_buffers_[current_frame_];

        Frame_Context context{};
        context.frame_index = current_frame_;
        context.width = width_;
        context.height = height_;
        context.mode = Run_Mode::runtime;
        context.outputs = render_targets_.outputs;

        const auto graph = frame_pipeline_.build_graph(context, device_->get_capabilities());
        const auto order = graph.compile();

        bool blit_render_pass_open = false;

        const auto run_pre_render = [&]() {
            if (pre_render_callback_) {
                pre_render_callback_(cmd);
            }
        };

        const auto run_scene_render = [&]() {
            cmd->begin_render_pass(
                scene_render_pass_,
                scene_framebuffer_,
                width_,
                height_
            );

            cmd->set_viewport(0.0f, 0.0f,
                static_cast<float>(width_),
                static_cast<float>(height_));
            cmd->set_scissor(0, 0, width_, height_);

            if (render_callback_) {
                render_callback_(cmd);
            }

            cmd->end_render_pass();
        };

        const auto run_post_process = [&]() {
            blit_passthrough_ = false;
            if (post_process_callback_) {
                post_process_callback_(cmd);
            }
        };

        const auto ensure_blit_render_pass = [&]() {
            if (blit_render_pass_open) {
                return;
            }

            cmd->begin_render_pass(
                blit_render_pass_,
                blit_framebuffers_[current_image_index_],
                width_,
                height_
            );

            cmd->set_viewport(0.0f, 0.0f,
                static_cast<float>(width_),
                static_cast<float>(height_));
            cmd->set_scissor(0, 0, width_, height_);
            blit_render_pass_open = true;
        };

        const auto run_final_blit = [&]() {
            ensure_blit_render_pass();

            if (blit_pipeline_ && blit_set_) {
                cmd->bind_pipeline(blit_pipeline_);
                cmd->bind_descriptor_set(0, blit_set_);

                Blit_Push_Constants pc = blit_pc_;
                if (blit_passthrough_) {
                    pc.tone_map_mode = 2;
                }
                cmd->push_constants(0, sizeof(Blit_Push_Constants), &pc);
                cmd->draw(3, 1, 0, 0);
            }
        };

        const auto run_imgui = [&]() {
            ensure_blit_render_pass();
            if (imgui_render_callback_) {
                imgui_render_callback_(cmd);
            }
        };

        for (const auto& pass_name : order) {
            if (pass_name == "pre_render") {
                run_pre_render();
            }
            else if (pass_name == "scene_render") {
                run_scene_render();
            }
            else if (pass_name == "post_process") {
                run_post_process();
            }
            else if (pass_name == "final_blit") {
                run_final_blit();
            }
            else if (pass_name == "imgui") {
                run_imgui();
            }
        }

        if (blit_render_pass_open) {
            cmd->end_render_pass();
        }

        end_frame();
    }
    auto Renderer::get_current_command_buffer() -> graphics::Command_Buffer_Handle
    {
        if (!frame_started_) {
            UH_WARN("Requesting command buffer but frame not started");
            return nullptr;
        }
        return command_buffers_[current_frame_];
    }

    auto Renderer::get_swapchain_image_count() const -> uint32_t
    {
        if (!swapchain_) {
            return 0;
        }
        return swapchain_->get_desc().image_count;
    }

    void Renderer::set_render_callback(RenderCallback callback)
    {
        render_callback_ = std::move(callback);
    }

    void Renderer::set_pre_render_callback(RenderCallback callback)
    {
        pre_render_callback_ = std::move(callback);
    }

    void Renderer::set_post_process_callback(RenderCallback callback)
    {
        post_process_callback_ = std::move(callback);
    }

    void Renderer::set_imgui_render_callback(RenderCallback callback)
    {
        imgui_render_callback_ = std::move(callback);
    }

    void Renderer::set_blit_source(graphics::Texture_Handle source)
    {
        if (!blit_set_ || !source || !blit_sampler_) return;

        graphics::Descriptor_Write hdr_write{};
        hdr_write.binding = 0;
        hdr_write.type = graphics::Descriptor_Type::combined_image_sampler;
        hdr_write.textures = { source };
        hdr_write.samplers = { blit_sampler_ };
        blit_set_->update({ hdr_write });
    }

    void Renderer::wait_idle()
    {
        if (device_) {
            device_->wait_idle();
        }
    }

    void Renderer::handle_resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) {
            UH_WARN("Invalid resize dimensions, ignoring");
            return;
        }

        if (width == width_ && height == height_) {
            return;
        }

        UH_INFO_FMT("Handling window resize: {}x{} -> {}x{}",
            width_, height_, width, height);

        width_ = width;
        height_ = height;
        swapchain_needs_recreation_ = true;
    }

    void Renderer::wait_for_window_size()
    {
        while (width_ == 0 || height_ == 0) {
            UH_WARN("Window has zero size, waiting...");
            break;
        }
    }

    void Renderer::recreate_swapchain()
    {
        UH_INFO("Recreating swapchain...");

        wait_for_window_size();
        wait_idle();

        cleanup_swapchain();

        try {
            create_swapchain();
            create_depth_resources();
            create_offscreen_resources();
            create_scene_render_pass();
            create_scene_framebuffer();
            create_blit_render_pass();
            create_blit_framebuffers();
            create_blit_pipeline();

            UH_INFO("Swapchain and offscreen resources recreated successfully");
        }
        catch (const std::exception& e) {
            UH_ERROR_FMT("Failed to recreate swapchain: {}", e.what());
            throw;
        }
    }

    void Renderer::cleanup_swapchain()
    {
        // Clean up in reverse order
        blit_pipeline_.reset();
        blit_set_.reset();
        blit_set_layout_.reset();
        blit_sampler_.reset();
        blit_framebuffers_.clear();
        blit_render_pass_.reset();

        scene_framebuffer_.reset();
        scene_render_pass_.reset();

        gbuffer_normal_.reset();
        hdr_color_.reset();
        depth_image_.reset();
        swapchain_.reset();

        UH_INFO("Swapchain and offscreen resources cleaned up");
    }

    void Renderer::cleanup()
    {
        if (device_) {
            wait_idle();
        }

        // Clean up in reverse order of creation
        in_flight_fences_.clear();
        image_available_semaphores_.clear();
        render_finished_semaphores_.clear();

        command_buffers_.clear();
        command_pool_.reset();
        graphics_queue_.reset();

        blit_pipeline_.reset();
        blit_set_.reset();
        blit_set_layout_.reset();
        blit_sampler_.reset();
        blit_framebuffers_.clear();
        blit_render_pass_.reset();

        scene_framebuffer_.reset();
        scene_render_pass_.reset();

        gbuffer_normal_.reset();
        hdr_color_.reset();
        depth_image_.reset();
        swapchain_.reset();

        device_.reset();

        UH_INFO("Renderer cleaned up");
    }

} // namespace mango::app
