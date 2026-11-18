# AudioLoom playback parity v1

This is an implementation/acceptance contract for the AudioLoom feature baseline
across SpatialFabric-Unreal, SpatialFabric-Unity and SpatialFabric-Godot. It is not
a new OSC wire version, a physical-audio certificate or a claim that engine asset
formats/CSV identifiers are interchangeable. OSC-v1 remains authoritative on wire.

## Required feature surface

- Explicit source preparation and direct-device playback, independent of the engine
  mixer. Default or configured output endpoint, channel 0 corresponding-channel
  mapping, positive channel routing source channel 0 to the chosen output.
- Play, stop, observed playing/ended/failed state, live loop changes, opt-in startup
  playback (off by default), and fixed or randomized natural-completion replay.
- Random delay is selected once per new natural end, never every poll. Sanitize
  nonfinite/negative delays; minimum is at least zero, maximum is at least minimum.
  Stop/failure/unregister cancels replay; loop does not schedule replay. Zero delay
  is valid. Replay is frame/ticker scheduled, not a sample-clock guarantee.
- Explicit output inventory, Windows exclusive-preferred playback with shared
  fallback on the **same endpoint**, actual-mode diagnostics, configurable buffers,
  and OS-reported output latency where supported. Never interpret an invalid
  configured endpoint/channel as permission to select another route. Fallback must
  not change the physical endpoint. Buffer duration and OS latency must be labeled
  separately; neither is a measured acoustic round trip.
- Derived unique default OSC addresses plus explicit overrides, syntax validation,
  duplicate-address diagnostics, typed play/stop/loop, observed state and monitoring.
  Preserve existing nonempty authored overrides during upgrade. Names/keys used in
  derivation and rename/reparent behavior must be documented per engine.
- Engine-only `/ack` and `/state` feedback must not receive another acknowledgement
  or execute transport commands. This rule also applies to live-input routing.
- Apply active route/source changes coherently with a stop/join/restart, or reject
  the operation explicitly. Editing an idle cue must never start it. Bulk import
  must not silently start audio or discard a configured endpoint.
- An editor manager with source/device/channel/OSC and transport/replay controls,
  conflict/error/latency visibility, source assignment and routing CSV import/export.
  Scene/asset edits must support engine-native Undo and persistence. Validate CSV
  before mutation; report unmatched/ambiguous routes and preserve existing values
  when a row cannot be accepted. Document atomic versus partial-application policy.
- Preserve legacy Unreal class/module identities and provide a backed-up migration
  path for SoundWaves, sidecar WAVs, Blueprints, configuration and routing CSVs.

## Safety and compatibility boundaries

No automatic listener, microphone or playback is enabled merely by installing,
importing, inspecting, discovering or rendering the editor UI. Startup playback
requires an explicitly authored per-cue opt-in and actual runtime startup. Example
stations remain opt-out. Remote/packaged networking stays opt-in and unauthenticated
OSC must be confined to trusted/firewalled networks.

Malformed/nonfinite audio and packets must fail closed. Bounded decoding, finite
memory budgets, supported rate/layout checks and explicit codec/cook policies are
intentional; do not restore legacy out-of-bounds reads, inaccurate sample-rate
handling or accidental nonnumeric triggers in the name of parity. Long-form cues
must fit the documented preparation budget or use an explicit supported migration
path. Do not advertise arbitrary streaming when playback prepares whole PCM files.

CSV compatibility means comparable workflow, not that a Unity GUID, Godot `res://`
path and Unreal `/Game/...` object reference identify the same asset. Legacy CSV
import must either resolve the reference, require an explicit mapped source, or
report it as unmapped—never pretend that the source was transferred.

## Validation gates

1. Portable deterministic tests: WAV shapes/bounds, replay including one draw per
   completion and cancellation, OSC thresholds/feedback suppression, route policy
   and quoted CSV round-trip/validation.
2. Native build/offline tests: routing, resampling/EOF, buffer lifecycle, errors,
   exported ABI and latency/fallback availability without opening real streams.
3. Engine checks: script/C++ compile, silent editor/headless tests, startup opt-in
   lifecycle, Undo/redo, scene save/reload, and preserved authored OSC overrides.
4. Deployment acceptance: real packaged assets, target OS/engine combinations,
   physical channel/device assignment, exclusive/shared negotiation, latency,
   hot-unplug, long duration and natural end/replay. Track these separately from
   software passes; do not claim untested platforms from source presence alone.

Deprecating the standalone distribution is separate from deleting AudioLoom's
retained Unreal modules or retiring support for an unqualified existing deployment.
Keep legacy releases and rollback instructions until migration is accepted.
