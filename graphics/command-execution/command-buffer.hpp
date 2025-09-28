#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "pipeline-state.hpp"
#include "sync/barrier.hpp"
#include "render-resource/buffer.hpp"
#include "render-resource/texture.hpp"
#include "render-resource/shader.hpp"

namespace mango::graphics
{
    class Pipeline_State;
    class Graphics_Pipeline_State;
    class Compute_Pipeline_State;
    class Render_Pass;
    class Framebuffer;
    class Descriptor_Set;
    class Sampler;
    class Texture;
    class Buffer;

    enum class Command_Buffer_State
    {
        initial,
        recording,
        executable,
        pending,
        invalid
    };

    // Subpass contents for inline/secondary usage (if relevant)
    enum class Subpass_Contents
    {
        inline_contents,
        secondary_command_buffers
    };

    // Command buffer interface - records GPU commands
    class Command_Buffer
    {
    public:
        virtual ~Command_Buffer() = default;

        // Record lifecycle
        virtual void begin() = 0;
        virtual void end() = 0;
        virtual void reset() = 0;

        // Render pass control
        virtual void begin_render_pass(std::shared_ptr<Render_Pass> renderPass,
                                       std::shared_ptr<Framebuffer> framebuffer,
                                       uint32_t width,
                                       uint32_t height,
                                       Subpass_Contents contents = Subpass_Contents::inline_contents) = 0;

        virtual void next_subpass(Subpass_Contents contents = Subpass_Contents::inline_contents) = 0;

        virtual void end_render_pass() = 0;

        // Bind pipeline / descriptor sets
        virtual void bind_pipeline(std::shared_ptr<Pipeline_State> pipeline) = 0;

        virtual void bind_descriptor_set(uint32_t setIndex, std::shared_ptr<Descriptor_Set> set) = 0;

        // Bind vertex/index buffers
        virtual void bind_vertex_buffer(uint32_t binding, std::shared_ptr<Buffer> buffer, uint64_t offset = 0) = 0;
        virtual void bind_index_buffer(std::shared_ptr<Buffer> buffer, uint64_t offset = 0, uint32_t indexType = 0) = 0;

        // Set viewport/scissor (simple forms)
        virtual void set_viewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
        virtual void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

        // Draw calls
        virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
        virtual void draw_indexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) = 0;

        // Dispatch for compute
        virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

        // Resource copy / upload helpers
        virtual void copy_buffer(std::shared_ptr<Buffer> src, std::shared_ptr<Buffer> dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size) = 0;
        virtual void copy_buffer_to_texture(std::shared_ptr<Buffer> src, std::shared_ptr<Texture> dst, uint32_t width, uint32_t height, uint32_t mip = 0, uint32_t arrayLayer = 0) = 0;

        //barriers
        virtual void resource_barrier(const Barrier& barrier) = 0;

        // Push constants (raw bytes)
        virtual void push_constants(uint32_t offset, uint32_t size, const void* data) = 0;

        // Secondary command buffer execution (if supported)
        virtual void execute_secondary(std::shared_ptr<Command_Buffer> secondary) = 0;

        // Query/Debug helpers (optional)
        virtual void begin_debug_region(const char* name) = 0;
        virtual void end_debug_region() = 0;

        // get current state
        virtual Command_Buffer_State get_state() const = 0;
    };

    using Command_Buffer_Handle = std::shared_ptr<Command_Buffer>;

} // namespace mango::graphics
