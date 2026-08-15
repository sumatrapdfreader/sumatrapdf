#!/bin/sh
# Install Ubuntu/Debian packages needed to build SumatraPDF on Linux:
#   bun cmd/build.ts -linux          # libraries + test_util / test_engines
#   the GTK 4 viewer (Ubuntu 24.04 / 26.04)
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

# gcc/g++ and clang/clang++: both toolchains (clang++ is in the clang package)
# build-essential: make, binutils (ar, objcopy)
# libssl-dev: openssl headers + libcrypto.so (mupdf pkcs7)
# xxd: font embedding on Linux arm64
packages="build-essential gcc g++ clang libssl-dev xxd"

if package_exists libasan8; then
  packages="$packages libasan8"
fi

clang_rt=$(clang_rt_package)
if [ -n "$clang_rt" ]; then
  packages="$packages $clang_rt"
fi

# pkg-config is a dummy that depends on pkgconf on Ubuntu 24.04+
if package_exists pkgconf; then
  packages="$packages pkgconf"
elif package_exists pkg-config; then
  packages="$packages pkg-config"
fi

# GTK 4 viewer. libgtk-4-dev pulls glib, cairo, pango, graphene, gdk-pixbuf,
# epoxy, wayland, and x11; cairo/pango/glib are also listed so a Gfx backend
# can compile against them without going through gtk4.pc.
gtk_packages="libgtk-4-dev libcairo2-dev libpango1.0-dev libglib2.0-dev"
missing_gtk=""
for pkg in $gtk_packages; do
  if package_exists "$pkg"; then
    packages="$packages $pkg"
  else
    missing_gtk="$missing_gtk $pkg"
  fi
done

echo "> apt-get install -y --no-install-recommends $packages"
# $packages is a space-separated list of package names
apt-get install -y --no-install-recommends $packages

echo
echo "Installed build dependencies:"
for pkg in $packages; do
  echo "  $pkg"
done

if [ -n "$missing_gtk" ]; then
  echo >&2
  echo "Missing GTK 4 packages (need Ubuntu 24.04 / 26.04 or Debian 12+):$missing_gtk" >&2
  exit 1
fi
