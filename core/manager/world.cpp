#include "world.hpp"

namespace mango::core
{
    World::World()
    {

    }

    World::~World()
    {
        clear_all();
    }

    auto World::is_entity_valid(Entity entity) const -> bool
    {
        return entities.exists(entity);
    }

    auto World::create_entity() -> Entity
    {
        return entities.allocate();
    }

    auto World::destroy_entity(Entity entity) -> bool
    {
        return entities.deallocate(entity);
    }


}
