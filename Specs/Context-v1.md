# SpatialFabric scene context, version 1

Read-only reference geometry that Path Studio draws behind your paths so keyframes can be authored in the
context of the room: walls, pillars, speakers, a listener. It is **never written into a path document** and
never changes an engine. Schema: [`Schemas/context-v1.schema.json`](../Schemas/context-v1.schema.json). Example:
`Apps/PathStudio/examples/room.sfctx.json`.

## Why a separate file

Scene discovery ([OSC v1](OSC-v1.md)) deliberately returns only SpatialFabric/AudioLoom records, not arbitrary
actors, meshes or colliders. Spatial bindings are exactly located (`stage_normalized`); listener, stage and cue
actors only carry engine-world transforms whose axes, rotation and units differ per engine, so Path Studio
**does not guess** them into stage space. Context files let each engine's editor tooling, or a person, supply
reference objects already converted to canonical stage-normalized coordinates.

## Document

```json
{
  "schema": "org.nplaudio.spatialfabric.context",
  "schema_version": 1,
  "space": "canonical",
  "name": "Example room",
  "objects": [
    {"id": "front-wall", "kind": "box", "label": "Front wall", "position": [1.15, 0, 0.5], "half_extents": [0.05, 1.2, 0.6]},
    {"id": "fountain", "kind": "sphere", "position": [0, 0, 0], "radius": 0.25},
    {"id": "speaker-fl", "kind": "point", "position": [1, -1, 0.8]},
    {"id": "listener", "kind": "listener", "position": [-0.6, 0, 0.15], "yaw_degrees": 0}
  ]
}
```

| Field | Contract |
|---|---|
| `space` | `canonical`: positions are `[front, right, up]` normalized to stage half extents, like Path v1 |
| `objects` | at most 4096 |
| `id` | 1–128 of `A-Z a-z 0-9 _ . / : -`, unique |
| `kind` | `box`, `sphere`, `point` or `listener` |
| `position` | three finite numbers within ±64 (context may extend beyond the ±16 path range) |
| `half_extents` | `box` only: three finite numbers in 0…64 (axis-aligned, front/right/up) |
| `radius` | `sphere` only: 0…64 |
| `yaw_degrees` | optional, `listener` only: −360…360; 0 faces +front, positive turns toward the left (like ADM azimuth) |
| `label` | optional string ≤256 |

Unknown fields are ignored. Files are limited to 8 MiB. Validation codes: `bad_schema`, `bad_version`, `bad_space`,
`bad_objects`, `bad_object`, `bad_id`, `duplicate_id`, `bad_kind`, `bad_position`, `bad_label`, `bad_extent`, `bad_yaw`.

## Planned producers

Per-engine editor exporters (Unity editor menu, Godot tool script, Unreal editor utility) that convert selected
actors/colliders to this format, applying that engine's stage transform. These are not part of version 1 of
the tooling and need per-engine validation against shared fixtures.
