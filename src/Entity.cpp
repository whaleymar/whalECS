#include "ECS.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id) {};

Entity Entity::copy(bool isActive) const {
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

bool Entity::isValid() const {
    return mId != 0;
}

void Entity::addChild(ecs::Entity child) const {
    World::getInstance().addChild(*this, child);
}

ecs::Entity Entity::createChild(bool isActive) const {
    return World::getInstance().createChild(*this, isActive);
}

void Entity::orphan() const {
    World::getInstance().orphan(*this);
}

}  // namespace whal::ecs
