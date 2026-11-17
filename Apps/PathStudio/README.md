# Path Studio

Desktop editor for authoring object, listener and camera paths for Unreal, Unity and Godot.
Design: [ADR 0001](../../Docs/PathStudio-ADR-0001.md). Contract: [Path v1](../../Specs/Path-v1.md).

## Status: Phase 3 (3D editing, timeline, shapes, undo/redo)

- Open, save (atomic, UTF-8, LF), new documents; 16 MiB size cap.
- **Four viewports** (software drawing, no GPU/OpenGL needed): Perspective plus orthographic **Top**,
  **Front** (from behind) and **Side**. Layout selector shows all four or one. In Perspective, orbit with
  right-drag or by dragging empty space; orthographic views pan instead; the wheel zooms; middle-drag pans.
  Drag a keyframe in any view: Perspective moves it on the horizontal plane at its height (**Shift-drag**
  for up/down); orthographic views move it in their plane and keep the third coordinate.
  Stage cube, grid, drop lines to the floor, waypoints and the playhead are drawn in each.
- **Context** (read-only reference, never saved into the path): *Import context file* draws boxes, spheres,
  points and a listener from a `.sfctx.json` ([Context v1](../../Specs/Context-v1.md); try
  `examples/room.sfctx.json`). *Query scene* also marks the engine's spatial bindings at their exact stage
  positions. Engine-world-only records (listener, stage, cue actors) are counted but not placed, because
  their coordinates are not stage-normalized. *Focus selected* centres the views; *Snap selected key to it*
  moves the selected keyframe onto a context object (undoable).
- Track list, live **validation panel** (exact codes and JSON paths) and a **2D plan view**
  (horizontal = right, vertical = front, stage = blue square) with the sampled curve, waypoints
  and a time scrubber that uses the reference sampler.
- Drag explicit keyframes in the plan view, or retime them in the **timeline** (lane per track, scrubbable
  playhead, keys clamped between neighbors). A whole drag is one undo step.
- **Keyframe inspector**: exact time, front/right/up, interpolation and ease; add a keyframe at the scrub
  time (it starts on the current path) or delete one. Editing a waypoint-linked key detaches it.
- **Shape generator**: line, circle, spiral, figure-eight and seeded random walk replace the selected
  track's keyframes (deterministic, bounded to the contract range).
- **Undo/redo** (Ctrl+Z / Ctrl+Y) for every edit; the title's `*` clears when you return to the saved state.
- **Scene discovery** (explicit *Query scene* button, background thread): sends only the read-only
  `/spatialfabric/scene/query` to the host/port you choose and lists bindable objects and listeners.
  *Bind to selected track* / *New track for target* store the key, label and `object_id`;
  new tracks start at the object's reported position. Remove track is also available.
- No playback commands, no audio, no device access. Without a query the editor opens no sockets.

**Baked playback (Unity):** save the path in Path Studio, then in Unity use
`Tools > SpatialFabric > Import Path Studio Path...` and add a `SpatialPathPlayer` to the object (assign the asset and
stage, call `Play()`). Playback is deterministic and matches the golden fixtures; it moves position only and needs no
network. Godot and Unreal exporters are not implemented yet.

Context exporters: Unity has one (`Tools > SpatialFabric > Export Path Studio Context...`, on Unity main, unreleased); Godot and Unreal do not yet.

Not yet: waypoint editing, bezier tangent handles, exporters, live driving. Engines currently have no inbound
verb that moves an object, so live driving needs a new contract version (see the ADR).

## Run

```sh
python -m venv .venv && .venv/Scripts/python -m pip install -r requirements.txt
.venv/Scripts/python -m pathstudio [file.sfpath.json]
```

## Test

```sh
python -m unittest discover -s tests -v          # contract + store; GUI tests skip without PySide6
.venv/Scripts/python -m unittest discover -s tests -v   # includes offscreen GUI tests
```

[`pathstudio/model.py`](pathstudio/model.py) is standard-library only and shared by exporters
and tests. Golden vectors: [`Fixtures/path-v1.json`](../../Fixtures/path-v1.json).
