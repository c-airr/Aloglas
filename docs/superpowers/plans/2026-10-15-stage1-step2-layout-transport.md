# Stage 1, Step 2: Window Layout and Live Transport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the static palette mock with the real Aloglas window: resizable panels, a clickable transport that plays the metronome (LOOP/SONG, tempo, signature, brace, follow, position, CPU and master meter), a working log, a status bar, and settings that remember the window and panel sizes.

**Architecture:** Builds on the foundation (old plan Tasks 1–11). The `Document`/`Controller` pair from the superseded old Task 12 is revived with only the transport part of the document; sounds, notes, tracks and clips are added by their own steps. `MainView` lays out the panels with a pure `compute_layout()` function; the browser, playlist, channel rack and piano roll are empty panels with headers until steps 3–6 fill them.

**Tech Stack:** C++20, clang (llvm-mingw), CMake + Ninja, SDL 3.4.18, NanoVG, miniaudio 0.11.25, doctest 2.5.3, nlohmann/json 3.12.0 (new, Task 9).

**Spec:** `docs/superpowers/specs/2026-10-13-stage1-playlist-transport-design.md` (§4.2, §5.2, §6, §8, §9.1–9.4, §9.9, §13; build order step 2)

**Previous plan:** `docs/superpowers/plans/2026-10-14-stage1-playlist-transport.md`. Its Tasks 1–11 are done; its Tasks 12–14 are replaced by this plan, and 15–17 move to the playlist step (6).

## Global Constraints

- Everything in the previous plan's Global Constraints still applies (C++20, llvm-mingw clang, `-static`, pinned deps, naming, English code, *why*-comments for a C++ learner, `-Wall -Wextra` clean, time in `double` / audio in `float`, PPQ 960, real-time rules for `render()`).
- Commit trailers for this plan:
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
  `Claude-Session: https://claude.ai/code/session_01Dqyev4aw1Vk1thQ1YnFXZL`
- `git pull --rebase --autostash` before each commit. **Never push** (public repo, the author approves pushes).
- Commands (repo root, Git Bash): configure `tools/dev.sh cmake --preset debug`; build `tools/dev.sh cmake --build --preset debug`; test `tools/dev.sh ctest --preset debug`; one case `build/debug/aloglas_tests.exe -tc="<name>"`.
- New `.cpp` files are listed explicitly in `CMakeLists.txt` (no globbing): app sources go into a new static library `aloglas_app_lib` (created in Task 2, links `aloglas_ui aloglas_engine aloglas_audio aloglas_core`), linked by both `aloglas` and `aloglas_tests`; new tests are appended to `aloglas_tests`.
- Look: the dense style approved on 2026-10-15 (commit `8a59ccc`): square corners, 1-px `panel_border` outlines, controls in a group touch (`x += w - 1`), `theme().group_gap` between groups, metrics from `Theme`. Palette: Graphite (default; the author can still switch, `--palettes` keeps rendering all three).
- Defaults from the spec: tempo 140.00, 4/4, metronome on, play mode LOOP, follow on, brace off over bars 1–5 (ticks 0–15360), splitters browser 220 px, right column 280 px, rack/log 60 %/40 %, piano roll 40 % of the height. Log keeps 500 lines.

## Review Focus

1. **Window shrunk below the sum of panel minimums, or a splitter dragged past its neighbour.** Panels give way down to zero, no rect ever has negative size, and the stored sizes are clamped so the splitter doesn't "wind up" past the edge. → Task 5 `compute_layout survives tiny windows`, `clamp_sizes keeps neighbours at their minimum`; Task 8 `splitter drag clamps and double-click resets`.
2. **Play with no audio device, or the device lost mid-playback.** Play sends nothing, commands don't pile up, the transport shows stopped, the status bar and log say why. → Task 2 `play without audio sends nothing`; Task 6 `frame_update pumps engine without device`, `lost device stops and logs once`.
3. **The loop gets shorter under a playing playhead** (4/4 → 2/4 while the playhead is in beat 4, LOOP mode). The playhead wraps on the next sample instead of running on forever. → Task 1 `loop length shrinking under the playhead wraps`.
4. **LOOP ↔ SONG switched while playing.** Playback continues from the new mode's origin with the metronome in phase (the first click lands on the origin's beat). → Task 1 `play mode change while playing jumps to the new origin`.
5. **settings.json corrupt, partial, or saved on a monitor that is now unplugged.** Defaults are used with one log warning; missing fields keep their defaults; the window opens on a visible display. → Task 9 `load_settings: missing, corrupt, partial`; manual check in Task 9.

---

## File Map

```
src/engine/commands.h          + PlayMode, SetPlayMode, SetLoopLength
src/engine/engine.h/.cpp       + LOOP mode wrap, master peak
src/ui/widget.h                + Cursor, UiHost::set_cursor, Widget::on_press_within
src/ui/event_router.cpp        calls on_press_within on ancestors
src/ui/window.h/.cpp           cursors, WindowPlacement, initial position
src/ui/widgets/*.cpp           flat restyle (square, 1-px border)
src/ui/widgets/splitter.h/.cpp Splitter
src/ui/widgets/meter_bar.h/.cpp MeterBar (stereo peak meter)
src/app/document.h/.cpp        Document (transport part), loop_length_ticks
src/app/controller.h/.cpp      Controller
src/app/log.h/.cpp             Log model
src/app/log_panel.h/.cpp       LogPanel
src/app/main_layout.h/.cpp     PanelSizes, MainLayout, compute_layout, clamp_sizes
src/app/live_state.h           LiveState (engine/device view for one frame)
src/app/frame_update.h/.cpp    frame_update, status_text, position_text
src/app/status_bar.h/.cpp      StatusBar
src/app/transport_bar.h/.cpp   TransportBar
src/app/panel.h/.cpp           Panel (header + body + focus border)
src/app/main_view.h/.cpp       MainView
src/app/settings.h/.cpp        Settings, load_settings, save_settings
src/app/main.cpp               wiring (replaces the demo)
src/app/demo/                  deleted in Task 8
```

---

### Task 1: Engine — play modes, loop length, master peak

**Files:**
- Modify: `src/engine/commands.h`, `src/engine/engine.h`, `src/engine/engine.cpp`
- Test: `tests/engine_mode_test.cpp` (new, add to CMake)

**Interfaces:**
- Produces:
  - `enum class PlayMode : uint8_t { Loop, Song };` in `commands.h`
  - `CommandType::SetPlayMode` (flag = song), `CommandType::SetLoopLength` (a = ticks); `Command::set_play_mode(PlayMode)`, `Command::set_loop_length(int64_t ticks)`
  - `Engine::play_mode() const -> PlayMode` (published atomic)
  - `struct StereoPeak { float left = 0, right = 0; };` and `StereoPeak Engine::take_master_peak()` — highest absolute sample per channel since the last call; resets to 0.

Rules:
- **Engine default stays `PlayMode::Song`** (the existing loop/clock tests rely on it); the Document's default LOOP reaches the engine through `Controller::sync_engine()`.
- Default loop length: one 4/4 bar (3840). `SetLoopLength` ignores values `<= 0`.
- LOOP mode: origin is tick 0; brace and start marker are ignored. After advancing, if `pos_ >= loop_length_`: `pos_ = std::fmod(pos_, loop_length)` (keeps the overshoot, and also handles a length that shrank below the playhead), then `next_beat_ = first_beat_at_or_after(pos_, sig_)`.
- SONG mode: unchanged (brace + start marker, existing tests).
- `SetPlayMode`: if the mode changes, `pos_ = origin()` and `next_beat_` is recomputed — while playing too (playback continues from the new origin).
- Peak: after the sample loop in `render()`, compute the per-channel max of `|out|` for this buffer and store `max(stored, new)` (relaxed load + store; a peak lost to a race with `take` is harmless for a meter). `take_master_peak()` uses `exchange(0)`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("LOOP mode starts at 0 and wraps at the loop length") {
    // 120 BPM, 48 kHz → one beat = 24000 frames; set_play_mode(Loop), set_loop_length(3840), set_start(7680) (ignored in LOOP)
    // play; render 1 bar + 10 frames → position_ticks() < 3840; clicks at frames 0, 24000, 48000, 72000, 96000 (accent at 0 and 96000)
}
TEST_CASE("loop length shrinking under the playhead wraps") {          // Review Focus 3
    // LOOP, length 3840, play, render to tick ~3000 (beat 4); send set_loop_length(1920) → next render: position_ticks() < 1920
}
TEST_CASE("play mode change while playing jumps to the new origin") {   // Review Focus 4
    // SONG, start marker 7680, play, render 1000 frames; set_play_mode(Loop) → after render(1): position < 100,
    // and the next click is an accent on the first rendered frame after the switch
}
TEST_CASE("SONG mode ignores the loop length") { /* render 2 bars with length 3840 → position > 3840 */ }
TEST_CASE("master peak is taken and reset") {
    // play with metronome; render 2400 frames → take_master_peak().left > 0.1f; next take (no render) == 0
}
TEST_CASE("invalid loop length is ignored") { /* set_loop_length(0) and (-5) → still wraps at 3840 */ }
```

- [ ] **Step 2: Run** `build/debug/aloglas_tests.exe -tc="*LOOP*,*loop length*,*play mode*,*master peak*"` — Expected: compile errors / FAIL.
- [ ] **Step 3: Implement** in `commands.h`, `engine.h`, `engine.cpp` per the rules; publish `pub_play_mode_` with the other atomics.
- [ ] **Step 4: Run the full suite** `tools/dev.sh ctest --preset debug` — Expected: all pass, including the old engine tests and `render does not allocate`.
- [ ] **Step 5: Commit** — `engine: LOOP/SONG play modes, loop length, master peak`

---

### Task 2: Document and Controller (transport part)

**Files:**
- Create: `src/app/document.h/.cpp`, `src/app/controller.h/.cpp`, `tests/support/recording_sink.h`
- Test: `tests/controller_test.cpp`
- Modify: `CMakeLists.txt` (create `aloglas_app_lib`)

**Interfaces:**
- Consumes: `Command`, `CommandSink`, `PlayMode` (Task 1), `core/time.h`, `clamp_to_spec`.
- Produces:
  - `struct LoopBrace { bool enabled = false; int64_t start = 0; int64_t end = 4 * 3840; };`
  - `struct Document { double tempo = 140.0; TimeSignature signature{4, 4}; bool metronome = true; PlayMode play_mode = PlayMode::Loop; LoopBrace brace; int64_t start_marker = 0; bool follow = true; };`
  - `int64_t loop_length_ticks(const Document&)` → `ticks_per_bar(signature)`. Comment: `// shortcut: no notes yet, so Auto = one bar; the piano roll step adds the real Auto/1/2/4/8 rule`.
  - `class Controller { public: Controller(Document&, CommandSink&); const Document& doc() const; std::function<bool()> ensure_audio; std::function<void()> on_change; void sync_engine(); void play(); void stop(); void toggle_play(bool playing_now); void set_tempo(double); void set_signature(TimeSignature); void set_metronome(bool); void set_play_mode(PlayMode); void set_brace_range(int64_t start, int64_t end); void set_brace_enabled(bool); void toggle_brace(); void set_start_marker(int64_t tick, bool seek_playhead); void set_follow(bool); };`
  - `struct RecordingSink : CommandSink { std::vector<Command> sent; bool send(const Command&) override; };`

Rules (old Task 12, minus tracks, plus modes):
- Every mutation sends its command(s), then calls `on_change` if set. A call that changes nothing sends nothing and doesn't call `on_change`.
- `sync_engine` sends, in order: SetTempo, SetSignature, SetMetronome, SetLoop (brace), SetStart, SetPlayMode, SetLoopLength.
- `play()`: if `ensure_audio` is set and returns false, send nothing. `toggle_play(true)` → `stop()`, `toggle_play(false)` → `play()`.
- `set_tempo`: `clamp_to_spec({20, 999, 2})`.
- `set_signature`: ignore invalid. After storing: re-normalize the brace, clamp the start marker to `[0, max_ticks - 1]`, send SetSignature, SetLoop, SetStart, SetLoopLength.
- Brace normalization: swap reversed bounds; clamp to `[0, max_ticks]`; if shorter than one beat, `end = start + ticks_per_beat`, shifting both left if that passes `max_ticks`.
- `set_start_marker`: clamp; SetStart, then Seek if `seek_playhead`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("defaults and sync_engine") {
    Document d; RecordingSink s; Controller c(d, s);
    c.sync_engine();
    REQUIRE(s.sent.size() == 7);
    CHECK(s.sent[0].type == CommandType::SetTempo); CHECK(s.sent[0].number == 140.0);
    CHECK(s.sent[5].type == CommandType::SetPlayMode); CHECK(s.sent[5].flag == false);   // LOOP
    CHECK(s.sent[6].type == CommandType::SetLoopLength); CHECK(s.sent[6].a == 3840);
}
TEST_CASE("tempo clamps and rounds") { /* 10→20, 1500→999, 140.456→140.46 */ }
TEST_CASE("signature change revalidates brace, start marker and loop length") {
    // brace {0, 960} in 4/4 → set_signature({4,2}) → brace.end == 1920
    // start_marker 999 bars of 4/4 → set_signature({1,16}) → start_marker == max_ticks({1,16}) - 1
    // set_signature({3,4}) → last SetLoopLength.a == 2880
    // set_signature({4,3}) → ignored, nothing sent, on_change not called
}
TEST_CASE("brace normalization") { /* reversed, shorter than a beat, beyond max_ticks */ }
TEST_CASE("start marker seek") { /* seek=true → SetStart, Seek; false → SetStart only */ }
TEST_CASE("play without audio sends nothing") {                            // Review Focus 2
    // ensure_audio false → play() sends nothing; true → Play; toggle_play(true) → Stop
}
TEST_CASE("play mode") { /* set_play_mode(Song) → SetPlayMode flag true; same mode again → nothing sent */ }
TEST_CASE("on_change fires only for real changes") {}
```

- [ ] **Step 2: Run** `-tc="*sync_engine*,*tempo clamps*,*signature change*,*brace*,*start marker*,*without audio*,*play mode*,*on_change*"` — Expected: FAIL to compile.
- [ ] **Step 3: Implement** `document.*`, `controller.*`, `recording_sink.h`; add `aloglas_app_lib` to CMake.
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: document (transport part) and controller`

---

### Task 3: Log and LogPanel

**Files:**
- Create: `src/app/log.h/.cpp`, `src/app/log_panel.h/.cpp`
- Test: `tests/log_test.cpp`

**Interfaces:**
- Consumes: `Button`, `Canvas`, `theme()`.
- Produces:
  - `enum class LogLevel { Info, Warning, Error };`
  - `struct LogLine { std::string time; LogLevel level; std::string text; };`
  - `class Log { public: static constexpr std::size_t kMaxLines = 500; void add(LogLevel, std::string text); void add_at(std::string time, LogLevel, std::string text); const std::deque<LogLine>& lines() const; void clear(); uint64_t total_added() const; std::function<void()> on_change; };`
    `add` stamps local time `hh:mm:ss` (`std::chrono::system_clock` + `localtime_s`) and calls `add_at`. Over 500 lines, the oldest is dropped. `total_added` counts every add, ever (also after clear).
  - `class LogPanel : public Widget { public: explicit LogPanel(Log&); Button& clear_button(); float scroll_from_bottom() const; };`

LogPanel behavior:
- Rows of `theme().row_height`; columns: time (`text_dim`, 52 px), text (Info `text`, Warning `warning`, Error `error`). Newest at the bottom.
- `scroll_from_bottom` in lines: 0 = stuck to the newest line. Wheel up increases it (3 lines per notch), clamped to `[0, max(0, lines - visible_rows)]`.
- While `scroll_from_bottom > 0`, new lines increase it by the number added since the last draw/layout (use `total_added()`), so the visible lines stay put; clamped again after the 500-line trim.
- `clear_button()` is a `Button("Clear")` that the LOG panel header hosts (Task 8); click → `log.clear()`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("log keeps the last 500 lines") {
    Log log; for (int i = 0; i < 10'000; ++i) log.add_at("12:00:00", LogLevel::Info, std::to_string(i));
    CHECK(log.lines().size() == 500); CHECK(log.lines().front().text == "9500"); CHECK(log.total_added() == 10'000);
}
TEST_CASE("log panel sticks to the bottom until scrolled up") {
    // panel 100 px high (5 rows of 17 + rest); 50 lines; scroll_from_bottom()==0
    // wheel up 1 notch → 3; add 5 lines (+ layout) → 8, same first visible text as before
    // wheel down far → 0; add line → still 0
}
TEST_CASE("clear empties the log") {}
```

- [ ] **Step 2: Run** `-tc="*log*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: log model and log panel`

---

### Task 4: Toolkit — flat restyle, cursors, Splitter, press-within hook

**Files:**
- Modify: `src/ui/widget.h`, `src/ui/event_router.cpp`, `src/ui/window.h/.cpp`, `src/ui/widgets/button.cpp`, `toggle_button.cpp`, `number_box.cpp`, `text_field.cpp`, `tests/support/fake_host.h`
- Create: `src/ui/widgets/splitter.h/.cpp`, `src/ui/widgets/meter_bar.h/.cpp`
- Test: `tests/splitter_test.cpp`, additions in `tests/event_router_test.cpp`, `tests/widgets_test.cpp`

**Interfaces:**
- Produces:
  - `enum class Cursor { Arrow, ResizeH, ResizeV };` and `virtual void UiHost::set_cursor(Cursor) = 0;` — Window caches `SDL_CreateSystemCursor` for `SDL_SYSTEM_CURSOR_DEFAULT / EW_RESIZE / NS_RESIZE`; `FakeHost` records `Cursor cursor`.
  - `virtual void Widget::on_press_within(const MouseEvent&) {}` — `EventRouter::mouse_down` calls it on every ancestor of the hit widget (root first, hit widget's parent last) **before** the normal dispatch, whether or not anyone handles the press. Used for panel focus.
  - `enum class SplitAxis { Columns, Rows };` (Columns: a vertical divider that moves in x.)
  - `class Splitter : public Widget { public: Splitter(SplitAxis, std::function<float()> value, std::function<void(float)> set_value, float sign, float default_value); };` — press stores `value()`; move calls `set_value(start + sign * delta)` (delta in x or y); double-click (`clicks == 2`) calls `set_value(default_value)`; hover enter/leave sets the resize cursor / Arrow. Draws nothing (the 1-px gap between panels is the visible divider).
  - `class MeterBar : public Widget { public: MeterBar(std::function<float()> left, std::function<float()> right); };` — two horizontal bars (left on top), linear peak → dB, scale −60 … +6 dBFS; `meter_ok` up to 0 dBFS, `meter_hot` above. `float meter_fraction(float peak)` free function, tested.
- Restyle (no behavior change): `fill_rect(rect, color)` without radius plus `stroke_rect(rect, t.panel_border)` for Button, ToggleButton, NumberBox, TextField (active TextField keeps its accent outline). Text in `font_size_small` as now.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("on_press_within reaches ancestors even when a child handles the press") {
    // root > panel(counts calls) > button; click the button → panel count 1, button clicked
}
TEST_CASE("splitter drags with sign and resets on double-click") {
    // value 220, sign +1, Columns; drag x 100→160 → set_value(280); sign -1 → 160; double-click → default
    // hover → host.cursor == ResizeH; leave → Arrow
}
TEST_CASE("meter_fraction maps dB") {
    CHECK(meter_fraction(0.0f) == 0.0f); CHECK(meter_fraction(1.0f) == doctest::Approx(60.0f / 66.0f));
    CHECK(meter_fraction(4.0f) == 1.0f);  // +12 dB clamps
}
```

- [ ] **Step 2: Run** `-tc="*press_within*,*splitter*,*meter_fraction*"` — Expected: FAIL.
- [ ] **Step 3: Implement**; update `FakeHost`.
- [ ] **Step 4: Run** the full suite — Expected: PASS (existing widget tests unchanged).
- [ ] **Step 5: Commit** — `ui: flat widget style, cursors, splitter, meter bar, press-within hook`

---

### Task 5: Main layout math

**Files:**
- Create: `src/app/main_layout.h/.cpp`
- Test: `tests/main_layout_test.cpp`

**Interfaces:**
- Consumes: `Rect`, `cut_*`, `Theme`.
- Produces:
  - `struct PanelSizes { float browser_width = 220; float right_width = 280; float rack_share = 0.6f; float piano_share = 0.4f; bool operator==(const PanelSizes&) const = default; };`
  - `struct PanelMinimums { float browser = 140, playlist = 240, right = 180, rack = 60, log = 60, top = 120, piano = 120; };`
  - `struct MainLayout { Rect transport, status, browser, playlist, rack, log, piano; Rect split_browser, split_right, split_rack_log, split_top_piano; };`
  - `MainLayout compute_layout(Rect window, const PanelSizes&, const Theme&)` — transport on top (`transport_height`), status at the bottom (`status_height`); browser column on the left full height; the rest splits into top area / piano roll (`piano_share` of the rest's height); the top area into playlist / right column (`right_width`); the right column into rack / log (`rack_share`). Neighbouring panels are separated by a 1-px gap. Each `split_*` rect is `theme.splitter` px thick, centred on its gap.
  - `PanelSizes clamp_sizes(PanelSizes, Rect window, const Theme&, PanelMinimums = {})` — clamps each size so both neighbours keep their minimum when there is room; when there isn't, the size goes to the midpoint of what is available (never negative); shares stay in `[0, 1]`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("compute_layout at 1280x760 matches the spec defaults") {
    auto l = compute_layout({0, 0, 1280, 760}, {}, theme());
    CHECK(l.transport.h == 28); CHECK(l.status.h == 18); CHECK(l.browser.w == doctest::Approx(219));  // 220 minus the gap
    CHECK(l.rack.y == l.playlist.y); CHECK(l.piano.x == l.playlist.x); CHECK(l.piano.right() == 1280);
}
TEST_CASE("compute_layout survives tiny windows") {                          // Review Focus 1
    // for windows {0,0,0,0}, {0,0,50,30}, {0,0,960,600}: every rect has w >= 0 && h >= 0 and lies inside the window
}
TEST_CASE("clamp_sizes keeps neighbours at their minimum") {                 // Review Focus 1
    // 1280 wide: browser_width 5000 → 1280 - (240 + 180 + gaps); browser_width -50 → 140; rack_share 2 → <= 1
}
```

- [ ] **Step 2: Run** `-tc="*compute_layout*,*clamp_sizes*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: main window layout math`

---

### Task 6: LiveState, frame_update, StatusBar

**Files:**
- Create: `src/app/live_state.h`, `src/app/frame_update.h/.cpp`, `src/app/status_bar.h/.cpp`
- Test: `tests/frame_update_test.cpp`

**Interfaces:**
- Consumes: `Engine` (incl. `take_master_peak`, `play_mode`), `AudioDevice`, `Controller`, `Log`.
- Produces:
  - `struct LiveState { bool playing = false; double position_ticks = 0; float cpu_load = 0; bool audio_disabled = false; AudioStatus audio = AudioStatus::Closed; std::string audio_error; uint32_t sample_rate = 0, buffer_frames = 0; uint32_t dropped_commands = 0; float meter_left = 0, meter_right = 0; };`
    (Named `LiveState`, not `Snapshot`: spec §5.3 reserves `Snapshot` for the compiled document the engine plays.)
  - `void frame_update(Engine&, AudioDevice* device, Controller&, Log&, LiveState&)`:
    1. `device && device->take_lost()` → `device->close()`, `controller.stop()`, log Error `"Audio device lost"`.
    2. No device or not `Running` → `engine.pump()`.
    3. Fill the fields; meters: `meter = max(peak, meter * 0.85f)` per frame, below 1e-4 → 0 (`// shortcut: decay per frame, not per second; fine at 60 fps, revisit if frame pacing changes`).
    4. When `dropped_commands` grew since the last call, log Warning `"<n> engine commands dropped"` once per increase.
  - `bool needs_frames(const LiveState&)` → playing or any meter > 0.
  - `std::string status_text(const LiveState&)` → `"Audio: 48000 Hz, 480 samples"` / `"No audio — press Play to retry"` (NoDevice or Closed) / `"Audio error: <msg>"` / `"Audio disabled (--no-audio)"`, plus `" · <n> commands dropped"` when n > 0.
  - `std::string position_text(const LiveState&, const Document&)` → `format_bbs(to_bbs(pos, sig))` and `format_clock(ticks_to_seconds(pos, tempo))` separated by two spaces.
  - `class StatusBar : public Widget { public: explicit StatusBar(const LiveState&); };` — draws `status_text` left in `text_dim`, `font_size_small`, on `panel_header` with a 1-px top line.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("status_text") { /* four states and the dropped suffix */ }
TEST_CASE("position_text") { /* pos 3840+960 at 120 BPM 4/4 → "2.2.1  00:02.500" */ }
TEST_CASE("frame_update pumps engine without device") {                     // Review Focus 2
    // Engine e; e.set_assert_on_drop(false); 10 rounds of { 100x controller.set_tempo(100 + i); frame_update(e, nullptr, ...) }
    // → live.dropped_commands == 0, live.audio_disabled, log has no warnings
}
TEST_CASE("lost device stops and logs once") {                              // Review Focus 2
    // AudioDevice with null_backend, open, play, debug_stop_stream, wait > kStallMs, frame_update twice
    // → engine not playing after pump, exactly one Error line "Audio device lost"
}
TEST_CASE("meters decay to zero") { /* peak 1.0 then silence: after enough frames meter_left == 0 and needs_frames false */ }
```

- [ ] **Step 2: Run** `-tc="*status_text*,*position_text*,*frame_update*,*lost device*,*meters*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: live state, per-frame update, status bar`

---

### Task 7: TransportBar

**Files:**
- Create: `src/app/transport_bar.h/.cpp`
- Test: `tests/transport_bar_test.cpp`

**Interfaces:**
- Consumes: `Controller`, `LiveState`, `ToggleButton`, `Button`, `NumberBox`, `Label`, `MeterBar`, `position_text`.
- Produces: `class TransportBar : public Widget { public: TransportBar(Controller&, const LiveState&); };` with accessors for tests: `ToggleButton& play(); Button& stop(); NumberBox& tempo(); NumberBox& sig_num(); NumberBox& sig_den(); ToggleButton& loop_mode(); ToggleButton& song_mode(); ToggleButton& metronome(); ToggleButton& brace(); ToggleButton& follow();`

Layout, left to right, height `control_height` centred in `transport_height`, starting at x + 8; groups separated by `group_gap`, members of a group touch (`x += w - 1`):
1. play (28, icon ▶, value `live.playing`, toggle → `toggle_play(live.playing)`) | stop (28, icon ■ → `stop()`)
2. tempo `NumberBox{20, 999, 2, 0.5, 0.01}` (52) + `"BPM"` label (24, `text_dim`)
3. `sig_num` `NumberBox{1, 32, 0, 1.0/8, 1.0/8}` (22) | `"/"` (10) | `sig_den` `NumberBox{1, 16, 0, 1.0/8, 1.0/8, {1, 2, 4, 8, 16}}` (22) — each change calls `set_signature` with the other part from the document
4. `LOOP` (46) | `SONG` (46): value = mode equals; toggle → `set_play_mode(that mode)`
5. `Metro` (46) | `Brace` (46) | `Follow` (50)
6. position `Label(position_text)` in a `field_bg` box (150)
7. right-aligned: `"CPU n%"` label (48) and `MeterBar` (100) on `live.meter_left/right`

No `≡` menu yet: its actions (New/Open/Save/Export) arrive with the project-file and export steps.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("transport buttons drive the controller") {
    // FakeHost + router over a TransportBar laid out at {0,0,1280,28}; ensure_audio = true
    // click play → Play sent; live.playing = true; click play → Stop; click SONG → doc.play_mode == Song
    // click Metro → metronome false; click Brace → brace.enabled; click Follow → follow false
}
TEST_CASE("tempo drag and signature boxes") {
    // drag tempo 20 px up → doc.tempo == 150; drag sig_den up 8 px → signature {4, 8}
}
TEST_CASE("transport layout fits 960 px") { /* every child inside the bar, none overlapping the meter */ }
```

- [ ] **Step 2: Run** `-tc="*transport*"` — Expected: FAIL.
- [ ] **Step 3: Implement.** Play/stop icons: ▶ triangle 7 px wide, ■ 7 px square, centred.
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Commit** — `app: transport bar`

---

### Task 8: Panels, MainView, wiring — checkpoint

**Files:**
- Create: `src/app/panel.h/.cpp`, `src/app/main_view.h/.cpp`
- Modify: `src/app/main.cpp`, `CMakeLists.txt`
- Delete: `src/app/demo/palette_preview.h/.cpp`
- Test: `tests/main_view_test.cpp`

**Interfaces:**
- Consumes: everything above.
- Produces:
  - `class Panel : public Widget { public: Panel(std::string title, Panel*& focused_slot); void set_body(Widget&); void add_header_widget(Widget&, float width); Widget* body() const; };` — header of `panel_header_height` (title in `text_dim`, bold, `font_size_small`; header widgets right-aligned, `control_height - 2` tall), body below. `on_press_within` sets `focused_slot = this` and invalidates. When `focused_slot == this`, draw a 1-px `accent` outline over the panel after the children.
  - `class MainView : public Widget { public: MainView(Controller&, const LiveState&, Log&); PanelSizes& sizes(); Panel* focused_panel() const; bool on_key(const KeyEvent&) override; };`
    Children: `TransportBar`, panels BROWSER, PLAYLIST, CHANNEL RACK, LOG (body `LogPanel`, header `Clear`), PIANO ROLL, `StatusBar`, and the four `Splitter`s added **last** (on top). Empty panels have an empty `Widget` body. `layout` = `compute_layout` + `clamp_sizes`. Splitters: browser (Columns, +1), right column (Columns, −1), rack/log (Rows, share-based: `set_value` converts px to share of the column height), top/piano (Rows, −1, share of the area). Every `set_value` stores, then `sizes_ = clamp_sizes(...)`, then invalidates.
    Shortcuts: Space → `toggle_play(live.playing)`; Ctrl+L → `toggle_brace()`.
  - `main.cpp`: flags `--screenshot <png>`, `--palettes <dir>` (MainView rendered once per theme variant, `palette-N.png`), `--no-audio`. Creates `Engine`, `AudioDevice` (unless `--no-audio` or offscreen), `Document`, `Controller` (`ensure_audio = device running || device->open()` — logs `"Audio: <rate> Hz, <frames> samples"` on success, Error `"Audio error: …"` on failure; `on_change = window.invalidate`), `Log` (`on_change = window.invalidate`), `LiveState`, `Window`, `MainView`. Then `controller.sync_engine()`, `engine.pump()`, one `device->open()` attempt. `before_frame`: `frame_update(...)`, return `needs_frames(live)`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("MainView shortcuts") { /* Space → Play (ensure_audio true); Space with live.playing → Stop; Ctrl+L → brace on */ }
TEST_CASE("Space while typing a tempo edits the text") { /* double-click tempo, Space → no Play sent */ }
TEST_CASE("clicking inside a panel focuses it") { /* click in the log body → focused_panel() == log panel; click transport → unchanged */ }
TEST_CASE("splitter drag clamps and double-click resets") {                   // Review Focus 1
    // 1280x760: drag browser splitter +2000 px → playlist keeps >= 240; double-click → browser_width == 220
    // drag top/piano splitter up by 1000 px → top area keeps >= 120
}
TEST_CASE("MainView layout survives tiny sizes") { /* layout 0x0, 120x60, 1280x760: all widgets w,h >= 0 */ }
```

- [ ] **Step 2: Run** `-tc="*MainView*,*typing a tempo*,*panel focuses*,*splitter drag*"` — Expected: FAIL.
- [ ] **Step 3: Implement**; delete the demo and its CMake entries.
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Screenshot.** Run `build/debug/aloglas.exe --palettes build/shots`; view `build/shots/palette-1.png` with Read. Expected: transport `140.00 BPM 4 / 4 LOOP|SONG Metro|Brace|Follow 1.1.1  00:00.000`, five headed panels separated by 1-px lines, status bar text.
- [ ] **Step 6: Live check (author).** Run `build/debug/aloglas.exe`. Expected: Space plays the metronome at 140 BPM, accent on beat 1; in LOOP the position wraps every bar; SONG plays on from the start marker; dragging tempo while playing changes speed at once; splitters drag with resize cursors and double-click resets; the log shows the audio line; Space again stops and returns to `1.1.1`.
- [ ] **Step 7: Commit** — `app: main view with panels, live transport wiring; remove palette mock`

**Checkpoint:** stop here and ask the author to try it before Task 9.

---

### Task 9: Settings and window placement

**Files:**
- Modify: `cmake/deps.cmake` (nlohmann/json), `CMakeLists.txt`, `src/ui/window.h/.cpp`, `src/app/main.cpp`
- Create: `src/app/settings.h/.cpp`
- Test: `tests/settings_test.cpp`

**Interfaces:**
- Consumes: `PanelSizes`, `Log`.
- Produces:
  - deps: `FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp URL_HASH SHA256=<computed> DOWNLOAD_NO_EXTRACT TRUE)`; target `aloglas_json_headers` (INTERFACE include of the download dir). Compute the hash with `sha256sum` on the downloaded file once and pin it.
  - `struct WindowPlacement { int x = 0, y = 0, w = 1280, h = 760; bool maximized = true; };` in `ui/window.h`; `WindowOptions` gains `std::optional<WindowPlacement> placement` (when set: restored size/position, then maximize if `maximized`). If `SDL_GetDisplayForRect` finds no display for the rect, the position is dropped and SDL centres the window (Review Focus 5).
  - `WindowPlacement Window::placement() const` — the last **non-maximized, non-minimized** position and size (tracked on `SDL_EVENT_WINDOW_MOVED` / `RESIZED`), plus the current maximized flag.
  - `struct Settings { std::optional<WindowPlacement> window; PanelSizes panels; };`
  - `struct SettingsLoad { Settings settings; std::string warning; };`
  - `SettingsLoad load_settings(const std::filesystem::path&)` — missing file → defaults, no warning; invalid JSON or a wrong top-level type → defaults + warning `"Settings unreadable, using defaults: <reason>"`; a missing or mistyped field → that field's default (no warning).
  - `bool save_settings(const std::filesystem::path&, const Settings&, std::string* error)` — writes `<path>.tmp`, then `std::filesystem::rename` over the target; creates the directory.
  - `std::filesystem::path settings_path()` → `SDL_GetPrefPath(nullptr, "Aloglas")` + `settings.json` (verify it resolves to `%APPDATA%\Aloglas\`).
  - JSON shape: `{"window": {"x", "y", "w", "h", "maximized"}, "panels": {"browser_width", "right_width", "rack_share", "piano_share"}}`.
  - `main.cpp`: load before creating the window (warning → log), apply placement and `main_view.sizes()`; on exit save placement and sizes (failure → stderr via `SDL_Log`, the log panel is gone by then).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("settings round trip") { /* save to a temp dir, load → equal placement and panels, no warning */ }
TEST_CASE("load_settings: missing, corrupt, partial") {                     // Review Focus 5
    // missing → defaults, warning empty; "{not json" → defaults, warning non-empty;
    // {"panels": {"browser_width": 300}} → browser_width 300, right_width 280, window nullopt;
    // {"panels": {"browser_width": "wide"}} → 220
}
TEST_CASE("save replaces an existing file") {}
```

- [ ] **Step 2: Run** `-tc="*settings*"` — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite — Expected: PASS.
- [ ] **Step 5: Live check.** Resize/move the window, drag splitters, close, reopen → same layout. Maximized on close → opens maximized, and un-maximizing restores the previous size. Edit `settings.json` to `x: 10000` → the window opens centred on a visible screen.
- [ ] **Step 6: Commit** — `app: settings with window placement and panel sizes`

---

## After this plan

Next plan: step 3 (samples and browser with preview), written against the code this plan produces.
