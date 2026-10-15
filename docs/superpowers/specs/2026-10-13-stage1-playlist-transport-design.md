# Stage 1: Beat-Making Core — Design

Date: 2026-10-13, expanded 2026-10-14
Status: approved by the author 2026-10-15
Project: **Aloglas** (https://github.com/c-airr/Aloglas, MIT License)

## 1. Context

Aloglas is an educational DAW written from scratch to understand how audio
software works internally. Visual direction: Ableton Live (flat, dark,
professional). Workflow direction: trap production, mixing ideas from FL Studio
and Ableton.

Stage 1 delivers a complete small beat-making loop: browse samples, put them
in a channel rack, program them in a piano roll (keys or drum-rack view),
arrange the loop on a playlist, save the project, export a WAV.

Success for stage 1: the author can build a trap beat (drums, hi-hat rolls,
an 808 with glide) entirely in Aloglas, save it, reopen it, and export it, with
sample-accurate timing that is covered by tests running without a sound card.

## 2. Scope

### In scope

- Window layout with resizable panels (§9.1), dark professional theme (§9.2).
- Transport: Play/Stop, tempo, time signature, LOOP/SONG mode, metronome,
  loop brace, follow, position display, CPU meter, master level meter.
- Browser: user-added sample folders as a tree, preview, drag and drop.
- Channel rack: sounds with mute/solo, volume, pan, per-sound sampler settings.
- Sampler instrument: pitch, root key, fine tune, Cut itself, Gate, Glide,
  velocity, 16-voice polyphony per sound.
- One loop (pattern) holding notes of all sounds; piano roll with Keys and
  Drums views, Ableton-style dragging with FL-style clicking, velocity lane,
  hi-hat roll tool, 808 slide notes, adaptive grid.
- Playlist: free tracks, clips that reference the loop, repeat by stretching.
- Project save/open (JSON), undo/redo, copy/paste/duplicate, WAV export.
- Log panel and status bar.

### Out of scope (later)

VST3 hosting (next spec; this design prepares for it through the `Instrument`
interface), multiple loops, audio clips on the playlist, mixer and effects,
browser search, relinking missing samples, recording, MIDI input, automation,
tempo/time-signature changes over time, the bottom-left panel area, Linux/macOS
builds.

## 3. Stack and build

| Piece | Choice | License |
|---|---|---|
| Language | C++20 | — |
| Compiler | clang from llvm-mingw, not MSVC | — |
| Build | CMake ≥ 3.25 + Ninja, deps via `FetchContent` pinned by URL + SHA256 | — |
| Window, input, GL context, file dialogs, message boxes | SDL 3.4.18 | zlib |
| 2D drawing | NanoVG (GL3 backend) + glad2-generated GL 3.3 core loader | zlib / MIT |
| Audio I/O, decoding (WAV/AIFF/FLAC/MP3), WAV encoding | miniaudio 0.11.25 | MIT-0 / public domain |
| JSON (project, settings) | nlohmann/json (pinned release) | MIT |
| Tests | doctest 2.5.3 | MIT |
| PNG screenshots | `SDL_SavePNG` | zlib |
| Font | Inter 4.1, embedded with `#embed` | OFL (license file shipped) |

Target: Windows x64, built locally (not through `offload`). Executables link
`-static` and depend only on system DLLs. Code, comments, commit messages and
UI labels are in English.

## 4. Architecture

```
src/core/     time math, SPSC queue, UTF-8 helpers, RT allocation guard
src/engine/   transport, clock, metronome, snapshots, scheduler,
              Instrument interface, Sampler, mixing; render()
src/audio/    miniaudio device wrapper; WAV export writer
src/samples/  Sample, SampleCache, background loader (decoding)
src/ui/       generic retained widget toolkit on NanoVG
src/app/      Document, Controller, undo, clipboard, project I/O, settings,
              compile (Document → Snapshot), all DAW panels, main()
tests/        doctest; engine/sampler/export tests render offline
```

Dependency direction: `app → ui, engine, audio, samples, core`;
`audio → engine`; `engine → samples, core`; `samples → core`; `ui → core`.

### 4.1 Threads

| Thread | Does | Must not |
|---|---|---|
| UI (main) | events, layout, drawing, owns `Document`, builds snapshots, deletes retired objects | block for long (decoding, export) |
| Audio (miniaudio) | `Engine::render()` | allocate, free, lock, log, throw |
| Sample loader | decodes files into `Sample`s | touch the `Document` |
| Export worker | renders a private `Engine` offline, writes the WAV | touch the live engine |

### 4.2 Communication

- **Commands** (UI → audio): SPSC ring of plain structs, capacity 256 —
  transport commands (`Play`, `Stop`, `Seek`, `SetTempo`, `SetSignature`,
  `SetMetronome`, `SetLoopBrace`, `SetStart`, `SetPlayMode`) and preview
  commands (`PreviewSample`, `PreviewNote`, `StopPreview`).
- **Snapshots** (UI → audio): after every document change the UI compiles an
  immutable `Snapshot` (§5.3) and passes ownership to the engine through a
  single-slot atomic exchange. The engine adopts the newest one at the start
  of the next buffer.
- **Garbage** (audio → UI): SPSC ring through which the engine hands back
  everything it no longer uses — replaced snapshots, finished preview
  payloads. The UI thread deletes them. The audio thread never frees memory.
- **State** (audio → UI): atomics for playing, position, play mode, CPU
  load, master peak (left/right), dropped commands.

**Ownership rule:** anything the engine reads by pointer is an owned heap
object that the UI gave it and that it gives back through the garbage queue.
`Snapshot` and preview payloads hold `std::shared_ptr<const Sample>`, so a
sample stays alive as long as any snapshot or payload that references it,
and is freed on the UI thread when the last one is deleted.

A replaced snapshot is returned only when no voice still plays a sample that
the new snapshot does not contain. Voices of sounds removed by the new
snapshot fade out over 5 ms. The engine keeps up to 4 pending old snapshots
in a fixed array; if it is full, those voices are stopped immediately so the
oldest snapshot can be returned.

## 5. Engine

### 5.1 Time

- Unit: tick, 960 per quarter note. Document positions are `int64_t` ticks.
- The running playhead is a `double` advanced once per sample by
  `ticks_per_sample = bpm * 960 / (60 * sample_rate)`. **Time in `double`,
  audio in `float`**: a `float` playhead stalls after ~272 bars at
  140 BPM / 48 kHz; a test documents this.
- A beat is one unit of the time-signature denominator.

### 5.2 Transport

- **Play modes:** `LOOP` (default) plays the loop from its start, wrapping at
  the loop length. `SONG` plays the playlist.
- **SONG mode** follows FL: Play starts at the loop-brace start when the brace
  is enabled, otherwise at the start marker. Stop returns to that origin
  (evaluated at the moment of stopping). While stopped, the playhead sits at
  the origin. Clicking the playlist ruler sets the start marker and seeks
  while playing. The loop brace wraps sample-accurately when the playhead
  crosses its end; a playhead already past the end plays on.
- **LOOP mode:** origin is tick 0 of the loop; Stop returns there.
- **Metronome:** synthesized sine burst (~1.5 kHz accented on bar starts,
  ~1 kHz otherwise, −6 dBFS, 5 ms decay constant), starting on the exact
  sample of each beat; cut on Stop.

### 5.3 Snapshot

Built by `compile(const Document&, sample_rate)` on the UI thread. Plain data,
immutable after construction:

- `sounds[]`: id, `std::shared_ptr<const Sample>`, gain (linear, from volume
  dB), pan gains (constant-power, see §5.5), audible flag (after mute),
  sampler params (root, fine, cut itself, gate, glide time).
- `loop_events[]`: note events in loop ticks, sorted by start; notes
  truncated at the loop length; plus `loop_length_ticks`.
- `song_events[]`: the playlist compiled into song ticks (§9.8), sorted by
  start; plus `song_end_ticks`.
- Each event: sound index, start tick, end tick, pitch, velocity, slide flag.

### 5.4 Scheduling

- The engine keeps a cursor into the active event list (`loop_events` or
  `song_events`). Each sample, every event whose start tick has been reached
  fires a note-on on its sound's instrument at that exact sample.
- Note-offs: each voice stores its end tick; when the playhead reaches it the
  instrument gets a note-off (meaningful only with Gate on).
- After a seek, a loop wrap, a play-mode change, or a snapshot swap, the
  cursor is re-found by binary search so that events are neither skipped nor
  fired twice. Already-sounding voices continue across a snapshot swap.
- Rendering stays per sample, so output is identical for any buffer size.

### 5.5 Instruments and sampler

`Instrument` (engine-side interface; the sampler is the only implementation in
stage 1, VST3 implements it later): `prepare(sample_rate)`,
`note_on(pitch, velocity, slide, end_tick)`, `note_off(voice)`, `all_off()`,
`render(float* stereo_out, frames)` into a per-sound buffer preallocated in
`prepare`.

Sampler (`Sampler : Instrument`):

- 16 voices per sound; when all are busy the oldest is stolen (5 ms fade).
- Playback rate = `2^((pitch − root + fine/100) / 12) × file_rate / device_rate`;
  4-point cubic Hermite interpolation; mono samples feed both channels.
- Without Gate the sample plays to its end; with Gate, note end → 5 ms fade.
- Cut itself: a new note fades the sound's previous voice out over 5 ms.
- Slide: a slide note that starts while a voice of the same sound is sounding
  (Cut itself on) does not retrigger; the sounding voice glides its pitch
  linearly in semitones to the new pitch over Glide time (default 80 ms) and
  adopts the new note's end and velocity. With nothing sounding, a slide note
  plays as a normal note.
- Velocity: linear gain 0–1, default 0.8.
- Mixing per sound: instrument buffer × gain × constant-power pan
  (`L = cos(θ)`, `R = sin(θ)`, θ from 0 to π/2, center −3 dB each side),
  skipped when not audible; summed into the master bus with the metronome and
  preview. No limiter. Master peak per channel published for the meter.

### 5.6 Preview

A dedicated preview voice (same sampler code): `PreviewSample` (browser file
at original pitch, −6 dB), `PreviewNote` (a sound at a pitch: channel-rack
click, piano keys, notes grabbed in the piano roll), `StopPreview`. A new
preview fades the previous one out. Payloads come back through the garbage
queue when finished.

### 5.7 Real-time rules

`render()` and everything it calls: no allocation, no free, no locks, no I/O,
no logging, no exceptions. In debug builds a global `operator new` override
aborts on allocation inside the audio thread (tests count instead of abort).
Everything is preallocated in `prepare()` and when snapshots are built.

## 6. Audio device

- Default playback device through miniaudio (shared mode), f32 stereo, native
  sample rate, default period. `Engine::prepare()` runs before the stream
  starts.
- States: `Running`, `Closed`, `NoDevice`, `Error(message)`.
- Opening fails → app keeps running, status bar shows "No audio — press Play
  to retry", Play retries. Device lost → transport stops, same retry path.
  While no audio thread runs, the UI thread pumps the engine each frame so
  commands never pile up.
- CPU meter: render time ÷ buffer duration, smoothed.

## 7. Samples

- `Sample`: float frames, 1 or 2 channels, original sample rate, path.
- `SampleCache` (UI thread): path → `std::weak_ptr<const Sample>`, so a file
  shared by preview and several sounds is decoded once.
- The loader thread decodes WAV, AIFF, FLAC, MP3 with `ma_decoder` to f32 at
  the file's own rate and channel count. Results return to the UI thread
  through a mutex-protected queue, drained once per frame (the loader is not
  real-time, so a mutex is fine there).
- Errors (unreadable, unsupported, longer than 10 minutes) → log error,
  nothing is added.

## 8. UI toolkit (src/ui)

Retained widget tree (as designed for the playlist): `Widget` with rect,
parent, non-owning children list; fixed children are members, dynamic
children are `unique_ptr`s owned by their parent. `EventRouter` handles
hit-testing, mouse capture, hover, focus, key bubbling, and forgetting
removed subtrees. `Canvas` wraps NanoVG (crisp 1-px lines, clipped text with
ellipsis, nested clips). Redraw only when invalidated or while playing.

Widgets: `Label`, `Button`, `ToggleButton`, `TextField` (UTF-8, selection,
clipboard), `NumberBox` (vertical drag, Shift fine, double-click to type,
comma accepted), `Knob` (vertical drag, double-click resets), `Dropdown`,
`PopupMenu` (right-click menus), `Splitter` (draggable divider, min sizes,
double-click resets), `ScrollArea`, `ModalDialog` (overlay with its own
focus), `TreeView` (browser).

**Drag and drop between panels:** the router owns one drag at a time with a
typed payload (`SampleFile{path}`, `SoundRef{id}`, `LoopRef`). A label follows
the cursor; widgets opt in with `accepts_drop(payload, pos)` and
`on_drop(payload, pos)`.

**Cursors:** `UiHost::set_cursor` (arrow, resize horizontal/vertical, move).

## 9. App

### 9.1 Window and layout

- Normal OS window with minimize/maximize/close, dark title bar
  (`DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` on the SDL HWND).
  First start: maximized. Later starts: last size, position, and maximized
  state. Minimum 960×600.

```
+--------------------------------------------------------------+
| Transport                                                    |
+-----------+------------------------------+-------------------+
| Browser   | Playlist                     | Channel Rack      |
|           |                              +-------------------+
|           |                              | Log               |
|           +------------------------------+-------------------+
|           | Piano Roll                                       |
+-----------+--------------------------------------------------+
| Status bar                                                   |
+--------------------------------------------------------------+
```

- Splitters: browser | rest (default 220 px), playlist | right column
  (default 280 px), channel rack / log (default 60 % / 40 %), top area /
  piano roll (piano roll default 40 % of the height). Each panel has a
  minimum size; double-click on a splitter restores its default. Sizes are
  saved in settings.
- Each panel has a thin header with its title and its own controls.
- **Panel focus:** editing shortcuts act on the last clicked panel, which
  shows a thin accent border.

### 9.2 Theme

Own palette in the spirit of Ableton's professional look, not a copy: dark
graphite grays, flat surfaces, 1-px borders, no gradients or shadows, Inter
12–13 px. Every sound gets a muted color from a fixed palette, used in the
channel rack, the drum view and on its notes. One accent color for the
playhead, selection, focus border and active buttons. All colors and metrics
live in `Theme`. After the first window exists, three palette variants are
rendered side by side in one screenshot and the author picks one.

### 9.3 Transport bar

`≡` menu (New, Open, Save, Save As, Export WAV) · Play/Stop · tempo
(20.00–999.00, drag or type) · time signature (numerator 1–32, denominator
1/2/4/8/16) · LOOP/SONG toggle · metronome · loop brace on/off · follow ·
position `bar.beat.sixteenth` and `mm:ss.mmm` · CPU meter · master level
meter (red above 0 dBFS).

### 9.4 Status bar and log

- Status bar: audio state ("Audio: 48000 Hz, 480 samples" / "No audio —
  press Play to retry" / "Audio error: …" / "Audio disabled (--no-audio)"),
  project name with `*` when modified, dropped-command count when non-zero.
- Log panel: time `hh:mm:ss`, level (info gray, warning yellow, error red),
  message. Newest at the bottom, auto-scroll unless scrolled up, keeps the
  last 500 lines, Clear button. Messages: audio opened/lost, sample loaded/
  failed, missing samples, project saved/opened/failed, export finished/
  failed, dropped commands.

### 9.5 Browser

- Header: `+ Folder` (SDL folder dialog), `Preview` toggle (default on).
- Tree of added folders; only folders and audio files (WAV, AIFF, FLAC, MP3);
  folders first, natural sort (`Kick 2` before `Kick 10`). Missing root
  folder: grayed, warning logged. Right-click a root → Remove (disk is
  untouched).
- Click a file → preview (new preview cuts the previous). Up/Down move and
  preview, Left/Right collapse/expand, Enter adds the file to the channel
  rack.
- Drag a file onto the channel rack → new sound; onto the piano roll → new
  sound, selected for editing.
- Directory listing happens on expand (synchronous `std::filesystem`;
  errors logged).

### 9.6 Channel rack

- Row: mute LED · color strip · name · volume knob · pan knob.
- LED click: mute/unmute (applies to loop and song). Ctrl+click: solo
  (mutes all others; Ctrl+click again on the soloed sound unmutes all).
- Name click: select the sound (piano roll switches to it in Keys view) and
  preview it at root pitch. Double-click: rename.
- Right-click menu: Rename, Color, Duplicate, Delete (also deletes the
  sound's notes; undoable).
- Drag a row onto the piano roll: select it there. Drag within the rack:
  reorder. Drop a browser file: append a new sound named after the file,
  next palette color, default settings.
- Volume −∞ to +6 dB (default 0 dB); pan L100–R100 (default center);
  double-click a knob resets it.
- Maximum 64 sounds.

### 9.7 Piano roll

**Loop:** exactly one. Each note: sound id, start tick, length ticks, pitch
(MIDI 0–127, named FL-style: middle C = C5), velocity (0–1, default 0.8),
slide flag. Loop length: `Auto` (end of the last note rounded up to whole
bars, minimum one bar) or fixed 1, 2, 4, 8 bars.

**Header:** `[Keys|Drums]` · sound selector · Root · Fine · `Cut itself` ·
`Gate` · Glide (ms) · Length (`Auto`/1/2/4/8) · `Roll ▾` · `⠿ Loop` drag
handle (drag onto the playlist).

**Keys view:** vertical piano keyboard on the left (click previews that
pitch); notes of the selected sound in its color; notes of other sounds drawn
gray behind and not editable; octave lines at C.

**Drums view:** one row per sound in channel-rack order; row header click
selects and previews. A note sits in its sound's row regardless of pitch; a
pitch different from the root shows a small label (`+2`). New notes get the
root pitch. Dragging vertically moves notes to another sound, keeping their
offset from root.

**Editing (both views): FL-style clicks, Ableton-style dragging.**
- Left click on empty space: add a note with the last used length (initially
  one grid step); keep holding to move it.
- Left click on a note: select it (Shift adds/removes); dragging moves the
  whole selection with **relative snapping** (each note keeps its offset from
  the grid); vertically by semitone (Keys) or by row (Drums); the grabbed
  note is auditioned, and again whenever its pitch changes.
- Dragging a note's left/right edge resizes all selected notes.
- Ctrl+drag a note: duplicate the selection and drag the copies.
- Ctrl+drag on empty space: rectangle selection.
- Right click or right drag: delete notes under the cursor.
- Alt held: no snapping.
- Keys: `S` toggles slide on selected notes (drawn with a marker), Delete,
  Ctrl+A, Ctrl+C/V/D, Ctrl+Z/Y, Left/Right move by one grid step, Up/Down by
  a semitone (Shift: octave).
- **Grid** (Ableton): adaptive by default; Ctrl+1 narrower, Ctrl+2 wider,
  Ctrl+3 triplets, Ctrl+4 snap on/off.
- Zoom: Ctrl+wheel horizontal (around the mouse); wheel scrolls vertically,
  Shift+wheel horizontally.

**Velocity lane** (below the notes): one bar per note (Drums view: the
selected row's notes). Drag a bar to set one note; drag across empty space to
draw a line that sets every note it crosses (interpolated along the line).

**Roll tool:** with notes selected, `Roll ▾` → 1/16, 1/32, 1/64, 1/16T, 1/32T
replaces each selected note by repeats of that length filling the note's
length (same pitch, velocity; slide off). One undo step.

**Playback:** in LOOP mode the piano roll shows its own playhead.

### 9.8 Playlist

- Ruler: loop-brace strip and bar numbers, as designed earlier (drag to
  create/move/resize the brace, double-click toggles it, click on bar numbers
  sets the start marker; snapping to the grid, Alt disables). Adaptive grid,
  Ctrl+wheel zoom (2–2000 px per quarter note), Shift+wheel horizontal and
  wheel vertical scroll, timeline up to 1000 bars, follow with page jumps.
- **Tracks:** free lanes (any clip on any track, overlaps allowed). Header:
  name (double-click renames, max 64 chars), `M` mutes the track. Ctrl+T adds
  a track after the selected one; Delete removes the selected track when the
  playlist has focus and no clip is selected. Default: four tracks.
- **Clips** reference the loop. Drawn in a neutral color with "Loop" and a
  miniature of all notes, with dividers at loop repeats.
  - Left click on an empty lane: place a loop clip there (snapped). Or drag
    the `⠿ Loop` handle from the piano roll header.
  - Same gestures as the piano roll: click selects, drag moves (snapped),
    edges resize, right click deletes, Ctrl+drag copies, Ctrl+drag on empty
    space selects by rectangle, Ctrl+C/V/D, Delete, Alt disables snapping.
  - Stretching the right edge repeats the loop; shortening cuts it; the left
    edge trims the start (`offset` into the loop).
  - A clip whose length was never changed by hand (`auto_length`) follows the
    loop length.
- **Compilation to song events:** for each clip on an unmuted track, every
  loop repeat inside the clip contributes the loop's notes shifted to song
  time; notes before the clip start (offset) are skipped, notes crossing the
  clip end are cut at the clip end. `song_end_ticks` = end of the last clip.

### 9.9 Shortcuts

| Key | Action |
|---|---|
| Space | Play/Stop |
| Ctrl+N / O / S / Shift+S / E | New / Open / Save / Save As / Export |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+L | Loop brace on/off |
| Ctrl+T | Add track |
| Delete, Ctrl+A, Ctrl+C/V/D, arrows, S, Ctrl+1/2/3/4 | Editing in the focused panel |
| Up/Down/Left/Right, Enter | Browser navigation |

A focused text field consumes keys except Ctrl shortcuts other than
Ctrl+A/C/V/X.

## 10. Document, Controller, undo, clipboard

- `Document` (everything saved in a project): tempo, signature, metronome,
  play mode, start marker, loop brace, follow; sounds (id, name, color index,
  sample path, volume dB, pan, muted, root, fine, cut itself, gate, glide ms);
  loop (length mode, notes); tracks (id, name, muted); clips (id, track id,
  start, length, offset, auto length).
- `Controller` is the only code that mutates the `Document`. After each
  mutation it recompiles and sends a snapshot, sends transport commands where
  needed, marks the project modified, and invalidates the window.
- **Undo:** before each action the Controller pushes a copy of the whole
  `Document` onto the undo stack (maximum 200); `begin_gesture()` /
  `end_gesture()` make a whole mouse drag one step. Redo is cleared by a new
  action. View state (zoom, scroll, selection, panel sizes) is not undone.
- **Clipboard:** internal, separate for notes and clips. Paste puts the
  earliest item at the panel's edit position (last left-click position,
  snapped); notes keep their pitch and sound.

## 11. Project file

- `.aloglas`, JSON, `"format_version": 1`. Contains the `Document`. Sample
  paths are stored absolute and relative to the project file; on load the
  relative path is tried first.
- Save writes `<name>.tmp` and renames it over the target.
- Load errors (invalid JSON, newer format version, schema mismatch) → message
  box + log; the open project is untouched. A missing sample keeps its sound
  (name in red, silent) and logs a warning.
- Unsaved changes on New/Open/close → message box: Save / Don't Save / Cancel.

## 12. WAV export

- Dialog: range `Song` or `Loop`, sample rate 44100 or 48000, bit depth 16,
  24 or 32 float; then the SDL save dialog.
- The export worker builds a private `Engine` at the chosen rate with the
  current snapshot and renders offline (faster than real time) with a
  progress bar; the live engine keeps playing.
- Song: from 0 to `song_end_ticks`, plus a tail until the output stays below
  −90 dBFS for 100 ms or 10 s have passed. Loop: exactly one loop length, no
  tail (seamless).
- 16-bit output uses TPDF dither. Writing uses `ma_encoder` (WAV).
- Errors (cannot write) → message box + log.

## 13. Settings

`%APPDATA%\Aloglas\settings.json`: window size/position/maximized, splitter
positions, browser folders, preview on/off. Unreadable settings → defaults
plus a log warning.

## 14. Testing

doctest, no sound card or window needed.

- **Engine timing:** clicks on exact samples at 120 BPM; no drift after 1000
  beats at 133 BPM; output identical for buffer sizes 1/64/480/1024; tempo
  change mid-play; signatures 4/4, 6/8, 7/4; loop-brace wrap on the exact
  sample; FL play origin and stop rules; float-vs-double lesson.
- **Sampler:** a sine sample played at +12 has exactly twice the frequency
  (zero crossings); Gate fades within 5 ms; Cut itself; glide reaches the
  target pitch in Glide time; velocity and constant-power pan values; mute
  and solo; voice stealing.
- **Scheduling:** note on the loop boundary fires on the exact sample; song
  compilation (repeats, trims, offsets, cut at clip end, muted tracks);
  snapshot swap mid-playback keeps voices and silences removed sounds; old
  snapshots and preview payloads come back through the garbage queue;
  `render()` never allocates.
- **Editing logic:** undo/redo with gesture coalescing, paste positions, roll
  tool, velocity line drawing, auto loop length, auto clip length.
- **Project:** save → load round trip gives an equal `Document`; missing
  sample, invalid JSON, newer version.
- **Files:** decoding of small WAV, AIFF, FLAC and MP3 fixtures committed to
  `tests/data`; export output compared with an offline engine render; 16-bit
  dither error within ±1 LSB.
- **UI:** simulated mouse/keyboard through `EventRouter` for the piano roll,
  playlist, splitters, browser and channel rack; `--screenshot` PNGs for the
  look; manual listening checks.

## 15. Build order and checkpoints

1. Foundation: build system, time, SPSC queue, RT guard, transport engine,
   audio device, UI toolkit core, window, basic widgets.
2. Window layout: splitters, panels, log, status bar, menu, settings.
   **Checkpoint: the author picks one of three palettes.**
3. Samples and browser with preview.
4. Snapshots, scheduler, sampler, channel rack.
   **Checkpoint: click a kick and hear it.**
5. Piano roll: Keys, Drums, editing, velocity, roll, slide/glide.
   **Checkpoint: first beat.**
6. Playlist clips, LOOP/SONG.
7. Project file, undo/redo, clipboard.
8. WAV export. **Checkpoint: full review.**

## 16. Later

VST3 instruments and effects (next spec; `Instrument` interface is the
seam), multiple loops, browser search, relinking missing samples, the
bottom-left panel, mixer, Linux/macOS.
