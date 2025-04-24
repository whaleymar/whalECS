#include "Serializer.h"

#include <sstream>
#include "ECS.h"

namespace whal::ecs {

std::string toString(Entity e) {
    // might want unique handling for tags? There no need for them to have non-null callbacks in the serializer
    std::stringstream result;
    result << "Entity::" << e.name() << std::endl;
    e.forTrait<Serialize>(
        [](Entity self, Entity cmp, std::stringstream& result) {
            if (cmp.has<internal::Tag>()) {
                result << "Tag::" << cmp.name() << std::endl;
            } else {
                result << "Component::" << cmp.name() << std::endl;
                result << cmp.get<Serialize>().ser(self) << std::endl;
                result << "/Component::" << cmp.name() << std::endl;
            }
        },
        result);

    result << "/Entity::" << e.name() << std::endl;

    return result.str();
}

Entity fromString(std::string data) {
    Entity e = World::getInstance().entity(false);
    // Tasks:
    // 1) parse entity name
    // 2) iterate components and extract their serialized string (if present)
    // 3) from the string name of the component, get the MetaEntity using World.lookup
    // 4) Serialize* serde = cmp.tryGet<Serialize>(); if (!serde) continue; serde.de(e, data);
    assert(false && "Not implemented");
    e.activate();
    return e;
}

}  // namespace whal::ecs
