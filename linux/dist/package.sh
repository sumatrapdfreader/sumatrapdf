#!/bin/bash
# Build a distributable Linux package for SumatraPDF.
# Produces:
#   out/sumatrapdf-<version>-linux-x86_64.tar.gz   (portable archive)
#   out/SumatraPDF-<version>-x86_64.AppImage        (if appimagetool is available)
#
# Usage: ./linux/dist/package.sh [--release]
#   --release: Build with optimizations (release mode)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERSION=$(grep "version:" "$PROJECT_ROOT/meson.build" | head -1 | sed "s/.*'\(.*\)'.*/\1/")
ARCH=$(uname -m)
BUILD_TYPE="debugoptimized"

if [[ "${1:-}" == "--release" ]]; then
    BUILD_TYPE="release"
fi

BUILDDIR="$PROJECT_ROOT/linux/builddir-dist"
OUTDIR="$PROJECT_ROOT/out"
INSTALL_PREFIX="$BUILDDIR/install"

echo "=== SumatraPDF Linux Package Builder ==="
echo "Version: $VERSION"
echo "Arch:    $ARCH"
echo "Build:   $BUILD_TYPE"
echo ""

# ─── Configure & Build ────────────────────────────────────────────────────────

echo "[1/5] Configuring..."
if [[ -d "$BUILDDIR" && -f "$BUILDDIR/build.ninja" ]]; then
    echo "  Using existing build directory: $BUILDDIR"
else
    meson setup "$BUILDDIR" \
        --prefix=/usr \
        --buildtype="$BUILD_TYPE" \
        --strip \
        -Db_lto=true 2>&1 | tail -3
fi

echo "[2/5] Building..."
meson compile -C "$BUILDDIR" -j"$(nproc)"

echo "[3/5] Installing to staging directory..."
rm -rf "$INSTALL_PREFIX"
DESTDIR="$INSTALL_PREFIX" meson install -C "$BUILDDIR" --no-rebuild 2>&1 | tail -5

# ─── Create tarball ───────────────────────────────────────────────────────────

echo "[4/5] Creating tarball..."
mkdir -p "$OUTDIR"

TARBALL_NAME="sumatrapdf-${VERSION}-linux-${ARCH}"
TARBALL_DIR="$BUILDDIR/$TARBALL_NAME"
rm -rf "$TARBALL_DIR"
mkdir -p "$TARBALL_DIR"

# Copy binary
cp "$INSTALL_PREFIX/usr/bin/sumatrapdf" "$TARBALL_DIR/"

# Copy desktop and icon
cp "$INSTALL_PREFIX/usr/share/applications/sumatrapdf.desktop" "$TARBALL_DIR/"
cp "$INSTALL_PREFIX/usr/share/icons/hicolor/256x256/apps/sumatrapdf.png" "$TARBALL_DIR/"

# Create a simple README
cat > "$TARBALL_DIR/README.txt" << 'EOF'
SumatraPDF for Linux
====================

A fast, lightweight document viewer for PDF, EPUB, XPS, CBZ and more.

Usage:
  ./sumatrapdf [file.pdf] [output.png]

Install (optional):
  sudo cp sumatrapdf /usr/local/bin/
  sudo cp sumatrapdf.desktop /usr/share/applications/
  sudo cp sumatrapdf.png /usr/share/icons/hicolor/256x256/apps/

For more information: https://www.sumatrapdfreader.org
EOF

tar -czf "$OUTDIR/${TARBALL_NAME}.tar.gz" -C "$BUILDDIR" "$TARBALL_NAME"
echo "  -> $OUTDIR/${TARBALL_NAME}.tar.gz"

# ─── Create AppImage (if appimagetool is available) ───────────────────────────

echo "[5/5] Creating AppImage..."

APPIMAGETOOL=""
if command -v appimagetool &>/dev/null; then
    APPIMAGETOOL="appimagetool"
elif [[ -x "$PROJECT_ROOT/tools/appimagetool" ]]; then
    APPIMAGETOOL="$PROJECT_ROOT/tools/appimagetool"
fi

if [[ -n "$APPIMAGETOOL" ]]; then
    APPDIR="$BUILDDIR/AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/bin"
    mkdir -p "$APPDIR/usr/share/applications"
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$APPDIR/usr/share/metainfo"

    cp "$INSTALL_PREFIX/usr/bin/sumatrapdf" "$APPDIR/usr/bin/"
    cp "$INSTALL_PREFIX/usr/share/applications/sumatrapdf.desktop" "$APPDIR/"
    cp "$INSTALL_PREFIX/usr/share/applications/sumatrapdf.desktop" "$APPDIR/usr/share/applications/"
    cp "$INSTALL_PREFIX/usr/share/icons/hicolor/256x256/apps/sumatrapdf.png" "$APPDIR/"
    cp "$INSTALL_PREFIX/usr/share/icons/hicolor/256x256/apps/sumatrapdf.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
    cp "$PROJECT_ROOT/linux/dist/org.sumatrapdf.SumatraPDF.metainfo.xml" "$APPDIR/usr/share/metainfo/"

    # AppRun launcher
    cat > "$APPDIR/AppRun" << 'APPRUN'
#!/bin/bash
SELF="$(readlink -f "$0")"
HERE="${SELF%/*}"
exec "${HERE}/usr/bin/sumatrapdf" "$@"
APPRUN
    chmod +x "$APPDIR/AppRun"

    # Symlink icon
    ln -sf usr/share/icons/hicolor/256x256/apps/sumatrapdf.png "$APPDIR/.DirIcon"

    APPIMAGE_NAME="SumatraPDF-${VERSION}-${ARCH}.AppImage"
    ARCH="$ARCH" "$APPIMAGETOOL" "$APPDIR" "$OUTDIR/$APPIMAGE_NAME" 2>&1 | tail -3
    echo "  -> $OUTDIR/$APPIMAGE_NAME"
else
    echo "  (skipped - appimagetool not found)"
    echo "  Install from: https://github.com/AppImage/appimagetool/releases"
    echo "  Or: wget -O tools/appimagetool https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage && chmod +x tools/appimagetool"
fi

# ─── Summary ──────────────────────────────────────────────────────────────────

echo ""
echo "=== Done ==="
echo "Outputs in: $OUTDIR/"
ls -lh "$OUTDIR"/sumatrapdf-* "$OUTDIR"/SumatraPDF-* 2>/dev/null || true
