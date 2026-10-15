# Stage 1, Step 3: Samples and Browser with Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The BROWSER panel works: the author adds sample folders, browses them as a tree (folders first, natural sort, audio files only), and hears a file the moment it is clicked or reached with the arrow keys; the folders and the Preview switch survive a restart.

**Architecture:** A new `samples` library decodes files with miniaudio into immutable `Sample`s on a loader thread; a `SampleCache` shares them. The engine gets its first sound source besides the metronome: a preview `SampleVoice` (4-point Hermite, rate conversion, 5 ms fades) fed through the command queue, with ownership handed back through a garbage queue so the audio thread never frees memory (spec §4.2). The browser is a generic `TreeView` over an app-side `BrowserModel`.

**Tech Stack:** as step 2; miniaudio's decoder (WAV, AIFF, FLAC, MP3), `SDL_ShowOpenFolderDialog`, `std::thread`.

**Spec:** `docs/superpowers/specs/2026-10-13-stage1-playlist-transport-design.md` (§4.1, §4.2, §5.5 interpolation, §5.6, §5.7, §7, §8 TreeView/PopupMenu, §9.4 log, §9.5, §13; build order step 3)

**Previous plan:** `docs/superpowers/plans/2026-10-15-stage1-step2-layout-transport.md` (done, merged to `main` as `4f4f28c`).

## Global Constraints

- Everything in step 2's Global Constraints still applies (toolchain, `-static`, pinned deps, naming, English code with *why*-comments, `-Wall -Wextra` clean, real-time rules for `render()`, commit trailers of this session, `git pull --rebase --autostash` before each commit, **never push**, explicit file lists in `CMakeLists.txt`, dense flat look from `Theme`).
- Work on a branch `stage1-step3` from `main`.
- Library graph after this step (spec §4): `core` ← `samples` ← `engine` ← `audio`; `core` ← `ui`; all ← `aloglas_app_lib`. miniaudio's implementation moves from `aloglas_audio` into its own static target `aloglas_miniaudio` (`src/audio/miniaudio_impl.c`, compiled with `-w`), linked by both `aloglas_samples` and `aloglas_audio`.
- Paths are `std::filesystem::path` everywhere; text shown or saved is UTF-8 via `u8string()` (Polish folder names must work). Never `path.string()` (throws for characters outside the ANSI code page).
- The audio thread never allocates or frees (spec §5.7). Anything the engine reads by pointer is owned by the engine until it comes back through the garbage queue (spec §4.2).
- Spec values: preview at original pitch, −6 dB (`0.5012f`); a new preview fades the previous one out over 5 ms; decode limit 10 minutes; formats WAV, AIFF, FLAC, MP3; more than 2 channels are downmixed to stereo; natural sort (`Kick 2` before `Kick 10`), case-insensitive, folders first.
- Test fixtures live in `tests/data/`; tests find them through the compile definition `ALOGLAS_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data"` on `aloglas_tests`.

**Deferred by design (later steps):** dragging files to the channel rack / piano roll and Enter → channel rack (step 4, the rack doesn't exist yet), relinking missing samples and browser search (spec §16).

**Ruling (plan):** successful preview loads are **not** logged (arrowing through a folder would flood the log); failures are. "Sample loaded" lines come with step 4's channel-rack loads.

## Review Focus

1. **Arrowing quickly through 50 large files.** Only the file the selection ends on plays; the loader doesn't work through a backlog of superseded previews; the UI never waits for a decode. → Task 2 `a new preview request replaces pending previews`; Task 6 `only the latest preview plays`.
2. **Folders and files with Polish characters** (`Sample żółć/Kick ą.wav`). They list, sort, decode, play, are saved in settings and come back after a restart. → Task 1 `decodes a file with a non-ASCII path`; Task 4 `lists non-ASCII names`; Task 7 `settings keep non-ASCII browser folders`.
3. **A root folder that is gone** (USB drive unplugged, renamed) **or a subfolder without permission.** The root shows grayed with one warning; listing errors are logged; nothing crashes; Remove still works. → Task 4 `missing root is dim and warns once`, `unlistable folder logs and shows empty`.
4. **Files that aren't what they claim:** a text file renamed `.wav`, a 0-byte file, a 6-channel file, a 20-minute file. Errors are logged and nothing plays; 6 channels become stereo; the length limit rejects without decoding everything. → Task 1 `rejects garbage, empty and too long files`, `downmixes more than two channels`.
5. **Preview ownership edges:** the command queue is full, the device is lost while a preview plays, the app quits mid-preview. Payloads are never freed on the audio thread, never leak, and the sample is freed once nothing uses it. → Task 3 `dropped preview command frees its payload`, `engine destructor frees held previews`, `render does not allocate or free with previews`.

---

## File Map

```
src/samples/sample.h/.cpp        Sample, DecodeResult, decode_sample
src/samples/sample_loader.h/.cpp SampleLoader (thread), LoadResult
src/samples/sample_cache.h/.cpp  SampleCache (path -> weak_ptr)
src/engine/sample_voice.h/.cpp   SampleVoice (Hermite, rate, fade)
src/engine/preview.h             PreviewPayload
src/engine/garbage.h             Garbage item (destroy fn + pointer)
src/engine/commands.h            + PreviewSample, StopPreview
src/engine/engine.h/.cpp         preview voices, garbage queue, previewing()
src/core/natural_sort.h/.cpp     natural_less
src/app/browser_model.h/.cpp     BrowserModel, BrowserNode, BrowserRow, is_audio_file
src/ui/widgets/tree_view.h/.cpp  TreeView, TreeRow
src/ui/widgets/popup_menu.h/.cpp PopupMenu
src/ui/widget.h, window.h/.cpp   UiHost::pick_folder, Window::wake
src/app/previewer.h/.cpp         Previewer
src/app/browser_panel.h/.cpp     BrowserPanel
src/app/settings.h/.cpp          + browser folders, preview flag
src/app/live_state.h, frame_update.cpp  + previewing, collect_garbage
src/app/main_view.h/.cpp, main.cpp      wiring
tests/data/                      fixtures (generated with ffmpeg, committed)
```

---

### Task 1: Sample decoding

**Files:**
- Create: `src/samples/sample.h/.cpp`, `tests/sample_test.cpp`, `tests/data/*` (fixtures below)
- Modify: `CMakeLists.txt` (targets `aloglas_miniaudio`, `aloglas_samples`; `ALOGLAS_TEST_DATA`)

**Interfaces:**
- Produces:
  - `struct Sample { std::vector<float> frames; int channels = 1; uint32_t sample_rate = 0; std::filesystem::path path; int64_t frame_count() const; };` (interleaved, 1 or 2 channels)
  - `struct DecodeResult { std::shared_ptr<const Sample> sample; std::string error; };` (exactly one is set)
  - `DecodeResult decode_sample(const std::filesystem::path& path, double max_seconds = 600.0);`

Rules:
- `ma_decoder_init_file_w(path.c_str(), …)` with `ma_format_f32`, native rate. Init once with channels 0 (native); if the file has more than 2 channels, uninit and init again with 2 (miniaudio downmixes).
- Read in chunks of 4096 frames until the end (MP3 length may be unknown up front). Stop and fail as soon as the count passes `max_seconds * rate`.
- Errors (`error` text, no sample): `"cannot read <name>"` (init failed: missing, unsupported, garbage), `"<name> is empty"`, `"<name> is longer than 10 minutes"` (minutes from `max_seconds`). `<name>` = `path.filename().u8string()`.

Fixtures (commit them; generate once with ffmpeg, ~0.1 s, 440 Hz, ffmpeg's sine amplitude is 1/8):
```
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.1" -ac 1 -c:a pcm_s16le tests/data/sine_mono_44k.wav
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.1" -ac 2 -c:a pcm_s16le tests/data/sine_stereo_48k.wav
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.1" -ac 1 -c:a pcm_s16be tests/data/sine_mono_44k.aiff
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.1" -ac 1 tests/data/sine_mono_44k.flac
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=0.1" -ac 1 -b:a 128k tests/data/sine_mono_44k.mp3
ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=0.1" -ac 6 -c:a pcm_s16le tests/data/sine_6ch_48k.wav
printf 'this is not audio' > tests/data/not_audio.wav;  : > tests/data/empty.wav
```

- [ ] **Step 1: Generate the fixtures** (commands above). Expected: 8 files, the largest under 60 KB.
- [ ] **Step 2: Write the failing tests**

```cpp
TEST_CASE("decodes WAV, AIFF and FLAC exactly") {
    // for each of sine_mono_44k.{wav,aiff,flac}: channels 1, rate 44100, frame_count 4410,
    // max |frame| == Approx(0.125).epsilon(0.02), path == the file
}
TEST_CASE("decodes MP3") { /* channels 1, rate 44100, frame_count within [4410, 4410 + 2400] (encoder padding) */ }
TEST_CASE("keeps stereo") { /* sine_stereo_48k.wav: channels 2, rate 48000, frames.size() == 2 * 4800 */ }
TEST_CASE("downmixes more than two channels") { /* sine_6ch_48k.wav: channels 2, frame_count 4800 */ }
TEST_CASE("decodes a file with a non-ASCII path") {                       // Review Focus 2
    // copy sine_mono_44k.wav to temp_directory_path() / u8"Sample żółć" / u8"Kick ą.wav" → decodes, 4410 frames
}
TEST_CASE("rejects garbage, empty and too long files") {                   // Review Focus 4
    // not_audio.wav → error contains "cannot read"; empty.wav → error non-empty; missing path → "cannot read"
    // decode_sample(sine_mono_44k.wav, 0.05) → error contains "longer than", no sample
}
```

- [ ] **Step 3: Run** `build/debug/aloglas_tests.exe -tc="decodes*,keeps stereo,downmixes*,rejects*"` — Expected: compile failure (no `samples/sample.h`).
- [ ] **Step 4: Implement**; move `miniaudio_impl.c` to `aloglas_miniaudio`; add `aloglas_samples` (links `aloglas_core`, PRIVATE `aloglas_miniaudio aloglas_miniaudio_headers`); `aloglas_engine` links `aloglas_samples` PUBLIC.
- [ ] **Step 5: Run** the full suite — Expected: PASS.
- [ ] **Step 6: Commit** — `samples: decode WAV/AIFF/FLAC/MP3 into Sample`

---

### Task 2: SampleLoader and SampleCache

**Files:**
- Create: `src/samples/sample_loader.h/.cpp`, `src/samples/sample_cache.h/.cpp`
- Test: `tests/sample_loader_test.cpp`

**Interfaces:**
- Consumes: `decode_sample`, `Sample`.
- Produces:
  - `struct LoadResult { uint64_t id = 0; std::filesystem::path path; bool preview = false; std::shared_ptr<const Sample> sample; std::string error; };`
  - `class SampleLoader { public: explicit SampleLoader(std::function<DecodeResult(const std::filesystem::path&)> decode = [](const auto& p) { return decode_sample(p); }); ~SampleLoader(); uint64_t request(const std::filesystem::path&, bool preview); std::vector<LoadResult> take_results(); std::function<void()> on_result; };` — `decode` is injectable so tests can hold the worker inside a decode deterministically.
    - One worker `std::thread`; a `std::mutex` + `std::condition_variable` guard the request deque and the result vector (the loader is not real-time, so locks are fine — comment why).
    - `request(…, preview = true)` first removes every **pending** preview request (not the one being decoded) — superseded previews are never decoded.
    - Ids increase from 1. `on_result` (set before the first request; called on the **worker thread** after a result is stored) lets the UI wake its event loop.
    - Destructor: sets a stop flag, wakes and joins the worker; pending requests are dropped.
  - `class SampleCache { public: std::shared_ptr<const Sample> find(const std::filesystem::path&) const; void put(std::shared_ptr<const Sample>); };` — keyed by `path.lexically_normal()`, stores `std::weak_ptr` (a sample lives only while something uses it); `put` also prunes expired entries.

- [ ] **Step 1: Write the failing tests**

```cpp
// helper: poll take_results() every 5 ms for up to 5 s until n results arrived
TEST_CASE("loader decodes in the background and reports errors") {
    // request(sine_mono_44k.wav, false) and request(not_audio.wav, false) → 2 results with matching ids;
    // first has sample (4410 frames), second has error; on_result was called twice
}
TEST_CASE("a new preview request replaces pending previews") {              // Review Focus 1
    // decode blocks on a latch for the first call: request(p0, preview) — the worker is now stuck in it;
    // request(p1..p29, preview); release the latch → exactly 2 results: p0 (already running) and p29 (the last);
    // decode was called exactly twice
}
TEST_CASE("non-preview requests are never dropped") { /* 5 normal requests interleaved with previews → all 5 arrive */ }
TEST_CASE("cache shares while used and forgets after") {
    // put(sample) → find(path) == same pointer; find(other spelling "dir/./x.wav") finds it too;
    // reset all shared_ptrs → find returns nullptr
}
TEST_CASE("destroying the loader with pending work joins cleanly") { /* 20 requests, destroy at once: no hang (test finishes) */ }
```

- [ ] **Step 2: Run** `-tc="loader*,a new preview*,non-preview*,cache*,destroying the loader*"` — Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `samples: background loader and shared sample cache`

---

### Task 3: Engine preview voice and garbage queue

**Files:**
- Create: `src/engine/sample_voice.h/.cpp`, `src/engine/preview.h`, `src/engine/garbage.h`
- Modify: `src/engine/commands.h`, `src/engine/engine.h/.cpp`, `tests/support/recording_sink.h`, `src/app/live_state.h`, `src/app/frame_update.cpp`
- Test: `tests/engine_preview_test.cpp`, `tests/sample_voice_test.cpp`

**Interfaces:**
- Consumes: `Sample`.
- Produces:
  - `struct PreviewPayload { std::shared_ptr<const Sample> sample; float gain = 0.5012f; };` (−6 dB)
  - `struct Garbage { void (*destroy)(void*) = nullptr; void* object = nullptr; };`
  - `CommandType::PreviewSample` (`Command::preview_sample(PreviewPayload* owned)`, new field `PreviewPayload* preview = nullptr` — the only pointer a Command carries; ownership moves to the engine with it) and `CommandType::StopPreview` (`Command::stop_preview()`).
  - `class SampleVoice { public: void start(const Sample&, double step, float gain); void fade_out(double sample_rate); void stop(); bool active() const; void render_frame(float& left, float& right); };` — `step` = source frames per output frame (`file_rate / device_rate`); 4-point cubic Hermite between frames, samples outside the file read as 0; mono feeds both channels; ends when the read position passes the last frame; `fade_out` = linear ramp to 0 over 5 ms then inactive.
  - `Engine`: `void collect_garbage();` (UI thread: pops the garbage queue and destroys each item), `bool previewing() const` (published atomic), and `~Engine()` frees payloads still held (precondition: no audio thread runs — documented).
  - `Engine::send` failing for a command that carries a payload **deletes the payload** on the spot (UI thread) so a full queue can't leak it.
  - `LiveState::previewing`; `frame_update` sets it and calls `engine.collect_garbage()`; `needs_frames` includes it (the meter moves during previews).
  - `RecordingSink` deletes the payloads of recorded `PreviewSample` commands in its destructor.

Engine rules:
- Two voice slots, `current` and `fading`, each with its owning `PreviewPayload*`.
- `PreviewSample(p)`: if `fading` is busy, stop it and hand its payload to the garbage queue; move `current` to `fading` and fade it; start `p` in `current` (`step = p->sample->sample_rate / sample_rate_`).
- `StopPreview`: same as above without a new voice.
- When a voice becomes inactive (end or fade), its payload goes to the garbage queue.
- Garbage queue `SpscQueue<Garbage, 256>` (audio → UI). If a push fails, the item waits in a fixed `std::array<Garbage, 8>` and is retried at the start of the next `render()`; when that is full too, the item is counted in `leaked_garbage()` (debug assert) — never freed on the audio thread.
- Preview output is added to the metronome in the output loop and counted in the master peak.

- [ ] **Step 1: Write the failing tests**

```cpp
// sample_voice_test.cpp — Sample built in the test, 48 kHz device
TEST_CASE("voice plays a sample at the same rate frame by frame") {
    // DC sample 0.5, 100 frames, step 1, gain 1 → 100 frames of 0.5 (exact), then inactive
}
TEST_CASE("voice converts the sample rate") {
    // DC 0.5, 100 frames at 24 kHz on 48 kHz (step 0.5) → active for 199..200 frames; frames 4..190 == Approx(0.5)
}
TEST_CASE("Hermite interpolation passes through the samples and is smooth") {
    // ramp 0,1,2,…,9 (step 0.25) → at integer positions the exact value; between them monotonic increasing
}
TEST_CASE("mono feeds both channels, stereo stays apart") {}
TEST_CASE("fade_out reaches silence within 5 ms") { /* 48 kHz: after 240 frames inactive; values decrease */ }

// engine_preview_test.cpp
TEST_CASE("preview plays at -6 dB and the payload comes back") {
    // DC 1.0 sample 480 frames @48k; send preview_sample(new PreviewPayload{s}); render 100 → out == Approx(0.5012);
    // previewing() true; render 1000 → previewing() false; collect_garbage(); weak_ptr to the sample expired
}
TEST_CASE("a new preview fades the previous one out") {
    // long DC sample A, render 100; preview B (DC 0.25); within 240 frames output → B level; A's payload returned
}
TEST_CASE("three previews in a row never hold more than two") { /* A, B, C back to back: A returned at once */ }
TEST_CASE("stop_preview fades and returns") {}
TEST_CASE("dropped preview command frees its payload") {                    // Review Focus 5
    // set_assert_on_drop(false); fill the queue with 256 stops; send preview → false; sample weak_ptr expired
}
TEST_CASE("engine destructor frees held previews") {                        // Review Focus 5
    // preview a long sample, render 10, destroy engine without collect → weak_ptr expired
}
TEST_CASE("render does not allocate or free with previews") {               // Review Focus 5
    // rt guard as in engine_clock_test; preview, render, preview again, render until done — violation_count unchanged.
    // Also cover the LOOP wrap path: set_play_mode(Loop), set_loop_length(960), play, render 50'000 frames.
}
```

- [ ] **Step 2: Run** `-tc="voice*,Hermite*,mono feeds*,fade_out*,preview*,a new preview fades*,three previews*,stop_preview*,dropped preview*,engine destructor*,render does not allocate or free*"` — Expected: compile failure.
- [ ] **Step 3: Implement** (`frame_update`: `collect_garbage()` every frame, after the pump).
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `engine: preview voice with Hermite interpolation and a garbage queue`

---

### Task 4: Browser model

**Files:**
- Create: `src/core/natural_sort.h/.cpp`, `src/app/browser_model.h/.cpp`
- Test: `tests/natural_sort_test.cpp`, `tests/browser_model_test.cpp`

**Interfaces:**
- Consumes: `Log`.
- Produces:
  - `bool natural_less(std::string_view a, std::string_view b);` — case-insensitive (ASCII folding; other UTF-8 bytes compare as bytes), digit runs compared by value (leading zeros ignored, shorter run first on ties).
  - `bool is_audio_file(const std::filesystem::path&);` — extension `.wav .aif .aiff .flac .mp3`, any case.
  - `struct BrowserNode { std::filesystem::path path; std::string name; bool folder = false; bool expanded = false; bool listed = false; bool missing = false; std::vector<BrowserNode> children; };`
  - `struct BrowserRow { BrowserNode* node; int depth; int root_index; };` (`root_index` is the index of the root the row belongs to; `depth == 0` means the row is that root)
  - `class BrowserModel { public: explicit BrowserModel(Log&); bool add_root(const std::filesystem::path&); void remove_root(std::size_t root_index); void set_expanded(std::size_t row, bool expanded); const std::vector<BrowserRow>& rows() const; std::vector<std::filesystem::path> root_paths() const; std::function<void()> on_change; };`

Rules:
- `add_root`: duplicates (same `lexically_normal`) are ignored (return false). A root that doesn't exist or isn't a folder is added `missing` (shown dim, can't expand) with one Warning `"Folder not found: <path u8>"`. Roots keep insertion order. Name = folder name (`u8string`), or the full path for a drive root.
- `set_expanded(row, true)` lists the folder the first time (synchronous `directory_iterator` with `skip_permission_denied` and `std::error_code`): sub-folders and audio files only, hidden/system entries included as the OS reports them; folders first, then `natural_less` by name. A listing error logs Warning `"Cannot list <path>: <message>"`; the folder stays expanded and empty. Rows are rebuilt after each change; `on_change` fires.
- `rows()`: depth-first over expanded folders.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("natural order") {
    CHECK(natural_less("Kick 2", "Kick 10")); CHECK_FALSE(natural_less("Kick 10", "Kick 2"));
    CHECK(natural_less("kick", "Snare"));
    CHECK(natural_less("a1", "a01")); CHECK_FALSE(natural_less("a01", "a1"));  // equal values: shorter digit run first
    CHECK(natural_less("808 a", "808 b")); CHECK_FALSE(natural_less("x", "x"));
}
TEST_CASE("audio extensions") { /* .WAV yes, .Aiff yes, .mp3 yes, .ogg no, .txt no, no extension no */ }
// temp tree: root/{B/, a/, Kick 10.wav, Kick 2.wav, notes.txt, hat.MP3}
TEST_CASE("folders first, natural order, audio only") {
    // add_root(root) → 1 row; set_expanded(0, true) → rows: root, a, B, hat.MP3, Kick 2.wav, Kick 10.wav
}
TEST_CASE("collapse hides children; expand again doesn't relist") { /* add a file after first expand → still not shown */ }
TEST_CASE("lists non-ASCII names") {                                        // Review Focus 2
    // root/u8"Sample żółć"/u8"Kick ą.wav" → names come back as the same UTF-8 strings
}
TEST_CASE("missing root is dim and warns once") {                           // Review Focus 3
    // add_root(nonexistent) → true, missing; one Warning; set_expanded(0,true) → still 1 row; remove_root(0) → 0 rows
}
TEST_CASE("duplicate roots are ignored") { /* add_root(root) twice, second with "root/." → false */ }
TEST_CASE("unlistable folder logs and shows empty") {                       // Review Focus 3
    // root/sub exists when root is listed; delete root/sub from disk; set_expanded(row of sub, true)
    // → one Warning containing "Cannot list", sub expanded with no children, no crash
}
```

- [ ] **Step 2: Run** `-tc="natural order,audio extensions,folders first*,collapse*,lists non-ASCII*,missing root*,duplicate roots*,unlistable*"` — Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: browser model with natural sort`

---

### Task 5: Toolkit — TreeView, PopupMenu, folder dialog, wake

**Files:**
- Create: `src/ui/widgets/tree_view.h/.cpp`, `src/ui/widgets/popup_menu.h/.cpp`
- Modify: `src/ui/widget.h`, `src/ui/window.h/.cpp`, `tests/support/fake_host.h`
- Test: `tests/tree_view_test.cpp`, `tests/popup_menu_test.cpp`

**Interfaces:**
- Produces:
  - `virtual void UiHost::pick_folder(std::function<void(std::filesystem::path)> on_picked) = 0;` — Window: `SDL_ShowOpenFolderDialog` (one at a time; a second call while open is ignored). The callback may run on another thread (SDL docs), so it stores the UTF-8 result under a mutex and calls `wake()`; `Window::run` delivers it to `on_picked` on the UI thread. Cancel/error → `on_picked` is not called. FakeHost stores the callback in `std::function<void(std::filesystem::path)> folder_request`.
  - `void Window::wake();` — thread-safe: pushes a registered SDL user event so `SDL_WaitEvent` returns.
  - `struct TreeRow { int depth = 0; bool folder = false; bool expanded = false; bool dim = false; std::string label; };`
  - `class TreeView : public Widget { public: std::function<std::size_t()> count; std::function<TreeRow(std::size_t)> row; std::function<void(std::size_t)> on_select; std::function<void(std::size_t, bool expand)> on_expand; std::function<void(std::size_t, Point)> on_context; int selected() const; void select(int index); bool focusable() const override { return true; } };`
  - `class PopupMenu : public Widget { public: void open(Point at, std::vector<std::string> items, std::function<void(int)> on_choose); void close(); bool is_open() const; bool focusable() const override; };`

TreeView behavior (rows `row_height` tall, indent 12 px per depth, disclosure triangle in the first 12 px after the indent; selected row in `selection`; dim rows in `text_dim`; files `text_dim`, folders `text`, selected `text` — as in the step-2 mock):
- Left press on a row: focus, select, `on_select`. Press on the triangle of a folder, or a double-click on a folder: `on_expand(i, !expanded)`.
- Right press on a row: select (no `on_select`), `on_context(i, pos)`.
- Keys (when focused): Up/Down move the selection by one (clamped) and call `on_select`; Right on a collapsed folder → `on_expand(i, true)`, on an expanded folder → select the first child; Left on an expanded folder → `on_expand(i, false)`, otherwise select the parent row (nearest row above with `depth - 1`). Other keys are not handled (bubble).
- Wheel: 3 rows per notch; the scroll is clamped; selection changes by key scroll it into view. `select()` clamps to `[-1, count-1]`.

PopupMenu behavior: hidden when closed. When open it covers its parent's rect (the parent lays it out over the whole window, as the last child) and draws a menu box at `at` (moved inside the rect), rows `row_height + 4`, width = widest label + 24, `panel_header` fill, 1-px `panel_border`, hovered row in `selection`. Press on an item → close, then `on_choose(i)`. Press anywhere else → close (consumes the press). Escape → close. `open` focuses it.

- [ ] **Step 1: Write the failing tests**

```cpp
// 5 rows: 0 folder(expanded, depth 0), 1 folder(collapsed, depth 1), 2 file(depth 1), 3 file(depth 1), 4 folder(depth 0)
TEST_CASE("tree click selects and reports") {}
TEST_CASE("tree triangle and double-click expand folders") {}
TEST_CASE("tree arrows move, expand, collapse and go to the parent") {
    // select 2; Down → 3 (on_select 3); Up,Up → 1; Right on 1 → on_expand(1,true); Left on 3 → select 0;
    // Left on 0 (expanded) → on_expand(0,false); Up at 0 stays 0
}
TEST_CASE("tree right click selects without previewing") {}
TEST_CASE("tree wheel scrolls and keys keep the selection visible") {}
TEST_CASE("popup chooses an item and closes") {}
TEST_CASE("popup closes on a press outside or Escape without choosing") {}
TEST_CASE("pick_folder is routed through the host") { /* widget calls host()->pick_folder; FakeHost invokes folder_request(u8"C:/x/ż") → callback got it */ }
```

- [ ] **Step 2: Run** `-tc="tree*,popup*,pick_folder*"` — Expected: compile failure.
- [ ] **Step 3: Implement** (Window parts are not unit-tested; they get the Task 7 live check).
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `ui: tree view, popup menu, folder dialog and thread-safe wake`

---

### Task 6: Previewer

**Files:**
- Create: `src/app/previewer.h/.cpp`
- Test: `tests/previewer_test.cpp`

**Interfaces:**
- Consumes: `CommandSink`, `SampleLoader`, `SampleCache`, `Log`, `PreviewPayload`.
- Produces: `class Previewer { public: Previewer(CommandSink& engine, SampleLoader& loader, SampleCache& cache, Log& log); std::function<bool()> audio_ready; void preview(const std::filesystem::path&); void stop(); void on_result(const LoadResult&); };`

Rules:
- `preview(p)`: remember `p` as the wanted preview. If `audio_ready` is set and returns false → nothing (no decode either). Cached → send `preview_sample(new PreviewPayload{sample})` now. Otherwise `loader.request(p, true)`.
- `stop()`: forget the wanted path, send `stop_preview()`.
- `on_result(r)`: a sample → `cache.put`; an error → log Error `"Cannot load <name>: <error>"` (preview or not). Plays only if `r.preview` and `r.path` is still the wanted path (the user may have moved on).

- [ ] **Step 1: Write the failing tests** (RecordingSink; real SampleLoader on the fixtures; helper waits for results and feeds them to `on_result`)

```cpp
TEST_CASE("preview loads then plays") { /* preview(wav) → nothing sent; after the result → one PreviewSample with 4410 frames */ }
TEST_CASE("a cached sample plays at once") { /* keep a shared_ptr in the cache; preview → sent immediately, no request */ }
TEST_CASE("only the latest preview plays") {                                // Review Focus 1
    // preview(a), preview(b) before any result arrives → after both results: exactly one PreviewSample, for b
}
TEST_CASE("no audio, no preview") { /* audio_ready false → nothing sent, nothing requested (loader idle) */ }
TEST_CASE("failed loads are logged") { /* not_audio.wav → one Error line containing "not_audio.wav" */ }
TEST_CASE("stop forgets the wanted preview") { /* preview(a); stop(); result for a → no PreviewSample; StopPreview sent */ }
```

- [ ] **Step 2: Run** `-tc="preview loads*,a cached*,only the latest*,no audio, no preview,failed loads*,stop forgets*"` — Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: previewer`

---

### Task 7: BrowserPanel, wiring, settings — checkpoint

**Files:**
- Create: `src/app/browser_panel.h/.cpp`
- Modify: `src/app/main_view.h/.cpp`, `src/app/main.cpp`, `src/app/settings.h/.cpp`, `tests/main_view_test.cpp`, `tests/settings_test.cpp`
- Test: `tests/browser_panel_test.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces:
  - `class BrowserPanel : public Widget { public: BrowserPanel(BrowserModel&, Previewer&, PopupMenu&); Button& add_folder_button(); ToggleButton& preview_button(); TreeView& tree(); bool preview_on() const; void set_preview_on(bool); };` — body of the BROWSER panel (the tree fills it); the header hosts `+ Folder` (58 px) and `Preview` (52 px), like the LOG panel's Clear.
    - Selecting a **file** row with preview on → `previewer.preview(path)`. Selecting a folder does nothing.
    - `+ Folder` → `host()->pick_folder(...)` → `model.add_root(path)`.
    - Right press on a **root** row → popup `{"Remove"}` → `model.remove_root(root_index)`; on other rows nothing.
    - Preview switched off → `previewer.stop()`.
    - `model.on_change` → relayout/invalidate; the tree's selection is clamped after a removal.
  - `MainView(Controller&, const LiveState&, Log&, BrowserPanel&, PopupMenu&)` — the BROWSER panel's body is the given `BrowserPanel`; the popup is added last and laid out over the whole view.
  - `Settings` gains `std::vector<std::filesystem::path> browser_folders; bool preview = true;` — JSON `"browser": {"folders": [UTF-8 strings], "preview": bool}`; non-string entries skipped; saved via `u8string()`. While touching `settings.cpp`, replace its three `path.string()` calls in error messages with `u8string()` (deferred minor from step 2's review — same file, same rule as Global Constraints).
  - `main.cpp`: `SampleLoader` declared **after** `Window` (its `on_result` calls `window.wake()`, so it must die first), `SampleCache`, `Previewer` (`audio_ready` = device running), `BrowserModel` (roots from settings), `PopupMenu`, `BrowserPanel` (preview flag from settings). `before_frame`: `for (auto& r : loader.take_results()) previewer.on_result(r);` before `frame_update`. Exit: save `model.root_paths()` and `browser.preview_on()`.

- [ ] **Step 1: Write the failing tests**

```cpp
// browser_panel_test.cpp: temp folder with two fixture copies, FakeHost, RecordingSink, real loader
TEST_CASE("+ Folder adds the picked folder") { /* click + Folder → host.folder_request set; call it with the temp dir → 1 root row */ }
TEST_CASE("clicking a file previews it") { /* expand root, click file row, wait for result → one PreviewSample */ }
TEST_CASE("arrow keys preview each file they reach") { /* Down, Down → 2 previews requested; only the last plays (Focus 1) */ }
TEST_CASE("preview off stops and stays quiet") { /* toggle Preview off → StopPreview sent; click file → nothing */ }
TEST_CASE("right click a root offers Remove") { /* right press root row → popup open; click "Remove" → 0 rows */ }
// settings_test.cpp
TEST_CASE("settings keep non-ASCII browser folders") {                      // Review Focus 2
    // folders {u8"C:/Sample żółć", "D:/Splice"}, preview false → round trip equal; {"browser":{"folders":[1,"x"]}} → {"x"}
}
// main_view_test.cpp: the fixture builds a BrowserModel/Previewer/PopupMenu/BrowserPanel; existing tests stay green
```

- [ ] **Step 2: Run** `-tc="+ Folder*,clicking a file*,arrow keys preview*,preview off*,right click a root*,settings keep*"` — Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Screenshot** `build/debug/aloglas.exe --screenshot build/shots/browser.png` — Expected: BROWSER header with `+ Folder` and `Preview` (on), empty tree.
- [ ] **Step 6: Live check (author).** `build/debug/aloglas.exe`: `+ Folder` → pick the Splice folder → it appears; expand; click a kick → hear it immediately; arrow down through hats → each plays, the previous cut; Preview off → silence; right-click the root → Remove; add it again, close, reopen → the folder and Preview state are back. A folder with Polish letters in its name works.
- [ ] **Step 7: Commit** — `app: browser panel with preview; folders and preview saved in settings`

**Checkpoint:** stop and let the author try it.

---

## After this plan

Next plan: step 4 (snapshots, scheduler, sampler, channel rack — "click a kick and hear it"), which also adds drag & drop from the browser and Enter → channel rack.
