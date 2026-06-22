3.1.4.1
=======

### Significant changes relative to 3.1.4:

1. Fixed multiple issues, some long-standing and some that were regressions
introduced in 3.1.4, that made the CMake package config files non-relocatable
and broke the `--prefix` option to `cmake --install`.


3.1.4
=====

### Significant changes relative to 3.1.3:

1. Fixed an issue in the TurboJPEG 2.x compatibility wrapper whereby, if a
calling program attempted to decompress a lossless JPEG image using
`tjDecompress2()` with decompression scaling, the decompressed image was
unexpectedly unscaled.  This could have led to a buffer overrun if the caller
allocated the packed-pixel destination buffer based on the assumption that the
decompressed image would be scaled down.

2. The SIMD dispatchers now use `getauxval()` or `elf_aux_info()`, if
available, to detect support for Neon and AltiVec instructions on AArch32 and
PowerPC Linux, Android, and *BSD systems.

3. Hardened the libjpeg API against hypothetical applications that may
erroneously set one of the exposed quantization table values to 0 just before
calling `jpeg_start_compress()`.  (This would never happen in a
correctly-written program, because `jpeg_add_quant_table()` clamps all values
less than 1.)

4. Fixed a division-by-zero error that occurred when attempting to use the
jpegtran `-drop` option with a specially-crafted malformed drop image
(specifically an image in which one or more of the quantization table values
was 0.)

5. Fixed an issue in the TurboJPEG API library's data destination manager that
manifested as:

     - a memory leak that occurred if a pre-allocated JPEG destination buffer
was passed to `tj3Compress*()` or `tj3Transform()`, `TJPARAM_NOREALLOC` was
unset, and it was necessary for the library to re-allocate the buffer to
accommodate the destination image, and
     - a potential caller double free that occurred if pre-allocated JPEG
destination buffers were passed to `tj3Transform()`, multiple lossless
transform operations were performed, and it was necessary for the library to
re-allocate the second buffer to accommodate the second destination image.

6. Fixed an issue in `tj3Transform()` whereby, if `TJPARAM_SAVEMARKERS` was set
to 2 or 4, `TJXOPT_COPYNONE` was not specified, an ICC profile was extracted
from the source image, and another ICC profile was associated with the
TurboJPEG instance using `tj3SetICCProfile()`, both profiles were embedded in
the destination image.  The documented API behavior is for `TJXOPT_COPYNONE` to
take precedence over `TJPARAM_SAVEMARKERS` and for `TJPARAM_SAVEMARKERS` to
take precedence over the associated ICC profile.  Thus, `tj3Transform()` now
ignores the associated ICC profile unless `TJXOPT_COPYNONE` is specified or
`TJPARAM_SAVEMARKERS` is set to something other than 2 or 4.

7. Fixed an oversight in the libjpeg API whereby, if a calling application
manually set `cinfo.Ss` (the predictor selection value) to a value less than 1
or greater than 7 after calling `jpeg_enable_lossless()` and prior to calling
`jpeg_start_compress()`, an incorrect (all white) lossless JPEG image was
silently generated.

8. Further hardened the TurboJPEG Java API against hypothetical applications
that may erroneously pass huge values to one of the compression, YUV encoding,
decompression, YUV decoding, or packed-pixel image I/O methods, leading to
signed integer overflow in the JNI wrapper's buffer size checks that rendered
those checks ineffective.


3.1.3
=====

### Significant changes relative to 3.1.2:

1. Hardened the TurboJPEG API against hypothetical applications that may
erroneously call `tj*Compress*()` or `tj*Transform()` with a reused JPEG
destination buffer pointer while specifying a destination buffer size of 0.

2. Hardened the TurboJPEG API against hypothetical applications that may
erroneously set `TJPARAM_LOSSLESS` or `TJPARAM_COLORSPACE` prior to calling
`tj3EncodeYUV*8()` or `tj3CompressFromYUV*8()`.  `tj3EncodeYUV*8()` and
`tj3CompressFromYUV*8()` now ignore `TJPARAM_LOSSLESS` and
`TJPARAM_COLORSPACE`.

3. Hardened the TurboJPEG Java API against hypothetical applications that may
erroneously pass huge X or Y offsets to one of the compression, YUV encoding,
decompression, or YUV decoding methods, leading to signed integer overflow in
the JNI wrapper's buffer size checks that rendered those checks ineffective.

4. Fixed an issue in the TurboJPEG Java API whereby
`TJCompressor.getSourceBuf()` sometimes returned the buffer from a previous
invocation of `TJCompressor.loadSourceImage()` if the target data precision was
changed before the most recent invocation.

5. Fixed an issue in the PPM reader that caused incorrect pixels to be
generated when using `tj3LoadImage*()` or `TJCompressor.loadSourceImage()` to
load a PBMPLUS (PPM/PGM) file into a CMYK buffer with a different data
precision than that of the file.


3.1.2
=====

### Significant changes relative to 3.1.1:

1. Fixed a regression introduced by 3.1 beta1[5] that caused a segfault in
TJBench if `-copy` or `-c` was passed as the last command-line argument.

2. The build system now uses wrappers rather than CMake object libraries to
compile source files for multiple data precisions.  This improves code
readability and facilitates adapting the libjpeg-turbo source code to non-CMake
build systems.

3. Fixed an issue whereby decompressing a 4:2:0 or 4:2:2 JPEG image with merged
upsampling disabled/one-pass color quantization enabled, then reusing the same
API instance to decompress a 4:2:0 or 4:2:2 JPEG image with merged upsampling
enabled/color quantization disabled, caused `jpeg_skip_scanlines()` to use
freed memory.  In practice, the freed memory was not reclaimed before it was
used.  Thus, this issue did not cause a segfault or other user-visible errant
behavior (it was only detectable with ASan), and it did not likely pose a
security risk.

4. The AArch64 (Arm 64-bit) Neon SIMD extensions and accelerated Huffman codec
now support the Arm64EC ABI on Windows, which allows Windows/x64 applications
to call native Arm64 functions when running under the Windows/x64 emulator on
Windows/Arm.


3.1.1
=====

### Significant changes relative to 3.1.0:

1. Hardened the libjpeg API against hypothetical calling applications that may
erroneously change the value of the `data_precision` field in
`jpeg_compress_struct` or `jpeg_decompress_struct` after calling
`jpeg_start_compress()` or `jpeg_start_decompress()`.


3.1.0
=====

### Significant changes relative to 3.1 beta1:

1. Fixed an issue in the TurboJPEG API whereby, when generating a
lossless JPEG image with more than 8 bits per sample, specifying a point
transform value greater than 7 resulted in an error ("Parameter value out of
range") unless `TJPARAM_PRECISION`/`TJ.PARAM_PRECISION` was specified before
`TJPARAM_LOSSLESSPT`/`TJ.PARAM_LOSSLESSPT`.

2. Fixed a regression introduced by 1.4 beta1[3] that prevented
`jpeg_set_defaults()` from resetting the Huffman tables to default (baseline)
values if Huffman table optimization or progressive mode was previously enabled
in the same libjpeg instance.

3. Fixed an issue whereby lossless JPEG compression could not be disabled if it
was previously enabled in a libjpeg or TurboJPEG instance.
`jpeg_set_defaults()` now disables lossless JPEG compression in a libjpeg
instance, and setting `TJPARAM_LOSSLESS`/`TJ.PARAM_LOSSLESS` to `0` now
disables lossless JPEG compression in a TurboJPEG instance.


3.0.90 (3.1 beta1)
==================

### Significant changes relative to 3.0.4:

1. The libjpeg-turbo source tree has been reorganized to make it easier to find
the README files, license information, and build instructions.  The
documentation for the libjpeg API library and associated programs has been
moved into the **doc/** subdirectory, all C source code and headers have been
moved into a new **src/** subdirectory, and test scripts have been moved into a
new **test/** subdirectory.

2. cjpeg no longer allows GIF input files to be converted into
12-bit-per-sample JPEG files.  That was never a useful feature, since GIF
images have at most 256 colors referenced from a palette of 8-bit-per-component
RGB values.

3. Added support for lossless JPEG images with 2 to 15 bits per sample to the
libjpeg and TurboJPEG APIs.  When creating or decompressing a lossless JPEG
image and when loading or saving a PBMPLUS image, functions/methods specific to
8-bit samples now handle 8-bit samples with 2 to 8 bits of data precision
(specified using the `data_precision` field in `jpeg_compress_struct` or
`jpeg_decompress_struct` or using `TJPARAM_PRECISION`/`TJ.PARAM_PRECISION`),
functions/methods specific to 12-bit samples now handle 12-bit samples with 9
to 12 bits of data precision, and functions/methods specific to 16-bit samples
now handle 16-bit samples with 13 to 16 bits of data precision.  Refer to
[libjpeg.txt](doc/libjpeg.txt), [usage.txt](doc/usage.txt), and the TurboJPEG
API documentation for more details.

4. All deprecated constants and methods in the TurboJPEG Java API have been
removed.

5. TJBench command-line arguments are now more consistent with those of cjpeg,
djpeg, and jpegtran.  More specifically:

     - `-copynone` has been replaced with `-copy none`.
     - `-fastdct` has been replaced with `-dct fast`.
     - `-fastupsample` has been replaced with `-nosmooth`.
     - `-hflip` and `-vflip` have been replaced with
`-flip {horizontal|vertical}`.
     - `-limitscans` has been replaced with `-maxscans`, which allows the scan
limit to be specified.
     - `-rgb`, `-bgr`, `-rgbx`, `-bgrx`, `-xbgr`, `-xrgb`, and `-cmyk` have
been replaced with `-pixelformat {rgb|bgr|rgbx|bgrx|xbgr|xrgb|cmyk}`.
     - `-rot90`, `-rot180`, and `-rot270` have been replaced with
`-rotate {90|180|270}`.
     - `-stoponwarning` has been replaced with `-strict`.
     - British spellings for `gray` (`grey`) and `optimize` (`optimise`) are
now allowed.

    The old command-line arguments are deprecated and will be removed in a
future release.  TJBench command-line arguments can now be abbreviated as well.
(Where possible, the abbreviations are the same as those supported by cjpeg,
djpeg, and jpegtran.)

6. Added a new TJBench option (`-pixelformat gray`) that can be used to test
the performance of compressing/decompressing a grayscale JPEG image from/to a
packed-pixel grayscale image.

7. Fixed an issue whereby, if `TJPARAM_NOREALLOC` was set, TurboJPEG
compression and lossless transformation functions ignored the JPEG buffer
size(s) passed to them and assumed that the JPEG buffer(s) had been allocated
to a worst-case size returned by `tj3JPEGBufSize()`.  This behavior was never
documented, although the documentation was unclear regarding whether the JPEG
buffer size should be specified if a JPEG buffer is pre-allocated to a
worst-case size.

8. The TurboJPEG C and Java APIs have been improved in the following ways:

     - New image I/O methods (`TJCompressor.loadSourceImage()` and
`TJDecompressor.saveImage()`) have been added to the Java API.  These methods
work similarly to the `tj3LoadImage*()` and `tj3SaveImage*()` functions in the
C API.
     - The TurboJPEG lossless transformation function and methods now add
restart markers to all destination images if
`TJPARAM_RESTARTBLOCKS`/`TJ.PARAM_RESTARTBLOCKS` or
`TJPARAM_RESTARTROWS`/`TJ.PARAM_RESTARTROWS` is set.
     - New functions/methods (`tj3SetICCProfile()` /
`TJCompressor.setICCProfile()` / `TJTransformer.setICCProfile()` and
`tj3GetICCProfile()` / `TJDecompressor.getICCProfile()`) can be used to embed
and retrieve ICC profiles.
     - A new parameter (`TJPARAM_SAVEMARKERS`/`TJ.PARAM_SAVEMARKERS`) can be
used to specify the types of markers that will be copied from the source image
to the destination image during lossless transformation if
`TJXOPT_COPYNONE`/`TJTransform.OPT_COPYNONE` is not specified.
     - A new convenience function/method (`tj3TransformBufSize()` /
`TJTransformer.bufSize()`) can be used to compute the worst-case destination
buffer size for a given lossless transform, taking into account cropping,
transposition of the width and height, grayscale conversion, and the embedded
or extracted ICC profile.

9. TJExample has been replaced with three programs (TJComp, TJDecomp, and
TJTran) that demonstrate how to approximate the functionality of cjpeg, djpeg,
and jpegtran using the TurboJPEG C and Java APIs.


3.0.4
=====

### Significant changes relative to 3.0.3:

1. Fixed an issue whereby the CPU usage of the default marker processor in the
decompressor grew exponentially with the number of markers.  This caused an
unreasonable slow-down in `jpeg_read_header()` if an application called
`jpeg_save_markers()` to save markers of a particular type and then attempted
to decompress a JPEG image containing an excessive number of markers of that
type.

2. Hardened the default marker processor in the decompressor to guard against
an issue (exposed by 3.0 beta2[6]) whereby attempting to decompress a
specially-crafted malformed JPEG image (specifically an image with a complete
12-bit-per-sample Start Of Frame segment followed by an incomplete
8-bit-per-sample Start Of Frame segment) using buffered-image mode and input
prefetching caused a segfault if the `fill_input_buffer()` method in the
calling application's custom source manager incorrectly returned `FALSE` in
response to a prematurely-terminated JPEG data stream.

3. Fixed an issue in cjpeg whereby, when generating a 12-bit-per-sample or
16-bit-per-sample lossless JPEG image, specifying a point transform value
greater than 7 resulted in an error ("Invalid progressive/lossless parameters")
unless the `-precision` option was specified before the `-lossless` option.

4. Fixed a regression introduced by 3.0.3[3] that made it impossible for
calling applications to generate 12-bit-per-sample arithmetic-coded lossy JPEG
images using the TurboJPEG API.

5. Fixed an error ("Destination buffer is not large enough") that occurred when
attempting to generate a full-color lossless JPEG image using the TurboJPEG
Java API's `byte[] TJCompressor.compress()` method if the value of
`TJ.PARAM_SUBSAMP` was not `TJ.SAMP_444`.

6. Fixed a segfault in djpeg that occurred if a negative width was specified
with the `-crop` option.  Since the cropping region width was read into an
unsigned 32-bit integer, a negative width was interpreted as a very large
value.  With certain negative width and positive left boundary values, the
bounds checks in djpeg and `jpeg_crop_scanline()` overflowed and did not detect
the out-of-bounds width, which caused a buffer overrun in the upsampling or
color conversion routine.  Both bounds checks now use 64-bit integers to guard
against overflow, and djpeg now checks for negative numbers when it parses the
crop specification from the command line.

7. Fixed an issue whereby the TurboJPEG lossless transformation function and
methods checked the specified cropping region against the source image
dimensions and level of chrominance subsampling rather than the destination
image dimensions and level of chrominance subsampling, which caused some
cropping regions to be unduly rejected when performing 90-degree rotation,
270-degree rotation, transposition, transverse transposition, or grayscale
conversion.

8. Fixed a regression, introduced by 3.0 beta2[4], that prevented the
`tjTransform()` backward compatibility function from copying extra markers from
the source image to the destination image.

9. Fixed an issue whereby the TurboJPEG lossless transformation function and
methods did not honor `TJXOPT_COPYNONE`/`TJTransform.OPT_COPYNONE` unless it
was specified for all lossless transforms.


3.0.3
=====

### Significant changes relative to 3.0.2:

1. Fixed an issue in the build system, introduced in 3.0.2, that caused all
libjpeg-turbo components to depend on the Visual C++ run-time DLL when built
with Visual C++ and CMake 3.15 or later, regardless of value of the
`WITH_CRT_DLL` CMake variable.

2. The x86-64 SIMD extensions now include support for Intel Control-flow
Enforcement Technology (CET), which is enabled automatically if CET is enabled
in the C compiler.

3. Fixed a regression introduced by 3.0 beta2[6] that made it impossible for
calling applications to supply custom Huffman tables when generating
12-bit-per-component lossy JPEG images using the libjpeg API.

4. Fixed a segfault that occurred when attempting to use the jpegtran `-drop`
option with a specially-crafted malformed input image or drop image
(specifically an image in which all of the scans contain fewer components than
the number of components specified in the Start Of Frame segment.)


3.0.2
=====

### Significant changes relative to 3.0.1:

1. Fixed a signed integer overflow in the `tj3CompressFromYUV8()`,
`tj3DecodeYUV8()`, `tj3DecompressToYUV8()`, and `tj3EncodeYUV8()` functions,
detected by the Clang and GCC undefined behavior sanitizers, that could be
triggered by setting the `align` parameter to an unreasonably large value.
This issue did not pose a security threat, but removing the warning made it
easier to detect actual security issues, should they arise in the future.

2. Introduced a new parameter (`TJPARAM_MAXMEMORY` in the TurboJPEG C API and
`TJ.PARAM_MAXMEMORY` in the TurboJPEG Java API) and a corresponding TJBench
option (`-maxmemory`) for specifying the maximum amount of memory (in
megabytes) that will be allocated for intermediate buffers, which are used with
progressive JPEG compression and decompression, Huffman table optimization,
lossless JPEG compression, and lossless transformation.  The new parameter and
option serve the same purpose as the `max_memory_to_use` field in the
`jpeg_memory_mgr` struct in the libjpeg API, the `JPEGMEM` environment
variable, and the cjpeg/djpeg/jpegtran `-maxmemory` option.

3. Introduced a new parameter (`TJPARAM_MAXPIXELS` in the TurboJPEG C API and
`TJ.PARAM_MAXPIXELS` in the TurboJPEG Java API) and a corresponding TJBench
option (`-maxpixels`) for specifying the maximum number of pixels that the
decompression, lossless transformation, and packed-pixel image loading
functions/methods will process.

4. Fixed an error ("Unsupported color conversion request") that occurred when
attempting to decompress a 3-component lossless JPEG image without an Adobe
APP14 marker.  The decompressor now assumes that a 3-component lossless JPEG
image without an Adobe APP14 marker uses the RGB colorspace if its component
IDs are 1, 2, and 3.


3.0.1
=====

### Significant changes relative to 3.0.0:

1. The x86-64 SIMD functions now use a standard stack frame, prologue, and
epilogue so that debuggers and profilers can reliably capture backtraces from
within the functions.

2. Fixed two minor issues in the interblock smoothing algorithm that caused
mathematical (but not necessarily perceptible) edge block errors when
decompressing progressive JPEG images exactly two DCT blocks in width or that
use vertical chrominance subsampling.

3. Fixed a regression introduced by 3.0 beta2[6] that, in rare cases, caused
the C Huffman encoder (which is not used by default on x86 and Arm CPUs) to
generate incorrect results if the Neon SIMD extensions were explicitly disabled
at build time (by setting the `WITH_SIMD` CMake variable to `0`) in an AArch64
build of libjpeg-turbo.


3.0.0
=====

### Significant changes relative to 3.0 beta2:

1. The TurboJPEG API now supports 4:4:1 (transposed 4:1:1) chrominance
subsampling, which allows losslessly transposed or rotated 4:1:1 JPEG images to
be losslessly cropped, partially decompressed, or decompressed to planar YUV
images.

2. Fixed various segfaults and buffer overruns (CVE-2023-2804) that occurred
when attempting to decompress various specially-crafted malformed
12-bit-per-component and 16-bit-per-component lossless JPEG images using color
quantization or merged chroma upsampling/color conversion.  The underlying
cause of these issues was that the color quantization and merged chroma
upsampling/color conversion algorithms were not designed with lossless
decompression in mind.  Since libjpeg-turbo explicitly does not support color
conversion when compressing or decompressing lossless JPEG images, merged
chroma upsampling/color conversion never should have been enabled for such
images.  Color quantization is a legacy feature that serves little or no
purpose with lossless JPEG images, so it is also now disabled when
decompressing such images.  (As a result, djpeg can no longer decompress a
lossless JPEG image into a GIF image.)

3. Fixed an oversight in 1.4 beta1[8] that caused various segfaults and buffer
overruns when attempting to decompress various specially-crafted malformed
12-bit-per-component JPEG images using djpeg with both color quantization and
RGB565 color conversion enabled.

4. Fixed an issue whereby `jpeg_crop_scanline()` sometimes miscalculated the
downsampled width for components with 4x2 or 2x4 subsampling factors if
decompression scaling was enabled.  This caused the components to be upsampled
incompletely, which caused the color converter to read from uninitialized
memory.  With 12-bit data precision, this caused a buffer overrun or underrun
and subsequent segfault if the sample value read from uninitialized memory was
outside of the valid sample range.

5. Fixed a long-standing issue whereby the `tj3Transform()` function, when used
with the `TJXOP_TRANSPOSE`, `TJXOP_TRANSVERSE`, `TJXOP_ROT90`, or
`TJXOP_ROT270` transform operation and without automatic JPEG destination
buffer (re)allocation or lossless cropping, computed the worst-case transformed
JPEG image size based on the source image dimensions rather than the
transformed image dimensions.  If a calling program allocated the JPEG
destination buffer based on the transformed image dimensions, as the API
documentation instructs, and attempted to transform a specially-crafted 4:2:2,
4:4:0, 4:1:1, or 4:4:1 JPEG source image containing a large amount of metadata,
the issue caused `tj3Transform()` to overflow the JPEG destination buffer
rather than fail gracefully.  The issue could be worked around by setting
`TJXOPT_COPYNONE`.  Note that, irrespective of this issue, `tj3Transform()`
cannot reliably transform JPEG source images that contain a large amount of
metadata unless automatic JPEG destination buffer (re)allocation is used or
`TJXOPT_COPYNONE` is set.

6. Fixed a regression introduced by 3.0 beta2[6] that prevented the djpeg
`-map` option from working when decompressing 12-bit-per-component lossy JPEG
images.

7. Fixed an issue that caused the C Huffman encoder (which is not used by
default on x86 and Arm CPUs) to read from uninitialized memory when attempting
to transform a specially-crafted malformed arithmetic-coded JPEG source image
into a baseline Huffman-coded JPEG destination image.


2.1.91 (3.0 beta2)
==================

### Significant changes relative to 2.1.5.1:

1. Significantly sped up the computation of optimal Huffman tables.  This
speeds up the compression of tiny images by as much as 2x and provides a
noticeable speedup for images as large as 256x256 when using optimal Huffman
tables.

2. All deprecated fields, constructors, and methods in the TurboJPEG Java API
have been removed.

3. Arithmetic entropy coding is now supported with 12-bit-per-component JPEG
images.

4. Overhauled the TurboJPEG API to address long-standing limitations and to
make the API more extensible and intuitive:

     - All C function names are now prefixed with `tj3`, and all version
suffixes have been removed from the function names.  Future API overhauls will
increment the prefix to `tj4`, etc., thus retaining backward API/ABI
compatibility without versioning each individual function.
     - Stateless boolean flags have been replaced with stateful integer API
parameters, the values of which persist between function calls.  New
functions/methods (`tj3Set()`/`TJCompressor.set()`/`TJDecompressor.set()` and
`tj3Get()`/`TJCompressor.get()`/`TJDecompressor.get()`) can be used to set and
query the value of a particular API parameter.
     - The JPEG quality and subsampling are now implemented using API
parameters rather than stateless function arguments (C) or dedicated set/get
methods (Java.)
     - `tj3DecompressHeader()` now stores all relevant information about the
JPEG image, including the width, height, subsampling type, entropy coding
algorithm, etc., in API parameters rather than returning that information
through pointer arguments.
     - `TJFLAG_LIMITSCANS`/`TJ.FLAG_LIMITSCANS` has been reimplemented as an
API parameter (`TJPARAM_SCANLIMIT`/`TJ.PARAM_SCANLIMIT`) that allows the number
of scans to be specified.
     - Huffman table optimization can now be specified using a new API
parameter (`TJPARAM_OPTIMIZE`/`TJ.PARAM_OPTIMIZE`), a new transform option
(`TJXOPT_OPTIMIZE`/`TJTransform.OPT_OPTIMIZE`), and a new TJBench option
(`-optimize`.)
     - Arithmetic entropy coding can now be specified or queried, using a new
API parameter (`TJPARAM_ARITHMETIC`/`TJ.PARAM_ARITHMETIC`), a new transform
option (`TJXOPT_ARITHMETIC`/`TJTransform.OPT_ARITHMETIC`), and a new TJBench
option (`-arithmetic`.)
     - The restart marker interval can now be specified, using new API
parameters (`TJPARAM_RESTARTROWS`/`TJ.PARAM_RESTARTROWS` and
`TJPARAM_RESTARTBLOCKS`/`TJ.PARAM_RESTARTBLOCKS`) and a new TJBench option
(`-restart`.)
     - Pixel density can now be specified or queried, using new API parameters
(`TJPARAM_XDENSITY`/`TJ.PARAM_XDENSITY`,
`TJPARAM_YDENSITY`/`TJ.PARAM_YDENSITY`, and
`TJPARAM_DENSITYUNITS`/`TJ.PARAM_DENSITYUNITS`.)
     - The accurate DCT/IDCT algorithms are now the default for both
compression and decompression, since the "fast" algorithms are considered to be
a legacy feature.  (The "fast" algorithms do not pass the ISO compliance tests,
and those algorithms are not any faster than the accurate algorithms on modern
x86 CPUs.)
     - All C initialization functions have been combined into a single function
(`tj3Init()`) that accepts an integer argument specifying the subsystems to
initialize.
     - All C functions now use the `const` keyword for pointer arguments that
point to unmodified buffers (and for both dimensions of pointer arguments that
point to sets of unmodified buffers.)
     - All C functions now use `size_t` rather than `unsigned long` to
represent buffer sizes, for compatibility with `malloc()` and to avoid
disparities in the size of `unsigned long` between LP64 (Un*x) and LLP64
(Windows) operating systems.
     - All C buffer size functions now return 0 if an error occurs, rather than
trying to awkwardly return -1 in an unsigned data type (which could easily be
misinterpreted as a very large value.)
     - Decompression scaling is now enabled explicitly, using a new
function/method (`tj3SetScalingFactor()`/`TJDecompressor.setScalingFactor()`),
rather than implicitly using awkward "desired width"/"desired height"
arguments.
     - Partial image decompression has been implemented, using a new
function/method (`tj3SetCroppingRegion()`/`TJDecompressor.setCroppingRegion()`)
and a new TJBench option (`-crop`.)
     - The JPEG colorspace can now be specified explicitly when compressing,
using a new API parameter (`TJPARAM_COLORSPACE`/`TJ.PARAM_COLORSPACE`.)  This
allows JPEG images with the RGB and CMYK colorspaces to be created.
     - TJBench no longer generates error/difference images, since identical
functionality is already available in ImageMagick.
     - JPEG images with unknown subsampling configurations can now be
fully decompressed into packed-pixel images or losslessly transformed (with the
exception of lossless cropping.)  They cannot currently be partially
decompressed or decompressed into planar YUV images.
     - `tj3Destroy()` now silently accepts a NULL handle.
     - `tj3Alloc()` and `tj3Free()` now return/accept void pointers, as
`malloc()` and `free()` do.
     - The C image I/O functions now accept a TurboJPEG instance handle, which
is used to transmit/receive API parameter values and to receive error
information.

5. Added support for 8-bit-per-component, 12-bit-per-component, and
16-bit-per-component lossless JPEG images.  A new libjpeg API function
(`jpeg_enable_lossless()`), TurboJPEG API parameters
(`TJPARAM_LOSSLESS`/`TJ.PARAM_LOSSLESS`,
`TJPARAM_LOSSLESSPSV`/`TJ.PARAM_LOSSLESSPSV`, and
`TJPARAM_LOSSLESSPT`/`TJ.PARAM_LOSSLESSPT`), and a cjpeg/TJBench option
(`-lossless`) can be used to create a lossless JPEG image.  (Decompression of
lossless JPEG images is handled automatically.)  Refer to
[libjpeg.txt](doc/libjpeg.txt), [usage.txt](doc/usage.txt), and the TurboJPEG
API documentation for more details.

6. Added support for 12-bit-per-component (lossy and lossless) and
16-bit-per-component (lossless) JPEG images to the libjpeg and TurboJPEG APIs:

     - The existing `data_precision` field in `jpeg_compress_struct` and
`jpeg_decompress_struct` has been repurposed to enable the creation of
12-bit-per-component and 16-bit-per-component JPEG images or to detect whether
a 12-bit-per-component or 16-bit-per-component JPEG image is being
decompressed.
     - New 12-bit-per-component and 16-bit-per-component versions of
`jpeg_write_scanlines()` and `jpeg_read_scanlines()`, as well as new
12-bit-per-component versions of `jpeg_write_raw_data()`,
`jpeg_skip_scanlines()`, `jpeg_crop_scanline()`, and `jpeg_read_raw_data()`,
provide interfaces for compressing from/decompressing to 12-bit-per-component
and 16-bit-per-component packed-pixel and planar YUV image buffers.
     - New 12-bit-per-component and 16-bit-per-component compression,
decompression, and image I/O functions/methods have been added to the TurboJPEG
API, and a new API parameter (`TJPARAM_PRECISION`/`TJ.PARAM_PRECISION`) can be
used to query the data precision of a JPEG image.  (YUV functions are currently
limited to 8-bit data precision but can be expanded to accommodate 12-bit data
precision in the future, if such is deemed beneficial.)
     - A new cjpeg and TJBench command-line argument (`-precision`) can be used
to create a 12-bit-per-component or 16-bit-per-component JPEG image.
(Decompression and transformation of 12-bit-per-component and
16-bit-per-component JPEG images is handled automatically.)

    Refer to [libjpeg.txt](doc/libjpeg.txt), [usage.txt](doc/usage.txt), and
the TurboJPEG API documentation for more details.


2.1.5.1
=======

### Significant changes relative to 2.1.5:

1. The SIMD dispatchers in libjpeg-turbo 2.1.4 and prior stored the list of
supported SIMD instruction sets in a global variable, which caused an innocuous
race condition whereby the variable could have been initialized multiple times
if `jpeg_start_*compress()` was called simultaneously in multiple threads.
libjpeg-turbo 2.1.5 included an undocumented attempt to fix this race condition
by making the SIMD support variable thread-local.  However, that caused another
issue whereby, if `jpeg_start_*compress()` was called in one thread and
`jpeg_read_*()` or `jpeg_write_*()` was called in a second thread, the SIMD
support variable was never initialized in the second thread.  On x86 systems,
this led the second thread to incorrectly assume that AVX2 instructions were
always available, and when it attempted to use those instructions on older x86
CPUs that do not support them, an illegal instruction error occurred.  The SIMD
dispatchers now ensure that the SIMD support variable is initialized before
dispatching based on its value.


2.1.5
=====

### Significant changes relative to 2.1.4:

1. Fixed issues in the build system whereby, when using the Ninja Multi-Config
CMake generator, a static build of libjpeg-turbo (a build in which
`ENABLE_SHARED` is `0`) could not be installed, a Windows installer could not
be built, and the Java regression tests failed.

2. Fixed a regression introduced by 2.0 beta1[15] that caused a buffer overrun
in the progressive Huffman encoder when attempting to transform a
specially-crafted malformed 12-bit-per-component JPEG image into a progressive
12-bit-per-component JPEG image using a 12-bit-per-component build of
libjpeg-turbo (`-DWITH_12BIT=1`.)  Given that the buffer overrun was fully
contained within the progressive Huffman encoder structure and did not cause a
segfault or other user-visible errant behavior, given that the lossless
transformer (unlike the decompressor) is not generally exposed to arbitrary
data exploits, and given that 12-bit-per-component builds of libjpeg-turbo are
uncommon, this issue did not likely pose a security risk.

3. Fixed an issue whereby, when using a 12-bit-per-component build of
libjpeg-turbo (`-DWITH_12BIT=1`), passing samples with values greater than 4095
or less than 0 to `jpeg_write_scanlines()` caused a buffer overrun or underrun
in the RGB-to-YCbCr color converter.

4. Fixed a floating point exception that occurred when attempting to use the
jpegtran `-drop` and `-trim` options to losslessly transform a
specially-crafted malformed JPEG image.

5. Fixed an issue in `tjBufSizeYUV2()` whereby it returned a bogus result,
rather than throwing an error, if the `align` parameter was not a power of 2.
Fixed a similar issue in `tjCompressFromYUV()` whereby it generated a corrupt
JPEG image in certain cases, rather than throwing an error, if the `align`
parameter was not a power of 2.

6. Fixed an issue whereby `tjDecompressToYUV2()`, which is a wrapper for
`tjDecompressToYUVPlanes()`, used the desired YUV image dimensions rather than
the actual scaled image dimensions when computing the plane pointers and
strides to pass to `tjDecompressToYUVPlanes()`.  This caused a buffer overrun
and subsequent segfault if the desired image dimensions exceeded the scaled
image dimensions.

7. Fixed an issue whereby, when decompressing a 12-bit-per-component JPEG image
(`-DWITH_12BIT=1`) using an alpha-enabled output color space such as
`JCS_EXT_RGBA`, the alpha channel was set to 255 rather than 4095.

8. Fixed an issue whereby the Java version of TJBench did not accept a range of
quality values.

9. Fixed an issue whereby, when `-progressive` was passed to TJBench, the JPEG
input image was not transformed into a progressive JPEG image prior to
decompression.


2.1.4
=====

### Significant changes relative to 2.1.3:

1. Fixed a regression introduced in 2.1.3 that caused build failures with
Visual Studio 2010.

2. The `tjDecompressHeader3()` function in the TurboJPEG C API and the
`TJDecompressor.setSourceImage()` method in the TurboJPEG Java API now accept
"abbreviated table specification" (AKA "tables-only") datastreams, which can be
used to prime the decompressor with quantization and Huffman tables that can be
used when decompressing subsequent "abbreviated image" datastreams.

3. libjpeg-turbo now performs run-time detection of AltiVec instructions on
OS X/PowerPC systems if AltiVec instructions are not enabled at compile time.
This allows both AltiVec-equipped (PowerPC G4 and G5) and non-AltiVec-equipped
(PowerPC G3) CPUs to be supported using the same build of libjpeg-turbo.

4. Fixed an error ("Bogus virtual array access") that occurred when attempting
to decompress a progressive JPEG image with a height less than or equal to one
iMCU (8 * the vertical sampling factor) using buffered-image mode with
interblock smoothing enabled.  This was a regression introduced by
2.1 beta1[6(b)].

5. Fixed two issues that prevented partial image decompression from working
properly with buffered-image mode:

     - Attempting to call `jpeg_crop_scanline()` after
`jpeg_start_decompress()` but before `jpeg_start_output()` resulted in an error
("Improper call to JPEG library in state 207".)
     - Attempting to use `jpeg_skip_scanlines()` resulted in an error ("Bogus
virtual array access") under certain circumstances.


2.1.3
=====

### Significant changes relative to 2.1.2:

1. Fixed a regression introduced by 2.0 beta1[7] whereby cjpeg compressed PGM
input files into full-color JPEG images unless the `-grayscale` option was
used.

2. cjpeg now automatically compresses GIF and 8-bit BMP input files into
grayscale JPEG images if the input files contain only shades of gray.

3. The build system now enables the intrinsics implementation of the AArch64
(Arm 64-bit) Neon SIMD extensions by default when using GCC 12 or later.

4. Fixed a segfault that occurred while decompressing a 4:2:0 JPEG image using
the merged (non-fancy) upsampling algorithms (that is, with
`cinfo.do_fancy_upsampling` set to `FALSE`) along with `jpeg_crop_scanline()`.
Specifically, the segfault occurred if the number of bytes remaining in the
output buffer was less than the number of bytes required to represent one
uncropped scanline of the output image.  For that reason, the issue could only
be reproduced using the libjpeg API, not using djpeg.


2.1.2
=====

### Significant changes relative to 2.1.1:

1. Fixed a regression introduced by 2.1 beta1[13] that caused the remaining
GAS implementations of AArch64 (Arm 64-bit) Neon SIMD functions (which are used
by default with GCC for performance reasons) to be placed in the `.rodata`
section rather than in the `.text` section.  This caused the GNU linker to
automatically place the `.rodata` section in an executable segment, which
prevented libjpeg-turbo from working properly with other linkers and also
represented a potential security risk.

2. Fixed an issue whereby the `tjTransform()` function incorrectly computed the
iMCU size for 4:4:4 JPEG images with non-unary sampling factors and thus unduly
rejected some cropping regions, even though those regions aligned with 8x8 iMCU
boundaries.

3. Fixed a regression introduced by 2.1 beta1[13] that caused the build system
to enable the Arm Neon SIMD extensions when targetting Armv6 and other legacy
architectures that do not support Neon instructions.

4. libjpeg-turbo now performs run-time detection of AltiVec instructions on
FreeBSD/PowerPC systems if AltiVec instructions are not enabled at compile
time.  This allows both AltiVec-equipped and non-AltiVec-equipped CPUs to be
supported using the same build of libjpeg-turbo.

5. cjpeg now accepts a `-strict` argument similar to that of djpeg and
jpegtran, which causes the compressor to abort if an LZW-compressed GIF input
image contains incomplete or corrupt image data.


2.1.1
=====

### Significant changes relative to 2.1.0:

1. Fixed a regression introduced in 2.1.0 that caused build failures with
non-GCC-compatible compilers for Un*x/Arm platforms.

2. Fixed a regression introduced by 2.1 beta1[13] that prevented the Arm 32-bit
(AArch32) Neon SIMD extensions from building unless the C compiler flags
included `-mfloat-abi=softfp` or `-mfloat-abi=hard`.

3. Fixed an issue in the AArch32 Neon SIMD Huffman encoder whereby reliance on
undefined C compiler behavior led to crashes ("SIGBUS: illegal alignment") on
Android systems when running AArch32/Thumb builds of libjpeg-turbo built with
recent versions of Clang.

4. Added a command-line argument (`-copy icc`) to jpegtran that causes it to
copy only the ICC profile markers from the source file and discard any other
metadata.

5. libjpeg-turbo should now build and run on CHERI-enabled architectures, which
use capability pointers that are larger than the size of `size_t`.

6. Fixed a regression (CVE-2021-37972) introduced by 2.1 beta1[5] that caused a
segfault in the 64-bit SSE2 Huffman encoder when attempting to losslessly
transform a specially-crafted malformed JPEG image.


2.1.0
=====

### Significant changes relative to 2.1 beta1:

1. Fixed a regression (CVE-2021-29390) introduced by 2.1 beta1[6(b)] whereby
attempting to decompress certain progressive JPEG images with one or more
component planes of width 8 or less caused a buffer overrun.

2. Fixed a regression introduced by 2.1 beta1[6(b)] whereby attempting to
decompress a specially-crafted malformed progressive JPEG image caused the
block smoothing algorithm to read from uninitialized memory.

3. Fixed an issue in the Arm Neon SIMD Huffman encoders that caused the
encoders to generate incorrect results when using the Clang compiler with
Visual Studio.

4. Fixed a floating point exception (CVE-2021-20205) that occurred when
attempting to compress a specially-crafted malformed GIF image with a specified
image width of 0 using cjpeg.

5. Fixed a regression introduced by 2.0 beta1[15] whereby attempting to
generate a progressive JPEG image on an SSE2-capable CPU using a scan script
containing one or more scans with lengths divisible by 32 and non-zero
successive approximation low bit positions would, under certain circumstances,
result in an error ("Missing Huffman code table entry") and an invalid JPEG
image.

6. Introduced a new flag (`TJFLAG_LIMITSCANS` in the TurboJPEG C API and
`TJ.FLAG_LIMIT_SCANS` in the TurboJPEG Java API) and a corresponding TJBench
command-line argument (`-limitscans`) that causes the TurboJPEG decompression
and transform functions/operations to return/throw an error if a progressive
JPEG image contains an unreasonably large number of scans.  This allows
applications that use the TurboJPEG API to guard against an exploit of the
progressive JPEG format described in the report
["Two Issues with the JPEG Standard"](https://libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf).

7. The PPM reader now throws an error, rather than segfaulting (due to a buffer
overrun, CVE-2021-46822) or generating incorrect pixels, if an application
attempts to use the `tjLoadImage()` function to load a 16-bit binary PPM file
(a binary PPM file with a maximum value greater than 255) into a grayscale
image buffer or to load a 16-bit binary PGM file into an RGB image buffer.

8. Fixed an issue in the PPM reader that caused incorrect pixels to be
generated when using the `tjLoadImage()` function to load a 16-bit binary PPM
file into an extended RGB image buffer.

9. Fixed an issue whereby, if a JPEG buffer was automatically re-allocated by
one of the TurboJPEG compression or transform functions and an error
subsequently occurred during compression or transformation, the JPEG buffer
pointer passed by the application was not updated when the function returned.


2.0.90 (2.1 beta1)
==================

### Significant changes relative to 2.0.6:

1. The build system, x86-64 SIMD extensions, and accelerated Huffman codec now
support the x32 ABI on Linux, which allows for using x86-64 instructions with
32-bit pointers.  The x32 ABI is generally enabled by adding `-mx32` to the
compiler flags.

     Caveats:
     - CMake 3.9.0 or later is required in order for the build system to
automatically detect an x32 build.
     - Java does not support the x32 ABI, and thus the TurboJPEG Java API will
automatically be disabled with x32 builds.

2. Added Loongson MMI SIMD implementations of the RGB-to-grayscale, 4:2:2 fancy
chroma upsampling, 4:2:2 and 4:2:0 merged chroma upsampling/color conversion,
and fast integer DCT/IDCT algorithms.  Relative to libjpeg-turbo 2.0.x, this
speeds up:

     - the compression of RGB source images into grayscale JPEG images by
approximately 20%
     - the decompression of 4:2:2 JPEG images by approximately 40-60% when
using fancy upsampling
     - the decompression of 4:2:2 and 4:2:0 JPEG images by approximately
15-20% when using merged upsampling
     - the compression of RGB source images by approximately 30-45% when using
the fast integer DCT
     - the decompression of JPEG images into RGB destination images by
approximately 2x when using the fast integer IDCT

    The overall decompression speedup for RGB images is now approximately
2.3-3.7x (compared to 2-3.5x with libjpeg-turbo 2.0.x.)

3. 32-bit (Armv7 or Armv7s) iOS builds of libjpeg-turbo are no longer
supported, and the libjpeg-turbo build system can no longer be used to package
such builds.  32-bit iOS apps cannot run in iOS 11 and later, and the App Store
no longer allows them.

4. 32-bit (i386) OS X/macOS builds of libjpeg-turbo are no longer supported,
and the libjpeg-turbo build system can no longer be used to package such
builds.  32-bit Mac applications cannot run in macOS 10.15 "Catalina" and
later, and the App Store no longer allows them.

5. The SSE2 (x86 SIMD) and C Huffman encoding algorithms have been
significantly optimized, resulting in a measured average overall compression
speedup of 12-28% for 64-bit code and 22-52% for 32-bit code on various Intel
and AMD CPUs, as well as a measured average overall compression speedup of
0-23% on platforms that do not have a SIMD-accelerated Huffman encoding
implementation.

6. The block smoothing algorithm that is applied by default when decompressing
progressive Huffman-encoded JPEG images has been improved in the following
ways:

     - The algorithm is now more fault-tolerant.  Previously, if a particular
scan was incomplete, then the smoothing parameters for the incomplete scan
would be applied to the entire output image, including the parts of the image
that were generated by the prior (complete) scan.  Visually, this had the
effect of removing block smoothing from lower-frequency scans if they were
followed by an incomplete higher-frequency scan.  libjpeg-turbo now applies
block smoothing parameters to each iMCU row based on which scan generated the
pixels in that row, rather than always using the block smoothing parameters for
the most recent scan.
     - When applying block smoothing to DC scans, a Gaussian-like kernel with a
5x5 window is used to reduce the "blocky" appearance.

7. Added SIMD acceleration for progressive Huffman encoding on Arm platforms.
This speeds up the compression of full-color progressive JPEGs by about 30-40%
on average (relative to libjpeg-turbo 2.0.x) when using modern Arm CPUs.

8. Added configure-time and run-time auto-detection of Loongson MMI SIMD
instructions, so that the Loongson MMI SIMD extensions can be included in any
MIPS64 libjpeg-turbo build.

9. Added fault tolerance features to djpeg and jpegtran, mainly to demonstrate
methods by which applications can guard against the exploits of the JPEG format
described in the report
["Two Issues with the JPEG Standard"](https://libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf).

     - Both programs now accept a `-maxscans` argument, which can be used to
limit the number of allowable scans in the input file.
     - Both programs now accept a `-strict` argument, which can be used to
treat all warnings as fatal.

10. CMake package config files are now included for both the libjpeg and
TurboJPEG API libraries.  This facilitates using libjpeg-turbo with CMake's
`find_package()` function.  For example:

        find_package(libjpeg-turbo CONFIG REQUIRED)

        add_executable(libjpeg_program libjpeg_program.c)
        target_link_libraries(libjpeg_program PUBLIC libjpeg-turbo::jpeg)

        add_executable(libjpeg_program_static libjpeg_program.c)
        target_link_libraries(libjpeg_program_static PUBLIC
          libjpeg-turbo::jpeg-static)

        add_executable(turbojpeg_program turbojpeg_program.c)
        target_link_libraries(turbojpeg_program PUBLIC
          libjpeg-turbo::turbojpeg)

        add_executable(turbojpeg_program_static turbojpeg_program.c)
        target_link_libraries(turbojpeg_program_static PUBLIC
          libjpeg-turbo::turbojpeg-static)

11. Since the Unisys LZW patent has long expired, cjpeg and djpeg can now
read/write both LZW-compressed and uncompressed GIF files (feature ported from
jpeg-6a and jpeg-9d.)

12. jpegtran now includes the `-wipe` and `-drop` options from jpeg-9a and
jpeg-9d, as well as the ability to expand the image size using the `-crop`
option.  Refer to jpegtran.1 or usage.txt for more details.

13. Added a complete intrinsics implementation of the Arm Neon SIMD extensions,
thus providing SIMD acceleration on Arm platforms for all of the algorithms
that are SIMD-accelerated on x86 platforms.  This new implementation is
significantly faster in some cases than the old GAS implementation--
depending on the algorithms used, the type of CPU core, and the compiler.  GCC,
as of this writing, does not provide a full or optimal set of Neon intrinsics,
so for performance reasons, the default when building libjpeg-turbo with GCC is
to continue using the GAS implementation of the following algorithms:

     - 32-bit RGB-to-YCbCr color conversion
     - 32-bit fast and accurate inverse DCT
     - 64-bit RGB-to-YCbCr and YCbCr-to-RGB color conversion
     - 64-bit accurate forward and inverse DCT
     - 64-bit Huffman encoding

    A new CMake variable (`NEON_INTRINSICS`) can be used to override this
default.

    Since the new intrinsics implementation includes SIMD acceleration
for merged upsampling/color conversion, 1.5.1[5] is no longer necessary and has
been reverted.

14. The Arm Neon SIMD extensions can now be built using Visual Studio.

15. The build system can now be used to generate a universal x86-64 + Armv8
libjpeg-turbo SDK package for both iOS and macOS.


2.0.6
=====

### Significant changes relative to 2.0.5:

1. Fixed "using JNI after critical get" errors that occurred on Android
platforms when using any of the YUV encoding/compression/decompression/decoding
methods in the TurboJPEG Java API.

2. Fixed or worked around multiple issues with `jpeg_skip_scanlines()`:

     - Fixed segfaults (CVE-2020-35538) or "Corrupt JPEG data: premature end of
data segment" errors in `jpeg_skip_scanlines()` that occurred when
decompressing 4:2:2 or 4:2:0 JPEG images using merged (non-fancy)
upsampling/color conversion (that is, when setting `cinfo.do_fancy_upsampling`
to `FALSE`.)  2.0.0[6] was a similar fix, but it did not cover all cases.
     - `jpeg_skip_scanlines()` now throws an error if two-pass color
quantization is enabled.  Two-pass color quantization never worked properly
with `jpeg_skip_scanlines()`, and the issues could not readily be fixed.
     - Fixed an issue whereby `jpeg_skip_scanlines()` always returned 0 when
skipping past the end of an image.

3. The Arm 64-bit (Armv8) Neon SIMD extensions can now be built using MinGW
toolchains targetting Arm64 (AArch64) Windows binaries.

4. Fixed unexpected visual artifacts that occurred when using
`jpeg_crop_scanline()` and interblock smoothing while decompressing only the DC
scan of a progressive JPEG image.

5. Fixed an issue whereby libjpeg-turbo would not build if 12-bit-per-component
JPEG support (`WITH_12BIT`) was enabled along with libjpeg v7 or libjpeg v8
API/ABI emulation (`WITH_JPEG7` or `WITH_JPEG8`.)


2.0.5
=====

### Significant changes relative to 2.0.4:

1. Worked around issues in the MIPS DSPr2 SIMD extensions that caused failures
in the libjpeg-turbo regression tests.  Specifically, the
`jsimd_h2v1_downsample_dspr2()` and `jsimd_h2v2_downsample_dspr2()` functions
in the MIPS DSPr2 SIMD extensions are now disabled until/unless they can be
fixed, and other functions that are incompatible with big endian MIPS CPUs are
disabled when building libjpeg-turbo for such CPUs.

2. Fixed an oversight in the `TJCompressor.compress(int)` method in the
TurboJPEG Java API that caused an error ("java.lang.IllegalStateException: No
source image is associated with this instance") when attempting to use that
method to compress a YUV image.

3. Fixed an issue (CVE-2020-13790) in the PPM reader that caused a buffer
overrun in cjpeg, TJBench, or the `tjLoadImage()` function if one of the values
in a binary PPM/PGM input file exceeded the maximum value defined in the file's
header and that maximum value was less than 255.  libjpeg-turbo 1.5.0 already
included a similar fix for binary PPM/PGM files with maximum values greater
than 255.

4. The TurboJPEG API library's global error handler, which is used in functions
such as `tjBufSize()` and `tjLoadImage()` that do not require a TurboJPEG
instance handle, is now thread-safe on platforms that support thread-local
storage.


2.0.4
=====

### Significant changes relative to 2.0.3:

1. Fixed a regression in the Windows packaging system (introduced by
2.0 beta1[2]) whereby, if both the 64-bit libjpeg-turbo SDK for GCC and the
64-bit libjpeg-turbo SDK for Visual C++ were installed on the same system, only
one of them could be uninstalled.

2. Fixed a signed integer overflow and subsequent segfault (CVE-2019-2201) that
occurred when attempting to decompress images with more than 715827882 pixels
using the 64-bit C version of TJBench.

3. Fixed out-of-bounds write in `tjDecompressToYUV2()` and
`tjDecompressToYUVPlanes()` (sometimes manifesting as a double free) that
occurred when attempting to decompress grayscale JPEG images that were
compressed with a sampling factor other than 1 (for instance, with
`cjpeg -grayscale -sample 2x2`).

4. Fixed a regression introduced by 2.0.2[5] that caused the TurboJPEG API to
incorrectly identify some JPEG images with unusual sampling factors as 4:4:4
JPEG images.  This was known to cause a buffer overflow when attempting to
decompress some such images using `tjDecompressToYUV2()` or
`tjDecompressToYUVPlanes()`.

5. Fixed an issue (CVE-2020-17541), detected by ASan, whereby attempting to
losslessly transform a specially-crafted malformed JPEG image containing an
extremely-high-frequency coefficient block (junk image data that could never be
generated by a legitimate JPEG compressor) could cause the Huffman encoder's
local buffer to be overrun. (Refer to 1.4.0[9] and 1.4beta1[15].)  Given that
the buffer overrun was fully contained within the stack and did not cause a
segfault or other user-visible errant behavior, and given that the lossless
transformer (unlike the decompressor) is not generally exposed to arbitrary
data exploits, this issue did not likely pose a security risk.

6. The Arm 64-bit (Armv8) Neon SIMD assembly code now stores constants in a
separate read-only data section rather than in the text section, to support
execute-only memory layouts.


2.0.3
=====

### Significant changes relative to 2.0.2:

1. Fixed "using JNI after critical get" errors that occurred on Android
platforms when passing invalid arguments to certain methods in the TurboJPEG
Java API.

2. Fixed a regression in the SIMD feature detection code, introduced by
the AVX2 SIMD extensions (2.0 beta1[1]), that was known to cause an illegal
instruction exception, in rare cases, on CPUs that lack support for CPUID leaf
07H (or on which the maximum CPUID leaf has been limited by way of a BIOS
setting.)

3. The 4:4:0 (h1v2) fancy (smooth) chroma upsampling algorithm in the
decompressor now uses a similar bias pattern to that of the 4:2:2 (h2v1) fancy
chroma upsampling algorithm, rounding up or down the upsampled result for
alternate pixels rather than always rounding down.  This ensures that,
regardless of whether a 4:2:2 JPEG image is rotated or transposed prior to
decompression (in the frequency domain) or after decompression (in the spatial
domain), the final image will be similar.

4. Fixed an integer overflow and subsequent segfault (CVE-2019-2201) that
occurred when attempting to compress or decompress images with more than 1
billion pixels using the TurboJPEG API.

5. Fixed a regression introduced by 2.0 beta1[15] whereby attempting to
generate a progressive JPEG image on an SSE2-capable CPU using a scan script
containing one or more scans with lengths divisible by 16 would result in an
error ("Missing Huffman code table entry") and an invalid JPEG image.

6. Fixed an issue whereby `tjDecodeYUV()` and `tjDecodeYUVPlanes()` would throw
an error ("Invalid progressive parameters") or a warning ("Inconsistent
progression sequence") if passed a TurboJPEG instance that was previously used
to decompress a progressive JPEG image.


2.0.2
=====

### Significant changes relative to 2.0.1:

1. Fixed a regression introduced by 2.0.1[5] that prevented a runtime search
path (rpath) from being embedded in the libjpeg-turbo shared libraries and
executables for macOS and iOS.  This caused a fatal error of the form
"dyld: Library not loaded" when attempting to use one of the executables,
unless `DYLD_LIBRARY_PATH` was explicitly set to the location of the
libjpeg-turbo shared libraries.

2. Fixed an integer overflow and subsequent segfault (CVE-2018-20330) that
occurred when attempting to load a BMP file with more than 1 billion pixels
using the `tjLoadImage()` function.

3. Fixed a buffer overrun (CVE-2018-19664) that occurred when attempting to
decompress a specially-crafted malformed JPEG image to a 256-color BMP using
djpeg.

4. Fixed a floating point exception that occurred when attempting to
decompress a specially-crafted malformed JPEG image with a specified image
width or height of 0 using the C version of TJBench.

5. The TurboJPEG API will now decompress 4:4:4 JPEG images with 2x1, 1x2, 3x1,
or 1x3 luminance and chrominance sampling factors.  This is a non-standard way
of specifying 1x subsampling (normally 4:4:4 JPEGs have 1x1 luminance and
chrominance sampling factors), but the JPEG format and the libjpeg API both
allow it.

6. Fixed a regression introduced by 2.0 beta1[7] that caused djpeg to generate
incorrect PPM images when used with the `-colors` option.

7. Fixed an issue whereby a static build of libjpeg-turbo (a build in which
`ENABLE_SHARED` is `0`) could not be installed using the Visual Studio IDE.

8. Fixed a severe performance issue in the Loongson MMI SIMD extensions that
occurred when compressing RGB images whose image rows were not 64-bit-aligned.


2.0.1
=====

### Significant changes relative to 2.0.0:

1. Fixed a regression introduced with the new CMake-based Un*x build system,
whereby jconfig.h could cause compiler warnings of the form
`"HAVE_*_H" redefined` if it was included by downstream Autotools-based
projects that used `AC_CHECK_HEADERS()` to check for the existence of locale.h,
stddef.h, or stdlib.h.

2. The `jsimd_quantize_float_dspr2()` and `jsimd_convsamp_float_dspr2()`
functions in the MIPS DSPr2 SIMD extensions are now disabled at compile time
if the soft float ABI is enabled.  Those functions use instructions that are
incompatible with the soft float ABI.

3. Fixed a regression in the SIMD feature detection code, introduced by
the AVX2 SIMD extensions (2.0 beta1[1]), that caused libjpeg-turbo to crash on
Windows 7 if Service Pack 1 was not installed.

4. Fixed out-of-bounds read in cjpeg that occurred when attempting to compress
a specially-crafted malformed color-index (8-bit-per-sample) Targa file in
which some of the samples (color indices) exceeded the bounds of the Targa
file's color table.

5. Fixed an issue whereby installing a fully static build of libjpeg-turbo
(a build in which `CFLAGS` contains `-static` and `ENABLE_SHARED` is `0`) would
fail with "No valid ELF RPATH or RUNPATH entry exists in the file."


2.0.0
=====

### Significant changes relative to 2.0 beta1:

1. The TurboJPEG API can now decompress CMYK JPEG images that have subsampled M
and Y components (not to be confused with YCCK JPEG images, in which the C/M/Y
components have been transformed into luma and chroma.)   Previously, an error
was generated ("Could not determine subsampling type for JPEG image") when such
an image was passed to `tjDecompressHeader3()`, `tjTransform()`,
`tjDecompressToYUVPlanes()`, `tjDecompressToYUV2()`, or the equivalent Java
methods.

2. Fixed an issue (CVE-2018-11813) whereby a specially-crafted malformed input
file (specifically, a file with a valid Targa header but incomplete pixel data)
would cause cjpeg to generate a JPEG file that was potentially thousands of
times larger than the input file.  The Targa reader in cjpeg was not properly
detecting that the end of the input file had been reached prematurely, so after
all valid pixels had been read from the input, the reader injected dummy pixels
with values of 255 into the JPEG compressor until the number of pixels
specified in the Targa header had been compressed.  The Targa reader in cjpeg
now behaves like the PPM reader and aborts compression if the end of the input
file is reached prematurely.  Because this issue only affected cjpeg and not
the underlying library, and because it did not involve any out-of-bounds reads
or other exploitable behaviors, it was not believed to represent a security
threat.

3. Fixed an issue whereby the `tjLoadImage()` and `tjSaveImage()` functions
would produce a "Bogus message code" error message if the underlying bitmap and
PPM readers/writers threw an error that was specific to the readers/writers
(as opposed to a general libjpeg API error.)

4. Fixed an issue (CVE-2018-1152) whereby a specially-crafted malformed BMP
file, one in which the header specified an image width of 1073741824 pixels,
would trigger a floating point exception (division by zero) in the
`tjLoadImage()` function when attempting to load the BMP file into a
4-component image buffer.

5. Fixed an issue whereby certain combinations of calls to
`jpeg_skip_scanlines()` and `jpeg_read_scanlines()` could trigger an infinite
loop when decompressing progressive JPEG images that use vertical chroma
subsampling (for instance, 4:2:0 or 4:4:0.)

6. Fixed a segfault in `jpeg_skip_scanlines()` that occurred when decompressing
a 4:2:2 or 4:2:0 JPEG image using the merged (non-fancy) upsampling algorithms
(that is, when setting `cinfo.do_fancy_upsampling` to `FALSE`.)

7. The new CMake-based build system will now disable the MIPS DSPr2 SIMD
extensions if it detects that the compiler does not support DSPr2 instructions.

8. Fixed out-of-bounds read in cjpeg (CVE-2018-14498) that occurred when
attempting to compress a specially-crafted malformed color-index
(8-bit-per-sample) BMP file in which some of the samples (color indices)
exceeded the bounds of the BMP file's color table.

9. Fixed a signed integer overflow in the progressive Huffman decoder, detected
by the Clang and GCC undefined behavior sanitizers, that could be triggered by
attempting to decompress a specially-crafted malformed JPEG image.  This issue
did not pose a security threat, but removing the warning made it easier to
detect actual security issues, should they arise in the future.


1.5.90 (2.0 beta1)
==================

### Significant changes relative to 1.5.3:

1. Added AVX2 SIMD implementations of the colorspace conversion, chroma
downsampling and upsampling, integer quantization and sample conversion, and
accurate integer DCT/IDCT algorithms.  When using the accurate integer DCT/IDCT
algorithms on AVX2-equipped CPUs, the compression of RGB images is
approximately 13-36% (avg. 22%) faster (relative to libjpeg-turbo 1.5.x) with
64-bit code and 11-21% (avg. 17%) faster with 32-bit code, and the
decompression of RGB images is approximately 9-35% (avg. 17%) faster with
64-bit code and 7-17% (avg. 12%) faster with 32-bit code.  (As tested on a
3 GHz Intel Core i7.  Actual mileage may vary.)

2. Overhauled the build system to use CMake on all platforms, and removed the
autotools-based build system.  This decision resulted from extensive
discussions within the libjpeg-turbo community.  libjpeg-turbo traditionally
used CMake only for Windows builds, but there was an increasing amount of
demand to extend CMake support to other platforms.  However, because of the
unique nature of our code base (the need to support different assemblers on
each platform, the need for Java support, etc.), providing dual build systems
as other OSS imaging libraries do (including libpng and libtiff) would have
created a maintenance burden.  The use of CMake greatly simplifies some aspects
of our build system, owing to CMake's built-in support for various assemblers,
Java, and unit testing, as well as generally fewer quirks that have to be
worked around in order to implement our packaging system.  Eliminating
autotools puts our project slightly at odds with the traditional practices of
the OSS community, since most "system libraries" tend to be built with
autotools, but it is believed that the benefits of this move outweigh the
risks.  In addition to providing a unified build environment, switching to
CMake allows for the use of various build tools and IDEs that aren't supported
under autotools, including XCode, Ninja, and Eclipse.  It also eliminates the
need to install autotools via MacPorts/Homebrew on OS X and allows
libjpeg-turbo to be configured without the use of a terminal/command prompt.
Extensive testing was conducted to ensure that all features provided by the
autotools-based build system are provided by the new build system.

3. The libjpeg API in this version of libjpeg-turbo now includes two additional
functions, `jpeg_read_icc_profile()` and `jpeg_write_icc_profile()`, that can
be used to extract ICC profile data from a JPEG file while decompressing or to
embed ICC profile data in a JPEG file while compressing or transforming.  This
eliminates the need for downstream projects, such as color management libraries
and browsers, to include their own glueware for accomplishing this.

4. Improved error handling in the TurboJPEG API library:

     - Introduced a new function (`tjGetErrorStr2()`) in the TurboJPEG C API
that allows compression/decompression/transform error messages to be retrieved
in a thread-safe manner.  Retrieving error messages from global functions, such
as `tjInitCompress()` or `tjBufSize()`, is still thread-unsafe, but since those
functions will only throw errors if passed an invalid argument or if a memory
allocation failure occurs, thread safety is not as much of a concern.
     - Introduced a new function (`tjGetErrorCode()`) in the TurboJPEG C API
and a new method (`TJException.getErrorCode()`) in the TurboJPEG Java API that
can be used to determine the severity of the last
compression/decompression/transform error.  This allows applications to
choose whether to ignore warnings (non-fatal errors) from the underlying
libjpeg API or to treat them as fatal.
     - Introduced a new flag (`TJFLAG_STOPONWARNING` in the TurboJPEG C API and
`TJ.FLAG_STOPONWARNING` in the TurboJPEG Java API) that causes the library to
immediately halt a compression/decompression/transform operation if it
encounters a warning from the underlying libjpeg API (the default behavior is
to allow the operation to complete unless a fatal error is encountered.)

5. Introduced a new flag in the TurboJPEG C and Java APIs (`TJFLAG_PROGRESSIVE`
and `TJ.FLAG_PROGRESSIVE`, respectively) that causes compression and transform
operations to generate progressive JPEG images.  Additionally, a new transform
option (`TJXOPT_PROGRESSIVE` in the C API and `TJTransform.OPT_PROGRESSIVE` in
the Java API) has been introduced, allowing progressive JPEG images to be
generated by selected transforms in a multi-transform operation.

6. Introduced a new transform option in the TurboJPEG API (`TJXOPT_COPYNONE` in
the C API and `TJTransform.OPT_COPYNONE` in the Java API) that allows the
copying of markers (including Exif and ICC profile data) to be disabled for a
particular transform.

7. Added two functions to the TurboJPEG C API (`tjLoadImage()` and
`tjSaveImage()`) that can be used to load/save a BMP or PPM/PGM image to/from a
memory buffer with a specified pixel format and layout.  These functions
replace the project-private (and slow) bmp API, which was previously used by
TJBench, and they also provide a convenient way for first-time users of
libjpeg-turbo to quickly develop a complete JPEG compression/decompression
program.

8. The TurboJPEG C API now includes a new convenience array (`tjAlphaOffset[]`)
that contains the alpha component index for each pixel format (or -1 if the
pixel format lacks an alpha component.)  The TurboJPEG Java API now includes a
new method (`TJ.getAlphaOffset()`) that returns the same value.  In addition,
the `tjRedOffset[]`, `tjGreenOffset[]`, and `tjBlueOffset[]` arrays-- and the
corresponding `TJ.getRedOffset()`, `TJ.getGreenOffset()`, and
`TJ.getBlueOffset()` methods-- now return -1 for `TJPF_GRAY`/`TJ.PF_GRAY`
rather than 0.  This allows programs to easily determine whether a pixel format
has red, green, blue, and alpha components.

9. Added a new example (tjexample.c) that demonstrates the basic usage of the
TurboJPEG C API.  This example mirrors the functionality of TJExample.java.
Both files are now included in the libjpeg-turbo documentation.

10. Fixed two signed integer overflows in the arithmetic decoder, detected by
the Clang undefined behavior sanitizer, that could be triggered by attempting
to decompress a specially-crafted malformed JPEG image.  These issues did not
pose a security threat, but removing the warnings makes it easier to detect
actual security issues, should they arise in the future.

11. Fixed a bug in the merged 4:2:0 upsampling/dithered RGB565 color conversion
algorithm that caused incorrect dithering in the output image.  This algorithm
now produces bitwise-identical results to the unmerged algorithms.

12. The SIMD function symbols for x86[-64]/ELF, MIPS/ELF, macOS/x86[-64] (if
libjpeg-turbo is built with Yasm), and iOS/Arm[64] builds are now private.
This prevents those symbols from being exposed in applications or shared
libraries that link statically with libjpeg-turbo.

13. Added Loongson MMI SIMD implementations of the RGB-to-YCbCr and
YCbCr-to-RGB colorspace conversion, 4:2:0 chroma downsampling, 4:2:0 fancy
chroma upsampling, integer quantization, and accurate integer DCT/IDCT
algorithms.  When using the accurate integer DCT/IDCT, this speeds up the
compression of RGB images by approximately 70-100% and the decompression of RGB
images by approximately 2-3.5x.

14. Fixed a build error when building with older MinGW releases (regression
caused by 1.5.1[7].)

15. Added SIMD acceleration for progressive Huffman encoding on SSE2-capable
x86 and x86-64 platforms.  This speeds up the compression of full-color
progressive JPEGs by about 85-90% on average (relative to libjpeg-turbo 1.5.x)
when using modern Intel and AMD CPUs.


1.5.3
=====

### Significant changes relative to 1.5.2:

1. Fixed a NullPointerException in the TurboJPEG Java wrapper that occurred
when using the YUVImage constructor that creates an instance backed by separate
image planes and allocates memory for the image planes.

2. Fixed an issue whereby the Java version of TJUnitTest would fail when
testing BufferedImage encoding/decoding on big endian systems.

3. Fixed a segfault in djpeg that would occur if an output format other than
PPM/PGM was selected along with the `-crop` option.  The `-crop` option now
works with the GIF and Targa formats as well (unfortunately, it cannot be made
to work with the BMP and RLE formats due to the fact that those output engines
write scanlines in bottom-up order.)  djpeg will now exit gracefully if an
output format other than PPM/PGM, GIF, or Targa is selected along with the
`-crop` option.

4. Fixed an issue (CVE-2017-15232) whereby `jpeg_skip_scanlines()` would
segfault if color quantization was enabled.

5. TJBench (both C and Java versions) will now display usage information if any
command-line argument is unrecognized.  This prevents the program from silently
ignoring typos.

6. Fixed an access violation in tjbench.exe (Windows) that occurred when the
program was used to decompress an existing JPEG image.

7. Fixed an ArrayIndexOutOfBoundsException in the TJExample Java program that
occurred when attempting to decompress a JPEG image that had been compressed
with 4:1:1 chrominance subsampling.

8. Fixed an issue whereby, when using `jpeg_skip_scanlines()` to skip to the
end of a single-scan (non-progressive) image, subsequent calls to
`jpeg_consume_input()` would return `JPEG_SUSPENDED` rather than
`JPEG_REACHED_EOI`.

9. `jpeg_crop_scanline()` now works correctly when decompressing grayscale JPEG
images that were compressed with a sampling factor other than 1 (for instance,
with `cjpeg -grayscale -sample 2x2`).


1.5.2
=====

### Significant changes relative to 1.5.1:

1. Fixed a regression introduced by 1.5.1[7] that prevented libjpeg-turbo from
building with Android NDK platforms prior to android-21 (5.0).

2. Fixed a regression introduced by 1.5.1[1] that prevented the MIPS DSPR2 SIMD
code in libjpeg-turbo from building.

3. Fixed a regression introduced by 1.5 beta1[11] that prevented the Java
version of TJBench from outputting any reference images (the `-nowrite` switch
was accidentally enabled by default.)

4. libjpeg-turbo should now build and run with full AltiVec SIMD acceleration
on PowerPC-based AmigaOS 4 and OpenBSD systems.

5. Fixed build and runtime errors on Windows that occurred when building
libjpeg-turbo with libjpeg v7 API/ABI emulation and the in-memory
source/destination managers.  Due to an oversight, the `jpeg_skip_scanlines()`
and `jpeg_crop_scanline()` functions were not being included in jpeg7.dll when
libjpeg-turbo was built with `-DWITH_JPEG7=1` and `-DWITH_MEMSRCDST=1`.

6. Fixed "Bogus virtual array access" error that occurred when using the
lossless crop feature in jpegtran or the TurboJPEG API, if libjpeg-turbo was
built with libjpeg v7 API/ABI emulation.  This was apparently a long-standing
bug that has existed since the introduction of libjpeg v7/v8 API/ABI emulation
in libjpeg-turbo v1.1.

7. The lossless transform features in jpegtran and the TurboJPEG API will now
always attempt to adjust the Exif image width and height tags if the image size
changed as a result of the transform.  This behavior has always existed when
using libjpeg v8 API/ABI emulation.  It was supposed to be available with
libjpeg v7 API/ABI emulation as well but did not work properly due to a bug.
Furthermore, there was never any good reason not to enable it with libjpeg v6b
API/ABI emulation, since the behavior is entirely internal.  Note that
`-copy all` must be passed to jpegtran in order to transfer the Exif tags from
the source image to the destination image.

8. Fixed several memory leaks in the TurboJPEG API library that could occur
if the library was built with certain compilers and optimization levels
(known to occur with GCC 4.x and clang with `-O1` and higher but not with
GCC 5.x or 6.x) and one of the underlying libjpeg API functions threw an error
after a TurboJPEG API function allocated a local buffer.

9. The libjpeg-turbo memory manager will now honor the `max_memory_to_use`
structure member in jpeg\_memory\_mgr, which can be set to the maximum amount
of memory (in bytes) that libjpeg-turbo should use during decompression or
multi-pass (including progressive) compression.  This limit can also be set
using the `JPEGMEM` environment variable or using the `-maxmemory` switch in
cjpeg/djpeg/jpegtran (refer to the respective man pages for more details.)
This has been a documented feature of libjpeg since v5, but the
`malloc()`/`free()` implementation of the memory manager (jmemnobs.c) never
implemented the feature.  Restricting libjpeg-turbo's memory usage is useful
for two reasons:  it allows testers to more easily work around the 2 GB limit
in libFuzzer, and it allows developers of security-sensitive applications to
more easily defend against one of the progressive JPEG exploits (LJT-01-004)
identified in
[this report](https://libjpeg-turbo.org/pmwiki/uploads/About/TwoIssueswiththeJPEGStandard.pdf).

10. TJBench will now run each benchmark for 1 second prior to starting the
timer, in order to improve the consistency of the results.  Furthermore, the
`-warmup` option is now used to specify the amount of warmup time rather than
the number of warmup iterations.

11. Fixed an error (`short jump is out of range`) that occurred when assembling
the 32-bit x86 SIMD extensions with NASM versions prior to 2.04.  This was a
regression introduced by 1.5 beta1[12].


1.5.1
=====

### Significant changes relative to 1.5.0:

1. Previously, the undocumented `JSIMD_FORCE*` environment variables could be
used to force-enable a particular SIMD instruction set if multiple instruction
sets were available on a particular platform.  On x86 platforms, where CPU
feature detection is bulletproof and multiple SIMD instruction sets are
available, it makes sense for those environment variables to allow forcing the
use of an instruction set only if that instruction set is available.  However,
since the ARM implementations of libjpeg-turbo can only use one SIMD
instruction set, and since their feature detection code is less bulletproof
(parsing /proc/cpuinfo), it makes sense for the `JSIMD_FORCENEON` environment
variable to bypass the feature detection code and really force the use of NEON
instructions.  A new environment variable (`JSIMD_FORCEDSPR2`) was introduced
in the MIPS implementation for the same reasons, and the existing
`JSIMD_FORCENONE` environment variable was extended to that implementation.
These environment variables provide a workaround for those attempting to test
ARM and MIPS builds of libjpeg-turbo in QEMU, which passes through
/proc/cpuinfo from the host system.

2. libjpeg-turbo previously assumed that AltiVec instructions were always
available on PowerPC platforms, which led to "illegal instruction" errors when
running on PowerPC chips that lack AltiVec support (such as the older 7xx/G3
and newer e5500 series.)  libjpeg-turbo now examines /proc/cpuinfo on
Linux/Android systems and enables AltiVec instructions only if the CPU supports
them.  It also now provides two environment variables, `JSIMD_FORCEALTIVEC` and
`JSIMD_FORCENONE`, to force-enable and force-disable AltiVec instructions in
environments where /proc/cpuinfo is an unreliable means of CPU feature
detection (such as when running in QEMU.)  On OS X, libjpeg-turbo continues to
assume that AltiVec support is always available, which means that libjpeg-turbo
cannot be used with G3 Macs unless you set the environment variable
`JSIMD_FORCENONE` to `1`.

3. Fixed an issue whereby 64-bit ARM (AArch64) builds of libjpeg-turbo would
crash when built with recent releases of the Clang/LLVM compiler.  This was
caused by an ABI conformance issue in some of libjpeg-turbo's 64-bit NEON SIMD
routines.  Those routines were incorrectly using 64-bit instructions to
transfer a 32-bit JDIMENSION argument, whereas the ABI allows the upper
(unused) 32 bits of a 32-bit argument's register to be undefined.  The new
Clang/LLVM optimizer uses load combining to transfer multiple adjacent 32-bit
structure members into a single 64-bit register, and this exposed the ABI
conformance issue.

4. Fancy upsampling is now supported when decompressing JPEG images that use
4:4:0 (h1v2) chroma subsampling.  These images are generated when losslessly
rotating or transposing JPEG images that use 4:2:2 (h2v1) chroma subsampling.
The h1v2 fancy upsampling algorithm is not currently SIMD-accelerated.

5. If merged upsampling isn't SIMD-accelerated but YCbCr-to-RGB conversion is,
then libjpeg-turbo will now disable merged upsampling when decompressing YCbCr
JPEG images into RGB or extended RGB output images.  This significantly speeds
up the decompression of 4:2:0 and 4:2:2 JPEGs on ARM platforms if fancy
upsampling is not used (for example, if the `-nosmooth` option to djpeg is
specified.)

6. The TurboJPEG API will now decompress 4:2:2 and 4:4:0 JPEG images with
2x2 luminance sampling factors and 2x1 or 1x2 chrominance sampling factors.
This is a non-standard way of specifying 2x subsampling (normally 4:2:2 JPEGs
have 2x1 luminance and 1x1 chrominance sampling factors, and 4:4:0 JPEGs have
1x2 luminance and 1x1 chrominance sampling factors), but the JPEG format and
the libjpeg API both allow it.

7. Fixed an unsigned integer overflow in the libjpeg memory manager, detected
by the Clang undefined behavior sanitizer, that could be triggered by
attempting to decompress a specially-crafted malformed JPEG image.  This issue
affected only 32-bit code and did not pose a security threat, but removing the
warning makes it easier to detect actual security issues, should they arise in
the future.

8. Fixed additional negative left shifts and other issues reported by the GCC
and Clang undefined behavior sanitizers when attempting to decompress
specially-crafted malformed JPEG images.  None of these issues posed a security
threat, but removing the warnings makes it easier to detect actual security
issues, should they arise in the future.

9. Fixed an out-of-bounds array reference, introduced by 1.4.90[2] (partial
image decompression) and detected by the Clang undefined behavior sanitizer,
that could be triggered by a specially-crafted malformed JPEG image with more
than four components.  Because the out-of-bounds reference was still within the
same structure, it was not known to pose a security threat, but removing the
warning makes it easier to detect actual security issues, should they arise in
the future.

10. Fixed another ABI conformance issue in the 64-bit ARM (AArch64) NEON SIMD
code.  Some of the routines were incorrectly reading and storing data below the
stack pointer, which caused segfaults in certain applications under specific
circumstances.


1.5.0
=====

### Significant changes relative to 1.5 beta1:

1. Fixed an issue whereby a malformed motion-JPEG frame could cause the "fast
path" of libjpeg-turbo's Huffman decoder to read from uninitialized memory.

2. Added libjpeg-turbo version and build information to the global string table
of the libjpeg and TurboJPEG API libraries.  This is a common practice in other
infrastructure libraries, such as OpenSSL and libpng, because it makes it easy
to examine an application binary and determine which version of the library the
application was linked against.

3. Fixed a couple of issues in the PPM reader that would cause buffer overruns
in cjpeg if one of the values in a binary PPM/PGM input file exceeded the
maximum value defined in the file's header and that maximum value was greater
than 255.  libjpeg-turbo 1.4.2 already included a similar fix for ASCII PPM/PGM
files.  Note that these issues were not security bugs, since they were confined
to the cjpeg program and did not affect any of the libjpeg-turbo libraries.

4. Fixed an issue whereby attempting to decompress a JPEG file with a corrupt
header using the `tjDecompressToYUV2()` function would cause the function to
abort without returning an error and, under certain circumstances, corrupt the
stack.  This only occurred if `tjDecompressToYUV2()` was called prior to
calling `tjDecompressHeader3()`, or if the return value from
`tjDecompressHeader3()` was ignored (both cases represent incorrect usage of
the TurboJPEG API.)

5. Fixed an issue in the ARM 32-bit SIMD-accelerated Huffman encoder that
prevented the code from assembling properly with clang.

6. The `jpeg_stdio_src()`, `jpeg_mem_src()`, `jpeg_stdio_dest()`, and
`jpeg_mem_dest()` functions in the libjpeg API will now throw an error if a
source/destination manager has already been assigned to the compress or
decompress object by a different function or by the calling program.  This
prevents these functions from attempting to reuse a source/destination manager
structure that was allocated elsewhere, because there is no way to ensure that
it would be big enough to accommodate the new source/destination manager.


1.4.90 (1.5 beta1)
==================

### Significant changes relative to 1.4.2:

1. Added full SIMD acceleration for PowerPC platforms using AltiVec VMX
(128-bit SIMD) instructions.  Although the performance of libjpeg-turbo on
PowerPC was already good, due to the increased number of registers available
to the compiler vs. x86, it was still possible to speed up compression by about
3-4x and decompression by about 2-2.5x (relative to libjpeg v6b) through the
use of AltiVec instructions.

2. Added two new libjpeg API functions (`jpeg_skip_scanlines()` and
`jpeg_crop_scanline()`) that can be used to partially decode a JPEG image.  See
[libjpeg.txt](doc/libjpeg.txt) for more details.

3. The TJCompressor and TJDecompressor classes in the TurboJPEG Java API now
implement the Closeable interface, so those classes can be used with a
try-with-resources statement.

4. The TurboJPEG Java classes now throw unchecked idiomatic exceptions
(IllegalArgumentException, IllegalStateException) for unrecoverable errors
caused by incorrect API usage, and those classes throw a new checked exception
type (TJException) for errors that are passed through from the C library.

5. Source buffers for the TurboJPEG C API functions, as well as the
`jpeg_mem_src()` function in the libjpeg API, are now declared as const
pointers.  This facilitates passing read-only buffers to those functions and
ensures the caller that the source buffer will not be modified.  This should
not create any backward API or ABI incompatibilities with prior libjpeg-turbo
releases.

6. The MIPS DSPr2 SIMD code can now be compiled to support either FR=0 or FR=1
FPUs.

7. Fixed additional negative left shifts and other issues reported by the GCC
and Clang undefined behavior sanitizers.  Most of these issues affected only
32-bit code, and none of them was known to pose a security threat, but removing
the warnings makes it easier to detect actual security issues, should they
arise in the future.

8. Removed the unnecessary `.arch` directive from the ARM64 NEON SIMD code.
This directive was preventing the code from assembling using the clang
integrated assembler.

9. Fixed a regression caused by 1.4.1[6] that prevented 32-bit and 64-bit
libjpeg-turbo RPMs from being installed simultaneously on recent Red Hat/Fedora
distributions.  This was due to the addition of a macro in jconfig.h that
allows the Huffman codec to determine the word size at compile time.  Since
that macro differs between 32-bit and 64-bit builds, this caused a conflict
between the i386 and x86_64 RPMs (any differing files, other than executables,
are not allowed when 32-bit and 64-bit RPMs are installed simultaneously.)
Since the macro is used only internally, it has been moved into jconfigint.h.

10. The x86-64 SIMD code can now be disabled at run time by setting the
`JSIMD_FORCENONE` environment variable to `1` (the other SIMD implementations
already had this capability.)

11. Added a new command-line argument to TJBench (`-nowrite`) that prevents the
benchmark from outputting any images.  This removes any potential operating
system overhead that might be caused by lazy writes to disk and thus improves
the consistency of the performance measurements.

12. Added SIMD acceleration for Huffman encoding on SSE2-capable x86 and x86-64
platforms.  This speeds up the compression of full-color JPEGs by about 10-15%
on average (relative to libjpeg-turbo 1.4.x) when using modern Intel and AMD
CPUs.  Additionally, this works around an issue in the clang optimizer that
prevents it (as of this writing) from achieving the same performance as GCC
when compiling the C version of the Huffman encoder
(<https://llvm.org/bugs/show_bug.cgi?id=16035>).  For the purposes of
benchmarking or regression testing, SIMD-accelerated Huffman encoding can be
disabled by setting the `JSIMD_NOHUFFENC` environment variable to `1`.

13. Added ARM 64-bit (ARMv8) NEON SIMD implementations of the commonly-used
compression algorithms (including the accurate integer forward DCT and h2v2 &
h2v1 downsampling algorithms, which are not accelerated in the 32-bit NEON
implementation.)  This speeds up the compression of full-color JPEGs by about
75% on average on a Cavium ThunderX processor and by about 2-2.5x on average on
Cortex-A53 and Cortex-A57 cores.

14. Added SIMD acceleration for Huffman encoding on NEON-capable ARM 32-bit
and 64-bit platforms.

    For 32-bit code, this speeds up the compression of full-color JPEGs by
about 30% on average on a typical iOS device (iPhone 4S, Cortex-A9) and by
about 6-7% on average on a typical Android device (Nexus 5X, Cortex-A53 and
Cortex-A57), relative to libjpeg-turbo 1.4.x.  Note that the larger speedup
under iOS is due to the fact that iOS builds use LLVM, which does not optimize
the C Huffman encoder as well as GCC does.

    For 64-bit code, NEON-accelerated Huffman encoding speeds up the
compression of full-color JPEGs by about 40% on average on a typical iOS device
(iPhone 5S, Apple A7) and by about 7-8% on average on a typical Android device
(Nexus 5X, Cortex-A53 and Cortex-A57), in addition to the speedup described in
[13] above.

    For the purposes of benchmarking or regression testing, SIMD-accelerated
Huffman encoding can be disabled by setting the `JSIMD_NOHUFFENC` environment
variable to `1`.

15. pkg-config (.pc) scripts are now included for both the libjpeg and
TurboJPEG API libraries on Un*x systems.  Note that if a project's build system
relies on these scripts, then it will not be possible to build that project
with libjpeg or with a prior version of libjpeg-turbo.

16. Optimized the ARM 64-bit (ARMv8) NEON SIMD decompression routines to
improve performance on CPUs with in-order pipelines.  This speeds up the
decompression of full-color JPEGs by nearly 2x on average on a Cavium ThunderX
processor and by about 15% on average on a Cortex-A53 core.

17. Fixed an issue in the accelerated Huffman decoder that could have caused
the decoder to read past the end of the input buffer when a malformed,
specially-crafted JPEG image was being decompressed.  In prior versions of
libjpeg-turbo, the accelerated Huffman decoder was invoked (in most cases) only
if there were > 128 bytes of data in the input buffer.  However, it is possible
to construct a JPEG image in which a single Huffman block is over 430 bytes
long, so this version of libjpeg-turbo activates the accelerated Huffman
decoder only if there are > 512 bytes of data in the input buffer.

18. Fixed a memory leak in tjunittest encountered when running the program
with the `-yuv` option.


1.4.2
=====

### Significant changes relative to 1.4.1:

1. Fixed an issue whereby cjpeg would segfault if a Windows bitmap with a
negative width or height was used as an input image (Windows bitmaps can have
a negative height if they are stored in top-down order, but such files are
rare and not supported by libjpeg-turbo.)

2. Fixed an issue whereby, under certain circumstances, libjpeg-turbo would
incorrectly encode certain JPEG images when quality=100 and the fast integer
forward DCT were used.  This was known to cause `make test` to fail when the
library was built with `-march=haswell` on x86 systems.

3. Fixed an issue whereby libjpeg-turbo would crash when built with the latest
& greatest development version of the Clang/LLVM compiler.  This was caused by
an x86-64 ABI conformance issue in some of libjpeg-turbo's 64-bit SSE2 SIMD
routines.  Those routines were incorrectly using a 64-bit `mov` instruction to
transfer a 32-bit JDIMENSION argument, whereas the x86-64 ABI allows the upper
(unused) 32 bits of a 32-bit argument's register to be undefined.  The new
Clang/LLVM optimizer uses load combining to transfer multiple adjacent 32-bit
structure members into a single 64-bit register, and this exposed the ABI
conformance issue.

4. Fixed a bug in the MIPS DSPr2 4:2:0 "plain" (non-fancy and non-merged)
upsampling routine that caused a buffer overflow (and subsequent segfault) when
decompressing a 4:2:0 JPEG image whose scaled output width was less than 16
pixels.  The "plain" upsampling routines are normally only used when
decompressing a non-YCbCr JPEG image, but they are also used when decompressing
a JPEG image whose scaled output height is 1.

5. Fixed various negative left shifts and other issues reported by the GCC and
Clang undefined behavior sanitizers.  None of these was known to pose a
security threat, but removing the warnings makes it easier to detect actual
security issues, should they arise in the future.


1.4.1
=====

### Significant changes relative to 1.4.0:

1. tjbench now properly handles CMYK/YCCK JPEG files.  Passing an argument of
`-cmyk` (instead of, for instance, `-rgb`) will cause tjbench to internally
convert the source bitmap to CMYK prior to compression, to generate YCCK JPEG
files, and to internally convert the decompressed CMYK pixels back to RGB after
decompression (the latter is done automatically if a CMYK or YCCK JPEG is
passed to tjbench as a source image.)  The CMYK<->RGB conversion operation is
not benchmarked.  NOTE: The quick & dirty CMYK<->RGB conversions that tjbench
uses are suitable for testing only.  Proper conversion between CMYK and RGB
requires a color management system.

2. `make test` now performs additional bitwise regression tests using tjbench,
mainly for the purpose of testing compression from/decompression to a subregion
of a larger image buffer.

3. `make test` no longer tests the regression of the floating point DCT/IDCT
by default, since the results of those tests can vary if the algorithms in
question are not implemented using SIMD instructions on a particular platform.
See the comments in [Makefile.am](Makefile.am) for information on how to
re-enable the tests and to specify an expected result for them based on the
particulars of your platform.

4. The NULL color conversion routines have been significantly optimized,
which speeds up the compression of RGB and CMYK JPEGs by 5-20% when using
64-bit code and 0-3% when using 32-bit code, and the decompression of those
images by 10-30% when using 64-bit code and 3-12% when using 32-bit code.

5. Fixed an "illegal instruction" error that occurred when djpeg from a
SIMD-enabled libjpeg-turbo MIPS build was executed with the `-nosmooth` option
on a MIPS machine that lacked DSPr2 support.  The MIPS SIMD routines for h2v1
and h2v2 merged upsampling were not properly checking for the existence of
DSPr2.

6. Performance has been improved significantly on 64-bit non-Linux and
non-Windows platforms (generally 10-20% faster compression and 5-10% faster
decompression.)  Due to an oversight, the 64-bit version of the accelerated
Huffman codec was not being compiled in when libjpeg-turbo was built on
platforms other than Windows or Linux.  Oops.

7. Fixed an extremely rare bug in the Huffman encoder that caused 64-bit
builds of libjpeg-turbo to incorrectly encode a few specific test images when
quality=98, an optimized Huffman table, and the accurate integer forward DCT
were used.

8. The Windows (CMake) build system now supports building only static or only
shared libraries.  This is accomplished by adding either `-DENABLE_STATIC=0` or
`-DENABLE_SHARED=0` to the CMake command line.

9. TurboJPEG API functions will now return an error code if a warning is
triggered in the underlying libjpeg API.  For instance, if a JPEG file is
corrupt, the TurboJPEG decompression functions will attempt to decompress
as much of the image as possible, but those functions will now return -1 to
indicate that the decompression was not entirely successful.

10. Fixed a bug in the MIPS DSPr2 4:2:2 fancy upsampling routine that caused a
buffer overflow (and subsequent segfault) when decompressing a 4:2:2 JPEG image
in which the right-most MCU was 5 or 6 pixels wide.


1.4.0
=====

### Significant changes relative to 1.4 beta1:

1. Fixed a build issue on OS X PowerPC platforms (md5cmp failed to build
because OS X does not provide the `le32toh()` and `htole32()` functions.)

2. The non-SIMD RGB565 color conversion code did not work correctly on big
endian machines.  This has been fixed.

3. Fixed an issue in `tjPlaneSizeYUV()` whereby it would erroneously return 1
instead of -1 if `componentID` was > 0 and `subsamp` was `TJSAMP_GRAY`.

3. Fixed an issue in `tjBufSizeYUV2()` whereby it would erroneously return 0
instead of -1 if `width` was < 1.

5. The Huffman encoder now uses `clz` and `bsr` instructions for bit counting
on ARM64 platforms (see 1.4 beta1[5].)

6. The `close()` method in the TJCompressor and TJDecompressor Java classes is
now idempotent.  Previously, that method would call the native `tjDestroy()`
function even if the TurboJPEG instance had already been destroyed.  This
caused an exception to be thrown during finalization, if the `close()` method
had already been called.  The exception was caught, but it was still an
expensive operation.

7. The TurboJPEG API previously generated an error (`Could not determine
subsampling type for JPEG image`) when attempting to decompress grayscale JPEG
images that were compressed with a sampling factor other than 1 (for instance,
with `cjpeg -grayscale -sample 2x2`).  Subsampling technically has no meaning
with grayscale JPEGs, and thus the horizontal and vertical sampling factors
for such images are ignored by the decompressor.  However, the TurboJPEG API
was being too rigid and was expecting the sampling factors to be equal to 1
before it treated the image as a grayscale JPEG.

8. cjpeg, djpeg, and jpegtran now accept an argument of `-version`, which will
print the library version and exit.

9. Referring to 1.4 beta1[15], another extremely rare circumstance was
discovered under which the Huffman encoder's local buffer can be overrun
when a buffered destination manager is being used and an
extremely-high-frequency block (basically junk image data) is being encoded.
Even though the Huffman local buffer was increased from 128 bytes to 136 bytes
to address the previous issue, the new issue caused even the larger buffer to
be overrun.  Further analysis reveals that, in the absolute worst case (such as
setting alternating AC coefficients to 32767 and -32768 in the JPEG scanning
order), the Huffman encoder can produce encoded blocks that approach double the
size of the unencoded blocks.  Thus, the Huffman local buffer was increased to
256 bytes, which should prevent any such issue from re-occurring in the future.

10. The new `tjPlaneSizeYUV()`, `tjPlaneWidth()`, and `tjPlaneHeight()`
functions were not actually usable on any platform except OS X and Windows,
because those functions were not included in the libturbojpeg mapfile.  This
has been fixed.

11. Restored the `JPP()`, `JMETHOD()`, and `FAR` macros in the libjpeg-turbo
header files.  The `JPP()` and `JMETHOD()` macros were originally implemented
in libjpeg as a way of supporting non-ANSI compilers that lacked support for
prototype parameters.  libjpeg-turbo has never supported such compilers, but
some software packages still use the macros to define their own prototypes.
Similarly, libjpeg-turbo has never supported MS-DOS and other platforms that
have far symbols, but some software packages still use the `FAR` macro.  A
pretty good argument can be made that this is a bad practice on the part of the
software in question, but since this affects more than one package, it's just
easier to fix it here.

12. Fixed issues that were preventing the ARM 64-bit SIMD code from compiling
for iOS, and included an ARMv8 architecture in all of the binaries installed by
the "official" libjpeg-turbo SDK for OS X.


1.3.90 (1.4 beta1)
==================

### Significant changes relative to 1.3.1:

1. New features in the TurboJPEG API:

     - YUV planar images can now be generated with an arbitrary line padding
(previously only 4-byte padding, which was compatible with X Video, was
supported.)
     - The decompress-to-YUV function has been extended to support image
scaling.
     - JPEG images can now be compressed from YUV planar source images.
     - YUV planar images can now be decoded into RGB or grayscale images.
     - 4:1:1 subsampling is now supported.  This is mainly included for
compatibility, since 4:1:1 is not fully accelerated in libjpeg-turbo and has no
significant advantages relative to 4:2:0.
     - CMYK images are now supported.  This feature allows CMYK source images
to be compressed to YCCK JPEGs and YCCK or CMYK JPEGs to be decompressed to
CMYK destination images.  Conversion between CMYK/YCCK and RGB or YUV images is
not supported.  Such conversion requires a color management system and is thus
out of scope for a codec library.
     - The handling of YUV images in the Java API has been significantly
refactored and should now be much more intuitive.
     - The Java API now supports encoding a YUV image from an arbitrary
position in a large image buffer.
     - All of the YUV functions now have a corresponding function that operates
on separate image planes instead of a unified image buffer.  This allows for
compressing/decoding from or decompressing/encoding to a subregion of a larger
YUV image.  It also allows for handling YUV formats that swap the order of the
U and V planes.

2. Added SIMD acceleration for DSPr2-capable MIPS platforms.  This speeds up
the compression of full-color JPEGs by 70-80% on such platforms and
decompression by 25-35%.

3. If an application attempts to decompress a Huffman-coded JPEG image whose
header does not contain Huffman tables, libjpeg-turbo will now insert the
default Huffman tables.  In order to save space, many motion JPEG video frames
are encoded without the default Huffman tables, so these frames can now be
successfully decompressed by libjpeg-turbo without additional work on the part
of the application.  An application can still override the Huffman tables, for
instance to re-use tables from a previous frame of the same video.

4. The Mac packaging system now uses pkgbuild and productbuild rather than
PackageMaker (which is obsolete and no longer supported.)  This means that
OS X 10.6 "Snow Leopard" or later must be used when packaging libjpeg-turbo,
although the packages produced can be installed on OS X 10.5 "Leopard" or
later.  OS X 10.4 "Tiger" is no longer supported.

5. The Huffman encoder now uses `clz` and `bsr` instructions for bit counting
on ARM platforms rather than a lookup table.  This reduces the memory footprint
by 64k, which may be important for some mobile applications.  Out of four
Android devices that were tested, two demonstrated a small overall performance
loss (~3-4% on average) with ARMv6 code and a small gain (also ~3-4%) with
ARMv7 code when enabling this new feature, but the other two devices
demonstrated a significant overall performance gain with both ARMv6 and ARMv7
code (~10-20%) when enabling the feature.  Actual mileage may vary.

6. Worked around an issue with Visual C++ 2010 and later that caused incorrect
pixels to be generated when decompressing a JPEG image to a 256-color bitmap,
if compiler optimization was enabled when libjpeg-turbo was built.  This caused
the regression tests to fail when doing a release build under Visual C++ 2010
and later.

7. Improved the accuracy and performance of the non-SIMD implementation of the
floating point inverse DCT (using code borrowed from libjpeg v8a and later.)
The accuracy of this implementation now matches the accuracy of the SSE/SSE2
implementation.  Note, however, that the floating point DCT/IDCT algorithms are
mainly a legacy feature.  They generally do not produce significantly better
accuracy than the accurate integer DCT/IDCT algorithms, and they are quite a
bit slower.

8. Added a new output colorspace (`JCS_RGB565`) to the libjpeg API that allows
for decompressing JPEG images into RGB565 (16-bit) pixels.  If dithering is not
used, then this code path is SIMD-accelerated on ARM platforms.

9. Numerous obsolete features, such as support for non-ANSI compilers and
support for the MS-DOS memory model, were removed from the libjpeg code,
greatly improving its readability and making it easier to maintain and extend.

10. Fixed a segfault that occurred when calling `output_message()` with
`msg_code` set to `JMSG_COPYRIGHT`.

11. Fixed an issue whereby wrjpgcom was allowing comments longer than 65k
characters to be passed on the command line, which was causing it to generate
incorrect JPEG files.

12. Fixed a bug in the build system that was causing the Windows version of
wrjpgcom to be built using the rdjpgcom source code.

13. Restored 12-bit-per-component JPEG support.  A 12-bit version of
libjpeg-turbo can now be built by passing an argument of `--with-12bit` to
configure (Unix) or `-DWITH_12BIT=1` to cmake (Windows.)  12-bit JPEG support
is included only for convenience.  Enabling this feature disables all of the
performance features in libjpeg-turbo, as well as arithmetic coding and the
TurboJPEG API.  The resulting library still contains the other libjpeg-turbo
features (such as the colorspace extensions), but in general, it performs no
faster than libjpeg v6b.

14. Added ARM 64-bit SIMD acceleration for the YCC-to-RGB color conversion
and IDCT algorithms (both are used during JPEG decompression.)  For
reasons (probably related to clang), this code cannot currently be compiled for
iOS.

15. Fixed an extremely rare bug (CVE-2014-9092) that could cause the Huffman
encoder's local buffer to overrun when a very high-frequency MCU is compressed
using quality 100 and no subsampling, and when the JPEG output buffer is being
dynamically resized by the destination manager.  This issue was so rare that,
even with a test program specifically designed to make the bug occur (by
injecting random high-frequency YUV data into the compressor), it was
reproducible only once in about every 25 million iterations.

16. Fixed an oversight in the TurboJPEG C wrapper:  if any of the JPEG
compression functions was called repeatedly with the same
automatically-allocated destination buffer, then TurboJPEG would erroneously
assume that the `jpegSize` parameter was equal to the size of the buffer, when
in fact that parameter was probably equal to the size of the most recently
compressed JPEG image.  If the size of the previous JPEG image was not as large
as the current JPEG image, then TurboJPEG would unnecessarily reallocate the
destination buffer.


1.3.1
=====

### Significant changes relative to 1.3.0:

1. On Un*x systems, `make install` now installs the libjpeg-turbo libraries
into /opt/libjpeg-turbo/lib32 by default on any 32-bit system, not just x86,
and into /opt/libjpeg-turbo/lib64 by default on any 64-bit system, not just
x86-64.  You can override this by overriding either the `prefix` or `libdir`
configure variables.

2. The Windows installer now places a copy of the TurboJPEG DLLs in the same
directory as the rest of the libjpeg-turbo binaries.  This was mainly done
to support TurboVNC 1.3, which bundles the DLLs in its Windows installation.
When using a 32-bit version of CMake on 64-bit Windows, it is impossible to
access the c:\WINDOWS\system32 directory, which made it impossible for the
TurboVNC build scripts to bundle the 64-bit TurboJPEG DLL.

3. Fixed a bug whereby attempting to encode a progressive JPEG with arithmetic
entropy coding (by passing arguments of `-progressive -arithmetic` to cjpeg or
jpegtran, for instance) would result in an error, `Requested feature was
omitted at compile time`.

4. Fixed a couple of issues (CVE-2013-6629 and CVE-2013-6630) whereby malformed
JPEG images would cause libjpeg-turbo to use uninitialized memory during
decompression.

5. Fixed an error (`Buffer passed to JPEG library is too small`) that occurred
when calling the TurboJPEG YUV encoding function with a very small (< 5x5)
source image, and added a unit test to check for this error.

6. The Java classes should now build properly under Visual Studio 2010 and
later.

7. Fixed an issue that prevented SRPMs generated using the in-tree packaging
tools from being rebuilt on certain newer Linux distributions.

8. Numerous minor fixes to eliminate compilation and build/packaging system
warnings, fix cosmetic issues, improve documentation clarity, and other general
source cleanup.


1.3.0
=====

### Significant changes relative to 1.3 beta1:

1. `make test` now works properly on FreeBSD, and it no longer requires the
md5sum executable to be present on other Un*x platforms.

2. Overhauled the packaging system:

     - To avoid conflict with vendor-supplied libjpeg-turbo packages, the
official RPMs and DEBs for libjpeg-turbo have been renamed to
"libjpeg-turbo-official".
     - The TurboJPEG libraries are now located under /opt/libjpeg-turbo in the
official Linux and Mac packages, to avoid conflict with vendor-supplied
packages and also to streamline the packaging system.
     - Release packages are now created with the directory structure defined
by the configure variables `prefix`, `bindir`, `libdir`, etc. (Un\*x) or by the
`CMAKE_INSTALL_PREFIX` variable (Windows.)  The exception is that the docs are
always located under the system default documentation directory on Un\*x and
Mac systems, and on Windows, the TurboJPEG DLL is always located in the Windows
system directory.
     - To avoid confusion, official libjpeg-turbo packages on Linux/Unix
platforms (except for Mac) will always install the 32-bit libraries in
/opt/libjpeg-turbo/lib32 and the 64-bit libraries in /opt/libjpeg-turbo/lib64.
     - Fixed an issue whereby, in some cases, the libjpeg-turbo executables on
Un*x systems were not properly linking with the shared libraries installed by
the same package.
     - Fixed an issue whereby building the "installer" target on Windows when
`WITH_JAVA=1` would fail if the TurboJPEG JAR had not been previously built.
     - Building the "install" target on Windows now installs files into the
same places that the installer does.

3. Fixed a Huffman encoder bug that prevented I/O suspension from working
properly.


1.2.90 (1.3 beta1)
==================

### Significant changes relative to 1.2.1:

1. Added support for additional scaling factors (3/8, 5/8, 3/4, 7/8, 9/8, 5/4,
11/8, 3/2, 13/8, 7/4, 15/8, and 2) when decompressing.  Note that the IDCT will
not be SIMD-accelerated when using any of these new scaling factors.

2. The TurboJPEG dynamic library is now versioned.  It was not strictly
necessary to do so, because TurboJPEG uses versioned symbols, and if a function
changes in an ABI-incompatible way, that function is renamed and a legacy
function is provided to maintain backward compatibility.  However, certain
Linux distro maintainers have a policy against accepting any library that isn't
versioned.

3. Extended the TurboJPEG Java API so that it can be used to compress a JPEG
image from and decompress a JPEG image to an arbitrary position in a large
image buffer.

4. The `tjDecompressToYUV()` function now supports the `TJFLAG_FASTDCT` flag.

5. The 32-bit supplementary package for amd64 Debian systems now provides
symlinks in /usr/lib/i386-linux-gnu for the TurboJPEG libraries in /usr/lib32.
This allows those libraries to be used on MultiArch-compatible systems (such as
Ubuntu 11 and later) without setting the linker path.

6. The TurboJPEG Java wrapper should now find the JNI library on Mac systems
without having to pass `-Djava.library.path=/usr/lib` to java.

7. TJBench has been ported to Java to provide a convenient way of validating
the performance of the TurboJPEG Java API.  It can be run with
`java -cp turbojpeg.jar TJBench`.

8. cjpeg can now be used to generate JPEG files with the RGB colorspace
(feature ported from jpeg-8d.)

9. The width and height in the `-crop` argument passed to jpegtran can now be
suffixed with `f` to indicate that, when the upper left corner of the cropping
region is automatically moved to the nearest iMCU boundary, the bottom right
corner should be moved by the same amount.  In other words, this feature causes
jpegtran to strictly honor the specified width/height rather than the specified
bottom right corner (feature ported from jpeg-8d.)

10. JPEG files using the RGB colorspace can now be decompressed into grayscale
images (feature ported from jpeg-8d.)

11. Fixed a regression caused by 1.2.1[7] whereby the build would fail with
multiple "Mismatch in operand sizes" errors when attempting to build the x86
SIMD code with NASM 0.98.

12. The in-memory source/destination managers (`jpeg_mem_src()` and
`jpeg_mem_dest()`) are now included by default when building libjpeg-turbo with
libjpeg v6b or v7 emulation, so that programs can take advantage of these
functions without requiring the use of the backward-incompatible libjpeg v8
ABI.  The "age number" of the libjpeg-turbo library on Un*x systems has been
incremented by 1 to reflect this.  You can disable this feature with a
configure/CMake switch in order to retain strict API/ABI compatibility with the
libjpeg v6b or v7 API/ABI (or with previous versions of libjpeg-turbo.)  See
[README.md](README.md) for more details.

13. Added ARMv7s architecture to libjpeg.a and libturbojpeg.a in the official
libjpeg-turbo binary package for OS X, so that those libraries can be used to
build applications that leverage the faster CPUs in the iPhone 5 and iPad 4.


1.2.1
=====

### Significant changes relative to 1.2.0:

1. Creating or decoding a JPEG file that uses the RGB colorspace should now
properly work when the input or output colorspace is one of the libjpeg-turbo
colorspace extensions.

2. When libjpeg-turbo was built without SIMD support and merged (non-fancy)
upsampling was used along with an alpha-enabled colorspace during
decompression, the unused byte of the decompressed pixels was not being set to
0xFF.  This has been fixed.  TJUnitTest has also been extended to test for the
correct behavior of the colorspace extensions when merged upsampling is used.

3. Fixed a bug whereby the libjpeg-turbo SSE2 SIMD code would not preserve the
upper 64 bits of xmm6 and xmm7 on Win64 platforms, which violated the Win64
calling conventions.

4. Fixed a regression (CVE-2012-2806) caused by 1.2.0[6] whereby decompressing
corrupt JPEG images (specifically, images in which the component count was
erroneously set to a large value) would cause libjpeg-turbo to segfault.

5. Worked around a severe performance issue with "Bobcat" (AMD Embedded APU)
processors.  The `MASKMOVDQU` instruction, which was used by the libjpeg-turbo
SSE2 SIMD code, is apparently implemented in microcode on AMD processors, and
it is painfully slow on Bobcat processors in particular.  Eliminating the use
of this instruction improved performance by an order of magnitude on Bobcat
processors and by a small amount (typically 5%) on AMD desktop processors.

6. Added SIMD acceleration for performing 4:2:2 upsampling on NEON-capable ARM
platforms.  This speeds up the decompression of 4:2:2 JPEGs by 20-25% on such
platforms.

7. Fixed a regression caused by 1.2.0[2] whereby, on Linux/x86 platforms
running the 32-bit SSE2 SIMD code in libjpeg-turbo, decompressing a 4:2:0 or
4:2:2 JPEG image into a 32-bit (RGBX, BGRX, etc.) buffer without using fancy
upsampling would produce several incorrect columns of pixels at the right-hand
side of the output image if each row in the output image was not evenly
divisible by 16 bytes.

8. Fixed an issue whereby attempting to build the SIMD extensions with Xcode
4.3 on OS X platforms would cause NASM to return numerous errors of the form
"'%define' expects a macro identifier".

9. Added flags to the TurboJPEG API that allow the caller to force the use of
either the fast or the accurate DCT/IDCT algorithms in the underlying codec.


1.2.0
=====

### Significant changes relative to 1.2 beta1:

1. Fixed build issue with Yasm on Unix systems (the libjpeg-turbo build system
was not adding the current directory to the assembler include path, so Yasm
was not able to find jsimdcfg.inc.)

2. Fixed out-of-bounds read in SSE2 SIMD code that occurred when decompressing
a JPEG image to a bitmap buffer whose size was not a multiple of 16 bytes.
This was more of an annoyance than an actual bug, since it did not cause any
actual run-time problems, but the issue showed up when running libjpeg-turbo in
valgrind.  See <http://crbug.com/72399> for more information.

3. Added a compile-time macro (`LIBJPEG_TURBO_VERSION`) that can be used to
check the version of libjpeg-turbo against which an application was compiled.

4. Added new RGBA/BGRA/ABGR/ARGB colorspace extension constants (libjpeg API)
and pixel formats (TurboJPEG API), which allow applications to specify that,
when decompressing to a 4-component RGB buffer, the unused byte should be set
to 0xFF so that it can be interpreted as an opaque alpha channel.

5. Fixed regression issue whereby DevIL failed to build against libjpeg-turbo
because libjpeg-turbo's distributed version of jconfig.h contained an `INLINE`
macro, which conflicted with a similar macro in DevIL.  This macro is used only
internally when building libjpeg-turbo, so it was moved into config.h.

6. libjpeg-turbo will now correctly decompress erroneous CMYK/YCCK JPEGs whose
K component is assigned a component ID of 1 instead of 4.  Although these files
are in violation of the spec, other JPEG implementations handle them
correctly.

7. Added ARMv6 and ARMv7 architectures to libjpeg.a and libturbojpeg.a in
the official libjpeg-turbo binary package for OS X, so that those libraries can
be used to build both OS X and iOS applications.


1.1.90 (1.2 beta1)
==================

### Significant changes relative to 1.1.1:

1. Added a Java wrapper for the TurboJPEG API.  See
[java/README.md](java/README.md) for more details.

2. The TurboJPEG API can now be used to scale down images during
decompression.

3. Added SIMD routines for RGB-to-grayscale color conversion, which
significantly improves the performance of grayscale JPEG compression from an
RGB source image.

4. Improved the performance of the C color conversion routines, which are used
on platforms for which SIMD acceleration is not available.

5. Added a function to the TurboJPEG API that performs lossless transforms.
This function is implemented using the same back end as jpegtran, but it
performs transcoding entirely in memory and allows multiple transforms and/or
crop operations to be batched together, so the source coefficients only need to
be read once.  This is useful when generating image tiles from a single source
JPEG.

6. Added tests for the new TurboJPEG scaled decompression and lossless
transform features to tjbench (the TurboJPEG benchmark, formerly called
"jpgtest".)

7. Added support for 4:4:0 (transposed 4:2:2) subsampling in TurboJPEG, which
was necessary in order for it to read 4:2:2 JPEG files that had been losslessly
transposed or rotated 90 degrees.

8. All legacy VirtualGL code has been re-factored, and this has allowed
libjpeg-turbo, in its entirety, to be re-licensed under a BSD-style license.

9. libjpeg-turbo can now be built with Yasm.

10. Added SIMD acceleration for ARM Linux and iOS platforms that support
NEON instructions.

11. Refactored the TurboJPEG C API and documented it using Doxygen.  The
TurboJPEG 1.2 API uses pixel formats to define the size and component order of
the uncompressed source/destination images, and it includes a more efficient
version of `TJBUFSIZE()` that computes a worst-case JPEG size based on the
level of chrominance subsampling.  The refactored implementation of the
TurboJPEG API now uses the libjpeg memory source and destination managers,
which allows the TurboJPEG compressor to grow the JPEG buffer as necessary.

12. Eliminated errors in the output of jpegtran on Windows that occurred when
the application was invoked using I/O redirection
(`jpegtran <input.jpg >output.jpg`.)

13. The inclusion of libjpeg v7 and v8 emulation as well as arithmetic coding
support in libjpeg-turbo v1.1.0 introduced several new error constants in
jerror.h, and these were mistakenly enabled for all emulation modes, causing
the error enum in libjpeg-turbo to sometimes have different values than the
same enum in libjpeg.  This represents an ABI incompatibility, and it caused
problems with rare applications that took specific action based on a particular
error value.  The fix was to include the new error constants conditionally
based on whether libjpeg v7 or v8 emulation was enabled.

14. Fixed an issue whereby Windows applications that used libjpeg-turbo would
fail to compile if the Windows system headers were included before jpeglib.h.
This issue was caused by a conflict in the definition of the INT32 type.

15. Fixed 32-bit supplementary package for amd64 Debian systems, which was
broken by enhancements to the packaging system in 1.1.

16. When decompressing a JPEG image using an output colorspace of
`JCS_EXT_RGBX`, `JCS_EXT_BGRX`, `JCS_EXT_XBGR`, or `JCS_EXT_XRGB`,
libjpeg-turbo will now set the unused byte to 0xFF, which allows applications
to interpret that byte as an alpha channel (0xFF = opaque).


1.1.1
=====

### Significant changes relative to 1.1.0:

1. Fixed a 1-pixel error in row 0, column 21 of the luminance plane generated
by `tjEncodeYUV()`.

2. libjpeg-turbo's accelerated Huffman decoder previously ignored unexpected
markers found in the middle of the JPEG data stream during decompression.  It
will now hand off decoding of a particular block to the unaccelerated Huffman
decoder if an unexpected marker is found, so that the unaccelerated Huffman
decoder can generate an appropriate warning.

3. Older versions of MinGW64 prefixed symbol names with underscores by
default, which differed from the behavior of 64-bit Visual C++.  MinGW64 1.0
has adopted the behavior of 64-bit Visual C++ as the default, so to accommodate
this, the libjpeg-turbo SIMD function names are no longer prefixed with an
underscore when building with MinGW64.  This means that, when building
libjpeg-turbo with older versions of MinGW64, you will now have to add
`-fno-leading-underscore` to the `CFLAGS`.

4. Fixed a regression bug in the NSIS script that caused the Windows installer
build to fail when using the Visual Studio IDE.

5. Fixed a bug in `jpeg_read_coefficients()` whereby it would not initialize
`cinfo->image_width` and `cinfo->image_height` if libjpeg v7 or v8 emulation
was enabled.  This specifically caused the jpegoptim program to fail if it was
linked against a version of libjpeg-turbo that was built with libjpeg v7 or v8
emulation.

6. Eliminated excessive I/O overhead that occurred when reading BMP files in
cjpeg.

7. Eliminated errors in the output of cjpeg on Windows that occurred when the
application was invoked using I/O redirection (`cjpeg <inputfile >output.jpg`.)


1.1.0
=====

### Significant changes relative to 1.1 beta1:

1. The algorithm used by the SIMD quantization function cannot produce correct
results when the JPEG quality is >= 98 and the fast integer forward DCT is
used.  Thus, the non-SIMD quantization function is now used for those cases,
and libjpeg-turbo should now produce identical output to libjpeg v6b in all
cases.

2. Despite the above, the fast integer forward DCT still degrades somewhat for
JPEG qualities greater than 95, so the TurboJPEG wrapper will now automatically
use the accurate integer forward DCT when generating JPEG images of quality 96
or greater.  This reduces compression performance by as much as 15% for these
high-quality images but is necessary to ensure that the images are perceptually
lossless.  It also ensures that the library can avoid the performance pitfall
created by [1].

3. Ported jpgtest.cxx to pure C to avoid the need for a C++ compiler.

4. Fixed visual artifacts in grayscale JPEG compression caused by a typo in
the RGB-to-luminance lookup tables.

5. The Windows distribution packages now include the libjpeg run-time programs
(cjpeg, etc.)

6. All packages now include jpgtest.

7. The TurboJPEG dynamic library now uses versioned symbols.

8. Added two new TurboJPEG API functions, `tjEncodeYUV()` and
`tjDecompressToYUV()`, to replace the somewhat hackish `TJ_YUV` flag.


1.0.90 (1.1 beta1)
==================

### Significant changes relative to 1.0.1:

1. Added emulation of the libjpeg v7 and v8 APIs and ABIs.  See
[README.md](README.md) for more details.  This feature was sponsored by
CamTrace SAS.

2. Created a new CMake-based build system for the Visual C++ and MinGW builds.

3. Grayscale bitmaps can now be compressed from/decompressed to using the
TurboJPEG API.

4. jpgtest can now be used to test decompression performance with existing
JPEG images.

5. If the default install prefix (/opt/libjpeg-turbo) is used, then
`make install` now creates /opt/libjpeg-turbo/lib32 and
/opt/libjpeg-turbo/lib64 sym links to duplicate the behavior of the binary
packages.

6. All symbols in the libjpeg-turbo dynamic library are now versioned, even
when the library is built with libjpeg v6b emulation.

7. Added arithmetic encoding and decoding support (can be disabled with
configure or CMake options)

8. Added a `TJ_YUV` flag to the TurboJPEG API, which causes both the compressor
and decompressor to output planar YUV images.

9. Added an extended version of `tjDecompressHeader()` to the TurboJPEG API,
which allows the caller to determine the type of subsampling used in a JPEG
image.

10. Added further protections against invalid Huffman codes.


1.0.1
=====

### Significant changes relative to 1.0.0:

1. The Huffman decoder will now handle erroneous Huffman codes (for instance,
from a corrupt JPEG image.)  Previously, these would cause libjpeg-turbo to
crash under certain circumstances.

2. Fixed typo in SIMD dispatch routines that was causing 4:2:2 upsampling to
be used instead of 4:2:0 when decompressing JPEG images using SSE2 code.

3. The configure script will now automatically determine whether the
`INCOMPLETE_TYPES_BROKEN` macro should be defined.


1.0.0
=====

### Significant changes relative to 0.0.93:

1. 2983700: Further FreeBSD build tweaks (no longer necessary to specify
`--host` when configuring on a 64-bit system)

2. Created symlinks in the Unix/Linux packages so that the TurboJPEG
include file can always be found in /opt/libjpeg-turbo/include, the 32-bit
static libraries can always be found in /opt/libjpeg-turbo/lib32, and the
64-bit static libraries can always be found in /opt/libjpeg-turbo/lib64.

3. The Unix/Linux distribution packages now include the libjpeg run-time
programs (cjpeg, etc.) and man pages.

4. Created a 32-bit supplementary package for amd64 Debian systems, which
contains just the 32-bit libjpeg-turbo libraries.

5. Moved the libraries from */lib32 to */lib in the i386 Debian package.

6. Include distribution package for Cygwin

7. No longer necessary to specify `--without-simd` on non-x86 architectures,
and unit tests now work on those architectures.


0.0.93
======

### Significant changes relative to 0.0.91:

1. 2982659: Fixed x86-64 build on FreeBSD systems

2. 2988188: Added support for Windows 64-bit systems


0.0.91
======

### Significant changes relative to 0.0.90:

1. Added documentation to .deb packages

2. 2968313: Fixed data corruption issues when decompressing large JPEG images
and/or using buffered I/O with the libjpeg-turbo decompressor


0.0.90
======

Initial release
