# Third-party dependencies, pinned by URL + SHA256 so every build uses the
# exact same sources. Downloaded once per build directory.
include(FetchContent)

# SDL3: window, input, OpenGL context, file dialogs. Built as a static library
# so the final .exe does not need SDL3.dll next to it.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
  URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)

# NanoVG: antialiased 2D vector drawing on top of OpenGL. It has no
# CMakeLists, so MakeAvailable only downloads it; the target is defined below.
FetchContent_Declare(nanovg
  URL https://github.com/memononen/nanovg/archive/ce3bf745eb2d2dbc14a50bf2446783f691ac4353.tar.gz
  URL_HASH SHA256=655fe16920f1d2ec0895adf3a0eed1d3d0403cc4e2de247f3600d136f206a520)

# miniaudio: single header for audio output and file decoding.
FetchContent_Declare(miniaudio
  URL https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h
  URL_HASH SHA256=ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89
  DOWNLOAD_NO_EXTRACT TRUE)

# doctest: single-header unit test framework.
FetchContent_Declare(doctest
  URL https://raw.githubusercontent.com/doctest/doctest/v2.5.3/doctest/doctest.h
  URL_HASH SHA256=cfd518a3ef90f67e1f3ba514df23fb3627437de1a2feeba78cf5062a40021421
  DOWNLOAD_NO_EXTRACT TRUE)

FetchContent_MakeAvailable(SDL3 nanovg miniaudio doctest)

add_library(aloglas_nanovg STATIC ${nanovg_SOURCE_DIR}/src/nanovg.c)
target_include_directories(aloglas_nanovg SYSTEM PUBLIC ${nanovg_SOURCE_DIR}/src)

add_library(aloglas_glad STATIC ${PROJECT_SOURCE_DIR}/third_party/glad/src/gl.c)
target_include_directories(aloglas_glad SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/glad/include)

add_library(aloglas_miniaudio_headers INTERFACE)
target_include_directories(aloglas_miniaudio_headers SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})

add_library(aloglas_doctest INTERFACE)
target_include_directories(aloglas_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
