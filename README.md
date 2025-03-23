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

- thread-safe system methods
- parallelization
- store components in std::vector for "unlimited" growth? Would also prevent allocating MAXENTITIES # of structs for low-use components
    - would require changes to:
        - ComponentArray.add: push_back
        - ComponentArray.remove: pop_back

### Considering:
- queue add/remove operations until end of frame? so i'm only iterating through the system stuff once. also avoids accidental mutation during update loops
    - con: cannot immediately access components added that frame

