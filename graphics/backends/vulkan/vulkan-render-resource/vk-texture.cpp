// vk_texture.cpp
#include "vk-texture.hpp"
#include <stdexcept>
#include <cstring>

namespace mango::graphics::vk
{
    Vk_Texture::Vk_Texture(VkDevice device, VkPhysicalDevice physical_device,
        const Texture_Desc& desc)
        : m_device(device)
        , m_physical_device(physical_device)
        , m_desc(desc)
    {
        m_vk_format = to_vk_format(desc.format);

        create_image();
        allocate_memory();

        vkBindImageMemory(m_device, m_image, m_memory, 0);

        create_image_view();
    }

    Vk_Texture::~Vk_Texture()
    {
        cleanup();
    }

    Vk_Texture::Vk_Texture(Vk_Texture&& other) noexcept
        : m_device(other.m_device)
        , m_physical_device(other.m_physical_device)
        , m_image(other.m_image)
        , m_image_view(other.m_image_view)
        , m_memory(other.m_memory)
        , m_vk_format(other.m_vk_format)
        , m_current_layout(other.m_current_layout)
        , m_desc(other.m_desc)
    {
        other.m_image = VK_NULL_HANDLE;
        other.m_image_view = VK_NULL_HANDLE;
        other.m_memory = VK_NULL_HANDLE;
    }

    auto Vk_Texture::operator=(Vk_Texture&& other) noexcept -> Vk_Texture&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_physical_device = other.m_physical_device;
            m_image = other.m_image;
            m_image_view = other.m_image_view;
            m_memory = other.m_memory;
            m_vk_format = other.m_vk_format;
            m_current_layout = other.m_current_layout;
            m_desc = other.m_desc;

            other.m_image = VK_NULL_HANDLE;
            other.m_image_view = VK_NULL_HANDLE;
            other.m_memory = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Texture::create_image()
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = to_vk_image_type();
        image_info.format = m_vk_format;
        image_info.extent.width = m_desc.width;
        image_info.extent.height = m_desc.height;
        image_info.extent.depth = m_desc.depth;
        image_info.mipLevels = m_desc.mip_levels;
        image_info.arrayLayers = m_desc.arrayLayers;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = get_image_usage_flags();
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // Cube need special tag
        if (m_desc.dimension == Texture_Kind::tex_cube) {
            image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        if (vkCreateImage(m_device, &image_info, nullptr, &m_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan image");
        }
    }

    void Vk_Texture::allocate_memory()
    {
        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(m_device, m_image, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory");
        }
    }

    void Vk_Texture::create_image_view()
    {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_image;
        view_info.viewType = to_vk_image_view_type();
        view_info.format = m_vk_format;

        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        view_info.subresourceRange.aspectMask = get_image_aspect_flags();
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = m_desc.mip_levels;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = m_desc.arrayLayers;

        if (vkCreateImageView(m_device, &view_info, nullptr, &m_image_view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }

    auto Vk_Texture::to_vk_format(Texture_Format format) const -> VkFormat
    {
        switch (format) {
            // Single channel
            case Texture_Format::r8:      return VK_FORMAT_R8_UNORM;
            case Texture_Format::r16f:    return VK_FORMAT_R16_SFLOAT;
            case Texture_Format::r32f:    return VK_FORMAT_R32_SFLOAT;
            case Texture_Format::r8u:     return VK_FORMAT_R8_UINT;
            case Texture_Format::r16u:    return VK_FORMAT_R16_UINT;
            case Texture_Format::r32u:    return VK_FORMAT_R32_UINT;
            case Texture_Format::r8i:     return VK_FORMAT_R8_SINT;
            case Texture_Format::r16i:    return VK_FORMAT_R16_SINT;
            case Texture_Format::r32i:    return VK_FORMAT_R32_SINT;

                // Two channels
            case Texture_Format::rg8:     return VK_FORMAT_R8G8_UNORM;
            case Texture_Format::rg16f:   return VK_FORMAT_R16G16_SFLOAT;
            case Texture_Format::rg32f:   return VK_FORMAT_R32G32_SFLOAT;
            case Texture_Format::rg8u:    return VK_FORMAT_R8G8_UINT;
            case Texture_Format::rg16u:   return VK_FORMAT_R16G16_UINT;
            case Texture_Format::rg32u:   return VK_FORMAT_R32G32_UINT;
            case Texture_Format::rg8i:    return VK_FORMAT_R8G8_SINT;
            case Texture_Format::rg16i:   return VK_FORMAT_R16G16_SINT;
            case Texture_Format::rg32i:   return VK_FORMAT_R32G32_SINT;

                // Three channels
            case Texture_Format::rgb8:    return VK_FORMAT_R8G8B8_UNORM;
            case Texture_Format::sRGB:    return VK_FORMAT_R8G8B8_SRGB;

                // Four channels
            case Texture_Format::rgba8:         return VK_FORMAT_R8G8B8A8_UNORM;
            case Texture_Format::rgba16f:       return VK_FORMAT_R16G16B16A16_SFLOAT;
            case Texture_Format::rgba32f:       return VK_FORMAT_R32G32B32A32_SFLOAT;
            case Texture_Format::sRGB_alpha8:   return VK_FORMAT_R8G8B8A8_SRGB;
            case Texture_Format::rgb10_alpha2:  return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
            case Texture_Format::rgba8u:        return VK_FORMAT_R8G8B8A8_UINT;
            case Texture_Format::rgba16u:       return VK_FORMAT_R16G16B16A16_UINT;
            case Texture_Format::rgba32u:       return VK_FORMAT_R32G32B32A32_UINT;
            case Texture_Format::rgba8i:        return VK_FORMAT_R8G8B8A8_SINT;
            case Texture_Format::rgba16i:       return VK_FORMAT_R16G16B16A16_SINT;
            case Texture_Format::rgba32i:       return VK_FORMAT_R32G32B32A32_SINT;

                // Depth/Stencil
            case Texture_Format::depth24:           return VK_FORMAT_X8_D24_UNORM_PACK32;
            case Texture_Format::depth32f:          return VK_FORMAT_D32_SFLOAT;
            case Texture_Format::depth24_stencil8:  return VK_FORMAT_D24_UNORM_S8_UINT;
            case Texture_Format::depth32f_stencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;

            default:
                throw std::runtime_error("Unsupported texture format");
        }
    }

    auto Vk_Texture::to_vk_image_type() const -> VkImageType
    {
        switch (m_desc.dimension) {
            case Texture_Kind::tex_2d:
            case Texture_Kind::tex_cube:
            case Texture_Kind::tex_2d_array:
                return VK_IMAGE_TYPE_2D;
            case Texture_Kind::tex_3d:
                return VK_IMAGE_TYPE_3D;
            default:
                throw std::runtime_error("Unsupported texture dimension");
        }
    }

    auto Vk_Texture::to_vk_image_view_type() const -> VkImageViewType
    {
        switch (m_desc.dimension) {
            case Texture_Kind::tex_2d:
                return VK_IMAGE_VIEW_TYPE_2D;
            case Texture_Kind::tex_3d:
                return VK_IMAGE_VIEW_TYPE_3D;
            case Texture_Kind::tex_cube:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case Texture_Kind::tex_2d_array:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            default:
                throw std::runtime_error("Unsupported texture dimension");
        }
    }

    auto Vk_Texture::get_image_usage_flags() const -> VkImageUsageFlags
    {
        VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        if (m_desc.sampled) {
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }

        if (m_desc.render_target) {
            if (is_depth_format()) {
                flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            } else {
                flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }
        }

        if (m_desc.mip_levels > 1) {
            flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        return flags;
    }

    auto Vk_Texture::get_image_aspect_flags() const -> VkImageAspectFlags
    {
        if (is_depth_format() && is_stencil_format()) {
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        } else if (is_depth_format()) {
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        } else if (is_stencil_format()) {
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        } else {
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    auto Vk_Texture::is_depth_format() const -> bool
    {
        switch (m_desc.format) {
            case Texture_Format::depth24:
            case Texture_Format::depth32f:
            case Texture_Format::depth24_stencil8:
            case Texture_Format::depth32f_stencil8:
                return true;
            default:
                return false;
        }
    }

    auto Vk_Texture::is_stencil_format() const -> bool
    {
        switch (m_desc.format) {
            case Texture_Format::depth24_stencil8:
            case Texture_Format::depth32f_stencil8:
                return true;
            default:
                return false;
        }
    }

    auto Vk_Texture::find_memory_type(uint32_t type_filter,
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

    // TODO: Use self define Command Buffer
    void Vk_Texture::upload(VkCommandBuffer cmd, const void* data, std::size_t size,
        uint32_t mip_level, uint32_t array_layer)
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer staging_buffer;
        vkCreateBuffer(m_device, &buffer_info, nullptr, &staging_buffer);

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(m_device, staging_buffer, &mem_requirements);

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        VkDeviceMemory staging_memory;
        vkAllocateMemory(m_device, &alloc_info, nullptr, &staging_memory);
        vkBindBufferMemory(m_device, staging_buffer, staging_memory, 0);

        void* mapped;
        vkMapMemory(m_device, staging_memory, 0, size, 0, &mapped);
        std::memcpy(mapped, data, size);
        vkUnmapMemory(m_device, staging_memory);

        transition_layout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = get_image_aspect_flags();
        region.imageSubresource.mipLevel = mip_level;
        region.imageSubresource.baseArrayLayer = array_layer;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            m_desc.width >> mip_level,
            m_desc.height >> mip_level,
            m_desc.depth
        };

        vkCmdCopyBufferToImage(cmd, staging_buffer, m_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        transition_layout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        vkDestroyBuffer(m_device, staging_buffer, nullptr);
        vkFreeMemory(m_device, staging_memory, nullptr);
    }

    void Vk_Texture::transition_layout(VkCommandBuffer cmd, VkImageLayout new_layout,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = m_current_layout;
        barrier.newLayout = new_layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange.aspectMask = get_image_aspect_flags();
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = m_desc.mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = m_desc.arrayLayers;

        if (m_current_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        } else if (m_current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        }

        vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        m_current_layout = new_layout;
    }

    void Vk_Texture::generate_mipmaps(VkCommandBuffer cmd)
    {
        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(m_physical_device, m_vk_format, &format_properties);

        if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("Texture format does not support linear blitting");
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = m_image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = m_desc.arrayLayers;
        barrier.subresourceRange.levelCount = 1;

        int32_t mip_width = m_desc.width;
        int32_t mip_height = m_desc.height;

        for (uint32_t i = 1; i < m_desc.mip_levels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_width, mip_height, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = m_desc.arrayLayers;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {
                mip_width > 1 ? mip_width / 2 : 1,
                mip_height > 1 ? mip_height / 2 : 1,
                1
            };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = m_desc.arrayLayers;

            vkCmdBlitImage(cmd,
                m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit, VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr, 0, nullptr, 1, &barrier);

            if (mip_width > 1) mip_width /= 2;
            if (mip_height > 1) mip_height /= 2;
        }

        barrier.subresourceRange.baseMipLevel = m_desc.mip_levels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        m_current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    void Vk_Texture::cleanup()
    {
        if (m_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_image_view, nullptr);
            m_image_view = VK_NULL_HANDLE;
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
    }

} // namespace mango::graphics::vk
