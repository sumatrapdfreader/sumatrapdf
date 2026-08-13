#!/bin/sh
# Install Ubuntu/Debian packages needed to build SumatraPDF on Linux
# (bun cmd/build-linux.ts).
#
# Usage:
#   sudo sh cmd/ubuntu-install-deps.sh
#
# Runs apt-get update then a non-interactive apt-get install.

set -eu

if [ "$(uname -s)" != "Linux" ]; then
  echo "This script must be run on Ubuntu/Debian (got $(uname -s))" >&2
  exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
  echo "This script must be run as root, e.g.:" >&2
  echo "  sudo sh cmd/ubuntu-install-deps.sh" >&2
  exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
  echo "apt-get not found. This script only supports Ubuntu/Debian." >&2
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive

package_exists() {
  apt-cache show "$1" >/dev/null 2>&1
}

# Ubuntu's clang package does not include compiler-rt; match the default clang-N.
clang_rt_package() {
  major=$(apt-cache show clang 2>/dev/null | sed -n 's/^Depends:.*clang-\([0-9][0-9]*\).*/\1/p' | head -n 1)
  if [ -n "$major" ] && package_exists "libclang-rt-${major}-dev"; then
    echo "libclang-rt-${major}-dev"
    return 0
  fi
  if package_exists libclang-rt-dev; then
    echo libclang-rt-dev
  fi
  return 0
}

echo "> apt-get update"
apt-get update

# build-essential: gcc, g++, make, binutils (ar, objcopy)
# clang: preferred compiler for build-linux.ts
# libssl-dev: openssl headers + libcrypto.so (mupdf pkcs7)
# xxd: font embedding on Linux arm64
packages="build-essential clang libssl-dev xxd"

if package_exists libasan8; then
  packages="$packages libasan8"
fi

clang_rt=$(clang_rt_package)
if [ -n "$clang_rt" ]; then
  packages="$packages $clang_rt"
fi

echo "> apt-get install -y --no-install-recommends $packages"
# $packages is a space-separated list of package names
apt-get install -y --no-install-recommends $packages

echo
echo "Installed build dependencies:"
for pkg in $packages; do
  echo "  $pkg"
done
