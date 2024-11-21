#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <concepts>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Expected.h"
#include "Traits.h"

typedef uint16_t u16;
typedef uint32_t u32;

namespace whal::gfx {
struct EntityRenderInfo;
struct RenderContext;
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
using EntityDeathCallback = void (*)(Entity);

class Entity {
public:
    Entity() = default;
    Entity(EntityID id);
    friend EntityManager;
    friend SystemManager;
    friend Expected<Entity>;  // create dummy id on error
    EntityID id() const { return mId; }

    EntityID operator()() const { return mId; }
    bool operator==(Entity other) const { return mId == other.mId; }
    bool operator<(Entity other) const { return mId < other.mId; }

    template <typename T>
    Entity add(T component);

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

    Expected<Entity> copy(bool isActive = true) const;

    void activate() const;
    void deactivate() const;
    void kill() const;

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
    // TODO
    // static_assert(std::is_trivial_v<T>, "Component is not trivial type (see
    // https://en.cppreference.com/w/cpp/language/classes#Trivial_class)");
    void addData(const Entity entity, T component) {
        if (hasData(entity)) {
            const u32 ix = mEntityToIndex[entity.id()];
            mComponentTable[ix] = component;
            return;
        }
        const u32 ix = mSize++;
        mEntityToIndex[entity.id()] = ix;
        mIndexToEntity[ix] = entity.id();
        mComponentTable[ix] = component;
    }

    void setData(const Entity entity, T component) {
        assert(mEntityToIndex[entity.id()] != -1 && "cannot set component value without adding it to the entity first");
        mComponentTable[mEntityToIndex[entity.id()]] = component;
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
            addData(dest, *cmpOpt);
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

    Expected<Entity> createEntity(bool isAlive);
    void destroyEntity(Entity entity);
    void setPattern(Entity entity, const Pattern& pattern);
    Pattern getPattern(Entity entity) const;
    u32 getEntityCount() const { return mEntityCount; }
    bool isActive(Entity entity) const;

    // returns true if entity was activated, false if it was already active
    bool activate(Entity entity);
    bool deactivate(Entity entity);

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
    ComponentManager() { mComponentToIndex.fill(-1); }

    template <typename T>
    void registerComponent() {
        const ComponentType type = getComponentID<T>();
        assert(type < MAX_COMPONENTS && "Registered more than MAX_COMPONENTS components");
        assert(getIndex<T>() == -1 && "Component type already registered");
        mComponentToIndex[type] = mComponentArrays.size();
        mComponentArrays.push_back(std::make_unique<ComponentArray<T>>());
    }

    template <typename T>
    ComponentType getComponentType() const {
        return getComponentID<T>();
    }

    template <typename T>
    void addComponent(const Entity entity, T component) {
        long index = getIndex<T>();
        if (index == -1) {
            registerComponent<T>();
            index = mComponentArrays.size() - 1;
        }
        getComponentArray<T>(index)->addData(entity, component);
    }

    template <typename T>
    void setComponent(const Entity entity, T component) {
        getComponentArray<T>(getIndex<T>())->setData(entity, component);
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

private:
    template <typename T>
    ComponentArray<T>* getComponentArray(int ix) const {
        return static_cast<ComponentArray<T>*>(mComponentArrays[ix].get());
    }

    template <typename T>
    long getIndex() const {
        const ComponentType type = getComponentID<T>();
        return mComponentToIndex[type];
    }

    std::array<long, MAX_COMPONENTS> mComponentToIndex;
    std::vector<std::unique_ptr<IComponentArray>> mComponentArrays;
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
    virtual void draw(const gfx::EntityRenderInfo& entityInfo, const gfx::RenderContext& ctx) const = 0;
    virtual void addToQueue(std::vector<gfx::EntityRenderInfo>& queue) const = 0;
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

        return static_cast<T*>(mSystems[mSystemIdToIndex.at(id)].get());
    }

    template <class T>
    T* registerSystem(u16 attributes = 0) {
        static_assert(is_base_of_template<ISystem, T>::value, "Cannot register class which doesn't inherit ISystem");
        const SystemId id = getSystemID<T>();
        assert(!mSystemIdToIndex.contains(id) && "System already registered");

        mSystemIdToIndex.insert({id, mSystems.size()});

        std::unique_ptr<T> system = std::make_unique<T>();

        // The way these two interfaces are used, it's convenient for these list's indices to match with mSystems
        mUpdateSystems.push_back(toInterfacePtr<T, IUpdate>(system.get()));
        mMonitorSystems.push_back(toInterfacePtr<T, IMonitorSystem>(system.get()));

        // Check other interfaces. These lists don't store nullptrs;
        if (auto iPtr = toInterfacePtr<T, IReactToPause>(system.get()); iPtr) {
            mPauseSystems.push_back(iPtr);
        }
        if (auto iPtr = toInterfacePtr<T, IRender>(system.get()); iPtr) {
            mRenderSystems.push_back({iPtr, system.get()});
        }

        // check attributes
        if (toInterfacePtr<T, AttrUniqueEntity>(system.get())) {
            attributes |= UniqueEntity;
        }
        if (toInterfacePtr<T, AttrUpdateDuringPause>(system.get())) {
            attributes |= UpdateDuringPause;
        }
        mAttributes.push_back(attributes);

        T* rawPtr = system.get();
        mSystems.push_back(std::move(system));
        return rawPtr;
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

    void clear() {
        for (auto& sys : mSystems) {
            sys->getEntitiesVirtual().clear();
        }
        mSystemIdToIndex.clear();
        mSystems.clear();
        mUpdateSystems.clear();
        mMonitorSystems.clear();
        mPauseSystems.clear();
        mRenderSystems.clear();
        mAttributes.clear();
        mUpdateGroups.clear();
        mFrame = 0;
        mIsWorldPaused = false;
    }

    void autoUpdate() {
        for (auto& [groupInfo, group] : mUpdateGroups) {
            // eventually if isParallel then do in parallel RESEARCH
            if (mFrame % groupInfo.intervalFrame != 0) {
                continue;
            }
            for (auto ix : group) {
                if (mIsWorldPaused && (mAttributes[ix] & UpdateDuringPause) == 0) {
                } else {
                    mUpdateSystems[ix]->update();
                }
            }
        }
        mFrame++;
    }

    void onEntityDestroyed(const Entity entity) const {
        // TODO make thread safe
        for (size_t i = 0; i < mSystems.size(); i++) {
            const auto& system = mSystems[i];
            auto const ix = system->getEntitiesVirtual().find(entity.id());
            if (ix != system->getEntitiesVirtual().end()) {
                system->getEntitiesVirtual().erase(entity.id());
                if (mMonitorSystems[i] != nullptr) {
                    mMonitorSystems[i]->onRemove(entity);
                }
            }
        }
    }

    void onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern) const {
        // TODO make thread safe
        for (size_t i = 0; i < mSystems.size(); i++) {
            auto const ix = mSystems[i]->getEntitiesVirtual().find(entity.id());
            if (mSystems[i]->isPatternInSystem(newEntityPattern)) {
                if (ix != mSystems[i]->getEntitiesVirtual().end()) {
                    // already in system
                    continue;
                }
                assert((!((mAttributes[i] & Attributes::UniqueEntity) > 0) || mSystems[i]->getEntitiesVirtual().size() < 1) &&
                       "Trying to assign more than one entity to system with UniqueEntity attribute");
                mSystems[i]->getEntitiesVirtual().insert({entity.id(), entity});
                if (mMonitorSystems[i] != nullptr) {
                    mMonitorSystems[i]->onAdd(entity);
                }
            } else if (ix != mSystems[i]->getEntitiesVirtual().end()) {
                if (mMonitorSystems[i] != nullptr) {
                    mMonitorSystems[i]->onRemove(entity);
                }
                mSystems[i]->getEntitiesVirtual().erase(entity.id());
            }
        }
    }

    void onPaused() {
        assert(!mIsWorldPaused);

        mIsWorldPaused = true;
        for (auto pSystem : mPauseSystems) {
            pSystem->onPause();
        }
    }

    void onUnpaused() {
        assert(mIsWorldPaused);

        mIsWorldPaused = false;
        for (auto pSystem : mPauseSystems) {
            pSystem->onUnpause();
        }
    }

    const std::vector<RenderSystemPair>& getRenderSystems() const { return mRenderSystems; }

private:
    // assign unique IDs to each system type
    static inline SystemId SystemID = 0;
    template <class T>
    static inline SystemId getSystemID() {
        static SystemId id = SystemID++;
        return id;
    }

    std::unordered_map<SystemId, int> mSystemIdToIndex;
    std::vector<std::unique_ptr<SystemBase>> mSystems;
    std::vector<IUpdate*> mUpdateSystems;          // may contain null ptrs
    std::vector<IMonitorSystem*> mMonitorSystems;  // may contain null ptrs
    std::vector<IReactToPause*> mPauseSystems;
    std::vector<RenderSystemPair> mRenderSystems;
    std::vector<u16> mAttributes;

    std::vector<std::pair<UpdateGroupInfo, std::vector<int>>>
        mUpdateGroups;  // ordered list of lists, where each list is 1+ systems which need to be updated sequentially
    int mFrame = 0;
    bool mIsWorldPaused = false;
};

class World {
public:
    inline static World& getInstance() {
        static World instance;
        return instance;
    }

    // ENTITY
    Expected<Entity> entity(bool isAlive = true) const;
    void kill(Entity entity);
    void killEntities();  // called by update. Should only be called manually in specific circumstances like scene loading

    Expected<Entity> copy(Entity entity, bool isActive) const;
    void activate(Entity entity) const;
    void deactivate(Entity entity) const;

    u32 getEntityCount() const;
    void setEntityDeathCallback(EntityDeathCallback callback);

    // COMPONENT
    template <typename T>
    void addComponent(const Entity entity, T component) {
        mComponentManager->addComponent(entity, component);

        auto pattern = mEntityManager->getPattern(entity);
        pattern.set(mComponentManager->getComponentType<T>(), true);
        mEntityManager->setPattern(entity, pattern);

        if (isActive(entity)) {
            mSystemManager->onEntityPatternChanged(entity, pattern);  // should always go after addComponent so onAdd can run w/out errors
        }
    }

    template <typename T>
    void setComponent(const Entity entity, T component) {
        mComponentManager->setComponent(entity, component);
    }

    template <typename T>
    void removeComponent(const Entity entity) {
        auto pattern = mEntityManager->getPattern(entity);
        pattern.set(mComponentManager->getComponentType<T>(), false);
        mEntityManager->setPattern(entity, pattern);
        if (isActive(entity)) {
            mSystemManager->onEntityPatternChanged(entity, pattern);  // should always go before removeComponent so we can run onRemove method
        }
        mComponentManager->removeComponent<T>(entity);
    }

    template <typename T>
    bool hasComponent(const Entity entity) {
        return mEntityManager->getPattern(entity).test(mComponentManager->getComponentType<T>());
    }

    template <typename T>
    std::optional<T> tryGetComponent(const Entity entity) const {
        return mComponentManager->tryGetComponent<T>(entity);
    }

    template <typename T>
    T& getComponent(const Entity entity) const {
        return mComponentManager->getComponent<T>(entity);
    }

    template <typename T>
    ComponentType getComponentType() const {
        return mComponentManager->getComponentType<T>();
    }

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

    // this doesn't do anything, but I want the caller code to be understandable
    SystemManager& BeginSystemRegistration() const { return *mSystemManager.get(); }

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
    World(const World&) = delete;
    void operator=(const World&) = delete;

    // is private because it's a bad idea to use this in game logic. An entity's ID could be recycled at any time
    bool isActive(Entity entity) const;

    std::unique_ptr<EntityManager> mEntityManager;
    std::unique_ptr<ComponentManager> mComponentManager;
    std::unique_ptr<SystemManager> mSystemManager;
    std::unordered_set<Entity, EntityHash> mToKill;
    EntityDeathCallback mDeathCallback = nullptr;
};

template <typename T>
Entity Entity::add(T component) {
    World::getInstance().addComponent<T>(*this, component);
    return *this;
}

template <typename T>
Entity Entity::add() {
    World::getInstance().addComponent<T>(*this, T());
    return *this;
}

template <typename T>
Entity Entity::set(T component) const {
    World::getInstance().setComponent<T>(*this, component);
    return *this;
}

template <typename T>
Entity Entity::remove() {
    World::getInstance().removeComponent<T>(*this);
    return *this;
}

template <typename T>
std::optional<T> Entity::tryGet() const {
    return World::getInstance().tryGetComponent<T>(*this);
}

template <typename T>
T& Entity::get() const {
    return World::getInstance().getComponent<T>(*this);
}

template <typename T>
bool Entity::has() const {
    return World::getInstance().hasComponent<T>(*this);
}

}  // namespace whal::ecs
