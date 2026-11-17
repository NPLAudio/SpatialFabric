# SpatialFabric

The shared hub for SpatialFabric: engine-neutral interoperability contracts,
OSC tooling and links to the Unreal, Unity and Godot integrations.

## Engine integrations

| Repository | Contents / downloads |
|---|---|
| [SpatialFabric-Unreal](https://github.com/NPLAudio/SpatialFabric-Unreal) | Unreal plugin and four-room C++ example · [Downloads](https://github.com/NPLAudio/SpatialFabric-Unreal/releases/latest) |
| [SpatialFabric-Unity](https://github.com/NPLAudio/SpatialFabric-Unity) | Unity UPM package, native audio and four-room showcase · [Downloads](https://github.com/NPLAudio/SpatialFabric-Unity/releases/latest) |
| [SpatialFabric-Godot](https://github.com/NPLAudio/SpatialFabric-Godot) | Godot addon, native audio and four-room showcase · [Downloads](https://github.com/NPLAudio/SpatialFabric-Godot/releases/latest) |

Each engine owns its runtime, examples, validation and releases. This repository is
not an engine plugin. The previous `SpatialFabric` Unreal repository is now
`SpatialFabric-Unreal`; the previous `SpatialFabric-Interop` repository is this hub.
Histories are preserved. Old URLs containing `NPLAudio/SpatialFabric` must be updated
for Unreal content because that name now points here, not to the renamed engine repo.

## Shared tools and contracts

- **[OSC Controller](Apps/OSCController/README.md)** — portable Windows desktop
  controller, debugger and silent engine simulator. See [releases](https://github.com/NPLAudio/SpatialFabric/releases)
  for the executable bundle; Python sources work independently of engine editors.
- **[OSC v1](Specs/OSC-v1.md)** — command and transport contract, `/ack`, `/status`
  and observed `/state` feedback. UDP acknowledgement is best effort, not delivery proof.
- **[Scene query clients](Examples/OSCSceneQuery/README.md)** — Python, Node, Max and Node-RED discovery examples.
- **[Scene schema v1](Schemas/scene-v1.schema.json)** — engine-neutral cue discovery.
  Unknown fields/kinds are forward-compatible; version changes must be explicit.
- **[Golden fixtures](Fixtures/)** — OSC/acknowledgement packets, command decisions,
  coordinates, ADM and Custom vectors, vendored with SHA-256 provenance by engines.

Canonical coordinates are **front/right/up**, normalized to stage half extents in
meters. Unreal converts X/Y/Z centimeters; Unity converts Z/X/Y meters. ADM wire XYZ
is **right/front/up**, coherently limited to a unit sphere with AED. Schema v1
`world_transform.position_cm` stays centimeters across engines. Do not silently change units.

Shared defaults: spatial input **8100**, playback input **9000**, monitor **9001**,
ADM **50018**, Custom **9002**. Configure distinct ports for simultaneous engines.
Explicitly start listeners and audio in the chosen integration; nothing should play
automatically. Use trusted networks and maintain an independent hardware mute.

## Development

```sh
python -m unittest discover -s Tools -p 'test_*.py'
cd Apps/OSCController
python -m unittest discover -s tests -v
```

Contract changes must update fixtures and engine provenance/tests. Cross-engine
schemas do not certify physical sound, latency, devices or every platform. GitHub's
automatic source archives do not contain engine native libraries; use the documented
engine release assets or build from source. MIT-licensed; retain runtime notices.
