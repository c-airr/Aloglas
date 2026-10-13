# Toolchain file: build with clang from llvm-mingw instead of MSVC.
#
# llvm-mingw ships clang plus the MinGW-w64 headers and libc++, so it needs no
# Visual Studio environment (unlike clang-cl). Point LLVM_MINGW at its root
# folder, or put its bin/ directory on PATH.

set(CMAKE_SYSTEM_NAME Windows)

set(_llvm_mingw_root "$ENV{LLVM_MINGW}")
if(_llvm_mingw_root)
  # Windows paths use backslashes, which CMake reads as escape characters.
  file(TO_CMAKE_PATH "${_llvm_mingw_root}" _llvm_mingw_root)
else()
  find_program(_llvm_mingw_clang clang)
  if(_llvm_mingw_clang)
    get_filename_component(_llvm_mingw_bin "${_llvm_mingw_clang}" DIRECTORY)
    get_filename_component(_llvm_mingw_root "${_llvm_mingw_bin}" DIRECTORY)
  endif()
endif()
if(NOT _llvm_mingw_root OR NOT EXISTS "${_llvm_mingw_root}/bin/clang.exe")
  message(FATAL_ERROR
    "llvm-mingw not found. Set the LLVM_MINGW environment variable to its root "
    "folder (the one containing bin/clang.exe) or put its bin/ on PATH.")
endif()

set(CMAKE_C_COMPILER   "${_llvm_mingw_root}/bin/clang.exe")
set(CMAKE_CXX_COMPILER "${_llvm_mingw_root}/bin/clang++.exe")
# SDL compiles a Windows version resource (.rc), so CMake needs a resource compiler.
set(CMAKE_RC_COMPILER  "${_llvm_mingw_root}/bin/llvm-windres.exe")
set(CMAKE_AR           "${_llvm_mingw_root}/bin/llvm-ar.exe")
set(CMAKE_RANLIB       "${_llvm_mingw_root}/bin/llvm-ranlib.exe")
