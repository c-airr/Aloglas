cmake_minimum_required(VERSION 3.25)
project(Aloglas VERSION 0.1.0 LANGUAGES C CXX)

include(cmake/deps.cmake)

# Settings shared by every target that holds our own code (not third-party).
function(aloglas_project_target target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  # -Wno-missing-field-initializers: our event/config structs give every field
  # a default, so initializing only the first few ({.pos} style) is intended.
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endfunction()

# Link the C++ runtime (libc++, libunwind, winpthread) statically, so the .exe
# runs on machines without llvm-mingw installed.
function(aloglas_static_exe target)
  target_link_options(${target} PRIVATE -static)
endfunction()

# --- core ------------------------------------------------------------------
add_library(aloglas_core STATIC
  src/core/time.cpp
  src/core/rt_guard.cpp
  src/core/utf8.cpp
  src/core/natural_sort.cpp)
aloglas_project_target(aloglas_core)
target_include_directories(aloglas_core PUBLIC src)

# Audio-thread allocation guard: on by default in Debug (see core/rt_guard.h).
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_rt_guard_default ON)
else()
  set(_rt_guard_default OFF)
endif()
option(ALOGLAS_RT_GUARD "Abort on allocation/free inside the audio thread" ${_rt_guard_default})
if(ALOGLAS_RT_GUARD)
  target_compile_definitions(aloglas_core PRIVATE ALOGLAS_RT_GUARD)
endif()

# --- miniaudio (third-party implementation, shared by samples and audio) ---
add_library(aloglas_miniaudio STATIC src/audio/miniaudio_impl.c)
target_link_libraries(aloglas_miniaudio PUBLIC aloglas_miniaudio_headers)
# Third-party code: compile without our warning flags.
set_source_files_properties(src/audio/miniaudio_impl.c PROPERTIES COMPILE_OPTIONS "-w")

# --- samples ---------------------------------------------------------------
add_library(aloglas_samples STATIC
  src/samples/sample.cpp
  src/samples/sample_loader.cpp
  src/samples/sample_cache.cpp)
aloglas_project_target(aloglas_samples)
target_link_libraries(aloglas_samples PUBLIC aloglas_core PRIVATE aloglas_miniaudio)

# --- engine ----------------------------------------------------------------
add_library(aloglas_engine STATIC
  src/engine/click.cpp
  src/engine/engine.cpp
  src/engine/sample_voice.cpp)
aloglas_project_target(aloglas_engine)
target_link_libraries(aloglas_engine PUBLIC aloglas_core aloglas_samples)

# --- audio -----------------------------------------------------------------
add_library(aloglas_audio STATIC
  src/audio/audio_device.cpp)
aloglas_project_target(aloglas_audio)
target_link_libraries(aloglas_audio PUBLIC aloglas_engine PRIVATE aloglas_miniaudio)

# --- ui --------------------------------------------------------------------
add_library(aloglas_ui STATIC
  src/ui/widget.cpp
  src/ui/event_router.cpp
  src/ui/text_edit.cpp
  src/ui/number_format.cpp
  src/ui/text_fit.cpp
  src/ui/theme.cpp
  src/ui/canvas.cpp
  src/ui/fonts.cpp
  src/ui/window.cpp
  src/ui/win32_titlebar.cpp
  src/ui/widgets/label.cpp
  src/ui/widgets/button.cpp
  src/ui/widgets/toggle_button.cpp
  src/ui/widgets/text_field.cpp
  src/ui/widgets/number_box.cpp
  src/ui/widgets/splitter.cpp
  src/ui/widgets/meter_bar.cpp
  src/ui/widgets/tree_view.cpp
  src/ui/widgets/popup_menu.cpp)
aloglas_project_target(aloglas_ui)
target_link_libraries(aloglas_ui
  PUBLIC aloglas_core
  PRIVATE SDL3::SDL3-static aloglas_glad aloglas_nanovg dwmapi)

# --- app -------------------------------------------------------------------
# Everything of the app except main(), so tests can link it.
add_library(aloglas_app_lib STATIC
  src/app/document.cpp
  src/app/controller.cpp
  src/app/log.cpp
  src/app/log_panel.cpp
  src/app/main_layout.cpp
  src/app/frame_update.cpp
  src/app/status_bar.cpp
  src/app/transport_bar.cpp
  src/app/panel.cpp
  src/app/main_view.cpp
  src/app/settings.cpp
  src/app/browser_model.cpp)
aloglas_project_target(aloglas_app_lib)
target_link_libraries(aloglas_app_lib
  PUBLIC aloglas_ui aloglas_engine aloglas_audio aloglas_core
  PRIVATE aloglas_json_headers SDL3::SDL3-static)

add_executable(aloglas
  src/app/main.cpp)
aloglas_project_target(aloglas)
aloglas_static_exe(aloglas)
target_link_libraries(aloglas PRIVATE aloglas_app_lib aloglas_ui SDL3::SDL3-static)

# --- tests -----------------------------------------------------------------
enable_testing()
add_executable(aloglas_tests
  tests/test_main.cpp
  tests/smoke_test.cpp
  tests/time_test.cpp
  tests/float_lesson_test.cpp
  tests/spsc_queue_test.cpp
  tests/rt_guard_test.cpp
  tests/engine_clock_test.cpp
  tests/engine_loop_test.cpp
  tests/audio_device_test.cpp
  tests/geometry_test.cpp
  tests/event_router_test.cpp
  tests/utf8_test.cpp
  tests/text_edit_test.cpp
  tests/number_format_test.cpp
  tests/text_fit_test.cpp
  tests/widgets_test.cpp
  tests/engine_mode_test.cpp
  tests/controller_test.cpp
  tests/log_test.cpp
  tests/splitter_test.cpp
  tests/main_layout_test.cpp
  tests/frame_update_test.cpp
  tests/transport_bar_test.cpp
  tests/main_view_test.cpp
  tests/settings_test.cpp
  tests/sample_test.cpp
  tests/sample_loader_test.cpp
  tests/sample_voice_test.cpp
  tests/engine_preview_test.cpp
  tests/natural_sort_test.cpp
  tests/browser_model_test.cpp
  tests/tree_view_test.cpp)
aloglas_project_target(aloglas_tests)
aloglas_static_exe(aloglas_tests)
target_include_directories(aloglas_tests PRIVATE tests)
target_compile_definitions(aloglas_tests PRIVATE ALOGLAS_TEST_DATA="${PROJECT_SOURCE_DIR}/tests/data")
target_link_libraries(aloglas_tests PRIVATE aloglas_doctest aloglas_app_lib aloglas_core aloglas_engine aloglas_audio aloglas_ui)
add_test(NAME aloglas_tests COMMAND aloglas_tests)
