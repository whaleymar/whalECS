#include "ECS/Lib/ECS.h"

namespace whal::ecs {

EntityManager::EntityManager() {
    for (u32 entity = 0; entity < MAX_ENTITIES; entity++) {
        mAvailableIDs.push(entity);
    }
}

Expected<Entity> EntityManager::createEntity() {
    if (mEntityCount + 1 >= MAX_ENTITIES) {
        return Expected<Entity>::error("Cannot allocate any more entities");
    }
    EntityID id = mAvailableIDs.front();
    mAvailableIDs.pop();
    mEntityCount++;
    return Entity(id);
}

void EntityManager::destroyEntity(Entity entity) {
    mPatterns[entity.mId].reset();  // invalidate pattern
    mAvailableIDs.push(entity.id());
    mEntityCount--;
}

void EntityManager::setPattern(Entity entity, Pattern pattern) {
    mPatterns[entity.mId] = pattern;
}

Pattern EntityManager::getPattern(Entity entity) const {
    return mPatterns[entity.mId];
}

}  // namespace whal::ecs
