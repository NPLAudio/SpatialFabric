# SpatialFabric [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Multi-protocol spatial audio show-control overlay for Unreal Engine. Maps UE
actor positions to **ADM-OSC**, **d&b DS100**, **RTTrPM**, **QLab**,
**SpaceMap Go**, and **TiMax** via a configurable Stage Volume bounding box —
without writing a line of Blueprint or C++.

An open-source tool for theatre, concert, and immersive-audio designers who
need real-time spatial audio positioning from Unreal Engine.

---

## Table of Contents

- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Feature Overview](#feature-overview)
- [Supported Protocols](#supported-protocols)
- [Editor Panel](#editor-panel)
- [Architecture](#architecture)
- [Blueprint API](#blueprint-api)
- [Configuration Reference](#configuration-reference)
- [Testing](#testing)
- [License](#license)

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Unreal Engine | **5.4** or newer (developed on 5.7) |
| Visual Studio | 2022 (Windows) or Xcode 15+ (macOS) |
| UE Plugin | **OSC** (built-in, must be enabled) |

SpatialFabric includes its own OSC client/server components that wrap UE's
built-in OSC plugin. No additional plugins are required beyond OSC.

---

## Installation

### Option A — Copy into an existing project

1. **Download or clone** this repository:

   ```bash
   git clone https://github.com/dhhuston/SpatialFabric.git
   ```

2. **Copy the plugin** into your project's `Plugins/` folder:

   ```
   YourProject/
   └── Plugins/
       └── SpatialFabric/    ← copy from Plugins/SpatialFabric/
   ```

3. **Enable the built-in OSC plugin** (dependency):
   - Open your project in the Unreal Editor.
   - Edit → Plugins → search **"OSC"** → enable → restart when prompted.

4. **Enable SpatialFabric**:
   - Edit → Plugins → search **"SpatialFabric"** → enable → restart the editor.

5. **Verify** by opening Window → **Spatial Fabric**. The panel should appear
   with Stage, Objects, Adapters, Radar, and Output tabs.

### Option B — Add to an existing .uproject manually

Add these entries to the `Plugins` array in your `.uproject` file:

```json
{
  "Plugins": [
    { "Name": "OSC", "Enabled": true },
    { "Name": "SpatialFabric", "Enabled": true }
  ]
}
```

Then copy the `SpatialFabric/` folder into your `Plugins/` directory and
regenerate project files.

### Option C — Build from source

```batch
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ^
    YourProjectEditor Win64 Development ^
    "C:\path\to\YourProject.uproject" -WaitMutex
```

Adjust the UE path and target name to match your installation.

### Dependency summary

```
SpatialFabric
├── OSC (UE built-in plugin)
├── Sockets / Networking (UE modules — for RTTrPM binary UDP)
└── DeveloperSettings (UE module — for Project Settings integration)
```

---

## Quick Start

1. **Place a Stage Volume** in your level:
   - In the Place Actors panel, search **"Spatial Stage Volume"** and drag it
     into the viewport.
   - Scale and position the box to match your physical audio space.
   - Set **Physical Width/Depth/Height (metres)** in the Details panel to match
     real-world dimensions.

2. **Place a Manager Actor**:
   - Search **"Spatial Fabric Manager"** in Place Actors and drag it into the
     level.
   - In the Details panel, assign your **Stage Volume** reference.

3. **Open the Spatial Fabric panel**: Window → **Spatial Fabric**.

4. **Add object bindings**:
   - In the **Objects** tab, click **+ Add Binding**.
   - Pick the actor you want to track and assign an Object ID.
   - The actor's position will now be tracked relative to the Stage Volume.

5. **Select an output format**:
   - In the **Adapters** tab (or the format selector), choose your target
     protocol (ADM-OSC, DS100, RTTrPM, or QLab).
   - Set the **Target IP** and **Port** for your audio system.

6. **Press Play** (or use the editor panel in edit mode) and verify output
   with a tool like [Protokol](https://hexler.net/protokol),
   [Wireshark](https://www.wireshark.org/), or your audio system's monitor.

---

## Feature Overview

### Stage Volume

A box-shaped actor that defines the physical coordinate space. All tracked
actor positions are **normalized to [-1, 1]** relative to this volume — an
actor at the centre maps to (0, 0, 0), an actor at the +X face maps to
(1, 0, 0). Positions outside the box are clamped.

- **Physical dimensions** (metres) for absolute-position protocols
- **Axis flip** (X/Y/Z) for convention matching
- **Listener actor** (optional) shifts the coordinate origin to a player pawn,
  camera, or any actor — enables binaural/head-tracked output
- **Auto-follow player** checkbox for zero-code listener tracking

### Object Bindings

Each binding maps one UE actor to one or more protocol targets:

| Property | Description |
|----------|-------------|
| **Target Actor** | The actor whose position is tracked |
| **Object ID** | Protocol channel/object number (≥ 1) |
| **Gain** | Linear amplitude multiplier [0–2] |
| **Width / Spread** | Object spread [0–1] |
| **Muted** | Suppress output for this object |
| **ADM Dref / Dmax** | Distance reference parameters (ADM-OSC only) |
| **DS100 Spread Mode** | Fixed or proximity-based spread (DS100 only) |
| **DS100 Delay Mode** | Off / Tight / Full delay (DS100 only) |
| **QLab Cue ID** | Cue number for QLab object audio |
| **QLab Object Name** | Audio object name within the cue |

### Multi-Protocol Output

All Phase 1 adapters run simultaneously — the Manager Actor fans each frame's
snapshot to every enabled adapter through the Protocol Router.

### Rate Limiting

Each adapter has a configurable **Send Rate (Hz)** (default 50 Hz, range 1–120).
Messages are only sent when the rate limiter allows, preventing network
saturation on high-framerate systems.

### Editor Panel

A dockable Slate panel with five tabs (Stage, Objects, Adapters, Radar,
Output) for configuration and live monitoring without requiring Play mode.

---

## Supported Protocols

### ADM-OSC (Phase 1 — Complete)

[EBU ADM-OSC v1.0](https://adm-osc.github.io/ADM-OSC/) — the open standard
for object-based audio positioning. Compatible with L-Acoustics L-ISA, FLUX::
SPAT Revolution, and other ADM-OSC renderers.

| OSC Address | Values | Description |
|-------------|--------|-------------|
| `/adm/obj/{n}/xyz` | x y z | Cartesian position [-1, 1] |
| `/adm/obj/{n}/aed` | azimuth elevation distance | Polar position |
| `/adm/obj/{n}/gain` | dB | Object gain |
| `/adm/obj/{n}/w` | 0–1 | Object width/spread |
| `/adm/obj/{n}/mute` | 0/1 | Mute state |
| `/adm/obj/{n}/name` | string | Object label |
| `/adm/obj/{n}/dref` | 0–1 | Distance reference |
| `/adm/obj/{n}/dmax` | metres | Max distance |
| `/adm/lis/xyz` | x y z | Listener position |
| `/adm/lis/ypr` | yaw pitch roll | Listener orientation |

**Default port:** 9000

### d&b DS100 Soundscape (Phase 1 — Complete)

d&b audiotechnik DS100 positioning protocol. Supports both absolute-position
and coordinate-mapping modes.

| OSC Address | Values | Description |
|-------------|--------|-------------|
| `/dbaudio1/positioning/source_position/{id}` | x y z | Absolute position (metres) |
| `/dbaudio1/coordinatemapping/source_position_xy/{area}/{id}` | x y | Normalized position [0, 1] |
| `/dbaudio1/positioning/source_spread/{id}` | 0–1 | Source spread |
| `/dbaudio1/positioning/source_delaymode/{id}` | 0/1/2 | Off / Tight / Full |

**Features:**
- Absolute mode (metres) or coordinate-mapping mode (normalized)
- Channel ID offset for multi-zone setups
- Fixed or proximity-based spread (inverse-square with listener distance)
- Per-object delay mode control

**Default port:** 50010

### RTTrPM — Real-Time Tracking Protocol (Phase 1 — Complete)

BlackTrax / CAST RTTrPM binary UDP protocol for tracking data. Used by
SpaceMap Go, lighting consoles, and other tracking-aware systems.

| Field | Format | Description |
|-------|--------|-------------|
| Signature | `0x4154` ('AT') | Protocol identifier |
| Version | `0x0200` | Version 2.0 |
| Module | Trackable (0x01) | Named trackable object |
| Submodule | Centroid (0x02) | X, Y, Z position (doubles, metres) |

All multi-byte values are **big-endian**. This adapter uses raw `FSocket` UDP
(not the OSC client) since RTTrPM is a binary protocol.

**Default port:** 36700

### QLab Object Audio (Phase 1 — Complete)

[QLab 5](https://qlab.app/) object audio positioning via OSC.

| OSC Address | Values | Description |
|-------------|--------|-------------|
| `/cue/{cueID}/object/{name}/position/live` | x y | Object position |
| `/cue/{cueID}/object/{name}/spread/live` | s | Object spread |

**Default port:** 53000

### Phase 2 Adapters (Stubs)

The following adapters are defined but not yet fully implemented:

| Adapter | Target System | Default Port |
|---------|---------------|-------------|
| **QLab Cue Control** | QLab cue triggering | 53000 |
| **SpaceMap Go** | Meyer Sound SpaceMap Go | 38033 |
| **TiMax SoundHub** | TiMax SoundHub / panLab | 7000 |

---

## Editor Panel

Open via Window → **Spatial Fabric**. The panel has five tabs:

| Tab | Purpose |
|-----|---------|
| **Stage** | Stage Volume assignment, physical dimensions, listener config |
| **Objects** | Add/remove object bindings, set Object IDs, per-object adapter parameters |
| **Adapters** | View adapter endpoints (IP, port, rate); configure via Manager's Details panel |
| **Radar** | Live 2D top-down visualization with aspect-correct stage coordinates |
| **Output** | Live protocol message preview and scrolling message log |

The panel works in **editor mode** (no PIE required) — an editor tickable
drives `ProcessFrame()` so you see live data while placing actors.

---

## Architecture

### Module Layout

```
Plugins/SpatialFabric/
├── SpatialFabric.uplugin
├── README.md
├── LICENSE
├── docs/
│   └── deep-research-report.md       — Full technical specification
└── Source/
    ├── SpatialFabric/                 # Runtime module
    │   ├── SpatialFabric.Build.cs
    │   ├── Public/
    │   │   ├── SpatialFabricTypes.h           — All structs and enums
    │   │   ├── SpatialFabricSettings.h        — Project Settings defaults
    │   │   ├── SpatialFabricManagerActor.h    — Central level actor
    │   │   ├── SpatialStageVolume.h           — Bounding box + coordinate transform
    │   │   ├── SpatialObjectRegistry.h        — Builds per-frame snapshot
    │   │   ├── ISpatialProtocolAdapter.h      — Adapter interface + rate limiter
    │   │   ├── ProtocolRouter.h               — Fan-out to all adapters
    │   │   ├── SpatialMath.h                  — Coordinate transform utilities
    │   │   └── Adapters/
    │   │       ├── ADMOSCAdapter.h            — ADM-OSC v1.0 (Phase 1)
    │   │       ├── DS100Adapter.h             — d&b DS100 (Phase 1)
    │   │       ├── RTTrPMAdapter.h            — Binary UDP (Phase 1)
    │   │       ├── QLabObjectAdapter.h        — QLab object audio (Phase 1)
    │   │       ├── QLabCueAdapter.h           — QLab cue control (Phase 2 stub)
    │   │       ├── SpaceMapGoAdapter.h        — SpaceMap Go (Phase 2 stub)
    │   │       └── TiMaxAdapter.h             — TiMax (Phase 2 stub)
    │   └── Private/
    │       ├── Tests/
    │       │   └── SpatialFabricTests.cpp     — 8 automation tests
    │       └── ... (implementation files)
    │
    └── SpatialFabricEditor/           # Editor-only module
        ├── SpatialFabricEditor.Build.cs
        └── Public/UI/
            └── SSpatialFabricPanel.h          — Dockable Slate panel (5 tabs)
```

### Runtime Components

| Class | Role |
|-------|------|
| `ASpatialFabricManagerActor` | Central level actor; owns all components and drives the frame loop |
| `ASpatialStageVolume` | Defines coordinate space; normalizes world positions to [-1, 1] |
| `USpatialObjectRegistry` | Reads actor positions each tick; builds `FSpatialFrameSnapshot` |
| `FProtocolRouter` | Fans snapshot to all enabled adapters; filters per-adapter object subsets |
| `ISpatialProtocolAdapter` | Pure interface all adapters implement; includes rate limiter |
| `USpatialOSCServerComponent` | Receives incoming OSC UDP via UE's OSC plugin |
| `USpatialOSCClientComponent` | Sends outgoing OSC UDP via UE's OSC plugin |

### Data Flow

```
Each tick:
  ASpatialStageVolume::ResolveListenerThisFrame()
    → cache listener position/rotation

  USpatialObjectRegistry::BuildSnapshot(Bindings, StageVolume)
    → resolve soft actor pointers
    → read world transforms
    → normalize to stage volume [-1, 1]
    → compute physical metres
    → produce FSpatialFrameSnapshot

  FProtocolRouter::ProcessFrame(Snapshot, Bindings, DeltaTime)
    → for each enabled adapter:
        FilterSnapshotForAdapter()  → slice per-adapter subset, resolve ObjectIDs
        Adapter::ProcessFrame()     → format protocol messages and send
```

### Module Dependencies

```
SpatialFabric (runtime):
    Core, CoreUObject, Engine, OSC, Sockets, Networking,
    DeveloperSettings, Slate, SlateCore, InputCore

SpatialFabricEditor (editor-only):
    SpatialFabric, EditorSubsystem, PropertyEditor, UnrealEd,
    LevelEditor, EditorFramework, OSC, WorkspaceMenuStructure, ToolMenus
```

---

## Blueprint API

| Node | Description |
|------|-------------|
| `GetOrSpawnManager` | Find or spawn the ASpatialFabricManagerActor in the current world |
| `StartServer` / `StopServer` | Start/stop the OSC receive server |
| `ConnectClient` / `DisconnectClient` | Open/close the shared OSC send socket |
| `ProcessFrame` | Manually drive one spatial frame (normally called by Tick) |
| `InitializeAdapters` | Re-create all adapters from current config (safe at runtime) |
| `GetRecentLog` | Retrieve recent protocol log entries |
| `WorldToNormalized` | Convert a world position to stage-normalized [-1, 1] coordinates |
| `WorldToMeters` | Convert a world position to physical metres relative to stage origin |
| `AssignListenerToPlayerPawn` | Set the player pawn as the listener actor |
| `AssignListenerToPlayerCamera` | Set the player camera as the listener actor |
| `ClearListener` | Remove listener assignment (revert to stage-fixed mode) |

---

## Configuration Reference

### Project Settings

Accessible via Project Settings → Plugins → **Spatial Fabric**:

| Setting | Default | Description |
|---------|---------|-------------|
| `bEnableInEditor` | `true` | Enable networking in the editor |
| `bEnableInPackagedBuilds` | `false` | Enable networking in shipped builds |
| `DefaultOSCListenPort` | `8100` | Incoming OSC port |
| `DefaultDS100Port` | `50010` | DS100 default send port |
| `DefaultADMOSCPort` | `9000` | ADM-OSC default send port |
| `DefaultQLabPort` | `53000` | QLab default send port |
| `DefaultRTTrPMPort` | `36700` | RTTrPM default send port |
| `DefaultSpaceMapGoPort` | `38033` | SpaceMap Go default send port |
| `DefaultSendRateHz` | `50.0` | Default adapter send rate (Hz) |

### Per-Adapter Configuration

Each adapter entry in `AdapterConfigs` on the Manager Actor supports:

| Field | Description |
|-------|-------------|
| `TargetIP` | Destination IP address |
| `TargetPort` | Destination UDP port |
| `SendRateHz` | Per-adapter rate limit (1–120 Hz) |
| `bEnabled` | Enable/disable this adapter |
| `ADMCoordinateMode` | Cartesian / Polar / Both (ADM-OSC only) |
| `bDS100AbsoluteMode` | Absolute metres vs normalized coordinates (DS100 only) |
| `DS100ChannelOffset` | Channel ID offset (DS100 only) |
| `QLabWorkspaceID` | Workspace identifier (QLab only) |

---

## Testing

### Automation Tests

SpatialFabric includes 8 UE automation tests (run via Session Frontend →
Automation → SpatialFabric):

| Test | Description |
|------|-------------|
| `SpatialMathCentreTest` | Object at stage centre → normalized (0, 0, 0) |
| `SpatialMathCornerTest` | Corner positions, clamping, metre conversion |
| `ADMOSCAddressTest` | ADM-OSC address format validation |
| `DS100AddressTest` | DS100 address format (absolute + coord-mapping + spread) |
| `QLabObjectAddressTest` | QLab position/live and spread/live format |
| `RTTrPMSignatureTest` | RTTrPM header signature bytes (0x4154, 0x0200) |
| `SpatialMathListenerRelativeTest` | Listener-relative coordinate transform |
| `DS100NormMappedTest` | DS100 coordinate mapping math |

### Recommended External Tools

| Tool | Use Case |
|------|----------|
| [Protokol](https://hexler.net/protokol) (free) | General-purpose OSC monitor |
| [python-osc](https://pypi.org/project/python-osc/) | Scripted testing and CI |
| [Wireshark](https://www.wireshark.org/) | RTTrPM binary packet inspection |
| [L-ISA Controller](https://www.l-acoustics.com/) (free offline) | ADM-OSC visual verification |
| [R1 Remote](https://www.dbaudio.com/) (free) | DS100 visual verification |
| [QLab](https://qlab.app/) (free tier) | QLab OSC Activity monitor |

---

## Known Limitations

- **One shared OSC client** per Manager Actor — adapters reconnect the client
  to their endpoint before each send. For truly simultaneous multi-IP output,
  place multiple Manager Actors in the level.
- **Networking is disabled in packaged builds** by default. Set
  `bEnableInPackagedBuilds = true` in Project Settings or DefaultEngine.ini.
- **Phase 2 adapters** (QLab Cue, SpaceMap Go, TiMax) are defined as stubs
  and not yet fully implemented.

---

## License

MIT License — see [LICENSE](LICENSE) for full text.

Copyright (c) 2026 SpatialFabric Contributors.
