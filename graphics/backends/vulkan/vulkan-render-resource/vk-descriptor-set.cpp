#include "vk-descriptor-set.hpp"
#include "vk-buffer.hpp"
#include "vk-texture.hpp"
#include "vk-sampler.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    // ==================== Vk_Descriptor_Set_Layout ====================

    Vk_Descriptor_Set_Layout::Vk_Descriptor_Set_Layout(VkDevice device,
                                                       const Descriptor_Set_Layout_Desc& desc)
        : m_device(device)
        , m_desc(desc)
    {
        create_layout();
    }

    Vk_Descriptor_Set_Layout::~Vk_Descriptor_Set_Layout()
    {
        cleanup();
    }

    Vk_Descriptor_Set_Layout::Vk_Descriptor_Set_Layout(Vk_Descriptor_Set_Layout&& other) noexcept
        : m_device(other.m_device)
        , m_layout(other.m_layout)
        , m_desc(std::move(other.m_desc))
    {
        other.m_layout = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    auto Vk_Descriptor_Set_Layout::operator=(Vk_Descriptor_Set_Layout&& other) noexcept
        -> Vk_Descriptor_Set_Layout&
    {
        if (this != &other) {
            cleanup();
            m_device = other.m_device;
            m_layout = other.m_layout;
            m_desc = std::move(other.m_desc);
            other.m_layout = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Descriptor_Set_Layout::create_layout()
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(m_desc.bindings.size());

        for (const auto& binding : m_desc.bindings) {
            VkDescriptorSetLayoutBinding vk_binding{};
            vk_binding.binding = binding.binding;
            vk_binding.descriptorType = to_vk_descriptor_type(binding.type);
            vk_binding.descriptorCount = binding.count;
            vk_binding.stageFlags = to_vk_shader_stages(binding.shader_stages);
            vk_binding.pImmutableSamplers = nullptr;
            bindings.push_back(vk_binding);
        }

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &m_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout");
        }

        UH_INFO_FMT("Descriptor set layout created with {} bindings", bindings.size());
    }

    void Vk_Descriptor_Set_Layout::cleanup()
    {
        if (m_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
        }
    }

    VkDescriptorType Vk_Descriptor_Set_Layout::to_vk_descriptor_type(Descriptor_Type type) const
    {
        switch (type) {
            case Descriptor_Type::uniform_buffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case Descriptor_Type::storage_buffer:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case Descriptor_Type::sampled_texture:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case Descriptor_Type::storage_texture:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case Descriptor_Type::sampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case Descriptor_Type::combined_image_sampler:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            default:
                throw std::runtime_error("Unknown descriptor type");
        }
    }

    VkShaderStageFlags Vk_Descriptor_Set_Layout::to_vk_shader_stages(uint32_t stages) const
    {
        // 假设 stages 是按位的 shader stage flags
        // 你可能需要根据实际的 shader stage 枚举来转换
        if (stages == 0) {
            return VK_SHADER_STAGE_ALL; // 默认所有阶段
        }
        return static_cast<VkShaderStageFlags>(stages);
    }

    // ==================== Vk_Descriptor_Pool ====================

    Vk_Descriptor_Pool::Vk_Descriptor_Pool(VkDevice device, uint32_t max_sets,
                                           const std::vector<Pool_Size>& pool_sizes)
        : m_device(device)
    {
        create_pool(max_sets, pool_sizes);
    }

    Vk_Descriptor_Pool::~Vk_Descriptor_Pool()
    {
        cleanup();
    }

    void Vk_Descriptor_Pool::create_pool(uint32_t max_sets,
                                         const std::vector<Pool_Size>& pool_sizes)
    {
        std::vector<VkDescriptorPoolSize> vk_pool_sizes;
        vk_pool_sizes.reserve(pool_sizes.size());

        for (const auto& size : pool_sizes) {
            VkDescriptorPoolSize vk_size{};
            vk_size.type = size.type;
            vk_size.descriptorCount = size.count;
            vk_pool_sizes.push_back(vk_size);
        }

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.poolSizeCount = static_cast<uint32_t>(vk_pool_sizes.size());
        pool_info.pPoolSizes = vk_pool_sizes.data();
        pool_info.maxSets = max_sets;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        if (vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }

        UH_INFO_FMT("Descriptor pool created (max sets: {})", max_sets);
    }

    void Vk_Descriptor_Pool::reset()
    {
        vkResetDescriptorPool(m_device, m_pool, 0);
    }

    void Vk_Descriptor_Pool::cleanup()
    {
        if (m_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device, m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
    }

    // ==================== Vk_Descriptor_Set ====================

    Vk_Descriptor_Set::Vk_Descriptor_Set(VkDevice device,
                                         VkDescriptorPool pool,
                                         std::shared_ptr<Vk_Descriptor_Set_Layout> layout)
        : m_device(device)
        , m_pool(pool)
        , m_layout(layout)
    {
        allocate();
    }

    Vk_Descriptor_Set::~Vk_Descriptor_Set()
    {
        if (m_set != VK_NULL_HANDLE && m_pool != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(m_device, m_pool, 1, &m_set);
            m_set = VK_NULL_HANDLE;
        }
    }

    void Vk_Descriptor_Set::allocate()
    {
        VkDescriptorSetLayout layout = m_layout->get_vk_layout();

        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = m_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(m_device, &alloc_info, &m_set) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }

        UH_INFO("Descriptor set allocated");
    }

    void Vk_Descriptor_Set::update(const std::vector<Descriptor_Write>& writes)
    {
        std::vector<VkWriteDescriptorSet> vk_writes;
        std::vector<std::vector<VkDescriptorBufferInfo>> buffer_infos_storage;
        std::vector<std::vector<VkDescriptorImageInfo>> image_infos_storage;

        vk_writes.reserve(writes.size());
        buffer_infos_storage.reserve(writes.size());
        image_infos_storage.reserve(writes.size());

        for (const auto& write : writes) {
            VkWriteDescriptorSet vk_write{};
            vk_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vk_write.dstSet = m_set;
            vk_write.dstBinding = write.binding;
            vk_write.dstArrayElement = write.array_element;
            vk_write.descriptorType = to_vk_descriptor_type(write.type);

            // 处理 Buffer
            if (!write.buffers.empty()) {
                std::vector<VkDescriptorBufferInfo> buffer_infos;
                for (size_t i = 0; i < write.buffers.size(); ++i) {
                    auto vk_buffer = std::dynamic_pointer_cast<Vk_Buffer>(write.buffers[i]);
                    if (!vk_buffer) {
                        throw std::runtime_error("Invalid buffer type in descriptor write");
                    }

                    VkDescriptorBufferInfo buffer_info{};
                    buffer_info.buffer = vk_buffer->get_vk_buffer();
                    buffer_info.offset = i < write.buffer_offsets.size() ?
                                        write.buffer_offsets[i] : 0;
                    buffer_info.range = i < write.buffer_ranges.size() ?
                                       write.buffer_ranges[i] : VK_WHOLE_SIZE;
                    buffer_infos.push_back(buffer_info);
                }

                buffer_infos_storage.push_back(std::move(buffer_infos));
                vk_write.descriptorCount = static_cast<uint32_t>(buffer_infos_storage.back().size());
                vk_write.pBufferInfo = buffer_infos_storage.back().data();
            }
            // 处理 Texture + Sampler (combined_image_sampler)
            else if (write.type == Descriptor_Type::combined_image_sampler) {
                std::vector<VkDescriptorImageInfo> image_infos;
                for (size_t i = 0; i < write.textures.size(); ++i) {
                    auto vk_texture = std::dynamic_pointer_cast<Vk_Texture>(write.textures[i]);
                    auto vk_sampler = i < write.samplers.size() ?
                                     std::dynamic_pointer_cast<Vk_Sampler>(write.samplers[i]) : nullptr;

                    if (!vk_texture) {
                        throw std::runtime_error("Invalid texture type in descriptor write");
                    }

                    VkDescriptorImageInfo image_info{};
                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    image_info.imageView = vk_texture->get_vk_image_view();
                    image_info.sampler = vk_sampler ? vk_sampler->get_vk_sampler() : VK_NULL_HANDLE;
                    image_infos.push_back(image_info);
                }

                image_infos_storage.push_back(std::move(image_infos));
                vk_write.descriptorCount = static_cast<uint32_t>(image_infos_storage.back().size());
                vk_write.pImageInfo = image_infos_storage.back().data();
            }
            // 处理单独的 Texture
            else if (!write.textures.empty()) {
                std::vector<VkDescriptorImageInfo> image_infos;
                for (const auto& texture : write.textures) {
                    auto vk_texture = std::dynamic_pointer_cast<Vk_Texture>(texture);
                    if (!vk_texture) {
                        throw std::runtime_error("Invalid texture type in descriptor write");
                    }

                    VkDescriptorImageInfo image_info{};
                    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    image_info.imageView = vk_texture->get_vk_image_view();
                    image_info.sampler = VK_NULL_HANDLE;
                    image_infos.push_back(image_info);
                }

                image_infos_storage.push_back(std::move(image_infos));
                vk_write.descriptorCount = static_cast<uint32_t>(image_infos_storage.back().size());
                vk_write.pImageInfo = image_infos_storage.back().data();
            }
            // 处理单独的 Sampler
            else if (!write.samplers.empty()) {
                std::vector<VkDescriptorImageInfo> image_infos;
                for (const auto& sampler : write.samplers) {
                    auto vk_sampler = std::dynamic_pointer_cast<Vk_Sampler>(sampler);
                    if (!vk_sampler) {
                        throw std::runtime_error("Invalid sampler type in descriptor write");
                    }

                    VkDescriptorImageInfo image_info{};
                    image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    image_info.imageView = VK_NULL_HANDLE;
                    image_info.sampler = vk_sampler->get_vk_sampler();
                    image_infos.push_back(image_info);
                }

                image_infos_storage.push_back(std::move(image_infos));
                vk_write.descriptorCount = static_cast<uint32_t>(image_infos_storage.back().size());
                vk_write.pImageInfo = image_infos_storage.back().data();
            }

            vk_writes.push_back(vk_write);
        }

        vkUpdateDescriptorSets(m_device,
                              static_cast<uint32_t>(vk_writes.size()),
                              vk_writes.data(),
                              0, nullptr);

        UH_INFO_FMT("Updated {} descriptor bindings", vk_writes.size());
    }

    VkDescriptorType Vk_Descriptor_Set::to_vk_descriptor_type(Descriptor_Type type) const
    {
        switch (type) {
            case Descriptor_Type::uniform_buffer:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case Descriptor_Type::storage_buffer:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case Descriptor_Type::sampled_texture:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case Descriptor_Type::storage_texture:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case Descriptor_Type::sampler:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case Descriptor_Type::combined_image_sampler:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            default:
                throw std::runtime_error("Unknown descriptor type");
        }
    }

} // namespace mango::graphics::vk
