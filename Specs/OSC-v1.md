# OSC interoperability v1

IPv4 unicast, OSC padded strings and network-byte-order int32/float32. Reject malformed
padding/lengths, non-finite values and unsupported tags atomically. Bundle timetags
are immediate dispatch, not scheduled/sample-accurate transport.

## Spatial output

`/adm/obj/{positive-id}/xyz` float right/front/up; `/aed` float azimuth degrees
(positive left), elevation degrees, radius. Both describe one sphere-limited point.
Optional `/gain` float linear, `/w` float 0..1, `/mute` int32 0/1, `/name` string.

## Custom adapter

Canonical XYZ -1..1 map into a configurable output position range. Packed sends all
float arguments on one address; Discrete sends one per token with `{axis}`. Argument
tokens: x/y/z/gain/width/id and azimuth/a/elevation/e/distance/d (case-insensitive).
AED mode maps azimuth -180..180, elevation -90..90, radius clamped 0..1 into the position
range. XYZ mode leaves polar token values zero. Gain/width inputs clamp 0..1 before
independent remapping. Reverse output ranges are allowed; id remains float32 unremapped.

Address built-ins: `{id}`, mapped `{x}/{y}/{z}/{gain}/{width}`, `{label}`; per-object fields
add keys. Discrete aliases resolve `{axis}` to azimuth/elevation/distance. Numeric address
formatting uses invariant decimals, at least `.0`, up to six fractional digits. Unknown
arguments/unresolved/invalid addresses must never become partial valid-looking output.
See `Fixtures/custom-v1.json` for exact expected packets. Unity additionally rejects
reserved/duplicate field keys and nested-template values rather than ambiguous injection.

## Playback

Explicit `/base/play`: no args, float >=0.5 or int32 >=1. `/stop`: address-only.
`/loop`: numeric 0/1 (float strictly >0.5 enables; address-only disables). If multiple
argument types are present, first float takes precedence over first int32 anywhere in
the argument list, matching AudioLoom. `/play` uses >=0.5 for float and >=1 for int32;
unknown nonnumeric payloads do not trigger. `/state` float 1/0 is observed worker lifecycle, not request ack.
Never emit Playing=1 for an unimplemented/refused backend. Channel 0 maps corresponding
source/device channels; 1..N selects the first source channel to one device output.

## Read-only scene discovery

`/spatialfabric/scene/query ,siii request_id reply_port offset limit` (short `,si` form:
offset 0, limit 64). ID: 1..64 ASCII alphanumeric/_/-. Reply port 1024..65535; offset
0..16384; limit 1..64. Only reply to sender IP; remote discovery disabled by default.

`/spatialfabric/scene/response ,siis request_id index count json_chunk`.
Zero-based index; <=128 chunks, <=900 UTF-8 bytes/string, <=96 KiB/page. Reassemble by
index before parsing. JSON schema `org.nplaudio.spatialfabric.scene`, version 1.
Follow next_offset until -1; >=300 ms between pages, fresh IDs/retries. Replies have
an ephemeral source port. Pages are not a globally frozen multi-page snapshot.

Read-only means no starting audio/receivers, loading assets, changing routes or
synthesizing persistent identities. Bound/requested settings and last observed state
must remain distinct. World scope is the queried listener's loaded scene/world.

Default Unreal inputs 8100/9000; Unity may use different configured ports to run side
by side. Addresses, types, units and response schema remain the same. OSC is not
an authenticated or Internet-safe service.
