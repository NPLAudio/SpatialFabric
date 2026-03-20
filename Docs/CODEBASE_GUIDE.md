# SpatialFabric — codebase guide

This document explains **how the plugin fits together** before you dive into C++ files. Read it once, then use file-top comments in the source as a map.

## What SpatialFabric does (one sentence)

It reads **Unreal actor positions** inside a **stage box**, converts them to **normalized coordinates**, and sends **OSC messages** to external spatial audio systems (ADM-OSC, or your own templates).

## Main ideas (no jargon)

| Idea | Meaning |
|------|--------|
| **Binding** | “Track *this* actor and treat it as spatial object #3 (for example).” |
| **Stage volume** | A box in the level: center = (0,0,0) in normalized space; edges = ±1. |
| **Snapshot** | One frame’s worth of all tracked positions + optional listener data. |
| **Adapter** | Code that turns snapshot data into a specific protocol (e.g. `/adm/obj/1/xyz`). |
| **Router** | Sends each adapter only the objects that are routed to that adapter. |

## Data flow (every frame)

1. **`ASpatialFabricManagerActor::Tick`** runs (or the editor tickable calls **`ProcessFrame`** without PIE).
2. **`USpatialObjectRegistry::BuildSnapshot`** walks **`ObjectBindings`**, resolves each **`TargetActor`**, and fills **`FSpatialFrameSnapshot`** using **`ASpatialStageVolume`** for normalization.
3. **`FProtocolRouter::ProcessFrame`** filters the snapshot per adapter type, then calls **`ISpatialProtocolAdapter::ProcessFrame`** on ADM-OSC and Custom adapters.
4. Adapters use **`USpatialOSCClientComponent`** to send UDP OSC to **`TargetIP:TargetPort`** from **`FSpatialAdapterConfig`**.

## Suggested reading order (C++)

1. **`SpatialFabricTypes.h`** — all structs/enums; everything else references these.
2. **`ISpatialProtocolAdapter.h`** — the “plugin point” for new protocols.
3. **`ProtocolRouter.h` / `.cpp`** — how objects are filtered per adapter.
4. **`SpatialObjectRegistry`** — actor → normalized state.
5. **`SpatialStageVolume`** — world position → [-1, 1] and meters.
6. **`SpatialFabricManagerActor`** — wires components, tick, adapters.
7. **`SpatialMath`** — polar/Cartesian helpers for OSC.
8. **`Adapters/ADMOSCAdapter`**, **`Adapters/CustomAdapter`** — wire format details.
9. **`SpatialOSCClientComponent` / `SpatialOSCServerComponent`** — UE OSC plugin wrappers.
10. **`SpatialFabricEditorModule`** + **`SSpatialFabricPanel`** — editor UI (large file; use section comments).

## Unreal Engine concepts you’ll see

- **`UCLASS` / `USTRUCT` / `UPROPERTY`** — reflection; Editor and Blueprints can edit or read these.
- **`AActor`** — anything placed in a level (your stage volume, manager, tracked props).
- **`UActorComponent`** — logic attached to an actor (registry, OSC client/server).
- **`TSoftObjectPtr`** — reference that may not be loaded; used for picking actors in the editor.
- **Game thread** — all of this runs on the game thread; adapters are not thread-safe for background use.

## Where settings live

- **`USpatialFabricSettings`** — project defaults (ports, IPs, debug flags): **Project Settings → Plugins → Spatial Fabric**, stored in config such as **`DefaultSpatialFabric.ini`**.
- **`FSpatialAdapterConfig`** on the manager — per-format IP, port, send rate, coordinate mode, etc.

## Adding a new protocol (high level)

1. Add a value to **`ESpatialAdapterType`** in **`SpatialFabricTypes.h`** (and any UI/panel wiring).
2. Implement **`ISpatialProtocolAdapter`** (see **`FADMOSCAdapter`** as a full example).
3. Register it in **`ASpatialFabricManagerActor::InitializeAdapters`**.
4. Ensure **`FilterSnapshotForAdapter`** in **`ProtocolRouter.cpp`** still matches binding **`Targets`** to your type (same pattern as Custom).

## Tests

Automation tests live in **`SpatialFabricTests.cpp`**. They validate math and address strings without loading a full level.

---

For line-level explanations, see the comment blocks at the top of each source file, **inline comments** inside functions (especially OSC, router, registry, adapters), and notes above non-trivial blocks.
