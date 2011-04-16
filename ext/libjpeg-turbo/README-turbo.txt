*******************************************************************************
**     Background
*******************************************************************************

libjpeg-turbo is a derivative of libjpeg which uses SIMD instructions (MMX,
SSE2, etc.) to accelerate baseline JPEG compression and decompression on x86
and x86-64 systems.  On such systems, libjpeg-turbo is generally 2-4x as fast
as the unmodified version of libjpeg, all else being equal.

libjpeg-turbo was originally based on libjpeg/SIMD by Miyasaka Masaru, but
the TigerVNC and VirtualGL projects made numerous enhancements to the codec in
2009, including improved support for Mac OS X, 64-bit support, support for
32-bit and big endian pixel formats (RGBX, XBGR, etc.), accelerated Huffman
encoding/decoding, and various bug fixes.  The goal was to produce a fully open
source codec that could replace the partially closed source TurboJPEG/IPP codec
used by VirtualGL and TurboVNC.  libjpeg-turbo generally performs in the range
of 80-120% of TurboJPEG/IPP.  It is faster in some areas but slower in others.

In early 2010, libjpeg-turbo spun off into its own independent project, with
the goal of making high-speed JPEG compression/decompression technology
available to a broader range of users and developers.  The libjpeg-turbo shared
libraries can be used as drop-in replacements for libjpeg on most systems.


*******************************************************************************
**     License
*******************************************************************************

The TurboJPEG/OSS wrapper, as well as some of the optimizations to the Huffman
encoder (jchuff.c) and decoder (jdhuff.c), were borrowed from VirtualGL, and
thus any distribution of libjpeg-turbo which includes those files must, as a
whole, be subject to the terms of the wxWindows Library Licence, Version 3.1.
A copy of this license can be found in this directory under LICENSE.txt.  The
wxWindows Library License is based on the LGPL but includes provisions which
allow the Library to be statically linked into proprietary libraries and
applications without requiring the resulting binaries to be distributed under
the terms of the LGPL.

The rest of the source code, apart from TurboJPEG/OSS and the Huffman codec
optimizations, falls under a less restrictive, BSD-style license (see README.)
You can choose to distribute libjpeg-turbo, as a whole, under this BSD-style
license by simply removing TurboJPEG/OSS and replacing the optimized jchuff.c
and jdhuff.c with their unoptimized counterparts from the libjpeg v6b source.


*******************************************************************************
**     Using libjpeg-turbo
*******************************************************************************

=============================
Replacing libjpeg at Run Time
=============================

If a Unix application is dynamically linked with libjpeg, then you can replace
libjpeg with libjpeg-turbo at run time by manipulating LD_LIBRARY_PATH.
For instance:

  [Using libjpeg]
  > time cjpeg <vgl_5674_0098.ppm >vgl_5674_0098.jpg
  real  0m0.392s
  user  0m0.074s
  sys   0m0.020s

  [Using libjpeg-turbo]
  > export LD_LIBRARY_PATH=/opt/libjpeg-turbo/{lib}:$LD_LIBRARY_PATH
  > time cjpeg <vgl_5674_0098.ppm >vgl_5674_0098.jpg
  real  0m0.109s
  user  0m0.029s
  sys   0m0.010s

NOTE: {lib} can be lib, lib32, lib64, or lib/64, depending on the O/S and
architecture.

System administrators can also replace the libjpeg sym links in /usr/{lib} with
links to the libjpeg dynamic library located in /opt/libjpeg-turbo/{lib}.  This
will effectively accelerate every dynamically linked libjpeg application on the
system.

The libjpeg-turbo SDK for Visual C++ installs the libjpeg-turbo DLL
(jpeg62.dll, jpeg7.dll, or jpeg8.dll, depending on whether libjpeg v6b, v7, or
v8 emulation is enabled) into c:\libjpeg-turbo[64]\bin, and the PATH
environment variable can be modified such that this directory is searched
before any others that might contain a libjpeg DLL.  However, if a libjpeg
DLL exists in an application's install directory, then Windows will load this
DLL first whenever the application is launched.  Thus, if an application ships
with jpeg62.dll, jpeg7.dll, or jpeg8.dll, then back up the application's
version of this DLL and copy c:\libjpeg-turbo[64]\bin\jpeg*.dll into the
application's install directory to accelerate it.

The version of the libjpeg-turbo DLL distributed in the libjpeg-turbo SDK for
Visual C++ requires the Visual C++ 2008 C run time DLL (msvcr90.dll).
msvcr90.dll ships with more recent versions of Windows, but users of older
Windows releases can obtain it from the Visual C++ 2008 Redistributable
Package, which is available as a free download from Microsoft's web site.

NOTE:  Features of libjpeg which require passing a C run time structure, such
as a file handle, from an application to libjpeg will probably not work with
the version of the libjpeg-turbo DLL distributed in the libjpeg-turbo SDK for
Visual C++, unless the application is also built to use the Visual C++ 2008 C
run time DLL.  In particular, this affects jpeg_stdio_dest() and
jpeg_stdio_src().

Mac applications typically embed their own copies of the libjpeg dylib inside
the (hidden) application bundle, so it is not possible to globally replace
libjpeg on OS X systems.  If an application uses a shared library version of
libjpeg, then it may be possible to replace the application's version of it.
This would generally involve copying libjpeg.*.dylib from libjpeg-turbo into
the appropriate place in the application bundle and using install_name_tool to
repoint the dylib to the new directory.  This requires an advanced knowledge of
OS X and would not survive an upgrade or a re-install of the application.
Thus, it is not recommended for most users.

=======================
Replacing TurboJPEG/IPP
=======================

libjpeg-turbo is a drop-in replacement for the TurboJPEG/IPP SDK used by
VirtualGL 2.1.x and TurboVNC 0.6 (and prior.)  libjpeg-turbo contains a wrapper
library (TurboJPEG/OSS) that emulates the TurboJPEG API using libjpeg-turbo
instead of the closed source Intel Performance Primitives.  You can replace the
TurboJPEG/IPP package on Linux systems with the libjpeg-turbo package in order
to make existing releases of VirtualGL 2.1.x and TurboVNC 0.x use the new codec
at run time.  Note that the 64-bit libjpeg-turbo packages contain only 64-bit
binaries, whereas the TurboJPEG/IPP 64-bit packages contained both 64-bit and
32-bit binaries.  Thus, to replace a TurboJPEG/IPP 64-bit package, install
both the 64-bit and 32-bit versions of libjpeg-turbo.

You can also build the VirtualGL 2.1.x and TurboVNC 0.6 source code with
the libjpeg-turbo SDK instead of TurboJPEG/IPP.  It should work identically.
libjpeg-turbo also includes static library versions of TurboJPEG/OSS, which
are used to build TurboVNC 1.0 and later.

========================================
Using libjpeg-turbo in Your Own Programs
========================================

For the most part, libjpeg-turbo should work identically to libjpeg, so in
most cases, an application can be built against libjpeg and then run against
libjpeg-turbo.  On Unix systems (including Cygwin), you can build against
libjpeg-turbo instead of libjpeg by setting

  CPATH=/opt/libjpeg-turbo/include
  and
  LIBRARY_PATH=/opt/libjpeg-turbo/{lib}

({lib} = lib32 or lib64, depending on whether you are building a 32-bit or a
64-bit application.)

If using MinGW, then set

  CPATH=/c/libjpeg-turbo-gcc[64]/include
  and
  LIBRARY_PATH=/c/libjpeg-turbo-gcc[64]/lib

Building against libjpeg-turbo is useful, for instance, if you want to build an
application that leverages the libjpeg-turbo colorspace extensions (see below.)
On Linux and Solaris systems, you would still need to manipulate
LD_LIBRARY_PATH or create appropriate sym links to use libjpeg-turbo at run
time.  On such systems, you can pass -R /opt/libjpeg-turbo/{lib} to the linker
to force the use of libjpeg-turbo at run time rather than libjpeg (also useful
if you want to leverage the colorspace extensions), or you can link against the
libjpeg-turbo static library.

To force a Linux, Solaris, or MinGW application to link against the static
version of libjpeg-turbo, you can use the following linker options:

  -Wl,-Bstatic -ljpeg -Wl,-Bdynamic

On OS X, simply add /opt/libjpeg-turbo/lib/libjpeg.a to the linker command
line (this also works on Linux and Solaris.)

To build Visual C++ applications using libjpeg-turbo, add
c:\libjpeg-turbo[64]\include to the system or user INCLUDE environment
variable and c:\libjpeg-turbo[64]\lib to the system or user LIB environment
variable, and then link against either jpeg.lib (to use the DLL version of
libjpeg-turbo) or jpeg-static.lib (to use the static version of libjpeg-turbo.)

=====================
Colorspace Extensions
=====================

libjpeg-turbo includes extensions which allow JPEG images to be compressed
directly from (and decompressed directly to) buffers which use BGR, BGRX,
RGBX, XBGR, and XRGB pixel ordering.  This is implemented with six new
colorspace constants:

  JCS_EXT_RGB   /* red/green/blue */
  JCS_EXT_RGBX  /* red/green/blue/x */
  JCS_EXT_BGR   /* blue/green/red */
  JCS_EXT_BGRX  /* blue/green/red/x */
  JCS_EXT_XBGR  /* x/blue/green/red */
  JCS_EXT_XRGB  /* x/red/green/blue */

Setting cinfo.in_color_space (compression) or cinfo.out_color_space
(decompression) to one of these values will cause libjpeg-turbo to read the
red, green, and blue values from (or write them to) the appropriate position in
the pixel when YUV conversion is performed.

Your application can check for the existence of these extensions at compile
time with:

  #ifdef JCS_EXTENSIONS

At run time, attempting to use these extensions with a version of libjpeg
that doesn't support them will result in a "Bogus input colorspace" error.

=================================
libjpeg v7 and v8 API/ABI support
=================================

libjpeg v7 and v8 added new features to the API/ABI, and, unfortunately, the
compression and decompression structures were extended in a backward-
incompatible manner to accommodate these features.  Thus, programs which are
built to use libjpeg v7 or v8 did not work with libjpeg-turbo, since it is
based on the libjpeg v6b code base.  Although libjpeg v7 and v8 are still not
as widely used as v6b, enough programs (including a few Linux distros) have
made the switch that it was desirable to provide support for the libjpeg v7/v8
API/ABI in libjpeg-turbo.

Some of the libjpeg v7 and v8 features -- DCT scaling, to name one -- involve
deep modifications to the code which cannot be accommodated by libjpeg-turbo
without either breaking compatibility with libjpeg v6b or producing an
unsupportable mess.  In order to fully support libjpeg v8 with all of its
features, we would have to essentially port the SIMD extensions to the libjpeg
v8 code base and maintain two separate code trees.  We are hesitant to do this
until/unless the newer libjpeg code bases garner more community support and
involvement and until/unless we have some notion of whether future libjpeg
releases will also be backward-incompatible.

By passing an argument of --with-jpeg7 or --with-jpeg8 to configure, or an
argument of -DWITH_JPEG7=1 or -DWITH_JPEG8=1 to cmake, you can build a version
of libjpeg-turbo which emulates the libjpeg v7 or v8 API/ABI, so that programs
which are built against libjpeg v7 or v8 can be run with libjpeg-turbo.  The
following section describes which libjpeg v7+ features are supported and which
aren't.

libjpeg v7 and v8 Features:
---------------------------

Fully supported:

-- cjpeg: Separate quality settings for luminance and chrominance
   Note that the libpjeg v7+ API was extended to accommodate this feature only
   for convenience purposes.  It has always been possible to implement this
   feature with libjpeg v6b (see rdswitch.c for an example.)

-- cjpeg: 32-bit BMP support

-- jpegtran: lossless cropping

-- jpegtran: -perfect option

-- rdjpgcom: -raw option

-- rdjpgcom: locale awareness


Fully supported when using libjpeg v7/v8 emulation:

-- libjpeg: In-memory source and destination managers


Not supported:

-- libjpeg: DCT scaling in compressor
   cinfo.scale_num and cinfo.scale_denom are silently ignored.

-- libjpeg: IDCT scaling extensions in decompressor
   libjpeg-turbo still supports IDCT scaling with scaling factors of 1/2, 1/4,
   and 1/8 (same as libjpeg v6b.)

-- libjpeg: Fancy downsampling in compressor
   cinfo.do_fancy_downsampling is silently ignored.

-- jpegtran: Scaling
   Seems to depend on the DCT scaling feature, which isn't supported.


*******************************************************************************
**     Performance pitfalls
*******************************************************************************

===============
Restart Markers
===============

The optimized Huffman decoder in libjpeg-turbo does not handle restart markers
in a way that makes libjpeg happy, so it is necessary to use the slow Huffman
decoder when decompressing a JPEG image that has restart markers.  This can
cause the decompression performance to drop by as much as 20%, but the
performance will still be much much greater than that of libjpeg v6b.  Many
consumer packages, such as PhotoShop, use restart markers when generating JPEG
images, so images generated by those programs will experience this issue.

===============================================
Fast Integer Forward DCT at High Quality Levels
===============================================

The algorithm used by the SIMD-accelerated quantization function cannot produce
correct results whenever the fast integer forward DCT is used along with a JPEG
quality of 98-100.  Thus, libjpeg-turbo must use the non-SIMD quantization
function in those cases.  This causes performance to drop by as much as 40%.
It is therefore strongly advised that you use the slow integer forward DCT
whenever encoding images with a JPEG quality of 98 or higher.
