/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005, 2006 Sun Microsystems, Inc.
 * Copyright (C)2009-2011 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#if (defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__)) && defined(_WIN32) && defined(DLLDEFINE)
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#define DLLCALL

/* Subsampling */
#define NUMSUBOPT 4

enum {TJ_444=0, TJ_422, TJ_420, TJ_GRAYSCALE};
#define TJ_411 TJ_420  /* for backward compatibility with VirtualGL <= 2.1.x,
                          TurboVNC <= 0.6, and TurboJPEG/IPP */

/* Flags */
#define TJ_BGR             1
  /* The components of each pixel in the source/destination bitmap are stored
     in B,G,R order, not R,G,B */
#define TJ_BOTTOMUP        2
  /* The source/destination bitmap is stored in bottom-up (Windows, OpenGL)
     order, not top-down (X11) order */
#define TJ_FORCEMMX        8
  /* Turn off CPU auto-detection and force TurboJPEG to use MMX code
     (IPP and 32-bit libjpeg-turbo versions only) */
#define TJ_FORCESSE       16
  /* Turn off CPU auto-detection and force TurboJPEG to use SSE code
     (32-bit IPP and 32-bit libjpeg-turbo versions only) */
#define TJ_FORCESSE2      32
  /* Turn off CPU auto-detection and force TurboJPEG to use SSE2 code
     (32-bit IPP and 32-bit libjpeg-turbo versions only) */
#define TJ_ALPHAFIRST     64
  /* If the source/destination bitmap is 32 bpp, assume that each pixel is
     ARGB/XRGB (or ABGR/XBGR if TJ_BGR is also specified) */
#define TJ_FORCESSE3     128
  /* Turn off CPU auto-detection and force TurboJPEG to use SSE3 code
     (64-bit IPP version only) */
#define TJ_FASTUPSAMPLE  256
  /* Use fast, inaccurate 4:2:2 and 4:2:0 YUV upsampling routines
     (libjpeg and libjpeg-turbo versions only) */
#define TJ_YUV           512
  /* Nothing to see here.  Pay no attention to the man behind the curtain. */

typedef void* tjhandle;

#define TJPAD(p) (((p)+3)&(~3))
#ifndef max
 #define max(a,b) ((a)>(b)?(a):(b))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* API follows */


/*
  tjhandle tjInitCompress(void)

  Creates a new JPEG compressor instance, allocates memory for the structures,
  and returns a handle to the instance.  Most applications will only
  need to call this once at the beginning of the program or once for each
  concurrent thread.  Don't try to create a new instance every time you
  compress an image, because this may cause performance to suffer in some
  TurboJPEG implementations.

  RETURNS: NULL on error
*/
DLLEXPORT tjhandle DLLCALL tjInitCompress(void);


/*
  int tjCompress(tjhandle j,
     unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
     unsigned char *dstbuf, unsigned long *size,
     int jpegsubsamp, int jpegqual, int flags)

  [INPUT] j = instance handle previously returned from a call to
     tjInitCompress()
  [INPUT] srcbuf = pointer to user-allocated image buffer containing RGB or
     grayscale pixels to be compressed
  [INPUT] width = width (in pixels) of the source image
  [INPUT] pitch = bytes per line of the source image (width*pixelsize if the
     bitmap is unpadded, else TJPAD(width*pixelsize) if each line of the bitmap
     is padded to the nearest 32-bit boundary, such as is the case for Windows
     bitmaps.  You can also be clever and use this parameter to skip lines,
     etc.  Setting this parameter to 0 is the equivalent of setting it to
     width*pixelsize.
  [INPUT] height = height (in pixels) of the source image
  [INPUT] pixelsize = size (in bytes) of each pixel in the source image
     RGBX/BGRX/XRGB/XBGR: 4, RGB/BGR: 3, Grayscale: 1
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the JPEG image.  Use the TJBUFSIZE(width, height) function to determine
     the appropriate size for this buffer based on the image width and height.
  [OUTPUT] size = pointer to unsigned long which receives the size (in bytes)
     of the compressed image
  [INPUT] jpegsubsamp = Specifies either 4:2:0, 4:2:2, 4:4:4, or grayscale
     subsampling.  When the image is converted from the RGB to YCbCr colorspace
     as part of the JPEG compression process, every other Cb and Cr
     (chrominance) pixel can be discarded to produce a smaller image with
     little perceptible loss of image clarity (the human eye is more sensitive
     to small changes in brightness than small changes in color.)

     TJ_420: 4:2:0 subsampling.  Discards every other Cb, Cr pixel in both
        horizontal and vertical directions
     TJ_422: 4:2:2 subsampling.  Discards every other Cb, Cr pixel only in
        the horizontal direction
     TJ_444: no subsampling
     TJ_GRAYSCALE: Generate grayscale JPEG image

  [INPUT] jpegqual = JPEG quality (an integer between 0 and 100 inclusive)
  [INPUT] flags = the bitwise OR of one or more of the flags described in the
     "Flags" section above

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjCompress(tjhandle j,
	unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
	unsigned char *dstbuf, unsigned long *size,
	int jpegsubsamp, int jpegqual, int flags);


/*
  unsigned long TJBUFSIZE(int width, int height)

  Convenience function which returns the maximum size of the buffer required to
  hold a JPEG image with the given width and height

  RETURNS: -1 if arguments are out of bounds
*/
DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height);


/*
  unsigned long TJBUFSIZEYUV(int width, int height, int subsamp)

  Convenience function which returns the size of the buffer required to
  hold a YUV planar image with the given width, height, and level of
  chrominance subsampling

  RETURNS: -1 if arguments are out of bounds
*/
DLLEXPORT unsigned long DLLCALL TJBUFSIZEYUV(int width, int height,
  int subsamp);


/*
  int tjEncodeYUV(tjhandle j,
     unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
     unsigned char *dstbuf, int subsamp, int flags)

  This function uses the accelerated color conversion routines in TurboJPEG's
  underlying codec to produce a planar YUV image that is suitable for X Video.
  Specifically, if the chrominance components are subsampled along the
  horizontal dimension, then the width of the luminance plane is padded to 2 in
  the output image (same goes for the height of the luminance plane, if the
  chrominance components are subsampled along the vertical dimension.)  Also,
  each line of each plane in the output image is padded to 4 bytes.  Although
  this will work with any subsampling option, it is really only useful in
  combination with TJ_420, which produces an image compatible with the I420
  (AKA "YUV420P") format.

  [INPUT] j = instance handle previously returned from a call to
     tjInitCompress()
  [INPUT] srcbuf = pointer to user-allocated image buffer containing RGB or
     grayscale pixels to be encoded
  [INPUT] width = width (in pixels) of the source image
  [INPUT] pitch = bytes per line of the source image (width*pixelsize if the
     bitmap is unpadded, else TJPAD(width*pixelsize) if each line of the bitmap
     is padded to the nearest 32-bit boundary, such as is the case for Windows
     bitmaps.  You can also be clever and use this parameter to skip lines,
     etc.  Setting this parameter to 0 is the equivalent of setting it to
     width*pixelsize.
  [INPUT] height = height (in pixels) of the source image
  [INPUT] pixelsize = size (in bytes) of each pixel in the source image
     RGBX/BGRX/XRGB/XBGR: 4, RGB/BGR: 3, Grayscale: 1
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the YUV image.  Use the TJBUFSIZEYUV(width, height, subsamp) function to
     determine the appropriate size for this buffer based on the image width,
     height, and level of subsampling.
  [INPUT] subsamp = Specifies either 4:2:0, 4:2:2, 4:4:4, or grayscale
     subsampling (see description under tjCompress())
  [INPUT] flags = the bitwise OR of one or more of the flags described in the
     "Flags" section above

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjEncodeYUV(tjhandle j,
	unsigned char *srcbuf, int width, int pitch, int height, int pixelsize,
	unsigned char *dstbuf, int subsamp, int flags);


/*
  tjhandle tjInitDecompress(void)

  Creates a new JPEG decompressor instance, allocates memory for the
  structures, and returns a handle to the instance.  Most applications will
  only need to call this once at the beginning of the program or once for each
  concurrent thread.  Don't try to create a new instance every time you
  decompress an image, because this may cause performance to suffer in some
  TurboJPEG implementations.

  RETURNS: NULL on error
*/
DLLEXPORT tjhandle DLLCALL tjInitDecompress(void);


/*
  int tjDecompressHeader2(tjhandle j,
     unsigned char *srcbuf, unsigned long size,
     int *width, int *height, int *jpegsubsamp)

  [INPUT] j = instance handle previously returned from a call to
     tjInitDecompress()
  [INPUT] srcbuf = pointer to a user-allocated buffer containing a JPEG image
  [INPUT] size = size of the JPEG image buffer (in bytes)
  [OUTPUT] width = width (in pixels) of the JPEG image
  [OUTPUT] height = height (in pixels) of the JPEG image
  [OUTPUT] jpegsubsamp = type of chrominance subsampling used when compressing
     the JPEG image

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height, int *jpegsubsamp);

/*
  Legacy version of the above function
*/
DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height);


/*
  int tjDecompress(tjhandle j,
     unsigned char *srcbuf, unsigned long size,
     unsigned char *dstbuf, int width, int pitch, int height, int pixelsize,
     int flags)

  [INPUT] j = instance handle previously returned from a call to
     tjInitDecompress()
  [INPUT] srcbuf = pointer to a user-allocated buffer containing the JPEG image
     to decompress
  [INPUT] size = size of the JPEG image buffer (in bytes)
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the bitmap image.  This buffer should normally be pitch*height
     bytes in size, although this pointer may also be used to decompress into
     a specific region of a larger buffer.
  [INPUT] width = width (in pixels) of the destination image
  [INPUT] pitch = bytes per line of the destination image (width*pixelsize if
     the bitmap is unpadded, else TJPAD(width*pixelsize) if each line of the
     bitmap is padded to the nearest 32-bit boundary, such as is the case for
     Windows bitmaps.  You can also be clever and use this parameter to skip
     lines, etc.  Setting this parameter to 0 is the equivalent of setting it
     to width*pixelsize.
  [INPUT] height = height (in pixels) of the destination image
  [INPUT] pixelsize = size (in bytes) of each pixel in the destination image
     RGBX/BGRX/XRGB/XBGR: 4, RGB/BGR: 3, Grayscale: 1
  [INPUT] flags = the bitwise OR of one or more of the flags described in the
     "Flags" section above.

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDecompress(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int width, int pitch, int height, int pixelsize,
	int flags);


/*
  int tjDecompressToYUV(tjhandle j,
     unsigned char *srcbuf, unsigned long size,
     unsigned char *dstbuf, int flags)

  This function performs JPEG decompression but leaves out the color conversion
  step, so a planar YUV image is generated instead of an RGB image.  The
  padding of the planes in this image is the same as in tjEncodeYUV().
  Note that, if the width or height of the output image is not a multiple of 8
  (or a multiple of 16 along any dimension in which chrominance subsampling is
  used), then an intermediate buffer copy will be performed within TurboJPEG.

  [INPUT] j = instance handle previously returned from a call to
     tjInitDecompress()
  [INPUT] srcbuf = pointer to a user-allocated buffer containing the JPEG image
     to decompress
  [INPUT] size = size of the JPEG image buffer (in bytes)
  [INPUT] dstbuf = pointer to user-allocated image buffer which will receive
     the YUV image.  Use the TJBUFSIZEYUV(width, height, subsamp) function to
     determine the appropriate size for this buffer based on the image width,
     height, and level of subsampling.
  [INPUT] flags = the bitwise OR of one or more of the flags described in the
     "Flags" section above.

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDecompressToYUV(tjhandle j,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int flags);


/*
  int tjDestroy(tjhandle h)

  Frees structures associated with a compression or decompression instance
  
  [INPUT] h = instance handle (returned from a previous call to
     tjInitCompress() or tjInitDecompress()

  RETURNS: 0 on success, -1 on error
*/
DLLEXPORT int DLLCALL tjDestroy(tjhandle h);


/*
  char *tjGetErrorStr(void)
  
  Returns a descriptive error message explaining why the last command failed
*/
DLLEXPORT char* DLLCALL tjGetErrorStr(void);

#ifdef __cplusplus
}
#endif
