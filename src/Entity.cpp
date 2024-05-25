#include "ECS.h"
#include "Expected.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id){};

Expected<Entity> Entity::copy() const {
    return ECS::getInstance().copy(*this);
}

void Entity::kill() const {
    ECS::getInstance().kill(*this);
}

void Entity::activate() const {
    ECS::getInstance().activate(*this);
}

}  // namespace whal::ecs
