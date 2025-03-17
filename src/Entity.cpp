#include "ECS.h"

namespace whal::ecs {

Entity::Entity(EntityID id) : mId(id) {};

Entity Entity::copy(bool isActive) const {
    return World::getInstance().copy(*this, isActive);
}

void Entity::kill() const {
    World::getInstance().kill(*this);
}

void Entity::activate() const {
    World::getInstance().activate(*this);
}

void Entity::deactivate() const {
    World::getInstance().deactivate(*this);
}

bool Entity::isValid() const {
    return mId != 0;
}

bool Entity::isKilledThisFrame() const {
    World& world = World::getInstance();
    return world.mToKill.contains(*this) || world.mKilledThisFrame.contains(*this);
}

void Entity::addChild(ecs::Entity child) const {
    const World& world = World::getInstance();
    Entity oldParent = world.mEntityManager->childToParent[child];
    world.mEntityManager->childToParent[child] = *this;
    world.mEntityManager->parentToChildren[oldParent].erase(child);
    world.mEntityManager->parentToChildren[*this].insert(child);
    if (isValid() && child.isValid() && world.mAdoptCallback) {
        world.mAdoptCallback(child, *this);
    }
}

ecs::Entity Entity::createChild(bool isActive) const {
    const World& world = World::getInstance();
    Entity e = world.mEntityManager->createEntity(isActive, *this);
    if (e.isValid() && world.mChildCreateCallback) {
        world.mChildCreateCallback(e, *this);
    }
    return e;
}

void Entity::orphan() const {
    // World::getInstance().orphan(*this);
    const World& world = World::getInstance();
    Entity oldParent = world.mEntityManager->childToParent[*this];
    if (oldParent == world.mRootEntity) {
        // cannot orphan top-level parent
        return;
    }

    world.mEntityManager->childToParent[*this] = world.mRootEntity;

    // there are no guarantees on which order parents/children are deleted if the deletes happen on the same frame.
    // BUT i don't think I touch a parent's list of children when it dies, so this should be fine?
    world.mEntityManager->parentToChildren[oldParent].erase(*this);
    world.mEntityManager->parentToChildren[world.mRootEntity].insert(*this);
}

Entity Entity::parent() const {
    return World::getInstance().mEntityManager->childToParent[*this];
}

const std::unordered_set<Entity, EntityHash>& Entity::children() const {
    return World::getInstance().mEntityManager->parentToChildren[*this];
}

}  // namespace whal::ecs
