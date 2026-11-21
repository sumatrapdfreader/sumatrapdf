#!/bin/bash
# Builds one ng Linux package inside a distro container: installs the
# toolchain, the GUI library headers and bun, then runs cmd/ng-<format>.ts.
# .github/workflows/ng-linux-release.yml runs it with the checkout mounted.
#
#   bash cmd/ng-linux-ci.sh deb|rpm|arch|tar
#
# Env: SUMATRA_NG_VER pins the version, NG_LINUX_OUT the output dir,
# RUN_TESTS=1 runs the unit tests first.
set -euo pipefail

fmt="${1:?usage: ng-linux-ci.sh deb|rpm|arch|tar}"
bun_version="bun-v1.4.0"

run() { printf '> %s\n' "$*"; "$@"; }

. /etc/os-release
echo "== $PRETTY_NAME, $(uname -m), $(nproc) cpus"

if command -v apt-get >/dev/null; then
  export DEBIAN_FRONTEND=noninteractive
  run apt-get update
  run apt-get install -y --no-install-recommends \
    build-essential pkg-config git unzip curl ca-certificates xz-utils binutils dpkg-dev \
    libx11-dev libxi-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev \
    libfontconfig1-dev libssl-dev libcurl4-openssl-dev
elif command -v dnf >/dev/null; then
  run dnf install -y \
    gcc gcc-c++ pkgconf-pkg-config libstdc++-static git unzip curl xz binutils rpm-build \
    libX11-devel libXi-devel cairo-devel pango-devel gdk-pixbuf2-devel glib2-devel \
    fontconfig-devel openssl-devel libcurl-devel
elif command -v pacman >/dev/null; then
  run pacman -Syu --noconfirm --needed \
    base-devel git unzip curl xz binutils \
    libx11 libxi cairo pango gdk-pixbuf2 glib2 fontconfig openssl
else
  echo "no apt-get, dnf or pacman on $PRETTY_NAME" >&2
  exit 1
fi

run curl -fsSL https://bun.sh/install -o /tmp/bun-install.sh
run bash /tmp/bun-install.sh "$bun_version"
export PATH="$HOME/.bun/bin:$PATH"
run bun --version

# a mounted checkout is owned by nobody git trusts
git config --global --add safe.directory "$PWD"
# gen-docs (run by ng-build.ts) needs the packages of cmd/package.json
(cd cmd && run bun install)

if [ "${RUN_TESTS:-}" = 1 ]; then
  run bun cmd/ng-build.ts -dbg test_util -run -- -for-ai
fi
run bun "cmd/ng-$fmt.ts"
