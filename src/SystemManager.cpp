#include "ECS.h"

namespace whal::ecs {

void SystemManager::clear() {
    for (SystemBase* sys : mSystems) {
        sys->getEntitiesVirtual().clear();
        delete sys;
    }
    for (auto it = mSystemIdToIndex.begin(); it != mSystemIdToIndex.end(); ++it) {
        *it = -1;
    }
    mSystems.clear();
    mUpdateSystems.clear();
    mMonitorSystems.clear();
    mAttributes.clear();
    mUpdateGroups.clear();
    mFrame = 0;
    mIsWorldPaused = false;
}

void SystemManager::autoUpdate() {
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

void SystemManager::onEntityDestroyed(const Entity entity) const {
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

static void tryRemoveChild(Entity entity, SystemBase* system, IMonitorSystem* pMonitor) {
    if (entity.has<OverrideAttributeIgnoreChildren>()) {
        return;
    }
    auto it = system->getEntitiesVirtual().find(entity.id());
    if (it != system->getEntitiesVirtual().end()) {
        if (pMonitor)
            pMonitor->onRemove(entity);
        system->getEntitiesVirtual().erase(it);
        entity.forChild(tryRemoveChild, false, system, pMonitor);
    }
}

void SystemManager::checkIfInSystem(Entity entity, SystemBase* system, size_t i, const Pattern& newEntityPattern, const Pattern& newTagPattern,
                                    bool isEntityInSystem) const {
    bool isExcluded = (mAttributes[i] & ExcludeChildren) > 0 && !entity.has<OverrideAttributeIgnoreChildren>() && system->isMatch(entity.parent());
    bool isPatternMatch = system->isPatternInSystem(newEntityPattern, newTagPattern);
    if (isPatternMatch && !isExcluded) {
        if (isEntityInSystem) {
            return;
        }
        assert((!((mAttributes[i] & Attributes::UniqueEntity) > 0) || system->getEntitiesVirtual().size() < 1) &&
               "Trying to assign more than one entity to system with UniqueEntity attribute");
        system->getEntitiesVirtual().insert({entity.id(), entity});
        if (mMonitorSystems[i] != nullptr) {
            mMonitorSystems[i]->onAdd(entity);
        }

        if ((mAttributes[i] & ExcludeChildren) > 0) {
            // now that this entity is added, make sure its children aren't in this system
            entity.forChild(tryRemoveChild, false, system, mMonitorSystems[i]);
        }
    } else if (isEntityInSystem) {
        if (mMonitorSystems[i] != nullptr) {
            mMonitorSystems[i]->onRemove(entity);
        }
        system->getEntitiesVirtual().erase(entity.id());
    }
}

void SystemManager::onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern, const Pattern& newTagPattern) const {
    // TODO make thread safe
    for (size_t i = 0; i < mSystems.size(); i++) {
        SystemBase* system = mSystems[i];
        auto const it = system->getEntitiesVirtual().find(entity.id());
        checkIfInSystem(entity, system, i, newEntityPattern, newTagPattern, it != system->getEntitiesVirtual().end());
    }
}

void SystemManager::onEntityParentChanged(const Entity entity) {
    for (size_t i = 0; i < mSystems.size(); i++) {
        SystemBase* system = mSystems[i];
        if ((mAttributes[i] & ExcludeChildren) == 0) {
            continue;
        }
        auto const it = system->getEntitiesVirtual().find(entity.id());
        checkIfInSystem(entity, system, i, entity.getPattern(), entity.getTagPattern(), it != system->getEntitiesVirtual().end());
    }
}

void SystemManager::onPaused() {
    mIsWorldPaused = true;
}

void SystemManager::onUnpaused() {
    mIsWorldPaused = false;
}

}  // namespace whal::ecs
