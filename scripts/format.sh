#!/usr/bin/env bash
# Formatting with the pinned clang-format from requirements-dev.txt.
#   scripts/format.sh          check only (CI): fails if any file is not formatted
#   scripts/format.sh --fix    rewrite files in place
set -euo pipefail
cd "$(dirname "$0")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
FILES=()
while IFS= read -r file; do FILES+=("$file"); done < <(
    git ls-files 'src/*.cpp' 'src/*.h' 'lib/*.cpp' 'lib/*.h' 'test/*.cpp' 'test/*.h' 'include/*.h')
# (mapfile would be shorter, but macOS ships bash 3.2 without it)
if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "format: no C++ files tracked by git (git add them first)" >&2
    exit 1
fi

echo "$("$CLANG_FORMAT" --version)"
if [[ "${1:-}" == "--fix" ]]; then
    "$CLANG_FORMAT" -i "${FILES[@]}"
else
    "$CLANG_FORMAT" --dry-run --Werror "${FILES[@]}"
fi
echo "format: ${#FILES[@]} files OK"
