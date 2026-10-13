cmake_minimum_required(VERSION 3.25)
project(Aloglas VERSION 0.1.0 LANGUAGES C CXX)

include(cmake/deps.cmake)

# Settings shared by every target that holds our own code (not third-party).
function(aloglas_project_target target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  target_compile_options(${target} PRIVATE -Wall -Wextra)
endfunction()

# Link the C++ runtime (libc++, libunwind, winpthread) statically, so the .exe
# runs on machines without llvm-mingw installed.
function(aloglas_static_exe target)
  target_link_options(${target} PRIVATE -static)
endfunction()

# --- core ------------------------------------------------------------------
add_library(aloglas_core STATIC
  src/core/time.cpp)
aloglas_project_target(aloglas_core)
target_include_directories(aloglas_core PUBLIC src)

# --- app -------------------------------------------------------------------
add_executable(aloglas src/app/main.cpp)
aloglas_project_target(aloglas)
aloglas_static_exe(aloglas)
target_link_libraries(aloglas PRIVATE SDL3::SDL3-static aloglas_glad aloglas_nanovg)

# --- tests -----------------------------------------------------------------
enable_testing()
add_executable(aloglas_tests
  tests/test_main.cpp
  tests/smoke_test.cpp
  tests/time_test.cpp
  tests/float_lesson_test.cpp
  tests/spsc_queue_test.cpp)
aloglas_project_target(aloglas_tests)
aloglas_static_exe(aloglas_tests)
target_include_directories(aloglas_tests PRIVATE tests)
target_link_libraries(aloglas_tests PRIVATE aloglas_doctest aloglas_core)
add_test(NAME aloglas_tests COMMAND aloglas_tests)
