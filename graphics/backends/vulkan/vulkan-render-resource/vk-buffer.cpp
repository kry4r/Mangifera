#include "vk-buffer.hpp"
#include "vulkan-command-execution/vk-command-buffer.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <cstring>

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

    Vk_Buffer::Vk_Buffer(Vk_Buffer&& other) noexcept
        : m_device(other.m_device)
        , m_physical_device(other.m_physical_device)
        , m_buffer(other.m_buffer)
        , m_memory(other.m_memory)
        , m_desc(std::move(other.m_desc))
        , m_mapped_data(other.m_mapped_data)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_physical_device = VK_NULL_HANDLE;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
        other.m_mapped_data = nullptr;
    }

    auto Vk_Buffer::operator=(Vk_Buffer&& other) noexcept -> Vk_Buffer&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_physical_device = other.m_physical_device;
            m_buffer = other.m_buffer;
            m_memory = other.m_memory;
            m_desc = std::move(other.m_desc);
            m_mapped_data = other.m_mapped_data;

            other.m_device = VK_NULL_HANDLE;
            other.m_physical_device = VK_NULL_HANDLE;
            other.m_buffer = VK_NULL_HANDLE;
            other.m_memory = VK_NULL_HANDLE;
            other.m_mapped_data = nullptr;
        }
        return *this;
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

        // GPU-only buffers need transfer destination capability
        if (m_desc.memory == Memory_Type::gpu_only) {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        // CPU-visible buffers might also need transfer for readback
        else if (m_desc.memory == Memory_Type::gpu2cpu) {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        // CPU-to-GPU buffers might be used as transfer source
        else if (m_desc.memory == Memory_Type::cpu2gpu) {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
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
            case Memory_Type::cpu_only:
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
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

    // CPU-visible buffer upload (direct memory copy)
    void Vk_Buffer::upload(const void* data, std::size_t size, std::size_t offset)
    {
        if (m_desc.memory == Memory_Type::gpu_only) {
            throw std::runtime_error("GPU-only buffer requires command buffer for upload. Use upload(cmd, data, size, offset) instead.");
        }

        void* mapped = map();
        std::memcpy(static_cast<char*>(mapped) + offset, data, size);
        flush(offset, size);

        // Don't unmap if it was already mapped
        if (m_mapped_data == nullptr) {
            unmap();
        }
    }

    // GPU-only buffer upload via staging buffer
    void Vk_Buffer::upload(std::shared_ptr<Command_Buffer> cmd, const void* data, std::size_t size, std::size_t offset)
    {
        if (!cmd) {
            throw std::runtime_error("Command buffer is null");
        }

        // Create staging buffer
        Buffer_Desc staging_desc{};
        staging_desc.size = size;
        staging_desc.usage = Buffer_Type::storage; // Just need a generic buffer
        staging_desc.memory = Memory_Type::cpu2gpu;

        Vk_Buffer staging_buffer(m_device, m_physical_device, staging_desc);

        // Copy data to staging buffer
        void* mapped = staging_buffer.map();
        std::memcpy(mapped, data, size);
        staging_buffer.flush();
        staging_buffer.unmap();

        // Copy from staging to this buffer via command buffer
        cmd->copy_buffer(
            std::make_shared<Vk_Buffer>(std::move(staging_buffer)),
            std::shared_ptr<Buffer>(this, [](Buffer*){}), // Non-owning shared_ptr
            0,      // src offset
            offset, // dst offset
            size
        );
    }

    // CPU-visible buffer download (direct memory copy)
    void Vk_Buffer::download(void* data, std::size_t size, std::size_t offset)
    {
        if (m_desc.memory == Memory_Type::gpu_only) {
            throw std::runtime_error("GPU-only buffer requires command buffer for download. Use download(cmd, data, size, offset) instead.");
        }

        void* mapped = map();
        invalidate(offset, size); // Ensure we read fresh data from GPU
        std::memcpy(data, static_cast<char*>(mapped) + offset, size);

        // Don't unmap if it was already mapped
        if (m_mapped_data == nullptr) {
            unmap();
        }
    }

    // GPU-only buffer download via staging buffer
    void Vk_Buffer::download(std::shared_ptr<Command_Buffer> cmd, void* data, std::size_t size, std::size_t offset)
    {
        if (!cmd) {
            throw std::runtime_error("Command buffer is null");
        }

        // Create staging buffer
        Buffer_Desc staging_desc{};
        staging_desc.size = size;
        staging_desc.usage = Buffer_Type::storage;
        staging_desc.memory = Memory_Type::gpu2cpu;

        Vk_Buffer staging_buffer(m_device, m_physical_device, staging_desc);

        // Copy from this buffer to staging via command buffer
        cmd->copy_buffer(
            std::shared_ptr<Buffer>(this, [](Buffer*){}), // Non-owning shared_ptr
            std::make_shared<Vk_Buffer>(std::move(staging_buffer)),
            offset, // src offset
            0,      // dst offset
            size
        );

        // NOTE: User must ensure command buffer is submitted and finished
        // before reading from staging buffer. This is typically done with a fence.

        // Read from staging buffer
        void* mapped = staging_buffer.map();
        staging_buffer.invalidate();
        std::memcpy(data, mapped, size);
        staging_buffer.unmap();
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
        // Only needed for non-coherent memory
        if (m_desc.memory == Memory_Type::cpu2gpu || m_desc.memory == Memory_Type::cpu_only) {
            VkMappedMemoryRange range{};
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = m_memory;
            range.offset = offset;
            range.size = (size == VK_WHOLE_SIZE) ? m_desc.size : size;
            vkFlushMappedMemoryRanges(m_device, 1, &range);
        }
    }

    void Vk_Buffer::invalidate(std::size_t offset, std::size_t size)
    {
        // Only needed for cached memory (gpu2cpu)
        if (m_desc.memory == Memory_Type::gpu2cpu) {
            VkMappedMemoryRange range{};
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = m_memory;
            range.offset = offset;
            range.size = (size == VK_WHOLE_SIZE) ? m_desc.size : size;
            vkInvalidateMappedMemoryRanges(m_device, 1, &range);
        }
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
