#!/usr/bin/env bash
# Runs a command with the build tools on PATH:
#   - llvm-mingw (clang) from $LLVM_MINGW, default %USERPROFILE%/llvm-mingw
#   - cmake and ninja, taken from the Visual Studio bundle when they are not
#     already on PATH (VS installs them but does not add them to PATH)
#
# Usage: tools/dev.sh cmake --preset debug
set -euo pipefail

llvm_mingw="${LLVM_MINGW:-${USERPROFILE:-$HOME}/llvm-mingw}"
# C:/forward/slashes for CMake, /c/unix/style for bash's PATH.
export LLVM_MINGW="$(cygpath -m "$llvm_mingw")"
export PATH="$(cygpath -u "$llvm_mingw")/bin:$PATH"

if ! command -v cmake >/dev/null 2>&1; then
  vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  if [ -x "$vswhere" ]; then
    vs=$("$vswhere" -latest -products '*' -property installationPath | tr -d '\r')
    if [ -n "$vs" ]; then
      vs=$(cygpath -u "$vs")
      cm="$vs/Common7/IDE/CommonExtensions/Microsoft/CMake"
      export PATH="$cm/CMake/bin:$cm/Ninja:$PATH"
    fi
  fi
fi

command -v cmake >/dev/null 2>&1 || { echo "dev.sh: cmake not found (install CMake or Visual Studio)" >&2; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "dev.sh: ninja not found" >&2; exit 1; }

exec "$@"
