# AudioLoom feature-completion follow-up

Scope: close the AudioLoom playback gaps identified in
[AUDIOLOOM_DEPRECATION_AUDIT.md](AUDIOLOOM_DEPRECATION_AUDIT.md) across the actual
Unreal, Unity and Godot implementations. The SpatialFabric hub owns the shared
contract and tools; it is not a fourth runtime. This report supersedes the audit's
implementation snapshot only where fresh evidence below establishes completion.
It does not replace hardware/deployment acceptance or authorize removal of retained
Unreal AudioLoom modules/classes.

## Implementation and validation

**The audited playback-feature gaps are implemented across all three runtime
ports, and the previously blocked engine/build gates now pass.** Unity licensing works;
actual engine suites pass, Godot editor teardown is clean, and Unreal Development/
Shipping plugin builds pass. Full hardware/platform/deployment acceptance is still
not certified. See each port's parity document:

- [Unreal](https://github.com/NPLAudio/SpatialFabric-Unreal/blob/f00c574d2b495d842a74caf75db414c4a8984547/Docs/PlaybackParity.md)
- [Unity](https://github.com/NPLAudio/SpatialFabric-Unity/blob/7fb1de39df4609a100151bece48f700fcb61a126/Docs/PARITY.md)
- [Godot](https://github.com/NPLAudio/SpatialFabric-Godot/blob/5b398b7ddaaba3f5309a6d4965b29e58448e9ed8/Docs/PARITY.md)
- [Shared acceptance contract](../Specs/Playback-Parity-v1.md)

### Shared native backend

Windows now negotiates exclusive-preferred output and then shared output on the
same resolved endpoint, using fresh clients including alignment retry. An invalid
configured endpoint or channel still fails closed; no device/mixer substitution.
Actual active mode and cached OS output latency are additive APIs, with unchanged
existing C status layouts. Buffer duration and OS-reported latency are distinct.
Core Audio HAL latency support is implemented but not built on this Windows host.
Godot's vendored source and SHA-256 provenance are synchronized.

Fresh Windows checks: **55 silent native checks** and **55 fake-COM WASAPI
negotiation/diagnostic checks** passed, including failure of alignment reactivation
and failure of shared reactivation. No valid audio stream was opened.

### Unreal

Restored bounded package-sibling WAV fallback (with explicit opt-out) and added a
configurable decoded-PCM budget while preserving the 256 MiB default. All original
serialized classes, legacy CSV and playback/OSC/editor features remain in place.
UE 5.8 Editor Development, Game Development and Game Shipping compilation plus
BuildPlugin distributable packaging passed; **36/36 automation tests passed** and
**33 Python tests passed**. Development compilation found a missing direct
`UObject/Package.h` include; it was fixed before the successful full rerun.
The new SoundWave-to-sidecar test has one expected imported-data warning from its
empty synthetic asset before successful fallback. Evidence:
`C:/Users/davo_/AppData/Local/Temp/SpatialFabricValidation-9vxp0dra/`.

A subsequent **actual Development game cook/stage/package/archive passed** with
IoStore and all 13 published assets. Packaged startup/content checks passed, but
**the silent smoke is partial: 16/22 checks passed, six loader/PrepareAudio assertions
failed; player exit 7**. UE 5.8 leaves `FSoundWaveProxy_InvalidFormat` under `-NoSound`.
The unchanged ordinary audio-enabled loader path remains unverified; this is not
proof of a corrupt cook. A separate device-free BINKA diagnostic decoded all three
actual cooked payloads successfully. Explicit sibling-WAV lookup also passed in a
separate archive copy with manually deployed sidecars; UAT did not stage those files.
These diagnostics do not turn the unchanged loader's failed assertions into passes.
Evidence: `C:/Users/davo_/AppData/Local/Temp/SpatialFabricCookValidation-012t2fc_/VALIDATION_REPORT.md`.
No physical routes, Shipping-game runtime or full packaged migration are certified.

### Unity

Added fixed/random replay, runtime-only opt-in startup, blank-derived unique OSC
bases with preserved overrides/conflict diagnostics, active-only route/source
restart and global feedback suppression. Existing manager/source/CSV/Undo workflows
remain; new OS latency/mode diagnostics support the updated native backend.
Review also fixed stop-from-EOF callback re-arming and failure cancellation.

Fresh checks: **196 portable CoreChecks passed**, **52 Python passed, 2 skipped**.
Unity 6000.6.9f1 actual engine execution: **81/81 EditMode + 6/6 PlayMode passed**,
zero failures/skips, with graphics and required native integrations. The earlier
license error preceded successful activation; the fresh Editor accepts the license.
A test's flat Editor JSON assumption was corrected to round-trip actual serialized
data; legacy OSC overrides are preserved. Evidence:
`C:/Users/davo_/AppData/Local/Temp/SpatialFabricUnityValidation-m62necnh/`.

The final engine run used a freshly rebuilt current DLL in the isolated copy only;
no working-project DLL or active Editor was replaced. The Windows native build gates
both 55-check suites. The reusable `--isolated` runner copies authored inputs/current
package, disables engine audio and uses a unique validation persistent-data identity.
It verifies individual NUnit results/counts and rejects unknown skips. Earlier test
attempts additionally generated Unity's default LocalLow TestResults.xml outside the
evidence folder; it was left untouched afterward. Unique identity prevents sharing
that path with working-project user data in subsequent isolated runs.

Actual **Mono and IL2CPP Windows Development players** subsequently built and ran;
native availability, offline PCM/EOF, missing-device refusal, no active playback and
9 scene records all passed independently in JSON. The current native DLL deployed
once with matching SHA-256 `5cd022ea431c603ecf98d5f560116da18ce2d9cc498c151ce8d9ae4eed9ea962`.
At the user's request, installed only Windows Build Support (IL2CPP) for 6000.6.9f1
through the official Unity CLI, then reran the isolated IL2CPP player successfully.
Evidence: `C:/Users/davo_/AppData/Local/Temp/SpatialFabricUnityPlayer-in8uzb2i/` (Mono)
and `C:/Users/davo_/AppData/Local/Temp/SpatialFabricUnityPlayer-0_cf4jwx/` (IL2CPP).
The reusable player validator now supports isolation/native override and checks
complete assertions, not just exit status. Review also added a pre-write guard
against recursive copying into custom source projects and a deployed-DLL hash check. An additional non-Development IL2CPP player also passed at
`C:/Users/davo_/AppData/Local/Temp/SpatialFabricUnityPlayer-whkiijgj/`: only the disposable
build driver's options changed to StrictMode without Development; source stayed
unchanged and project-default stripping was retained. Clean-snapshot release
publishing, other stripping profiles and arbitrary asset migration remain separate.

### Godot

Added random replay/runtime startup, derived/custom OSC/conflict diagnostics,
active-only route/source restart, full playback-cue authoring manager, draft WAV
selection and bounded native/legacy CSV workflows with real EditorUndoRedoManager.
Exposed native OS latency/mode in status/discovery/manager/showcase. Review fixed
EOF-subscriber cancellation, disabling pending replay, nested-base feedback and
playback/live editor conflicts. Most-specific bases own nested commands; exact
address duplicates retain diagnosed fan-out. Long finite replay delays are preserved.

Fresh Godot 4.7.2 checks: **308 headless/native passed**, **18 editor authoring
assertions passed with clean shutdown**, **22 Python passed**. Debug/release wrappers
built. The former custom SceneTree editor runner's leaks reproduced without the
addon; switching only the test architecture to an opt-in EditorPlugin in a disposable
project resolved them. The new gate rejects warnings/errors/leaks after editor exit.
Runtime evidence: `C:/Users/davo_/AppData/Local/Temp/SpatialFabricGodotChecks-r4b0kq9a/`.
Editor evidence: `C:/Users/davo_/AppData/Local/Temp/SpatialFabricGodotEditor-u8qlgo8d/`.
Its temporary CSV fixture is confined to the evidence directory, not shared `user://`.

CSV imports reject whole invalid/ambiguous rows, allow independent valid rows, and
apply accepted changes as one Undo transaction. Legacy Unreal source/device
references require unique existing WAV/device mappings, not fake asset conversion.

A fresh **Windows release export** passed headless/windowed self-checks: native
extension, 10 inventory records, 3 retained example WAVs decoded, inactive startup
playback/live/network, plus rendered screenshot, notices/local ZIP checks. Explicit
ADM loopback received 131 XYZ packets on four object addresses. Dummy audio only.
Evidence: `C:/Users/davo_/AppData/Local/Temp/AudioLoom-release-validation-9dlayt1f/`.
Source/copied/exported DLL hashes match; all 24 expected exports exist. Vendored
hashes identify the current dirty source bytes, but three native files differ from
the recorded upstream commit; that commit alone is insufficient build provenance.
This validates the current example export, not arbitrary migrated content or hardware.

### Shared tooling

Shared Tools: **8 passed**. OSC Controller: **65 passed, 14 skipped**; its existing
unclosed-report-file ResourceWarning remains nonfatal. No protocol or fixture
version bump was required for these local playback-policy changes.

## Deployment/deprecation boundary

Keep legacy releases and backups. For Unreal, remove the standalone plugin and its
project entry before installing SpatialFabric-Unreal, then rebuild; both declare
the same modules. Verify exact OSC paths/network opt-ins, cooked content/sidecars,
memory budgets, source identity and physical channel/device assignment.

Bounded in-memory decoding is intentional, not arbitrary long-form streaming.
Unity GUIDs, Godot `res://` paths and Unreal SoundWave references require explicit
asset mapping. Do not infer hardware latency, hot-unplug robustness, Apple platform
support or a successful packaged migration from silent/software test passes.

No actual playback or microphone capture was started during validation. Existing
Unity `ExampleProject/ProjectSettings/InputManager.asset` and Godot `project.godot`
user edits were preserved. The initial validation phase made no commits/releases;
a subsequent user-authorized commit/push and local release phase is recorded below.
No blanket deprecation, repository archival, Git tag or GitHub release is implied.

## Authorized commit/push and local release phase

The user subsequently requested commits, pushes and necessary local builds.
Runtime/legacy changes were committed and pushed on `main`:

| Repository | Commit |
| --- | --- |
| AudioLoom | `eddbcaa3ea414ddb735ec9cac869764173382eec` |
| SpatialFabric-Unity | `7fb1de39df4609a100151bece48f700fcb61a126` |
| SpatialFabric-Godot | `5b398b7ddaaba3f5309a6d4965b29e58448e9ed8` |
| SpatialFabric-Unreal | `f00c574d2b495d842a74caf75db414c4a8984547` |

The shared hub retains this report, the historical audit, acceptance contract and
[local artifact manifest](LOCAL_RELEASE_MANIFEST_2026-11-18.json). The pre-existing
Unity InputManager and Godot project settings edits were excluded from commits and
release snapshots. No old AudioLoom binary was rebuilt for documentation-only changes.

Fresh Git-bearing clones produced eight verified archives under:
`C:/dev/UnrealSpatial/LocalReleases/2026-11-18-audioloom-parity/`.
`RELEASES.json`, `SHA256SUMS.txt` and per-engine summary JSONs identify exact inputs,
outputs and checks. Existing version numbers were retained in this unique local
folder; no published assets were replaced and no Git tags/GitHub releases were created.

- **Unity 0.7.0:** UPM with fresh native DLL, complete-source ZIP and non-Development
  Mono showcase ZIP. **55 native + 55 mocked checks**, **53 release-player checks**
  and **five rendered views** passed. Fifteen pinned notice documents verified.
  The fresh committed-source DLL also passed isolated IL2CPP player smoke checks.
  DLL SHA-256: `a0cf687350f4b980f4b65aafc222faa963c1f98c3a5308a2d12d9da5d5afe963`.
- **Godot 0.2.0:** release showcase ZIP, rebuilt debug/import dependency and release
  wrapper; headless/windowed, 24 exports, three WAVs, ten inventory records and four
  ADM loopback XYZ addresses passed. DLL SHA-256:
  `5514088c434da07db1298f7773dabec9a922b5f2b90b1823a680e3df62ee1827`.
  Provenance was refreshed after the Unity commit and all five vendored hashes match
  that committed tree; the earlier dirty-upstream caveat is superseded for this build.
- **Unreal 2.2.0:** source plugin/example/complete-source ZIPs and a UE 5.8 Win64
  binary-plugin ZIP. Editor Development, Game Development and Game Shipping builds
  passed. This rebuild did not rerun or fix the six `-NoSound` loader assertions.

All eight archive integrity checks passed and SHA-256 inventories were recorded.
These local builds do not waive the physical, Apple-platform or Unreal packaged
loader/migration limitations described above.
