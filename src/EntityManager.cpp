#include <mutex>
#include "ECS.h"

namespace whal::ecs {

EntityManager::EntityManager() {
    // entity ID 0 is reserved as a Dummy ID (in case entity creation fails)
    for (u32 entity = 1; entity < MAX_ENTITIES; entity++) {
        mAvailableIDs.push(entity);
    }
    mActiveEntities.reset();
}

Expected<Entity> EntityManager::createEntity(bool isAlive) {
    std::unique_lock<std::mutex> lock{mCreatorMutex};
    if (mEntityCount + 1 >= MAX_ENTITIES) {
        return Expected<Entity>::error("Cannot allocate any more entities");
    }
    EntityID id = mAvailableIDs.front();
    mAvailableIDs.pop();
    mEntityCount++;
    if (isAlive) {
        mActiveEntities.set(static_cast<u32>(id));
    }
    return Entity(id);
}

void EntityManager::destroyEntity(Entity entity) {
    mActiveEntities.reset(static_cast<u32>(entity.id()));
    mPatterns[entity.mId].reset();  // invalidate pattern
    mAvailableIDs.push(entity.id());
    mEntityCount--;
}

void EntityManager::setPattern(Entity entity, const Pattern& pattern) {
    mPatterns[entity.mId] = pattern;
}

Pattern EntityManager::getPattern(Entity entity) const {
    return mPatterns[entity.mId];
}

bool EntityManager::isActive(Entity entity) const {
    return mActiveEntities.test(static_cast<u32>(entity.id()));
}

bool EntityManager::activate(Entity entity) {
    if (isActive(entity)) {
        return false;
    }
    mActiveEntities.set(static_cast<u32>(entity.id()));
    return true;
}

bool EntityManager::deactivate(Entity entity) {
    if (!isActive(entity)) {
        return false;
    }
    mActiveEntities.reset(static_cast<u32>(entity.id()));
    return true;
}

}  // namespace whal::ecs
