
# OpenJPEG installation

The build method maintained by OpenJPEG is [CMake](https://cmake.org/).

## UNIX/LINUX - MacOS (terminal) - WINDOWS (cygwin, MinGW)

To build the library, type from source tree directory:
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```
Binaries are then located in the 'bin' directory.

To install the library, type with root privileges:
```
make install
make clean
```

To build the html documentation, you need doxygen to be installed on your system.
It will create an "html" directory in TOP\_LEVEL/build/doc)
```
make doc
```

Main available cmake flags:
  * To specify the install path: '-DCMAKE\_INSTALL\_PREFIX=/path'
  * To build the shared libraries and links the executables against it: '-DBUILD\_SHARED\_LIBS:bool=on' (default: 'ON')
> Note: when using this option, static libraries are not built and executables are dynamically linked.
  * PKG_CONFIG files are by default built for Unix compile, you can force to build on other platforms by adding: '-DBUILD_PKGCONFIG_FILES=on'
  * To build the CODEC executables: '-DBUILD\_CODEC:bool=on' (default: 'ON')
  * To build opjstyle (internal version of astyle) for OpenJPEG development: '-DWITH_ASTYLE=ON'
  * [OBSOLETE] To build the MJ2 executables: '-DBUILD\_MJ2:bool=on' (default: 'OFF')
  * [OBSOLETE] To build the JPWL executables and JPWL library: '-DBUILD\_JPWL:bool=on' (default: 'OFF')
  * [OBSOLETE] To build the JPIP client (java compiler recommended) library and executables: '-DBUILD\_JPIP:bool=on' (default: 'OFF')
  * [OBSOLETE] To build the JPIP server (need fcgi) library and executables: '-DBUILD\_JPIP\_SERVER:bool=on' (default: 'OFF')
  * To enable testing (and automatic result upload to http://my.cdash.org/index.php?project=OPENJPEG):
```
cmake . -DBUILD_TESTING:BOOL=ON -DOPJ_DATA_ROOT:PATH='path/to/the/data/directory' -DBUILDNAME:STRING='name_of_the_build'
make
make Experimental
```
Note : test data is available on the following github repo: https://github.com/uclouvain/openjpeg-data

If '-DOPJ\_DATA\_ROOT:PATH' option is omitted, test files will be automatically searched in '${CMAKE\_SOURCE\_DIR}/../data'.

Note 2 : to execute the encoding test suite, kakadu binaries are needed to decode encoded image and compare it to the baseline. Kakadu binaries are freely available for non-commercial purposes at http://www.kakadusoftware.com. kdu\_expand will need to be in your PATH for cmake to find it.

Note 3 : OpenJPEG encoder and decoder (not the library itself !) depends on several libraries: png, tiff, lcms, z. If these libraries are not found on the system, they are automatically built from the versions available in the source tree. You can force the use of these embedded version with BUILD\_THIRDPARTY:BOOL=ON. On a Debian-like system you can also simply install these libraries with:
```
sudo apt-get install liblcms2-dev  libtiff-dev libpng-dev libz-dev
```

Note 4 : On MacOS, if it does not work, try adding the following flag to the cmake command :
```
-DCMAKE_OSX_ARCHITECTURES:STRING=i386
```

## MacOS (XCode) - WINDOWS (VisualStudio, etc)

You can use cmake to generate the project files for the IDE you are using (VC2010, XCode, etc).
Type `cmake --help` for available generators on your platform.

Examples for Windows with Visual Studio C++ compiler:

If using directly the cl compiler:

```
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE:string="Release" -DBUILD_SHARED_LIBS:bool=on -DCMAKE_INSTALL_PREFIX:path="%USERPROFILE%" -DCMAKE_LIBRARY_PATH:path="%USERPROFILE%" -DCMAKE_INCLUDE_PATH:path="%USERPROFILE%\include" ..
```

To compile a 64-bit application, open 64-Bit Visual C\+\+ toolset on the command line and run cmake. For further information, please refer to: [How to: Enable a 64-Bit Visual C\+\+ Toolset on the Command Line](https://msdn.microsoft.com/en-us/library/x4d2c09s.aspx).


If you do not want directly use the cl compiler, you could use:

```
cmake -DCMAKE_BUILD_TYPE:string="Release" -DBUILD_SHARED_LIBS:bool=on -DCMAKE_INSTALL_PREFIX:path="%USERPROFILE%" -DCMAKE_LIBRARY_PATH:path="%USERPROFILE%" -DCMAKE_INCLUDE_PATH:path="%USERPROFILE%\include" ..
```

To create Visual Studio solution (.sln) and project files (.vcproj / .vcxproj):
```
cmake -G "Visual Studio 14 2015" -DCMAKE_BUILD_TYPE:string="Release" -DBUILD_SHARED_LIBS:bool=on -DCMAKE_INSTALL_PREFIX:path="%USERPROFILE%" -DCMAKE_LIBRARY_PATH:path="%USERPROFILE%" -DCMAKE_INCLUDE_PATH:path="%USERPROFILE%\include" ..
```

64-bit application:
```
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE:string="Release" -DBUILD_SHARED_LIBS:bool=on -DCMAKE_INSTALL_PREFIX:path="%USERPROFILE%" -DCMAKE_LIBRARY_PATH:path="%USERPROFILE%" -DCMAKE_INCLUDE_PATH:path="%USERPROFILE%\include" ..
```


# Enabling CPU specific optimizations

For Intel/AMD processors, OpenJPEG implements optimizations using the SSE4.1
instruction set (for example, for the 9x7 inverse MCT transform) and the AVX2
instruction set (for example, for the 5x3 inverse discrete wavelet transform).
Currently, those optimizations are only available if OpenJPEG is built to
use those instruction sets (and the resulting binary will only run on compatible
CPUs)

With gcc/clang, it is possible to enable those instruction sets with the following :

```
cmake -DCMAKE_C_FLAGS="-O3 -msse4.1 -DNDEBUG" ..
```

```
cmake -DCMAKE_C_FLAGS="-O3 -mavx2 -DNDEBUG" ..
```

(AVX2 implies SSE4.1)

Or if the binary is dedicated to run on the machine where it has
been compiled :

```
cmake -DCMAKE_C_FLAGS="-O3 -march=native -DNDEBUG" ..
```

# Modifying OpenJPEG

Before committing changes, run:
```scripts/prepare-commit.sh```

# Using OpenJPEG

To use openjpeg exported cmake file, simply create your application doing:

```
$ cat CMakeLists.txt
find_package(OpenJPEG REQUIRED)
include_directories(${OPENJPEG_INCLUDE_DIRS})
add_executable(myapp myapp.c)
target_link_libraries(myapp ${OPENJPEG_LIBRARIES})
```
