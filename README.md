# Whaley's Entity Component System
This code is heavily based on this writeup: https://austinmorlan.com/posts/entity_component_system/

The main changes are:

1. Does not rely on RTTI to make unique IDs for components/systems. Uses compile-time type IDs instead
2. Single header (plus some cpp files) for an improved API. Now components can be added with `entity.add<Transform>()` (default constructor) or `entity.add(transform)`. 
3. Systems now have virtual onAdd and onDelete functions, which is my solution to running non-trivial constructors/destructors when components are created/destroyed.

## Constraints

1. Components need a default constructor and be copyable (you can still use other constructors for initialization)
2. Components need a unique type (after name mangling -> aliases aren't unique)
3. Systems need a default constructor
4. Cannot store pointers to components. Components are densely packed in arrays, so a deleted entity may make the pointer invalid

## TODO

### Need:

- safer way to call entity.set(), because it currently circumvents the onRemove pattern I have, causing memory leaks for things with VAOs/VBOs/other manually deallocated stuff
    - when entity.set() called, should call onRemove and onAdd for all systems i guess. Or just remove .set?
- thread-safe system methods
- auto-call systems
    - system scheduler (every X frames / ongamestart / ongameend) + chaining (run sys1, then sys2; sys3 no dependencies so can thread)

### Nice to have

- max entities/max components values should be macros which are optionally defined before the first time ECS.h is included, similar to rapidXML
