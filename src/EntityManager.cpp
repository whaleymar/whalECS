#include "EntityManager.h"

#include <mutex>
#include "ECS.h"

namespace whal::ecs {

EntityManager::EntityManager() : mActiveEntities(MAX_ENTITIES) {
    // entity ID 0 is reserved as a Dummy ID (in case entity creation fails)
    mPatterns[0].resize(MAX_COMPONENTS);
    mTagPatterns[0].resize(MAX_COMPONENTS);
    for (u32 entity = 1; entity < MAX_ENTITIES; entity++) {
        mAvailableIDs.push(entity);
        mPatterns[entity].resize(MAX_COMPONENTS);
        mTagPatterns[entity].resize(MAX_COMPONENTS);
    }
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
    mPatterns[entity.mId].reset();     // invalidate pattern
    mTagPatterns[entity.mId].reset();  // invalidate pattern
    mAvailableIDs.push(entity.id());
    mEntityCount--;
}

void EntityManager::setPattern(Entity entity, const Pattern& pattern) {
    mPatterns[entity.mId] = pattern;
}

const Pattern& EntityManager::getPattern(Entity entity) const {
    return mPatterns[entity.mId];
}

Pattern& EntityManager::getPatternMut(Entity entity) {
    return mPatterns[entity.mId];
}

void EntityManager::setTagPattern(Entity entity, const Pattern& pattern) {
    mTagPatterns[entity.mId] = pattern;
}

const Pattern& EntityManager::getTagPattern(Entity entity) const {
    return mTagPatterns[entity.mId];
}

Pattern& EntityManager::getTagPatternMut(Entity entity) {
    return mTagPatterns[entity.mId];
}

u32 EntityManager::getActiveEntityCount() const {
    return mActiveEntities.count();
}

bool EntityManager::isActive(Entity entity) const {
    return mActiveEntities.test(static_cast<u32>(entity.id()));
}

void EntityManager::setEntityName(Entity entity, const char* name) {
    mEntityNames[entity.id()] = name;
}

void EntityManager::setEntityName(Entity entity, std::string_view name) {
    mEntityNames[entity.id()] = std::string(name);
}

const char* EntityManager::getEntityName(Entity entity) {
    auto it = mEntityNames.find(entity.id());
    if (it != mEntityNames.end()) {
        return it->second.c_str();
    }

    // create a name on the fly
    mEntityNames[entity.id()] = std::string("entity ") + std::to_string(entity.id());
    return mEntityNames[entity.id()].c_str();
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

const std::unordered_set<Entity, EntityHash>& EntityManager::getChildren(Entity e) {
    return parentToChildren[e];
}

}  // namespace whal::ecs
