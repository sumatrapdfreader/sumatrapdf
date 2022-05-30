: # This install script was originally taken from libavif but might have been modified.

: # If you want to use a local build of dav1d, you must clone the dav1d repo in this directory first, then enable CMake's AVIF_CODEC_DAV1D and AVIF_LOCAL_DAV1D options.
: # The git SHA below is known to work, and will occasionally be updated. Feel free to use a more recent commit.

: # The odd choice of comment style in this file is to try to share this script between *nix and win32.

: # meson and ninja must be in your PATH.

: # If you're running this on Windows, be sure you've already run this (from your VC2019 install dir):
: #     "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

git clone -b 1.0.0 --depth 1 https://code.videolan.org/videolan/dav1d.git

cd dav1d

: # macOS might require: -Dc_args=-fno-stack-check
: # Build with asan: -Db_sanitize=address
: # Build with ubsan: -Db_sanitize=undefined
meson build --default-library=static --buildtype release --prefix "$(pwd)/dist" $@
ninja -C build
ninja -C build install
cd ..
