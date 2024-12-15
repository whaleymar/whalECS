#include "ECS.h"

namespace whal::ecs {

World::World() : mEntityManager(new EntityManager), mComponentManager(new ComponentManager), mSystemManager(new SystemManager) {}

World::~World() {
    delete mEntityManager;
    delete mComponentManager;
    delete mSystemManager;
}

Entity World::entity(bool isActive) const {
    return mEntityManager->createEntity(isActive, mRootEntity);
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
            if (mDeathCallback != nullptr) {
                mDeathCallback(entityToKill);
            }
            mSystemManager->onEntityDestroyed(entityToKill);  // this goes first so onRemove can fetch components before
                                                              // they're deallocated
            orphan(entityToKill);
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
    // TODO copy parent? Or maybe childof prefab?

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

void World::setEntityDeathCallback(EntityDeathCallback callback) {
    mDeathCallback = callback;
}

void World::addChild(Entity parent, Entity child) {
    mEntityManager->parentToChildren[parent].insert(child);
}

Entity World::createChild(Entity parent, bool isActive) {
    return mEntityManager->createEntity(isActive, parent);
}

void World::orphan(Entity e) {
    Entity oldParent = mEntityManager->childToParent[e];
    if (oldParent == mRootEntity) {
        // cannot orphan top-level parent
        return;
    }

    mEntityManager->childToParent[e] = mRootEntity;

    // there are no guarantees on which order parents/children are deleted if the deletes happen on the same frame.
    // BUT i don't think I touch a parent's list of children when it dies, so this should be fine?
    mEntityManager->parentToChildren[oldParent].erase(e);

    // auto it = mEntityManager->parentToChildren[oldParent].find(e);
    // if (it != mEntityManager->parentToChildren[oldParent].end()) {
    // }
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
