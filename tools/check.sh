#!/usr/bin/env bash
# Full local check before committing/pushing: PC build + headless tests (incl. render goldens),
# then the 3DS dev and release builds. Stops at the first failure and prints its full log;
# otherwise prints only warnings and the test summary.
#   tools/check.sh            (from Git Bash, any directory)
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# VS 2022's bundled cmake (a cmake on PATH may lack the Visual Studio generator).
CMAKE="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
[ -x "$CMAKE" ] || CMAKE="cmake"

# step <title> <command...>: runs the command, failing loudly with its output.
step() {
    local title="$1"
    shift
    echo "== $title"
    if ! "$@" > "$LOG" 2>&1; then
        cat "$LOG"
        echo "== FAILED: $title"
        exit 1
    fi
    grep -iE "warning|error" "$LOG" | grep -v "CMake Deprecation\|0 Error" || true
}

if [ ! -f build-pc/CMakeCache.txt ]; then
    step "PC configure" "$CMAKE" -S . -B build-pc -G "Visual Studio 17 2022" -A x64
fi
step "PC build" "$CMAKE" --build build-pc --config Debug
step "Tests" ./build-pc/Debug/slider3ds_tests.exe
tail -1 "$LOG"
step "3DS dev build" /c/devkitPro/msys2/usr/bin/bash.exe -lc "cd '$ROOT' && make"
step "3DS release build" /c/devkitPro/msys2/usr/bin/bash.exe -lc "cd '$ROOT' && make RELEASE=1"
echo "== All checks passed"
