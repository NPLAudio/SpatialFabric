/* MIT. Dependency-free OSC/JSON scene client for Node.js, Node-RED and Node for Max. */
'use strict';
const dgram = require('node:dgram');
const net = require('node:net');
const crypto = require('node:crypto');
const SCHEMA = 'org.nplaudio.spatialfabric.scene';
const QUERY = '/spatialfabric/scene/query';
const RESPONSE = '/spatialfabric/scene/response';
function oscString(text) {
    const raw = Buffer.from(text + '\0', 'utf8');
    const result = Buffer.alloc(Math.ceil(raw.length / 4) * 4); raw.copy(result); return result;
}
function integer(value) { const result = Buffer.alloc(4); result.writeInt32BE(value); return result; }
function encodeQuery(id, replyPort, offset = 0, limit = 16) {
    if (!/^[A-Za-z0-9_-]{1,64}$/.test(id) || !Number.isInteger(replyPort) || replyPort < 1024 || replyPort > 65535 || !Number.isInteger(offset) || offset < 0 || offset > 16384 || !Number.isInteger(limit) || limit < 1 || limit > 64) throw Error('Invalid request');
    return Buffer.concat([oscString(QUERY), oscString(',siii'), oscString(id), integer(replyPort), integer(offset), integer(limit)]);
}
function decodeResponse(packet) {
    if (!Buffer.isBuffer(packet) || packet.length < 8 || packet.length > 1200 || packet.length % 4) throw Error('Invalid datagram size');
    let pos = 0;
    function string() {
        const end = packet.indexOf(0, pos); if (end < 0) throw Error('Unterminated string');
        const next = Math.ceil((end + 1) / 4) * 4;
        if (next > packet.length || packet.subarray(end, next).some(x => x !== 0)) throw Error('Bad padding');
        const raw = packet.subarray(pos, end), value = raw.toString('utf8');
        if (!Buffer.from(value, 'utf8').equals(raw)) throw Error('Invalid UTF8');
        pos = next; return value;
    }
    function int() { if (pos + 4 > packet.length) throw Error('Truncated integer'); const result = packet.readInt32BE(pos); pos += 4; return result; }
    if (string() !== RESPONSE || string() !== ',siis') throw Error('Not a scene response');
    const id = string(), index = int(), total = int(), chunk = string();
    if (pos !== packet.length) throw Error('Unexpected trailing bytes');
    return { id, index, total, chunk };
}
class PageAssembler {
    constructor(id) { this.id = id; this.count = null; this.chunks = new Map(); this.bytes = 0; }
    accept(response) {
        const { id, index, total, chunk } = response;
        if (id !== this.id) return null;
        if (!Number.isInteger(index) || !Number.isInteger(total) || total < 1 || total > 128 || index < 0 || index >= total || typeof chunk !== 'string' || Buffer.byteLength(chunk, 'utf8') > 900) throw Error('Invalid chunk');
        if (this.count !== null && this.count !== total) throw Error('Chunk count changed');
        this.count = total;
        if (this.chunks.has(index)) { if (this.chunks.get(index) !== chunk) throw Error('Conflicting duplicate'); return null; }
        this.bytes += Buffer.byteLength(chunk, 'utf8'); if (this.bytes > 96 * 1024) throw Error('Page too large');
        this.chunks.set(index, chunk); if (this.chunks.size !== total) return null;
        const page = JSON.parse(Array.from({ length: total }, (_, i) => this.chunks.get(i)).join(''));
        if (!page || page.schema !== SCHEMA || page.schema_version !== 1) throw Error('Unsupported schema');
        if (page.error) throw Error('Unreal: ' + page.error);
        return page;
    }
}
function queryScene(host = '127.0.0.1', port = 8100, pageSize = 16) {
    return new Promise((resolve, reject) => {
        if (net.isIP(host) !== 4 || host === '0.0.0.0' || !Number.isInteger(port) || port < 1 || port > 65535 || !Number.isInteger(pageSize) || pageSize < 1 || pageSize > 64) { reject(Error('Literal IPv4, valid port and page size required')); return; }
        const sock = dgram.createSocket('udp4');
        let timer = null, closed = false, assembler, offset = 0, total = null, world = null, attempts = 0, bytes = 0;
        const objects = [], keys = new Set(), times = [];
        function finish(error, result) {
            if (closed) return; closed = true; clearTimeout(timer); try { sock.close(); } catch (_) { /* Bind may have failed. */ }
            if (error) reject(error); else resolve(result);
        }
        function request() {
            if (closed) return;
            if (++attempts > 3) { finish(Error('Scene query timed out: start Unreal OSC and check query permissions / reply path')); return; }
            const id = crypto.randomBytes(16).toString('hex'); assembler = new PageAssembler(id);
            sock.send(encodeQuery(id, sock.address().port, offset, pageSize), port, host, error => { if (error) finish(error); });
            timer = setTimeout(request, 1500);
        }
        sock.on('error', error => finish(error));
        sock.on('message', (packet, peer) => {
            if (closed || peer.address !== host || !assembler) return;
            let response; try { response = decodeResponse(packet); } catch (_) { return; }
            try {
                const page = assembler.accept(response); if (!page) return;
                for (const key of ['offset', 'limit', 'returned', 'total', 'next_offset']) if (!Number.isInteger(page[key])) throw Error('Invalid page counters');
                if (page.offset !== offset || typeof page.world !== 'string' || !Array.isArray(page.objects) || page.returned !== page.objects.length || page.limit < 1 || page.limit > 64 || page.objects.length > page.limit || page.total < 0 || page.total > 16384) throw Error('Invalid page shape');
                if (total !== null && (total !== page.total || world !== page.world)) throw Error('Scene changed while paging; query again');
                total = page.total; world = page.world; bytes += assembler.bytes;
                if (bytes > 8 * 1024 * 1024) throw Error('Inventory exceeds 8 MiB');
                for (const row of page.objects) {
                    if (!row || typeof row.key !== 'string' || !row.key || typeof row.kind !== 'string' || keys.has(row.key)) throw Error('Duplicate/invalid record; scene may have changed');
                    keys.add(row.key); objects.push(row);
                }
                times.push(page.snapshot_utc); clearTimeout(timer);
                if (page.next_offset === -1) {
                    if (objects.length !== total) throw Error('Incomplete inventory');
                    finish(null, { ...page, offset: 0, returned: objects.length, objects, page_snapshot_times: times, paginated: times.length > 1 });
                } else {
                    if (page.next_offset !== offset + page.objects.length || page.next_offset <= offset || page.next_offset >= total) throw Error('Invalid continuation');
                    offset = page.next_offset; attempts = 0; assembler = null; timer = setTimeout(request, 350);
                }
            } catch (error) { finish(error); }
        });
        sock.bind(0, host.startsWith('127.') ? '127.0.0.1' : '0.0.0.0', () => {
            try { sock.setRecvBufferSize(1024 * 1024); } catch (_) { /* Smaller pageSize is useful on restricted hosts. */ }
            request();
        });
    });
}
module.exports = { queryScene, encodeQuery, decodeResponse, PageAssembler, SCHEMA };
if (require.main === module) {
    queryScene(process.argv[2] || '127.0.0.1', Number(process.argv[3] || 8100), Number(process.argv[4] || 16))
        .then(scene => process.stdout.write(JSON.stringify(scene, null, 2) + '\n'))
        .catch(error => { console.error(error.message); process.exitCode = 1; });
}
