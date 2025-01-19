#include "ECS.h"

namespace whal::ecs {

World::World() : mEntityManager(new EntityManager), mComponentManager(new ComponentManager), mSystemManager(new SystemManager) {}

World::~World() {
    delete mEntityManager;
    delete mComponentManager;
    delete mSystemManager;
}

u32 World::getComponentCount() const {
    return mComponentManager->getRegisteredCount();
}

Entity World::entity(bool isActive) const {
    Entity e = mEntityManager->createEntity(isActive, mRootEntity);
    if (e.isValid() && mCreateCallback) {
        mCreateCallback(e);
    }
    return e;
}

// works for inactive entities too, trust me
void World::kill(Entity entity) {
    mToKill.insert(entity);

    // recursively kill child entities
    for (Entity child : mEntityManager->parentToChildren[entity]) {
        kill(child);
    }
}

void World::killEntities() {
    while (!mToKill.empty()) {
        // copy mToKill in case onRemove callbacks kill more entities
        auto tmpToKill = std::move(mToKill);
        mToKill.clear();
        for (auto entityToKill : tmpToKill) {
            if (mDeathCallback) {
                mDeathCallback(entityToKill);
            }
            mSystemManager->onEntityDestroyed(entityToKill);  // this goes first so onRemove can fetch components before
                                                              // they're deallocated
            unparent(entityToKill);
            mEntityManager->destroyEntity(entityToKill);
            mComponentManager->entityDestroyed(entityToKill);
        }

        // remove any redundant kills
        for (auto entityToKill : tmpToKill) {
            mToKill.erase(entityToKill);
        }
    }
}

Entity World::copy(Entity prefab, bool isActive) const {
    Entity newEntity = entity(false);
    if (!newEntity.isValid()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity);
    mEntityManager->setPattern(newEntity, mEntityManager->getPattern(prefab));

    // copy prefab's parent
    mEntityManager->childToParent[newEntity] = mEntityManager->childToParent[prefab];
    mEntityManager->parentToChildren[mEntityManager->childToParent[newEntity]].insert(newEntity);

    if (isActive) {
        newEntity.activate();
    }

    return newEntity;
}

void World::activate(Entity entity) const {
    if (mEntityManager->activate(entity)) {
        auto pattern = mEntityManager->getPattern(entity);
        mSystemManager->onEntityPatternChanged(entity, pattern);
    }

    // recursively activate children
    for (Entity child : mEntityManager->parentToChildren[entity]) {
        activate(child);
    }
}

void World::deactivate(Entity entity) const {
    // remove from systems but keep in entity manager and component manager
    if (mEntityManager->deactivate(entity)) {
        mSystemManager->onEntityDestroyed(entity);
    }

    // recursively deactivate children
    for (Entity child : mEntityManager->parentToChildren[entity]) {
        deactivate(child);
    }
}

u32 World::getEntityCount() const {
    return mEntityManager->getEntityCount();
}

u32 World::getActiveEntityCount() const {
    return mEntityManager->getActiveEntityCount();
}

void World::setEntityDeathCallback(EntityCallback callback) {
    mDeathCallback = callback;
}

void World::setEntityCreateCallback(EntityCallback callback) {
    mCreateCallback = callback;
}

void World::setEntityChildCreateCallback(EntityPairCallback callback) {
    mChildCreateCallback = callback;
}

void World::setEntityAdoptCallback(EntityPairCallback callback) {
    mAdoptCallback = callback;
}

void World::unparent(Entity e) const {
    Entity oldParent = mEntityManager->childToParent[e];
    mEntityManager->childToParent.erase(e);
    mEntityManager->parentToChildren[oldParent].erase(e);
}

bool World::isActive(Entity entity) const {
    return mEntityManager->isActive(entity);
}

void World::clear() {
    mSystemManager->clear();
    delete mEntityManager;
    delete mComponentManager;
    mToKill.clear();

    mEntityManager = new EntityManager;
    mComponentManager = new ComponentManager;
}

}  // namespace whal::ecs
