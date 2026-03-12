# SpatialFabric [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE) [![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-yellow?logo=buymeacoffee)](https://buymeacoffee.com/notpunny)

Multi-protocol spatial audio show-control plugin for Unreal Engine. Maps UE
actor positions to spatial audio renderers via **ADM-OSC** —
without writing a line of Blueprint or C++.

Through ADM-OSC, a single output reaches any compatible renderer: L-Acoustics
L-ISA, FLUX:: SPAT Revolution, d&b Soundscape, Meyer Sound SpaceMap Go, TiMax
SoundHub, and any other ADM-OSC–compliant system.

An open-source tool for theatre, concert, and immersive-audio designers who
need real-time spatial audio positioning from Unreal Engine.

---

## Table of Contents

- [Requirements](#requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Coordinate Convention](#coordinate-convention)
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
| Unreal Engine | **5.4 - 5.7** |
| Visual Studio | 2022 (Windows) or Xcode 15+ (macOS) |
| UE Plugin | **OSC** (built-in, must be enabled) |

SpatialFabric includes its own `USpatialOSCClientComponent` and
`USpatialOSCServerComponent` that wrap UE's built-in OSC plugin directly.
No additional plugins are required beyond OSC.

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
       └── SpatialFabric/
   ```

3. **Enable the built-in OSC plugin** (dependency):
   - Open your project in the Unreal Editor.
   - Edit → Plugins → search **"OSC"** → enable → restart when prompted.

4. **Enable SpatialFabric**:
   - Edit → Plugins → search **"SpatialFabric"** → enable → restart the editor.

5. **Verify** by opening Window → **Spatial Fabric**. The panel should appear
   with Stage, Objects, Radar, and Output tabs.

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
    -Project="C:\path\to\YourProject.uproject" -WaitMutex -FromMsBuild
```

Adjust the UE path and target name to match your installation.

For UE 5.4 and other 5.x installs, use the matching engine path for that
version when invoking the build script.

### Dependency summary

```
SpatialFabric
├── OSC (UE built-in plugin)
├── Sockets / Networking (UE modules)
└── DeveloperSettings (UE module — for Project Settings integration)
```

---

## Quick Start

1. **Open the Spatial Fabric panel**:
   - In the Unreal Editor, go to Window → **Spatial Fabric**.
   - Use this panel as the main setup and monitoring surface after the plugin
     is installed.

2. **Create or locate the Manager Actor**:
   - If the level does not already contain a **Spatial Fabric Manager**, the
     panel shows a warning banner at the top.
   - Click **Spawn Manager Actor** from that banner to create and select it.

3. **Create the Stage Volume from the panel**:
   - In the **Stage** tab, click **Spawn Stage Volume in Level**. If a stage
     volume already exists, the same control lets you select and assign it.
   - Scale and position the box to match your physical audio space.
   - Set **Physical Width/Depth/Height (meters)** in the Details panel to match
     real-world dimensions.

4. **Add object bindings**:
   - In the **Objects** tab, click **+ Add Selected Actors**.
   - Pick the actor you want to track and assign an Object ID.
   - The actor's position will now be tracked relative to the Stage Volume.

5. **Enable an output adapter**:
   - In the **Objects** tab (or the Manager's Details panel), enable
     **ADM-OSC** (for any ADM-OSC–compatible renderer) or another adapter.
   - Set the **Target IP** and **Port** for your audio system.

6. **Press Play** (or use the editor panel in edit mode) and verify output
   with a tool like [Protokol](https://hexler.net/protokol) or your audio
   system's OSC monitor.

---

## Coordinate Convention

SpatialFabric uses the **audience / listener perspective** (house left/right),
which aligns with UE's native axis orientation:

```
        +X (front / toward audience)
         ↑
  -Y ←── ● ──→ +Y (house right)
         ↓
        -X (back / upstage)

  +Z = up
  -Z = down
```

| Axis | Direction | UE Equivalent |
|------|-----------|---------------|
| **+X** | Front (downstage / toward audience) | Forward |
| **-X** | Back (upstage) | Backward |
| **+Y** | House right (audience's right) | Right |
| **-Y** | House left (audience's left) | Left |
| **+Z** | Up (fly space) | Up |
| **-Z** | Down | Down |

All positions are **normalized to [-1, 1]** relative to the Stage Volume.
Adapters that use different conventions (e.g., ADM-OSC's left-positive azimuth)
handle the conversion internally at the point of send.

---

## Feature Overview

### Stage Volume

A box-shaped actor that defines the physical coordinate space. All tracked
actor positions are **normalized to [-1, 1]** relative to this volume — an
actor at the center maps to (0, 0, 0), an actor at the +X face maps to
(1, 0, 0). Positions outside the box are clamped.

- **Physical dimensions** (meters) for absolute-position protocols (DS100)
- **Axis flip** (X/Y/Z) checkboxes for convention overrides
- **Listener actor** (optional) shifts the coordinate origin to a player pawn,
  camera, or any actor — enables binaural/head-tracked output
- **Listener-relative orientation** rotates axes to follow the listener's facing
  direction (useful for head-tracked binaural rendering)
- **Auto-follow player** checkbox for zero-code listener tracking

### Object Bindings

Each binding maps one UE actor to one or more protocol targets:

| Property | Description |
|----------|-------------|
| **Target Actor** | The actor whose position is tracked |
| **Object ID** | Protocol channel/object number (>= 1) |
| **Label** | Display name (falls back to actor label) |
| **Gain** | Linear amplitude multiplier [0-2] |
| **Width / Spread** | Object spread [0-1] |
| **Muted** | Suppress output for this object |
| **ADM Dref / Dmax** | Distance reference parameters (ADM-OSC only) |
| **Adapter Targets** | Per-binding adapter type overrides and per-target ObjectID overrides |
| **CustomFields** | Key-value pairs for Custom adapter placeholders (e.g. `slot`, `channel`) |

> **DS100-specific fields** (Spread Mode, Delay Mode) are available on the
> binding when the DS100 adapter is active. **Custom adapter** bindings support
> CustomFields for template placeholders like `{slot}` and `{channel}`.

### Multi-Protocol Output

All adapters run simultaneously — the Manager Actor fans each frame's
snapshot to every enabled adapter through the Protocol Router.

### Rate Limiting and Transmit-on-Change

Each adapter has a configurable **Send Rate (Hz)** (default 50 Hz, range 1-120)
in the Objects tab. Messages are only sent when the rate limiter allows,
preventing network saturation on high-framerate systems.

**Only when changed:** When enabled, the adapter sends full state on level
start, then only when coordinates (and gain/width/mute) change. Reduces
network traffic when objects are static.

### Editor Panel

A dockable Slate panel with four tabs for configuration and live monitoring
without requiring Play mode. **ADM-OSC** is shown by default in the Objects
tab; DS100, SpaceMap Go, and TiMax are available in the Manager Actor's
Details panel for advanced use.

---

## Supported Protocols

### ADM-OSC

[ADM-OSC](https://adm-osc.github.io/ADM-OSC/) is the open standard for
object-based audio positioning over OSC ([spec on GitHub](https://github.com/immersive-audio-live/ADM-OSC)).
A single ADM-OSC output from SpatialFabric works with any compliant renderer:

| System | Vendor | Documentation |
|--------|--------|---------------|
| [L-ISA Controller](https://www.l-acoustics.com/products/l-isa-controller/) | L-Acoustics | [L-ISA OSC Protocol](https://l-isa-immersive.com/en/tech-resources/) |
| [SPAT Revolution](https://www.flux.audio/project/spat-revolution/) | FLUX:: | [SPAT Revolution OSC](https://doc.flux.audio/spat-revolution/) |
| [Soundscape / DS100](https://www.dbaudio.com/global/en/products/signal-processors/ds100/) | d&b audiotechnik | [DS100 OSC Protocol](https://www.dbaudio.com/global/en/service/documentation/) |
| [SpaceMap Go](https://meyersound.com/product/spacemap-go/) | Meyer Sound | [SpaceMap Go OSC](https://meyersound.com/product/spacemap-go/) |
| [TiMax SoundHub](https://thetimaxgroup.com/timax-soundhub/) | TiMax | [TiMax OSC Reference](https://thetimaxgroup.com/resources/) |
| [Space Controller](https://www.spacecontroller.com/) | Sound Particles | [Space Controller Docs](https://www.soundparticles.com/products/spacecontroller) |

ADM-OSC sends position, gain, spread, mute, label, and optional distance
parameters per object, plus listener position and orientation for binaural
head-tracking.

| OSC Address | Values | Description |
|-------------|--------|-------------|
| `/adm/obj/{n}/xyz` | x y z | Cartesian position [-1, 1] (Y negated on wire — ADM convention is left-positive) |
| `/adm/obj/{n}/aed` | azimuth elevation distance | Polar position (azimuth: 0°=front, +left) |
| `/adm/obj/{n}/gain` | linear | Object gain |
| `/adm/obj/{n}/w` | 0-1 | Object width/spread |
| `/adm/obj/{n}/mute` | 0/1 | Mute state |
| `/adm/obj/{n}/name` | string | Object label |
| `/adm/obj/{n}/dref` | 0-1 | Distance reference |
| `/adm/obj/{n}/dmax` | meters | Max distance |
| `/adm/lis/xyz` | x y z | Listener position |
| `/adm/lis/ypr` | yaw pitch roll | Listener orientation |

**Default port:** 9000
**Recommended send rate:** ≤ 50 Hz (per ADM-OSC spec)

### Custom (Template)

The **Custom** adapter sends OSC using user-defined address and argument
templates. Use it for systems that don't follow ADM-OSC or other built-in
protocols.

| Setting | Description |
|---------|-------------|
| **Coords** | **xyz** = Cartesian (x, y, z); **aed** = polar (azimuth, elevation, distance) |
| **Send** | **Bundled** = one message with multiple args; **Discrete** = one message per value |
| **Address** | OSC path template with `{placeholders}` |
| **Args** | Space-separated arg names: `x y z gain width id` (xyz) or `a e d` (aed) |
| **Range** | Map position [-1..1], gain [0..1], width [0..1] to custom output ranges |

**Placeholders:** `{id}`, `{x}`, `{y}`, `{z}`, `{gain}`, `{width}`, `{label}`,
`{axis}` (discrete mode only — replaced with current axis name, e.g. `/source/{id}/{axis}` → `/source/1/x`).
Add per-binding **CustomFields** for `{slot}`, `{channel}`, etc.

**AED arg names:** `azimuth`/`a`, `elevation`/`e`, `distance`/`d`.

### Custom Adapter

The **Custom** adapter is available from the Manager Actor's Details panel and
the Objects tab format selector.

Use it when you need template-based OSC output instead of one of the built-in
protocol-specific adapters.

| Setting | Description |
|---------|-------------|
| **Address** | OSC path template with `{placeholders}` |
| **Args** | Space-separated arg names such as `x y z gain width id` |
| **Coords** | `xyz` for Cartesian or `aed` for polar coordinates |
| **Send** | Bundled or discrete message sending |
| **Range** | Output remapping for position, gain, and width values |

---

## Editor Panel

Open via Window → **Spatial Fabric**. The panel has four tabs:

| Tab | Purpose |
|-----|---------|
| **Stage** | Stage Volume assignment, physical dimensions, listener config |
| **Objects** | Add/remove object bindings, set Object IDs, format selector (ADM-OSC, Custom, DS100, SpaceMap Go, TiMax) |
| **Radar** | Live 2D top-down visualization with aspect-correct stage coordinates (front=top, right=right) |
| **Output** | Live protocol message preview and scrolling message log |

**Objects tab:** The format selector (ADM-OSC, Custom, DS100, etc.) switches the
active adapter. The **Rate (Hz)** control sets the polling limit (1–120 Hz);
**Only when changed** sends only when coordinates change (full state on start).
When **Custom** is selected, a template section appears with coordinate mode
(xyz/aed), send mode (bundled/discrete), address and args templates, and range
mapping. Each object row has an **enable checkbox** and a **delete icon** to
remove the binding.

The panel works in **editor mode** (no PIE required) — an editor tickable
drives `ProcessFrame()` so you see live data while placing actors.

> **Advanced adapters** (DS100, SpaceMap Go, TiMax) show protocol-specific
> options in each binding row when that format is active.

---

## Architecture

### Module Layout

```
Plugins/SpatialFabric/
├── SpatialFabric.uplugin
├── README.md
├── LICENSE
└── Source/
    ├── SpatialFabric/                   # Runtime module
    │   ├── SpatialFabric.Build.cs
    │   ├── Public/
    │   │   ├── SpatialFabricTypes.h             — All structs and enums
    │   │   ├── SpatialFabricSettings.h          — Project Settings defaults
    │   │   ├── SpatialFabricManagerActor.h      — Central level actor
    │   │   ├── SpatialStageVolume.h             — Bounding box + coordinate transform
    │   │   ├── SpatialObjectRegistry.h          — Builds per-frame snapshot
    │   │   ├── ISpatialProtocolAdapter.h        — Adapter interface + rate limiter
    │   │   ├── ProtocolRouter.h                 — Fan-out to all adapters
    │   │   ├── SpatialMath.h                    — Coordinate transform utilities
    │   │   ├── SpatialOSCClientComponent.h      — Sends OSC UDP (wraps UE OSC plugin)
    │   │   ├── SpatialOSCServerComponent.h      — Receives OSC UDP (wraps UE OSC plugin)
    │   │   └── Adapters/
    │   │       ├── ADMOSCAdapter.h              — ADM-OSC v1.0
    │   │       ├── CustomAdapter.h              — Template-based OSC (xyz/aed, bundled/discrete)
    │   │       ├── DS100Adapter.h               — d&b DS100
    │   │       ├── SpaceMapGoAdapter.h          — SpaceMap Go
    │   │       └── TiMaxAdapter.h               — TiMax SoundHub (delegates to ADM-OSC)
    │   └── Private/
    │       ├── SpatialFabricManagerActor.cpp
    │       ├── SpatialStageVolume.cpp
    │       ├── SpatialObjectRegistry.cpp
    │       ├── SpatialMath.cpp
    │       ├── ProtocolRouter.cpp
    │       ├── SpatialOSCClientComponent.cpp
    │       ├── SpatialOSCServerComponent.cpp
    │       ├── SpatialFabricModule.cpp
    │       ├── SpatialFabricSettings.cpp
    │       ├── Adapters/
    │       │   ├── ADMOSCAdapter.cpp
    │       │   ├── CustomAdapter.cpp
    │       │   ├── DS100Adapter.cpp
    │       │   ├── SpaceMapGoAdapter.cpp
    │       │   └── TiMaxAdapter.cpp
    │       └── Tests/
    │           └── SpatialFabricTests.cpp       — 7 automation tests
    │
    └── SpatialFabricEditor/             # Editor-only module
        ├── SpatialFabricEditor.Build.cs
        ├── Public/
        │   ├── SpatialFabricEditorModule.h
        │   └── UI/
        │       └── SSpatialFabricPanel.h        — Dockable Slate panel (4 tabs)
        └── Private/
            ├── SpatialFabricEditorModule.cpp
            └── UI/
                └── SSpatialFabricPanel.cpp
```

### Runtime Components

| Class | Role |
|-------|------|
| `ASpatialFabricManagerActor` | Central level actor; owns all components and drives the frame loop |
| `ASpatialStageVolume` | Defines coordinate space; normalizes world positions to [-1, 1] |
| `USpatialObjectRegistry` | Reads actor positions each tick; builds `FSpatialFrameSnapshot` |
| `FProtocolRouter` | Fans snapshot to all enabled adapters; filters per-adapter object subsets |
| `ISpatialProtocolAdapter` | Pure interface all adapters implement; includes rate limiter |
| `USpatialOSCClientComponent` | Sends outgoing OSC UDP via UE's built-in OSC plugin |
| `USpatialOSCServerComponent` | Receives incoming OSC UDP via UE's built-in OSC plugin |

### Data Flow

```
Each tick:
  ASpatialStageVolume::ResolveListenerThisFrame()
    → cache listener position/rotation
    → if auto-follow: teleport stage volume to listener

  USpatialObjectRegistry::BuildSnapshot(Bindings, StageVolume)
    → resolve soft actor pointers (with cached label fallback)
    → read world transforms
    → normalize to stage volume [-1, 1]
    → compute physical meters
    → produce FSpatialFrameSnapshot

  FProtocolRouter::ProcessFrame(Snapshot, Bindings, DeltaTime)
    → for each enabled adapter:
        FilterSnapshotForAdapter()  → slice per-adapter subset, resolve ObjectIDs
        Adapter::ProcessFrame()     → format protocol messages and send
```

### Adapter Wire Convention Handling

The internal coordinate system uses +Y = right (audience perspective).
Each adapter converts to its wire format at the point of send:

| Adapter | Wire Convention | Conversion |
|---------|----------------|------------|
| ADM-OSC Cartesian | +Y = left | Negate Y |
| ADM-OSC Polar | Azimuth +left (CCW) | Negate Y before atan2 |
| DS100 (absolute) | Direct meters | No conversion needed |
| DS100 (mapped) | X: 0=left, 1=right | `(NormY + 1) * 0.5` |
| SpaceMap Go | X: left=-1000, right=+1000; Y: back=-1000, front=+1000 | `NormY * 1000` / `NormX * 1000` |
| Custom | User-defined (xyz or aed, bundled or discrete) | Per template / range mapping |
| TiMax | Same as ADM-OSC (delegates internally) | Negate Y |

### Module Dependencies

```
SpatialFabric (runtime):
    Core, CoreUObject, Engine, OSC, Sockets, Networking, DeveloperSettings,
    Slate, SlateCore, InputCore

SpatialFabricEditor (editor-only):
    SpatialFabric, EditorSubsystem, PropertyEditor, UnrealEd,
    LevelEditor, EditorStyle, OSC, WorkspaceMenuStructure
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
| `WorldToMeters` | Convert a world position to physical meters relative to stage origin |
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
| `DefaultSpaceMapGoPort` | `38033` | SpaceMap Go default send port |
| `DefaultSendRateHz` | `50.0` | Default adapter send rate (Hz) |

### Per-Adapter Configuration

Each adapter entry in `AdapterConfigs` on the Manager Actor supports:

| Field | Description |
|-------|-------------|
| `TargetIP` | Destination IP address |
| `TargetPort` | Destination UDP port |
| `SendRateHz` | Per-adapter rate limit (1-120 Hz) |
| `bSendOnlyOnChange` | When true, send only when coordinates/gain/width/mute change (full state on start) |
| `bEnabled` | Enable/disable this adapter |
| `ADMCoordinateMode` | Cartesian / Polar / Both (ADM-OSC only) |
| `CustomCoordinateMode` | xyz (Cartesian) or aed (polar) — Custom adapter |
| `CustomSendMode` | Bundled or Discrete — Custom adapter |
| `CustomAddressTemplate` | OSC path with `{placeholders}` — Custom adapter |
| `CustomArgTemplate` | Space-separated arg names — Custom adapter |
| `CustomPositionRange` / `CustomGainRange` / `CustomWidthRange` | Output range (min, max) — Custom adapter |
| `bDS100AbsoluteMode` | Absolute meters vs normalized coordinates (DS100 only) |
| `DS100ChannelOffset` | Channel ID offset (DS100 only) |

---

## Testing

### Automation Tests

SpatialFabric includes 7 UE automation tests (run via Session Frontend →
Automation → SpatialFabric):

| # | Test | Description |
|---|------|-------------|
| 1 | `Math.Center` | Object at stage center → normalized (0, 0, 0) |
| 2 | `Math.Corner` | Corner positions, clamping, meter conversion |
| 3 | `Adapters.ADMOSCAddress` | ADM-OSC address format and value range validation |
| 4 | `Adapters.DS100Address` | DS100 address format (absolute + coord-mapping + spread) |
| 5 | `Math.ListenerRelative` | Listener-relative coordinate transform (no rotation) |
| 6 | `Math.ListenerRelativeRotated` | Listener-relative with yaw rotation (Y-axis sign) |
| 7 | `Math.DS100Mapped` | DS100 coordinate mapping math (front-right/front-left) |

### Recommended External Tools

| Tool | Use Case |
|------|----------|
| [Protokol](https://hexler.net/protokol) (free) | General-purpose OSC monitor |
| [python-osc](https://pypi.org/project/python-osc/) | Scripted testing and CI |
| [L-ISA Controller](https://www.l-acoustics.com/) (free offline) | ADM-OSC visual verification |
| [R1 Remote](https://www.dbaudio.com/) (free) | DS100 visual verification |

---

## Known Limitations

- **One shared OSC client** per Manager Actor — adapters reconnect the client
  to their endpoint before each send. For truly simultaneous multi-IP output,
  place multiple Manager Actors in the level.
- **Networking is disabled in packaged builds** by default. Set
  `bEnableInPackagedBuilds = true` in Project Settings or DefaultEngine.ini.
  A warning is logged to the Output Log when networking is skipped.
- **All adapters disabled by default** — enable per-show in the Manager Actor's
  AdapterConfigs map. The Objects tab shows a hint and **Enable** button when
  the active adapter is disabled.

---

## Support

If SpatialFabric is useful to you, consider
[buying me a coffee](https://buymeacoffee.com/notpunny).

---

## License

MIT License — see [LICENSE](LICENSE) for full text.

Copyright (c) 2026 SpatialFabric Contributors.

### Trademark Notice

L-Acoustics, L-ISA, FLUX::, SPAT Revolution, d&b, Soundscape, Meyer Sound,
SpaceMap Go, TiMax, SoundHub, Sound Particles, and Space Controller are the
property of their respective owners. SpatialFabric is an independent plugin and
is not affiliated with, endorsed by, or sponsored by those manufacturers.
