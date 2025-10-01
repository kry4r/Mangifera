// app/renderer/renderer.cpp
#include "renderer.hpp"
#include "backends/vulkan/vk-device.hpp"
#include "backends/vulkan/vulkan-render-resource/vk-texture.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <algorithm>

namespace mango::app
{
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
            create_render_pass();
            create_framebuffers();
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
        // List of candidate formats in preference order
        std::vector<graphics::Texture_Format> candidates;

        #ifdef __APPLE__
        // MoltenVK supported depth formats (in preference order)
        candidates = {
            graphics::Texture_Format::depth32f_stencil8,
            graphics::Texture_Format::depth32f
        };
        #else
        // Standard Vulkan depth formats
        candidates = {
            graphics::Texture_Format::depth24_stencil8,
            graphics::Texture_Format::depth32f_stencil8,
            graphics::Texture_Format::depth32f,
            graphics::Texture_Format::depth24
        };
        #endif

        // For now, return the first candidate
        // TODO: Query device support with vkGetPhysicalDeviceFormatProperties
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
        depth_desc.sampled = false;
        depth_desc.render_target = true;

        depth_image_ = device_->create_texture(depth_desc);

        if (!depth_image_) {
            throw std::runtime_error("Failed to create depth image");
        }

        UH_INFO("Depth resources created");
    }

    void Renderer::create_render_pass()
    {
        UH_INFO("Creating render pass...");

        graphics::Render_Pass_Desc rp_desc{};

        // Attachment 0: Color (swapchain image)
        graphics::Attachment_Desc color_attachment{};
        color_attachment.texture = swapchain_->get_images()[0];
        color_attachment.load_op = 1;  // Clear
        color_attachment.store_op = 0; // Store
        color_attachment.initial_state = 0; // Undefined
        color_attachment.final_state = 8;  // Present

        rp_desc.attachments.push_back(color_attachment);

        // Attachment 1: Depth
        graphics::Attachment_Desc depth_attachment{};
        depth_attachment.texture = depth_image_;
        depth_attachment.load_op = 1;  // Clear
        depth_attachment.store_op = 1; // Don't care (we don't need to preserve depth)
        depth_attachment.initial_state = 0; // Undefined
        depth_attachment.final_state = 3;  // Depth stencil attachment

        rp_desc.attachments.push_back(depth_attachment);

        // Subpass 0
        graphics::Subpass_Desc subpass{};
        subpass.color_attachments.push_back(0);
        subpass.depth_stencil_attachment = 1;

        rp_desc.subpasses.push_back(subpass);

        render_pass_ = device_->create_render_pass(rp_desc);

        if (!render_pass_) {
            throw std::runtime_error("Failed to create render pass");
        }

        UH_INFO("Render pass created");
    }

    void Renderer::create_framebuffers()
    {
        UH_INFO("Creating framebuffers...");

        const auto& swapchain_images = swapchain_->get_images();
        framebuffers_.resize(swapchain_images.size());

        for (size_t i = 0; i < swapchain_images.size(); i++) {
            graphics::Framebuffer_Desc fb_desc{};
            fb_desc.render_pass = render_pass_;
            fb_desc.attachments.push_back(swapchain_images[i]);
            fb_desc.attachments.push_back(depth_image_);
            fb_desc.width = width_;
            fb_desc.height = height_;
            fb_desc.layers = 1;

            framebuffers_[i] = device_->create_framebuffer(fb_desc);

            if (!framebuffers_[i]) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }

        UH_INFO_FMT("Created {} framebuffers", framebuffers_.size());
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
            // Create fence starting at value 0 (unsignaled)
            in_flight_fences_[i] = device_->create_fence(false);

            if (!in_flight_fences_[i]) {
                throw std::runtime_error("Failed to create fence");
            }

            // Create binary semaphores
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

        if (wait_value > 0) {  // Only wait if we've submitted before
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

        cmd->begin_render_pass(
            render_pass_,
            framebuffers_[current_image_index_],
            width_,
            height_
        );

        cmd->set_viewport(0.0f, 0.0f,
            static_cast<float>(width_),
            static_cast<float>(height_));
        cmd->set_scissor(0, 0, width_, height_);

        frame_started_ = true;
    }

    void Renderer::end_frame()
    {
        if (!frame_started_) {
            UH_WARN("end_frame called but frame not started");
            return;
        }

        auto& cmd = command_buffers_[current_frame_];

        cmd->end_render_pass();
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
            // Frame was skipped (e.g., swapchain recreation)
            return;
        }

        // Execute custom rendering if callback is set
        if (render_callback_) {
            render_callback_(command_buffers_[current_frame_]);
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

    void Renderer::set_render_callback(RenderCallback callback)
    {
        render_callback_ = std::move(callback);
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
            // No change
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
        // Wait until window has valid size (not minimized)
        while (width_ == 0 || height_ == 0) {
            // This should be handled by the window system
            // For now, just log and break
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
            create_framebuffers();

            UH_INFO("Swapchain recreated successfully");
        }
        catch (const std::exception& e) {
            UH_ERROR_FMT("Failed to recreate swapchain: {}", e.what());
            throw;
        }
    }

    void Renderer::cleanup_swapchain()
    {
        framebuffers_.clear();
        depth_image_.reset();
        swapchain_.reset();

        UH_INFO("Swapchain resources cleaned up");
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

        framebuffers_.clear();
        render_pass_.reset();
        depth_image_.reset();
        swapchain_.reset();

        device_.reset();

        UH_INFO("Renderer cleaned up");
    }

} // namespace mango::app
