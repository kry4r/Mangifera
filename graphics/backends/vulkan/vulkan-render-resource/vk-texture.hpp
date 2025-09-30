#pragma once
#include "render-resource/texture.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace mango::graphics {
    class Command_Buffer;
}

namespace mango::graphics::vk {

class Vk_Texture : public Texture {
public:
    Vk_Texture(VkDevice device, VkPhysicalDevice physical_device,
               const Texture_Desc& desc);
    ~Vk_Texture() override;

    Vk_Texture(const Vk_Texture&) = delete;
    Vk_Texture& operator=(const Vk_Texture&) = delete;
    Vk_Texture(Vk_Texture&& other) noexcept;
    Vk_Texture& operator=(Vk_Texture&& other) noexcept;

    auto getDesc() const -> const Texture_Desc& override {
        return m_desc;
    }

    auto get_vk_image() const -> VkImage { return m_image; }
    auto get_vk_image_view() const -> VkImageView { return m_image_view; }
    auto get_device_memory() const -> VkDeviceMemory { return m_memory; }
    auto get_vk_format() const -> VkFormat { return m_vk_format; }
    auto get_current_layout() const -> VkImageLayout { return m_current_layout; }

    void upload(std::shared_ptr<Command_Buffer> cmd, const void* data, std::size_t size,
                uint32_t mip_level = 0, uint32_t array_layer = 0);

    void transition_layout(std::shared_ptr<Command_Buffer> cmd, VkImageLayout new_layout,
                          VkPipelineStageFlags src_stage,
                          VkPipelineStageFlags dst_stage);

    void generate_mipmaps(std::shared_ptr<Command_Buffer> cmd);

    std::size_t get_data_size(uint32_t mip_level = 0) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_image_view = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkFormat m_vk_format = VK_FORMAT_UNDEFINED;
    VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    Texture_Desc m_desc;

    auto to_vk_format(Texture_Format format) const -> VkFormat;
    auto to_vk_image_type() const -> VkImageType;
    auto to_vk_image_view_type() const -> VkImageViewType;
    auto get_image_usage_flags() const -> VkImageUsageFlags;
    auto get_image_aspect_flags() const -> VkImageAspectFlags;
    auto is_depth_format() const -> bool;
    auto is_stencil_format() const -> bool;
    auto get_format_size() const -> uint32_t; // 每像素字节数

    auto find_memory_type(uint32_t type_filter,
                         VkMemoryPropertyFlags properties) const -> uint32_t;

    void create_image();
    void allocate_memory();
    void create_image_view();
    void cleanup();
};

} // namespace mango::graphics::vk
