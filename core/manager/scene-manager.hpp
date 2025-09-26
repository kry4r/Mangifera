#pragma once
#include "core/data-struct/freelist.hpp"
#include "core/data-struct/scene-node.hpp"
#include "core/data-struct/singleton.hpp"
#include "grove.hpp"
#include "gardener.hpp"
#include <unordered_map>
#include <memory>
#include <vector>

namespace mango::scene
{
    using namespace mango::core;

    struct Scene_Manager : core::Singleton<Scene_Manager>
    {

    };
}
