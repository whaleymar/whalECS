#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <concepts>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Traits.h"

typedef uint16_t u16;
typedef uint32_t u32;

namespace whal::gfx {
struct EntityRenderInfo;
struct RenderContext;
class RenderQueue;
}  // namespace whal::gfx

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
using Pattern = std::bitset<MAX_COMPONENTS>;
using SystemId = u16;

class Entity;
struct EntityHash;
using EntityCallback = void (*)(Entity);
using EntityPairCallback = void (*)(Entity, Entity);

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
    Entity add(T component);

    // template <typename T>
    // Entity add(T&& component);

    template <typename T>
    Entity add();

    // set value of component that's been added
    template <typename T>
    Entity set(T component) const;

    template <typename T>
    Entity remove();

    template <typename T>
    std::optional<T> tryGet() const;

    template <typename T>
    T& get() const;

    template <typename T>
    bool has() const;

    Entity copy(bool isActive = true) const;

    void activate() const;
    void deactivate() const;
    void kill() const;
    bool isValid() const;

    void addChild(Entity child) const;
    Entity createChild(bool isActive = true) const;
    void orphan() const;  // makes mRootEntity the parent of `e`

    template <typename... T>
    void forChild(void(callback)(ecs::Entity, T...), bool isRecursive, T... args) const;

    Entity parent() const;                                           // parent getter
    const std::unordered_set<Entity, EntityHash>& children() const;  // children getter

private:
    EntityID mId = 0;
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

// std::find from <algorithm> so I don't have to include the whole thing
template <class InputIterator, class T>
InputIterator whal_find(InputIterator first, InputIterator last, const T& val) {
    while (first != last) {
        if (*first == val)
            return first;
        ++first;
    }
    return last;
}

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

    std::optional<T> tryGetData(const Entity entity) {
        if (!hasData(entity)) {
            return std::nullopt;
        }
        const u32 ix = mEntityToIndex.at(entity.id());
        return mComponentTable.at(ix);
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
        auto cmpOpt = tryGetData(prefab);
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

class EntityManager {
public:
    EntityManager();

    Entity createEntity(bool isAlive, Entity parent);
    void destroyEntity(Entity entity);
    void setPattern(Entity entity, const Pattern& pattern);
    Pattern getPattern(Entity entity) const;
    u32 getEntityCount() const { return mEntityCount; }
    u32 getActiveEntityCount() const;
    bool isActive(Entity entity) const;

    // returns true if entity was activated, false if it was already active
    bool activate(Entity entity);
    bool deactivate(Entity entity);

    std::unordered_map<Entity, Entity, EntityHash> childToParent;
    std::unordered_map<Entity, std::unordered_set<Entity, EntityHash>, EntityHash> parentToChildren;

private:
    std::queue<EntityID> mAvailableIDs;
    std::array<Pattern, MAX_ENTITIES> mPatterns;
    std::bitset<MAX_ENTITIES> mActiveEntities;
    std::mutex mCreatorMutex;
    u32 mEntityCount = 0;
};

template <typename T>
class Exclude;

class ComponentManager {
public:
    ComponentManager();
    ~ComponentManager();

    template <typename T>
    void registerComponent() {
        const ComponentType type = getComponentID<T>();
        assert(type < MAX_COMPONENTS && "Registered more than MAX_COMPONENTS components");
        assert(getIndex<T>() == -1 && "Component type already registered");
        mComponentToIndex[type] = mComponentArrays.size();
        mComponentArrays.push_back(new ComponentArray<T>());
    }

    template <typename T>
    void addComponent(const Entity entity, T&& component) {
        long index = getIndex<T>();
        if (index == -1) {
            registerComponent<T>();
            index = mComponentArrays.size() - 1;
        }
        getComponentArray<T>(index)->addData(entity, std::move(component));
    }

    template <typename T>
    void setComponent(const Entity entity, T&& component) {
        getComponentArray<T>(getIndex<T>())->setData(entity, std::move(component));
    }

    template <typename T>
    void removeComponent(const Entity entity) const {
        const long ix = getIndex<T>();
        if (ix == -1) {
            return;
        }
        getComponentArray<T>(ix)->removeData(entity);
    }

    template <typename T>
    bool hasComponent(const Entity entity) const {
        const long ix = getIndex<T>();
        if (ix == -1) {
            return false;
        }
        return getComponentArray<T>(ix)->hasData(entity);
    }

    template <typename T>
    std::optional<T> tryGetComponent(const Entity entity) const {
        const long ix = getIndex<T>();
        if (ix == -1) {
            return std::nullopt;
        }
        return getComponentArray<T>(ix)->tryGetData(entity);
    }

    template <typename T>
    T& getComponent(const Entity entity) const {
        return getComponentArray<T>(getIndex<T>())->getData(entity);
    }

    void entityDestroyed(const Entity entity);
    void copyComponents(const Entity prefab, Entity dest);

    // assign unique IDs to each component type
    static inline ComponentType ComponentID = 0;
    template <typename T>
        requires(!is_base_of_template<Exclude, T>::value)
    static inline ComponentType getComponentID() {
        static ComponentType id = ComponentID++;
        return id;
    }

    template <typename T>
        requires(is_base_of_template<Exclude, T>::value)
    static inline ComponentType getComponentID() {
        return T::COMPONENT_TYPE;
    }

    u32 getRegisteredCount() const { return mComponentArrays.size(); }

private:
    template <typename T>
    ComponentArray<T>* getComponentArray(int ix) const {
        return static_cast<ComponentArray<T>*>(mComponentArrays[ix]);
    }

    template <typename T>
    long getIndex() const {
        const ComponentType type = getComponentID<T>();
        return mComponentToIndex[type];
    }

    std::array<long, MAX_COMPONENTS> mComponentToIndex;
    std::vector<IComponentArray*> mComponentArrays;
};

// wrapper type which tells a system that the entity should *not* have this component
template <typename T>
class Exclude {
public:
    inline static ComponentType COMPONENT_TYPE = ComponentManager::getComponentID<T>();
};

class SystemBase {
public:
    friend SystemManager;
    virtual ~SystemBase() = default;

    virtual std::unordered_map<EntityID, Entity>& getEntitiesVirtual() = 0;  // only used by SystemManager
    virtual bool isPatternInSystem(Pattern pattern) = 0;
};

// might combine these two? not sure who would use it
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

class IReactToPause {
public:
    virtual void onPause() = 0;
    virtual void onUnpause() = 0;
};

class IRender {
public:
    // Draw a single entity.
    virtual void draw(const gfx::EntityRenderInfo& entityInfo, const gfx::RenderContext& ctx) const = 0;

    // Adds all entities to the draw queue. Culling is performed automatically.
    virtual void addToQueue(gfx::RenderQueue& queue) const = 0;
};

class IRenderLight {
public:
    // Draws all lights to the lighting texture.
    virtual void draw(const gfx::RenderContext& ctx) const = 0;
};

class AttrUniqueEntity {};
class AttrUpdateDuringPause {};

// each system has a set of entities it operates on
// currently their methods are called manually
template <typename... T>
class ISystem : public SystemBase {
public:
    ISystem() {
        std::vector<ComponentType> componentTypes;
        std::vector<ComponentType> antiComponentTypes;
        InitializeIDs<T...>(componentTypes, antiComponentTypes);
        for (auto const& componentType : componentTypes) {
            mPattern.set(componentType);
        }
        for (auto const& componentType : antiComponentTypes) {
            mAntiPattern.set(componentType);
        }
    }

    std::unordered_map<EntityID, Entity>& getEntitiesVirtual() override { return mEntities; }
    static std::unordered_map<EntityID, Entity>& getEntitiesMutable() { return mEntities; }
    static std::unordered_map<EntityID, Entity> getEntitiesCopy() { return mEntities; }
    static const std::unordered_map<EntityID, Entity>& getEntities() { return mEntities; }
    static Entity first() { return mEntities.begin()->second; }
    Pattern getPattern() { return mPattern; }
    bool isPatternInSystem(Pattern pattern) override { return (pattern & mPattern) == mPattern && (pattern & mAntiPattern) == 0; }

private:
    // need a First and Second, otherwise there is ambiguity when there's only one
    // element (InitializeIDs<type> vs InitializeIDs<type, <>>)
    template <typename First, typename Second, typename... Rest>
    static void InitializeIDs(std::vector<ComponentType>& componentTypes, std::vector<ComponentType>& antiComponentTypes) {
        if (is_base_of_template<Exclude, First>::value) {
            antiComponentTypes.push_back(ComponentManager::getComponentID<First>());
        } else {
            componentTypes.push_back(ComponentManager::getComponentID<First>());
        }
        InitializeIDs<Second, Rest...>(componentTypes, antiComponentTypes);
    }

    template <typename Last>
    static void InitializeIDs(std::vector<ComponentType>& componentTypes, std::vector<ComponentType>& antiComponentTypes) {
        if (is_base_of_template<Exclude, Last>::value) {
            antiComponentTypes.push_back(ComponentManager::getComponentID<Last>());
        } else {
            componentTypes.push_back(ComponentManager::getComponentID<Last>());
        }
    }

    inline static std::unordered_map<EntityID, Entity> mEntities = {};
    Pattern mPattern;
    Pattern mAntiPattern;
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

struct RenderSystemPair {
    IRender* pIRender;
    SystemBase* pSystem;
};

class SystemManager {
    struct UpdateGroupInfo {
        int intervalFrame;
        bool isParallel;
    };

public:
    enum Attributes : u16 {
        UniqueEntity = 1,
        UpdateDuringPause = 1 << 1,
    };

    template <class T>
    T* getSystem() const {
        const SystemId id = getSystemID<T>();
        assert(mSystemIdToIndex.contains(id) && "System not registered");

        return static_cast<T*>(mSystems[mSystemIdToIndex.at(id)]);
    }

    template <class T>
    T* registerSystem(u16 attributes = 0) {
        static_assert(is_base_of_template<ISystem, T>::value, "Cannot register class which doesn't inherit ISystem");
        const SystemId id = getSystemID<T>();
        assert(!mSystemIdToIndex.contains(id) && "System already registered");

        mSystemIdToIndex.insert({id, mSystems.size()});

        T* system = new T;

        // The way these two interfaces are used, it's convenient for these list's indices to match with mSystems
        mUpdateSystems.push_back(toInterfacePtr<T, IUpdate>(system));
        mMonitorSystems.push_back(toInterfacePtr<T, IMonitorSystem>(system));

        // Check other interfaces. These lists don't store nullptrs;
        if (auto iPtr = toInterfacePtr<T, IReactToPause>(system); iPtr) {
            mPauseSystems.push_back(iPtr);
        }
        if (auto iPtr = toInterfacePtr<T, IRender>(system); iPtr) {
            mRenderSystems.push_back({iPtr, system});
        }
        if (auto iPtr = toInterfacePtr<T, IRenderLight>(system); iPtr) {
            mLightRenderSystems.push_back(iPtr);
        }

        // check attributes
        if (toInterfacePtr<T, AttrUniqueEntity>(system)) {
            attributes |= UniqueEntity;
        }
        if (toInterfacePtr<T, AttrUpdateDuringPause>(system)) {
            attributes |= UpdateDuringPause;
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
    void onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern) const;
    void onPaused();
    void onUnpaused();

    const std::vector<RenderSystemPair>& getRenderSystems() const { return mRenderSystems; }
    const std::vector<IRenderLight*>& getLightSystems() const { return mLightRenderSystems; }

private:
    // assign unique IDs to each system type
    static inline SystemId SystemID = 0;
    template <class T>
    static inline SystemId getSystemID() {
        static SystemId id = SystemID++;
        return id;
    }

    std::unordered_map<SystemId, int> mSystemIdToIndex;
    std::vector<SystemBase*> mSystems;
    std::vector<IUpdate*> mUpdateSystems;          // may contain null ptrs
    std::vector<IMonitorSystem*> mMonitorSystems;  // may contain null ptrs
    std::vector<IReactToPause*> mPauseSystems;
    std::vector<RenderSystemPair> mRenderSystems;
    std::vector<IRenderLight*> mLightRenderSystems;
    std::vector<u16> mAttributes;

    std::vector<std::pair<UpdateGroupInfo, std::vector<int>>>
        mUpdateGroups;  // ordered list of lists, where each list is 1+ systems which need to be updated sequentially
    int mFrame = 0;
    bool mIsWorldPaused = false;
};

class World {
    friend Entity;

public:
    inline static World& getInstance() {
        static World instance;
        return instance;
    }

    // COMPONENT
    u32 getComponentCount() const;

    // ENTITY
    Entity entity(bool isActive = true) const;
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

    // SYSTEM
    template <typename T>
    T* getSystem() const {
        return mSystemManager->getSystem<T>();
    }

    template <typename T>
    T* registerSystem(u16 attributes = 0) const {
        return mSystemManager->registerSystem<T>(attributes);
    }

    const std::vector<RenderSystemPair>& getRenderSystems() const { return mSystemManager->getRenderSystems(); }
    const std::vector<IRenderLight*>& getLightSystems() const { return mSystemManager->getLightSystems(); }

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

    EntityManager* mEntityManager;
    ComponentManager* mComponentManager;
    SystemManager* mSystemManager;
    std::unordered_set<Entity, EntityHash> mToKill;
    EntityCallback mDeathCallback = nullptr;
    EntityCallback mCreateCallback = nullptr;
    EntityPairCallback mChildCreateCallback = nullptr;
    EntityPairCallback mAdoptCallback = nullptr;
    Entity mRootEntity;  // I use the "invalid" entity as the world root. Entities created with `entity()` are children of this entity.
};

template <typename T>
Entity Entity::add(T component) {
    const World& world = World::getInstance();
    world.mComponentManager->addComponent(*this, std::move(component));
    Pattern pattern = world.mEntityManager->getPattern(*this);
    pattern.set(ComponentManager::getComponentID<T>(), true);
    world.mEntityManager->setPattern(*this, pattern);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pattern);
    }
    return *this;
}

// template <typename T>
// Entity Entity::add(T&& component) {
//     const World& world = World::getInstance();
//     world.mComponentManager->addComponent(*this, component);
//     Pattern pattern = world.mEntityManager->getPattern(*this);
//     pattern.set(ComponentManager::getComponentID<T>(), true);
//     world.mEntityManager->setPattern(*this, pattern);
//     if (world.isActive(*this)) {
//         world.mSystemManager->onEntityPatternChanged(*this, pattern);
//     }
//     return *this;
// }

template <typename T>
Entity Entity::add() {
    add(T());
    return *this;
}

template <typename T>
Entity Entity::set(T component) const {
    World::getInstance().mComponentManager->setComponent(*this, std::move(component));
    return *this;
}

template <typename T>
Entity Entity::remove() {
    // World::getInstance().removeComponent<T>(*this);
    const World& world = World::getInstance();
    Pattern pattern = world.mEntityManager->getPattern(*this);
    pattern.set(ComponentManager::getComponentID<T>(), false);
    world.mEntityManager->setPattern(*this, pattern);
    if (world.isActive(*this)) {
        world.mSystemManager->onEntityPatternChanged(*this, pattern);  // should always go before removeComponent so we can run onRemove method
    }
    world.mComponentManager->removeComponent<T>(*this);
    return *this;
}

template <typename T>
std::optional<T> Entity::tryGet() const {
    return World::getInstance().mComponentManager->tryGetComponent<T>(*this);
}

template <typename T>
T& Entity::get() const {
    return World::getInstance().mComponentManager->getComponent<T>(*this);
}

template <typename T>
bool Entity::has() const {
    return World::getInstance().mEntityManager->getPattern(*this).test(ComponentManager::getComponentID<T>());
}

template <typename... T>
void Entity::forChild(void(callback)(ecs::Entity, T...), bool isRecursive, T... args) const {
    if (isRecursive) {
        for (const Entity& child : World::getInstance().mEntityManager->parentToChildren[*this]) {
            callback(child, args...);
            child.forChild(callback, true, args...);
        }
    } else {
        for (const Entity& child : World::getInstance().mEntityManager->parentToChildren[*this]) {
            callback(child, args...);
        }
    }
}

}  // namespace whal::ecs
