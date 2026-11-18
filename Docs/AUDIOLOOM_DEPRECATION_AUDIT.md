# AudioLoom deprecation audit

Date: 2026-11-18. Scope: local source trees, not remote releases or hardware certification.

> Historical pre-implementation snapshot. Subsequent feature changes and fresh
> validation are tracked in [AUDIOLOOM_FEATURE_COMPLETION.md](AUDIOLOOM_FEATURE_COMPLETION.md).
> Findings below describe the inspected revisions, not necessarily the current
> working tree; deployment/deprecation acceptance remains a separate gate.

## Decision

**Do not confirm the proposed all-engine feature-parity condition. It is not met.**

The standalone AudioLoom repository can reasonably remain a legacy distribution in favor of **SpatialFabric-Unreal** for new development, subject to migration acceptance for existing projects. Even Unreal has content-compatibility limits that prevent an unconditional replacement claim. That is a narrower decision than claiming every SpatialFabric port fully replaces it. Deprecating the standalone distribution must not remove the retained AudioLoom modules/classes from Unreal.

## Revisions inspected

| Tree | HEAD | Working-tree qualification |
|---|---|---|
| `AudioLoom` | `a677b43` | Clean at audit start |
| `SpatialSandbox/Plugins/SpatialFabric` (SpatialFabric-Unreal) | `0267fd3` | Clean at audit start |
| `SpatialFabric-Unity` | `0574f10` | Existing modification to `ExampleProject/ProjectSettings/InputManager.asset` |
| `SpatialFabric-Godot` | `1e61ed9` | Existing modification to `project.godot` |
| `SpatialFabric` | `29e5553` | Shared hub, **not an engine runtime** |

No application source, deprecation flag, repository archival state or existing user modification was changed by this audit.

## Summary matrix

| Baseline capability | SpatialFabric-Unreal | SpatialFabric-Unity | SpatialFabric-Godot |
|---|---|---|---|
| Direct device/channel PCM, WAV, loop/fixed replay | Implemented, expanded; stricter content limits | Implemented, expanded | Implemented, expanded |
| Random min/max replay | Retained | Missing | Missing |
| Opt-in BeginPlay/startup playback | Retained | Requires custom scripting | Requires custom scripting |
| Automatic shared fallback from exclusive request | Retained | Not provided | Not provided |
| Bulk routing CSV and management UI | Retained, expanded | Engine-native equivalent; not legacy Unreal asset/CSV interchange | Missing equivalent workflow |
| Derived default OSC paths/conflict workflow | Retained with migration changes | Literal shared default; authoring gap | Literal shared default, explicit bindings; authoring gap |
| Live route changes / OS-latency API | Retained | Reduced/different | Reduced/different |
| Observed state, ack/status, discovery, live input | Added | Added | Added |
| Fresh physical/platform certification in this audit | Not performed | Not performed | Not performed |

All implementation judgments are source-level, not guarantees of audible output. Detailed evidence and qualifications follow.

## AudioLoom baseline

The baseline is actual implementation, not just its README feature list:

- Unreal actor/component and Blueprint APIs; SoundWave loading; selected/default output device; channel 0 = corresponding source/output channels, positive channel = first source channel routed to that output.
- Windows WASAPI and macOS Core Audio. Windows exclusive request with automatic shared-mode fallback, configurable buffer and output-latency estimate.
- Play/stop, looping, optional BeginPlay playback, fixed **and randomized min/max replay delay**.
- Derived/custom OSC addresses, play/stop/loop commands, monitoring, configurable ports/monitor target and address validation.
- Editor management/device/source controls, duplicate-path warnings and routing CSV import/export, including legacy file matching.

Primary evidence: `AudioLoom/Source/AudioLoom/Public/AudioLoomComponent.h:42-168`; `AudioLoom/Source/AudioLoom/Private/AudioLoomComponent.cpp:117-163`; `AudioLoom/Source/AudioLoom/Private/AudioLoomPlaybackBackend.cpp:143-177`; `AudioLoom/Source/AudioLoomEditor/Private/AudioLoomRoutingCsv.cpp:193-475`.

## Confirmed blockers to the all-ports condition

### 1. Randomized replay is missing from Unity and Godot

AudioLoom implements `bRandomReplayDelay`, `ReplayDelayMin`, `ReplayDelayMax` and random delay selection, not merely a fixed replay interval:

- `AudioLoom/Source/AudioLoom/Public/AudioLoomComponent.h:86-100`
- `AudioLoom/Source/AudioLoom/Private/AudioLoomComponent.cpp:135-163`

The inspected Unity cue exposes only `replayDelaySeconds` and passes that fixed value to replay observation:

- `SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/Playback/PlaybackCue.cs:25-26`
- `SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/Playback/PlaybackCue.cs:107`

Godot likewise schedules only `_replay_remaining = replay_delay_seconds`:

- `SpatialFabric-Godot/addons/spatial_fabric/runtime/playback_cue.gd:18-20`
- `SpatialFabric-Godot/addons/spatial_fabric/runtime/playback_cue.gd:148-149`

### 2. Godot lacks AudioLoom-equivalent routing CSV and management workflows

AudioLoom provides substantive CSV export/import and editable per-component controls. Godot's dedicated dock only inspects and displays scene JSON; its roadmap explicitly lists transactional CSV, source import and full authoring panels/Undo as unimplemented:

- `AudioLoom/Source/AudioLoomEditor/Private/AudioLoomRoutingCsv.cpp:193-475`
- `AudioLoom/Source/AudioLoomEditor/Private/UI/SAudioLoomExpandableRow.cpp:214-647`
- `SpatialFabric-Godot/addons/spatial_fabric/plugin.gd:7-35`
- `SpatialFabric-Godot/Docs/ROADMAP.md:23-24`

Runtime cue properties and keyboard controls do not replace this bulk authoring/import/export workflow. Missing Custom spatial output alone is **not** an AudioLoom parity blocker: standalone AudioLoom never supplied that feature.

### 3. Replacement behavior is deliberately not identical

AudioLoom automatically retries unsupported Windows exclusive initialization in shared mode. Unity's native backend instead fails with `Exclusive mix format unsupported (no silent shared fallback)`; Godot uses the same native C ABI implementation vendored into its tree. This is a defensible fail-closed policy, but not transparent behavior parity:

- `AudioLoom/Source/AudioLoom/Private/AudioLoomPlaybackBackend.cpp:143-177`
- `SpatialFabric-Unity/Native/src/audio.cpp:124-139`
- `SpatialFabric-Godot/Native/vendor/sf_audio/src/audio.cpp:136-153`

AudioLoom also has a built-in opt-in BeginPlay playback property. Neither inspected Unity nor Godot cue has an equivalent built-in startup option; explicit engine scripting can implement it, but that is a migration task rather than feature equivalence. Their no-startup-I/O defaults are intentional and should remain the default.

### 4. Address authoring and diagnostics are reduced

AudioLoom derives a default OSC address from hierarchy-root label/component index, validates overrides and discovers world components when listening starts. Godot uses a literal `/godot/playback/cue` default and explicit `cue_paths`; authors must configure addresses/bindings. Its dock has no equivalent duplicate-address/device/channel management or output-latency display:

- `AudioLoom/Source/AudioLoom/Private/AudioLoomComponent.cpp:303-338`
- `AudioLoom/Source/AudioLoom/Private/AudioLoomOscSubsystem.cpp:67-95`
- `SpatialFabric-Godot/addons/spatial_fabric/runtime/playback_cue.gd:11-20`
- `SpatialFabric-Godot/addons/spatial_fabric/runtime/spatial_manager.gd:58-64`
- `SpatialFabric-Godot/addons/spatial_fabric/plugin.gd:7-35`

Unity also defaults every playback cue to the literal `/unity/playback/cue` and dispatches to every matching cue. Duplicate defaults can therefore cause unintended fan-out rather than unique per-instance control (`SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/Playback/PlaybackCue.cs:20`; `SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/SpatialFabricManager.cs:193-205`). Deliberate shared-address groups can be useful, but need explicit authoring/conflict diagnostics.

AudioLoom restarts active playback after editor routing changes; Godot's exported device/channel fields have no equivalent live restart handling (`AudioLoom/Source/AudioLoom/Private/AudioLoomComponent.cpp:167-190`; `SpatialFabric-Godot/addons/spatial_fabric/runtime/playback_cue.gd:14-17,95`). Unity similarly applies routing at start and only updates loop during regular playback ticks (`SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/Playback/PlaybackCue.cs:83-98`). Its buffer-frame/rate diagnostics are useful but not equivalent to AudioLoom's OS stream-latency API.

### 5. Platform/hardware equivalence is not established

Godot documents macOS runtime/Core Audio as unvalidated. Unity's native macOS build/silent checks are not Unity-editor/player or physical-output validation. Neither port's added live-input, discovery or acknowledgement features prove physical routing/latency equivalence.

- `SpatialFabric-Godot/VALIDATION.md`, especially "Not measured / not implemented"
- `SpatialFabric-Unity/Docs/PARITY.md`, "External validation gates"
- `SpatialSandbox/Plugins/SpatialFabric/VALIDATION.md`, "macOS Apple Silicon: user-reported validation"

AudioLoom itself has no checked-in automated test suite establishing all its advertised platform behavior. Absence of evidence must not be treated as either certified success or a proven runtime failure.

### 6. Unreal also narrows some previously accepted content

The successor removes automatic sibling `.wav`/`.WAV` fallback from `LoadFromSoundWave()`. Its bounded loader caps decoded float PCM at **256 MiB**, rates at **8–384 kHz** and channels at **1–64**, and rejects streaming cooked SoundWaves and unmapped multichannel Vorbis ordering. The decoded limit is about **11 minutes 39 seconds of stereo 48 kHz audio**, shorter for wider cues. These safety policies are defensible, but long installation cues and sidecar-dependent content need explicit migration; added features do not erase that restriction.

- `AudioLoom/Source/AudioLoom/Private/AudioLoomPcmLoader.cpp:177-199`
- `SpatialSandbox/Plugins/SpatialFabric/Source/AudioLoom/Private/AudioLoomPcmLoader.cpp:11-23,54-59,73-85,100-135`

Use appropriate Force Inline/cooked codec settings, inventory cue lengths/channel layouts and test actual packaged assets. `LoadFromFile()` still exists, but does not restore the removed automatic SoundWave sidecar fallback.

## Implemented improvements (not substitutes for gaps)

Unity and Godot implement direct-device playback, selected/default output, per-channel routing, WAV decoding, fixed replay, loop control, OSC commands and observed state. They add live input, scene discovery and command acknowledgements/status. Native rate conversion and broader WAV support exceed the legacy implementation; observed worker state and live loop changes correct legacy weaknesses.

Do not demand preservation of legacy defects as features. For example, actual legacy `/play` uses `TriggerValue > 0.5f || Floats.Num() == 0`, so float 0.5 does not play while integer-only 0 does. Successors implement the documented numeric trigger contract instead. This is an intentional compatibility change to test with existing controllers, not a reason to reproduce unsafe or erroneous behavior (`AudioLoom/Source/AudioLoom/Private/AudioLoomOscSubsystem.cpp:163-216`; `SpatialFabric-Godot/addons/spatial_fabric/runtime/playback_control.gd:15-30`).

## Additional source-confirmed operational risk

Unity classifies incoming `/ack` and `/state` as ignored, but still responds with an acknowledgement. A monitor endpoint pointed back at a cue-command input, or a reflecting bridge, can sustain acknowledgement traffic. This was identified by code inspection, not a live flood test. Suppress responses to engine-only feedback or reject self-referential configurations before claiming robust replacement behavior:

- `SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/Osc/PlaybackControl.cs:18`
- `SpatialFabric-Unity/Packages/com.nplaudio.spatialfabric/Runtime/SpatialFabricManager.cs:174-190`

## Unreal migration boundary

The actual Unreal successor is `SpatialSandbox/Plugins/SpatialFabric`, whose Git remote is SpatialFabric-Unreal. `SpatialFabric/README.md:6-18` identifies the sibling `SpatialFabric` tree as the shared hub, not the former Unreal plugin.

Unreal retains AudioLoom runtime/editor module names, `/Script/AudioLoom` classes, settings section, CSV columns and hidden legacy tab. It also retains randomized replay and BeginPlay playback:

- `SpatialSandbox/Plugins/SpatialFabric/SpatialFabric.uplugin:27-34`
- `SpatialSandbox/Plugins/SpatialFabric/Source/AudioLoom/Public/AudioLoomComponent.h:78-102`
- `SpatialSandbox/Plugins/SpatialFabric/Docs/AudioPlayback.md:201-219`

Migration still requires backing up the project, removing duplicate standalone modules/plugin entries, installing only the combined plugin, rebuilding and checking external OSC addresses/configuration. Documented changes include descendant OSC address segments, loopback receive defaults, packaged-network opt-ins and missing-device fail-closed behavior. Serialized class retention does not prove unchanged show-control behavior. Do not install both plugins together. The backend `Start()` adds a defaulted sample-rate parameter, preserving ordinary source calls but not binary ABI or function-pointer signatures; rebuild external C++ clients as well (`SpatialSandbox/Plugins/SpatialFabric/Source/AudioLoom/Public/AudioLoomPlaybackBackend.h:33-61`).

AudioLoom's README already labels it legacy but points Unreal users at the now-renamed hub. Update that recommendation to **SpatialFabric-Unreal** when formalizing deprecation (`AudioLoom/README.md:13-26`; `SpatialFabric/README.md:14-18`).

## Validation executed during this audit

All checks below were run without starting engine editors, playback or capture:

| Check | Result |
|---|---|
| Unreal plugin Python Tools suite | 33 passed |
| Unity Python Tools suite | 45 discovered: 43 passed, 2 skipped |
| Godot Python Tools suite | 18 passed |
| Shared hub Python Tools suite | 8 passed |
| Shared OSC Controller tests | 79 discovered: 65 passed, 14 skipped |
| Unity portable .NET CoreChecks | 188 passed |
| Unity/Godot shared-fixture provenance and Godot native-source sync checks | Passed |

Post-check Git status matched the pre-audit repository modifications. `git diff --check` reported trailing whitespace in the already-modified Unity `ExampleProject/ProjectSettings/InputManager.asset`; this audit did not change or repair that file. The other checked engine/baseline diffs had no whitespace findings.

Controller tests also emitted a non-fatal `ResourceWarning` for an unclosed test report file. A first hub-suite invocation from the workspace root failed because that root has no `Tools` directory; the corrected invocation inside `SpatialFabric` passed as listed above.

These are structural, portable policy/codec/WAV/CSV and software checks—not fresh engine builds, interactive UI tests, audible playback, real microphone tests or hardware latency measurements. Existing validation documents were reviewed as historical evidence, not reported as fresh audit runs. The Unreal reviewer additionally inspected `C:/Users/davo_/AppData/Local/Temp/SpatialFabricAckFinalGate/Report/index.json:18-24`: 35 historical automation tests succeeded, with zero failed/not-run/in-process; all 95 current source files byte-match that retained snapshot. This strengthens source/build evidence but is not fresh hardware certification. Existing live integration evidence explicitly did not verify physical audio (`SpatialSandbox/Saved/SpatialFabricDemo/runtime_validation.json:2-29,67-68`). Current acknowledgement validation was Editor-only; packaged codec playback and older engine/Intel Mac compatibility remain gates. Skipped tests are not passes.

## Acceptance gates before the user's broad condition can be confirmed

1. Supply randomized replay and an opt-in startup-playback equivalent in Unity/Godot, or explicitly agree to retire those capabilities rather than claiming complete parity.
2. Implement and test Godot routing CSV/source assignment and an AudioLoom-equivalent management workflow, including persistence/Undo as applicable.
3. Resolve/document exclusive-mode fallback and diagnostics differences; test chosen policies on target devices.
4. Validate claimed platforms per engine and perform physical device/channel, EOF/loop/replay, latency, exclusive-mode and disconnect acceptance.
5. Run a backed-up real AudioLoom Unreal project migration and verify assets (including long/sidecar-dependent cues and packaged codecs), Blueprints, settings, legacy CSV and external OSC cues. CSV is not a full backup or atomic migration: preserve originals, inspect row warnings and compare a post-import export.
6. Update repository links and publish a precise deprecation notice: standalone distribution superseded by SpatialFabric-Unreal; Unity/Godot have their own stated parity and validation boundaries. Preserve legacy tags/source and the Unreal AudioLoom compatibility modules.

**Bottom line:** extra spatial/live-input capabilities do not compensate for missing playback/authoring features. The strict all-SpatialFabrics prerequisite is currently false; a narrower standalone-to-Unreal deprecation can be considered separately.
