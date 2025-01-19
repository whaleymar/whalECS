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

Entity EntityManager::createEntity(bool isAlive, Entity parent) {
    std::unique_lock<std::mutex> lock{mCreatorMutex};
    if (mEntityCount + 1 >= MAX_ENTITIES) {
        // TODO logging
        return Entity{0};
    }
    const EntityID id = mAvailableIDs.front();
    mAvailableIDs.pop();
    mEntityCount++;
    if ((parent.id() == 0 || isActive(parent)) && isAlive) {
        mActiveEntities.set(static_cast<u32>(id));
    }

    const Entity self = Entity{id};
    parentToChildren[self].clear();         // no children
    parentToChildren[parent].insert(self);  // add self as child of parent
    childToParent[self] = parent;           // add our parent
    return self;
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

u32 EntityManager::getActiveEntityCount() const {
    return mActiveEntities.count();
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
