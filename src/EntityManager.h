#pragma once

#include <mutex>
#include <queue>
#include <string>
#include "ECS.h"

namespace whal::ecs {
class EntityManager : public IEntityManager {
public:
    EntityManager();

    Entity createEntity(bool isAlive, Entity parent);
    void destroyEntity(Entity entity);
    void setPattern(Entity entity, const Pattern& pattern);
    const Pattern& getPattern(Entity entity) const;
    Pattern& getPatternMut(Entity entity);
    void setTagPattern(Entity entity, const Pattern& pattern);
    const Pattern& getTagPattern(Entity entity) const;
    Pattern& getTagPatternMut(Entity entity);
    u32 getEntityCount() const { return mEntityCount; }
    u32 getActiveEntityCount() const;
    bool isActive(Entity entity) const override;

    void setEntityName(Entity entity, const char* name);
    void setEntityName(Entity entity, std::string_view name);
    const char* getEntityName(Entity entity);

    // returns true if entity was activated, false if it was already active
    bool activate(Entity entity);
    bool deactivate(Entity entity);

    const std::unordered_set<Entity, EntityHash>& getChildren(Entity e) override;

    std::unordered_map<Entity, Entity, EntityHash> childToParent;
    std::unordered_map<Entity, std::unordered_set<Entity, EntityHash>, EntityHash> parentToChildren;

private:
    std::queue<EntityID> mAvailableIDs;
    std::array<Pattern, MAX_ENTITIES> mPatterns;
    std::array<Pattern, MAX_ENTITIES> mTagPatterns;
    DynamicBitset mActiveEntities;
    std::mutex mCreatorMutex;
    std::unordered_map<EntityID, std::string> mEntityNames;
    u32 mEntityCount = 0;
};

}  // namespace whal::ecs
