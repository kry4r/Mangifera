#pragma once
#include "sync/barrier.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace mango::graphics::vk
{
    // Vulkan 专用的 Barrier，提供更精细的控制
    struct Vk_Barrier : public Barrier
    {
        // 子资源范围控制（用于 Texture）
        uint32_t base_mip_level = 0;
        uint32_t mip_level_count = VK_REMAINING_MIP_LEVELS; // 所有 mip levels
        uint32_t base_array_layer = 0;
        uint32_t array_layer_count = VK_REMAINING_ARRAY_LAYERS; // 所有 array layers

        // 队列族转换（用于跨队列传输）
        uint32_t src_queue_family = VK_QUEUE_FAMILY_IGNORED;
        uint32_t dst_queue_family = VK_QUEUE_FAMILY_IGNORED;

        // 更精细的 pipeline stage 控制（可选，如果为 0 则使用默认映射）
        VkPipelineStageFlags src_stage_mask = 0;
        VkPipelineStageFlags dst_stage_mask = 0;

        // 更精细的 access flags 控制（可选，如果为 0 则使用默认映射）
        VkAccessFlags src_access_mask = 0;
        VkAccessFlags dst_access_mask = 0;

        // 依赖标志
        VkDependencyFlags dependency_flags = 0; // e.g., VK_DEPENDENCY_BY_REGION_BIT

        // 构造函数：从基础 Barrier 转换
        Vk_Barrier() = default;

        explicit Vk_Barrier(const Barrier& base)
            : Barrier(base)
        {
        }

        // 辅助方法：设置子资源范围
        Vk_Barrier& set_subresource(uint32_t base_mip, uint32_t mip_count,
                                     uint32_t base_layer, uint32_t layer_count)
        {
            base_mip_level = base_mip;
            mip_level_count = mip_count;
            base_array_layer = base_layer;
            array_layer_count = layer_count;
            return *this;
        }

        // 辅助方法：设置队列族转换
        Vk_Barrier& set_queue_family_transfer(uint32_t src_family, uint32_t dst_family)
        {
            src_queue_family = src_family;
            dst_queue_family = dst_family;
            return *this;
        }

        // 辅助方法：设置精细的 stage 控制
        Vk_Barrier& set_stages(VkPipelineStageFlags src, VkPipelineStageFlags dst)
        {
            src_stage_mask = src;
            dst_stage_mask = dst;
            return *this;
        }

        // 辅助方法：设置精细的 access 控制
        Vk_Barrier& set_access(VkAccessFlags src, VkAccessFlags dst)
        {
            src_access_mask = src;
            dst_access_mask = dst;
            return *this;
        }
    };

    // 批量 barrier 辅助结构
    struct Vk_Barrier_Batch
    {
        std::vector<VkBufferMemoryBarrier> buffer_barriers;
        std::vector<VkImageMemoryBarrier> image_barriers;
        VkPipelineStageFlags src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkDependencyFlags dependency_flags = 0;

        bool empty() const
        {
            return buffer_barriers.empty() && image_barriers.empty();
        }

        void clear()
        {
            buffer_barriers.clear();
            image_barriers.clear();
            src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependency_flags = 0;
        }
    };

} // namespace mango::graphics::vk
