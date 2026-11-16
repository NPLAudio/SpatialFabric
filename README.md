# SpatialFabric interoperability contracts

Engine-neutral OSC/JSON conventions and golden test vectors shared by Unreal,
Unity and client applications. No engine assemblies, audio binaries or credentials.

## SpatialFabric repositories

| Repository | Purpose |
|---|---|
| [SpatialFabric](https://github.com/NPLAudio/SpatialFabric) | Unreal plugin, four-room example and standalone OSC Controller |
| [SpatialFabric-Unity](https://github.com/NPLAudio/SpatialFabric-Unity) | Unity UPM port, native audio backend and four-room walking showcase |
| [SpatialFabric-Interop](https://github.com/NPLAudio/SpatialFabric-Interop) | Shared OSC contracts, JSON schemas and golden wire fixtures |
| [AudioLoom](https://github.com/NPLAudio/AudioLoom) | Legacy standalone Unreal playback repository; access-controlled/private, integrated into SpatialFabric |

## Contract contents

- `Specs/OSC-v1.md`: command/transport contract.
- `Schemas/scene-v1.schema.json`: native JSON page schema, copied from the released
  Unreal v2.1.0 contract. Unknown fields/kinds are forward-compatible; version changes
  must be explicit.
- `Fixtures/osc-v1.json`: deterministic wire vectors and malformed packets.
- `Fixtures/coordinates-v1.json`: canonical axes and ADM expectations.
- `Fixtures/custom-v1.json`: packed/discrete XYZ/AED, clamped/reverse ranges,
  custom-field substitution and invariant numeric-address wire vectors, derived from
  the released Unreal Custom adapter contract.

Canonical spatial coordinates are **front/right/up**, normalized to stage half
extents in meters. Unreal converts X/Y/Z centimeters; Unity converts Z/X/Y meters.
ADM wire XYZ is **right/front/up**, coherently limited to a unit sphere with AED.
Schema v1 `world_transform.position_cm` remains centimeters in both engines.
`engine` may be added as an optional field; do not silently change existing units.

Contract fixtures are engine-neutral and vendored into implementation tests with SHA-256
provenance. A contract change should update fixtures and validate both clients/engines;
shared schemas are not a claim of physical audio or cross-platform certification.
