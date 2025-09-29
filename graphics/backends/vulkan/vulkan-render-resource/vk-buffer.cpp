#include "vk-buffer.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{

    Vk_Buffer::Vk_Buffer(VkDevice device, VkPhysicalDevice physical_device,
        const Buffer_Desc& desc)
        : m_device(device)
        , m_physical_device(physical_device)
        , m_desc(desc)
    {
        create_buffer();
        allocate_memory();

        vkBindBufferMemory(m_device, m_buffer, m_memory, 0);

        if (m_desc.memory != Memory_Type::gpu_only) {
            map();
        }
    }

    Vk_Buffer::~Vk_Buffer()
    {
        cleanup();
    }

    void Vk_Buffer::create_buffer()
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = m_desc.size;
        buffer_info.usage = get_buffer_usage_flags();
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_buffer) != VK_SUCCESS) {
            UH_ERROR("Failed to create Vulkan buffer");
            throw std::runtime_error("Failed to create Vulkan buffer");
        }
    }

    void Vk_Buffer::allocate_memory()
    {
        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(m_device, m_buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(
            mem_requirements.memoryTypeBits,
            get_memory_property_flags()
        );

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory");
        }
    }

    auto Vk_Buffer::get_buffer_usage_flags() const -> VkBufferUsageFlags
    {
        VkBufferUsageFlags flags = 0;

        switch (m_desc.usage) {
            case Buffer_Type::vertex:
                flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                break;
            case Buffer_Type::index:
                flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                break;
            case Buffer_Type::uniform:
                flags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;
            case Buffer_Type::storage:
                flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;
        }

        if (m_desc.memory == Memory_Type::gpu_only) {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        return flags;
    }

    auto Vk_Buffer::get_memory_property_flags() const -> VkMemoryPropertyFlags
    {
        switch (m_desc.memory) {
            case Memory_Type::gpu_only:
                return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            case Memory_Type::cpu2gpu:
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            case Memory_Type::gpu2cpu:
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            default:
                return 0;
        }
    }

    auto Vk_Buffer::find_memory_type(uint32_t type_filter,
        VkMemoryPropertyFlags properties) const -> uint32_t
    {
        VkPhysicalDeviceMemoryProperties mem_properties;
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_properties);

        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
            if ((type_filter & (1 << i)) &&
                (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

    void Vk_Buffer::upload(const void* data, std::size_t size, std::size_t offset)
    {
        if (m_desc.memory != Memory_Type::gpu_only) {
            void* mapped = map();
            std::memcpy(static_cast<char*>(mapped) + offset, data, size);
            flush(offset, size);
            if (!m_mapped_data) unmap();
        } else {
            // Use staging buffer
            // Create a staging buffer and copy it by command buffer
            // Need command buffer implement
            throw std::runtime_error("GPU-only buffer upload requires staging buffer");
        }
    }

    auto Vk_Buffer::map() -> void*
    {
        if (!m_mapped_data) {
            if (vkMapMemory(m_device, m_memory, 0, m_desc.size, 0, &m_mapped_data) != VK_SUCCESS) {
                throw std::runtime_error("Failed to map buffer memory");
            }
        }
        return m_mapped_data;
    }

    void Vk_Buffer::unmap()
    {
        if (m_mapped_data) {
            vkUnmapMemory(m_device, m_memory);
            m_mapped_data = nullptr;
        }
    }

    void Vk_Buffer::flush(std::size_t offset, std::size_t size)
    {
        VkMappedMemoryRange range{};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = m_memory;
        range.offset = offset;
        range.size = size;
        vkFlushMappedMemoryRanges(m_device, 1, &range);
    }

    void Vk_Buffer::cleanup()
    {
        if (m_mapped_data) {
            unmap();
        }
        if (m_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
    }

} // namespace mango::graphics::vk
