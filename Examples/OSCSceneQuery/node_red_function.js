// Paste into a Node-RED Function node, or import node_red_flow.json.
// settings.js: functionGlobalContext: { sfScene: require('/absolute/path/scene_query.js') }
const api = global.get('sfScene');
if (!api) { node.error('Configure functionGlobalContext.sfScene and restart Node-RED', msg); return null; }
const options = (msg.payload && typeof msg.payload === 'object') ? msg.payload : {};
api.queryScene(options.host || '127.0.0.1', Number(options.port || 8100), 16)
    .then(scene => { node.send({ ...msg, topic: 'spatialfabric/scene', payload: scene }); node.done(); })
    .catch(error => { node.error('Scene query: ' + error.message, msg); node.done(); });
return null; // Async: the entire completed JSON inventory is sent by node.send above.
