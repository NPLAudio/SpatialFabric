# Scene Object Manager v1

## Purpose

A central **editor authoring** surface for AudioLoom/SpatialFabric settings across Unreal, Unity and Godot. This is not a network-routing matrix, property-link graph, runtime subscription service or general-purpose replacement for the engine's Inspector.

The user can browse ordinary scene objects before adding SpatialFabric functionality, then adjust its properties centrally instead of visiting each object separately. Normal Details/Inspector editing remains available and uses the same stored properties.

## Common behavior

- Enumerate placed objects in the current editor scene scope, including unconfigured objects and hidden/inactive objects. Display scene scope explicitly. Unreal covers loaded actors/levels, not unloaded World Partition descriptors; Unity covers loaded authoring scenes; Godot covers the edited scene tree.
- Do not load unloaded scenes, instantiate assets, include editor-internal objects, or require a runtime/network manager merely to browse.
- Search/filter the list; permit multi-selection. Identify configured objects and missing functionality. Object identity must not depend on display names, which may repeat.
- Offer explicit add actions with the functionality type clearly named. Do not silently add every component, replace an actor, or create a networking manager as a side effect.
- Attach engine-native components in Unreal/Unity. Godot represents these capabilities as typed child nodes owned by the edited scene. Reuse existing compatible functionality rather than adding accidental duplicates.
- Spatial tracking and utility OSC clients/servers are not interchangeable. Unreal's spatial bindings remain manager-owned records; an arbitrary OSC component does not enroll an actor as a spatial object.
- Respect native instance, prefab/inherited-scene, locked-level and read-only restrictions. Explain unsupported targets. Preflight bulk changes and keep accepted changes in a single logical Undo operation.
- Expose only AudioLoom/SpatialFabric properties, using real objects/components and the engine's property mechanisms. Do not maintain an independent settings copy that can diverge from ordinary Inspector/Details values.
- Support common-property multi-editing with mixed values indicated correctly; inspecting a mixed selection must not overwrite its values. Different capability types may have separate property groups. Use native/typed controls for common fields (booleans, numbers, enums, vectors, audio files and node references), honor export constraints and validate every target before a bulk edit. Do not require code-like Variant syntax for routine authoring.
- Keep property selection and unapplied drafts associated with their scene, target identities and property key. Refresh must not silently redirect a pending channel edit into an identity field.
- Refresh safely after scene/object changes and Undo/Redo. Deleted or replaced objects must not leave stale actionable references. Mark affected scenes dirty and retain changes on save/reload.
- This is an edit-mode authoring tool. Disable unsafe mutation during play/simulation and on protected targets. Browsing, selecting and adding inactive capabilities must not start audio, capture, OSC sockets or runtime processing. An authored startup checkbox remains an explicit opt-in for a subsequent runtime session.
- Keep existing playback, CSV, routing, radar and output tools available; integrate the new surface into the existing SpatialFabric editor entry point.

## Acceptance evidence

Each engine should test production enumeration/add/edit helpers, not just a parallel mock:

1. Ordinary objects without any SpatialFabric functionality appear, including hidden/inactive objects and duplicate labels.
2. Single and multiple selected objects receive only the requested missing capability; repeating the add does not duplicate it.
3. One Undo removes the batch; Redo restores valid owned/serialized instances. General engine properties are unchanged.
4. Central edits update the same properties seen by normal Inspector/Details, preserve mixed values until edited and survive save/reload.
5. Invalid, deleted, foreign-scene and protected targets cannot be silently mutated. Play-mode mutation is rejected.
6. Opening the manager requires no audio device, capture permission or network listener.

Automated silent editor tests establish authoring behavior, not visual usability, physical sound, microphone, latency or every platform. Document any remaining manual UI checks and engine-specific scope restrictions.
