#pragma once
#include "sync/semaphore.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Semaphore : public Semaphore
    {
    public:
        Vk_Semaphore(VkDevice device, const Semaphore_Desc& desc);
        ~Vk_Semaphore() override;

        Vk_Semaphore(const Vk_Semaphore&) = delete;
        Vk_Semaphore& operator=(const Vk_Semaphore&) = delete;
        Vk_Semaphore(Vk_Semaphore&& other) noexcept;
        Vk_Semaphore& operator=(Vk_Semaphore&& other) noexcept;

        // Semaphore interface
        Semaphore_Type get_type() const override { return m_type; }
        std::uint64_t get_value() const override;
        void signal(std::uint64_t value) override;
        void wait(std::uint64_t value) override;

        // Vulkan specific
        auto get_vk_semaphore() const -> VkSemaphore { return m_semaphore; }

    private:
        void create_semaphore(const Semaphore_Desc& desc);
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkSemaphore m_semaphore = VK_NULL_HANDLE;
        Semaphore_Type m_type = Semaphore_Type::binary;
    };

} // namespace mango::graphics::vk
