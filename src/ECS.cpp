#include "ECS.h"

namespace whal::ecs {

ECS::ECS() {
    mComponentManager = std::make_unique<ComponentManager>();
    mEntityManager = std::make_unique<EntityManager>();
    mSystemManager = std::make_unique<SystemManager>();
}

Expected<Entity> ECS::entity(bool isAlive) const {
    return mEntityManager->createEntity(isAlive);
}

void ECS::kill(Entity entity) {
    mToKill.insert(entity);
}

void ECS::killEntities() {
    for (auto entity : mToKill) {
        if (mDeathCallback != nullptr) {
            mDeathCallback(entity);
        }
        mSystemManager->entityDestroyed(entity);  // this goes first so onRemove can fetch components before
                                                  // they're deallocated
        mEntityManager->destroyEntity(entity);
        mComponentManager->entityDestroyed(entity);
    }
    mToKill.clear();
}

Expected<Entity> ECS::copy(Entity prefab) const {
    Expected<Entity> newEntity = entity(false);
    if (!newEntity.isExpected()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity.value());

    newEntity.value().activate();

    return newEntity;
}

void ECS::activate(Entity entity) const {
    if (mEntityManager->activate(entity)) {
        auto pattern = mEntityManager->getPattern(entity);
        mSystemManager->entityPatternChanged(entity, pattern);
    }
}

u32 ECS::getEntityCount() const {
    return mEntityManager->getEntityCount();
}

void ECS::setEntityDeathCallback(EntityDeathCallback callback) {
    mDeathCallback = callback;
}

bool ECS::isActive(Entity entity) const {
    return mEntityManager->isActive(entity);
}

}  // namespace whal::ecs
