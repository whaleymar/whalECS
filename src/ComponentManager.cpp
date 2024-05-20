#include "ECS.h"

namespace whal::ecs {

void ComponentManager::entityDestroyed(const Entity entity) {
    for (auto const& componentArray : mComponentArrays) {
        componentArray->entityDestroyed(entity);
    }
}

void ComponentManager::copyComponents(const Entity prefab, Entity dest) {
    for (auto const& componentArray : mComponentArrays) {
        componentArray->copyComponent(prefab, dest);
    }
}

}  // namespace whal::ecs
