#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "command-execution/command-buffer.hpp"
#include "command-execution/command-pool.hpp"
#include "command-execution/command-queue.hpp"
#include "render-pass/render-pass.hpp"
#include "render-pass/framebuffer.hpp"
#include "render-pass/swapchain.hpp"
#include "pipeline-state/pipeline-state.hpp"
#include "pipeline-state/compute-pipeline-state.hpp"
#include "pipeline-state/graphics-pipeline-state.hpp"
#include "pipeline-state/raytracing-pipeline-state.hpp"
#include "render-resource/buffer.hpp"
#include "render-resource/texture.hpp"
#include "render-resource/sampler.hpp"
#include "render-resource/shader.hpp"
#include "render-resource/descriptor-set.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"

namespace mango::graphics
{
    struct Device_Desc
    {
        bool enable_validation = false;
        bool enable_raytracing = false;
        uint32_t preferred_adapter_index = 0; // optional
    };

    class Device
    {
    public:
        virtual ~Device() = default;

        // Resource creation
        virtual Command_Pool_Handle create_command_pool() = 0;
        virtual Command_Queue_Handle create_command_queue(Queue_Type type = Queue_Type::graphics) = 0;

        virtual Fence_Handle create_fence(bool signaled = false) = 0;
        virtual Semaphore_Handle create_semaphore(bool timeline = false, uint64_t initial_value = 0) = 0;

        virtual Buffer_Handle create_buffer(const Buffer_Desc& desc) = 0;
        virtual Texture_Handle create_texture(const Texture_Desc& desc) = 0;
        virtual Sampler_Handle create_sampler(const Sampler_Desc& desc) = 0;
        virtual Shader_Handle create_shader(const Shader_Desc& desc) = 0;

        virtual Render_Pass_Handle create_render_pass(const Render_Pass_Desc& desc) = 0;
        virtual Framebuffer_Handle create_framebuffer(const Framebuffer_Desc& desc) = 0;
        virtual Swapchain_Handle create_swapchain(const Swapchain_Desc& desc) = 0;

        virtual Graphics_Pipeline_Handle create_graphics_pipeline(const Graphics_Pipeline_Desc& desc) = 0;
        virtual Compute_Pipeline_Handle create_compute_pipeline(const Compute_Pipeline_Desc& desc) = 0;
        virtual Raytracing_Pipeline_Handle create_raytracing_pipeline(const Raytracing_Pipeline_Desc& desc) = 0;

        // Device-level queries
        virtual uint32_t get_queue_family_count() const = 0;
        virtual std::vector<Queue_Type> get_supported_queues() const = 0;

        virtual Descriptor_Set_Layout_Handle create_descriptor_set_layout(
            const Descriptor_Set_Layout_Desc& desc) = 0;

        virtual Descriptor_Set_Handle create_descriptor_set(
            std::shared_ptr<Descriptor_Set_Layout> layout) = 0;

        // Device synchronization
        virtual void wait_idle() = 0;
    };

    using Device_Handle = std::shared_ptr<Device>;

} // namespace mango::graphics
