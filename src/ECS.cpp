#include "ECS.h"

namespace whal::ecs {

World::World() : mEntityManager(new EntityManager), mComponentManager(new ComponentManager), mSystemManager(new SystemManager) {}

World::~World() {
    delete mEntityManager;
    delete mComponentManager;
    delete mSystemManager;
}

Entity World::entity(bool isAlive) const {
    return mEntityManager->createEntity(isAlive);
}

// works for inactive entities too, trust me
void World::kill(Entity entity) {
    mToKill.insert(entity);
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
}

void World::deactivate(Entity entity) const {
    // remove from systems but keep in entity manager and component manager
    if (mEntityManager->deactivate(entity)) {
        mSystemManager->onEntityDestroyed(entity);
    }
}

u32 World::getEntityCount() const {
    return mEntityManager->getEntityCount();
}

void World::setEntityDeathCallback(EntityDeathCallback callback) {
    mDeathCallback = callback;
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
