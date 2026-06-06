#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
usage: scripts/package-linux.sh --build-dir <dir> --version <version> --out-dir <dir>

Build RPM, DEB, and Pacman packages from an already-built KCP CMake tree.
The build tree must contain the kcp and cp_runtime targets and install rules.
EOF
}

build_dir=""
version=""
out_dir=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            build_dir="${2:-}"
            shift 2
            ;;
        --version)
            version="${2:-}"
            shift 2
            ;;
        --out-dir)
            out_dir="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$build_dir" || -z "$version" || -z "$out_dir" ]]; then
    usage >&2
    exit 2
fi

command -v cmake >/dev/null
command -v rpmbuild >/dev/null
command -v dpkg-deb >/dev/null
command -v tar >/dev/null
command -v zstd >/dev/null

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$(cd "$build_dir" && pwd)"
out_dir="$(mkdir -p "$out_dir" && cd "$out_dir" && pwd)"
input_version="${version#v}"
package_version="${input_version%%-*}"
release="1"
if [[ "$input_version" == *-* ]]; then
    release="1.${input_version#*-}"
fi
release="${release//[^A-Za-z0-9._+]/.}"
arch_rpm="$(rpm --eval '%{_arch}')"
arch_deb="amd64"
arch_pacman="x86_64"
work_dir="$repo_root/build/package-linux"
install_root="$work_dir/install-root"

rm -rf "$work_dir"
mkdir -p "$install_root" "$out_dir"

DESTDIR="$install_root" cmake --install "$build_dir" --prefix /usr --strip

if [[ ! -x "$install_root/usr/bin/kcp" ]]; then
    echo "installed kcp executable is missing" >&2
    exit 1
fi
if [[ ! -f "$install_root/usr/lib/kcp/libcp_runtime.a" ]]; then
    echo "installed runtime archive is missing" >&2
    exit 1
fi
if [[ ! -d "$install_root/usr/lib/kcp/std" ]]; then
    echo "installed stdlib directory is missing" >&2
    exit 1
fi

build_rpm() {
    local top="$work_dir/rpmbuild"
    mkdir -p "$top/BUILD" "$top/RPMS" "$top/SOURCES" "$top/SPECS" "$top/SRPMS"
    tar -C "$install_root" -czf "$top/SOURCES/kcp-$package_version-install.tar.gz" .
    cat > "$top/SPECS/kcp.spec" <<EOF
Name: kcp
Version: $package_version
Release: $release%{?dist}
Summary: KCP programming language compiler
License: MIT
URL: https://github.com/kkkzbh/KCP
Source0: kcp-$package_version-install.tar.gz
Requires: clang

%description
KCP is a compiler and standard library for the KCP programming language.

%prep

%build

%install
mkdir -p "%{buildroot}"
tar -xzf "%{SOURCE0}" -C "%{buildroot}"

%files
/usr/bin/kcp
/usr/lib/kcp/libcp_runtime.a
/usr/lib/kcp/std

%changelog
* Sat Jun 06 2026 kkkzbh <kkkzbh@users.noreply.github.com> - $package_version-$release
- Release KCP $package_version.
EOF
    rpmbuild --define "_topdir $top" -bb "$top/SPECS/kcp.spec"
    cp "$top"/RPMS/"$arch_rpm"/kcp-"$package_version"-"$release"*.rpm "$out_dir/"
}

build_deb() {
    local root="$work_dir/deb/kcp_${package_version}_${arch_deb}"
    mkdir -p "$root/DEBIAN"
    cp -a "$install_root"/. "$root/"
    local installed_size
    installed_size="$(du -ks "$root/usr" | awk '{ print $1 }')"
    cat > "$root/DEBIAN/control" <<EOF
Package: kcp
Version: $package_version-$release
Section: devel
Priority: optional
Architecture: $arch_deb
Depends: clang
Installed-Size: $installed_size
Maintainer: kkkzbh <kkkzbh@users.noreply.github.com>
Description: KCP programming language compiler
 KCP is a compiler and standard library for the KCP programming language.
EOF
    dpkg-deb --build --root-owner-group "$root" "$out_dir/kcp_${package_version}-${release}_${arch_deb}.deb"
}

build_pacman() {
    local root="$work_dir/pacman"
    mkdir -p "$root"
    cp -a "$install_root"/. "$root/"
    local size
    size="$(du -sb "$root/usr" | awk '{ print $1 }')"
    cat > "$root/.PKGINFO" <<EOF
pkgname = kcp
pkgbase = kcp
pkgver = $package_version-$release
pkgdesc = KCP programming language compiler
url = https://github.com/kkkzbh/KCP
builddate = $(date +%s)
packager = kkkzbh <kkkzbh@users.noreply.github.com>
size = $size
arch = $arch_pacman
license = MIT
depend = clang
EOF
    tar --zstd -C "$root" -cf "$out_dir/kcp-$package_version-$release-$arch_pacman.pkg.tar.zst" .PKGINFO usr
}

build_rpm
build_deb
build_pacman

find "$out_dir" -maxdepth 1 -type f -print | sort
