#pragma once
#include "sync/fence.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Fence : public Fence
    {
    public:
        Vk_Fence(VkDevice device, bool signaled);
        ~Vk_Fence() override;

        Vk_Fence(const Vk_Fence&) = delete;
        Vk_Fence& operator=(const Vk_Fence&) = delete;
        Vk_Fence(Vk_Fence&& other) noexcept;
        Vk_Fence& operator=(Vk_Fence&& other) noexcept;

        // Fence interface
        std::uint64_t get_completed_value() const override;
        void wait(std::uint64_t value, std::uint64_t timeout_ns = UINT64_MAX) override;
        void signal(std::uint64_t value) override;

        // Vulkan specific
        auto get_vk_semaphore() const -> VkSemaphore { return m_semaphore; }

    private:
        void create_timeline_semaphore(bool signaled);
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkSemaphore m_semaphore = VK_NULL_HANDLE; // Timeline semaphore
        std::uint64_t m_initial_value = 0;
    };

} // namespace mango::graphics::vk
