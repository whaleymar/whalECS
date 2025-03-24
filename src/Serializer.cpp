#include "Serializer.h"

#include <sstream>
#include "ECS.h"

namespace whal::ecs {

struct StringKV {
    std::string key;
    std::string val;
    bool isTag;
};

std::string toString(ecs::Entity e) {
    std::vector<StringKV> data;
    ComponentList components = e.getComponents();
    // might want unique handling for tags? There no need for them to have non-null callbacks in the serializer
    for (Entity cmp : components) {
        Serialize* serde = cmp.tryGet<Serialize>();
        if (!serde) {
            continue;
        }
        if (cmp.has<internal::Tag>()) {
            data.push_back({cmp.name(), "", true});
        } else {
            data.push_back({cmp.name(), serde->ser(e), false});
        }
    }

    std::stringstream result;
    result << "Entity::" << e.name() << std::endl;
    for (const auto& [key, value, isTag] : data) {
        if (isTag) {
            result << "Tag::" << key << std::endl;
        } else {
            result << "Component::" << key << std::endl;
            result << value << std::endl;
            result << "/Component::" << key << std::endl;
        }
    }
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
