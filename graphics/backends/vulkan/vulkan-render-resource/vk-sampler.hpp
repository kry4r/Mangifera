#pragma once
#include "render-resource/sampler.hpp"
#include <vulkan/vulkan.h>

namespace mango::graphics::vk
{
    class Vk_Sampler : public Sampler
    {
    public:
        Vk_Sampler(VkDevice device, VkPhysicalDevice physical_device, const Sampler_Desc& desc);
        ~Vk_Sampler() override;

        Vk_Sampler(const Vk_Sampler&) = delete;
        Vk_Sampler& operator=(const Vk_Sampler&) = delete;
        Vk_Sampler(Vk_Sampler&& other) noexcept;
        Vk_Sampler& operator=(Vk_Sampler&& other) noexcept;

        // Sampler interface
        auto getDesc() const -> const Sampler_Desc& override { return m_desc; }

        // Vulkan specific
        auto get_vk_sampler() const -> VkSampler { return m_sampler; }

    private:
        void create_sampler();
        void cleanup();

        VkFilter filter_mode_to_vk(Filter_Mode mode) const;
        VkSamplerAddressMode edge_mode_to_vk(Edge_Mode mode) const;

        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        Sampler_Desc m_desc;
    };

} // namespace mango::graphics::vk
