#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

if [[ ! -f build/CMakeCache.txt ]]; then
    cmake -S . -B build -G Ninja
else
    cmake -S . -B build
fi

cmake --build build --target kcp -j "$(nproc)"

install -Dm755 build/compiler/kcp "$HOME/.local/bin/kcp"
install -Dm644 build/runtime/libcp_runtime.a "$HOME/.local/lib/kcp/libcp_runtime.a"

rm -rf "$HOME/.local/share/kcp/std"
mkdir -p "$HOME/.local/share/kcp"
cp -a std "$HOME/.local/share/kcp/std"

"$HOME/.local/bin/kcp" --help
