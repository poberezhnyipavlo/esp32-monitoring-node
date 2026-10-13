#!/usr/bin/env bash
# clang-tidy with the pinned version from requirements-dev.txt, on host-compiled code only:
# lib/ (strict, .clang-tidy) and test/ (relaxed, test/.clang-tidy).
# src/ is Arduino code and is NOT checked here: it is covered by the esp32dev build.
set -euo pipefail
cd "$(dirname "$0")/.."

CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
GTEST_INCLUDE=".pio/libdeps/native/googletest/googletest/include"
if [[ ! -d "$GTEST_INCLUDE" ]]; then
    echo "No $GTEST_INCLUDE: run 'pio pkg install -e native' (or 'pio test -e native') first" >&2
    exit 1
fi

FLAGS=(-std=c++17)
for dir in lib/*/; do FLAGS+=("-I$PWD/${dir%/}"); done
if [[ "$(uname)" == "Darwin" ]]; then
    FLAGS+=(-isysroot "$(xcrun --show-sdk-path)")  # non-Apple clang needs the SDK to find libc++
fi

LIB_FILES=()
while IFS= read -r file; do LIB_FILES+=("$file"); done < <(git ls-files 'lib/*.cpp')
TEST_FILES=()
while IFS= read -r file; do TEST_FILES+=("$file"); done < <(git ls-files 'test/*.cpp')

if [[ ${#LIB_FILES[@]} -eq 0 || ${#TEST_FILES[@]} -eq 0 ]]; then
    echo "tidy: no lib/ or test/ .cpp files tracked by git (git add them first)" >&2
    exit 1
fi

echo "$("$CLANG_TIDY" --version | grep -i version)"
"$CLANG_TIDY" --quiet "${LIB_FILES[@]}" -- "${FLAGS[@]}"
"$CLANG_TIDY" --quiet "${TEST_FILES[@]}" -- "${FLAGS[@]}" -isystem "$GTEST_INCLUDE"
echo "tidy: ${#LIB_FILES[@]} lib + ${#TEST_FILES[@]} test files OK"
