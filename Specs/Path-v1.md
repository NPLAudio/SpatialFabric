# SpatialFabric path document, version 1

Engine-neutral description of how objects, listeners and cameras move over time.
Used by **Path Studio** (`Apps/PathStudio`) for authoring, live streaming and baked
export. Schema: [`Schemas/path-v1.schema.json`](../Schemas/path-v1.schema.json).
Golden vectors: [`Fixtures/path-v1.json`](../Fixtures/path-v1.json). Decisions:
[ADR 0001](../Docs/PathStudio-ADR-0001.md).

Files are UTF-8 JSON, conventionally `*.sfpath.json`. Unknown fields are ignored so
version 1 can grow compatibly; changing sampling semantics requires a new version.

## Space

All positions are **canonical** `[front, right, up]`, normalized to stage half
extents (see [OSC v1](OSC-v1.md) and `Fixtures/coordinates-v1.json`). Engine units are
produced only at the edge: Unity `[right, up, front]` meters, Unreal `[front, right, up]`
centimeters, ADM `[right, front, up]`. Never store engine units in a path document.

## Document

```json
{
  "schema": "org.nplaudio.spatialfabric.path",
  "schema_version": 1,
  "name": "Fly-by",
  "space": "canonical",
  "waypoints": [{"id": "door", "name": "Door", "position": [0.0, 1.0, 0.0]}],
  "tracks": [{
    "id": "t1",
    "target": {"kind": "object", "key": "unity/playback/mono"},
    "loop": "none",
    "speed": 1.0,
    "keyframes": [
      {"t": 0.0, "position": [0, 0, 0], "interp": "linear", "ease": "linear"},
      {"t": 2.0, "waypoint": "door"}
    ],
    "parameters": {"gain": [{"t": 0, "value": 0}, {"t": 2, "value": 1}]}
  }]
}
```

| Field | Contract |
|---|---|
| `schema`, `schema_version` | exactly the constant and `1` |
| `space` | `"canonical"` (only value in v1) |
| `waypoints[]` | optional named positions; `id` is 1–64 `[A-Za-z0-9_-]`, unique |
| `tracks[]` | 1–256; `id` unique, same charset |
| `target.kind` | `object` (audio source/binding), `listener`, or `camera` |
| `target.key` | the stable key returned by scene discovery (1–256 chars); not engine-resolved here |
| `loop` | `none` (default), `loop`, or `ping_pong` |
| `speed` | finite number > 0 and ≤ 64; default 1 |
| `keyframes[]` | 1–10000; `t` finite seconds ≥ 0, **strictly increasing** |
| keyframe position | exactly one of `position` (3 finite numbers, each −16…16) or `waypoint` (id reference) |
| `interp` | applies to the segment **leaving** this keyframe: `linear` (default), `hold`, `catmull_rom`, `bezier` |
| `ease` | `linear` (default), `in`, `out`, `in_out` |
| `tangent_in` / `tangent_out` | optional offset vectors for `bezier`, default `[0,0,0]` |
| `parameters` | optional curves `gain` (0…4) and `spread` (0…1); each 1–10000 points `{t, value}`, strictly increasing `t` |

Listener and camera tracks carry position only in v1. Orientation (yaw/look-at) is a
planned compatible addition and must not change position sampling.

## Sampling

`sample(track, time)` is deterministic and uses IEEE double arithmetic.

1. **Time map.** `τ = time × speed`. Let `t0`, `tN` be the first/last keyframe times
   and `span = tN − t0`.
   - `τ ≤ t0` → first keyframe; `span = 0` → first keyframe.
   - `none`: `τ ≥ tN` → last keyframe.
   - `loop`: `τ = t0 + ((τ − t0) mod span)`. There is no segment from last to first;
     author a closing keyframe to make a continuous circuit. At exactly `τ = tN` the
     result is the first keyframe.
   - `ping_pong`: `m = (τ − t0) mod 2·span`; if `m > span` then `m = 2·span − m`; `τ = t0 + m`.
2. **Segment.** Find `i` with `t_i ≤ τ < t_{i+1}` and `u = (τ − t_i) / (t_{i+1} − t_i)`.
3. **Ease** (not for `hold`): `linear u`; `in u²`; `out 1−(1−u)²`; `in_out u²(3−2u)`.
4. **Interpolate** between positions `P_i` and `P_{i+1}` using key `i`:
   - `hold`: `P_i` until the next key time.
   - `linear`: `P_i + (P_{i+1} − P_i)·u`.
   - `catmull_rom`: uniform, with neighbors `P_{i−1}`, `P_{i+2}` clamped to the ends:
     `0.5·(2P_i + (−P_{i−1}+P_{i+1})u + (2P_{i−1}−5P_i+4P_{i+1}−P_{i+2})u² + (−P_{i−1}+3P_i−3P_{i+1}+P_{i+2})u³)`.
     Passes through every keyframe and reproduces evenly spaced collinear data on interior
     segments; the first and last segments use clamped neighbors and may curve slightly.
   - `bezier`: cubic with control points `C1 = P_i + tangent_out_i`, `C2 = P_{i+1} + tangent_in_{i+1}`:
     `(1−u)³P_i + 3(1−u)²u·C1 + 3(1−u)u²·C2 + u³P_{i+1}`.
5. **Parameters** use the same time map and **linear** interpolation, clamped at the ends.

Consumers compare against fixtures with an absolute tolerance of `1e-9`.

## Validation codes

A validator reports every applicable code (document path in the human message):

`bad_schema`, `bad_version`, `bad_space`, `no_tracks`, `too_many_tracks`, `bad_id`,
`duplicate_id`, `bad_target`, `bad_loop`, `bad_speed`, `no_keyframes`, `too_many_keyframes`,
`bad_time`, `times_not_increasing`, `bad_position`, `out_of_range`, `position_conflict`,
`unknown_waypoint`, `bad_interp`, `bad_ease`, `bad_tangent`, `bad_parameter`.

## Delivery and safety

- **Live streaming** samples at 30–100 Hz and sends position through the engine's
  existing spatial input (see OSC v1). UDP is lossy; use it for authoring and preview.
- **Baked export** produces engine assets sampled from this document for deterministic
  in-engine playback. Exporters must match the fixtures.
- Live sending is **explicitly armed** by the operator, targets only a chosen host/port,
  and never starts or stops audio unless separately requested. Treat documents from
  untrusted sources as data: validate before use, and enforce the size limits above.
