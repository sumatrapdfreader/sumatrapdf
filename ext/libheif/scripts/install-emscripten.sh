#!/bin/sh
set -e

VERSION=$1
TARGET=$2

if [ -z "$VERSION" ] || [ -z "$TARGET" ]; then
    echo "USAGE: $0 <sdk-version> <target-folder>"
    exit 1
fi

LIBSTDC_BASE=http://de.archive.ubuntu.com/ubuntu/pool/main/g/gcc-5
EMSDK_DOWNLOAD=https://github.com/emscripten-core/emsdk.git

CODENAME=$(/usr/bin/lsb_release --codename --short)
if [ "$CODENAME" = "trusty" ] && [ ! -e "/usr/lib/x86_64-linux-gnu/libstdc++.so.6.0.21" ]; then
    CONTENTS=$(curl --location $LIBSTDC_BASE)
    LIBSTDC_VERSION=$(echo $CONTENTS | sed 's|.*libstdc++6_\([^_]*\)_amd64\.deb.*|\1|g')
    TMPDIR=$(mktemp --directory)
    echo "Installing libstdc++6 $LIBSTDC_VERSION to fix Emscripten ..."
    echo "Extracting in $TMPDIR ..."
    curl "${LIBSTDC_BASE}/libstdc++6_${LIBSTDC_VERSION}_amd64.deb" > "$TMPDIR/libstdc++6_${LIBSTDC_VERSION}_amd64.deb"
    dpkg -x "$TMPDIR/libstdc++6_${LIBSTDC_VERSION}_amd64.deb" "$TMPDIR"
    sudo mv "$TMPDIR/usr/lib/x86_64-linux-gnu/"libstdc++* /usr/lib/x86_64-linux-gnu
    rm -rf "$TMPDIR"
fi

cd "$TARGET"
if [ ! -d emsdk ]; then
    echo "Cloning SDK base system ..."
    git clone --verbose --recursive "$EMSDK_DOWNLOAD" emsdk
fi

cd emsdk
echo "Updating SDK ..."
git pull --verbose

echo "Installing SDK version ${VERSION} ..."
./emsdk install sdk-fastcomp-${VERSION}-64bit

echo "Activating SDK version ${VERSION} ..."
./emsdk activate sdk-fastcomp-${VERSION}-64bit
