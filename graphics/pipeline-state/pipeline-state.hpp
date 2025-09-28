#pragma once
#include <memory>
#include <vector>
#include <string>

namespace mango::graphics
{

    enum struct Pipeline_Type
    {
        graphics,
        compute,
        raytracing,
    };

    struct Pipeline_State
    {
    public:
        virtual ~Pipeline_State() = default;

        virtual Pipeline_Type get_type() const = 0;
    };

    using Pipeline_State_Handle = std::shared_ptr<Pipeline_State>;

} // namespace mango::graphics
