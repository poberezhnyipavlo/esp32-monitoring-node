#!/usr/bin/env bash
# clang-tidy with the pinned version from requirements-dev.txt, on host-compiled code only:
# lib/ (strict, .clang-tidy) and test/ (relaxed, test/.clang-tidy).
# src/ is Arduino code and is NOT checked here: it is covered by the esp32dev build.
set -euo pipefail
cd "$(dirname "$0")/.."

# Prefer the pinned tool from .venv; fall back to PATH (CI installs it without a venv).
if [[ -z "${CLANG_TIDY:-}" && -x .venv/bin/clang-tidy ]]; then
    CLANG_TIDY=.venv/bin/clang-tidy
fi
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
LIBDEPS=".pio/libdeps/native"
GTEST_INCLUDE="$LIBDEPS/googletest/googletest/include"
GMOCK_INCLUDE="$LIBDEPS/googletest/googlemock/include"
METRIC_INCLUDE="$LIBDEPS/metric-protocol/include"
for dir in "$GTEST_INCLUDE" "$GMOCK_INCLUDE" "$METRIC_INCLUDE"; do
    if [[ ! -d "$dir" ]]; then
        echo "No $dir: run 'pio pkg install -e native' (or 'pio test -e native') first" >&2
        exit 1
    fi
done

FLAGS=(-std=c++17 -isystem "$METRIC_INCLUDE")
for dir in lib/*/; do FLAGS+=("-I$PWD/${dir%/}"); done  # absolute: HeaderFilterRegex needs /lib/ in the path
if [[ "$(uname)" == "Darwin" ]]; then
    FLAGS+=(-isysroot "$(xcrun --show-sdk-path)")  # non-Apple clang needs the SDK to find libc++
fi

# while read instead of mapfile: macOS ships bash 3.2
LIB_FILES=()
while IFS= read -r f; do LIB_FILES+=("$f"); done < <(git ls-files 'lib/*.cpp')
TEST_FILES=()
while IFS= read -r f; do TEST_FILES+=("$f"); done < <(git ls-files 'test/*.cpp')

echo "$("$CLANG_TIDY" --version | grep -i version)"
if ((${#LIB_FILES[@]} > 0)); then
    "$CLANG_TIDY" --quiet "${LIB_FILES[@]}" -- "${FLAGS[@]}"
fi
"$CLANG_TIDY" --quiet "${TEST_FILES[@]}" -- "${FLAGS[@]}" -isystem "$GTEST_INCLUDE" -isystem "$GMOCK_INCLUDE"
echo "tidy: ${#LIB_FILES[@]} lib + ${#TEST_FILES[@]} test files OK"
