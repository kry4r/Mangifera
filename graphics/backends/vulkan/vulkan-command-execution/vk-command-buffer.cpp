#include "command-execution/command-pool.hpp"
#include "vk-command-buffer.hpp"
#include "vulkan-render-resource/vk-buffer.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include "vulkan-render-resource/vk-descriptor-set.hpp"
#include "vulkan-render-pass/vk-render-pass.hpp"
#include "vulkan-render-pass/vk-framebuffer.hpp"
#include "vulkan-pipeline-state/vk-compute-pipeline-state.hpp"
#include "vulkan-pipeline-state/vk-graphics-pipeline-state.hpp"
#include "vulkan-pipeline-state/vk-raytracing-pipeline-state.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Command_Buffer::Vk_Command_Buffer(VkDevice device, VkCommandBuffer cmd_buffer, VkCommandPool pool, Command_Buffer_Level level)
        : m_device(device)
        , m_command_buffer(cmd_buffer)
        , m_pool(pool)
        , m_level(level)
        , m_state(Command_Buffer_State::initial)
    {
    }

    Vk_Command_Buffer::~Vk_Command_Buffer()
    {
        // Command buffer is freed by the pool, not individually
        // Just set to null to indicate it's no longer valid
        m_command_buffer = VK_NULL_HANDLE;
    }

    // ========== Record lifecycle ==========

    void Vk_Command_Buffer::begin()
    {
        if (m_state == Command_Buffer_State::recording) {
            UH_ERROR("Command buffer is already in recording state");
            return;
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0; // Optional: VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, etc.
        begin_info.pInheritanceInfo = nullptr; // For secondary command buffers

        if (vkBeginCommandBuffer(m_command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        m_state = Command_Buffer_State::recording;
    }

    void Vk_Command_Buffer::end()
    {
        if (m_state != Command_Buffer_State::recording) {
            UH_ERROR("Command buffer is not in recording state");
            return;
        }

        if (vkEndCommandBuffer(m_command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end recording command buffer");
        }

        m_state = Command_Buffer_State::executable;
    }

    void Vk_Command_Buffer::reset()
    {
        // Note: Command buffer must have been allocated from a pool with
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag
        VkCommandBufferResetFlags flags = 0; // or VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT

        if (vkResetCommandBuffer(m_command_buffer, flags) != VK_SUCCESS) {
            UH_ERROR("Failed to reset command buffer");
            throw std::runtime_error("Failed to reset command buffer");
        }

        m_state = Command_Buffer_State::initial;
        m_current_pipeline = VK_NULL_HANDLE;
        m_current_pipeline_layout = VK_NULL_HANDLE;
    }

    // ========== Render pass control ==========

    void Vk_Command_Buffer::begin_render_pass(
        std::shared_ptr<Render_Pass> render_pass,
        std::shared_ptr<Framebuffer> framebuffer,
        uint32_t width,
        uint32_t height,
        Subpass_Contents contents)
    {
        auto vk_render_pass = std::dynamic_pointer_cast<Vk_Render_Pass>(render_pass);
        auto vk_framebuffer = std::dynamic_pointer_cast<Vk_Framebuffer>(framebuffer);

        if (!vk_render_pass || !vk_framebuffer) {
            UH_ERROR("Invalid render pass or framebuffer type");
            throw std::runtime_error("Invalid render pass or framebuffer type");
        }

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = vk_render_pass->get_vk_render_pass();
        render_pass_info.framebuffer = vk_framebuffer->get_vk_framebuffer();
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = {width, height};

        // Setup clear values for attachments that use CLEAR load op
        const auto& desc = render_pass->get_desc();
        std::vector<VkClearValue> clear_values(desc.attachments.size());

        for (size_t i = 0; i < desc.attachments.size(); ++i) {
            const auto& attachment = desc.attachments[i];

            // Check if this is a depth-stencil attachment
            auto texture = std::dynamic_pointer_cast<Vk_Texture>(attachment.texture);
            if (texture) {
                const auto& tex_desc = texture->getDesc();
                bool is_depth = (tex_desc.format == Texture_Format::depth24 ||
                    tex_desc.format == Texture_Format::depth32f ||
                    tex_desc.format == Texture_Format::depth24_stencil8 ||
                    tex_desc.format == Texture_Format::depth32f_stencil8);

                if (is_depth) {
                    clear_values[i].depthStencil = {1.0f, 0};
                } else {
                    clear_values[i].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
                }
            }
        }

        render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        VkSubpassContents vk_contents = (contents == Subpass_Contents::inline_contents)
            ? VK_SUBPASS_CONTENTS_INLINE
            : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

        vkCmdBeginRenderPass(m_command_buffer, &render_pass_info, vk_contents);
    }

    void Vk_Command_Buffer::next_subpass(Subpass_Contents contents)
    {
        VkSubpassContents vk_contents = (contents == Subpass_Contents::inline_contents)
            ? VK_SUBPASS_CONTENTS_INLINE
            : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;

        vkCmdNextSubpass(m_command_buffer, vk_contents);
    }

    void Vk_Command_Buffer::end_render_pass()
    {
        vkCmdEndRenderPass(m_command_buffer);
    }

    // ========== Bind pipeline / descriptor sets ==========

    void Vk_Command_Buffer::bind_pipeline(std::shared_ptr<Pipeline_State> pipeline)
    {
        if (!pipeline) {
            throw std::runtime_error("Pipeline is null");
        }

        auto type = pipeline->get_type();

        if (type == Pipeline_Type::graphics) {
            auto vk_graphics = std::dynamic_pointer_cast<Vk_Graphics_Pipeline_State>(pipeline);
            if (vk_graphics) {
                m_current_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
                m_current_pipeline = vk_graphics->get_vk_pipeline();
                m_current_pipeline_layout = vk_graphics->get_vk_pipeline_layout();
                vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_current_pipeline);
            }
        } else if (type == Pipeline_Type::compute) {
            auto vk_compute = std::dynamic_pointer_cast<Vk_Compute_Pipeline_State>(pipeline);
            if (vk_compute) {
                m_current_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
                m_current_pipeline = vk_compute->get_vk_pipeline();
                m_current_pipeline_layout = vk_compute->get_vk_pipeline_layout();
                vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_current_pipeline);
            }
        } else if (type == Pipeline_Type::raytracing) {
            auto vk_raytracing = std::dynamic_pointer_cast<Vk_Raytracing_Pipeline_State>(pipeline);
            if (vk_raytracing) {
                m_current_bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
                m_current_pipeline = vk_raytracing->get_vk_pipeline();
                m_current_pipeline_layout = vk_raytracing->get_vk_pipeline_layout();
                vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_current_pipeline);
            }
        }
    }

    void Vk_Command_Buffer::bind_descriptor_set(uint32_t set_index,
        std::shared_ptr<Descriptor_Set> set)
    {
        auto vk_set = std::dynamic_pointer_cast<Vk_Descriptor_Set>(set);
        if (!vk_set) {
            UH_ERROR("Invalid descriptor set type for Vulkan command buffer");
            return;
        }

        VkDescriptorSet vk_descriptor_set = vk_set->get_vk_descriptor_set();

        vkCmdBindDescriptorSets(
            m_command_buffer,
            m_current_bind_point,
            m_current_pipeline_layout,
            set_index,
            1,
            &vk_descriptor_set,
            0,
            nullptr
        );
    }

    // ========== Bind vertex/index buffers ==========

    void Vk_Command_Buffer::bind_vertex_buffer(uint32_t binding, std::shared_ptr<Buffer> buffer, uint64_t offset)
    {
        auto vk_buffer = std::dynamic_pointer_cast<Vk_Buffer>(buffer);
        if (!vk_buffer) {
            UH_ERROR("Invalid buffer type for Vulkan command buffer");
            return;
        }

        VkBuffer vk_buf = vk_buffer->get_vk_buffer();
        VkDeviceSize vk_offset = offset;

        vkCmdBindVertexBuffers(m_command_buffer, binding, 1, &vk_buf, &vk_offset);
    }

    void Vk_Command_Buffer::bind_index_buffer(std::shared_ptr<Buffer> buffer, uint64_t offset, uint32_t index_type)
    {
        auto vk_buffer = std::dynamic_pointer_cast<Vk_Buffer>(buffer);
        if (!vk_buffer) {
            UH_ERROR("Invalid buffer type for Vulkan command buffer");
            return;
        }

        VkBuffer vk_buf = vk_buffer->get_vk_buffer();

        // index_type: 0 = uint16, 1 = uint32
        VkIndexType vk_index_type = (index_type == 0) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

        vkCmdBindIndexBuffer(m_command_buffer, vk_buf, offset, vk_index_type);
    }

    // ========== Set viewport/scissor ==========

    void Vk_Command_Buffer::set_viewport(float x, float y, float width, float height, float min_depth, float max_depth)
    {
        VkViewport viewport{};
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = min_depth;
        viewport.maxDepth = max_depth;

        vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    }

    void Vk_Command_Buffer::set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        VkRect2D scissor{};
        scissor.offset = {x, y};
        scissor.extent = {width, height};

        vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
    }

    // ========== Draw calls ==========

    void Vk_Command_Buffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
    {
        vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    void Vk_Command_Buffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
    {
        vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    // ========== Dispatch for compute ==========

    void Vk_Command_Buffer::dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        vkCmdDispatch(m_command_buffer, group_count_x, group_count_y, group_count_z);
    }

    // ========== Resource copy / upload ==========

    void Vk_Command_Buffer::copy_buffer(std::shared_ptr<Buffer> src, std::shared_ptr<Buffer> dst,
        uint64_t src_offset, uint64_t dst_offset, uint64_t size)
    {
        auto vk_src = std::dynamic_pointer_cast<Vk_Buffer>(src);
        auto vk_dst = std::dynamic_pointer_cast<Vk_Buffer>(dst);

        if (!vk_src || !vk_dst) {
            UH_ERROR("Invalid buffer types for Vulkan copy");
            return;
        }

        VkBufferCopy copy_region{};
        copy_region.srcOffset = src_offset;
        copy_region.dstOffset = dst_offset;
        copy_region.size = size;

        vkCmdCopyBuffer(m_command_buffer, vk_src->get_vk_buffer(), vk_dst->get_vk_buffer(), 1, &copy_region);
    }

    void Vk_Command_Buffer::copy_buffer_to_texture(std::shared_ptr<Buffer> src, std::shared_ptr<Texture> dst,
        uint32_t width, uint32_t height, uint32_t mip, uint32_t array_layer)
    {
        auto vk_src = std::dynamic_pointer_cast<Vk_Buffer>(src);
        auto vk_dst = std::dynamic_pointer_cast<Vk_Texture>(dst);

        if (!vk_src || !vk_dst) {
            UH_ERROR("Invalid resource types for Vulkan copy");
            return;
        }

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;   // Tightly packed
        region.bufferImageHeight = 0; // Tightly packed

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Assume color, would need to check for depth
        region.imageSubresource.mipLevel = mip;
        region.imageSubresource.baseArrayLayer = array_layer;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
            m_command_buffer,
            vk_src->get_vk_buffer(),
            vk_dst->get_vk_image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Image must be in this layout
            1,
            &region
        );
    }

    // ========== Barriers ==========
    void Vk_Command_Buffer::resource_barrier(const Barrier& barrier)
    {
        Vk_Barrier vk_barrier(barrier);
        resource_barrier(vk_barrier);
    }

    void Vk_Command_Buffer::resource_barrier(const Vk_Barrier& barrier)
    {
        if (!barrier.resource) {
            return;
        }

        Vk_Barrier_Batch batch;
        process_barrier_internal(barrier, batch);

        if (!batch.empty()) {
            submit_barrier_batch(batch);
        }
    }

    void Vk_Command_Buffer::resource_barriers(const std::vector<Vk_Barrier>& barriers)
    {
        if (barriers.empty()) {
            return;
        }

        Vk_Barrier_Batch batch;

        for (const auto& barrier : barriers) {
            if (barrier.resource) {
                process_barrier_internal(barrier, batch);
            }
        }

        if (!batch.empty()) {
            submit_barrier_batch(batch);
        }
    }

    void Vk_Command_Buffer::process_barrier_internal(const Vk_Barrier& barrier, Vk_Barrier_Batch& batch)
    {
        VkPipelineStageFlags src_stage = barrier.src_stage_mask != 0
            ? barrier.src_stage_mask
            : resource_state_to_pipeline_stage(barrier.before);

        VkPipelineStageFlags dst_stage = barrier.dst_stage_mask != 0
            ? barrier.dst_stage_mask
            : resource_state_to_pipeline_stage(barrier.after);

        VkAccessFlags src_access = barrier.src_access_mask != 0
            ? barrier.src_access_mask
            : resource_state_to_access_flags(barrier.before);

        VkAccessFlags dst_access = barrier.dst_access_mask != 0
            ? barrier.dst_access_mask
            : resource_state_to_access_flags(barrier.after);

        batch.src_stage_mask |= src_stage;
        batch.dst_stage_mask |= dst_stage;
        batch.dependency_flags |= barrier.dependency_flags;

        // Try to cast to Buffer first
        auto buffer = static_cast<Buffer*>(barrier.resource);
        auto vk_buffer = dynamic_cast<Vk_Buffer*>(buffer);

        if (vk_buffer) {
            VkBufferMemoryBarrier buffer_barrier{};
            buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            buffer_barrier.srcAccessMask = src_access;
            buffer_barrier.dstAccessMask = dst_access;
            buffer_barrier.srcQueueFamilyIndex = barrier.src_queue_family;
            buffer_barrier.dstQueueFamilyIndex = barrier.dst_queue_family;
            buffer_barrier.buffer = vk_buffer->get_vk_buffer();
            buffer_barrier.offset = 0;
            buffer_barrier.size = VK_WHOLE_SIZE;

            batch.buffer_barriers.push_back(buffer_barrier);
            return;
        }

        // Try to cast to Texture
        auto texture = static_cast<Texture*>(barrier.resource);
        auto vk_texture = dynamic_cast<Vk_Texture*>(texture);

        if (vk_texture) {
            VkImageMemoryBarrier image_barrier{};
            image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_barrier.srcAccessMask = src_access;
            image_barrier.dstAccessMask = dst_access;
            image_barrier.oldLayout = resource_state_to_image_layout(barrier.before);
            image_barrier.newLayout = resource_state_to_image_layout(barrier.after);
            image_barrier.srcQueueFamilyIndex = barrier.src_queue_family;
            image_barrier.dstQueueFamilyIndex = barrier.dst_queue_family;
            image_barrier.image = vk_texture->get_vk_image();

            const auto& desc = vk_texture->getDesc();
            if (desc.format == Texture_Format::depth24 ||
                desc.format == Texture_Format::depth32f ||
                desc.format == Texture_Format::depth24_stencil8 ||
                desc.format == Texture_Format::depth32f_stencil8) {
                image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                if (desc.format == Texture_Format::depth24_stencil8 ||
                    desc.format == Texture_Format::depth32f_stencil8) {
                    image_barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            } else {
                image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            image_barrier.subresourceRange.baseMipLevel = barrier.base_mip_level;
            image_barrier.subresourceRange.levelCount = barrier.mip_level_count;
            image_barrier.subresourceRange.baseArrayLayer = barrier.base_array_layer;
            image_barrier.subresourceRange.layerCount = barrier.array_layer_count;

            batch.image_barriers.push_back(image_barrier);
            return;
        }

        UH_ERROR("Unknown resource type in barrier");
    }

    void Vk_Command_Buffer::submit_barrier_batch(const Vk_Barrier_Batch& batch)
    {
        if (batch.empty()) {
            return;
        }

        vkCmdPipelineBarrier(
            m_command_buffer,
            batch.src_stage_mask,
            batch.dst_stage_mask,
            batch.dependency_flags,
            0, nullptr, // Memory barriers
            static_cast<uint32_t>(batch.buffer_barriers.size()),
            batch.buffer_barriers.empty() ? nullptr : batch.buffer_barriers.data(),
            static_cast<uint32_t>(batch.image_barriers.size()),
            batch.image_barriers.empty() ? nullptr : batch.image_barriers.data()
        );
    }

    // ========== Helper functions for state conversion ==========

    VkImageLayout Vk_Command_Buffer::resource_state_to_image_layout(Resource_State state) const
    {
        switch (state) {
            case Resource_State::undefined:         return VK_IMAGE_LAYOUT_UNDEFINED;
            case Resource_State::common:            return VK_IMAGE_LAYOUT_GENERAL;
            case Resource_State::render_target:     return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case Resource_State::depth_stencil:     return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case Resource_State::shader_resource:   return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case Resource_State::unordered_access:  return VK_IMAGE_LAYOUT_GENERAL;
            case Resource_State::copy_src:          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case Resource_State::copy_dst:          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case Resource_State::present:           return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:                                return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    VkPipelineStageFlags Vk_Command_Buffer::resource_state_to_pipeline_stage(Resource_State state) const
    {
        switch (state) {
            case Resource_State::undefined:
                return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            case Resource_State::common:
                return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            case Resource_State::render_target:
                return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            case Resource_State::depth_stencil:
                return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

            case Resource_State::shader_resource:
                return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            case Resource_State::unordered_access:
                return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            case Resource_State::copy_src:
            case Resource_State::copy_dst:
                return VK_PIPELINE_STAGE_TRANSFER_BIT;

            case Resource_State::present:
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

            default:
                return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
    }

    VkAccessFlags Vk_Command_Buffer::resource_state_to_access_flags(Resource_State state) const
    {
        switch (state) {
            case Resource_State::undefined:
                return 0;

            case Resource_State::common:
                return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

            case Resource_State::render_target:
                return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            case Resource_State::depth_stencil:
                return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            case Resource_State::shader_resource:
                return VK_ACCESS_SHADER_READ_BIT;

            case Resource_State::unordered_access:
                return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            case Resource_State::copy_src:
                return VK_ACCESS_TRANSFER_READ_BIT;

            case Resource_State::copy_dst:
                return VK_ACCESS_TRANSFER_WRITE_BIT;

            case Resource_State::present:
                return 0;

            default:
                return 0;
        }
    }

    // ========== Push constants ==========

    void Vk_Command_Buffer::push_constants(uint32_t offset, uint32_t size, const void* data)
    {
        if (m_current_pipeline_layout == VK_NULL_HANDLE) {
            UH_ERROR("No pipeline bound, cannot push constants");
            return;
        }

        // Assume all shader stages for now
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_ALL;

        vkCmdPushConstants(m_command_buffer, m_current_pipeline_layout, stage_flags, offset, size, data);
    }

    // ========== Secondary command buffer execution ==========

    void Vk_Command_Buffer::execute_secondary(std::shared_ptr<Command_Buffer> secondary)
    {
        auto vk_secondary = std::dynamic_pointer_cast<Vk_Command_Buffer>(secondary);
        if (!vk_secondary) {
            UH_ERROR("Invalid command buffer type for execute_secondary");
            return;
        }

        VkCommandBuffer vk_cmd = vk_secondary->get_vk_command_buffer();
        vkCmdExecuteCommands(m_command_buffer, 1, &vk_cmd);
    }

    // ========== Debug helpers ==========

    void Vk_Command_Buffer::begin_debug_region(const char* name)
    {
        // This requires VK_EXT_debug_utils extension
        // We need to load the function pointer
        auto func = (PFN_vkCmdBeginDebugUtilsLabelEXT) vkGetDeviceProcAddr(m_device, "vkCmdBeginDebugUtilsLabelEXT");
        if (func) {
            VkDebugUtilsLabelEXT label_info{};
            label_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label_info.pLabelName = name;
            label_info.color[0] = 1.0f;
            label_info.color[1] = 1.0f;
            label_info.color[2] = 1.0f;
            label_info.color[3] = 1.0f;
            func(m_command_buffer, &label_info);
        }
    }

    void Vk_Command_Buffer::end_debug_region()
    {
        auto func = (PFN_vkCmdEndDebugUtilsLabelEXT) vkGetDeviceProcAddr(m_device, "vkCmdEndDebugUtilsLabelEXT");
        if (func) {
            func(m_command_buffer);
        }
    }

    // ========== Pool management ==========

    void Vk_Command_Buffer::mark_freed()
    {
        m_command_buffer = VK_NULL_HANDLE;
        m_state = Command_Buffer_State::invalid;
    }

    void Vk_Command_Buffer::mark_reset_by_pool()
    {
        m_state = Command_Buffer_State::initial;
        m_current_pipeline = VK_NULL_HANDLE;
        m_current_pipeline_layout = VK_NULL_HANDLE;
    }

} // namespace mango::graphics::vk
