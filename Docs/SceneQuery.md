# Scene query / configuration discovery — v1

The shared discovery contract is in [OSC v1](../Specs/OSC-v1.md), with the exact JSON
shape in [scene schema v1](../Schemas/scene-v1.schema.json). Use the
[runnable Python, Node, Max and Node-RED clients](../Examples/OSCSceneQuery/README.md)
against any engine integration that implements this contract.

Query a deliberately started engine input (defaults 8100 or 9000), not outgoing
ADM 50018 or monitoring 9001. Bind a UDP reply listener before sending:

```text
/spatialfabric/scene/query ,siii request_id reply_port offset limit
```

Request IDs are 1–64 ASCII letters/digits/underscore/hyphen; reply_port is int32
1024–65535; offset is int32 0–16384; limit is int32 1–64. Numeric strings and floats
are not int32. The short `,si request_id reply_port` uses offset0, limit64.

Replies use `/spatialfabric/scene/response ,siis request_id chunk_index chunk_count
json_fragment`. Reassemble by request ID and chunk order before parsing
JSON; use bounded buffers/timeouts. The reply destination is the request sender's IP
and chosen reply port, not an arbitrary caller-supplied IP. Correlate requests, not
source ports. Treat remote responses as untrusted input. Querying does not play audio.

For engine-specific fields, capabilities, lifecycle and safety limits see:
- [Unreal discovery reference](https://github.com/NPLAudio/SpatialFabric-Unreal/blob/main/Docs/SceneQuery.md)
- [Unity integration](https://github.com/NPLAudio/SpatialFabric-Unity)
- [Godot integration](https://github.com/NPLAudio/SpatialFabric-Godot)

The Unreal repository retains an offline-tested copy of these clients for its
source distributions; this hub is the shared tooling home. Contract/schema version1
is unchanged by the repository migration.
