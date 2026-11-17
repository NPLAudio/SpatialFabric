/* MIT. Run with Max's built-in [node.script], not the legacy [js] object. */
'use strict';
const maxApi = require('max-api'); // Supplied by Node for Max; no npm install.
const { queryScene } = require('./scene_query');
let busy = false;
maxApi.addHandler('query', async (host = '127.0.0.1', port = 8100) => {
    if (busy) { maxApi.post('Scene query already in progress'); return; }
    busy = true;
    try {
        const scene = await queryScene(String(host), Number(port), 16);
        // A JS object is emitted as a Max dictionary, suitable for [dict.view]/[dict.unpack].
        maxApi.outlet(scene);
        maxApi.post(`Scene query: ${scene.total} records, ${scene.world}`);
    } catch (error) { maxApi.post('Scene query failed: ' + error.message); }
    finally { busy = false; }
});
maxApi.post('Ready: send "query 127.0.0.1 8100". This never sends Play/Stop/Loop.');
