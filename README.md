# Whaley's Entity Component System
This code is heavily based on this writeup: https://austinmorlan.com/posts/entity_component_system/

The main changes are:

1. Does not rely on RTTI to make unique IDs for components/systems. Uses compile-time type IDs instead
2. Single header (plus some cpp files) for an improved API. Now components can be added with `entity.add<Transform>()` (default constructor) or `entity.add(transform)`. 
3. Several Attributes/Interfaces for Systems to inherit for complex behavior
4. Automatic schedule system updates

## Constraints

1. Components need a default constructor and be copyable (you can still use other constructors for initialization)
2. Components need a unique type (after name mangling -> aliases aren't unique)
3. Systems need a default constructor
4. Cannot store pointers to components. Components are densely packed in arrays, so a deleted entity may make the pointer invalid

## TODO

### Need:

- optimize for empty types (ie tags). there's a std::is_empty_type (or something) i can use to dispatch a different method in ecs::World. If the type is empty i only need to update the bitmask (can use a separate one for tags) and don't need to touch the component manager
    - will drastically reduce memory usage of tags -- currently using MAX_ENTITIES bytes per tag (so 5kb w/ defaults), this would reduce it to 1 bit
- thread-safe system methods
- parallelization
- queue add/remove operations until end of frame? so i'm only iterating through the system stuff once. also avoids accidental mutation during update loops
    - con: cannot immediately access components added that frame
- store components in std::vector for "unlimited" growth? Would also prevent allocating MAXENTITIES # of structs for low-use components
    - would require changes to:
        - ComponentArray.add: push_back
        - ComponentArray.remove: pop_back
        - EntityManager::mPatterns: would be a vector & would need slightly more management (id->ix map or something)
        - EntityManager::mActiveEntities: is a bitset, would need to convert to a std::vector<bool>

- *considering* having entity creation return a plain `Entity` instead of an `Expected<Entity>` and just having a `bool Entity::isValid() const {return mId != 0}` method instead. The pro would be slightly easier error checking and more readable code. The con would be that it's easier for the end-user to forget that entity creation could fail
- *considering* having tryGetComponent return a pointer instead of an `Optional<T>` so that manipulating the queried value is easier, rather than needing to call .set
