#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "render-resource/texture.hpp"

namespace mango::graphics
{
    struct Swapchain_Desc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t image_count = 2;
        uint32_t format = 0; // VkFormat / DXGI_FORMAT
        void* native_window = nullptr; // platform-specific handle
    };

    class Swapchain
    {
    public:
        virtual ~Swapchain() = default;

        // Acquire next image index (returns -1 on failure)
        virtual int32_t acquire_next_image(std::shared_ptr<class Semaphore> wait_semaphore = nullptr) = 0;

        // Present image to the screen
        virtual void present(uint32_t image_index, const std::vector<std::shared_ptr<class Semaphore>>& wait_semaphores = {}) = 0;

        virtual const Swapchain_Desc& get_desc() const = 0;

        virtual const std::vector<std::shared_ptr<Texture>>& get_images() const = 0;
    };

    using Swapchain_Handle = std::shared_ptr<Swapchain>;

} // namespace mango::graphics
