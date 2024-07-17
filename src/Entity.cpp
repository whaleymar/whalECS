#include "ECS.h"
#include "Expected.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id){};

Expected<Entity> Entity::copy(bool isActive) const {
    return World::getInstance().copy(*this, isActive);
}

void Entity::kill() const {
    World::getInstance().kill(*this);
}

void Entity::activate() const {
    World::getInstance().activate(*this);
}

void Entity::deactivate() const {
    World::getInstance().deactivate(*this);
}

}  // namespace whal::ecs
