#include "ECS.h"
#include "EntityManager.h"

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
    assert(!child.has<internal::Component>() && "Cannot re-parent a Component entity");
    const World& world = World::getInstance();
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Entity oldParent = pEM->childToParent[child];
    pEM->childToParent[child] = *this;
    pEM->parentToChildren[oldParent].erase(child);
    pEM->parentToChildren[*this].insert(child);
    if (isValid() && child.isValid() && world.mAdoptCallback) {
        world.mAdoptCallback(child, *this);
    }
}

ecs::Entity Entity::createChild(bool isActive) const {
    const World& world = World::getInstance();
    Entity e = static_cast<EntityManager*>(world.mEntityManager)->createEntity(isActive, *this);
    if (e.isValid() && world.mChildCreateCallback) {
        world.mChildCreateCallback(e, *this);
    }
    return e;
}

ecs::Entity Entity::createChild(const char* name, bool isActive) const {
    const World& world = World::getInstance();
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Entity e = pEM->createEntity(isActive, *this);
    pEM->setEntityName(e, name);

    if (e.isValid() && world.mChildCreateCallback) {
        world.mChildCreateCallback(e, *this);
    }
    return e;
}

void Entity::orphan() const {
    // World::getInstance().orphan(*this);
    const World& world = World::getInstance();
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Entity oldParent = pEM->childToParent[*this];
    if (oldParent == world.mRootEntity) {
        // cannot orphan top-level parent
        return;
    }

    pEM->childToParent[*this] = world.mRootEntity;

    // there are no guarantees on which order parents/children are deleted if the deletes happen on the same frame.
    // BUT i don't think I touch a parent's list of children when it dies, so this should be fine?
    pEM->parentToChildren[oldParent].erase(*this);
    pEM->parentToChildren[world.mRootEntity].insert(*this);
}

Entity Entity::parent() const {
    return static_cast<EntityManager*>(World::getInstance().mEntityManager)->childToParent[*this];
}

const std::unordered_set<Entity, EntityHash>& Entity::children() const {
    return static_cast<EntityManager*>(World::getInstance().mEntityManager)->parentToChildren[*this];
}

const char* Entity::name() const {
    return static_cast<EntityManager*>(World::getInstance().mEntityManager)->getEntityName(*this);
}

void Entity::setName(const char* name) const {
    static_cast<EntityManager*>(World::getInstance().mEntityManager)->setEntityName(*this, name);
}

void Entity::setName(std::string_view name) const {
    static_cast<EntityManager*>(World::getInstance().mEntityManager)->setEntityName(*this, name);
}

ComponentList Entity::getComponents() const {
    World& world = World::getInstance();
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);

    ComponentList result;

    // components:
    const Pattern& pattern = pEM->getPattern(*this);
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern.test(i)) {
            result.push_back(world.mComponentManager->getComponentEntity(i));
        }
    }

    // tags:
    const Pattern& tagPattern = pEM->getTagPattern(*this);
    for (size_t i = 0; i < tagPattern.size(); i++) {
        if (tagPattern.test(i)) {
            result.push_back(world.mComponentManager->getTagEntity(i));
        }
    }
    return result;
}

const Pattern& Entity::getPattern() const {
    const EntityManager* pEM = static_cast<EntityManager*>(World::getInstance().mEntityManager);
    return pEM->getPattern(*this);
}

const Pattern& Entity::getTagPattern() const {
    const EntityManager* pEM = static_cast<EntityManager*>(World::getInstance().mEntityManager);
    return pEM->getTagPattern(*this);
}

void Entity::removeFromMgr(const World& world, ComponentType t) {
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Pattern& pattern = pEM->getPatternMut(*this);
    pattern.set(t, false);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pattern, pEM->getTagPattern(*this));
    }
}

void Entity::removeTagFromMgr(const World& world, ComponentType t) {
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Pattern& pattern = pEM->getTagPatternMut(*this);
    pattern.set(t, false);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pEM->getPattern(*this), pattern);
    }
}

void Entity::addToMgr(const World& world, ComponentType t) {
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Pattern& pattern = pEM->getPatternMut(*this);
    pattern.set(t, true);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pattern, pEM->getTagPattern(*this));
    }
}

void Entity::addTagToMgr(const World& world, ComponentType t) {
    EntityManager* pEM = static_cast<EntityManager*>(world.mEntityManager);
    Pattern& pattern = pEM->getTagPatternMut(*this);
    pattern.set(t, true);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pEM->getPattern(*this), pattern);
    }
}

bool Entity::_has(ComponentType t) const {
    return static_cast<EntityManager*>(World::getInstance().mEntityManager)->getPattern(*this).test(t);
}

bool Entity::_hasTag(ComponentType t) const {
    return static_cast<EntityManager*>(World::getInstance().mEntityManager)->getTagPattern(*this).test(t);
}

}  // namespace whal::ecs
