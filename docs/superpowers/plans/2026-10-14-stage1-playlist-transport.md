# Stage 1: Playlist + Transport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Windows app that shows an Ableton-style playlist with empty named tracks, plays a sample-accurate metronome at the chosen tempo and time signature, loops a region, and moves a playhead in sync with what is heard.

**Architecture:** Five static libraries with one-way dependencies (`core` ← `engine` ← `audio`, `core` ← `ui`, all ← `app`). The engine renders per sample into a buffer and never touches a device, so tests render it offline. The UI is a small retained widget toolkit drawn with NanoVG on an SDL3 OpenGL 3.3 window. The UI thread talks to the audio thread only through an SPSC command queue and atomics.

**Tech Stack:** C++20, clang (llvm-mingw), CMake ≥ 3.25 + Ninja, SDL 3.4.18, NanoVG, glad2 (GL 3.3 core), miniaudio 0.11.25, doctest 2.5.3, Inter 4.1.

**Spec:** `docs/superpowers/specs/2026-10-13-stage1-playlist-transport-design.md`

## Global Constraints

- C++20. Compiler: clang/clang++ from llvm-mingw (`LLVM_MINGW` env var, default `%USERPROFILE%\llvm-mingw`). Never MSVC or clang-cl.
- CMake ≥ 3.25 and Ninja. On this machine they come from the Visual Studio bundle; `tools/dev.sh` puts them on PATH.
- Builds run locally (not `offload`).
- Executables link with `-static`. `objdump -p` may list only Windows system DLLs (KERNEL32, USER32, api-ms-win-crt-*, …), with no libc++/libunwind/winpthread DLLs.
- Dependencies are pinned exactly as in Task 1 (URL + SHA256). Nothing is installed system-wide.
- Namespace `aloglas`. Files `snake_case`, types `PascalCase`, functions and members `snake_case`, private members end with `_`.
- Code, comments, and commit messages are in English. The author is learning C++ and audio from this code, so comment *why* on anything non-obvious (atomics, memory order, real-time rules, NanoVG quirks). Don't narrate the obvious.
- Warnings `-Wall -Wextra` on project targets only (not on third-party code). No new warnings in project code.
- **Time in `double`, audio in `float`.** PPQ = 960. Document positions are `int64_t` ticks.
- `Engine::render()` and everything it calls: no allocation, no locks, no I/O, no exceptions.
- Every commit message ends with these two trailer lines:
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
  `Claude-Session: https://claude.ai/code/session_01QM8oCyp2AgdkEyS1cBE7GY`
- Run `git pull --rebase --autostash` before each commit. **Never push.** The repo (`github.com/c-airr/Aloglas`) is public, and the author approves pushes.
- Build/test commands (from the repo root, in Git Bash):
  - configure: `tools/dev.sh cmake --preset debug`
  - build: `tools/dev.sh cmake --build --preset debug`
  - test: `tools/dev.sh ctest --preset debug`
  - single test case: `build/debug/aloglas_tests.exe -tc="<name>"`

## Review Focus

1. **Ctrl+T (or any track-list rebuild) while a track name is being edited.** The edited header must survive: headers are reused by track id rather than rebuilt, so focus never points at a destroyed widget. → Task 16, test `rename survives adding a track`.
2. **A time-signature change that makes the loop shorter than one beat, or pushes markers past the new 1000-bar end.** The loop grows to one beat and the markers are clamped. → Task 12, test `signature change revalidates loop and start marker`.
3. **BPM typed as `140,5`, `abc`, `5000`, or empty.** A comma works as the decimal separator, out-of-range values are clamped, and garbage reverts without a change. → Task 9, test `parse_number accepts comma and clamps`; Task 11, test `NumberBox text entry`.
4. **Window shrunk to a sliver or minimized while playing.** Layout never produces negative sizes, and a minimized window doesn't spin the CPU. → Task 14, test `MainView layout survives tiny sizes`; Task 10, step on minimized waiting; Task 17 manual check.
5. **Play pressed with no audio device, or the device lost mid-playback.** The transport doesn't get stuck showing "playing", commands don't pile up in the queue, and the status bar explains what happened. → Task 12, test `play without audio sends nothing`; Task 14, test `frame_update pumps engine without device`.

---

## File Map

```
CMakeLists.txt                 targets, warnings, tests
CMakePresets.json              debug / release presets (build/<preset>)
cmake/llvm-mingw.cmake         toolchain file
cmake/deps.cmake               FetchContent: SDL3, nanovg, miniaudio, doctest
tools/dev.sh                   puts llvm-mingw + VS-bundled cmake/ninja on PATH, runs "$@"
third_party/glad/              generated GL 3.3 core loader (committed)
assets/fonts/                  Inter-Regular.ttf, Inter-SemiBold.ttf, OFL.txt
src/core/time.h/.cpp           ticks, signatures, conversions, formatting
src/core/utf8.h/.cpp           code-point helpers (track names, text field)
src/core/spsc_queue.h          lock-free single-producer/single-consumer ring
src/core/rt_guard.h/.cpp       audio-thread allocation guard (operator new override)
src/engine/commands.h          Command, CommandSink
src/engine/click.h/.cpp        ClickVoice (metronome sound)
src/engine/engine.h/.cpp       Engine: transport, clock, metronome, loop, render()
src/audio/audio_device.h/.cpp  AudioDevice (miniaudio wrapper)
src/audio/miniaudio_impl.c     MINIAUDIO_IMPLEMENTATION
src/ui/geometry.h              Point, Rect, cut_*/inset
src/ui/events.h                Key, Modifiers, MouseEvent, WheelEvent, KeyEvent, TextEvent
src/ui/widget.h/.cpp           UiHost, Widget, hit_test
src/ui/event_router.h/.cpp     mouse capture, hover, focus, bubbling
src/ui/text_edit.h/.cpp        text editing model (no drawing)
src/ui/number_format.h/.cpp    NumberSpec, drag/parse/format
src/ui/theme.h/.cpp            Color, Theme, theme()
src/ui/canvas.h/.cpp           Canvas over NanoVG
src/ui/fonts.h/.cpp            embedded Inter (#embed)
src/ui/window.h/.cpp           SDL window + GL + NanoVG, UiHost, main loop, render_to_png
src/ui/widgets/label.h/.cpp, button.h/.cpp, toggle_button.h/.cpp,
               text_field.h/.cpp, number_box.h/.cpp
src/app/document.h/.cpp        Document, Track, LoopRange, make_default_document
src/app/controller.h/.cpp      Controller (only mutator of Document)
src/app/snapshot.h             Snapshot (engine/device state for one frame)
src/app/frame_update.h/.cpp    per-frame glue: device loss, pump, snapshot
src/app/timeline_view.h/.cpp   TimelineView, GridSpacing, choose_grid, snap_tick
src/app/transport_bar.h/.cpp   TransportBar
src/app/status_bar.h/.cpp      StatusBar, status_text
src/app/ruler.h/.cpp           Ruler (loop strip + bar numbers)
src/app/lane_area.h/.cpp       LaneArea (lanes, grid, markers, playhead)
src/app/track_list.h/.cpp      TrackList, TrackHeader
src/app/main_view.h/.cpp       MainView (root widget, layout, shortcuts)
src/app/main.cpp               args, wiring, run
tests/test_main.cpp            doctest main
tests/support/*.h              RecordingSink, FakeHost, render helpers
tests/*_test.cpp               one file per unit
```

---

### Task 1: Build skeleton and pinned dependencies

**Files:**
- Create: `CMakeLists.txt`, `CMakePresets.json`, `cmake/llvm-mingw.cmake`, `cmake/deps.cmake`, `tools/dev.sh`, `.gitignore`, `.gitattributes`
- Create: `third_party/glad/` (generated), `assets/fonts/Inter-Regular.ttf`, `assets/fonts/Inter-SemiBold.ttf`, `assets/fonts/OFL.txt`
- Create: `src/app/main.cpp` (placeholder), `tests/test_main.cpp`, `tests/smoke_test.cpp`

**Interfaces:**
- Produces CMake targets: `aloglas_glad` (static, PUBLIC include `third_party/glad/include`), `aloglas_nanovg` (static, `nanovg.c`, PUBLIC include `${nanovg_SOURCE_DIR}/src`), `aloglas_miniaudio_headers` (INTERFACE include `${miniaudio_SOURCE_DIR}`), `aloglas_doctest` (INTERFACE include `${doctest_SOURCE_DIR}`), `SDL3::SDL3-static`, executable `aloglas`, executable `aloglas_tests` (registered with `add_test(NAME aloglas_tests COMMAND aloglas_tests)`), function `aloglas_project_target(<target>)` that adds `-Wall -Wextra` and C++20.
- Later tasks add their library with `add_library(aloglas_<lib> STATIC ...)` + `aloglas_project_target(...)`, link it into `aloglas_tests`, and append test files with `target_sources(aloglas_tests PRIVATE ...)`.

- [ ] **Step 1: Toolchain and presets**

`cmake/llvm-mingw.cmake`: `CMAKE_SYSTEM_NAME Windows`. Root = `$ENV{LLVM_MINGW}`, else derived from `find_program(clang)` on PATH, else `FATAL_ERROR` with a message that names `LLVM_MINGW`. Set `CMAKE_C_COMPILER` = `<root>/bin/clang.exe`, `CMAKE_CXX_COMPILER` = `clang++.exe`, `CMAKE_RC_COMPILER` = `llvm-windres.exe` (SDL builds a version resource), `CMAKE_AR` = `llvm-ar.exe`, `CMAKE_RANLIB` = `llvm-ranlib.exe`.

`CMakePresets.json` (version 6): hidden base with generator `Ninja`, `binaryDir` `${sourceDir}/build/${presetName}`, toolchain file `cmake/llvm-mingw.cmake`, `CMAKE_EXPORT_COMPILE_COMMANDS ON`. Configure presets `debug` (`CMAKE_BUILD_TYPE=Debug`) and `release` (`RelWithDebInfo`). Matching build presets. Test presets `debug`/`release` with `"output": {"outputOnFailure": true}`.

`tools/dev.sh` (bash, executable bit set via `git update-index --chmod=+x`): prepend `${LLVM_MINGW:-$USERPROFILE/llvm-mingw}/bin` to PATH. If `cmake` is not on PATH, locate Visual Studio with `vswhere.exe -latest -products * -property installationPath` and prepend `<vs>/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin` and `.../CMake/Ninja`. Then `exec "$@"`.

- [ ] **Step 2: Dependencies (`cmake/deps.cmake`)**

```cmake
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
  URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)
FetchContent_Declare(nanovg
  URL https://github.com/memononen/nanovg/archive/ce3bf745eb2d2dbc14a50bf2446783f691ac4353.tar.gz
  URL_HASH SHA256=655fe16920f1d2ec0895adf3a0eed1d3d0403cc4e2de247f3600d136f206a520)
FetchContent_Declare(miniaudio
  URL https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h
  URL_HASH SHA256=ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89
  DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_Declare(doctest
  URL https://raw.githubusercontent.com/doctest/doctest/v2.5.3/doctest/doctest.h
  URL_HASH SHA256=cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421
  DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(SDL3 nanovg miniaudio doctest)
```

nanovg has no CMakeLists, so `MakeAvailable` only downloads it and `CMakeLists.txt` defines `aloglas_nanovg` from `${nanovg_SOURCE_DIR}/src/nanovg.c`.

- [ ] **Step 3: Vendored files**

- glad: in a throwaway venv, `pip install glad2==2.0.8`, then `glad --api gl:core=3.3 --extensions "" --out-path third_party/glad c`. This yields `include/glad/gl.h`, `include/KHR/khrplatform.h`, `src/gl.c`. Commit the output.
- Fonts: from `https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip` (SHA256 `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`) copy `extras/ttf/Inter-Regular.ttf`, `extras/ttf/Inter-SemiBold.ttf`, and `LICENSE.txt` → `assets/fonts/OFL.txt`.
- `.gitignore`: `build/`. `.gitattributes`: `* text=auto`, `*.ttf binary`, `*.sh text eol=lf`.

- [ ] **Step 4: Smoke test + placeholder main**

`tests/test_main.cpp`: `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` + `#include <doctest.h>`.

```cpp
// tests/smoke_test.cpp
TEST_CASE("toolchain smoke") {
    CHECK(sizeof(void*) == 8);
    CHECK(__cplusplus >= 202002L);
}
```

`src/app/main.cpp`: include `<SDL3/SDL_main.h>`, `int main(int, char**)` that calls `SDL_Init(0)`, prints `SDL_GetVersion()` via `SDL_Log`, calls `SDL_Quit()`, and returns 0. `aloglas` links `SDL3::SDL3-static aloglas_glad aloglas_nanovg` with `-static` (`target_link_options`). `aloglas_tests` links `aloglas_doctest` with `-static`.

- [ ] **Step 5: Configure, build, run**

Run: `tools/dev.sh cmake --preset debug && tools/dev.sh cmake --build --preset debug && tools/dev.sh ctest --preset debug`
Expected: `100% tests passed`. `build/debug/aloglas.exe` prints `SDL 3.4.18`.
Run: `llvm-mingw/bin/x86_64-w64-mingw32-objdump -p build/debug/aloglas.exe | grep "DLL Name"`. Expected: system DLLs only.

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "Build skeleton: CMake presets, llvm-mingw toolchain, pinned deps"
```

---

### Task 2: Musical time (`core/time`) and the float lesson

**Files:**
- Create: `src/core/time.h`, `src/core/time.cpp`
- Test: `tests/time_test.cpp`, `tests/float_lesson_test.cpp`
- Modify: `CMakeLists.txt` (add `aloglas_core`)

**Interfaces:**
- Produces (`aloglas` namespace, `core/time.h`):
  - `inline constexpr int64_t kPpq = 960; inline constexpr int64_t kTicksPerSixteenth = 240; inline constexpr int kMaxBars = 1000;`
  - `struct TimeSignature { int num = 4; int den = 4; };` with `==`
  - `bool is_valid(TimeSignature)`: num 1–32, den ∈ {1,2,4,8,16}
  - `int64_t ticks_per_beat(TimeSignature)` = `kPpq * 4 / den`
  - `int64_t ticks_per_bar(TimeSignature)` = `num * ticks_per_beat`
  - `int64_t max_ticks(TimeSignature)` = `kMaxBars * ticks_per_bar`
  - `double ticks_per_sample(double bpm, double sample_rate)` = `bpm * kPpq / (60 * sample_rate)`
  - `double ticks_to_seconds(double ticks, double bpm)`
  - `int64_t first_beat_at_or_after(double tick, TimeSignature)` (smallest beat multiple ≥ tick)
  - `struct BarBeatSixteenth { int64_t bar; int beat; int sixteenth; };` (1-based)
  - `BarBeatSixteenth to_bbs(double ticks, TimeSignature)` (floor; the sixteenth counts 240-tick steps inside the beat)
  - `std::string format_bbs(const BarBeatSixteenth&)` → `"12.3.2"`
  - `std::string format_clock(double seconds)` → `"mm:ss.mmm"`, minutes not capped (`"125:00.000"`)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("ticks per beat/bar") {
    CHECK(ticks_per_beat({4,4}) == 960);  CHECK(ticks_per_bar({4,4}) == 3840);
    CHECK(ticks_per_beat({6,8}) == 480);  CHECK(ticks_per_bar({6,8}) == 2880);
    CHECK(ticks_per_beat({7,4}) == 960);  CHECK(ticks_per_bar({7,4}) == 6720);
    CHECK(max_ticks({4,4}) == 3'840'000);
    CHECK_FALSE(is_valid({4,3})); CHECK_FALSE(is_valid({0,4})); CHECK_FALSE(is_valid({33,4}));
}
TEST_CASE("ticks_per_sample") { CHECK(ticks_per_sample(120, 48000) == doctest::Approx(0.04)); }
TEST_CASE("to_bbs") {
    CHECK(format_bbs(to_bbs(0, {4,4})) == "1.1.1");
    CHECK(format_bbs(to_bbs(3840 + 960 + 240, {4,4})) == "2.2.2");
    CHECK(format_bbs(to_bbs(2880 + 480 * 5 + 240, {6,8})) == "2.6.2");
    CHECK(format_bbs(to_bbs(6720 - 1, {7,4})) == "1.7.4");
}
TEST_CASE("format_clock") {
    CHECK(format_clock(0) == "00:00.000");
    CHECK(format_clock(65.25) == "01:05.250");
    CHECK(format_clock(7500) == "125:00.000");
    CHECK(ticks_to_seconds(1920, 120) == doctest::Approx(1.0));
}
TEST_CASE("first_beat_at_or_after") {
    CHECK(first_beat_at_or_after(0, {4,4}) == 0);
    CHECK(first_beat_at_or_after(0.01, {4,4}) == 960);
    CHECK(first_beat_at_or_after(960, {4,4}) == 960);
    CHECK(first_beat_at_or_after(500, {6,8}) == 960);
}
```

`tests/float_lesson_test.cpp`: accumulate `ticks_per_sample(140, 48000)` once per sample into a `float` and a `double`, as in the spec's "Float lesson". The test must:

```cpp
TEST_CASE("float lesson: time must be double") {
    // ... loop until bar 1000 for double; track the first sample where float += step leaves it unchanged
    MESSAGE("float stalls at bar " << stall_bar);       // shown in test output: the lesson
    CHECK(stall_bar < 300);                              // float playhead stops (~bar 272)
    CHECK(std::abs(double_error_samples) < 1.0);         // double after 1000 bars
}
```

The test runs in a few hundred ms in debug. Stop the float loop once it stalls.

- [ ] **Step 2: Run the tests and see them fail** (they don't compile: no `core/time.h`)

Run: `tools/dev.sh cmake --build --preset debug`
Expected: compile error `'core/time.h' file not found`.

- [ ] **Step 3: Implement `core/time.h/.cpp`; add `aloglas_core` (PUBLIC include `src/`) and link it into `aloglas_tests`**

- [ ] **Step 4: Run the tests**

Run: `tools/dev.sh cmake --build --preset debug && tools/dev.sh ctest --preset debug`
Expected: PASS. `build/debug/aloglas_tests.exe -tc="float lesson*" -s` prints the stall bar (≈272).

- [ ] **Step 5: Commit** — `"core: musical time math and the float-vs-double lesson"`

---

### Task 3: SPSC queue

**Files:**
- Create: `src/core/spsc_queue.h`
- Test: `tests/spsc_queue_test.cpp`

**Interfaces:**
- Produces: `template <typename T, std::size_t Capacity> class SpscQueue` (Capacity is a power of two, enforced by `static_assert`) with `bool push(const T&)` (producer thread only; returns false when it already holds `Capacity` items) and `bool pop(T& out)` (consumer thread only; returns false when empty). `T` must be trivially copyable (`static_assert`).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("SpscQueue holds exactly Capacity items, FIFO") {
    SpscQueue<int, 4> q;
    for (int i = 0; i < 4; ++i) CHECK(q.push(i));
    CHECK_FALSE(q.push(99));
    int v;
    for (int i = 0; i < 4; ++i) { REQUIRE(q.pop(v)); CHECK(v == i); }
    CHECK_FALSE(q.pop(v));
    CHECK(q.push(5));           // wraps around
}
TEST_CASE("SpscQueue across threads keeps order") {
    // producer thread pushes 0..999'999 (spinning on full); consumer pops and checks v == expected++
}
```

- [ ] **Step 2: Run and see it fail** (missing header)
- [ ] **Step 3: Implement.** Use two monotonically increasing `std::atomic<std::size_t>` indices (`head_` written by the producer, `tail_` by the consumer), each `alignas(64)` to avoid false sharing. `push` stores with `memory_order_release` and loads the other index with `acquire`; `pop` mirrors that. Index the slot with `i & (Capacity - 1)`. Explain in comments why release/acquire is enough here.
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"core: lock-free SPSC queue"`

---

### Task 4: Real-time allocation guard

**Files:**
- Create: `src/core/rt_guard.h`, `src/core/rt_guard.cpp`
- Test: `tests/rt_guard_test.cpp`
- Modify: `CMakeLists.txt` (option `ALOGLAS_RT_GUARD`, default ON when `CMAKE_BUILD_TYPE` is `Debug`, applied as a PRIVATE definition on `aloglas_core`)

**Interfaces:**
- Produces (`aloglas::rt`):
  - `class AudioThreadScope` (RAII; marks the current thread as "audio" for its lifetime; nestable, restores the previous value; non-copyable)
  - `bool in_audio_thread()`, `bool guard_enabled()`, `uint64_t violation_count()`, `void set_abort_on_violation(bool)` (default true)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("rt guard counts allocations inside an audio scope") {
    if (!rt::guard_enabled()) { MESSAGE("guard disabled in this build"); return; }
    rt::set_abort_on_violation(false);
    auto before = rt::violation_count();
    { rt::AudioThreadScope scope; auto* p = new int(1); delete p; }
    CHECK(rt::violation_count() == before + 1);
    { auto* p = new int(2); delete p; }               // outside the scope: not counted
    CHECK(rt::violation_count() == before + 1);
    rt::set_abort_on_violation(true);
}
TEST_CASE("AudioThreadScope nests") {
    CHECK_FALSE(rt::in_audio_thread());
    { rt::AudioThreadScope a; { rt::AudioThreadScope b; } CHECK(rt::in_audio_thread()); }
    CHECK_FALSE(rt::in_audio_thread());
}
```

- [ ] **Step 2: Run and see it fail**
- [ ] **Step 3: Implement.** A `thread_local bool` holds the flag. When `ALOGLAS_RT_GUARD` is defined, `rt_guard.cpp` replaces the global `operator new(size_t)`, `operator new(size_t, std::align_val_t)` and the matching `operator delete` overloads (sized and unsized), using `malloc`/`free` and `_aligned_malloc`/`_aligned_free`. On a violation it increments an atomic counter. If abort is enabled, it writes `"aloglas: allocation on the audio thread\n"` with `fputs(stderr)` and calls `abort()`. Comment that the override lives in the same `.cpp` as `in_audio_thread()` on purpose: a static library only links object files that something references, and the engine references this one.
- [ ] **Step 4: Run tests** — Expected: PASS in `debug`. In `release` the first test prints the message and returns.
- [ ] **Step 5: Commit** — `"core: audio-thread allocation guard"`

---

### Task 5: Engine — transport, clock, metronome

**Files:**
- Create: `src/engine/commands.h`, `src/engine/click.h`, `src/engine/click.cpp`, `src/engine/engine.h`, `src/engine/engine.cpp`
- Create: `tests/support/render.h`
- Test: `tests/engine_clock_test.cpp`
- Modify: `CMakeLists.txt` (add `aloglas_engine` → `aloglas_core`)

**Interfaces:**
- Consumes: `core/time.h`, `SpscQueue`, `rt::AudioThreadScope`.
- Produces:
  - `enum class CommandType : uint8_t { Play, Stop, Seek, SetTempo, SetSignature, SetMetronome, SetLoop, SetStart };`
  - `struct Command { CommandType type; double number = 0; int64_t a = 0, b = 0; bool flag = false; static Command play(); static Command stop(); static Command seek(int64_t tick); static Command set_tempo(double bpm); static Command set_signature(TimeSignature); static Command set_metronome(bool on); static Command set_loop(bool enabled, int64_t start, int64_t end); static Command set_start(int64_t tick); };` (Seek/SetStart: `a`=tick. SetLoop: `flag`=enabled, `a`=start, `b`=end. SetSignature: `a`=num, `b`=den. SetTempo: `number`. SetMetronome: `flag`.)
  - `class CommandSink { public: virtual ~CommandSink() = default; virtual bool send(const Command&) = 0; };`
  - `class ClickVoice { public: void prepare(double sample_rate); void trigger(bool accent); void stop(); float next(); bool active() const; };`
  - `struct ClickEvent { int64_t frame; bool accent; };`
  - `class Engine final : public CommandSink` with: `void prepare(double sample_rate)`; `bool send(const Command&) override` (UI thread; on a full queue it returns false, increments the dropped counter, and asserts in debug); `void render(float* out, uint32_t frames)` (audio thread, interleaved stereo, `out` may be null only when `frames == 0`); `void pump()` (= `render(nullptr, 0)`, only valid while no audio thread is running); atomically published `bool playing() const`, `double position_ticks() const`, `uint32_t dropped_commands() const`, `int64_t frames_rendered() const`; click log `std::size_t click_count() const`, `ClickEvent click(std::size_t i) const`, `void clear_click_log()` (fixed `std::array<ClickEvent, 4096>`, oldest entries overwritten, read only while no render is running).
  - Engine defaults before any command: 120 BPM, 4/4, metronome on, loop disabled with range [0, 0), start marker 0, sample rate 48000.
  - `tests/support/render.h`: `std::vector<float> render_frames(Engine&, int64_t frames, uint32_t block = 512)` returns the interleaved output; `std::vector<int64_t> click_frames(const Engine&)` and `std::vector<bool> click_accents(const Engine&)`.

- [ ] **Step 1: Write the failing tests** (`engine_clock_test.cpp`; each test calls `prepare(48000)` and sends its settings first)

```cpp
TEST_CASE("clicks land on exact samples at 120 BPM") {
    Engine e; e.prepare(48000); e.send(Command::set_tempo(120)); e.send(Command::play());
    render_frames(e, 100'000);
    auto f = click_frames(e);
    REQUIRE(f.size() == 5);                       // 0, 24000, 48000, 72000, 96000
    for (size_t i = 0; i < f.size(); ++i) CHECK(std::llabs(f[i] - int64_t(i) * 24000) <= 1);
}
TEST_CASE("no drift after 1000 beats at 133 BPM") {
    // render 1000 beats in 4096-frame blocks; ideal frame of click k = k * 60 * 48000 / 133
    CHECK(std::abs(f[999] - 999 * 60.0 * 48000 / 133) <= 1.0);
}
TEST_CASE("block size does not change the output") {
    // 140 BPM, 10 s, blocks 64 / 480 / 1024 → identical click_frames and identical sample vectors
}
TEST_CASE("tempo change mid-play") {
    // 120 BPM, render 48000 frames (clicks 0, 24000), send set_tempo(240), render 48000
    CHECK(std::llabs(f[2] - 48000) <= 1);
    CHECK(std::llabs((f[3] - f[2]) - 12000) <= 1);
}
TEST_CASE("signatures: beat spacing and accents") {
    // 6/8 at 120 BPM: spacing 12000 frames; accents at clicks 0 and 6 only (first 7 clicks)
    // 7/4 at 120 BPM: spacing 24000; accents at 0 and 7
    // 4/4: accents at 0, 4, 8
}
TEST_CASE("click sound: stereo, short, starts on its frame") {
    // 120 BPM, render 30000 frames: L == R for every frame;
    // some |sample| > 0.1 within frames [0, 240); all samples exactly 0 in [2400, 24000)  (click < 50 ms)
}
TEST_CASE("stop silences and resets") {
    // play 10000 frames, send stop, render 1000: all zero, playing() false, position_ticks() == 0
}
TEST_CASE("metronome off renders silence but the playhead moves") {}
TEST_CASE("render does not allocate") {
    // guard enabled only: set_abort_on_violation(false); send one of every command type; render 4096; violation_count unchanged
}
TEST_CASE("full queue drops and counts") {
    // without rendering, send 300 set_tempo commands: 256 accepted, dropped_commands() == 44
    // (this test must disable the debug assert: Engine has `void set_assert_on_drop(bool)` for tests)
}
```

Add `void set_assert_on_drop(bool)` to `Engine`'s interface (default true; tests turn it off).

- [ ] **Step 2: Run and see it fail** (missing headers)

- [ ] **Step 3: Implement `ClickVoice`.** Sine burst. `accent` → 1500 Hz, else 1000 Hz. Start amplitude 0.5f (−6 dBFS), phase 0, exponential decay with time constant 5 ms (`decay = exp(-1 / (0.005 * sample_rate))`, computed in `prepare`). The voice goes inactive when the amplitude falls below 1e-4. `trigger` restarts the phase and amplitude.

- [ ] **Step 4: Implement `Engine`.** Per-sample loop. Block-size independence (the test in Step 1) depends on advancing exactly once per sample:

```
render(out, frames):
  rt::AudioThreadScope scope
  while queue.pop(cmd): apply(cmd)
  for i in [0, frames):
    if playing_:
      if pos_ >= next_beat_:
        click_.trigger(next_beat_ % ticks_per_bar(sig_) == 0)    // only if metronome_
        log {frame_, accent}; next_beat_ += ticks_per_beat(sig_)
      pos_ += tps_
      (Task 6 inserts the loop wrap here)
    s = click_.next(); out[2i] = out[2i+1] = s
    ++frame_
  publish playing_, pos_, frame_ (memory_order_relaxed is enough; explain why)
```

`apply`:
- `Play`: if not already playing, `pos_ = origin()`, `next_beat_ = first_beat_at_or_after(pos_)`, `playing_ = true`
- `Stop`: `playing_ = false`, `click_.stop()`, `pos_ = origin()`
- `Seek`: `pos_ = tick`, `next_beat_ = first_beat_at_or_after(pos_)`
- `SetTempo`: update `tps_`
- `SetSignature`: store it; `next_beat_ = first_beat_at_or_after(pos_)`
- `SetMetronome`: store it
- `SetLoop` / `SetStart`: store them; if stopped, `pos_ = origin()`

In this task `origin()` returns the start marker (Task 6 extends it). `prepare` sets the rate and recomputes `tps_` and the click decay. `static_assert(std::atomic<double>::is_always_lock_free)`. Queue capacity 256.

- [ ] **Step 5: Run tests** — Expected: PASS (debug and release)
- [ ] **Step 6: Commit** — `"engine: sample-accurate clock and metronome"`

---

### Task 6: Engine — loop and FL-style play origin

**Files:**
- Modify: `src/engine/engine.cpp`
- Test: `tests/engine_loop_test.cpp`

**Interfaces:**
- Consumes: `Engine` from Task 5. No new public API.

- [ ] **Step 1: Write the failing tests** (120 BPM, 4/4, 48 kHz; bar 2 starts at tick 3840; 3840 ticks = 96000 frames)

```cpp
TEST_CASE("loop jumps back on the exact sample and clicks the accent") {
    // set_loop(true, 3840, 7680), play, render 200'000
    // play starts at loop start: clicks at 0(accent), 24000, 48000, 72000, 96000(accent), 120000 ... (±1)
    CHECK(e.position_ticks() >= 3840); CHECK(e.position_ticks() < 7680);
}
TEST_CASE("play origin: loop start when enabled, start marker otherwise") {
    // set_start(9600), set_loop(false, 3840, 7680), play → position right after play ≈ 9600
    // stop; set_loop(true, 3840, 7680); play → ≈ 3840
}
TEST_CASE("stop returns to the origin evaluated at stop time") {
    // loop disabled, start 0, play, render 50'000, set_start(7680), render 1000, stop → position == 7680
}
TEST_CASE("while stopped the playhead follows the origin") {
    // set_start(1920); pump(); position == 1920; set_loop(true, 3840, 7680); pump(); position == 3840
}
TEST_CASE("playhead past the loop end does not loop") {
    // set_loop(true, 3840, 7680), play, seek(11520), render 48'000 → position > 11520
}
TEST_CASE("loop end crossed inside a tiny block") {
    // same as the first test but block = 1 frame: identical click frames
}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement.** `origin()` returns `loop.start` when the loop is enabled and its range is valid (`end > start`), else the start marker. After `pos_ += tps_`, if the loop is enabled and `prev < loop.end && pos_ >= loop.end`, set `pos_ = loop.start + (pos_ - loop.end)` (keeps the overshoot) and `next_beat_ = first_beat_at_or_after(loop.start)`. That way a beat sitting on the loop start fires on the very next sample.
- [ ] **Step 4: Run tests** — Expected: PASS (both test files)
- [ ] **Step 5: Commit** — `"engine: sample-accurate loop and FL-style play origin"`

---

### Task 7: Audio device

**Files:**
- Create: `src/audio/audio_device.h`, `src/audio/audio_device.cpp`, `src/audio/miniaudio_impl.c`
- Test: `tests/audio_device_test.cpp`
- Modify: `CMakeLists.txt` (add `aloglas_audio` → `aloglas_engine`, `aloglas_miniaudio_headers`; warnings off for `miniaudio_impl.c`)

**Interfaces:**
- Consumes: `Engine::prepare`, `Engine::render`.
- Produces:
  - `enum class AudioStatus { Closed, Running, NoDevice, Error };`
  - `struct AudioDeviceOptions { bool null_backend = false; };`
  - `class AudioDevice { public: explicit AudioDevice(Engine&, AudioDeviceOptions = {}); ~AudioDevice(); bool open(); void close(); AudioStatus status() const; const std::string& error() const; uint32_t sample_rate() const; uint32_t buffer_frames() const; float cpu_load() const; bool take_lost(); };`
  - `open()` (UI thread) is idempotent while running. Failure sets `NoDevice` (`MA_NO_DEVICE` / `MA_DEVICE_NOT_INITIALIZED` from a missing default device) or `Error` with `ma_result_description` text and returns false. `close()` sets `Closed`. `take_lost()` returns true once after a stop the app didn't request.

- [ ] **Step 1: Write the failing tests** (null backend: miniaudio simulates a device in real time)

```cpp
TEST_CASE("null device runs the engine") {
    Engine e; AudioDevice d(e, {.null_backend = true});
    REQUIRE(d.open());
    CHECK(d.status() == AudioStatus::Running);
    CHECK(d.sample_rate() > 0); CHECK(d.buffer_frames() > 0);
    CHECK(d.open());                                   // idempotent
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(e.frames_rendered() > 0);
    CHECK(d.cpu_load() >= 0.0f);
    d.close();
    CHECK(d.status() == AudioStatus::Closed);
    CHECK_FALSE(d.take_lost());                        // a requested stop is not "lost"
}
TEST_CASE("engine settings survive close/reopen") {
    // send set_tempo(90) while running; close; e.pump(); reopen → position keeps advancing; no dropped commands
}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement.** Playback config: `ma_format_f32`, 2 channels, `sampleRate = 0` (native), shared mode, `dataCallback` → `engine.render`. With `null_backend`, initialize an `ma_context` restricted to `ma_backend_null`. After init, `engine.prepare(device.sampleRate)` runs **before** `ma_device_start`. The CPU load measures `std::chrono::steady_clock` time around `render`, divides by `frames / rate`, smooths it (`load += 0.1f * (sample - load)`), and stores it in an atomic. A `notificationCallback` on `ma_device_notification_type_stopped` sets `lost_` unless `closing_` is set. Comment why the engine's `prepare` must happen before start (no audio thread exists yet, so nothing races).
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"audio: miniaudio device wrapper with CPU meter and loss detection"`

---

### Task 8: UI core — widgets and event routing

**Files:**
- Create: `src/ui/geometry.h`, `src/ui/events.h`, `src/ui/widget.h`, `src/ui/widget.cpp`, `src/ui/event_router.h`, `src/ui/event_router.cpp`
- Create: `tests/support/fake_host.h`
- Test: `tests/geometry_test.cpp`, `tests/event_router_test.cpp`
- Modify: `CMakeLists.txt` (add `aloglas_ui` → `aloglas_core`; SDL/NanoVG are linked in Task 10)

**Interfaces:**
- Produces:
  - `struct Point { float x = 0, y = 0; }; struct Rect { float x = 0, y = 0, w = 0, h = 0; float right() const; float bottom() const; bool contains(Point) const; bool empty() const; };`
  - `Rect cut_left(Rect& r, float a)`, `cut_right`, `cut_top`, `cut_bottom` (they remove a strip from `r` and return it, clamping `a` to `[0, available]`); `Rect inset(Rect, float d)` (size never negative)
  - `enum class Key { Unknown, Space, Enter, Escape, Backspace, Delete, Left, Right, Up, Down, Home, End, Tab, A, C, L, T, V, X };`
  - `struct Modifiers { bool shift = false, ctrl = false, alt = false; };`
  - `enum class MouseButton { Left, Right, Middle };`
  - `struct MouseEvent { Point pos; MouseButton button = MouseButton::Left; int clicks = 1; Modifiers mods; };`
  - `struct WheelEvent { Point pos; float dx = 0, dy = 0; Modifiers mods; };` (dy > 0 = wheel away from the user)
  - `struct KeyEvent { Key key; Modifiers mods; bool repeat = false; }; struct TextEvent { std::string_view utf8; };`
  - `class Canvas;` (forward declaration; defined in Task 10)
  - `class UiHost { public: virtual ~UiHost() = default; virtual void invalidate() = 0; virtual void set_focus(Widget*) = 0; virtual Widget* focus() const = 0; virtual void forget(Widget& subtree) = 0; virtual std::string clipboard_text() = 0; virtual void set_clipboard_text(std::string_view) = 0; virtual void set_text_input(bool on) = 0; };`
  - `class Widget`: non-copyable; public `Rect rect; bool visible = true; bool enabled = true;`; `Widget* parent() const; const std::vector<Widget*>& children() const; void add(Widget&); void remove(Widget&)` (calls `host()->forget(child)` first when a host exists); `void set_host(UiHost*)` (root only); `UiHost* host() const` (walks to the root); `void invalidate(); void focus(); bool has_focus() const;` virtuals `layout(Rect)` (default `rect = r`), `draw(Canvas&)` (default `draw_children`), `void draw_children(Canvas&)`, `bool focusable() const` (false), `bool on_mouse_down(const MouseEvent&)` (false), `void on_mouse_up(const MouseEvent&)`, `void on_mouse_move(const MouseEvent&)`, `bool on_wheel(const WheelEvent&)` (false), `bool on_key(const KeyEvent&)` (false), `bool on_text(const TextEvent&)` (false), `on_hover_enter()`, `on_hover_leave()`, `on_focus_gained()`, `on_focus_lost()`.
  - `Widget* hit_test(Widget& root, Point)`: deepest visible+enabled widget containing the point; later children win over earlier siblings. `bool contains_widget(const Widget& ancestor, const Widget& w)` (true if `w` is `ancestor` or one of its descendants).
  - `class EventRouter { public: explicit EventRouter(Widget& root); void mouse_down(const MouseEvent&); void mouse_up(const MouseEvent&); void mouse_move(const MouseEvent&); void mouse_left_window(); bool wheel(const WheelEvent&); bool key(const KeyEvent&); bool text(const TextEvent&); Widget* focus() const; void set_focus(Widget*); Widget* captured() const; Widget* hovered() const; void forget(Widget& subtree); };`
  - `tests/support/fake_host.h`: `struct FakeHost : UiHost` wraps an `EventRouter&`, counts `invalidations`, stores `clipboard`, records `text_input`. Helpers `click(router, Point, Modifiers = {}, int clicks = 1)` and `drag(router, Point from, Point to, Modifiers = {})` (down, one move, up).

Routing rules (spec §7.2 plus these decisions):
1. `mouse_down`: the target is `hit_test`. If a widget has focus and the target is outside it (`!contains_widget(*focus, *target)`), `set_focus(nullptr)` runs first. Then `on_mouse_down` is called from the target up through its ancestors until one returns true. That widget takes the capture.
2. `mouse_move`: with a capture, deliver it to the captured widget. Otherwise update hover (`on_hover_leave` on the old widget, `on_hover_enter` on the new) and deliver `on_mouse_move` to the hovered widget.
3. `mouse_up`: deliver to the captured widget, release the capture, then update hover.
4. `wheel`: hit target, bubbling up until a widget returns true.
5. `key`: start at the focused widget (or the root if nothing has focus), bubbling up to the root. `text`: focused widget only.
6. `set_focus`: ignores non-focusable widgets. Calls `on_focus_lost` on the old widget and `on_focus_gained` on the new one.
7. `forget(subtree)`: clears focus, capture, and hover pointers that point into `subtree`, without calling any callbacks.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("cut and inset never go negative") {
    Rect r{0, 0, 100, 20};
    CHECK(cut_left(r, 30).w == 30); CHECK(r.x == 30); CHECK(r.w == 70);
    CHECK(cut_left(r, 500).w == 70); CHECK(r.w == 0);
    CHECK(inset({0, 0, 4, 4}, 10).w == 0);
}
TEST_CASE("hit_test picks the deepest, topmost visible widget") {}
TEST_CASE("mouse_down bubbles until handled and captures") {
    // child returns false, parent returns true → parent gets moves outside its rect and the up
}
TEST_CASE("clicking outside the focused widget removes focus before delivery") {}
TEST_CASE("hover enter/leave") {}
TEST_CASE("keys go to focus, bubble to root, root when no focus") {}
TEST_CASE("text goes only to the focused widget") {}
TEST_CASE("forget clears pointers into a removed subtree without callbacks") {
    // focus + capture inside child; parent.remove(child) via FakeHost → focus()/captured() null, on_focus_lost not called
}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"ui: retained widget tree and event routing"`

---

### Task 9: Text editing and number formatting (pure logic)

**Files:**
- Create: `src/core/utf8.h`, `src/core/utf8.cpp`, `src/ui/text_edit.h`, `src/ui/text_edit.cpp`, `src/ui/number_format.h`, `src/ui/number_format.cpp`
- Test: `tests/utf8_test.cpp`, `tests/text_edit_test.cpp`, `tests/number_format_test.cpp`

**Interfaces:**
- Produces:
  - `core/utf8.h`: `std::size_t utf8_length(std::string_view)` (code points); `std::size_t utf8_next(std::string_view, std::size_t byte)`; `std::size_t utf8_prev(std::string_view, std::size_t byte)`; `std::string utf8_truncate(std::string_view, std::size_t max_code_points)`; `std::string trim(std::string_view)` (ASCII whitespace)
  - `class TextEdit { public: explicit TextEdit(std::size_t max_chars = 64); const std::string& text() const; std::size_t caret() const; std::size_t anchor() const; bool has_selection() const; std::string selected_text() const; void set_text(std::string_view); void insert(std::string_view utf8); void backspace(); void del(); void move_left(bool select); void move_right(bool select); void home(bool select); void end(bool select); void select_all(); void set_caret(std::size_t byte, bool select); };` Caret and anchor are byte offsets on code-point boundaries. `insert` replaces the selection, and `max_chars` truncates whatever doesn't fit. `set_text` puts the caret at the end with no selection.
  - `struct NumberSpec { double min = 0, max = 1; int decimals = 0; double drag_per_px = 1; double fine_drag_per_px = 0.1; std::vector<double> allowed; };`
  - `double clamp_to_spec(const NumberSpec&, double)`: clamps, rounds to `decimals`, and with `allowed` picks the nearest allowed value
  - `double drag_value(const NumberSpec&, double start, float dy_px, bool fine)`: dy > 0 = mouse moved up = larger. With `allowed`, steps through the list by index (`round(dy * drag_per_px)` steps).
  - `std::optional<double> parse_number(const NumberSpec&, std::string_view)`: trims, accepts `,` as the decimal separator, returns nullopt for empty or non-numeric text, clamps via `clamp_to_spec`
  - `std::string format_number(const NumberSpec&, double)` → fixed `decimals` (`"140.00"`)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("utf8 helpers") {
    CHECK(utf8_length("żółw") == 4);
    CHECK(utf8_truncate("żółwie", 3) == "żół");
    CHECK(trim("  a b \t") == "a b");
}
TEST_CASE("TextEdit moves and deletes by code point") {
    TextEdit t; t.set_text("żó");
    t.backspace(); CHECK(t.text() == "ż");
    t.move_left(false); CHECK(t.caret() == 0); t.del(); CHECK(t.text().empty());
}
TEST_CASE("TextEdit selection replace and max length") {
    TextEdit t(5); t.set_text("abc"); t.select_all(); t.insert("xy"); CHECK(t.text() == "xy");
    t.insert("1234567"); CHECK(t.text() == "xy123");
    t.home(false); t.move_right(true); CHECK(t.selected_text() == "x");
}
TEST_CASE("parse_number accepts comma and clamps") {           // Review Focus 3
    NumberSpec bpm{20, 999, 2, 0.5, 0.01, {}};
    CHECK(*parse_number(bpm, "140,5") == doctest::Approx(140.5));
    CHECK(*parse_number(bpm, " 95.123 ") == doctest::Approx(95.12));
    CHECK(*parse_number(bpm, "5000") == doctest::Approx(999));
    CHECK(*parse_number(bpm, "3") == doctest::Approx(20));
    CHECK_FALSE(parse_number(bpm, "abc")); CHECK_FALSE(parse_number(bpm, ""));
    CHECK_FALSE(parse_number(bpm, "12abc"));
}
TEST_CASE("drag_value") {
    NumberSpec bpm{20, 999, 2, 0.5, 0.01, {}};
    CHECK(drag_value(bpm, 140, 20, false) == doctest::Approx(150));
    CHECK(drag_value(bpm, 140, 20, true) == doctest::Approx(140.2));
    CHECK(drag_value(bpm, 990, 100, false) == doctest::Approx(999));
    NumberSpec den{1, 16, 0, 1.0 / 8, 1.0 / 8, {1, 2, 4, 8, 16}};
    CHECK(drag_value(den, 4, 8, false) == 8);
    CHECK(drag_value(den, 4, -16, false) == 1);
}
TEST_CASE("format_number") { CHECK(format_number({20, 999, 2}, 140) == "140.00"); }
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement** (`parse_number` uses `std::from_chars` on a copy with `,` replaced by `.`, and requires the whole string to be consumed)
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"ui: text editing and number parsing/formatting"`

---

### Task 10: Window, Canvas, Theme, fonts, screenshot

**Files:**
- Create: `src/ui/theme.h`, `src/ui/theme.cpp`, `src/ui/canvas.h`, `src/ui/canvas.cpp`, `src/ui/fonts.h`, `src/ui/fonts.cpp`, `src/ui/window.h`, `src/ui/window.cpp`
- Modify: `src/app/main.cpp` (temporary demo root), `CMakeLists.txt` (`aloglas_ui` links `SDL3::SDL3-static aloglas_glad aloglas_nanovg`)

**Interfaces:**
- Produces:
  - `struct Color { uint8_t r, g, b, a = 255; }; constexpr Color rgb(uint32_t hex, uint8_t alpha = 255);`
  - `struct Theme` with colors `background, panel, panel_border, text, text_dim, accent, button, button_hover, button_on, field_bg, caret, selection, track_header, track_header_selected, lane, lane_alt, grid_bar, grid_beat, grid_sub, playhead, start_marker, loop_on, loop_off` and metrics `font_size = 13, font_size_small = 11, transport_height = 40, status_height = 24, header_width = 180, loop_strip_height = 14, bar_numbers_height = 18, track_height = 48`; `const Theme& theme();`. Colors are a temporary dark gray scheme close to Ableton's default dark theme, with an orange accent. The author replaces them later.
  - `class Canvas { public: enum class Align { Left, Center, Right }; Canvas(NVGcontext*, int font_regular, int font_bold); void fill_rect(Rect, Color); void stroke_rect(Rect, Color, float width = 1); void hline(float x0, float x1, float y, Color); void vline(float x, float y0, float y1, Color); void fill_triangle(Point, Point, Point, Color); void text(Rect, std::string_view, Color, float size, Align = Align::Left, bool bold = false); float text_width(std::string_view, float size, bool bold = false); void push_clip(Rect); void pop_clip(); };` `hline`/`vline` snap 1-px lines to pixel centers (crisp grid). `text` centers vertically and adds an ellipsis when the text doesn't fit. Clips nest by intersection (NanoVG's scissor doesn't nest on its own, so keep a stack).
  - `fonts.h`: `std::span<const unsigned char> inter_regular(); std::span<const unsigned char> inter_semibold();` (via `#embed "../../assets/fonts/…"`, `-Wc23-extensions` silenced locally with a pragma)
  - `struct WindowOptions { std::string title = "Aloglas"; int width = 1280; int height = 760; bool hidden = false; };`
  - `class Window final : public UiHost { public: explicit Window(const WindowOptions&); ~Window() override; bool ok() const; const std::string& error() const; void set_root(Widget&); void run(const std::function<bool()>& before_frame); bool render_to_png(const std::string& path, int width, int height); /* UiHost overrides */ };`

Window behavior:
- GL 3.3 core context with an 8-bit stencil, `gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)`, `nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES)`, fonts loaded with `nvgCreateFontMem(..., freeData = 0)`, vsync on (`SDL_GL_SetSwapInterval(1)`).
- `run` loop: when nothing is dirty and `before_frame` returned false last time, block in `SDL_WaitEvent`. Drain events with `SDL_PollEvent`, call `before_frame()`, and render if dirty or animating. While the window is minimized or zero-sized, skip rendering and wait with `SDL_WaitEventTimeout(…, 16)` so playback doesn't spin a core (Review Focus 4).
- Rendering: logical size = window size, pixel ratio from `SDL_GetWindowDisplayScale`. `root.layout({0,0,w,h})`, clear to `theme().background`, `nvgBeginFrame`, `root.draw(canvas)`, `nvgEndFrame`, swap.
- Event translation: SDL mouse button/motion/wheel → `EventRouter` (`clicks` from `event.button.clicks`, modifiers from `SDL_GetModState`). Key down → `Key` (unmapped → `Unknown`). `SDL_EVENT_TEXT_INPUT` → `text`. `SDL_EVENT_WINDOW_MOUSE_LEAVE` → `mouse_left_window`. Every input event marks the window dirty. `set_text_input` calls `SDL_StartTextInput`/`SDL_StopTextInput` on the window.
- `render_to_png`: `nvgluCreateFramebuffer` (from `nanovg_gl_utils.h`) at `width×height`, scale 1, renders one frame into it, `glReadPixels` RGBA, flips rows, `SDL_CreateSurfaceFrom(…, SDL_PIXELFORMAT_ABGR8888, …)`, `SDL_SavePNG`. Works with `hidden = true`. (A scratch spike confirmed this whole path works with these exact versions.)

- [ ] **Step 1: Implement theme, canvas, fonts, window**
- [ ] **Step 2: Temporary demo root in `main.cpp`**: a widget that draws theme swatches, a 1-px grid, and the text `"Aloglas 140.00 1.1.1"` in regular and bold. Support `--screenshot <path>` (hidden window, `render_to_png(path, 1280, 760)`, exit code 0 on success).
- [ ] **Step 3: Build and capture**

Run: `tools/dev.sh cmake --build --preset debug && build/debug/aloglas.exe --screenshot build/shot.png`
Expected: exit 0. Open `build/shot.png` with the Read tool: swatches visible, text crisp and antialiased, grid lines exactly 1 px wide.

- [ ] **Step 4: Interactive check**: run `build/debug/aloglas.exe` and confirm the window opens, resizes, and closes cleanly. When idle, Task Manager shows ~0% CPU for the process.
- [ ] **Step 5: Run the whole test suite** — Expected: PASS (no regressions)
- [ ] **Step 6: Commit** — `"ui: SDL3/NanoVG window, canvas, theme, embedded Inter, PNG screenshots"`

---

### Task 11: Basic widgets

**Files:**
- Create: `src/ui/widgets/label.h/.cpp`, `button.h/.cpp`, `toggle_button.h/.cpp`, `text_field.h/.cpp`, `number_box.h/.cpp`
- Test: `tests/widgets_test.cpp`

**Interfaces:**
- Consumes: `Widget`, `EventRouter`, `TextEdit`, `NumberSpec` helpers, `Canvas`, `theme()`.
- Produces:
  - `class Label : public Widget { public: Label(std::function<std::string()> text, Canvas::Align = Canvas::Align::Left, bool bold = false); };`
  - `class Button : public Widget { public: explicit Button(std::string label); std::function<void()> on_click; };`. The click fires on mouse up only if the press started inside and the release is inside.
  - `class ToggleButton : public Widget { public: explicit ToggleButton(std::string label); std::function<bool()> value; std::function<void(bool)> on_toggle; std::function<void(Canvas&, Rect, bool on)> draw_icon; };`. A click calls `on_toggle(!value())`. When `draw_icon` is set, it's drawn instead of the label.
  - `class TextField : public Widget { public: explicit TextField(std::size_t max_chars = 64); void begin(std::string_view initial); bool active() const; TextEdit& edit(); std::function<void(const std::string&)> on_commit; std::function<void()> on_cancel; };`. `begin` sets the text, selects all, takes focus, and turns text input on. Enter → commit. Escape → cancel. Focus lost while active → commit. Commit and cancel clear `active` first, so a commit never fires twice, then release focus. While focused it consumes every key **except** keys with Ctrl other than Ctrl+A/C/V/X (so Ctrl+T and Ctrl+L bubble). A click places the caret and a drag extends the selection (`Canvas::text_width` is only available while drawing, so store the glyph x positions computed in `draw` for hit-testing). Ctrl+C/V/X go through `UiHost` clipboard calls.
  - `class NumberBox : public Widget { public: explicit NumberBox(NumberSpec); std::function<double()> value; std::function<void(double)> on_change; };`. A vertical drag calls `on_change(drag_value(spec, start, start_y - y, shift))` on each move (only when the value changes). A double-click opens an internal `TextField` with `format_number(value())`. On commit, `parse_number` → `on_change` if it returns a value; otherwise nothing changes. Escape → no change.

- [ ] **Step 1: Write the failing tests** (FakeHost + EventRouter, no drawing)

```cpp
TEST_CASE("Button clicks only when released inside") {}
TEST_CASE("ToggleButton flips the value it reads") {}
TEST_CASE("TextField commit, cancel, focus loss") {
    // begin("old"); text "new" + Enter → on_commit("new") once; focus lost afterwards does not commit again
    // begin; Escape → on_cancel, no commit; begin; click elsewhere → on_commit(current text)
}
TEST_CASE("TextField consumes Space and Delete, lets Ctrl+T bubble") {}
TEST_CASE("NumberBox drag") {
    // spec {20,999,2,0.5,0.01}, value 140; drag 20 px up → last on_change == 150; with shift → 140.2
}
TEST_CASE("NumberBox text entry") {                         // Review Focus 3
    // double-click, select-all replaced by "140,5" + Enter → on_change(140.5)
    // "abc" + Enter → no on_change; Escape → no on_change; "5000" → on_change(999)
}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement** (drawing uses `theme()`: buttons `button`/`button_hover`/`button_on`, fields `field_bg`/`caret`/`selection`)
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"ui: label, button, toggle, text field, number box"`

---

### Task 12: Document and Controller

**Files:**
- Create: `src/app/document.h`, `src/app/document.cpp`, `src/app/controller.h`, `src/app/controller.cpp`
- Create: `tests/support/recording_sink.h`
- Test: `tests/controller_test.cpp`
- Modify: `CMakeLists.txt` (add `aloglas_app_lib` → ui, engine, audio, core; `aloglas` and `aloglas_tests` link it)

**Interfaces:**
- Consumes: `Command`, `CommandSink`, `core/time.h`, `utf8_truncate`, `trim`.
- Produces:
  - `struct Track { int id; std::string name; };`
  - `struct LoopRange { bool enabled = false; int64_t start = 0; int64_t end = 4 * 3840; };`
  - `struct Document { double tempo = 140.0; TimeSignature signature{4, 4}; bool metronome = true; LoopRange loop; int64_t start_marker = 0; bool follow = true; std::vector<Track> tracks; int selected_track = -1; int next_track_number = 1; };`
  - `Document make_default_document();` → tracks `{1,"1 Track"}…{4,"4 Track"}`, `next_track_number = 5`, nothing selected
  - `class Controller { public: Controller(Document&, CommandSink&); const Document& doc() const; std::function<bool()> ensure_audio; std::function<void()> on_change; void sync_engine(); void play(); void stop(); void toggle_play(bool playing_now); void set_tempo(double bpm); void set_signature(TimeSignature); void set_metronome(bool); void set_loop_range(int64_t start, int64_t end); void set_loop_enabled(bool); void toggle_loop(); void set_start_marker(int64_t tick, bool seek_playhead); void set_follow(bool); int add_track(); void delete_selected_track(); void rename_track(int id, std::string_view name); void select_track(int id); };`
  - `tests/support/recording_sink.h`: `struct RecordingSink : CommandSink { std::vector<Command> sent; bool send(const Command&) override; };`

Rules:
- Every mutation sends the matching command (when there is one) and then calls `on_change` (if set). `sync_engine` sends SetTempo, SetSignature, SetMetronome, SetLoop, SetStart in that order.
- `play()`: if `ensure_audio` is set and returns false, send nothing. Otherwise send Play. `toggle_play(true)` → `stop()`, `toggle_play(false)` → `play()`.
- `set_tempo`: `clamp_to_spec({20, 999, 2})`.
- `set_signature`: ignores invalid signatures. After storing a valid one it re-normalizes the loop and clamps the start marker (Review Focus 2).
- `set_loop_range` normalization: swap reversed bounds; clamp to `[0, max_ticks]`; if `end - start < ticks_per_beat`, set `end = start + ticks_per_beat`, and if that passes `max_ticks`, shift both left.
- `set_start_marker`: clamp to `[0, max_ticks - 1]`. Sends SetStart, then Seek if `seek_playhead`.
- `add_track`: inserts after the selected track (or at the end), named `"<n> Track"` with id `n = next_track_number++`, selects it, returns the id.
- `delete_selected_track`: no-op without a selection. Afterwards it selects the next track, else the previous one, else none.
- `rename_track`: `trim`, empty → no change, `utf8_truncate(…, 64)`, unknown id → no change.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("defaults and sync_engine") {
    Document d = make_default_document(); RecordingSink s; Controller c(d, s);
    CHECK(d.tracks.size() == 4); CHECK(d.tracks[3].name == "4 Track");
    c.sync_engine();
    REQUIRE(s.sent.size() == 5);
    CHECK(s.sent[0].type == CommandType::SetTempo); CHECK(s.sent[0].number == 140.0);
}
TEST_CASE("tempo clamps and rounds") { /* 10→20, 1500→999, 140.456→140.46 */ }
TEST_CASE("signature change revalidates loop and start marker") {   // Review Focus 2
    // loop [0, 960] in 4/4 → set_signature({4,2}) → loop.end == 1920
    // start_marker = 999 bars of 4/4 (3'836'160) → set_signature({1,16}) → start_marker == max_ticks({1,16}) - 1
    // set_signature({4,3}) → ignored, nothing sent
}
TEST_CASE("loop range normalization") { /* reversed, too short, beyond max */ }
TEST_CASE("start marker seek") { /* seek=true → SetStart then Seek; false → SetStart only */ }
TEST_CASE("play without audio sends nothing") {                     // Review Focus 5
    // ensure_audio = []{ return false; } → play() sends nothing; = true → sends Play; toggle_play(true) → Stop
}
TEST_CASE("add, delete, select tracks") {
    // select 2, add → new id 5 at index 2 named "5 Track", selected
    // delete selected (5) → selects track at the same index (id 3); delete last → selects previous; delete only → -1
}
TEST_CASE("rename rules") { /* trims; "" ignored; 70 'ż' → 64 code points; unknown id ignored */ }
TEST_CASE("on_change fires for every mutation") {}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"app: document model and controller"`

---

### Task 13: Timeline math

**Files:**
- Create: `src/app/timeline_view.h`, `src/app/timeline_view.cpp`
- Test: `tests/timeline_view_test.cpp`

**Interfaces:**
- Produces:
  - `inline constexpr double kMinPxPerQuarter = 2.0, kMaxPxPerQuarter = 2000.0; inline constexpr float kMinGridPx = 12.0f, kMinBarNumberPx = 40.0f;`
  - `struct GridSpacing { int64_t line_ticks; int bar_line_step; int bar_number_step; int64_t snap_ticks; };`
  - `GridSpacing choose_grid(double px_per_quarter, TimeSignature)`. Candidates in ascending order: 120, 240, 480 ticks, then `ticks_per_beat`, then `ticks_per_bar`. Drop duplicates and any candidate larger than a bar or one that doesn't divide the bar. `line_ticks` = the finest candidate with spacing ≥ 12 px, else `ticks_per_bar`. `bar_line_step` = smallest power of two with `bar_px * step ≥ 12`. `bar_number_step` = smallest power of two with `bar_px * step ≥ 40`. `snap_ticks` = `line_ticks` when `bar_line_step == 1`, else `bar_line_step * ticks_per_bar`.
  - `int64_t snap_tick(double tick, int64_t snap_ticks)` (round to nearest)
  - `struct TimelineView { double px_per_quarter = 40.0; double scroll_ticks = 0.0; float x0 = 0; float width = 0; float tick_to_x(double) const; double x_to_tick(float) const; double visible_ticks() const; void zoom_at(float x, double factor, TimeSignature); void scroll_by_px(float dx, TimeSignature); void clamp_scroll(TimeSignature); bool follow(double playhead, TimeSignature); };`
  - `follow`: if `playhead < scroll_ticks` or `playhead >= scroll_ticks + visible_ticks()`, set `scroll_ticks = playhead`, clamp, return true. Otherwise return false.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("tick <-> x") {
    TimelineView v{.px_per_quarter = 40, .scroll_ticks = 960, .x0 = 180, .width = 1100};
    CHECK(v.tick_to_x(960) == doctest::Approx(180)); CHECK(v.tick_to_x(1920) == doctest::Approx(220));
    CHECK(v.x_to_tick(220) == doctest::Approx(1920));
}
TEST_CASE("zoom keeps the tick under the mouse and clamps") {
    // tick at x=500 before == after zoom_at(500, 2.0); zoom_at(…, 1e6) → px_per_quarter == 2000; 1e-6 → 2
}
TEST_CASE("scroll clamps to [0, end - visible]") { /* incl. at 2 px/quarter where visible > end → scroll 0 */ }
TEST_CASE("grid density") {
    auto g = choose_grid(40, {4,4});  CHECK(g.line_ticks == 480); CHECK(g.bar_line_step == 1); CHECK(g.bar_number_step == 1);
    g = choose_grid(2, {4,4});        CHECK(g.line_ticks == 3840); CHECK(g.bar_line_step == 2); CHECK(g.bar_number_step == 8);
    CHECK(g.snap_ticks == 2 * 3840);
    g = choose_grid(2000, {4,4});     CHECK(g.line_ticks == 120);
    g = choose_grid(40, {6,8});       CHECK(g.line_ticks == 480);
}
TEST_CASE("snap") { CHECK(snap_tick(1000, 480) == 960); CHECK(snap_tick(1300, 480) == 1440); }
TEST_CASE("follow pages forward and back") { /* inside → false; past right edge → scroll == playhead; left of view → scroll == playhead */ }
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Commit** — `"app: timeline view math, adaptive grid, snapping"`

---

### Task 14: MainView, TransportBar, StatusBar, and wiring

**Files:**
- Create: `src/app/snapshot.h`, `src/app/frame_update.h/.cpp`, `src/app/transport_bar.h/.cpp`, `src/app/status_bar.h/.cpp`, `src/app/main_view.h/.cpp`
- Modify: `src/app/main.cpp` (replace the demo)
- Test: `tests/app_shell_test.cpp`

**Interfaces:**
- Consumes: Controller, Document, Engine, AudioDevice, Window, widgets, TimelineView.
- Produces:
  - `struct Snapshot { bool playing = false; double position_ticks = 0; float cpu_load = 0; bool audio_disabled = false; AudioStatus audio = AudioStatus::Closed; std::string audio_error; uint32_t sample_rate = 0, buffer_frames = 0; uint32_t dropped_commands = 0; };`
  - `void frame_update(Engine&, AudioDevice* device, Controller&, Snapshot&)`. If `device && device->take_lost()`: `device->close(); controller.stop();`. If there's no device or it isn't `Running`: `engine.pump()`, so queued commands apply and don't pile up. Then fill every Snapshot field (`audio_disabled = device == nullptr`).
  - `std::string status_text(const Snapshot&)` → `"Audio: 48000 Hz, 480 samples"` / `"No audio — press Play to retry"` (NoDevice or Closed) / `"Audio error: <msg>"` / `"Audio disabled (--no-audio)"`, plus `" · <n> commands dropped"` when `n > 0`.
  - `std::string position_text(const Snapshot&, const Document&)` → `format_bbs(to_bbs(pos, sig)) + "  " + format_clock(ticks_to_seconds(pos, tempo))`
  - `class TransportBar : public Widget { public: TransportBar(Controller&, const Snapshot&); };`. Children left to right: play `ToggleButton` (icon: triangle when stopped, square when playing; value = `snapshot.playing`, toggle → `controller.toggle_play(snapshot.playing)`), tempo `NumberBox{20, 999, 2, 0.5, 0.01}`, numerator `NumberBox{1, 32, 0, 1.0/8, 1.0/8}`, denominator `NumberBox{1, 16, 0, 1.0/8, 1.0/8, {1,2,4,8,16}}`, metronome/loop/follow `ToggleButton`s, position `Label(position_text)`, CPU meter (custom-drawn bar + `"CPU 12%"`).
  - `class StatusBar : public Widget { public: explicit StatusBar(const Snapshot&); };`
  - `class MainView : public Widget { public: MainView(Controller&, const Snapshot&); TimelineView& timeline(); float& track_scroll_y(); bool on_key(const KeyEvent&) override; };`. Layout per spec §8.2 with `theme()` metrics. Tasks 15/16 put Ruler + LaneArea and TrackList into the regions this task leaves empty (they're drawn as plain `panel` rectangles until then). Shortcuts: Space → `toggle_play(snapshot.playing)`, Ctrl+T → `add_track`, Delete → `delete_selected_track`, Ctrl+L → `toggle_loop`.
  - `main.cpp`: flags `--screenshot <path>`, `--no-audio`. It creates `Engine`, `AudioDevice` (unless `--no-audio`), `Document = make_default_document()`, `Controller` (with `ensure_audio = device running || device->open()`, `on_change = window.invalidate`), `Snapshot`, `Window`, `MainView`. It calls `controller.sync_engine()`, then `engine.pump()`, then tries to open the device once. `before_frame`: `frame_update(...)`; with `doc.follow` and playing, `main_view.timeline().follow(...)` (invalidate when it returns true); return `snapshot.playing`. In `--screenshot` mode: no audio device, hidden window, `render_to_png(path, 1280, 760)`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("status_text") { /* all four states and the dropped suffix */ }
TEST_CASE("position_text") { /* pos 3840+960 at 120 BPM 4/4 → "2.2.1  00:02.500" */ }
TEST_CASE("frame_update pumps engine without device") {                  // Review Focus 5
    // Engine e; e.set_assert_on_drop(false); Controller over e; 10 rounds of { 100x set_tempo; frame_update(e, nullptr, c, snap) }
    // → snap.dropped_commands == 0, snap.audio_disabled
}
TEST_CASE("MainView shortcuts") {
    // FakeHost + router over MainView; Space with snapshot.playing=false → Play sent (ensure_audio true)
    // Ctrl+T → 5 tracks; Ctrl+L → loop enabled; Delete with selection → track removed
}
TEST_CASE("MainView layout survives tiny sizes") {                       // Review Focus 4
    // layout({0,0,0,0}), ({0,0,120,60}), ({0,0,1280,760}): every widget in the tree has w >= 0 && h >= 0
}
TEST_CASE("TransportBar tempo drag changes the document") { /* drag 20 px up on the tempo box → doc.tempo == 150 */ }
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Screenshot and live run**

Run: `build/debug/aloglas.exe --screenshot build/shot.png`, then view it with Read. Expected: transport bar with 140.00, 4 / 4, toggles, `1.1.1  00:00.000`, status bar text.
Run: `build/debug/aloglas.exe`, press Space. Expected: the metronome clicks at 140 BPM with an accent on beat 1, the position display counts, and Space again stops and returns to `1.1.1`. Dragging the tempo while playing changes the speed immediately.

- [ ] **Step 6: Commit** — `"app: main view, transport bar, status bar, engine/device wiring"`

---

### Task 15: Playlist — Ruler and LaneArea

**Files:**
- Create: `src/app/ruler.h/.cpp`, `src/app/lane_area.h/.cpp`
- Modify: `src/app/main_view.cpp` (place them)
- Test: `tests/playlist_test.cpp`

**Interfaces:**
- Consumes: `TimelineView`, `choose_grid`, `snap_tick`, `Controller`, `Snapshot`, `theme()`.
- Produces:
  - `class Ruler : public Widget { public: Ruler(Controller&, TimelineView&, const Snapshot&); };`. The top `loop_strip_height` is the loop strip; the next `bar_numbers_height` holds the bar numbers.
  - `class LaneArea : public Widget { public: LaneArea(Controller&, TimelineView&, const Snapshot&, float& track_scroll_y); };`
  - `MainView::layout` sets `timeline().x0` and `timeline().width` to the playlist region and calls `clamp_scroll`.

Ruler behavior:
- Loop strip `on_mouse_down`: within 4 px of an edge → resize that edge. Inside the brace → move (length kept). Elsewhere → create a range from the pressed tick and enable the loop. Each move calls `controller.set_loop_range` with ticks snapped by `snap_tick(tick, choose_grid(view.px_per_quarter, sig).snap_ticks)` unless `mods.alt`. Double-click (`clicks == 2`) → `toggle_loop`.
- Bar-number strip `on_mouse_down` → `set_start_marker(snap(tick), snapshot.playing)`.
- Draw: the loop brace in `loop_on`/`loop_off`, bar numbers every `bar_number_step` bars, the start-marker triangle, and the playhead line.

LaneArea behavior:
- Draw: alternating `lane`/`lane_alt` rows per track at `track_height`, offset by `track_scroll_y`. Grid lines from `choose_grid` (bar lines `grid_bar`, beats `grid_beat`, finer `grid_sub`), drawn only for the visible range. Faint start-marker line. Loop range lightly shaded when enabled. Playhead on top.
- Wheel: Ctrl → `zoom_at(pos.x, pow(1.15, dy))`. Shift → `scroll_by_px(-dy * 60)`. Otherwise `track_scroll_y -= dy * 40`, clamped to `[0, max(0, tracks * track_height - rect.h)]`.

- [ ] **Step 1: Write the failing tests** (MainView at 1280×760 with FakeHost, `px_per_quarter = 40`, so 1/8 snapping = 480 ticks)

```cpp
TEST_CASE("drag in the empty loop strip creates and enables a loop") {
    // drag from x(tick 3840) to x(tick 11520) → loop {enabled, 3840, 11520}
}
TEST_CASE("drag the brace body moves it, edges resize, min one beat") {}
TEST_CASE("alt disables snapping") { /* end lands on an unsnapped tick */ }
TEST_CASE("double-click the loop strip toggles loop") {}
TEST_CASE("bar-number click sets the start marker; seeks only while playing") {
    // snapshot.playing = false → SetStart only; = true → SetStart + Seek
}
TEST_CASE("lane wheel: ctrl zooms at mouse, shift scrolls, plain scrolls tracks (clamped)") {}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Screenshot + live check**: the screenshot shows the ruler with numbers 1, 2, 3…, a gray loop brace over bars 1–5, the grid, and the playhead at bar 1. Live: create a loop by dragging; with loop on, Play starts at the loop start, jumps back on time with an accent, and the view pages with the playhead. Ctrl+wheel zooms smoothly around the mouse.
- [ ] **Step 6: Commit** — `"app: playlist ruler with loop brace and lanes with adaptive grid"`

---

### Task 16: Track list

**Files:**
- Create: `src/app/track_list.h/.cpp`
- Modify: `src/app/main_view.cpp` (place it)
- Test: `tests/track_list_test.cpp`

**Interfaces:**
- Consumes: `Controller`, `TextField`, `theme()`.
- Produces:
  - `class TrackHeader : public Widget { public: TrackHeader(Controller&, int track_id); int track_id() const; bool editing() const; };`
  - `class TrackList : public Widget { public: TrackList(Controller&, float& track_scroll_y); TrackHeader* header_for(int track_id); };`
- `TrackList::layout` syncs headers with `doc().tracks`. It **reuses** existing `TrackHeader` objects by id, creates headers for new ids, and removes headers for missing ids (via `remove()` so the router forgets them). Then it reorders `children` to match document order and stacks them at `track_height` offset by `track_scroll_y`. Headers live in `std::vector<std::unique_ptr<TrackHeader>>`.
- `TrackHeader`: a click selects the track. A double-click on the name starts `TextField::begin(name)`. Commit → `rename_track`. Cancel → nothing. Drawn in `track_header` / `track_header_selected` with the name in `text`.
- Wheel over the list scrolls `track_scroll_y` like the lanes (same clamp).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("headers follow the document and are reused") {
    // 4 headers; remember header_for(2) pointer; Ctrl+T → 5 headers, header_for(2) is the same pointer
}
TEST_CASE("click selects, double-click renames") {
    // double-click header 2 → editing(); text "Bass żółw" + Enter → doc name "Bass żółw"; Escape path keeps the old name
}
TEST_CASE("Space and Delete while renaming edit the text, not the transport/tracks") {
    // while editing: Space → no Play sent; Delete → still 4 tracks
}
TEST_CASE("rename survives adding a track") {                            // Review Focus 1
    // editing header 2; Ctrl+T → no crash, header_for(2)->editing() still true, router focus is inside header 2
    // Enter → rename applied to track 2
}
TEST_CASE("deleting the selected track removes its header and clears router pointers") {}
```

- [ ] **Step 2: Run and see them fail**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run tests** — Expected: PASS
- [ ] **Step 5: Screenshot + live check**: four headers "1 Track"…"4 Track" aligned with the lanes. Rename with Polish characters, add/delete with Ctrl+T/Delete, and scroll headers and lanes together.
- [ ] **Step 6: Commit** — `"app: track list with inline rename"`

---

### Task 17: README and final verification

**Files:**
- Create: `README.md`

- [ ] **Step 1: README** (English, short): what Aloglas is (an educational DAW, stage 1), build requirements (llvm-mingw, CMake ≥ 3.25, Ninja), the three build/test commands, run flags, shortcuts table (spec §8.7), license (MIT; Inter under OFL in `assets/fonts/OFL.txt`). Nothing about the author or private infrastructure.
- [ ] **Step 2: Full test run in both presets**

Run: `tools/dev.sh cmake --preset release && tools/dev.sh cmake --build --preset release && tools/dev.sh ctest --preset release && tools/dev.sh ctest --preset debug`
Expected: both `100% tests passed`, and the build shows no warnings from project code.

- [ ] **Step 3: Manual checklist** (record the results in the final report to the author)
  - The metronome is audibly in time with the moving playhead at 70, 140, and 200 BPM.
  - The loop jump has no gap or double click, and the accent lands on the loop start.
  - Stop returns to the start marker (or to the loop start when the loop is on).
  - Idle (stopped, window focused) shows ~0% CPU. Minimized while playing stays at low single digits and the audio keeps playing.
  - With the audio device disabled in Windows: the status bar says "No audio — press Play to retry", Play does nothing, and the app doesn't freeze.
  - `objdump -p build/release/aloglas.exe` lists only system DLLs.
- [ ] **Step 4: Commit** — `"docs: README for stage 1"`
