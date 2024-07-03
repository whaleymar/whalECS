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

- onModify virtual method for IMonitor systems: safer way to call entity.set(), because it currently circumvents the onRemove pattern I have, causing memory leaks for things with VAOs/VBOs/other manually deallocated stuff
- thread-safe system methods
