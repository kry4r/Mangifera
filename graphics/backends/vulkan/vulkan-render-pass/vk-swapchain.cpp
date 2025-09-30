#include "vk-swapchain.hpp"
#include "vulkan-render-resource/vk-texture.hpp"
#include "vulkan-sync/vk-semaphore.hpp"
#include "log/historiographer.hpp"
#include <stdexcept>
#include <algorithm>

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#elif __linux__
#include <vulkan/vulkan_xcb.h>
#elif __APPLE__
#include <vulkan/vulkan_metal.h>
#endif

namespace mango::graphics::vk
{
    Vk_Swapchain::Vk_Swapchain(VkDevice device, VkPhysicalDevice physical_device,
                               VkInstance instance, const Swapchain_Desc& desc)
        : m_device(device)
        , m_physical_device(physical_device)
        , m_instance(instance)
        , m_desc(desc)
    {
        if (!desc.native_window) {
            throw std::runtime_error("Native window handle is null");
        }

        create_surface();
        create_swapchain();
        create_image_views();
    }

    Vk_Swapchain::~Vk_Swapchain()
    {
        cleanup();
    }

    Vk_Swapchain::Vk_Swapchain(Vk_Swapchain&& other) noexcept
        : m_device(other.m_device)
        , m_physical_device(other.m_physical_device)
        , m_instance(other.m_instance)
        , m_surface(other.m_surface)
        , m_swapchain(other.m_swapchain)
        , m_desc(std::move(other.m_desc))
        , m_vk_images(std::move(other.m_vk_images))
        , m_image_views(std::move(other.m_image_views))
        , m_images(std::move(other.m_images))
        , m_image_format(other.m_image_format)
        , m_extent(other.m_extent)
    {
        other.m_surface = VK_NULL_HANDLE;
        other.m_swapchain = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_physical_device = VK_NULL_HANDLE;
        other.m_instance = VK_NULL_HANDLE;
    }

    auto Vk_Swapchain::operator=(Vk_Swapchain&& other) noexcept -> Vk_Swapchain&
    {
        if (this != &other) {
            cleanup();

            m_device = other.m_device;
            m_physical_device = other.m_physical_device;
            m_instance = other.m_instance;
            m_surface = other.m_surface;
            m_swapchain = other.m_swapchain;
            m_desc = std::move(other.m_desc);
            m_vk_images = std::move(other.m_vk_images);
            m_image_views = std::move(other.m_image_views);
            m_images = std::move(other.m_images);
            m_image_format = other.m_image_format;
            m_extent = other.m_extent;

            other.m_surface = VK_NULL_HANDLE;
            other.m_swapchain = VK_NULL_HANDLE;
            other.m_device = VK_NULL_HANDLE;
            other.m_physical_device = VK_NULL_HANDLE;
            other.m_instance = VK_NULL_HANDLE;
        }
        return *this;
    }

    void Vk_Swapchain::create_surface()
    {
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        create_info.hwnd = static_cast<HWND>(m_desc.native_window);
        create_info.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(m_instance, &create_info, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Win32 surface");
        }
#elif __linux__
        // If Xcb
        // VkXcbSurfaceCreateInfoKHR create_info{};
        // create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        // create_info.connection = connection;
        // create_info.window = static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(m_desc.native_window));

        // if (vkCreateXcbSurfaceKHR(m_instance, &create_info, nullptr, &m_surface) != VK_SUCCESS) {
        //     throw std::runtime_error("Failed to create XCB surface");
        // }

        throw std::runtime_error("Linux surface creation not fully implemented - need XCB/Xlib connection");
#elif __APPLE__
        VkMetalSurfaceCreateInfoEXT create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        create_info.pLayer = static_cast<CAMetalLayer*>(m_desc.native_window);

        if (vkCreateMetalSurfaceEXT(m_instance, &create_info, nullptr, &m_surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Metal surface");
        }
#else
        throw std::runtime_error("Unsupported platform for surface creation");
#endif

        UH_INFO("Vulkan surface created");
    }

    void Vk_Swapchain::create_swapchain()
    {
        SwapchainSupportDetails support = query_swapchain_support();

        if (support.formats.empty() || support.present_modes.empty()) {
            throw std::runtime_error("Swapchain support is inadequate");
        }

        VkSurfaceFormatKHR surface_format = choose_swap_surface_format(support.formats);
        VkPresentModeKHR present_mode = choose_swap_present_mode(support.present_modes);
        VkExtent2D extent = choose_swap_extent(support.capabilities);

        uint32_t image_count = m_desc.image_count;
        if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
            image_count = support.capabilities.maxImageCount;
        }
        if (image_count < support.capabilities.minImageCount) {
            image_count = support.capabilities.minImageCount;
        }

        VkSwapchainCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
        create_info.preTransform = support.capabilities.currentTransform;
        create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE;
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan swapchain");
        }

        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
        m_vk_images.resize(image_count);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, m_vk_images.data());

        m_image_format = surface_format.format;
        m_extent = extent;

        m_desc.width = extent.width;
        m_desc.height = extent.height;
        m_desc.image_count = image_count;
        m_desc.format = static_cast<uint32_t>(surface_format.format);

        UH_INFO_FMT("Vulkan swapchain created ({}x{}, {} images, format: {})",
            extent.width, extent.height, image_count, surface_format.format);
    }

    void Vk_Swapchain::create_image_views()
    {
        m_image_views.resize(m_vk_images.size());
        m_images.resize(m_vk_images.size());

        for (size_t i = 0; i < m_vk_images.size(); i++) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = m_vk_images[i];
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = m_image_format;
            view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device, &view_info, nullptr, &m_image_views[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain image view");
            }

            m_images[i] = nullptr;
        }

        UH_INFO_FMT("Created {} swapchain image views", m_image_views.size());
    }

    int32_t Vk_Swapchain::acquire_next_image(std::shared_ptr<Semaphore> wait_semaphore)
    {
        VkSemaphore vk_semaphore = VK_NULL_HANDLE;

        if (wait_semaphore) {
            auto vk_sem = std::dynamic_pointer_cast<Vk_Semaphore>(wait_semaphore);
            if (!vk_sem) {
                throw std::runtime_error("Invalid semaphore type for swapchain");
            }

            if (vk_sem->get_type() != Semaphore_Type::binary) {
                throw std::runtime_error("Swapchain acquire_next_image requires binary semaphore");
            }

            vk_semaphore = vk_sem->get_vk_semaphore();
        }

        uint32_t image_index = 0;
        VkResult result = vkAcquireNextImageKHR(
            m_device,
            m_swapchain,
            UINT64_MAX,
            vk_semaphore,      // Signal this semaphore when image is ready
            VK_NULL_HANDLE,    // Or use a fence
            &image_index
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            UH_WARN("Swapchain out of date during acquire");
            return -1;
        } else if (result == VK_SUBOPTIMAL_KHR) {
            UH_WARN("Swapchain suboptimal during acquire");
            // Still return the image, but may want to recreate swapchain later
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        return static_cast<int32_t>(image_index);
    }

    void Vk_Swapchain::present(uint32_t image_index,
                               const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores)
    {
        throw std::runtime_error("Use Command_Queue::present() instead of calling Swapchain::present() directly");
    }

    Vk_Swapchain::SwapchainSupportDetails Vk_Swapchain::query_swapchain_support()
    {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &details.capabilities);

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, nullptr);
        if (format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, nullptr);
        if (present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR Vk_Swapchain::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
    {
        for (const auto& format : available_formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        for (const auto& format : available_formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        return available_formats[0];
    }

    VkPresentModeKHR Vk_Swapchain::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_modes)
    {
        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Vk_Swapchain::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }

        VkExtent2D actual_extent = {m_desc.width, m_desc.height};

        actual_extent.width = std::clamp(actual_extent.width,
                                        capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height,
                                         capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actual_extent;
    }

    void Vk_Swapchain::cleanup()
    {
        for (auto image_view : m_image_views) {
            if (image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, image_view, nullptr);
            }
        }
        m_image_views.clear();
        m_images.clear();

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
            UH_INFO("Vulkan swapchain destroyed");
        }

        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            UH_INFO("Vulkan surface destroyed");
        }
    }

} // namespace mango::graphics::vk
