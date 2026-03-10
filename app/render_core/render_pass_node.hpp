#pragma once

#include <string>
#include <vector>

namespace mango::app
{
    struct Render_Pass_Node
    {
        std::string name;
        std::vector<std::string> reads;
        std::vector<std::string> writes;
    };
}
