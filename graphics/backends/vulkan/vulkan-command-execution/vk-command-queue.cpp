#include "vk-command-queue.hpp"
#include "vk-command-buffer.hpp"
#include "vulkan-sync/vk-fence.hpp"
#include "vulkan-sync/vk-semaphore.hpp"
#include "vulkan-render-pass/vk-swapchain.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Command_Queue::Vk_Command_Queue(VkDevice device, VkQueue queue, uint32_t queue_family_index, Queue_Type type)
        : m_device(device)
        , m_queue(queue)
        , m_queue_family_index(queue_family_index)
        , m_type(type)
    {
        if (m_queue == VK_NULL_HANDLE) {
            throw std::runtime_error("Vulkan queue is null");
        }

        UH_INFO_FMT("Command queue created (type: {}, family: {})",
            static_cast<int>(type), queue_family_index);
    }

    Vk_Command_Queue::~Vk_Command_Queue()
    {
        // Queue is owned by device, don't destroy it
    }

    void Vk_Command_Queue::submit(const Submit_Info& info, std::shared_ptr<Fence> fence)
    {
        // Convert command buffers
        std::vector<VkCommandBuffer> vk_command_buffers;
        vk_command_buffers.reserve(info.command_buffers.size());

        for (const auto& cmd : info.command_buffers) {
            auto vk_cmd = std::dynamic_pointer_cast<Vk_Command_Buffer>(cmd);
            if (!vk_cmd) {
                throw std::runtime_error("Invalid command buffer type for Vulkan queue");
            }
            vk_command_buffers.push_back(vk_cmd->get_vk_command_buffer());
        }

        // Convert wait semaphores
        std::vector<VkSemaphore> vk_wait_semaphores;
        std::vector<VkPipelineStageFlags> vk_wait_stages;
        vk_wait_semaphores.reserve(info.wait_semaphores.size());
        vk_wait_stages.reserve(info.wait_stage_masks.size());

        for (const auto& semaphore : info.wait_semaphores) {
            auto vk_semaphore = std::dynamic_pointer_cast<Vk_Semaphore>(semaphore);
            if (!vk_semaphore) {
                throw std::runtime_error("Invalid semaphore type for Vulkan queue");
            }
            vk_wait_semaphores.push_back(vk_semaphore->get_vk_semaphore());
        }

        // Convert stage masks
        for (uint32_t mask : info.wait_stage_masks) {
            vk_wait_stages.push_back(static_cast<VkPipelineStageFlags>(mask));
        }

        // If no stage masks provided but have wait semaphores, default to all commands
        if (vk_wait_stages.empty() && !vk_wait_semaphores.empty()) {
            vk_wait_stages.resize(vk_wait_semaphores.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        }

        // Convert signal semaphores
        std::vector<VkSemaphore> vk_signal_semaphores;
        vk_signal_semaphores.reserve(info.signal_semaphores.size());

        for (const auto& semaphore : info.signal_semaphores) {
            auto vk_semaphore = std::dynamic_pointer_cast<Vk_Semaphore>(semaphore);
            if (!vk_semaphore) {
                throw std::runtime_error("Invalid semaphore type for Vulkan queue");
            }
            vk_signal_semaphores.push_back(vk_semaphore->get_vk_semaphore());
        }

        // Handle fence (timeline semaphore)
        VkSemaphore fence_semaphore = VK_NULL_HANDLE;
        uint64_t fence_signal_value = 0;

        if (fence) {
            auto vk_fence = std::dynamic_pointer_cast<Vk_Fence>(fence);
            if (!vk_fence) {
                throw std::runtime_error("Invalid fence type for Vulkan queue");
            }

            fence_semaphore = vk_fence->get_vk_semaphore();
            fence_signal_value = vk_fence->get_completed_value() + 1;

            vk_signal_semaphores.push_back(fence_semaphore);
        }

        // Create submit info
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = static_cast<uint32_t>(vk_command_buffers.size());
        submit_info.pCommandBuffers = vk_command_buffers.data();
        submit_info.waitSemaphoreCount = static_cast<uint32_t>(vk_wait_semaphores.size());
        submit_info.pWaitSemaphores = vk_wait_semaphores.data();
        submit_info.pWaitDstStageMask = vk_wait_stages.data();
        submit_info.signalSemaphoreCount = static_cast<uint32_t>(vk_signal_semaphores.size());
        submit_info.pSignalSemaphores = vk_signal_semaphores.data();

        // Timeline semaphore info (for fence)
        VkTimelineSemaphoreSubmitInfo timeline_info{};
        std::vector<uint64_t> wait_values;
        std::vector<uint64_t> signal_values;

        if (fence) {
            wait_values.resize(vk_wait_semaphores.size(), 0);

            signal_values.resize(info.signal_semaphores.size(), 0); // binary semaphores
            signal_values.push_back(fence_signal_value); // fence (timeline semaphore)

            timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timeline_info.waitSemaphoreValueCount = static_cast<uint32_t>(wait_values.size());
            timeline_info.pWaitSemaphoreValues = wait_values.data();
            timeline_info.signalSemaphoreValueCount = static_cast<uint32_t>(signal_values.size());
            timeline_info.pSignalSemaphoreValues = signal_values.data();

            submit_info.pNext = &timeline_info;
        }

        // Submit to queue
        if (vkQueueSubmit(m_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer to Vulkan queue");
        }
    }

    void Vk_Command_Queue::present(std::shared_ptr<Swapchain> swapchain,
                                   uint32_t image_index,
                                   const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores)
    {
        auto vk_swapchain = std::dynamic_pointer_cast<Vk_Swapchain>(swapchain);
        if (!vk_swapchain) {
            throw std::runtime_error("Invalid swapchain type for Vulkan queue");
        }

        // Convert wait semaphores (should be binary semaphores)
        std::vector<VkSemaphore> vk_wait_semaphores;
        vk_wait_semaphores.reserve(wait_semaphores.size());

        for (const auto& semaphore : wait_semaphores) {
            auto vk_semaphore = std::dynamic_pointer_cast<Vk_Semaphore>(semaphore);
            if (!vk_semaphore) {
                throw std::runtime_error("Invalid semaphore type for Vulkan queue");
            }

            if (vk_semaphore->get_type() != Semaphore_Type::binary) {
                UH_WARN("Present queue expects binary semaphores, got timeline semaphore");
            }

            vk_wait_semaphores.push_back(vk_semaphore->get_vk_semaphore());
        }

        VkSwapchainKHR vk_swapchain_handle = vk_swapchain->get_vk_swapchain();

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = static_cast<uint32_t>(vk_wait_semaphores.size());
        present_info.pWaitSemaphores = vk_wait_semaphores.data();
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &vk_swapchain_handle;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;

        VkResult result = vkQueuePresentKHR(m_queue, &present_info);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            UH_WARN("Swapchain out of date");
            // Should trigger swapchain recreation
        } else if (result == VK_SUBOPTIMAL_KHR) {
            UH_WARN("Swapchain suboptimal");
            // May want to recreate swapchain
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image");
        }
    }

    void Vk_Command_Queue::wait_idle()
    {
        if (vkQueueWaitIdle(m_queue) != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for Vulkan queue to idle");
        }
    }

} // namespace mango::graphics::vk
