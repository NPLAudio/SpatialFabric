# SpatialFabric OSC Controller 1.3

A **separate desktop application**, not an Unreal plugin or editor panel. It sends
OSC/UDP to SpatialFabric or another OSC device, and monitors incoming messages.
It never plays audio itself. No Unreal installation, MCP server, web server,
account, or third-party OSC library is required.

## Run the Windows app

After building, double-click **`dist/SpatialFabricOSCController.exe`**. The companion
`dist/SpatialFabricOSCController-Windows-x64.zip` is the portable distribution:
extract it and run the executable inside. A version-separated local build may be
under **`dist/1.4.0/SpatialFabricOSCController.exe`**; check About for version 1.4.
Python and Tcl/Tk are bundled; recipients
do not need to install Python. Keep the included license notices when sharing.

The portable ZIP also contains two **console tools** (no Tk): `SpatialFabricDebugger.exe` (sender-app debugger, see below) and `SpatialFabricEngineSimulator.exe` (a stand-in engine that makes no sound). Run either with `--help`. A build also writes `SHA256SUMS.txt` and `BUILD_MANIFEST.json`.

This is an unsigned local executable: Windows may show an unsigned-app warning.
There is no installer, elevation requirement, telemetry, or automatic network
connection. On startup **live sending is off, no packets are sent, and no listener
is bound**. Loading a profile does not send or start listening either.

## Run from source

Use Python 3.10+ with Tk (validated here on Windows/Python 3.13). From this folder:

```sh
python launch.py
# or
python -m osc_controller
python launch.py --profile profiles/spatialfabric-showcase.json
```

On Windows with Python 3.13 installed, `run.cmd` is a double-clickable source
launcher. On Linux, install the distribution's Tk package if necessary. Source
uses only the Python standard library; Mac/Linux GUI/device behavior has not been
validated. There is no audio driver/backend in the controller.

## Connect to the showcase

1. Open the SpatialFabric showcase in Unreal and select a safe output device/channel.
   Begin with a low hardware level. The app cannot inspect your physical patch.
2. In **Spatial Fabric → Playback**, start the OSC receiver. Its default input is
   **127.0.0.1:9000**. This app's playback destination must match.
3. In the app, click **Start all receivers (state + objects)**. This starts
   **127.0.0.1:9001** for playback state and **50018 / 9002** for spatial tracking
   simultaneously. Unreal's playback Send IP/Port must point to 9001. No Play is sent.
   Use **Diagnose same-PC OSC** to test an isolated loopback round-trip and check
   whether the configured Unreal input appears bound. A successful send alone is
   not a connection test.
4. Return to **Cue transport**, select a cue and press **Play**. Play sends numeric
   float `1`; Stop is address-only; Loop ON/OFF sends float `1`/`0`.
5. Press **Stop All Configured Cues** before leaving a show. It sends one Stop per
   configured base and disables local spatial live changes. **Closing the app does
   not stop remote audio**, and UDP delivery is not guaranteed. Keep an independent
   system mute; this is not a safety-rated emergency stop.

| Cue | OSC base | Example audio patch |
|---|---|---|
| Warm percussion | `/demo/playback/mono` | First source channel → output 1 |
| Stereo groove | `/demo/playback/stereo` | Channel 0: corresponding source/device channels |
| Glass/mechanical | `/demo/playback/replay` | First source channel → output 2; example replay behavior |

Only configured cue bases receive Stop All. Add/Edit a cue to match the actual
component's **Get OSC Address**. Bases must be unique; fan-out belongs on the
receiver side. Removing an entry does not stop that remote cue. Add/Edit opens a form
with a read-only **Type dropdown** (Playback or Live input); only name and OSC base are typed.

### Command acknowledgement and observed state

Start the monitor and point the engine's Send/Monitor IP:port at it (default 9001).
All three engines send `<base>/ack ,ssis command result playing detail`. The command
column shows **RECEIVED**, **IGNORED**, **REJECTED**, or **FAILED**, including the reason.
**RECEIVED for Play means a start request, not that playback started.** A subsequent
`<base>/state 1` reports the actual worker start; a failed start sends another ack
with **FAILED**. Stop and natural completion report `/state 0`.

**Status?** sends address-only `<base>/status`, asking for an acknowledgement and the
current observed state without starting audio. It also works from anyone's OSC code.
The state column accepts `/state` float/int `1` or `0` and the ack's observed-playing
field. Startup is Unknown; historic state always shows its age. Neither column is
an audio meter, heartbeat, listening test, or proof of correct channels.

A command without a matching acknowledgement shows a warning after 1.5 seconds.
This can mean a wrong port, missing monitor, old engine version, or dropped UDP;
it is **not proof of command failure**. The protocol has no request ID, so concurrent
senders or repeated same-command requests cannot be correlated unambiguously.

## Live-input cues (microphone / line in)

A cue has a **Type**: *Playback* (default) or *Live input*. A live-input cue controls a
`UAudioLoomLiveInputComponent`, Unity `LiveInputCue` or Godot `SpatialFabricLiveInputCue`, which routes a
capture device to an output device on the **remote** machine. The controller itself never captures or plays audio.

| Button | Sends for a live cue |
|---|---|
| **Play** | `<base>/play` (float 1.0), only after a confirmation dialog. It asks the remote machine to open its microphone/line input (the OS may prompt there) and play it through its output. |
| **Stop** | `<base>/stop` (no confirmation; also included in Stop All) |
| **Live gain...** | `<base>/gain` with one finite float 0..4; other values are refused locally |
| Loop ON/OFF | Refused: loop does not apply to a live input |

The state column shows the last reported `<base>/state` as **LIVE** or *stopped*. That is the remote audio
worker's observed lifecycle, not proof of sound at a loudspeaker. Command acknowledgements use the same monitor endpoint.
Import from **Query scene** adds `live_input` records as live cues and, like playback import, only
replaces the cue list locally after confirmation and sends nothing, so discovery can never open a microphone.

Profiles gain an optional per-cue `"kind": "live"` (default `"playback"`; older profiles load unchanged and
unknown kinds are rejected):

```json
{ "name": "Stage mic", "base": "/audioloom/Stage/live/1", "kind": "live" }
```

**A microphone near the speakers it feeds will howl:** use headphones or a low gain. Start only deliberately.

## Debugger / test bed for sender apps

The **Debugger / test bed** tab (and `python -m osc_controller.debugger`) answers: *what is my app really
sending, does it conform, and what does the running engine do with it?* Point your sender (Max, TouchOSC,
a show-control app, your own code) at the debugger's listen port instead of the engine.

1. **Start debugger.** It binds only the listen port (default `9400`), the optional engine-state port, and a
   read-only query reply socket. Nothing binds or sends until you press Start.
2. **Load engine contract.** A read-only scene query pulls the engine's real cue bases, kinds
   (playback / live input) and bound object IDs. Without it only wire rules are checked and cue bases show as
   *unverified* (payload rules still apply).
3. Every datagram is decoded and judged: malformed packets (with their raw bytes and the decode reason),
   unsupported type tags (booleans are not a Play/Loop command), addresses that do not exist in the running
   engine (with a "did you mean" suggestion), commands that do not apply to the cue kind (loop on a live input),
   AudioLoom trigger rules (first float anywhere beats the first int; play float >= 0.5 / int >= 1; loop float
   strictly > 0.5, address-only loop disables; non-numeric payloads never act), live gain 0..4, ADM xyz/aed/gain/
   width/mute/name types and ranges, unit-sphere limiting, object IDs the engine does not bind, scene-query shape,
   sending engine-only messages (`/state`, `/ack`, scene responses), type changes per address, and send-rate bursts.
   Each row says what the engine **will do** or what is wrong.
4. **Forward + verify (off by default).** Valid messages (never ones with errors) are forwarded to the engine
   input, then the debugger compares the sender's intent with the engine's *own* report: `/state`, and a fresh
   scene query for `playing`, `loop`, live `gain`. Results: *engine reports it is running*, or *never reported
   running* together with the engine's own `last_error` / backend fields, loop and gain mismatches, a stop that
   did not stop. **A forwarded `/play` can start real playback and a live-input `/play` can open a microphone on
   the engine machine** (a confirmation is required).

`Export report...` (or `--report`) writes JSON: contract, totals, per-address statistics, effects and records.

Command line (automation; exits non-zero when findings reach `--fail-on`, default `error`):

```sh
python -m osc_controller.debugger --listen 127.0.0.1:9400 --engine-query 127.0.0.1:9000 --monitor 127.0.0.1:9101 --duration 30 --report out.json
python -m osc_controller.debugger --listen 127.0.0.1:9400 --engine-query 127.0.0.1:9000 --monitor 127.0.0.1:9101 --forward 127.0.0.1:9000
```

**Engine simulator** for testing a sender with no engine at all:

```sh
python -m osc_controller.simulator --listen 127.0.0.1:9000 --monitor 127.0.0.1:9101 --cue /demo/playback/mono --live /audioloom/Stage/live/1 --fail-start /demo/playback/glass --cue /demo/playback/glass
```

It implements cue `/play /stop /loop /gain`, observed `/state` and read-only scene discovery, and keeps simulated
state. It **never produces sound, opens a device or touches a microphone**. `--fail-start BASE` makes a cue refuse to
start (like an invalid device) so a sender's error handling can be exercised. Its transport rules are written
independently of the debugger's, and a test requires both to agree on every payload.

Limits, stated plainly: ADM/Custom messages are checked against the contract only; their effect on an engine is not
verified. Effect checks need the engine's discovery rows (`loop`, `gain`, `playing`, `last_error`), UDP is not
acknowledged, and `/state` is sent only when the worker state changes. Live comparison has been exercised against the
**Godot** engine only (headless, silent, via `SpatialFabric-Godot/Tests/Godot/serve_for_debugger.gd`); Unity and Unreal
were not run live for this feature. The checked-in Windows executable was not rebuilt.

## Profiles for other engines

Unreal, Unity and Godot all default to the same ports (spatial listener `8100`, playback control `9000`, state `9001`, ADM `50018`,
Custom example `9002`), so the controller's default profile talks to any of them. The engine profiles differ only in **cue bases**:
`profiles/spatialfabric-godot.json` (`/godot/...` plus a live-input cue) and, for Unity, `ExampleProject/OSCControllerProfile.json`
(`/unity/...`). Only one engine app can own the input ports at a time; start a second and it reports a bind error.

## Spatial / renderer

The pad is **direct ADM renderer control**. It does **not** move Unreal actors,
change stage transforms, or spatialize AudioLoom PCM. Unreal's spatial input on
8100 handles numeric feedback, not remote XYZ actor movement.

- Patch the spatial destination to the actual renderer. Default `127.0.0.1:50018`
  is only a local test destination, not a guaranteed listening device.
- Enter object ID, Front, Right, Up, linear Gain, Width and optional mute metadata.
  Click **Send once**, or deliberately enable **live changes** before dragging.
- Live mode sends only changed values, limited by Maximum Hz (1–60, default 20).
  GUI timers are best-effort, not sample-accurate scheduling. No stationary refresh
  or timetag scheduling is promised.
- Each update sends five independent datagrams: `/adm/obj/{id}/xyz`, `/aed`,
  `/gain`, `/w`, `/mute`. Device support must be checked; XYZ/AED are equivalent
  descriptions, not separate positions. These datagrams are not an atomic bundle.
- Pad axes are internal Front/+X and Right/+Y. Wire XYZ is **right, front, up**;
  AED azimuth is positive left, in degrees. Values are limited coherently to a
  unit sphere before transmission; the cyan point shows the limited XY preview.
- **Center pad** disables live and centers locally. It deliberately sends nothing
  until Send once or live is re-enabled. Live is never persisted in profiles.
- Monitor port 50018 to see received ADM objects as purple outlines (expire after
  10 seconds). The dedicated Object tracker below offers more views. Playback
  monitoring on 9001 remains active alongside the spatial receivers. Receiving
  positions never automatically retransmits them.

**Avoid competing writers:** Unreal and this app can both send to a renderer.
Disable the unwanted sender or give each a separate object-ID range. If you listen
on the same endpoint you send to, the monitor may display your own datagrams;
that is not a renderer reply.

## Object tracker — receive only

Click **Start tracking** in the Object tracker tab, or **Start all receivers** in
its top toolbar. Default spatial inputs are 50018 (ADM) and 9002 (example Custom).
Playback state on 9001 can run simultaneously. Exact duplicate endpoints inside
this app share one socket rather than competing for the port. A failed multi-port
start preserves existing listeners and releases newly created partial bindings.
Stopping one owner does not close a socket still needed by the other owner.

Views include **top, front, side, fixed isometric 3D, polar/AED and XYZ history**.
Trails, ADM/Custom filtering, per-object isolation, Fit Received, coordinate tables,
name/gain/width/mute, last-position age and observed position-update rate help debug
routing. The fixed isometric view is a projection, not a full orbit-camera renderer.
The tracker does not fabricate motion or forward incoming messages to Unreal.

ADM `/adm/obj/{id}/xyz` and `/aed` are supported, plus metadata. XYZ takes priority
while recently present so Both mode does not double-count rate or history. The
example Custom `/demo/{zone}/{id}/xyz` uses stage-normalized front/right/up. Generic
Custom templates/ranges are **not automatically inferred**. ADM and Custom values
may differ because ADM sphere-limits its position; they are not silently paired.
Sources are keyed by protocol, sender IP, local port and object ID. Different
senders on the same IP and receive port with the same ID cannot be distinguished.

At most 256 objects, 240 history samples per object and 32 displayed objects keep
rendering bounded. Select an object for a clear view when labels overlap. Old dots
are dimmed, not treated as a disconnected device: send-on-change sources can be
quiet while their last position remains valid. Start playback/PIE or otherwise
change an object if a late-starting tracker has missed the sender's initial state.
Rendering is capped near 20 Hz; the worker receives independently. Spatial packet
logging defaults off to avoid flooding the UI; turn it on in Monitor / log if needed.

## Query scene configuration from Unreal

In **Object tracker**, use **Query scene** (query IP/port default 127.0.0.1:8100;
reply port 9011). A running Playback input on 9000 also accepts this command. It
retrieves versioned JSON through bounded, correlated OSC chunks, retries timeouts,
and follows pages. Reply binding uses the tracker Listen IP. The temporary query
receiver shares exact duplicate endpoints and does not interrupt live tracking.

The **Scene inventory** view shows complete JSON (preview capped at 256 KiB; Export
JSON saves everything). **Scene** dots show explicitly labelled query snapshots,
not fabricated live OSC telemetry. **Import discovered cues…** confirms before
replacing the app cue list/playback destination; it never changes Unreal routing
or plays audio. Querying alone does not replace cues. Save a profile explicitly.
Failed/incomplete queries preserve the previous complete inventory. More than 64
distinct cue bases must be reduced deliberately instead of being silently truncated.

Remote discovery is disabled by default in Unreal; explicit trusted-network opt-in
is required. The response describes configuration/software state, not device
presence or physical audio. See [command/schema](../../Docs/SceneQuery.md) and
[runnable Max/Node-RED/Python/Node examples](../../Examples/OSCSceneQuery/README.md).

## Unreal losing frame rate when the controller has focus

In **Edit → Editor Preferences**, search **Use Less CPU when in Background**
(General / Performance) and uncheck it. The underlying setting is
`bThrottleCPUWhenNotForeground=False` in EditorPerformanceSettings. This keeps
editor/PIE processing responsive while the controller is focused; it uses more
CPU/GPU and power. Also keep the viewport realtime or run PIE for animation.
It is an editor preference, not a packaged-game setting or a latency guarantee.
Do not disable every Slate throttle or force an unlimited frame rate unnecessarily.

## Message composer

Choose an independent destination and absolute OSC path. Arguments are **one per
line**, with literal typed values:

```text
f 0.5
i 1
s Main stage
```

Standalone `T`, `F`, `N` are supported for generic OSC equipment. Prefer numeric
forms for AudioLoom Play/Loop; supported OSC tags do not imply transport semantics.
Leave the argument box empty for an address-only message. Nothing is evaluated
as Python or shell code. There is no automatic repeat.

## Monitor and network safety

Default listen address is literal loopback. Binding verifies the real socket and
fails if another application owns the endpoint; address reuse is never enabled.
The app internally shares duplicate endpoints between tracker and monitor.
Stop other capture/controller programs if port 9001/50018/9002 is already occupied.
Changing the listen fields does not rebind an active socket: press Start monitor
to restart. The displayed bound endpoint remains authoritative.

For a second computer, use its reachable IPv4 address, patch both directions and
allow only the required UDP ports on trusted networks. The app accepts literal
IPv4 unicast destinations, not DNS names, IPv6 or multicast. `0.0.0.0` is allowed
only for an explicit all-interface listener, after confirmation. UDP/OSC has no
authentication, encryption or guaranteed delivery. Never expose it to the Internet.

Incoming messages support `i/f/s/b/h/d/T/F/N` and bounded bundles; timetags are
**not scheduled**. Unsupported tags, malformed padding/lengths, non-finite numbers,
overdeep bundles and oversized packets reject the whole datagram. Each listener's event queue
is bounded to 2048; the log retains only the latest 1000 records. Excess events
are dropped and counted rather than growing memory indefinitely. Export CSV is
explicit; logs are not written automatically. The monitor does not authenticate
senders: reported state may be spoofed on an untrusted network.

## Profiles

Load/Save JSON explicitly; see `profiles/spatialfabric-showcase.json`. Profiles
store endpoints, cue names/bases and spatial settings. They also store tracker receive IP/ports. Old 1.0 profiles still load using default
tracker settings. They contain no credentials and never store live-enabled or
listener-running state. Validation occurs before
application; invalid profiles do not replace settings. Saving uses atomic replace.
The composer and last-reported state are intentionally not persisted.

## Build the Windows executable

On Windows, use a regular **64-bit Python 3.13 with Tk**:

```powershell
py -3.13 -m venv .venv
.venv\Scripts\python -m pip install -r requirements-build.txt
.venv\Scripts\python build_windows.py
# Avoid overwriting an older executable that is still open:
.venv\Scripts\python build_windows.py --output-dir dist/1.4.0
```

Builds run the protocol/profile/UDP tests, use PyInstaller, bundle Python/Tk and
collect runtime license notices. One upstream Tcl license download may be needed
if the Python distribution omits it. No Unreal build or native SDK is involved.
Build artifacts stay under ignored `dist/`; source is independently runnable.
Read `THIRD_PARTY_NOTICES.md` and **About / licenses** in the app.

## Test and limits

```powershell
python -m unittest discover -s tests -v
# Optional on an interactive desktop; sends only to ephemeral loopback sockets:
$env:OSC_CONTROLLER_GUI_TESTS = '1'
python -m unittest discover -s tests -v
# Check packaged startup without sending or binding:
dist\SpatialFabricOSCController.exe --smoke-test "$env:TEMP\osc-smoke.json"
```

**Live-input cues (unreleased, source only):** 43 tests pass (34 core, including 4 new model tests, plus 9 interactive GUI tests, 4 of them new, run with the GUI flag set): kind validation and old-profile compatibility, profile round trip, confirmed Start that sends nothing when declined, unconfirmed Stop, refused Loop, bounded Gain (NaN/inf/bool/string/out-of-range refused), `live_input` import that sends nothing, and the LIVE state label. The checked-in Windows executable and ZIP were **not rebuilt**; rebuild with `build_windows.py` before distributing. The controller was not run against a live Unreal/Unity/Godot instance for this feature (wire format and addresses are covered by encode/decode tests and each engine's own tests).

Local validation: Windows 11 x64, Python 3.13.14 / Tk 8.6; **30 core tests and
5 interactive GUI tests passed**, including paged scene queries and confirmed cue
import without transport commands. A live Unreal loopback Loop-control check restored
the original value without starting audio. A separate eight-second live tracker
check received **8,474 messages / 8 ADM+Custom objects**, about 30 position updates/s
while Unreal was unfocused. Physical hardware integration remains unverified.

Tests cover wire bytes, bounded/atomic parsing, profiles, socket ownership,
loopback sending/receiving and opt-in UI transport/composer behavior. Startup smoke
checks verify no implicit network activity. This does not certify a renderer,
physical audio, latency, channel fidelity, packet delivery, or every OS. No network
operations or audio are required merely to open the UI.
