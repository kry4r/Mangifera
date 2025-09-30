#include "vk-fence.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Fence::Vk_Fence(VkDevice device, bool signaled)
        : m_device(device)
        , m_initial_value(signaled ? 1 : 0)
    {
        create_timeline_semaphore(signaled);
    }

    Vk_Fence::~Vk_Fence()
    {
        cleanup();
    }

    Vk_Fence::Vk_Fence(Vk_Fence&& other) noexcept
        : m_device(other.m_device)
        , m_semaphore(other.m_semaphore)
        , m_initial_value(other.m_initial_value)
    {
        other.m_semaphore = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Fence::operator=(Vk_Fence&& other) noexcept -> Vk_Fence&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_semaphore = other.m_semaphore;
            m_initial_value = other.m_initial_value;

            other.m_semaphore = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Fence::create_timeline_semaphore(bool signaled)
    {
        VkSemaphoreTypeCreateInfo timeline_info{};
        timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_info.initialValue = m_initial_value;

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = &timeline_info;

        if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan timeline semaphore for fence");
        }

        UH_INFO_FMT("Fence created (timeline semaphore, initial value: {})", m_initial_value);
    }

    std::uint64_t Vk_Fence::get_completed_value() const
    {
        uint64_t value = 0;
        if (vkGetSemaphoreCounterValue(m_device, m_semaphore, &value) != VK_SUCCESS) {
            throw std::runtime_error("Failed to get fence completed value");
        }
        return value;
    }

    void Vk_Fence::wait(std::uint64_t value, std::uint64_t timeout_ns)
    {
        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &m_semaphore;
        wait_info.pValues = &value;

        VkResult result = vkWaitSemaphores(m_device, &wait_info, timeout_ns);

        if (result == VK_TIMEOUT) {
            UH_WARN_FMT("Fence wait timed out (waiting for value {})", value);
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for fence");
        }
    }

    void Vk_Fence::signal(std::uint64_t value)
    {
        VkSemaphoreSignalInfo signal_info{};
        signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = m_semaphore;
        signal_info.value = value;

        if (vkSignalSemaphore(m_device, &signal_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to signal fence");
        }
    }

    void Vk_Fence::cleanup()
    {
        if (m_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_semaphore, nullptr);
            m_semaphore = VK_NULL_HANDLE;
            UH_INFO("Fence destroyed");
        }
    }

} // namespace mango::graphics::vk
