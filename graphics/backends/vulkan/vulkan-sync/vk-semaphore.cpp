#include "vk-semaphore.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Semaphore::Vk_Semaphore(VkDevice device, const Semaphore_Desc& desc)
        : m_device(device)
        , m_type(desc.type)
    {
        create_semaphore(desc);
    }

    Vk_Semaphore::~Vk_Semaphore()
    {
        cleanup();
    }

    Vk_Semaphore::Vk_Semaphore(Vk_Semaphore&& other) noexcept
        : m_device(other.m_device)
        , m_semaphore(other.m_semaphore)
        , m_type(other.m_type)
    {
        other.m_semaphore = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Semaphore::operator=(Vk_Semaphore&& other) noexcept -> Vk_Semaphore&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_semaphore = other.m_semaphore;
            m_type = other.m_type;

            other.m_semaphore = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Semaphore::create_semaphore(const Semaphore_Desc& desc)
    {
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphoreTypeCreateInfo timeline_info{};
        if (desc.type == Semaphore_Type::timeline) {
            timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timeline_info.initialValue = desc.initial_value;

            semaphore_info.pNext = &timeline_info;
        }
        // Binary semaphore: pNext = nullptr (default)

        if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_semaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan semaphore");
        }

        UH_INFO_FMT("Semaphore created (type: {})",
            desc.type == Semaphore_Type::binary ? "binary" : "timeline");
    }

    std::uint64_t Vk_Semaphore::get_value() const
    {
        if (m_type == Semaphore_Type::binary) {
            throw std::runtime_error("Cannot get value of binary semaphore");
        }

        uint64_t value = 0;
        if (vkGetSemaphoreCounterValue(m_device, m_semaphore, &value) != VK_SUCCESS) {
            throw std::runtime_error("Failed to get semaphore value");
        }

        return value;
    }

    void Vk_Semaphore::signal(std::uint64_t value)
    {
        if (m_type == Semaphore_Type::binary) {
            throw std::runtime_error("Cannot signal binary semaphore with value (use queue submission)");
        }

        VkSemaphoreSignalInfo signal_info{};
        signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signal_info.semaphore = m_semaphore;
        signal_info.value = value;

        if (vkSignalSemaphore(m_device, &signal_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to signal semaphore");
        }
    }

    void Vk_Semaphore::wait(std::uint64_t value)
    {
        if (m_type == Semaphore_Type::binary) {
            throw std::runtime_error("Cannot wait on binary semaphore with value (use queue submission)");
        }

        VkSemaphoreWaitInfo wait_info{};
        wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wait_info.semaphoreCount = 1;
        wait_info.pSemaphores = &m_semaphore;
        wait_info.pValues = &value;

        VkResult result = vkWaitSemaphores(m_device, &wait_info, UINT64_MAX);

        if (result == VK_TIMEOUT) {
            UH_WARN("Semaphore wait timed out");
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for semaphore");
        }
    }

    void Vk_Semaphore::cleanup()
    {
        if (m_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_semaphore, nullptr);
            m_semaphore = VK_NULL_HANDLE;
            UH_INFO("Semaphore destroyed");
        }
    }

} // namespace mango::graphics::vk
