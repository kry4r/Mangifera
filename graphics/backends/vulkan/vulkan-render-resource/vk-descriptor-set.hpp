#pragma once
#include "render-resource/descriptor-set.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace mango::graphics::vk
{
    // Vulkan Descriptor Set Layout
    struct Vk_Descriptor_Set_Layout : public Descriptor_Set_Layout
    {
    public:
        Vk_Descriptor_Set_Layout(VkDevice device, const Descriptor_Set_Layout_Desc& desc);
        ~Vk_Descriptor_Set_Layout() override;

        Vk_Descriptor_Set_Layout(const Vk_Descriptor_Set_Layout&) = delete;
        Vk_Descriptor_Set_Layout& operator=(const Vk_Descriptor_Set_Layout&) = delete;
        Vk_Descriptor_Set_Layout(Vk_Descriptor_Set_Layout&& other) noexcept;
        Vk_Descriptor_Set_Layout& operator=(Vk_Descriptor_Set_Layout&& other) noexcept;

        const Descriptor_Set_Layout_Desc& get_desc() const override { return m_desc; }

        VkDescriptorSetLayout get_vk_layout() const { return m_layout; }

    private:
        void create_layout();
        void cleanup();

        VkDescriptorType to_vk_descriptor_type(Descriptor_Type type) const;
        VkShaderStageFlags to_vk_shader_stages(uint32_t stages) const;

        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
        Descriptor_Set_Layout_Desc m_desc;
    };

    // Vulkan Descriptor Pool
    struct Vk_Descriptor_Pool
    {
    public:
        struct Pool_Size
        {
            VkDescriptorType type;
            uint32_t count;
        };

        Vk_Descriptor_Pool(VkDevice device, uint32_t max_sets,
                          const std::vector<Pool_Size>& pool_sizes);
        ~Vk_Descriptor_Pool();

        Vk_Descriptor_Pool(const Vk_Descriptor_Pool&) = delete;
        Vk_Descriptor_Pool& operator=(const Vk_Descriptor_Pool&) = delete;

        VkDescriptorPool get_vk_pool() const { return m_pool; }

        void reset();

    private:
        void create_pool(uint32_t max_sets, const std::vector<Pool_Size>& pool_sizes);
        void cleanup();

        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
    };

    // Vulkan Descriptor Set
    struct Vk_Descriptor_Set : public Descriptor_Set
    {
    public:
        Vk_Descriptor_Set(VkDevice device,
                         VkDescriptorPool pool,
                         std::shared_ptr<Vk_Descriptor_Set_Layout> layout);
        ~Vk_Descriptor_Set() override;

        Vk_Descriptor_Set(const Vk_Descriptor_Set&) = delete;
        Vk_Descriptor_Set& operator=(const Vk_Descriptor_Set&) = delete;

        void update(const std::vector<Descriptor_Write>& writes) override;

        VkDescriptorSet get_vk_descriptor_set() const { return m_set; }
        std::shared_ptr<Vk_Descriptor_Set_Layout> get_layout() const { return m_layout; }

    private:
        void allocate();
        VkDescriptorType to_vk_descriptor_type(Descriptor_Type type) const;

        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
        VkDescriptorSet m_set = VK_NULL_HANDLE;
        std::shared_ptr<Vk_Descriptor_Set_Layout> m_layout;
    };

} // namespace mango::graphics::vk
