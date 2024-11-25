#include "ECS.h"

namespace whal::ecs {

void SystemManager::clear() {
    for (SystemBase* sys : mSystems) {
        sys->getEntitiesVirtual().clear();
        delete sys;
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

void SystemManager::onEntityPatternChanged(const Entity entity, const Pattern& newEntityPattern) const {
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

void SystemManager::onPaused() {
    assert(!mIsWorldPaused);

    mIsWorldPaused = true;
    for (auto pSystem : mPauseSystems) {
        pSystem->onPause();
    }
}

void SystemManager::onUnpaused() {
    assert(mIsWorldPaused);

    mIsWorldPaused = false;
    for (auto pSystem : mPauseSystems) {
        pSystem->onUnpause();
    }
}

}  // namespace whal::ecs
