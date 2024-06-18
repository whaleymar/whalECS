#include "ECS.h"

namespace whal::ecs {

World::World() {
    mComponentManager = Corrade::Containers::pointer<ComponentManager>();
    mEntityManager = Corrade::Containers::pointer<EntityManager>();
    mSystemManager = Corrade::Containers::pointer<SystemManager>();
}

Expected<Entity> World::entity(bool isAlive) const {
    return mEntityManager->createEntity(isAlive);
}

void World::kill(Entity entity) {
    mToKill.insert(entity);
}

void World::killEntities() {
    // copy mToKill in case onRemove callbacks kill more entities
    auto tmpToKill = std::move(mToKill);
    mToKill.clear();
    for (auto entity : tmpToKill) {
        if (mDeathCallback != nullptr) {
            mDeathCallback(entity);
        }
        mSystemManager->entityDestroyed(entity);  // this goes first so onRemove can fetch components before
                                                  // they're deallocated
        mEntityManager->destroyEntity(entity);
        mComponentManager->entityDestroyed(entity);
    }
}

Expected<Entity> World::copy(Entity prefab) const {
    Expected<Entity> newEntity = entity(false);
    if (!newEntity.isExpected()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity.value());

    newEntity.value().activate();

    return newEntity;
}

void World::activate(Entity entity) const {
    if (mEntityManager->activate(entity)) {
        auto pattern = mEntityManager->getPattern(entity);
        mSystemManager->entityPatternChanged(entity, pattern);
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

}  // namespace whal::ecs
