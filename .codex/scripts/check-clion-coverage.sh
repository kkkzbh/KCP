#!/usr/bin/env bash
set -euo pipefail

repo_root="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/../.." >/dev/null
    pwd
)"
minimum="${CP_CLION_COVERAGE_MIN:-${CP_COVERAGE_MIN:-}}"
report="$repo_root/clion-plugin/build/reports/jacoco/test/jacocoTestReport.xml"

(
    cd "$repo_root/clion-plugin"
    ./gradlew test "$@" --rerun-tasks
    ./gradlew jacocoTestReport -x test --rerun-tasks
)

collector=(
    python3
    "$repo_root/.codex/scripts/collect-jacoco-coverage.py"
    --report "$report"
)

if [[ -n "$minimum" ]]; then
    collector+=(--minimum "$minimum")
fi

"${collector[@]}"
