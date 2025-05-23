#pragma once

#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DynamicBitset.h"
#include "Traits.h"
#include "TypeName.h"

typedef uint16_t u16;
typedef uint32_t u32;

#ifndef MAX_ENTITIES
#define MAX_ENTITIES 5000
#endif

#ifndef MAX_COMPONENTS
#define MAX_COMPONENTS 64
#endif

namespace whal::ecs {

class EntityManager;
class SystemManager;

using EntityID = u32;
using ComponentType = u16;
using Pattern = DynamicBitset;
using SystemId = u16;

class Entity;
class World;
struct EntityHash;
using EntityCallback = void (*)(Entity);
using EntityPairCallback = void (*)(Entity, Entity);
using ComponentList = std::vector<Entity>;

namespace internal {

// marks an entity as a Component
struct Component {
    ComponentType id;
};

// marks entity as a Tag component (these entities will also have the Component tag for convenience)
struct Tag {
    ComponentType id;
};

// Components uses by other components are called Traits*.
// When a component T is added to another component C, then C implements the trait T.
// Any component used as a trait is given the TraitUsers struct to cache which components implement it.
// * An exception is made for components added to themselves (which is how singleton components are implemented).
struct TraitUsers {
    Pattern componentPattern;
    Pattern tagPattern;
};

}  // namespace internal

// Tags used by the ECS World

struct OverrideAttributeIgnoreChildren {};

class Entity {
public:
    Entity() = default;
    Entity(EntityID id);
    friend EntityManager;
    friend SystemManager;
    EntityID id() const { return mId; }

    EntityID operator()() const { return mId; }
    bool operator==(Entity other) const { return mId == other.mId; }
    bool operator<(Entity other) const { return mId < other.mId; }

    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity add(T component);

    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity add();

    template <typename T>
        requires(std::is_empty_v<T>)
    Entity add();

    // set value of component that's been added
    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity set(T component) const;

    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity remove();

    template <typename T>
        requires(std::is_empty_v<T>)
    Entity remove();

    template <typename T>
    T* tryGet() const;

    template <typename T>
    T& get() const;

    // returns a pointer to the component of type T in the entity or any of its children. Searches in a DFS manner.
    template <typename T>
    T* getInChildren(bool includeInactive = false) const;

    template <typename T>
        requires(!std::is_empty_v<T>)
    bool has() const;

    template <typename T>
        requires(std::is_empty_v<T>)
    bool has() const;

    template <typename T>
    const T& getTrait() const;

    // returns the first of this entity's components which implement the trait T.
    template <typename T>
    Entity getTraitHolder() const;

    Entity copy(bool isActive = true) const;

    Entity activate() const;
    Entity deactivate() const;
    void kill() const;
    bool isValid() const;
    bool isKilledThisFrame() const;

    void addChild(Entity child) const;
    Entity createChild(bool isActive = true) const;
    Entity createChild(const char* name, bool isActive = true) const;
    void orphan() const;  // makes mRootEntity the parent of `e`

    template <typename F, typename... Args>
        requires std::invocable<F, Entity, Args...>
    void forChild(F&& callback, bool isRecursive, Args&&... args) const;

    template <typename Trait, typename F, typename... Args>
        requires std::invocable<F, Entity, Entity, Args...>
    void forTrait(F&& callback, Args&&... args) const;

    Entity parent() const;                                           // parent getter
    const std::unordered_set<Entity, EntityHash>& children() const;  // children getter
    const char* name() const;
    void setName(const char* name) const;
    void setName(std::string_view name) const;

    ComponentList getComponents() const;
    const Pattern& getPattern() const;
    const Pattern& getTagPattern() const;

private:
    EntityID mId = 0;

    void removeFromMgr(const World& world, ComponentType t);
    void removeTagFromMgr(const World& world, ComponentType t);
    void addToMgr(const World& world, ComponentType t);
    void addTagToMgr(const World& world, ComponentType t);
    void addTraitImplementer(ComponentType implementer, bool isImplementerTag);
    void removeTraitImplementer(ComponentType implementer, bool isImplementerTag);
    bool _has(ComponentType t) const;
    bool _hasTag(ComponentType t) const;
};

// utility class. Uses RAII to defer an entity's activation until it goes out of scope
class DeferActivate {
public:
    DeferActivate(Entity entity) : mEntity(entity) {}
    ~DeferActivate() { mEntity.activate(); }

private:
    Entity mEntity;
};

struct EntityHash {
    u32 operator()(ecs::Entity entity) const { return entity.id(); }
};

// methods run in a loop by component manager need to be virtual
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    virtual void entityDestroyed(Entity entity) = 0;
    virtual void copyComponent(Entity prefab, Entity dest) = 0;
};

// maintains dense component data
template <typename T>
class ComponentArray : public IComponentArray {
public:
    ComponentArray() {
        mEntityToIndex.fill(-1);
        mIndexToEntity.fill(0);
    }
    void addData(const Entity entity, T&& component) {
        if (hasData(entity)) {
            // overwrite the current value
            const u32 ix = mEntityToIndex[entity.id()];
            mComponentTable[ix] = std::move(component);
            return;
        }
        // register new entity
        const u32 ix = mSize++;
        mEntityToIndex[entity.id()] = ix;
        mIndexToEntity[ix] = entity.id();
        mComponentTable[ix] = std::move(component);
    }

    void setData(const Entity entity, T&& component) {
        assert(mEntityToIndex[entity.id()] != -1 && "cannot set component value without adding it to the entity first");
        mComponentTable[mEntityToIndex[entity.id()]] = std::move(component);
    }

    void removeData(const Entity entity) {
        if (!hasData(entity)) {
            return;
        }

        // maintain density of entities
        const u32 removeIx = mEntityToIndex[entity.id()];
        const u32 lastIx = --mSize;
        if (removeIx != lastIx) {
            mComponentTable[removeIx] = mComponentTable[lastIx];

            Entity lastEntity = mIndexToEntity[lastIx];
            mEntityToIndex[lastEntity.id()] = removeIx;
            mIndexToEntity[removeIx] = lastEntity.id();
        }

        mEntityToIndex[entity.id()] = -1;
        mIndexToEntity[lastIx] = 0;
    }

    bool hasData(const Entity entity) const { return mEntityToIndex[entity.id()] != -1; }

    T* tryGetData(const Entity entity) {
        if (!hasData(entity)) {
            return nullptr;
        }
        const u32 ix = mEntityToIndex.at(entity.id());
        return &mComponentTable.at(ix);
    }

    T& getData(const Entity entity) {
        assert(hasData(entity) && "getData on entity without component");
        const u32 ix = mEntityToIndex.at(entity.id());
        return mComponentTable.at(ix);
    }

    void entityDestroyed(const Entity entity) override {
        if (!hasData(entity)) {
            return;
        }
        removeData(entity);
    }

    void copyComponent(const Entity prefab, Entity dest) override {
        T* cmpOpt = tryGetData(prefab);
        if (cmpOpt) {
            addData(dest, std::move(*cmpOpt));
        }
    }

private:
    std::array<T, MAX_ENTITIES> mComponentTable;
    std::array<long, MAX_ENTITIES> mEntityToIndex;
    std::array<EntityID, MAX_ENTITIES> mIndexToEntity;
    u32 mSize = 0;
};

// I am doing a tiny interface for EntityManager so I don't have to include the whole class in this header
class IEntityManager {
public:
    virtual ~IEntityManager() = default;
    virtual const std::unordered_set<Entity, EntityHash>& getChildren(Entity e) = 0;
    virtual bool isActive(Entity e) const = 0;
};

template <typename T>
class Exclude;

template <typename T>
class MatchTrait;

template <typename T>
concept IsQueryMetaComponent = is_base_of_template<Exclude, T>::value || is_base_of_template<MatchTrait, T>::value;

// This manages all components. For each registered component, it has an array of the component data, as well as an Entity which holds data about the
// component. For tags, it only has the entities. No data.
class ComponentManager {
    friend World;

public:
    ComponentManager();
    ~ComponentManager();

    template <typename T>
        requires(!std::is_empty_v<T>)
    void registerComponent();

    template <typename T>
        requires(std::is_empty_v<T>)
    void registerTag();

    template <typename T>
    void addComponent(const Entity entity, T&& component) {
        long index = mComponentToIndex[getComponentID<T>()];
        if (index == -1) {
            registerComponent<T>();
            index = mComponentArrays.size() - 1;
        }
        getComponentArray<T>(index)->addData(entity, std::move(component));
    }

    template <typename T>
    void setComponent(const Entity entity, T&& component) {
        getComponentArray<T>(mComponentToIndex[getComponentID<T>()])->setData(entity, std::move(component));
    }

    template <typename T>
    void removeComponent(const Entity entity) const {
        const long ix = mComponentToIndex[getComponentID<T>()];
        if (ix == -1) {
            return;
        }
        getComponentArray<T>(ix)->removeData(entity);
    }

    template <typename T>
    bool hasComponent(const Entity entity) const {
        const long ix = mComponentToIndex[getComponentID<T>()];
        if (ix == -1) {
            return false;
        }
        return getComponentArray<T>(ix)->hasData(entity);
    }

    template <typename T>
    T* tryGetComponent(const Entity entity) const {
        const long ix = mComponentToIndex[getComponentID<T>()];
        if (ix == -1) {
            return nullptr;
        }
        return getComponentArray<T>(ix)->tryGetData(entity);
    }

    template <typename T>
    T& getComponent(const Entity entity) const {
        return getComponentArray<T>(mComponentToIndex[getComponentID<T>()])->getData(entity);
    }

    void entityDestroyed(const Entity entity);
    void copyComponents(const Entity prefab, Entity dest);

    // assign unique IDs to each component type
    static inline ComponentType ComponentID = 0;
    template <typename T>
        requires(!IsQueryMetaComponent<T>)
    static inline ComponentType getComponentID() {
        static ComponentType id = ComponentID++;
        return id;
    }

    // use a separate ID for tags
    static inline ComponentType TagComponentID = 0;
    template <typename T>
        requires(!IsQueryMetaComponent<T>)
    static inline ComponentType getTagID() {
        static ComponentType id = TagComponentID++;
        return id;
    }

    u32 getRegisteredCount() const { return mComponentArrays.size(); }

    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity getComponentEntity() {
        return mComponentEntities[mComponentToIndex[getComponentID<T>()]];
    }

    Entity getComponentEntity(ComponentType type) { return mComponentEntities[mComponentToIndex[type]]; }

    template <typename T>
        requires(std::is_empty_v<T>)
    Entity getTagEntity() {
        return mTagEntities[getTagID<T>()];
    }

    Entity getTagEntity(ComponentType type) { return mTagEntities[type]; }

private:
    template <typename T>
    ComponentArray<T>* getComponentArray(int ix) const {
        return static_cast<ComponentArray<T>*>(mComponentArrays[ix]);
    }

    std::array<long, MAX_COMPONENTS> mComponentToIndex;
    std::vector<Entity> mComponentEntities;
    std::array<Entity, MAX_COMPONENTS> mTagEntities;
    std::vector<IComponentArray*> mComponentArrays;
};

// wrapper type which tells a system that the entity should *not* have this component
template <typename T>
class Exclude {
public:
    using type = T;
};

template <typename T>
class MatchTrait {
public:
    using type = T;
};

class SystemBase {
public:
    friend SystemManager;
    virtual ~SystemBase() = default;

    virtual std::unordered_map<EntityID, Entity>& getEntitiesVirtual() = 0;  // only used by SystemManager
    virtual bool isPatternInSystem(const Pattern& pattern, const Pattern& tagPattern) const = 0;
    virtual bool isMatch(Entity entity) const = 0;
};

// might combine these two? Would be nice for resource allocations/cleanup
// class IWorldStartSystem {
// public:
//     virtual void onStart() = 0;
// };
//
// class IWorldEndSystem {
// public:
//     virtual void onEnd() = 0;
// };

class IUpdate {
public:
    virtual void update() = 0;
};

template <typename T>
struct NotFixedUpdate : std::bool_constant<!std::is_base_of_v<IUpdate, T>> {};

// called when an entity is added/removed from a system. All components in that system can be accessed for that entity when these methods run.
// An entity is not added to any systems until it's activated (FYI).
// NOTE: neither method is run when a component is modified using entity.set<T>().
class IMonitorSystem {
public:
    virtual void onAdd(const Entity entity) = 0;
    virtual void onRemove(const Entity entity) = 0;
};

class AttrUniqueEntity {};
class AttrUpdateDuringPause {};
class AttrExcludeChildren {};

// each system has a set of entities it operates on
// currently their methods are called manually
template <typename... T>
class ISystem : public SystemBase {
public:
    ISystem() {
        mPattern.resize(MAX_COMPONENTS);
        mAntiPattern.resize(MAX_COMPONENTS);
        mTagPattern.resize(MAX_COMPONENTS);
        mTagAntiPattern.resize(MAX_COMPONENTS);
        (InitializeIDs<T>(), ...);
    }

    std::unordered_map<EntityID, Entity>& getEntitiesVirtual() override { return mEntities; }
    static std::unordered_map<EntityID, Entity>& getEntitiesMutable() { return mEntities; }
    static std::unordered_map<EntityID, Entity> getEntitiesCopy() { return mEntities; }
    static const std::unordered_map<EntityID, Entity>& getEntities() { return mEntities; }
    static Entity first() { return mEntities.begin()->second; }
    bool isPatternInSystem(const Pattern& pattern, const Pattern& tagPattern) const override {
        // each trait is effectively an OR on a set of components.
        // to match all traits, we need to match at least one component in each set.
        bool isTraitsSatisfied = true;
        for (Entity trait : mTraits) {
            const internal::TraitUsers* traitUsers = trait.tryGet<internal::TraitUsers>();
            if (traitUsers) {
                if (traitUsers->componentPattern.containsNone(pattern) && traitUsers->tagPattern.containsNone(tagPattern)) {
                    isTraitsSatisfied = false;
                    break;
                }
            } else {
                isTraitsSatisfied = false;
                break;
            }
        }
        return isTraitsSatisfied && mPattern.contains(pattern) && mTagPattern.contains(tagPattern) && mAntiPattern.containsNone(pattern) &&
               mTagAntiPattern.containsNone(tagPattern);
    }

    bool isMatch(Entity e) const override { return isPatternInSystem(e.getPattern(), e.getTagPattern()); }

private:
    template <typename Component>
    void InitializeIDs();

    inline static std::unordered_map<EntityID, Entity> mEntities = {};
    Pattern mPattern;
    Pattern mAntiPattern;
    Pattern mTagPattern;
    Pattern mTagAntiPattern;
    std::vector<Entity> mTraits;
};

// i fucking love concepts
template <typename T, typename I>
    requires std::derived_from<T, I>
I* toInterfacePtr(T* ptr) {
    return static_cast<I*>(ptr);
}

template <typename T, typename I>
    requires(!std::derived_from<T, I>)
I* toInterfacePtr(const T* const ptr) {
    return nullptr;
}

class SystemManager {
    struct UpdateGroupInfo {
        int intervalFrame;
        bool isParallel;
    };

public:
    enum Attributes : u16 {
        UniqueEntity = 1,
        UpdateDuringPause = 1 << 1,
        ExcludeChildren = 1 << 2,
    };

    template <class T>
    T* getSystem() const {
        const SystemId id = getSystemID<T>();

        // if idToIndex's size is <= to ID, then we've never registered this system.
        // otherwise, make sure this ID corresponds to a registered system (not -1).
        assert(mSystemIdToIndex.size() > static_cast<size_t>(id) && mSystemIdToIndex[id] != -1 && "System not registered");

        return static_cast<T*>(mSystems[mSystemIdToIndex[id]]);
    }

    template <class T>
    T* registerSystem(u16 attributes = 0) {
        static_assert(is_base_of_template<ISystem, T>::value, "Cannot register class which doesn't inherit ISystem");
        const SystemId id = getSystemID<T>();

        // if idToIndex's size is > ID, we've generated an ID for this type before.
        // if we have, make sure it is unregistered (is -1)
        const size_t id_t = static_cast<size_t>(id);
        assert(mSystemIdToIndex.size() <= id_t || mSystemIdToIndex[id_t] == -1 && "System already registered");

        if (id_t == mSystemIdToIndex.size()) {
            mSystemIdToIndex.push_back(mSystems.size());
        } else if (id_t > mSystemIdToIndex.size()) {
            // we've generated some System IDs since the last time something was registered. Add -1s for each one we've generated (up to and including
            // the current ID)
            long toAdd = id_t - mSystemIdToIndex.size() + 1;
            for (long i = 0; i < toAdd; i++) {
                mSystemIdToIndex.push_back(-1);
            }

            // set the id's index
            mSystemIdToIndex[id_t] = mSystems.size();
        } else {
            // id was generated previously and another system has since been registered, so our index is -1. Update it.
            mSystemIdToIndex[id_t] = mSystems.size();
        }

        T* system = new T;

        // The way these two interfaces are used, it's convenient for these list's indices to match with mSystems
        mUpdateSystems.push_back(toInterfacePtr<T, IUpdate>(system));
        mMonitorSystems.push_back(toInterfacePtr<T, IMonitorSystem>(system));

        // check attributes
        if (std::derived_from<T, AttrUniqueEntity>) {
            attributes |= UniqueEntity;
        }
        if (std::derived_from<T, AttrUpdateDuringPause>) {
            attributes |= UpdateDuringPause;
        }
        if (std::derived_from<T, AttrExcludeChildren>) {
            attributes |= ExcludeChildren;
        }
        mAttributes.push_back(attributes);

        mSystems.push_back(system);
        return system;
    }

    // register systems which don't implement FixedUpdate
    template <class... T>
        requires(std::conjunction_v<NotFixedUpdate<T>...>)
    SystemManager& registerSystems(u16 attributes = 0) {
        (registerSystem<T>(attributes), ...);

        return *this;
    }

    // registers systems which must run sequentially
    template <class... T>
    SystemManager& sequential(int interval = 1) {
        int groupStartIx = mSystems.size();
        (registerSystem<T>(), ...);
        std::vector<int> groupIndices;
        for (size_t i = groupStartIx; i < mSystems.size(); i++) {
            if (mUpdateSystems[i]) {
                groupIndices.push_back(i);
            }
        }
        mUpdateGroups.push_back({UpdateGroupInfo(interval, false), std::move(groupIndices)});

        return *this;
    }

    // registers systems which can run in parallel
    template <class... T>
    SystemManager& parallel(int interval = 1) {
        int groupStartIx = mSystems.size();
        (registerSystem<T>(), ...);
        std::vector<int> groupIndices;
        for (size_t i = groupStartIx; i < mSystems.size(); i++) {
            if (mUpdateSystems[i]) {
                groupIndices.push_back(i);
            }
        }
        bool isParallel = groupIndices.size() > 1;
        mUpdateGroups.push_back({UpdateGroupInfo(interval, isParallel), std::move(groupIndices)});

        return *this;
    }

    void clear();
    void autoUpdate();
    void onEntityDestroyed(const Entity entity) const;
    void onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern, const Pattern& newEntityTagPattern) const;
    void onEntityParentChanged(const Entity entity);
    void onPaused();
    void onUnpaused();

private:
    // assign unique IDs to each system type
    static inline SystemId SystemID = 0;
    template <class T>
    static inline SystemId getSystemID() {
        static SystemId id = SystemID++;
        return id;
    }

    template <typename T>
    long getIndex() {
        SystemId id = getSystemID<T>();
        return mSystemIdToIndex[id];
    }

    void checkIfInSystem(Entity entity, SystemBase* system, size_t systemIx, const Pattern& entityPattern, const Pattern& tagPattern,
                         bool isEntityInSystem) const;

    std::vector<long> mSystemIdToIndex;
    std::vector<SystemBase*> mSystems;
    std::vector<IUpdate*> mUpdateSystems;          // may contain null ptrs
    std::vector<IMonitorSystem*> mMonitorSystems;  // may contain null ptrs
    std::vector<u16> mAttributes;

    std::vector<std::pair<UpdateGroupInfo, std::vector<int>>>
        mUpdateGroups;  // ordered list of lists, where each list is 1+ systems which need to be updated sequentially
    int mFrame = 0;
    bool mIsWorldPaused = false;
};

class World {
    friend Entity;
    friend ComponentManager;

public:
    inline static World& getInstance() {
        static World instance;
        return instance;
    }

    // COMPONENT
    u32 getComponentCount() const;

    // registers a component/tag (if not done already) and returns the associated entity.
    template <typename T>
    Entity component() const {
        if constexpr (std::is_empty_v<T>) {
            if (!mComponentManager->getTagEntity<T>().isValid()) {
                // tag not registered
                mComponentManager->registerTag<T>();
            }
            return mComponentManager->getTagEntity<T>();
        } else {
            if (mComponentManager->mComponentToIndex[mComponentManager->getComponentID<T>()] == -1) {
                // component not registered
                mComponentManager->registerComponent<T>();
            }
            return mComponentManager->getComponentEntity<T>();
        }
    }

    // SINGLETON COMPONENTS

    // Singleton components basically store their component in the corresponding component entity.
    // The component needs to be added manually. Components aren't singletons by default.
    // Tags cannot be singletons
    template <typename T>
        requires(!std::is_empty_v<T>)
    T& get() const {
        Entity e = mComponentManager->getComponentEntity<T>();
        return e.get<T>();
    }

    template <typename T>
    void add() const {
        Entity e = component<T>();
        e.add<T>();
    }

    template <typename T>
        requires(!std::is_empty_v<T>)
    void add(T data) const {
        Entity e = component<T>();
        e.add<T>(std::move(data));
    }

    template <typename T>
        requires(!std::is_empty_v<T>)
    void set(T component) const {
        Entity e = mComponentManager->getComponentEntity<T>();
        e.set<T>(std::move(component));
    }

    template <typename T>
    void remove() const {
        // doesn't implicitly register a component because calling remove on an unregistered component is an error
        Entity e;
        if constexpr (std::is_empty_v<T>) {
            e = mComponentManager->getTagEntity<T>();
        } else {
            e = mComponentManager->getComponentEntity<T>();
        }
        e.remove<T>();
    }

    template <typename T>
    bool has() const {
        Entity e = component<T>();
        return e.has<T>();
    }

    // ENTITY
    Entity entity(bool isActive = true) const;
    Entity entity(const char* name, bool isActive = true) const;
    void kill(Entity entity);
    void killEntities();  // called by update. Should only be called manually in specific circumstances like scene loading

    Entity copy(Entity entity, bool isActive) const;
    void activate(Entity entity) const;
    void deactivate(Entity entity) const;

    u32 getEntityCount() const;
    u32 getActiveEntityCount() const;
    void setEntityDeathCallback(EntityCallback callback);
    void setEntityCreateCallback(EntityCallback callback);
    void setEntityChildCreateCallback(EntityPairCallback callback);
    void setEntityAdoptCallback(EntityPairCallback callback);

    void unparent(Entity e) const;  // removes `e` from all parent lists

    // query an entity by name.
    // Name uniqueness is not enforced. If multiple entities have the same name, returns the entity which was most recently created.
    Entity lookup(const char* name) const;

    // SYSTEM
    template <typename T>
    T* getSystem() const {
        return mSystemManager->getSystem<T>();
    }

    template <typename T>
    T* registerSystem(u16 attributes = 0) const {
        return mSystemManager->registerSystem<T>(attributes);
    }

    // this doesn't do anything, but I want the caller code to be understandable
    SystemManager& BeginSystemRegistration() const { return *mSystemManager; }

    void update() {
        mSystemManager->autoUpdate();
        killEntities();
    }

    void pause() const { mSystemManager->onPaused(); }
    void unpause() const { mSystemManager->onUnpaused(); }
    void clear();

private:
    // no copy
    World();
    ~World();
    World(const World&) = delete;
    void operator=(const World&) = delete;

    // is private because it's a bad idea to use this in game logic. An entity's ID could be recycled at any time
    bool isActive(Entity entity) const;

    // creates an entity for the component. Does *not* emit a creation callback. Entity is also not alive.
    Entity componentEntity(ComponentType type);

    IEntityManager* mEntityManager;
    ComponentManager* mComponentManager;
    SystemManager* mSystemManager;
    std::unordered_set<Entity, EntityHash> mToKill;
    std::unordered_set<Entity, EntityHash> mKilledThisFrame;
    EntityCallback mDeathCallback = nullptr;
    EntityCallback mCreateCallback = nullptr;
    EntityPairCallback mChildCreateCallback = nullptr;
    EntityPairCallback mAdoptCallback = nullptr;
    Entity mRootEntity;  // I use the "invalid" entity as the world root. Entities created with `entity()` are children of this entity.
};

template <typename T>
    requires(!std::is_empty_v<T>)
Entity Entity::add(T component) {
    const World& world = World::getInstance();
    world.mComponentManager->addComponent(*this, std::move(component));
    addToMgr(world, ComponentManager::getComponentID<T>());
    return *this;
}

template <typename T>
    requires(!std::is_empty_v<T>)
Entity Entity::add() {
    add(T());
    return *this;
}

template <typename T>
    requires(std::is_empty_v<T>)
Entity Entity::add() {
    const World& world = World::getInstance();
    if (!world.mComponentManager->getTagEntity<T>().isValid()) {
        world.mComponentManager->registerTag<T>();
    }
    addTagToMgr(world, ComponentManager::getTagID<T>());
    return *this;
}

template <typename T>
    requires(!std::is_empty_v<T>)
Entity Entity::set(T component) const {
    World::getInstance().mComponentManager->setComponent(*this, std::move(component));
    return *this;
}

template <typename T>
    requires(!std::is_empty_v<T>)
Entity Entity::remove() {
    const World& world = World::getInstance();
    removeFromMgr(world, ComponentManager::getComponentID<T>());  // should always go before removeComponent so we can run onRemove method
    world.mComponentManager->removeComponent<T>(*this);
    return *this;
}

template <typename T>
    requires(std::is_empty_v<T>)
Entity Entity::remove() {
    removeTagFromMgr(World::getInstance(), ComponentManager::getTagID<T>());
    return *this;
}

template <typename T>
T* Entity::tryGet() const {
    return World::getInstance().mComponentManager->tryGetComponent<T>(*this);
}

template <typename T>
T& Entity::get() const {
    return World::getInstance().mComponentManager->getComponent<T>(*this);
}

template <typename T>
T* Entity::getInChildren(bool includeInactive) const {
    const World& world = World::getInstance();
    T* cmp = world.mComponentManager->tryGetComponent<T>(*this);
    if (cmp) {
        return cmp;
    }
    for (const Entity& child : world.mEntityManager->getChildren(*this)) {
        if (includeInactive || world.mEntityManager->isActive(child)) {
            cmp = child.getInChildren<T>(includeInactive);
            if (cmp) {
                return cmp;
            }
        }
    }
    return nullptr;
}

template <typename T>
    requires(!std::is_empty_v<T>)
bool Entity::has() const {
    return _has(ComponentManager::getComponentID<T>());
}

template <typename T>
    requires(std::is_empty_v<T>)
bool Entity::has() const {
    return _hasTag(ComponentManager::getTagID<T>());
}

template <typename T>
Entity Entity::getTraitHolder() const {
    World& world = World::getInstance();
    const Entity traitEntity = world.component<T>();

    const internal::TraitUsers* traitUsers = traitEntity.tryGet<internal::TraitUsers>();
    if (!traitUsers) {
        // this trait is not implemented by any components
        return Entity{};
    }

    // matchIx > size() if there's no match
    const Pattern& cmpPattern = getPattern();
    const Pattern& traitPattern = traitUsers->componentPattern;
    size_t matchIx = traitPattern.getIndexOfFirstMatch(cmpPattern);
    if (matchIx < traitPattern.size()) {
        return world.mComponentManager->getComponentEntity(matchIx);
    }

    // no match on components. check tags.
    const Pattern& tagPattern = getTagPattern();
    const Pattern& traitTagPattern = traitEntity.getTagPattern();
    matchIx = traitTagPattern.getIndexOfFirstMatch(tagPattern);
    if (matchIx < traitTagPattern.size()) {
        return world.mComponentManager->getTagEntity(matchIx);
    }

    // trait not found in entity
    return Entity{};
}

template <typename T>
const T& Entity::getTrait() const {
    Entity traitHolder = getTraitHolder<T>();
    assert(traitHolder.isValid() && "getTrait called on Entity without trait");
    return traitHolder.get<T>();
}

template <typename F, typename... Args>
    requires std::invocable<F, Entity, Args...>
void Entity::forChild(F&& callback, bool isRecursive, Args&&... args) const {
    if (isRecursive) {
        for (const Entity& child : World::getInstance().mEntityManager->getChildren(*this)) {
            callback(child, std::forward<Args>(args)...);
            child.forChild(std::forward<F>(callback), true, std::forward<Args>(args)...);
        }
    } else {
        for (const Entity& child : World::getInstance().mEntityManager->getChildren(*this)) {
            callback(child, std::forward<Args>(args)...);
        }
    }
}

template <typename Trait, typename F, typename... Args>
    requires std::invocable<F, Entity, Entity, Args...>
void Entity::forTrait(F&& callback, Args&&... args) const {
    World& world = World::getInstance();
    const Entity traitEntity = world.component<Trait>();

    const internal::TraitUsers* traitUsers = traitEntity.tryGet<internal::TraitUsers>();
    if (!traitUsers) {
        // this trait is not implemented by any components
        return;
    }

    // matchIx > size() if there's no match
    const Pattern& traitPattern = traitUsers->componentPattern;
    Pattern cmpPattern = getPattern();
    size_t matchIx = traitPattern.getIndexOfFirstMatch(cmpPattern);
    while (matchIx < traitPattern.size()) {
        Entity traitHolder = world.mComponentManager->getComponentEntity(matchIx);
        callback(*this, traitHolder, std::forward<Args>(args)...);
        cmpPattern.set(matchIx, 0);  // 0 this index and search for the next one
        matchIx = traitPattern.getIndexOfFirstMatch(cmpPattern);
    }

    // no match on components. check tags.
    cmpPattern = getTagPattern();
    const Pattern& traitTagPattern = traitEntity.getTagPattern();
    matchIx = traitTagPattern.getIndexOfFirstMatch(cmpPattern);
    while (matchIx < traitTagPattern.size()) {
        Entity traitHolder = world.mComponentManager->getTagEntity(matchIx);
        callback(*this, traitHolder, std::forward<Args>(args)...);
        cmpPattern.set(matchIx, 0);
        matchIx = traitTagPattern.getIndexOfFirstMatch(cmpPattern);
    }
}

template <typename T>
    requires(!std::is_empty_v<T>)
void ComponentManager::registerComponent() {
    const ComponentType type = getComponentID<T>();
    assert(type < MAX_COMPONENTS && "Registered more than MAX_COMPONENTS components");
    assert(mComponentToIndex[getComponentID<T>()] == -1 && "Component type already registered");
    const long newIndex = mComponentArrays.size();
    mComponentToIndex[type] = newIndex;
    mComponentArrays.push_back(new ComponentArray<T>());

    // not passing name here because I need to include string to parse the string_view and I'd rather do that in a source file.
    Entity e = World::getInstance().componentEntity(type);
    e.setName(type_of<T>());
    mComponentEntities.push_back(e);
    e.add<internal::Component>({.id = type});
}

template <typename T>
    requires(std::is_empty_v<T>)
void ComponentManager::registerTag() {
    const ComponentType type = getTagID<T>();
    assert(type < MAX_COMPONENTS && "Registered more than MAX_COMPONENTS tags");
    assert(mTagEntities[type].id() == 0 && "Tag type already registered");

    Entity e = World::getInstance().componentEntity(type);
    e.setName(type_of<T>());
    mTagEntities[type] = e;

    // must add these *after* setting mTagEntities, otherwise will infinitely recurse
    e.add<internal::Component>({.id = MAX_COMPONENTS + 1});
    e.add<internal::Tag>({.id = type});
}

template <typename... T>
template <typename Component>
void ISystem<T...>::InitializeIDs() {
    if constexpr (is_base_of_template<Exclude, Component>::value) {
        if constexpr (std::is_empty_v<Component>) {
            mTagAntiPattern.set(ComponentManager::getTagID<typename Component::type>());
        } else {
            mAntiPattern.set(ComponentManager::getComponentID<typename Component::type>());
        }
    } else if constexpr (is_base_of_template<MatchTrait, Component>::value) {
        mTraits.push_back(World::getInstance().component<typename Component::type>());
    } else {
        if constexpr (std::is_empty_v<Component>) {
            mTagPattern.set(ComponentManager::getTagID<Component>());
        } else {
            mPattern.set(ComponentManager::getComponentID<Component>());
        }
    }
}

}  // namespace whal::ecs
