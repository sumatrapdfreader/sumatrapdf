This is an attempt to create a mingw-based cross-compilation.

Compilation is run inside Linux VM running under VirtualBox, controlled by
Vagrant.

This is to detect code that is compiler dependent (i.e. assumes things about
MSVC that aren't true for gcc).

There are no plans to publish those build.

There are two options to run the build.

Option 1:
* get access to some recent Linux (I use Ubuntu 14.04) distro e.g.:
 - use VM running under VirtualBox or VMWare
 - or install Linux on a PC
 - or get a cheap VPS e.g from DigitalOcean
* install necessary tools (mingw, Go, etc, see VagrantBootstrap.sh for
  commands needed to install those on Ubuntu)
* go run tools/mingw/build.go to run the build

Option 2:
* get a Mac (might also work on Windows but haven't tried)
* install latest VirtualBox
* install latest Vagrant
* cd tools/mingw-build
* ./vagrant_build.sh

Notes:
* x86_64-w64-mingw32-gcc is the compiler name for 64-bit builds
* i686-w64-mingw32-gcc is for 32-bit builds
* http://www.blogcompiler.com/2010/07/11/compile-for-windows-on-linux/

Template error about 'no argument .. that depend on template parameter':
http://www.linuxquestions.org/questions/programming-9/compile-error-there-are-no-arguments-to-'function'-that-depend-on-a-template-param-931065/
