#include "vk-command-pool.hpp"
#include "vk-command-buffer.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <algorithm>

namespace mango::graphics::vk
{
    Vk_Command_Pool::Vk_Command_Pool(VkDevice device, uint32_t queue_family_index, bool transient, bool reset_command_buffer)
        : m_device(device)
        , m_queue_family_index(queue_family_index)
    {
        create_command_pool(transient, reset_command_buffer);
    }

    Vk_Command_Pool::~Vk_Command_Pool()
    {
        cleanup();
    }

    Vk_Command_Pool::Vk_Command_Pool(Vk_Command_Pool&& other) noexcept
        : m_device(other.m_device)
        , m_command_pool(other.m_command_pool)
        , m_queue_family_index(other.m_queue_family_index)
        , m_allocated_buffers(std::move(other.m_allocated_buffers))
    {
        other.m_command_pool = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Command_Pool::operator=(Vk_Command_Pool&& other) noexcept -> Vk_Command_Pool&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_command_pool = other.m_command_pool;
            m_queue_family_index = other.m_queue_family_index;
            m_allocated_buffers = std::move(other.m_allocated_buffers);

            other.m_command_pool = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Command_Pool::create_command_pool(bool transient, bool reset_command_buffer)
    {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = m_queue_family_index;
        pool_info.flags = 0;

        // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT:
        // Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
        if (transient) {
            pool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        }

        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT:
        // Allow command buffers to be rerecorded individually
        // Without this flag, all command buffers must be reset together by resetting the pool
        if (reset_command_buffer) {
            pool_info.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }

        if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        UH_INFO_FMT("Command pool created for queue family {}", m_queue_family_index);
    }

    std::shared_ptr<Command_Buffer> Vk_Command_Pool::allocate_command_buffer(Command_Buffer_Level level)
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = m_command_pool;
        alloc_info.level = (level == Command_Buffer_Level::primary)
            ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
            : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        alloc_info.commandBufferCount = 1;

        VkCommandBuffer vk_cmd_buffer;
        if (vkAllocateCommandBuffers(m_device, &alloc_info, &vk_cmd_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        // Create our wrapper
        auto cmd_buffer = std::make_shared<Vk_Command_Buffer>(m_device, vk_cmd_buffer, m_command_pool, level);

        // Track the allocated buffer
        m_allocated_buffers.push_back(cmd_buffer);

        // Clean up expired weak pointers periodically
        if (m_allocated_buffers.size() > 100) {
            m_allocated_buffers.erase(
                std::remove_if(m_allocated_buffers.begin(), m_allocated_buffers.end(),
                    [](const std::weak_ptr<Command_Buffer>& wp) { return wp.expired(); }),
                m_allocated_buffers.end()
            );
        }

        return cmd_buffer;
    }

    void Vk_Command_Pool::free_command_buffer(std::shared_ptr<Command_Buffer> cmd_buffer)
    {
        if (!cmd_buffer) {
            return;
        }

        // Cast to our Vulkan implementation
        auto vk_cmd_buffer = std::dynamic_pointer_cast<Vk_Command_Buffer>(cmd_buffer);
        if (!vk_cmd_buffer) {
            UH_ERROR("Attempted to free a non-Vulkan command buffer");
            return;
        }

        VkCommandBuffer vk_cmd = vk_cmd_buffer->get_vk_command_buffer();

        if (vk_cmd != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(m_device, m_command_pool, 1, &vk_cmd);
            vk_cmd_buffer->mark_freed(); // Tell the buffer it's been freed
        }

        // Remove from tracking
        m_allocated_buffers.erase(
            std::remove_if(m_allocated_buffers.begin(), m_allocated_buffers.end(),
                [&cmd_buffer](const std::weak_ptr<Command_Buffer>& wp) {
                    auto sp = wp.lock();
                    return !sp || sp == cmd_buffer;
                }),
            m_allocated_buffers.end()
        );
    }

    void Vk_Command_Pool::reset()
    {
        // Reset the entire pool, which resets all command buffers allocated from it
        // VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT releases resources back to the system
        VkCommandPoolResetFlags flags = 0; // or VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT

        if (vkResetCommandPool(m_device, m_command_pool, flags) != VK_SUCCESS) {
            UH_ERROR("Failed to reset command pool");
            throw std::runtime_error("Failed to reset command pool");
        }

        // Notify all allocated command buffers that they've been reset
        for (auto& weak_buffer : m_allocated_buffers) {
            if (auto buffer = weak_buffer.lock()) {
                auto vk_buffer = std::dynamic_pointer_cast<Vk_Command_Buffer>(buffer);
                if (vk_buffer) {
                    vk_buffer->mark_reset_by_pool();
                }
            }
        }

        UH_INFO("Command pool reset");
    }

    void Vk_Command_Pool::cleanup()
    {
        if (m_command_pool != VK_NULL_HANDLE) {
            // Note: When destroying a command pool, all command buffers allocated from it
            // are automatically freed, so we don't need to free them individually

            vkDestroyCommandPool(m_device, m_command_pool, nullptr);
            m_command_pool = VK_NULL_HANDLE;
            UH_INFO("Command pool destroyed");
        }

        m_allocated_buffers.clear();
    }

} // namespace mango::graphics::vk
