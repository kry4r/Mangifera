#pragma once
#include "command-execution/command-pool.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace mango::graphics::vk
{
    class Vk_Command_Pool : public Command_Pool
    {
    public:
        Vk_Command_Pool(VkDevice device, uint32_t queue_family_index, bool transient = false, bool reset_command_buffer = true);
        ~Vk_Command_Pool() override;

        Vk_Command_Pool(const Vk_Command_Pool&) = delete;
        Vk_Command_Pool& operator=(const Vk_Command_Pool&) = delete;
        Vk_Command_Pool(Vk_Command_Pool&& other) noexcept;
        Vk_Command_Pool& operator=(Vk_Command_Pool&& other) noexcept;

        // Command_Pool interface
        std::shared_ptr<Command_Buffer> allocate_command_buffer(Command_Buffer_Level level = Command_Buffer_Level::primary) override;
        void free_command_buffer(std::shared_ptr<Command_Buffer> cmd_buffer) override;
        void reset() override;

        // Vulkan specific
        auto get_vk_command_pool() const -> VkCommandPool { return m_command_pool; }
        auto get_device() const -> VkDevice { return m_device; }
        auto get_queue_family_index() const -> uint32_t { return m_queue_family_index; }

    private:
        void create_command_pool(bool transient, bool reset_command_buffer);
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        uint32_t m_queue_family_index = 0;

        // Track allocated command buffers (for proper cleanup)
        std::vector<std::weak_ptr<Command_Buffer>> m_allocated_buffers;
    };

} // namespace mango::graphics::vk
