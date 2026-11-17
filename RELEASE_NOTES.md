# SpatialFabric 1.0.0 — shared hub and OSC Controller 1.4.0

First release of the shared hub: OSC contracts, JSON schema, golden fixtures and
engine-neutral controller used by the Unreal, Unity and Godot integrations.

The former Unreal repository is now [SpatialFabric-Unreal](https://github.com/NPLAudio/SpatialFabric-Unreal).
The former SpatialFabric-Interop repository is this hub; both histories are preserved.
See the README for links to every engine's source, examples and releases.

## OSC Controller 1.4.0

- Portable unsigned Windows controller, debugger and silent engine simulator.
- Command acknowledgement and observed-state columns, Status button, readonly
  playback/live Type dropdown and pending-command timeout tracking.
- 79 tests: 77 passed, two optional real-engine skips; GUI tests enabled. Packaged
  startup and silent simulator UDP checks passed in the preceding feedback build.
- Download and extract the complete portable ZIP, including runtime-licenses.
  Nothing listens or plays until explicitly requested. Source is in `Apps/OSCController`.

## Interoperability contracts

- Standard ports: spatial input 8100, playback input 9000, monitor 9001, ADM 50018,
  Custom 9002; configurable for concurrent engines.
- OSC v1 command acknowledgements: `<base>/ack ,ssis command result playing detail`,
  with received/ignored/rejected/failed outcomes and observed playing state.
- `<base>/status` snapshots and observed lifecycle `/state` reporting. Feedback goes
  to the configured monitor destination, not necessarily the command sender port.
- Seven exact acknowledgement packets and 34 command decisions join existing ADM,
  Custom and scene-query fixtures. Scene schema remains version 1.
- Seven Python fixture checks passed; engine repositories vendor byte-verified copies.

Engine runtimes are released from their respective engine repositories. GitHub's
automatic source archive includes the controller source but not its Windows executable. UDP remains best effort
and unauthenticated; a received command is not proof of physical sound. See `Specs/`,
`Schemas/`, `Fixtures/`, the MIT license and accompanying SHA-256 checksums.
