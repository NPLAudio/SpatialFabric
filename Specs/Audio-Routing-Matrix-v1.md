# Audio Routing Matrix v1

## Scope

A common editor authoring experience for Unity, Unreal and Godot, complementing the [Scene Object Manager](Scene-Object-Manager-v1.md). This is direct playback route configuration, not a Dante network subscription editor, per-source DSP mixer, or multi-send engine.

## Presentation

- Present a true rectangular crosspoint matrix by default: playback cue/component rows, grouped interface headers and aligned output-channel columns. Do not substitute independent strips of channel buttons.
- Check the configured intersection. A green check means **authored configuration**, not successful playback, a live subscription or verified hardware connectivity. Each cue currently owns one endpoint/channel route; clicking its assigned cell is a no-op, not a disconnect.
- Support two-axis scrolling and keep row/column identities readable. Bound rendering to the visible viewport rather than constructing every cue × channel widget. Clip both input and painted cells to ancestor viewports. Leave wheel input for an enclosing scroll view when internal scrolling cannot move.
- Provide **Audio interface** and **Output channel** dropdowns for the selected/individual cue, and an explicitly labeled **Refresh Interfaces** action. Disambiguate duplicate interface names with identity information. Standard Inspector/Details properties remain available.
- Enumerate playback functionality in the editor scene scope without requiring a runtime manager or implicitly attaching components. Ordinary unconfigured objects remain available through Scene Objects' explicit add actions.

## Route semantics

- Store the actual existing component/node endpoint ID and channel, using engine-native Undo and scene/prefab persistence. Do not maintain a separate matrix-only route copy.
- Match endpoint IDs exactly, never by friendly name. An empty ID remains **dynamic OS default**, resolved only when playback starts; do not silently replace it with today's fixed default endpoint.
- Channel **Auto / 0** preserves corresponding source-to-output channels. Positive numbers are **1-based destinations for the first source channel**, not arbitrary source-channel subscriptions.
- Known cached endpoints expose their recorded physical output channels even offline. Unknown IDs and dynamic default expose Auto and explicitly stored positive channels, not invented capacity. Preserve out-of-range stored routes and identify them as unverified; never clamp or erase them during inventory or browsing.
- An interface change preserves the authored channel until the user explicitly chooses another. This may reveal a stored out-of-range value; it must not silently retarget it.

## Inventory and cache

- Opening, drawing or selecting must not enumerate hardware, prepare PCM, start/stop playback, capture audio or start networking.
- Only explicit Refresh may probe/enumerate interfaces. Enumeration must not open audio streams.
- Persist a bounded, versioned, project-local editor-user cache of exact IDs, friendly names, channel counts, sample rates and last-seen timestamps. Do not store cue references or route assignments in it.
- Load entries as offline/unverified. Session online means present at the last successful explicit enumeration, not continuous monitoring.
- Validate complete inventories before merging/saving. A native enumeration error, stale result, malformed inventory or capacity overflow must preserve prior history and all cue routes, not pretend refresh succeeded.
- Preserve unplugged endpoints and their layout. Never evict, clear or rewrite routes merely because an endpoint disappeared. Reconnection matches its exact ID. No automatic fallback to another fixed endpoint is implied.

## Mutation safety

- Require stopped editor authoring mode, fresh valid scene targets, no active worker/playback/pending replay, and editable unlocked scene/prefab ownership. Reject unsafe/protected targets rather than stopping them to permit an edit.
- Capture stable presentation data for the UI event/layout cycle, but freshly validate the target at the mutation boundary. Deleted/replaced objects and changed worker state must not leave stale actionable cells.
- One route action changes only that cue's endpoint/channel, records one native Undo action, preserves unrelated fields and marks the correct scene/prefab override dirty. Undo/Redo must not start audio or networking.

## Verification

Use disposable editor projects and fake/synthetic inventories for cache, unplug/reconnect, unknown/default/channel mapping, geometry/virtualization, protected/active targets, native Undo and real UI interactions. Confirm actual engine compilation/editor execution separately from portable unit checks. Physical hotplug, audible output, acoustic latency and platform acceptance remain explicit external gates; a checked cell or a silent test is not that certification.
