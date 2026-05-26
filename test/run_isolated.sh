#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 0 ]]; then
    echo "usage: test/run_isolated.sh <command> [args...]" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
log_dir="${repo_root}/build/isolated-logs"
mkdir -p "${log_dir}"

stamp="$(date +%Y%m%d-%H%M%S)"
unit="cp-isolated-${stamp}-$$"
log_file="${log_dir}/${unit}.log"
status_file="${log_dir}/${unit}.status"
memory_max="${CP_ISOLATED_MEMORY_MAX:-12G}"
swap_max="${CP_ISOLATED_SWAP_MAX:-512M}"
detach="${CP_ISOLATED_DETACH:-0}"

run_args=(
    --user
    --unit="${unit}" \
    --description="CP isolated test command" \
    --working-directory="${repo_root}" \
    --collect \
    --expand-environment=no \
    -p MemoryAccounting=yes \
    -p MemoryMax="${memory_max}" \
    -p MemorySwapMax="${swap_max}" \
    -p OOMPolicy=kill \
    -p KillMode=control-group \
    -p TasksMax=512
)

if [[ "${detach}" == "1" ]]; then
    run_args+=(--no-block)
else
    run_args+=(--wait)
fi

set +e
systemd-run "${run_args[@]}" \
    /usr/bin/bash -lc '
        log_file="$1"
        status_file="$2"
        shift 2
        "$@" >"${log_file}" 2>&1
        status=$?
        printf "%s\n" "${status}" >"${status_file}"
        exit "${status}"
    ' bash "${log_file}" "${status_file}" "$@"
systemd_status=$?
set -e

printf "unit=%s\n" "${unit}"
printf "log=%s\n" "${log_file}"
printf "status=%s\n" "${status_file}"
printf "memory_max=%s\n" "${memory_max}"
printf "swap_max=%s\n" "${swap_max}"
printf "detach=%s\n" "${detach}"

if [[ "${detach}" == "1" ]]; then
    exit "${systemd_status}"
fi

command_status="$(cat "${status_file}" 2>/dev/null || printf "%s" "${systemd_status}")"
printf "exit_status=%s\n" "${command_status}"
exit "${command_status}"
