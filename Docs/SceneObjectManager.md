# Central Scene Object Manager

This feature addresses a different workflow from the AudioLoom deprecation audit: **manage AudioLoom/SpatialFabric properties for all objects in an editor scene from one place**.

The scene list includes ordinary objects with no SpatialFabric capability. Select one or more objects, explicitly add the appropriate capability, then edit the same stored properties that their normal Inspector/Details exposes. It is not live property linking between objects, general transform editing or routing between engine instances.

## Design boundaries

See [Scene Object Manager v1](../Specs/Scene-Object-Manager-v1.md) for scope, Undo/Redo, mixed-value and no-implicit-I/O requirements.

- Unreal uses actor components for playback/live input and manager-owned records for spatial bindings.
- Unity uses scene GameObject components.
- Godot uses typed child nodes owned by the edited scene.

A component/child-node add is an explicit authoring action, not permission to start audio, capture or sockets. Existing runtime/startup opt-ins remain distinct. Protected instances, locked levels and play-mode mutation require native engine restrictions; unloaded scene content is not silently loaded.

## Open the manager

| Engine | Editor entry point |
| --- | --- |
| Unreal | Window → Spatial Fabric → Scene Objects |
| Unity | Tools → SpatialFabric → Open Window → Scene Objects |
| Godot | SpatialFabric editor dock → Scene Objects |

Recompile/reload the editor integration after updating its source. Engine-specific supported capability types and validation evidence are documented in each integration's `Docs/SceneObjectManager.md`.

## Quick authoring check

1. Place two ordinary objects, leaving one hidden/inactive. Confirm both appear without first adding SpatialFabric.
2. Select both and explicitly add Playback. Repeat the action: no duplicate capability should appear.
3. Give their loop settings different values in normal Inspector/Details. The central multi-edit should indicate a mixed value rather than choosing the first object's value.
4. Set a common value centrally, then inspect each original object. Undo/Redo should restore/reapply both values together.
5. Undo the add as a separate history step, then Redo and save/reopen the scene. Components/owned child nodes and values should persist.
6. Confirm that no audio, capture or network start was required. Try a protected object or play-mode mutation: the authoring surface should refuse it with a reason.

Graphical layout and keyboard usability remain a separate manual check from headless regression tests.

## Local validation (2026-11-18)

- **Unreal 5.8:** isolated Win64 Editor Development, Game Development and Game Shipping builds and **44 native tests passed** (43 clean, one existing SoundWave warning). Includes initialized-manager callback/Undo transport preservation, indexed actor status/identity replacement, and the [Audio Routing matrix](AudioRoutingMatrix.md), with real per-user Slate capture/release and stopped-on-Undo regression coverage. Evidence: `C:/Temp/SFUnrealRouting-final-clipping/Report/index.json`. Graphical Details mixed-value and native map save/reopen acceptance remain manual; later frame-driven networking behavior is unchanged.
- **Unity 6000.6.9f1:** isolated graphics-enabled suite, **139 EditMode + 6 PlayMode passed**, zero skips/failures, native tests required. Includes viewport virtualization, cached display snapshots, fresh mutation guards, selection/Undo and actual-window live-binding/mouse-input regressions. The Unity panel follows Hierarchy selection by default, opens real component properties automatically, and supports explicit scene refresh; manual grid batches can opt out of following. Operator tabs also auto-find existing managers and browse loaded unbound/unassigned components, with collapsible object/property sections. Outputs defaults to a true crosspoint matrix (also available in Unreal and Godot): cue rows, grouped interface/channel columns, configured-route checks and two-axis scrolling. Interface/channel dropdowns are available as an alternative and on expanded Playback cues, with explicit Refresh Interfaces. Cached offline layouts and guarded Undo assignments remain separate from OSC destinations. Real-grid click/Undo, ancestor clipping, dropdown rendering/channel choices and simulated unplug regressions pass. Evidence: `C:/Temp/SFSceneObjectManager-20261118/true-crosspoint-clipping/validation.json`.
- **Godot 4.7.2:** **149 editor assertions + 308 native-required runtime checks + 22 Python tests passed**, clean headless editor shutdown. Covers typed controls, mixed drafts, per-target node references, atomic validation, ownership/save-reload, sibling-order Undo, hash-indexed filtering and the routing grid/cache/selectors. Evidence: `C:/dev/GodotRouting-20261118-reviewed149-bounddraft/` (`editor-tests.json` and `runtime-tests.json`).

Engine-specific documents list remaining manual usability and protected-instance restrictions. Source publication does not certify physical audio/hotplug or refresh previously built release archives; local test artifacts are not public release certificates.
