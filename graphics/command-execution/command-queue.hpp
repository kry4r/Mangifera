#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include "command-buffer.hpp"
#include "sync/fence.hpp"
#include "sync/semaphore.hpp"
#include "render-resource/texture.hpp"
#include "render-pass/swapchain.hpp"

namespace mango::graphics
{
    enum struct Queue_Type
    {
        graphics,
        compute,
        transfer,
        present
    };

    struct Submit_Info
    {
        // Command buffers to submit (primary)
        std::vector<std::shared_ptr<Command_Buffer>> command_buffers;

        // Wait semaphores (pipeline stage masks optional)
        std::vector<std::shared_ptr<Semaphore>> wait_semaphores;
        std::vector<uint32_t> wait_stage_masks; // same size as wait_semaphores

        // Signal semaphores
        std::vector<std::shared_ptr<Semaphore>> signal_semaphores;
    };

    // Command queue abstraction: submit command buffers and present
    struct Command_Queue
    {
    public:
        virtual ~Command_Queue() = default;

        // Submit a set of command buffers with optional fence for CPU-GPU sync
        // If fence is provided, it will be signaled when submission finishes
        virtual void submit(const Submit_Info& info, std::shared_ptr<Fence> fence = nullptr) = 0;

        // Present a swapchain image (simple abstraction)
        // The present waits on semaphores provided by app (e.g. render finished)
        virtual void present(std::shared_ptr<Swapchain> swapchain,
                             uint32_t imageIndex,
                             const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) = 0;

        // Wait idle on this queue
        virtual void wait_idle() = 0;

        // Get queue type
        virtual Queue_Type get_type() const = 0;
    };

    using Command_Queue_Handle = std::shared_ptr<Command_Queue>;

} // namespace mango::graphics
