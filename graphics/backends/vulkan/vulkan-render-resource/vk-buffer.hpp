#pragma once
#include "render-resource/buffer.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace mango::graphics {
    class Command_Buffer;
}

namespace mango::graphics::vk {

class Vk_Buffer : public Buffer {
public:
    Vk_Buffer(VkDevice device, VkPhysicalDevice physical_device,
              const Buffer_Desc& desc);
    ~Vk_Buffer() override;

    Vk_Buffer(const Vk_Buffer&) = delete;
    Vk_Buffer& operator=(const Vk_Buffer&) = delete;
    Vk_Buffer(Vk_Buffer&& other) noexcept;
    Vk_Buffer& operator=(Vk_Buffer&& other) noexcept;

    auto get_buffer_desc() const -> const Buffer_Desc& override {
        return m_desc;
    }

    auto get_vk_buffer() const -> VkBuffer { return m_buffer; }
    auto get_device_memory() const -> VkDeviceMemory { return m_memory; }

    // CPU-visible buffer: direct upload (no command buffer needed)
    void upload(const void* data, std::size_t size, std::size_t offset = 0);

    // GPU-only buffer: upload via staging buffer (requires command buffer)
    void upload(std::shared_ptr<Command_Buffer> cmd, const void* data, std::size_t size, std::size_t offset = 0);

    // Download from GPU buffer (requires command buffer for GPU-only)
    void download(void* data, std::size_t size, std::size_t offset = 0);
    void download(std::shared_ptr<Command_Buffer> cmd, void* data, std::size_t size, std::size_t offset = 0);

    auto map() -> void*;
    void unmap();
    void flush(std::size_t offset = 0, std::size_t size = VK_WHOLE_SIZE);
    void invalidate(std::size_t offset = 0, std::size_t size = VK_WHOLE_SIZE);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;

    Buffer_Desc m_desc;
    void* m_mapped_data = nullptr;

    auto get_buffer_usage_flags() const -> VkBufferUsageFlags;
    auto get_memory_property_flags() const -> VkMemoryPropertyFlags;
    auto find_memory_type(uint32_t type_filter,
                         VkMemoryPropertyFlags properties) const -> uint32_t;

    void create_buffer();
    void allocate_memory();
    void cleanup();
};

} // namespace mango::graphics::vk
