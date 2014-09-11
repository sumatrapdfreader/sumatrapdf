This is an attempt to create a mingw-based cross-compilation.

Compilation is run inside Linux VM running under VirtualBox, controlled by
Vagrant.

This is to detect code that is compiler dependent (i.e. assumes things about
MSVC that aren't true for gcc).

There are no plans to publish those build.

How to run it:
* get a Mac
* install latest VirtualBox
* install latest Vagrant
* cd tools/mingw-build
* ./vagrant_build.sh

Notes:
* x86_64-w64-mingw32-gcc is the compiler name for 64-bit builds
# 686-w64-mingw32-gcc is for 32-bit builds
* http://www.blogcompiler.com/2010/07/11/compile-for-windows-on-linux/
