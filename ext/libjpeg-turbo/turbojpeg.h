/*
 * Copyright (C)2009-2013 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __TURBOJPEG_H__
#define __TURBOJPEG_H__

#if defined(_WIN32) && defined(DLLDEFINE)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif
#define DLLCALL


/**
 * @addtogroup TurboJPEG
 * TurboJPEG API.  This API provides an interface for generating, decoding, and
 * transforming planar YUV and JPEG images in memory.
 *
 * @{
 */


/**
 * The number of chrominance subsampling options
 */
#define TJ_NUMSAMP 5

/**
 * Chrominance subsampling options.
 * When an image is converted from the RGB to the YCbCr colorspace as part of
 * the JPEG compression process, some of the Cb and Cr (chrominance) components
 * can be discarded or averaged together to produce a smaller image with little
 * perceptible loss of image clarity (the human eye is more sensitive to small
 * changes in brightness than small changes in color.)  This is called
 * "chrominance subsampling".
 * <p>
 * NOTE: Technically, the JPEG format uses the YCbCr colorspace, but per the
 * convention of the digital video community, the TurboJPEG API uses "YUV" to
 * refer to an image format consisting of Y, Cb, and Cr image planes.
 */
enum TJSAMP
{
  /**
   * 4:4:4 chrominance subsampling (no chrominance subsampling).  The JPEG or
   * YUV image will contain one chrominance component for every pixel in the
   * source image.
   */
  TJSAMP_444=0,
  /**
   * 4:2:2 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 2x1 block of pixels in the source image.
   */
  TJSAMP_422,
  /**
   * 4:2:0 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 2x2 block of pixels in the source image.
   */
  TJSAMP_420,
  /**
   * Grayscale.  The JPEG or YUV image will contain no chrominance components.
   */
  TJSAMP_GRAY,
  /**
   * 4:4:0 chrominance subsampling.  The JPEG or YUV image will contain one
   * chrominance component for every 1x2 block of pixels in the source image.
   * Note that 4:4:0 subsampling is not fully accelerated in libjpeg-turbo.
   */
  TJSAMP_440
};

/**
 * MCU block width (in pixels) for a given level of chrominance subsampling.
 * MCU block sizes:
 * - 8x8 for no subsampling or grayscale
 * - 16x8 for 4:2:2
 * - 8x16 for 4:4:0
 * - 16x16 for 4:2:0 
 */
static const int tjMCUWidth[TJ_NUMSAMP]  = {8, 16, 16, 8, 8};

/**
 * MCU block height (in pixels) for a given level of chrominance subsampling.
 * MCU block sizes:
 * - 8x8 for no subsampling or grayscale
 * - 16x8 for 4:2:2
 * - 8x16 for 4:4:0
 * - 16x16 for 4:2:0 
 */
static const int tjMCUHeight[TJ_NUMSAMP] = {8, 8, 16, 8, 16};


/**
 * The number of pixel formats
 */
#define TJ_NUMPF 11

/**
 * Pixel formats
 */
enum TJPF
{
  /**
   * RGB pixel format.  The red, green, and blue components in the image are
   * stored in 3-byte pixels in the order R, G, B from lowest to highest byte
   * address within each pixel.
   */
  TJPF_RGB=0,
  /**
   * BGR pixel format.  The red, green, and blue components in the image are
   * stored in 3-byte pixels in the order B, G, R from lowest to highest byte
   * address within each pixel.
   */
  TJPF_BGR,
  /**
   * RGBX pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order R, G, B from lowest to highest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  TJPF_RGBX,
  /**
   * BGRX pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order B, G, R from lowest to highest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  TJPF_BGRX,
  /**
   * XBGR pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order R, G, B from highest to lowest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  TJPF_XBGR,
  /**
   * XRGB pixel format.  The red, green, and blue components in the image are
   * stored in 4-byte pixels in the order B, G, R from highest to lowest byte
   * address within each pixel.  The X component is ignored when compressing
   * and undefined when decompressing.
   */
  TJPF_XRGB,
  /**
   * Grayscale pixel format.  Each 1-byte pixel represents a luminance
   * (brightness) level from 0 to 255.
   */
  TJPF_GRAY,
  /**
   * RGBA pixel format.  This is the same as @ref TJPF_RGBX, except that when
   * decompressing, the X component is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  TJPF_RGBA,
  /**
   * BGRA pixel format.  This is the same as @ref TJPF_BGRX, except that when
   * decompressing, the X component is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  TJPF_BGRA,
  /**
   * ABGR pixel format.  This is the same as @ref TJPF_XBGR, except that when
   * decompressing, the X component is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  TJPF_ABGR,
  /**
   * ARGB pixel format.  This is the same as @ref TJPF_XRGB, except that when
   * decompressing, the X component is guaranteed to be 0xFF, which can be
   * interpreted as an opaque alpha channel.
   */
  TJPF_ARGB
};

/**
 * Red offset (in bytes) for a given pixel format.  This specifies the number
 * of bytes that the red component is offset from the start of the pixel.  For
 * instance, if a pixel of format TJ_BGRX is stored in <tt>char pixel[]</tt>,
 * then the red component will be <tt>pixel[tjRedOffset[TJ_BGRX]]</tt>.
 */
static const int tjRedOffset[TJ_NUMPF] = {0, 2, 0, 2, 3, 1, 0, 0, 2, 3, 1};
/**
 * Green offset (in bytes) for a given pixel format.  This specifies the number
 * of bytes that the green component is offset from the start of the pixel.
 * For instance, if a pixel of format TJ_BGRX is stored in
 * <tt>char pixel[]</tt>, then the green component will be
 * <tt>pixel[tjGreenOffset[TJ_BGRX]]</tt>.
 */
static const int tjGreenOffset[TJ_NUMPF] = {1, 1, 1, 1, 2, 2, 0, 1, 1, 2, 2};
/**
 * Blue offset (in bytes) for a given pixel format.  This specifies the number
 * of bytes that the Blue component is offset from the start of the pixel.  For
 * instance, if a pixel of format TJ_BGRX is stored in <tt>char pixel[]</tt>,
 * then the blue component will be <tt>pixel[tjBlueOffset[TJ_BGRX]]</tt>.
 */
static const int tjBlueOffset[TJ_NUMPF] = {2, 0, 2, 0, 1, 3, 0, 2, 0, 1, 3};

/**
 * Pixel size (in bytes) for a given pixel format.
 */
static const int tjPixelSize[TJ_NUMPF] = {3, 3, 4, 4, 4, 4, 1, 4, 4, 4, 4};


/**
 * The uncompressed source/destination image is stored in bottom-up (Windows,
 * OpenGL) order, not top-down (X11) order.
 */
#define TJFLAG_BOTTOMUP        2
/**
 * Turn off CPU auto-detection and force TurboJPEG to use MMX code (if the
 * underlying codec supports it.)
 */
#define TJFLAG_FORCEMMX        8
/**
 * Turn off CPU auto-detection and force TurboJPEG to use SSE code (if the
 * underlying codec supports it.)
 */
#define TJFLAG_FORCESSE       16
/**
 * Turn off CPU auto-detection and force TurboJPEG to use SSE2 code (if the
 * underlying codec supports it.)
 */
#define TJFLAG_FORCESSE2      32
/**
 * Turn off CPU auto-detection and force TurboJPEG to use SSE3 code (if the
 * underlying codec supports it.)
 */
#define TJFLAG_FORCESSE3     128
/**
 * When decompressing an image that was compressed using chrominance
 * subsampling, use the fastest chrominance upsampling algorithm available in
 * the underlying codec.  The default is to use smooth upsampling, which
 * creates a smooth transition between neighboring chrominance components in
 * order to reduce upsampling artifacts in the decompressed image.
 */
#define TJFLAG_FASTUPSAMPLE  256
/**
 * Disable buffer (re)allocation.  If passed to #tjCompress2() or
 * #tjTransform(), this flag will cause those functions to generate an error if
 * the JPEG image buffer is invalid or too small rather than attempting to
 * allocate or reallocate that buffer.  This reproduces the behavior of earlier
 * versions of TurboJPEG.
 */
#define TJFLAG_NOREALLOC     1024
/**
 * Use the fastest DCT/IDCT algorithm available in the underlying codec.  The
 * default if this flag is not specified is implementation-specific.  For
 * example, the implementation of TurboJPEG for libjpeg[-turbo] uses the fast
 * algorithm by default when compressing, because this has been shown to have
 * only a very slight effect on accuracy, but it uses the accurate algorithm
 * when decompressing, because this has been shown to have a larger effect.
 */
#define TJFLAG_FASTDCT       2048
/**
 * Use the most accurate DCT/IDCT algorithm available in the underlying codec.
 * The default if this flag is not specified is implementation-specific.  For
 * example, the implementation of TurboJPEG for libjpeg[-turbo] uses the fast
 * algorithm by default when compressing, because this has been shown to have
 * only a very slight effect on accuracy, but it uses the accurate algorithm
 * when decompressing, because this has been shown to have a larger effect.
 */
#define TJFLAG_ACCURATEDCT   4096


/**
 * The number of transform operations
 */
#define TJ_NUMXOP 8

/**
 * Transform operations for #tjTransform()
 */
enum TJXOP
{
  /**
   * Do not transform the position of the image pixels
   */
  TJXOP_NONE=0,
  /**
   * Flip (mirror) image horizontally.  This transform is imperfect if there
   * are any partial MCU blocks on the right edge (see #TJXOPT_PERFECT.)
   */
  TJXOP_HFLIP,
  /**
   * Flip (mirror) image vertically.  This transform is imperfect if there are
   * any partial MCU blocks on the bottom edge (see #TJXOPT_PERFECT.)
   */
  TJXOP_VFLIP,
  /**
   * Transpose image (flip/mirror along upper left to lower right axis.)  This
   * transform is always perfect.
   */
  TJXOP_TRANSPOSE,
  /**
   * Transverse transpose image (flip/mirror along upper right to lower left
   * axis.)  This transform is imperfect if there are any partial MCU blocks in
   * the image (see #TJXOPT_PERFECT.)
   */
  TJXOP_TRANSVERSE,
  /**
   * Rotate image clockwise by 90 degrees.  This transform is imperfect if
   * there are any partial MCU blocks on the bottom edge (see
   * #TJXOPT_PERFECT.)
   */
  TJXOP_ROT90,
  /**
   * Rotate image 180 degrees.  This transform is imperfect if there are any
   * partial MCU blocks in the image (see #TJXOPT_PERFECT.)
   */
  TJXOP_ROT180,
  /**
   * Rotate image counter-clockwise by 90 degrees.  This transform is imperfect
   * if there are any partial MCU blocks on the right edge (see
   * #TJXOPT_PERFECT.)
   */
  TJXOP_ROT270
};


/**
 * This option will cause #tjTransform() to return an error if the transform is
 * not perfect.  Lossless transforms operate on MCU blocks, whose size depends
 * on the level of chrominance subsampling used (see #tjMCUWidth
 * and #tjMCUHeight.)  If the image's width or height is not evenly divisible
 * by the MCU block size, then there will be partial MCU blocks on the right
 * and/or bottom edges.  It is not possible to move these partial MCU blocks to
 * the top or left of the image, so any transform that would require that is
 * "imperfect."  If this option is not specified, then any partial MCU blocks
 * that cannot be transformed will be left in place, which will create
 * odd-looking strips on the right or bottom edge of the image.
 */
#define TJXOPT_PERFECT  1
/**
 * This option will cause #tjTransform() to discard any partial MCU blocks that
 * cannot be transformed.
 */
#define TJXOPT_TRIM     2
/**
 * This option will enable lossless cropping.  See #tjTransform() for more
 * information.
 */
#define TJXOPT_CROP     4
/**
 * This option will discard the color data in the input image and produce
 * a grayscale output image.
 */
#define TJXOPT_GRAY     8
/**
 * This option will prevent #tjTransform() from outputting a JPEG image for
 * this particular transform (this can be used in conjunction with a custom
 * filter to capture the transformed DCT coefficients without transcoding
 * them.)
 */
#define TJXOPT_NOOUTPUT 16


/**
 * Scaling factor
 */
typedef struct
{
  /**
   * Numerator
   */
  int num;
  /**
   * Denominator
   */
  int denom;
} tjscalingfactor;

/**
 * Cropping region
 */
typedef struct
{
  /**
   * The left boundary of the cropping region.  This must be evenly divisible
   * by the MCU block width (see #tjMCUWidth.)
   */
  int x;
  /**
   * The upper boundary of the cropping region.  This must be evenly divisible
   * by the MCU block height (see #tjMCUHeight.)
   */
  int y;
  /**
   * The width of the cropping region. Setting this to 0 is the equivalent of
   * setting it to the width of the source JPEG image - x.
   */
  int w;
  /**
   * The height of the cropping region. Setting this to 0 is the equivalent of
   * setting it to the height of the source JPEG image - y.
   */
  int h;
} tjregion;

/**
 * Lossless transform
 */
typedef struct tjtransform
{
  /**
   * Cropping region
   */
  tjregion r;
  /**
   * One of the @ref TJXOP "transform operations"
   */
  int op;
  /**
   * The bitwise OR of one of more of the @ref TJXOPT_CROP "transform options"
   */
  int options;
  /**
   * Arbitrary data that can be accessed within the body of the callback
   * function
   */
  void *data;
  /**
   * A callback function that can be used to modify the DCT coefficients
   * after they are losslessly transformed but before they are transcoded to a
   * new JPEG image.  This allows for custom filters or other transformations
   * to be applied in the frequency domain.
   *
   * @param coeffs pointer to an array of transformed DCT coefficients.  (NOTE:
   *        this pointer is not guaranteed to be valid once the callback
   *        returns, so applications wishing to hand off the DCT coefficients
   *        to another function or library should make a copy of them within
   *        the body of the callback.)
   * @param arrayRegion #tjregion structure containing the width and height of
   *        the array pointed to by <tt>coeffs</tt> as well as its offset
   *        relative to the component plane.  TurboJPEG implementations may
   *        choose to split each component plane into multiple DCT coefficient
   *        arrays and call the callback function once for each array.
   * @param planeRegion #tjregion structure containing the width and height of
   *        the component plane to which <tt>coeffs</tt> belongs
   * @param componentID ID number of the component plane to which
   *        <tt>coeffs</tt> belongs (Y, Cb, and Cr have, respectively, ID's of
   *        0, 1, and 2 in typical JPEG images.)
   * @param transformID ID number of the transformed image to which
   *        <tt>coeffs</tt> belongs.  This is the same as the index of the
   *        transform in the <tt>transforms</tt> array that was passed to
   *        #tjTransform().
   * @param transform a pointer to a #tjtransform structure that specifies the
   *        parameters and/or cropping region for this transform
   *
   * @return 0 if the callback was successful, or -1 if an error occurred.
   */
  int (*customFilter)(short *coeffs, tjregion arrayRegion,
    tjregion planeRegion, int componentIndex, int transformIndex,
    struct tjtransform *transform);
} tjtransform;

/**
 * TurboJPEG instance handle
 */
typedef void* tjhandle;


/**
 * Pad the given width to the nearest 32-bit boundary
 */
#define TJPAD(width) (((width)+3)&(~3))

/**
 * Compute the scaled value of <tt>dimension</tt> using the given scaling
 * factor.  This macro performs the integer equivalent of <tt>ceil(dimension *
 * scalingFactor)</tt>. 
 */
#define TJSCALED(dimension, scalingFactor) ((dimension * scalingFactor.num \
  + scalingFactor.denom - 1) / scalingFactor.denom)


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Create a TurboJPEG compressor instance.
 *
 * @return a handle to the newly-created instance, or NULL if an error
 * occurred (see #tjGetErrorStr().)
 */
DLLEXPORT tjhandle DLLCALL tjInitCompress(void);


/**
 * Compress an RGB or grayscale image into a JPEG image.
 *
 * @param handle a handle to a TurboJPEG compressor or transformer instance
 * @param srcBuf pointer to an image buffer containing RGB or grayscale pixels
 *        to be compressed
 * @param width width (in pixels) of the source image
 * @param pitch bytes per line of the source image.  Normally, this should be
 *        <tt>width * #tjPixelSize[pixelFormat]</tt> if the image is unpadded,
 *        or <tt>#TJPAD(width * #tjPixelSize[pixelFormat])</tt> if each line of
 *        the image is padded to the nearest 32-bit boundary, as is the case
 *        for Windows bitmaps.  You can also be clever and use this parameter
 *        to skip lines, etc.  Setting this parameter to 0 is the equivalent of
 *        setting it to <tt>width * #tjPixelSize[pixelFormat]</tt>.
 * @param height height (in pixels) of the source image
 * @param pixelFormat pixel format of the source image (see @ref TJPF
 *        "Pixel formats".)
 * @param jpegBuf address of a pointer to an image buffer that will receive the
 *        JPEG image.  TurboJPEG has the ability to reallocate the JPEG buffer
 *        to accommodate the size of the JPEG image.  Thus, you can choose to:
 *        -# pre-allocate the JPEG buffer with an arbitrary size using
 *        #tjAlloc() and let TurboJPEG grow the buffer as needed,
 *        -# set <tt>*jpegBuf</tt> to NULL to tell TurboJPEG to allocate the
 *        buffer for you, or
 *        -# pre-allocate the buffer to a "worst case" size determined by
 *        calling #tjBufSize().  This should ensure that the buffer never has
 *        to be re-allocated (setting #TJFLAG_NOREALLOC guarantees this.)
 *        .
 *        If you choose option 1, <tt>*jpegSize</tt> should be set to the
 *        size of your pre-allocated buffer.  In any case, unless you have
 *        set #TJFLAG_NOREALLOC, you should always check <tt>*jpegBuf</tt> upon
 *        return from this function, as it may have changed.
 * @param jpegSize pointer to an unsigned long variable that holds the size of
 *        the JPEG image buffer.  If <tt>*jpegBuf</tt> points to a
 *        pre-allocated buffer, then <tt>*jpegSize</tt> should be set to the
 *        size of the buffer.  Upon return, <tt>*jpegSize</tt> will contain the
 *        size of the JPEG image (in bytes.)
 * @param jpegSubsamp the level of chrominance subsampling to be used when
 *        generating the JPEG image (see @ref TJSAMP
 *        "Chrominance subsampling options".)
 * @param jpegQual the image quality of the generated JPEG image (1 = worst,
          100 = best)
 * @param flags the bitwise OR of one or more of the @ref TJFLAG_BOTTOMUP
 *        "flags".
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
*/
DLLEXPORT int DLLCALL tjCompress2(tjhandle handle, unsigned char *srcBuf,
  int width, int pitch, int height, int pixelFormat, unsigned char **jpegBuf,
  unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags);


/**
 * The maximum size of the buffer (in bytes) required to hold a JPEG image with
 * the given parameters.  The number of bytes returned by this function is
 * larger than the size of the uncompressed source image.  The reason for this
 * is that the JPEG format uses 16-bit coefficients, and it is thus possible
 * for a very high-quality JPEG image with very high-frequency content to
 * expand rather than compress when converted to the JPEG format.  Such images
 * represent a very rare corner case, but since there is no way to predict the
 * size of a JPEG image prior to compression, the corner case has to be
 * handled.
 *
 * @param width width of the image (in pixels)
 * @param height height of the image (in pixels)
 * @param jpegSubsamp the level of chrominance subsampling to be used when
 *        generating the JPEG image (see @ref TJSAMP
 *        "Chrominance subsampling options".)
 *
 * @return the maximum size of the buffer (in bytes) required to hold the
 * image, or -1 if the arguments are out of bounds.
 */
DLLEXPORT unsigned long DLLCALL tjBufSize(int width, int height,
  int jpegSubsamp);


/**
 * The size of the buffer (in bytes) required to hold a YUV planar image with
 * the given parameters.
 *
 * @param width width of the image (in pixels)
 * @param height height of the image (in pixels)
 * @param subsamp level of chrominance subsampling in the image (see
 *        @ref TJSAMP "Chrominance subsampling options".)
 *
 * @return the size of the buffer (in bytes) required to hold the image, or
 * -1 if the arguments are out of bounds.
 */
DLLEXPORT unsigned long DLLCALL tjBufSizeYUV(int width, int height,
  int subsamp);


/**
 * Encode an RGB or grayscale image into a YUV planar image.  This function
 * uses the accelerated color conversion routines in TurboJPEG's underlying
 * codec to produce a planar YUV image that is suitable for X Video.
 * Specifically, if the chrominance components are subsampled along the
 * horizontal dimension, then the width of the luminance plane is padded to the
 * nearest multiple of 2 in the output image (same goes for the height of the
 * luminance plane, if the chrominance components are subsampled along the
 * vertical dimension.)  Also, each line of each plane in the output image is
 * padded to 4 bytes.  Although this will work with any subsampling option, it
 * is really only useful in combination with TJ_420, which produces an image
 * compatible with the I420 (AKA "YUV420P") format.
 * <p>
 * NOTE: Technically, the JPEG format uses the YCbCr colorspace, but per the
 * convention of the digital video community, the TurboJPEG API uses "YUV" to
 * refer to an image format consisting of Y, Cb, and Cr image planes.
 *
 * @param handle a handle to a TurboJPEG compressor or transformer instance
 * @param srcBuf pointer to an image buffer containing RGB or grayscale pixels
 *        to be encoded
 * @param width width (in pixels) of the source image
 * @param pitch bytes per line of the source image.  Normally, this should be
 *        <tt>width * #tjPixelSize[pixelFormat]</tt> if the image is unpadded,
 *        or <tt>#TJPAD(width * #tjPixelSize[pixelFormat])</tt> if each line of
 *        the image is padded to the nearest 32-bit boundary, as is the case
 *        for Windows bitmaps.  You can also be clever and use this parameter
 *        to skip lines, etc.  Setting this parameter to 0 is the equivalent of
 *        setting it to <tt>width * #tjPixelSize[pixelFormat]</tt>.
 * @param height height (in pixels) of the source image
 * @param pixelFormat pixel format of the source image (see @ref TJPF
 *        "Pixel formats".)
 * @param dstBuf pointer to an image buffer that will receive the YUV image.
 *        Use #tjBufSizeYUV() to determine the appropriate size for this buffer
 *        based on the image width, height, and level of chrominance
 *        subsampling.
 * @param subsamp the level of chrominance subsampling to be used when
 *        generating the YUV image (see @ref TJSAMP
 *        "Chrominance subsampling options".)
 * @param flags the bitwise OR of one or more of the @ref TJFLAG_BOTTOMUP
 *        "flags".
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
*/
DLLEXPORT int DLLCALL tjEncodeYUV2(tjhandle handle,
  unsigned char *srcBuf, int width, int pitch, int height, int pixelFormat,
  unsigned char *dstBuf, int subsamp, int flags);


/**
 * Create a TurboJPEG decompressor instance.
 *
 * @return a handle to the newly-created instance, or NULL if an error
 * occurred (see #tjGetErrorStr().)
*/
DLLEXPORT tjhandle DLLCALL tjInitDecompress(void);


/**
 * Retrieve information about a JPEG image without decompressing it.
 *
 * @param handle a handle to a TurboJPEG decompressor or transformer instance
 * @param jpegBuf pointer to a buffer containing a JPEG image
 * @param jpegSize size of the JPEG image (in bytes)
 * @param width pointer to an integer variable that will receive the width (in
 *        pixels) of the JPEG image
 * @param height pointer to an integer variable that will receive the height
 *        (in pixels) of the JPEG image
 * @param jpegSubsamp pointer to an integer variable that will receive the
 *        level of chrominance subsampling used when compressing the JPEG image
 *        (see @ref TJSAMP "Chrominance subsampling options".)
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
*/
DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle handle,
  unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height,
  int *jpegSubsamp);


/**
 * Returns a list of fractional scaling factors that the JPEG decompressor in
 * this implementation of TurboJPEG supports.
 *
 * @param numscalingfactors pointer to an integer variable that will receive
 *        the number of elements in the list
 *
 * @return a pointer to a list of fractional scaling factors, or NULL if an
 * error is encountered (see #tjGetErrorStr().)
*/
DLLEXPORT tjscalingfactor* DLLCALL tjGetScalingFactors(int *numscalingfactors);


/**
 * Decompress a JPEG image to an RGB or grayscale image.
 *
 * @param handle a handle to a TurboJPEG decompressor or transformer instance
 * @param jpegBuf pointer to a buffer containing the JPEG image to decompress
 * @param jpegSize size of the JPEG image (in bytes)
 * @param dstBuf pointer to an image buffer that will receive the decompressed
 *        image.  This buffer should normally be <tt>pitch * scaledHeight</tt>
 *        bytes in size, where <tt>scaledHeight</tt> can be determined by
 *        calling #TJSCALED() with the JPEG image height and one of the scaling
 *        factors returned by #tjGetScalingFactors().  The <tt>dstBuf</tt>
 *        pointer may also be used to decompress into a specific region of a
 *        larger buffer.
 * @param width desired width (in pixels) of the destination image.  If this is
 *        different than the width of the JPEG image being decompressed, then
 *        TurboJPEG will use scaling in the JPEG decompressor to generate the
 *        largest possible image that will fit within the desired width.  If
 *        <tt>width</tt> is set to 0, then only the height will be considered
 *        when determining the scaled image size.
 * @param pitch bytes per line of the destination image.  Normally, this is
 *        <tt>scaledWidth * #tjPixelSize[pixelFormat]</tt> if the decompressed
 *        image is unpadded, else <tt>#TJPAD(scaledWidth *
 *        #tjPixelSize[pixelFormat])</tt> if each line of the decompressed
 *        image is padded to the nearest 32-bit boundary, as is the case for
 *        Windows bitmaps.  (NOTE: <tt>scaledWidth</tt> can be determined by
 *        calling #TJSCALED() with the JPEG image width and one of the scaling
 *        factors returned by #tjGetScalingFactors().)  You can also be clever
 *        and use the pitch parameter to skip lines, etc.  Setting this
 *        parameter to 0 is the equivalent of setting it to <tt>scaledWidth
 *        * #tjPixelSize[pixelFormat]</tt>.
 * @param height desired height (in pixels) of the destination image.  If this
 *        is different than the height of the JPEG image being decompressed,
 *        then TurboJPEG will use scaling in the JPEG decompressor to generate
 *        the largest possible image that will fit within the desired height.
 *        If <tt>height</tt> is set to 0, then only the width will be
 *        considered when determining the scaled image size.
 * @param pixelFormat pixel format of the destination image (see @ref
 *        TJPF "Pixel formats".)
 * @param flags the bitwise OR of one or more of the @ref TJFLAG_BOTTOMUP
 *        "flags".
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
 */
DLLEXPORT int DLLCALL tjDecompress2(tjhandle handle,
  unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
  int width, int pitch, int height, int pixelFormat, int flags);


/**
 * Decompress a JPEG image to a YUV planar image.  This function performs JPEG
 * decompression but leaves out the color conversion step, so a planar YUV
 * image is generated instead of an RGB image.  The padding of the planes in
 * this image is the same as in the images generated by #tjEncodeYUV2().  Note
 * that, if the width or height of the image is not an even multiple of the MCU
 * block size (see #tjMCUWidth and #tjMCUHeight), then an intermediate buffer
 * copy will be performed within TurboJPEG.
 * <p>
 * NOTE: Technically, the JPEG format uses the YCbCr colorspace, but per the
 * convention of the digital video community, the TurboJPEG API uses "YUV" to
 * refer to an image format consisting of Y, Cb, and Cr image planes.
 *
 * @param handle a handle to a TurboJPEG decompressor or transformer instance
 * @param jpegBuf pointer to a buffer containing the JPEG image to decompress
 * @param jpegSize size of the JPEG image (in bytes)
 * @param dstBuf pointer to an image buffer that will receive the YUV image.
 *        Use #tjBufSizeYUV() to determine the appropriate size for this buffer
 *        based on the image width, height, and level of subsampling.
 * @param flags the bitwise OR of one or more of the @ref TJFLAG_BOTTOMUP
 *        "flags".
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
 */
DLLEXPORT int DLLCALL tjDecompressToYUV(tjhandle handle,
  unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
  int flags);


/**
 * Create a new TurboJPEG transformer instance.
 *
 * @return a handle to the newly-created instance, or NULL if an error
 * occurred (see #tjGetErrorStr().)
 */
DLLEXPORT tjhandle DLLCALL tjInitTransform(void);


/**
 * Losslessly transform a JPEG image into another JPEG image.  Lossless
 * transforms work by moving the raw coefficients from one JPEG image structure
 * to another without altering the values of the coefficients.  While this is
 * typically faster than decompressing the image, transforming it, and
 * re-compressing it, lossless transforms are not free.  Each lossless
 * transform requires reading and performing Huffman decoding on all of the
 * coefficients in the source image, regardless of the size of the destination
 * image.  Thus, this function provides a means of generating multiple
 * transformed images from the same source or  applying multiple
 * transformations simultaneously, in order to eliminate the need to read the
 * source coefficients multiple times.
 *
 * @param handle a handle to a TurboJPEG transformer instance
 * @param jpegBuf pointer to a buffer containing the JPEG image to transform
 * @param jpegSize size of the JPEG image (in bytes)
 * @param n the number of transformed JPEG images to generate
 * @param dstBufs pointer to an array of n image buffers.  <tt>dstBufs[i]</tt>
 *        will receive a JPEG image that has been transformed using the
 *        parameters in <tt>transforms[i]</tt>.  TurboJPEG has the ability to
 *        reallocate the JPEG buffer to accommodate the size of the JPEG image.
 *        Thus, you can choose to:
 *        -# pre-allocate the JPEG buffer with an arbitrary size using
 *        #tjAlloc() and let TurboJPEG grow the buffer as needed,
 *        -# set <tt>dstBufs[i]</tt> to NULL to tell TurboJPEG to allocate the
 *        buffer for you, or
 *        -# pre-allocate the buffer to a "worst case" size determined by
 *        calling #tjBufSize() with the transformed or cropped width and
 *        height.  This should ensure that the buffer never has to be
 *        re-allocated (setting #TJFLAG_NOREALLOC guarantees this.)
 *        .
 *        If you choose option 1, <tt>dstSizes[i]</tt> should be set to
 *        the size of your pre-allocated buffer.  In any case, unless you have
 *        set #TJFLAG_NOREALLOC, you should always check <tt>dstBufs[i]</tt>
 *        upon return from this function, as it may have changed.
 * @param dstSizes pointer to an array of n unsigned long variables that will
 *        receive the actual sizes (in bytes) of each transformed JPEG image.
 *        If <tt>dstBufs[i]</tt> points to a pre-allocated buffer, then
 *        <tt>dstSizes[i]</tt> should be set to the size of the buffer.  Upon
 *        return, <tt>dstSizes[i]</tt> will contain the size of the JPEG image
 *        (in bytes.)
 * @param transforms pointer to an array of n #tjtransform structures, each of
 *        which specifies the transform parameters and/or cropping region for
 *        the corresponding transformed output image.
 * @param flags the bitwise OR of one or more of the @ref TJFLAG_BOTTOMUP
 *        "flags".
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
 */
DLLEXPORT int DLLCALL tjTransform(tjhandle handle, unsigned char *jpegBuf,
  unsigned long jpegSize, int n, unsigned char **dstBufs,
  unsigned long *dstSizes, tjtransform *transforms, int flags);


/**
 * Destroy a TurboJPEG compressor, decompressor, or transformer instance.
 *
 * @param handle a handle to a TurboJPEG compressor, decompressor or
 *        transformer instance
 *
 * @return 0 if successful, or -1 if an error occurred (see #tjGetErrorStr().)
 */
DLLEXPORT int DLLCALL tjDestroy(tjhandle handle);


/**
 * Allocate an image buffer for use with TurboJPEG.  You should always use
 * this function to allocate the JPEG destination buffer(s) for #tjCompress2()
 * and #tjTransform() unless you are disabling automatic buffer
 * (re)allocation (by setting #TJFLAG_NOREALLOC.)
 *
 * @param bytes the number of bytes to allocate
 * 
 * @return a pointer to a newly-allocated buffer with the specified number of
 *         bytes
 *
 * @sa tjFree()
 */
DLLEXPORT unsigned char* DLLCALL tjAlloc(int bytes);


/**
 * Free an image buffer previously allocated by TurboJPEG.  You should always
 * use this function to free JPEG destination buffer(s) that were automatically
 * (re)allocated by #tjCompress2() or #tjTransform() or that were manually
 * allocated using #tjAlloc().
 *
 * @param buffer address of the buffer to free
 *
 * @sa tjAlloc()
 */
DLLEXPORT void DLLCALL tjFree(unsigned char *buffer);


/**
 * Returns a descriptive error message explaining why the last command failed.
 *
 * @return a descriptive error message explaining why the last command failed.
 */
DLLEXPORT char* DLLCALL tjGetErrorStr(void);


/* Backward compatibility functions and macros (nothing to see here) */
#define NUMSUBOPT TJ_NUMSAMP
#define TJ_444 TJSAMP_444
#define TJ_422 TJSAMP_422
#define TJ_420 TJSAMP_420
#define TJ_411 TJSAMP_420
#define TJ_GRAYSCALE TJSAMP_GRAY

#define TJ_BGR 1
#define TJ_BOTTOMUP TJFLAG_BOTTOMUP
#define TJ_FORCEMMX TJFLAG_FORCEMMX
#define TJ_FORCESSE TJFLAG_FORCESSE
#define TJ_FORCESSE2 TJFLAG_FORCESSE2
#define TJ_ALPHAFIRST 64
#define TJ_FORCESSE3 TJFLAG_FORCESSE3
#define TJ_FASTUPSAMPLE TJFLAG_FASTUPSAMPLE
#define TJ_YUV 512

DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height);

DLLEXPORT unsigned long DLLCALL TJBUFSIZEYUV(int width, int height,
  int jpegSubsamp);

DLLEXPORT int DLLCALL tjCompress(tjhandle handle, unsigned char *srcBuf,
  int width, int pitch, int height, int pixelSize, unsigned char *dstBuf,
  unsigned long *compressedSize, int jpegSubsamp, int jpegQual, int flags);

DLLEXPORT int DLLCALL tjEncodeYUV(tjhandle handle,
  unsigned char *srcBuf, int width, int pitch, int height, int pixelSize,
  unsigned char *dstBuf, int subsamp, int flags);

DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle handle,
  unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height);

DLLEXPORT int DLLCALL tjDecompress(tjhandle handle,
  unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
  int width, int pitch, int height, int pixelSize, int flags);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
