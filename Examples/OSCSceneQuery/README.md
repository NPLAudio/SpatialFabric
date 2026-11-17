# Read-only scene query examples

[Complete command, schema and safety contract](../../Docs/SceneQuery.md)

These examples query the existing Unreal OSC input and reconstruct **complete JSON**
from correlated, bounded OSC string chunks. They do not send Play/Stop/Loop, change
Unreal routing, or require MCP/HTTP. Start the SpatialFabric spatial receiver (8100)
or Playback OSC (9000) first. Remote hosts require explicit scene-query opt-in;
loopback is the default. Query permissions do not start listeners automatically.

## Python — no pip packages

From the **complete repository** root:

```sh
python Examples/OSCSceneQuery/query_scene.py --port 8100 > scene.json
python Examples/OSCSceneQuery/query_scene.py --port 9000 --page-size 16
```

The wrapper reuses `Apps/OSCController/osc_controller/{scene_query,protocol,model}.py`;
keep that package when copying this example. It opens an ephemeral reply socket,
retries bounded timeouts, handles Unicode/out-of-order chunks and follows pages.
Access `scene['objects']`, then filter `kind == 'playback'` or `spatial_binding`.

## Node.js — no npm packages

Use Node 18+ (locally exercised with Node 22):

```sh
node Examples/OSCSceneQuery/scene_query.js 127.0.0.1 8100 16 > scene.json
```

Or reuse the CommonJS module:

```js
const { queryScene } = require('./scene_query');
const scene = await queryScene('127.0.0.1', 8100, 16);
const cues = scene.objects.filter(o => o.kind === 'playback');
for (const cue of cues) {
  console.log(cue.actor_label, cue.osc_base, cue.configured_device_id,
              cue.output_channel, cue.channel_policy, cue.playing);
}
```

Wrap `await` in an async function in a CommonJS script. No third-party OSC library
is needed; `scene_query.js` contains the wire encoder, bounded decoder/reassembler,
reply-source/request-ID validation, timeout/retry and pagination handling. It uses
an OS-assigned receive port, not shared show-control monitoring ports.

## Max/MSP — built-in Node for Max

Keep `max_scene_query.js` next to `scene_query.js`.

1. Create `[node.script max_scene_query.js @autostart 1]` in Max 8+ with Node for Max.
   Use a full script path or put this folder on Max's search path.
2. Connect a message box `query 127.0.0.1 8100` to its inlet. Use 9000 for Playback.
3. Connect its outlet to `[dict.view]`. A successful response is a **Max dictionary**,
   not a raw fragment or a JSON string requiring manual concatenation.
4. For structured patching, also connect to
   `[dict.unpack schema_version: world: total: objects:]`; inspect/iterate the
   `objects` array's dictionaries and branch on each record's `kind`.
5. For playback records, use `osc_base`/`controls` and output routing fields to
   configure your own patch. No transport commands are emitted by this wrapper.

Use **Node for Max's `[node.script]`**, not Max's legacy `[js]`. `max-api` comes from
Max; do not npm-install another OSC dependency. Query status/errors are posted to
the Max console. Concurrent clicks are refused while one query is in progress.
The Node wire client is tested; the actual Max patch/runtime requires local testing
with your installed Max/Node version. The wrapper avoids assuming an OSC external,
OSC argument auto-typing, or a particular Max UDP object's binary mode.

## Node-RED — importable flow

1. Copy `scene_query.js` somewhere readable by Node-RED.
2. In Node-RED's **settings.js**, add it to the existing `functionGlobalContext`
   object (do not duplicate that top-level setting):

   ```js
   functionGlobalContext: {
       sfScene: require('C:/tools/SpatialFabric/Examples/OSCSceneQuery/scene_query.js')
   },
   ```

   Use your own absolute path, or a Unix path on Linux. Restart Node-RED.
3. Import **`node_red_flow.json`** and Deploy. It uses only standard Inject,
   Function, Debug and Comment nodes; no contributed OSC nodes are required.
4. Click **Query scene (read only)**. The complete inventory appears as a JS object
   in Debug. The second branch extracts readable playback routing summaries.
5. Edit the Inject payload's `host`/`port` for your receiver. Network queries outside
   loopback need Unreal remote discovery enabled and an appropriate reply route.

`node_red_function.js` is the same query Function source in a copyable file.
`node.send()` runs only after complete reassembly/pagination; errors use `node.error()`.
The reusable module owns its ephemeral UDP reply socket and closes it on completion
or failure. There are no permanently bound UDP nodes to collide with the tracker.
An actual Node-RED deployment/UI is not certified by command-line Node tests.

## Other OSC applications / hand-built patches

Send `/spatialfabric/scene/query` with types **`,siii`**:
`"your_request_001", reply_port, 0, 16`.

Route incoming `/spatialfabric/scene/response`, types **`,siis`**. Validate the ID,
store chunks by the zero-based index, concatenate only when `chunk_count` distinct
chunks have arrived, and JSON-parse. Follow `next_offset` with a new request ID,
waiting at least 300 ms between requests. Do not interpret `chunk_count` as object
count or parse individual chunks as JSON. Do not insert delimiters between chunks.
See the guide for bounds, timeouts, rate limits, world scope and security.

`sample_scene.json` is **illustrative**, not a dump of this workstation's devices.
`scene.schema.json` describes an individual native response page. A merged inventory
can contain more than 64 records and therefore is not itself a single wire page.
Keep `schema_version` checks; ignore unknown fields/record kinds for forward
compatibility. The data describes configuration and software state, not certified
physical audio, hardware presence or latency.
