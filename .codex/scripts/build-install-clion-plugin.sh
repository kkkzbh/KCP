#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root/clion-plugin"

./gradlew buildPlugin

plugin_archive="$(
    find build/distributions -maxdepth 1 -type f -name '*.zip' -printf '%T@ %p\n' |
        sort -nr |
        head -n 1 |
        cut -d' ' -f2-
)"

if [[ -z "$plugin_archive" ]]; then
    echo "failed to find built plugin archive" >&2
    exit 1
fi

plugin_dir="$(
    python3 - "$plugin_archive" <<'PY'
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    for name in archive.namelist():
        if name.strip("/"):
            print(name.split("/", 1)[0])
            break
PY
)"

if [[ -z "$plugin_dir" ]]; then
    echo "failed to infer plugin directory from $plugin_archive" >&2
    exit 1
fi

stale_plugins_dir=""
if [[ -n "${CLION_PLUGINS_DIR:-}" ]]; then
    install_dir="$CLION_PLUGINS_DIR"
else
    clion_selector="${CLION_SELECTOR:-}"
    if [[ -z "$clion_selector" ]]; then
        clion_selector="$(
            ps -eo args |
                sed -nE 's#.*(idea.paths.selector=|JetBrains/)(CLion[0-9]+[.][0-9]+).*#\2#p' |
                head -n 1
        )"
    fi
    if [[ -z "$clion_selector" ]]; then
        clion_selector="$(
            find "$HOME/.local/share/JetBrains" -maxdepth 1 -type d -name 'CLion*' -printf '%f\n' 2>/dev/null |
                sort -V |
                tail -n 1
        )"
    fi
    if [[ -z "$clion_selector" ]]; then
        echo "failed to find a CLion config directory; set CLION_PLUGINS_DIR or CLION_SELECTOR" >&2
        exit 1
    fi
    install_dir="$HOME/.local/share/JetBrains/$clion_selector"
    stale_plugins_dir="$install_dir/plugins"
fi

mkdir -p "$install_dir"
rm -rf "$install_dir/$plugin_dir"
unzip -q "$plugin_archive" -d "$install_dir"

plugin_jar="$(
    find "$install_dir/$plugin_dir/lib" -maxdepth 1 -type f -name '*.jar' ! -name '*searchableOptions.jar' -printf '%f\n' |
        sort |
        head -n 1
)"
if [[ -z "$plugin_jar" ]]; then
    echo "failed to find installed plugin jar under $install_dir/$plugin_dir/lib" >&2
    exit 1
fi

full_plugin_jar="$repo_root/clion-plugin/build/libs/$plugin_jar"
if [[ -f "$full_plugin_jar" ]]; then
    cp "$full_plugin_jar" "$install_dir/$plugin_dir/lib/$plugin_jar"
fi

if ! jar tf "$install_dir/$plugin_dir/lib/$plugin_jar" | grep -qx 'org/cp/lang/clion/CpPlugin.class'; then
    echo "installed plugin jar is missing cp plugin classes: $install_dir/$plugin_dir/lib/$plugin_jar" >&2
    exit 1
fi

if [[ -n "$stale_plugins_dir" && "$stale_plugins_dir" != "$install_dir" ]]; then
    rm -rf "$stale_plugins_dir/$plugin_dir"
fi

echo "installed $plugin_dir from $plugin_archive"
echo "target: $install_dir"
echo "restart CLion to load the updated plugin"
