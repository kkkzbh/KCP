#!/usr/bin/env bash
set -euo pipefail

repo_root="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/../.." >/dev/null
    pwd
)"
build_dir="${CP_COVERAGE_BUILD_DIR:-$repo_root/build-coverage}"
jobs="${CP_COVERAGE_JOBS:-4}"
minimum="${CP_COVERAGE_MIN:-}"
targets="${CP_COVERAGE_TARGETS:-}"

cmake \
    -S "$repo_root" \
    -B "$build_dir" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_C_FLAGS=--coverage -O0 -g" \
    "-DCMAKE_CXX_FLAGS=--coverage -O0 -g" \
    "-DCMAKE_EXE_LINKER_FLAGS=--coverage" \
    "-DCMAKE_SHARED_LINKER_FLAGS=--coverage"

if [[ -n "$targets" ]]; then
    for target in $targets; do
        cmake --build "$build_dir" --target "$target" -j "$jobs"
    done
else
    cmake --build "$build_dir" -j "$jobs"
fi

find "$build_dir" -name '*.gcda' -delete
export CP_COMPILER_TEST_LINK_GCOV=1
ctest --test-dir "$build_dir" --output-on-failure "$@"

collector=(
    python3
    "$repo_root/.codex/scripts/collect-gcov-coverage.py"
    --repo-root "$repo_root"
    --build-dir "$build_dir"
)

if [[ -n "$minimum" ]]; then
    collector+=(--minimum "$minimum")
fi

"${collector[@]}"
