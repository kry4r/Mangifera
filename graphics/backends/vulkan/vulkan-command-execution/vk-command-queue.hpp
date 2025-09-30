#pragma once
#include "command-execution/command-queue.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Command_Queue : public Command_Queue
    {
    public:
        Vk_Command_Queue(VkDevice device, VkQueue queue, uint32_t queue_family_index, Queue_Type type);
        ~Vk_Command_Queue() override;

        Vk_Command_Queue(const Vk_Command_Queue&) = delete;
        Vk_Command_Queue& operator=(const Vk_Command_Queue&) = delete;

        // Command_Queue interface
        void submit(const Submit_Info& info, std::shared_ptr<Fence> fence = nullptr) override;

        void present(std::shared_ptr<Swapchain> swapchain,
                    uint32_t image_index,
                    const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) override;

        void wait_idle() override;

        Queue_Type get_type() const override { return m_type; }

        // Vulkan specific
        auto get_vk_queue() const -> VkQueue { return m_queue; }
        auto get_queue_family_index() const -> uint32_t { return m_queue_family_index; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_queue = VK_NULL_HANDLE;
        uint32_t m_queue_family_index = 0;
        Queue_Type m_type = Queue_Type::graphics;
    };

} // namespace mango::graphics::vk
