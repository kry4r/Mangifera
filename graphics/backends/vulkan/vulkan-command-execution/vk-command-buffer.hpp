#pragma once
#include "command-execution/command-buffer.hpp"
#include "vulkan-sync/vk-barrier.hpp"
#include "command-execution/command-pool.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Command_Buffer: public Command_Buffer
    {
    public:
        Vk_Command_Buffer(VkDevice device, VkCommandBuffer cmd_buffer, VkCommandPool pool, Command_Buffer_Level level);
        ~Vk_Command_Buffer() override;

        Vk_Command_Buffer(const Vk_Command_Buffer&) = delete;
        Vk_Command_Buffer& operator=(const Vk_Command_Buffer&) = delete;
        Vk_Command_Buffer(Vk_Command_Buffer&&) = delete;
        Vk_Command_Buffer& operator=(Vk_Command_Buffer&&) = delete;

        // ========== Record lifecycle ==========
        void begin() override;
        void end() override;
        void reset() override;

        // ========== Render pass control ==========
        void begin_render_pass(std::shared_ptr<Render_Pass> render_pass,
            std::shared_ptr<Framebuffer> framebuffer,
            uint32_t width,
            uint32_t height,
            Subpass_Contents contents = Subpass_Contents::inline_contents) override;

        void next_subpass(Subpass_Contents contents = Subpass_Contents::inline_contents) override;
        void end_render_pass() override;

        // ========== Bind pipeline / descriptor sets ==========
        void bind_pipeline(std::shared_ptr<Pipeline_State> pipeline) override;
        void bind_descriptor_set(uint32_t set_index, std::shared_ptr<Descriptor_Set> set) override;

        // ========== Bind vertex/index buffers ==========
        void bind_vertex_buffer(uint32_t binding, std::shared_ptr<Buffer> buffer, uint64_t offset = 0) override;
        void bind_index_buffer(std::shared_ptr<Buffer> buffer, uint64_t offset = 0, uint32_t index_type = 0) override;

        // ========== Set viewport/scissor ==========
        void set_viewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f) override;
        void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

        // ========== Draw calls ==========
        void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0) override;
        void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) override;

        // ========== Dispatch for compute ==========
        void dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;

        // ========== Resource copy / upload ==========
        void copy_buffer(std::shared_ptr<Buffer> src, std::shared_ptr<Buffer> dst, uint64_t src_offset, uint64_t dst_offset, uint64_t size) override;
        void copy_buffer_to_texture(std::shared_ptr<Buffer> src, std::shared_ptr<Texture> dst, uint32_t width, uint32_t height, uint32_t mip = 0, uint32_t array_layer = 0) override;

        // ========== Barriers ==========
        void resource_barrier(const Barrier& barrier) override;

        // Vulkan enhanced
        void resource_barrier(const Vk_Barrier& barrier);

        // Vulkan multi
        void resource_barriers(const std::vector<Vk_Barrier>& barriers);

        // direct
        void submit_barrier_batch(const Vk_Barrier_Batch& batch);

        // ========== Push constants ==========
        void push_constants(uint32_t offset, uint32_t size, const void* data) override;

        // ========== Secondary command buffer execution ==========
        void execute_secondary(std::shared_ptr<Command_Buffer> secondary) override;

        // ========== Debug helpers ==========
        void begin_debug_region(const char* name) override;
        void end_debug_region() override;

        // ========== State query ==========
        Command_Buffer_State get_state() const override { return m_state; }

        // ========== Vulkan specific ==========
        auto get_vk_command_buffer() const -> VkCommandBuffer { return m_command_buffer; }

        // Called by command pool
        void mark_freed();
        void mark_reset_by_pool();

    private:
        // Helper functions for barrier conversion
        VkImageLayout resource_state_to_image_layout(Resource_State state) const;
        VkPipelineStageFlags resource_state_to_pipeline_stage(Resource_State state) const;
        VkAccessFlags resource_state_to_access_flags(Resource_State state) const;
        void process_barrier_internal(const Vk_Barrier& barrier, Vk_Barrier_Batch& batch);

        void transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
            uint32_t mip_levels, uint32_t array_layers,
            VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
            VkAccessFlags src_access, VkAccessFlags dst_access);

        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
        VkCommandPool m_pool = VK_NULL_HANDLE;
        Command_Buffer_Level m_level = Command_Buffer_Level::primary;
        Command_Buffer_State m_state = Command_Buffer_State::initial;

        // Currently bound pipeline (for push constants)
        VkPipeline m_current_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_current_pipeline_layout = VK_NULL_HANDLE;
        VkPipelineBindPoint m_current_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    };

} // namespace mango::graphics::vk
