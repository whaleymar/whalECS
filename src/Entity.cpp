#include "ECS.h"
#include "Expected.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id){};

Expected<Entity> Entity::copy() const {
    return World::getInstance().copy(*this);
}

void Entity::kill() const {
    World::getInstance().kill(*this);
}

void Entity::activate() const {
    World::getInstance().activate(*this);
}

}  // namespace whal::ecs
