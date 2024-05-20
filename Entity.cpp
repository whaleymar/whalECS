#include "ECS/Lib/ECS.h"
#include "Util/Expected.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id){};

Expected<Entity> Entity::copy() const {
    return ECS::getInstance().copy(*this);
}

void Entity::kill() const {
    ECS::getInstance().kill(*this);
}

}  // namespace whal::ecs
