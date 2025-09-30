#pragma once
#include <memory>
#include <cstdint>
#include "command-buffer.hpp"

namespace mango::graphics
{
    // Command buffer level
    enum struct Command_Buffer_Level
    {
        primary,
        secondary
    };

    // Command pool abstraction
    struct Command_Pool
    {
    public:
        virtual ~Command_Pool() = default;

        // Allocate a command buffer from this pool
        virtual std::shared_ptr<Command_Buffer> allocate_command_buffer(Command_Buffer_Level level = Command_Buffer_Level::primary) = 0;

        // Free a command buffer (optional; some backends prefer reset)
        virtual void free_command_buffer(std::shared_ptr<Command_Buffer> cmdBuffer) = 0;

        // Reset the pool (invalidate all allocated buffers)
        virtual void reset() = 0;
    };

    using Command_Pool_Handle = std::shared_ptr<Command_Pool>;

} // namespace mango::graphics
