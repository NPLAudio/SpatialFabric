# Cross-engine audio routing matrix

The common authoring contract is [Audio Routing Matrix v1](../Specs/Audio-Routing-Matrix-v1.md). It extends the [Scene Object Manager](SceneObjectManager.md) with direct playback output assignment, not network routing or a DSP mixer.

## Using the editor

- Open the SpatialFabric editor and its audio-routing grid.
- Click **Refresh Interfaces** to explicitly discover output hardware. Opening the new routing grid does not enumerate devices or open audio streams; legacy selectors retain their documented behavior.
- Read playback cue/component rows against interface/channel columns; click an intersection to configure that route. Checks show stored configuration, not live connectivity.
- Use the interface/channel dropdowns when preferred. Each cue has one route. **Auto / 0** uses corresponding source channels; a positive number sends the first source channel to that 1-based output.
- Offline cached interfaces remain available. Unplugging hardware does not erase its channel layout or any scene assignment. An empty endpoint ID remains dynamic OS default, not a fixed hardware identity.
- Save the scene normally. Route changes use native Undo; the editor-user inventory cache contains no scene route assignments.

## Current local verification

| Port | State | Evidence |
|---|---|---|
| Unity | Implemented; real crosspoint grid and interface/channel dropdowns | Unity 6000.6.9f1: 139 EditMode + 6 PlayMode passed, zero skipped; `C:/Temp/SFSceneObjectManager-20261118/true-crosspoint-clipping/validation.json` |
| Unreal | Implemented; true crosspoint grid, selectors, offline cache and guarded native Undo | UE 5.8 Win64 Editor Development / Game Development / Game Shipping builds passed; 44 native tests passed (43 clean, one existing warning); `C:/Temp/SFUnrealRouting-final-clipping/Report/index.json` |
| Godot | Implemented; grid, selectors, offline cache and native Undo | Godot 4.7.2: 149 editor + 308 native-required runtime assertions, 22 Python tests passed; `C:/dev/GodotRouting-20261118-reviewed149-bounddraft/` |

Per-port implementation notes:

- [Unity](https://github.com/NPLAudio/SpatialFabric-Unity/blob/main/Docs/AudioRoutingMatrix.md)
- [Godot](https://github.com/NPLAudio/SpatialFabric-Godot/blob/main/Docs/AudioRoutingMatrix.md)
- [Unreal](https://github.com/NPLAudio/SpatialFabric-Unreal/blob/main/Docs/AudioRoutingMatrix.md)

Tests use synthetic inventories and silent/offline probes. Physical hotplug, audible output, microphone capture, acoustic latency and platform certification are separate acceptance gates. Do not infer them from a green configured-route check.
