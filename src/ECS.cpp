#include "ECS.h"

#include "EntityManager.h"

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
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    Entity e = pEM->createEntity(isActive, mRootEntity);
    if (e.isValid() && mCreateCallback) {
        mCreateCallback(e);
    }
    return e;
}

Entity World::entity(const char* name, bool isActive) const {
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    Entity e = pEM->createEntity(isActive, mRootEntity);
    pEM->setEntityName(e, name);
    if (e.isValid() && mCreateCallback) {
        mCreateCallback(e);
    }
    return e;
}

Entity World::componentEntity(ComponentType type) {
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    Entity e = pEM->createEntity(false, mRootEntity);
    // e.add<internal::Component>(); // can't do this here bc it will infinitely recurse when registering the tag
    return e;
}

// works for inactive entities too, trust me
void World::kill(Entity entity) {
    assert(!entity.has<internal::Component>() &&
           "Killing Component Entities not implemented (maybe one day this will unregister the Component and remove it from all entities idk)");
    mToKill.insert(entity);

    // recursively kill child entities
    for (Entity child : static_cast<EntityManager*>(mEntityManager)->parentToChildren[entity]) {
        kill(child);
    }
}

void World::killEntities() {
    while (!mToKill.empty()) {
        // copy mToKill in case onRemove callbacks kill more entities
        auto tmpToKill = std::move(mToKill);
        mKilledThisFrame.insert(tmpToKill.begin(), tmpToKill.end());
        mToKill.clear();
        for (auto entityToKill : tmpToKill) {
            if (mDeathCallback) {
                mDeathCallback(entityToKill);
            }
            mSystemManager->onEntityDestroyed(entityToKill);  // this goes first so onRemove can fetch components before
                                                              // they're deallocated
            unparent(entityToKill);
            static_cast<EntityManager*>(mEntityManager)->destroyEntity(entityToKill);
            mComponentManager->entityDestroyed(entityToKill);
        }

        // remove any redundant kills
        for (auto entityToKill : tmpToKill) {
            mToKill.erase(entityToKill);
        }
    }
    mKilledThisFrame.clear();
}

Entity World::copy(Entity prefab, bool isActive) const {
    Entity newEntity = entity(false);
    if (!newEntity.isValid()) {
        return newEntity;
    }
    mComponentManager->copyComponents(prefab, newEntity);
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    pEM->setPattern(newEntity, pEM->getPattern(prefab));

    // copy prefab's parent
    pEM->childToParent[newEntity] = pEM->childToParent[prefab];
    pEM->parentToChildren[pEM->childToParent[newEntity]].insert(newEntity);

    if (isActive) {
        newEntity.activate();
    }

    return newEntity;
}

void World::activate(Entity entity) const {
    assert(!entity.has<internal::Component>() && "Cannot activate a Component Entity");
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    if (pEM->activate(entity)) {
        mSystemManager->onEntityPatternChanged(entity, pEM->getPattern(entity), pEM->getTagPattern(entity));
    }

    // recursively activate children
    for (Entity child : pEM->parentToChildren[entity]) {
        activate(child);
    }
}

void World::deactivate(Entity entity) const {
    // remove from systems but keep in entity manager and component manager
    if (static_cast<EntityManager*>(mEntityManager)->deactivate(entity)) {
        mSystemManager->onEntityDestroyed(entity);
    }

    // recursively deactivate children
    for (Entity child : static_cast<EntityManager*>(mEntityManager)->parentToChildren[entity]) {
        deactivate(child);
    }
}

u32 World::getEntityCount() const {
    return static_cast<EntityManager*>(mEntityManager)->getEntityCount();
}

u32 World::getActiveEntityCount() const {
    return static_cast<EntityManager*>(mEntityManager)->getActiveEntityCount();
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
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    Entity oldParent = pEM->childToParent[e];
    pEM->childToParent.erase(e);
    pEM->parentToChildren[oldParent].erase(e);
}

Entity World::lookup(const char* name) const {
    EntityManager* pEM = static_cast<EntityManager*>(mEntityManager);
    return pEM->lookup(name);
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
