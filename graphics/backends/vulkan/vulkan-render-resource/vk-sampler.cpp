#include "vk-sampler.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>

namespace mango::graphics::vk
{
    Vk_Sampler::Vk_Sampler(VkDevice device, VkPhysicalDevice physical_device, const Sampler_Desc& desc)
        : m_device(device)
        , m_physical_device(physical_device)
        , m_desc(desc)
    {
        create_sampler();
    }

    Vk_Sampler::~Vk_Sampler()
    {
        cleanup();
    }

    Vk_Sampler::Vk_Sampler(Vk_Sampler&& other) noexcept
        : m_device(other.m_device)
        , m_physical_device(other.m_physical_device)
        , m_sampler(other.m_sampler)
        , m_desc(std::move(other.m_desc))
    {
        other.m_sampler = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_physical_device = VK_NULL_HANDLE;
    }

    auto Vk_Sampler::operator=(Vk_Sampler&& other) noexcept -> Vk_Sampler&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_physical_device = other.m_physical_device;
            m_sampler = other.m_sampler;
            m_desc = std::move(other.m_desc);

            other.m_sampler = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
            other.m_physical_device = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Sampler::create_sampler()
    {
        // Query device properties for max anisotropy
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physical_device, &properties);

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        // Magnification filter
        sampler_info.magFilter = filter_mode_to_vk(m_desc.magFilter);

        // Minification filter
        sampler_info.minFilter = filter_mode_to_vk(m_desc.minFilter);

        // Address modes
        sampler_info.addressModeU = edge_mode_to_vk(m_desc.addressU);
        sampler_info.addressModeV = edge_mode_to_vk(m_desc.addressV);
        sampler_info.addressModeW = edge_mode_to_vk(m_desc.addressW);

        // Anisotropic filtering (enable if using linear filtering)
        if (m_desc.minFilter == Filter_Mode::linear || m_desc.magFilter == Filter_Mode::linear) {
            sampler_info.anisotropyEnable = VK_TRUE;
            sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        } else {
            sampler_info.anisotropyEnable = VK_FALSE;
            sampler_info.maxAnisotropy = 1.0f;
        }

        // Border color (for clamp to border mode)
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

        // Unnormalized coordinates (false = use [0, 1] range)
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        // Comparison (for shadow mapping, disabled by default)
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

        // Mipmap settings
        sampler_info.mipmapMode = (m_desc.minFilter == Filter_Mode::linear)
            ? VK_SAMPLER_MIPMAP_MODE_LINEAR
            : VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE; // No limit

        if (vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan sampler");
        }

        UH_INFO_FMT("Vulkan sampler created (min: {}, mag: {}, anisotropy: {})",
            (m_desc.minFilter == Filter_Mode::linear) ? "linear" : "nearest",
            (m_desc.magFilter == Filter_Mode::linear) ? "linear" : "nearest",
            sampler_info.anisotropyEnable ? "enabled" : "disabled");
    }

    void Vk_Sampler::cleanup()
    {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
            UH_INFO("Vulkan sampler destroyed");
        }
    }

    VkFilter Vk_Sampler::filter_mode_to_vk(Filter_Mode mode) const
    {
        switch (mode) {
            case Filter_Mode::nearest:
                return VK_FILTER_NEAREST;

            case Filter_Mode::linear:
                return VK_FILTER_LINEAR;

            default:
                return VK_FILTER_LINEAR;
        }
    }

    VkSamplerAddressMode Vk_Sampler::edge_mode_to_vk(Edge_Mode mode) const
    {
        switch (mode) {
            case Edge_Mode::repeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;

            case Edge_Mode::clamp:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

            default:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
    }

} // namespace mango::graphics::vk
