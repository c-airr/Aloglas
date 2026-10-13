# Stage 1: Playlist + Transport — Design

Date: 2026-10-13
Status: approved
Project: **Aloglas** (https://github.com/c-airr/Aloglas, MIT License)

## 1. Context

An educational DAW written from scratch to understand how audio software works
internally. Visual direction: Ableton Live (flat, dark, readable). Workflow
direction: trap production. Later stages will add an Ableton-style session view
and piano roll (preferred over FL's equivalents); there will be no FL-style
channel rack. Play/Stop behaves like FL Studio.

The author designs the final look of the UI later. Stage 1 ships a functional,
deliberately plain UI whose colors and font live in a single place so the look
can be replaced without touching widget code.

Success for stage 1: the app opens a window, shows a playlist with empty named
tracks, plays a sample-accurate metronome at the chosen tempo and time
signature, loops a region, and the playhead moves smoothly in sync with what is
heard. All engine timing is covered by tests that run without a sound card.

## 2. Scope

### In scope

- Transport bar: Play/Stop, tempo, time signature, metronome toggle, loop
  toggle, follow-playhead toggle, position display, CPU meter.
- Playlist: ruler with loop strip and bar numbers, adaptive grid, horizontal
  zoom and scroll, vertical scroll, playhead, start marker, loop brace.
- Tracks: empty lanes with names; add, delete, rename, select.
- Status bar: audio device state, sample rate, buffer size.
- Audio output through the default device.

### Out of scope (later stages)

Clips of any kind, audio import, sample playback, project save/load, undo/redo,
MIDI, export to WAV, session view, piano roll, mixer, plugin hosting, audio
device selection, tempo or time-signature changes over time, Linux/macOS builds.

## 3. Stack and build

| Piece | Choice | License |
|---|---|---|
| Language | C++20 | — |
| Compiler | clang from llvm-mingw (`C:\Users\cairr\llvm-mingw`), not MSVC | — |
| Build | CMake + Ninja, dependencies via `FetchContent` pinned to exact tags/commits | — |
| Window, input, GL context | SDL3 | zlib |
| 2D drawing | NanoVG (GL3 backend) + glad for GL 3.3 core loading | zlib / MIT |
| Audio I/O | miniaudio (single header) | MIT-0 / public domain |
| Tests | doctest | MIT |
| PNG writing (screenshots) | `SDL_SavePNG` (built into SDL 3.4) | zlib |
| Font | Inter, embedded (temporary until the author picks one) | OFL |

Target platform for stage 1: Windows x64. Builds run locally, not through
`offload`, because the result is a Windows GUI app that runs on this machine.

Code, comments and commit messages are in English (public repository).

## 4. Architecture

```
src/core/     shared types: musical time math, SPSC queue, small utilities
src/engine/   transport, clock, metronome, render(); no allocations or locks
src/audio/    thin miniaudio wrapper: opens the device, calls engine.render()
src/ui/       generic retained widget toolkit on NanoVG (no DAW knowledge)
src/app/      DAW widgets (TransportBar, Playlist, TrackList, StatusBar),
              Document, Controller, main()
tests/        doctest tests; engine tests render offline, no device
assets/fonts/ Inter
```

Dependency direction: `app → ui, engine, audio, core`; `audio → engine, core`;
`engine → core`; `ui → core`. The engine never depends on the device, so it can
be rendered offline in tests (and later for WAV export).

### Threads

- **UI thread** (main): SDL events, widget layout and drawing. Owns the
  `Document`. Sends commands to the engine.
- **Audio thread** (created by miniaudio): calls `Engine::render(float* out,
  uint32_t frames)` every buffer. Drains pending commands at the start of each
  call, then renders.

### Engine ↔ UI communication

- UI → engine: single-producer single-consumer ring buffer of plain structs,
  fixed capacity 256. Commands: `Play`, `Stop`, `Seek{tick}`,
  `SetTempo{bpm}`, `SetSignature{num, den}`, `SetMetronome{on}`,
  `SetLoop{enabled, start_tick, end_tick}`, `SetStart{tick}`. If the queue is
  full the push fails: debug builds assert, release builds drop the command
  and increment a counter shown in the status bar. In practice the queue is
  drained every few milliseconds and UI sends at most a few commands per frame.
- Engine → UI: atomics: `playing` (bool), `position_ticks` (double),
  `cpu_load` (float, smoothed). The UI reads them once per frame.

## 5. Engine

### 5.1 Time representation

- Musical time unit: **tick**, 960 ticks per quarter note (PPQ).
- Positions stored in the document (start marker, loop bounds) are `int64_t`
  ticks, so they stay on their bar when the tempo changes.
- The running playhead is a `double` in ticks. Each sample advances it by
  `ticks_per_sample = bpm * 960 / (60 * sample_rate)`. Changing the tempo only
  changes this rate, so the playhead immediately moves faster or slower.
- Rule of thumb for the whole project: **time in `double`, audio in `float`.**
  A `float` playhead drifts by tens of milliseconds within 10 bars and stops
  moving entirely after ~272 bars at 140 BPM / 48 kHz; `double` stays within
  0.14 samples after 1000 bars. A test documents this (section 9).

### 5.2 Metronome

- A beat is one unit of the time-signature denominator (quarter note in 4/4,
  eighth note in 6/8). The first beat of each bar is accented.
- Each buffer, the engine finds the beat boundaries the playhead crosses
  inside that buffer and computes the exact sample offset of each one. A click
  can start in the middle of a buffer.
- Click sound is synthesized: sine burst with exponential decay (~30 ms),
  ~1.5 kHz accented / ~1 kHz normal, fixed level around -6 dBFS. No samples.
- A click in progress keeps ringing across buffer boundaries and is cut on Stop.
- Clicks are produced only while the transport is playing and the metronome
  is on.

### 5.3 Loop

- Loop has `start_tick`, `end_tick` (minimum length one beat) and an
  `enabled` flag.
- When enabled and the playhead crosses `end_tick` inside a buffer, the buffer
  is split: render up to the boundary, jump to `start_tick`, render the rest.
  The jump is sample-accurate. A beat on `start_tick` clicks right after the jump.
- Looping only triggers on crossing `end_tick`. A playhead already past the
  end keeps playing forward.

### 5.4 Transport semantics (FL-style)

- **Play** starts from the play origin: the loop start when the loop is
  enabled, otherwise the start marker.
- **Stop** stops and returns the playhead to the play origin, evaluated at the
  moment of stopping (so a start marker moved during playback is honored).
- While stopped, the playhead always sits at the play origin, so moving the
  start marker or the loop while stopped moves the playhead with it.
- Space toggles Play/Stop.
- Clicking the ruler sets the start marker. While playing, it also seeks the
  playhead there.

### 5.5 Real-time rules

`Engine::render()` and everything it calls must not allocate, lock, do I/O,
log, or throw. In debug builds a global `operator new` override checks a
`thread_local` "in audio thread" flag and aborts with a message if memory is
allocated on the audio thread. Engine state is preallocated in
`Engine::prepare(sample_rate, max_frames)`.

### 5.6 Output format

32-bit float, stereo interleaved, device-native sample rate, miniaudio default
period size. The metronome writes the same signal to both channels.

## 6. Audio device (src/audio)

- `AudioDevice` opens the default playback device through miniaudio
  (shared mode), calls `engine.prepare()` with the actual sample rate, then
  starts the stream.
- It reports state to the UI: `Running` / `NoDevice` / `Error(message)`, plus
  sample rate and buffer size.
- **Error handling:**
  - Failure to open: the app keeps running, the status bar shows "No audio",
    and each Play press retries opening the device.
  - Device lost while running (miniaudio stop/reroute notification): the
    transport stops and the same retry-on-Play path applies.
  - If the native sample rate changes on reopen, the engine is re-prepared.
    Tick positions are unaffected because they do not depend on the sample rate.
- CPU meter: time spent in `render()` divided by the buffer duration,
  smoothed, published through an atomic.

## 7. UI toolkit (src/ui) — retained

### 7.1 Widget

`Widget` base class:

- Fields: `Rect rect`, `Widget* parent`, `std::vector<Widget*> children`
  (non-owning), `bool visible`, `bool enabled`.
- Virtuals: `layout(Rect)`, `draw(Canvas&)`, `on_mouse_down/up/move`,
  `on_wheel`, `on_key`, `on_text`, `on_hover_enter/leave`,
  `on_focus_gained/lost`. Event handlers return `bool` (handled).
- **Ownership:** fixed children are member fields registered with `add()`.
  Dynamic children (track headers) are held by the parent in
  `std::vector<std::unique_ptr<Widget>>` and also registered with `add()`. The
  `children` list never owns.

### 7.2 Events

- Mouse events are hit-tested top-down and delivered to the deepest visible,
  enabled widget under the cursor. Unhandled events bubble to the parent.
- Mouse capture: the widget that handled `on_mouse_down` receives all mouse
  moves and the matching `on_mouse_up`, even outside its rect (dragging a
  number box or the loop brace).
- Keyboard and text input go to the focused widget first. If it does not
  handle the event, the window processes global shortcuts. A focused text
  field consumes Space as text.
- Double-click is detected by the toolkit (SDL click count) and delivered as a
  flag on the mouse-down event.

### 7.3 Drawing

- `Canvas` wraps NanoVG with the primitives the app needs: filled/stroked
  rect, line, text (left/center/right, ellipsis), clip rect push/pop.
- Everything is in logical pixels. The window scale comes from
  `SDL_GetWindowDisplayScale` and is applied once to NanoVG.
- `Theme` is one struct with every color, font size and metric used by the
  app. Stage 1 ships a temporary dark gray theme.
- **Redraw policy:** a frame is rendered only when something called
  `invalidate()` (input, document change, window resize) or while the
  transport is playing. When idle, the main loop waits on SDL events and uses
  no CPU.

### 7.4 Stage 1 widgets

`Button` (momentary), `ToggleButton`, `NumberBox` (vertical drag changes the
value, Shift for fine steps, double-click turns it into text entry, Enter
commits, Escape cancels), `TextField` (single line, caret, selection with
mouse and Shift+arrows, clipboard copy/paste through SDL), `Label`,
`ScrollView` helpers for vertical scrolling.

## 8. App (src/app)

### 8.1 Document and Controller

- `Document`: tempo (20.00–999.00 BPM, two decimals, default 140), signature
  (numerator 1–32, denominator 1/2/4/8/16, default 4/4), metronome on/off
  (default on — it is the only sound in stage 1),
  loop (enabled, start, end; default bars 1–5 disabled), start marker
  (default bar 1), follow-playhead flag (default on), tracks (name, id),
  selected track.
- `Controller`: the only code that mutates the `Document`. Each mutation also
  sends the matching engine command and invalidates the window. Widgets read
  the document while drawing and never cache its values.
- The track list rebuilds its header widgets when the track count changes.
- Default document: four tracks named "1 Track" … "4 Track".

### 8.2 Window layout

```
+------------------------------------------------------------------+
| TransportBar                                                     |
+--------------+---------------------------------------------------+
|              | loop strip  [====loop brace====]                  |
|              | bar numbers  1    2    3    4    5    6    7      |
| TrackList    +---------------------------------------------------+
| (headers)    | track lanes with grid, playhead, start marker     |
|              |                                                   |
+--------------+---------------------------------------------------+
| StatusBar                                                        |
+------------------------------------------------------------------+
```

Track headers and lanes scroll vertically together.

### 8.3 TransportBar

Left to right: Play/Stop toggle, tempo `NumberBox`, signature numerator and
denominator `NumberBox`es, metronome toggle, loop toggle, follow toggle,
position display `bar.beat.sixteenth` (1-based, e.g. `1.1.1`) and time
`mm:ss.mmm`, CPU meter.

### 8.4 Playlist

- **Ruler**, two strips:
  - **Loop strip** (top): drag on an empty area creates a loop region and
    enables the loop; drag the brace body to move it; drag its edges to
    resize; double-click toggles loop on/off. The brace is always drawn:
    accent-colored when enabled, gray when disabled.
  - **Bar numbers** (bottom): click sets the start marker (seek while
    playing).
- **Snap:** loop edges and the start marker snap to the current grid
  division; holding Alt disables snapping.
- **Grid:** adaptive density. The finest division among 1/32, 1/16, 1/8,
  beat, bar whose on-screen spacing is at least 12 px is drawn; bar lines are
  stronger, and when bars themselves get closer than 12 px only every 2nd,
  4th, 8th… bar is drawn and numbered.
- **Zoom:** Ctrl+wheel around the mouse position, from 2 to 2000 px per beat.
- **Scroll:** wheel scrolls vertically, Shift+wheel horizontally. The
  timeline extends to 1000 bars.
- **Playhead:** vertical line at `position_ticks`. With follow on, when the
  playhead leaves the visible range (forward, or backward after a loop jump)
  the view jumps so the playhead sits at the left edge (Ableton-style page
  jump, not continuous scroll).
- **Start marker:** small triangle on the ruler plus a faint line.

### 8.5 Tracks

- Click a header to select it. Ctrl+T adds a track after the selected one (or
  at the end). Delete removes the selected track. Double-click a name to
  rename it inline with `TextField` (max 64 characters, empty name reverts).
- Zero tracks is allowed; the lane area then shows only the grid.

### 8.6 StatusBar

Device state ("Audio: 48000 Hz, 480 samples" / "No audio — press Play to
retry" / error text) and the dropped-command counter when non-zero.

### 8.7 Shortcuts (stage 1)

| Key | Action |
|---|---|
| Space | Play/Stop |
| Ctrl+T | Add track |
| Delete | Delete selected track |
| Ctrl+L | Toggle loop |
| Ctrl+wheel | Zoom |
| Shift+wheel | Horizontal scroll |

### 8.8 Command-line flags

- `--screenshot <file.png>`: render one frame of the default document to a
  PNG and exit (used to verify drawing without a human looking at the screen).
- `--no-audio`: start without opening an audio device.

## 9. Testing

doctest, run through CTest. Engine tests render offline into buffers with no
audio device.

Engine:

1. Clicks at 120 BPM / 48 kHz / 4/4 start exactly at samples 0, 24000,
   48000, … (±1 sample).
2. At 133 BPM (non-integer samples per beat) the 1000th click is within ±1
   sample of its ideal position: no drift.
3. Rendering the same span with buffer sizes 64, 480 and 1024 gives identical
   click positions (block size independence).
4. Tempo change mid-play: clicks after the change follow the new spacing.
5. Loop: with loop bars 2–3, the playhead jumps from the loop end to the
   loop start on the exact sample, and the beat on the loop start clicks.
6. Play with loop enabled starts at the loop start; without it, at the start
   marker. Stop returns to that origin.
7. Signatures 4/4, 6/8, 7/4: beat spacing and accent placement are correct.
8. **Float lesson:** the same accumulation run with `float` and `double` at
   140 BPM / 48 kHz; asserts that `double` stays within 1 sample after 1000
   bars and that `float` stalls (adding the per-sample increment no longer
   changes the value) before bar 300. The test prints the bar where `float`
   breaks down.
9. Real-time guard: in test builds the guard counts allocations made on the
   audio thread instead of aborting (doctest has no death tests). The test
   renders with every command type in the queue and checks the counter stays
   zero, and separately checks that a deliberate allocation inside a guarded
   scope is counted.

Core / UI math:

10. Tick ↔ `bar.beat.sixteenth` and ↔ `mm:ss.mmm` conversions for 4/4, 6/8, 7/4.
11. Pixel ↔ tick mapping at several zoom levels and scroll offsets.
12. Grid density selection at the zoom extremes.
13. Snap to grid, and no snap with Alt.
14. SPSC queue: push until full fails cleanly; FIFO order preserved.

Manual verification (by Claude before handing over, then by the author):
`--screenshot` output looks right; metronome audibly in time with the
playhead; loop jumps are click-free and on time; idle CPU usage is ~0%.

## 10. Open items for the author

- Visual design (theme, font) — planned separately, replaces `Theme` only.

The project is MIT-licensed; all dependencies are compatible (zlib, MIT,
MIT-0, OFL for the font, which must ship with its license file).
