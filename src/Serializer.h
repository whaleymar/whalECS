#pragma once

#include <string>

namespace whal::ecs {

class Entity;

// Meta-Component for serializing other components.
// To make a component serializable, it should be added like this:
// `World.component<T>().add<Serialize>({
//      .ser = [](ecs::Entity e) -> std::string {...},
//      .de = [](ecs::Entity e, std::string data) {...},
// })`
// Where the lambdas are custom code to read/write the component data.
// For tags (empty types) .ser can be null.
struct Serialize {
    using SerializeCallback = std::string (*)(Entity);
    using DeserializeCallback = void (*)(Entity, std::string);

    SerializeCallback ser;
    DeserializeCallback de;
};

// output is in a dumb custom format that looks like this:
/*
Component::Transform
{transform serialization here. Each component defines their own ser/de scheme}
/Component::Transform
Component::Velocity
...etc
*/
std::string toString(ecs::Entity e);

Entity fromString(std::string data);

}  // namespace whal::ecs
