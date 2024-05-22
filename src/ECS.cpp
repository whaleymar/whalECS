#include "ECS.h"

namespace whal::ecs {

ECS::ECS() {
    mComponentManager = std::make_unique<ComponentManager>();
    mEntityManager = std::make_unique<EntityManager>();
    mSystemManager = std::make_unique<SystemManager>();
}

Expected<Entity> ECS::entity() const {
    return mEntityManager->createEntity();
}

void ECS::kill(Entity entity) {
    mToKill.push_back(entity);
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
    // this is probably quite slow
    Expected<Entity> newEntity = entity();
    if (!newEntity.isExpected()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity.value());

    auto pattern = mEntityManager->getPattern(prefab);
    mEntityManager->setPattern(newEntity.value(), pattern);
    mSystemManager->entityPatternChanged(newEntity.value(), pattern);

    return newEntity;
}

u32 ECS::getEntityCount() const {
    return mEntityManager->getEntityCount();
}

void ECS::setEntityDeathCallback(EntityDeathCallback callback) {
    mDeathCallback = callback;
}

}  // namespace whal::ecs
