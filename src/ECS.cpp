#include "ECS.h"

namespace whal::ecs {

World::World()
    : mEntityManager(Corrade::Containers::pointer<EntityManager>()), mComponentManager(Corrade::Containers::pointer<ComponentManager>()),
      mSystemManager(Corrade::Containers::pointer<SystemManager>()) {}

Expected<Entity> World::entity(bool isAlive) const {
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

Expected<Entity> World::copy(Entity prefab, bool isActive) const {
    Expected<Entity> newEntity = entity(false);
    if (!newEntity.isExpected()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity.value());
    mEntityManager->setPattern(newEntity.value(), mEntityManager->getPattern(prefab));

    if (isActive) {
        newEntity.value().activate();
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
    mEntityManager.release();
    mComponentManager.release();
    mToKill.clear();

    mEntityManager = Corrade::Containers::pointer<EntityManager>();
    mComponentManager = Corrade::Containers::pointer<ComponentManager>();
}

}  // namespace whal::ecs
