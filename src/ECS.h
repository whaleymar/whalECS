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
using Pattern = DynamicBitset;
using SystemId = u16;

class Entity;
class World;
struct EntityHash;
using EntityCallback = void (*)(Entity);
using EntityPairCallback = void (*)(Entity, Entity);
using ComponentList = std::vector<Entity>;

namespace internal {

struct Component {};  // marks an entity as a Component
struct Tag {};        // marks entity as a Tag component (these entities will also have the Component tag for convenience)

}  // namespace internal

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

    Entity copy(bool isActive = true) const;

    void activate() const;
    void deactivate() const;
    void kill() const;
    bool isValid() const;
    bool isKilledThisFrame() const;

    void addChild(Entity child) const;
    Entity createChild(bool isActive = true) const;
    Entity createChild(const char* name, bool isActive = true) const;
    void orphan() const;  // makes mRootEntity the parent of `e`

    template <typename... T>
    void forChild(void(callback)(ecs::Entity, T...), bool isRecursive, T... args) const;

    Entity parent() const;                                           // parent getter
    const std::unordered_set<Entity, EntityHash>& children() const;  // children getter
    const char* name() const;
    void setName(const char* name) const;
    void setName(std::string_view name) const;

    ComponentList getComponents() const;

private:
    EntityID mId = 0;

    void removeFromMgr(const World& world, ComponentType t);
    void removeTagFromMgr(const World& world, ComponentType t);
    void addToMgr(const World& world, ComponentType t);
    void addTagToMgr(const World& world, ComponentType t);
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
    T* tryGetComponent(const Entity entity) const {
        const long ix = getIndex<T>();
        if (ix == -1) {
            return nullptr;
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
    // requires(!is_base_of_template<Exclude, T>::value && !std::is_empty_v<T>)
        requires(!is_base_of_template<Exclude, T>::value)
    static inline ComponentType getComponentID() {
        static ComponentType id = ComponentID++;
        return id;
    }

    template <typename T>
    // requires(is_base_of_template<Exclude, T>::value && !std::is_empty_v<T>)
        requires(is_base_of_template<Exclude, T>::value)
    static inline ComponentType getComponentID() {
        return T::COMPONENT_TYPE;
    }

    // use a separate ID for tags
    static inline ComponentType TagComponentID = 0;
    template <typename T>
    static inline ComponentType getTagID() {
        if constexpr (is_base_of_template<Exclude, T>::value) {
            return T::COMPONENT_TYPE;
        } else {
            static ComponentType id = TagComponentID++;
            return id;
        }
    }

    u32 getRegisteredCount() const { return mComponentArrays.size(); }

    template <typename T>
        requires(!std::is_empty_v<T>)
    Entity getComponentEntity() {
        return mComponentEntities[getComponentID<T>()];
    }

    Entity getComponentEntity(ComponentType type) { return mComponentEntities[type]; }

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

    template <typename T>
    long getIndex() const {
        const ComponentType type = getComponentID<T>();
        return mComponentToIndex[type];
    }

    std::array<long, MAX_COMPONENTS> mComponentToIndex;
    std::array<Entity, MAX_COMPONENTS> mComponentEntities;
    std::array<Entity, MAX_COMPONENTS> mTagEntities;
    std::vector<IComponentArray*> mComponentArrays;
};

// wrapper type which tells a system that the entity should *not* have this component
template <typename T>
class Exclude {
public:
    inline static ComponentType COMPONENT_TYPE = std::is_empty_v<T> ? ComponentManager::getTagID<T>() : ComponentManager::getComponentID<T>();
};

class SystemBase {
public:
    friend SystemManager;
    virtual ~SystemBase() = default;

    virtual std::unordered_map<EntityID, Entity>& getEntitiesVirtual() = 0;  // only used by SystemManager
    virtual bool isPatternInSystem(const Pattern& pattern, const Pattern& tagPattern) = 0;
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
        mPattern.resize(MAX_COMPONENTS);
        mAntiPattern.resize(MAX_COMPONENTS);
        mTagPattern.resize(MAX_COMPONENTS);
        mTagAntiPattern.resize(MAX_COMPONENTS);
        InitializeIDs<T...>();
    }

    std::unordered_map<EntityID, Entity>& getEntitiesVirtual() override { return mEntities; }
    static std::unordered_map<EntityID, Entity>& getEntitiesMutable() { return mEntities; }
    static std::unordered_map<EntityID, Entity> getEntitiesCopy() { return mEntities; }
    static const std::unordered_map<EntityID, Entity>& getEntities() { return mEntities; }
    static Entity first() { return mEntities.begin()->second; }
    bool isPatternInSystem(const Pattern& pattern, const Pattern& tagPattern) override {
        return (pattern & mPattern) == mPattern && (pattern & mAntiPattern).all_zero() && (tagPattern & mTagPattern) == mTagPattern &&
               (tagPattern & mTagAntiPattern).all_zero();
    }

private:
    // need a First and Second, otherwise there is ambiguity when there's only one
    // element (InitializeIDs<type> vs InitializeIDs<type, <>>)
    template <typename First, typename Second, typename... Rest>
    void InitializeIDs() {
        if constexpr (is_base_of_template<Exclude, First>::value) {
            if constexpr (std::is_empty_v<First>) {
                mTagAntiPattern.set(ComponentManager::getTagID<First>());
            } else {
                mAntiPattern.set(ComponentManager::getComponentID<First>());
            }
        } else {
            if constexpr (std::is_empty_v<First>) {
                mTagPattern.set(ComponentManager::getTagID<First>());
            } else {
                mPattern.set(ComponentManager::getComponentID<First>());
            }
        }
        InitializeIDs<Second, Rest...>();
    }

    template <typename Last>
    void InitializeIDs() {
        if constexpr (is_base_of_template<Exclude, Last>::value) {
            if constexpr (std::is_empty_v<Last>) {
                mTagAntiPattern.set(ComponentManager::getTagID<Last>());
            } else {
                mAntiPattern.set(ComponentManager::getComponentID<Last>());
            }
        } else {
            if constexpr (std::is_empty_v<Last>) {
                mTagPattern.set(ComponentManager::getTagID<Last>());
            } else {
                mPattern.set(ComponentManager::getComponentID<Last>());
            }
        }
    }

    inline static std::unordered_map<EntityID, Entity> mEntities = {};
    Pattern mPattern;
    Pattern mAntiPattern;
    Pattern mTagPattern;
    Pattern mTagAntiPattern;
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
    void onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern, const Pattern& newEntityTagPattern) const;
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
            if (mComponentManager->getIndex<T>() == -1) {
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

template <typename... T>
void Entity::forChild(void(callback)(ecs::Entity, T...), bool isRecursive, T... args) const {
    if (isRecursive) {
        for (const Entity& child : World::getInstance().mEntityManager->getChildren(*this)) {
            callback(child, args...);
            child.forChild(callback, true, args...);
        }
    } else {
        for (const Entity& child : World::getInstance().mEntityManager->getChildren(*this)) {
            callback(child, args...);
        }
    }
}

template <typename T>
    requires(!std::is_empty_v<T>)
void ComponentManager::registerComponent() {
    const ComponentType type = getComponentID<T>();
    assert(type < MAX_COMPONENTS && "Registered more than MAX_COMPONENTS components");
    assert(getIndex<T>() == -1 && "Component type already registered");
    mComponentToIndex[type] = mComponentArrays.size();
    mComponentArrays.push_back(new ComponentArray<T>());

    // not passing name here because I need to include string to parse the string_view and I'd rather do that in a source file.
    Entity e = World::getInstance().componentEntity(type);
    e.setName(type_of<T>());
    mComponentEntities[type] = e;
    e.add<internal::Component>();
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
    e.add<internal::Component>();
    e.add<internal::Tag>();
}

}  // namespace whal::ecs
