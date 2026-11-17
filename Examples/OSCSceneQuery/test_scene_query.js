/* Run: node --test Examples/OSCSceneQuery/test_scene_query.js */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const dgram = require('node:dgram');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { queryScene, encodeQuery, PageAssembler, decodeResponse, SCHEMA } = require('./scene_query');
function string(value) { const data = Buffer.from(value + '\0'); const out = Buffer.alloc(Math.ceil(data.length / 4) * 4); data.copy(out); return out; }
function int(value) { const out = Buffer.alloc(4); out.writeInt32BE(value); return out; }
function response(id, index, total, chunk) { return Buffer.concat([string('/spatialfabric/scene/response'), string(',siis'), string(id), int(index), int(total), string(chunk)]); }
function page(offset, key, next) { return { schema: SCHEMA, schema_version: 1, world: '/Game/Test', offset, limit: 16, returned: 1, total: 2, next_offset: next, playback_network: {}, objects: [{ kind: 'stage', key, name: '日本語 '.repeat(100) }] }; }
test('request bytes/types and bounds', () => {
    const packet = encodeQuery('max_001', 9011, 0, 16);
    assert.equal(packet.subarray(0, 28).toString().replace(/\0/g, ''), '/spatialfabric/scene/query');
    assert.ok(packet.includes(Buffer.from(',siii\0')));
    assert.throws(() => encodeQuery('bad id', 9011));
    assert.throws(() => encodeQuery('ok', 80));
});
test('bounded reassembly, duplicates and UTF8', () => {
    const data = JSON.stringify(page(0, '/stage', -1));
    const chunks = [data.slice(0, 100), data.slice(100, 200), data.slice(200)];
    // Use ASCII JSON for this direct constructor test, preserving bounded native UTF8 chunks.
    const text = JSON.stringify({ schema: SCHEMA, schema_version: 1, objects: [], label: '日本語' });
    const a = new PageAssembler('r');
    assert.equal(a.accept({ id: 'r', index: 1, total: 2, chunk: text.slice(10) }), null);
    assert.equal(a.accept({ id: 'r', index: 1, total: 2, chunk: text.slice(10) }), null);
    assert.equal(a.accept({ id: 'r', index: 0, total: 2, chunk: text.slice(0, 10) }).label, '日本語');
    assert.throws(() => new PageAssembler('r').accept({ id: 'r', index: 0, total: 129, chunk: '' }));
    assert.throws(() => decodeResponse(Buffer.from('bad')));
});
test('actual UDP paginated client, out-of-order chunks', async () => {
    const server = dgram.createSocket('udp4');
    await new Promise(resolve => server.bind(0, '127.0.0.1', resolve));
    server.on('message', (packet, peer) => {
        let pos = 0;
        function readString() { const end = packet.indexOf(0, pos), value = packet.subarray(pos, end).toString(); pos = Math.ceil((end + 1) / 4) * 4; return value; }
        assert.equal(readString(), '/spatialfabric/scene/query'); assert.equal(readString(), ',siii');
        const id = readString(), replyPort = packet.readInt32BE(pos), offset = packet.readInt32BE(pos + 4);
        const data = JSON.stringify(page(offset, offset ? '/second' : '/first', offset ? -1 : 1));
        const chunks = []; let chunk = '';
        for (const ch of data) { if (Buffer.byteLength(chunk + ch) > 900) { chunks.push(chunk); chunk = ''; } chunk += ch; }
        if (chunk) chunks.push(chunk);
        for (let i = chunks.length - 1; i >= 0; --i) server.send(response(id, i, chunks.length, chunks[i]), replyPort, peer.address);
    });
    try { const result = await queryScene('127.0.0.1', server.address().port, 16); assert.equal(result.objects.length, 2); assert.equal(result.paginated, true); }
    finally { server.close(); }
});
test('Node-RED function and Max wrapper pass full JSON objects (host APIs stubbed)', async () => {
    const result = { schema: SCHEMA, schema_version: 1, objects: [], total: 0 };
    const sent = [], handlers = {};
    const fakeApi = { queryScene: async () => result };
    const code = fs.readFileSync(path.join(__dirname, 'node_red_function.js'), 'utf8');
    const fn = vm.runInNewContext('(function(msg,node,global){' + code + '\n})');
    fn({ payload: {} }, { send: message => sent.push(message), done: () => {}, error: message => { throw Error(message); } }, { get: () => fakeApi });
    const maxApi = { addHandler: (name, handler) => { handlers[name] = handler; }, outlet: data => sent.push(data), post: () => {} };
    vm.runInNewContext(fs.readFileSync(path.join(__dirname, 'max_scene_query.js'), 'utf8'), { require: name => name === 'max-api' ? maxApi : fakeApi });
    await handlers.query('127.0.0.1', 8100); await new Promise(resolve => setImmediate(resolve));
    assert.ok(sent.some(entry => entry.payload === result)); assert.ok(sent.includes(result));
    const flow = JSON.parse(fs.readFileSync(path.join(__dirname, 'node_red_flow.json'), 'utf8'));
    for (const node of flow.filter(n => n.type === 'function')) assert.doesNotThrow(() => new vm.Script('(function(msg,node,global){' + node.func + '\n})'));
});
