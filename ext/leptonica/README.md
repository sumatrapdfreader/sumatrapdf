# Leptonica Library #

[![sw](https://github.com/DanBloomberg/leptonica/actions/workflows/sw.yml/badge.svg)](https://github.com/DanBloomberg/leptonica/actions/workflows/sw.yml)

www.leptonica.org

## The library supports many operations that are useful on ##

  * Document images
  * Natural images

## Fundamental image processing and image analysis operations ##

  * Rasterop (aka bitblt)
  * Affine transforms (scaling, translation, rotation, shear) on images of arbitrary pixel depth
  * Projective and bilinear transforms
  * Binary and grayscale morphology, rank order filters, and convolution
  * Seedfill and connected components
  * Image transformations with changes in pixel depth, both at the same scale and with scale change
  * Pixelwise masking, blending, enhancement, arithmetic ops, etc.

## Ancillary utilities ##

  * I/O for standard image formats (_jpg_, _png_, _tiff_, _webp_, _jp2_, _bmp_, _pnm_, _gif_, _ps_, _pdf_)
  * Utilities to handle arrays of image-related data types (e.g., _pixa_, _boxa_, _pta_)
  * Utilities for stacks, generic arrays, queues, heaps, lists, sets, ordered maps, hashmaps; number and string arrays; etc.

## Examples of some applications enabled and implemented ##

  * Octcube-based color quantization (w/ and w/out dithering)
  * Modified median cut color quantization (w/ and w/out dithering)
  * Determination of skew and orientation of text images
  * Adaptive background normalization and binarization
  * Segmentation of page images with mixed text and images
  * Color segmentation by clustering and seed-filling
  * Location of baselines and local skew determination
  * jbig2 unsupervised classifier
  * Border representations of 1 bpp images and raster conversion for SVG
  * Postscript generation (levels 1, 2 and 3) of images for device-independent output
  * PDF generation (G4, DCT, FLATE) of images for device-independent output
  * Connectivity-preserving thinning and thickening of 1 bpp images
  * Image warping (captcha, stereoscopic)
  * Image dewarping based on content (textlines)
  * Watershed transform
  * Greedy splitting of components into rectangles
  * Location of largest fg or bg rectangles in 1 bpp images
  * Search for least-cost paths on binary and grayscale images
  * Barcode reader for 1D barcodes (very early version as of 1.55)

## Implementation characteristics ##

  * _Efficient_: image data is packed binary (into 32-bit words); operations on 32-bit data whenever possible
  * _Simple_: small number of data structures; simplest implementations provided that are efficient
  * _Consistent_: data allocated on the heap with simple ownership rules; function names usually begin with primary data structure (e.g., _pix_); simple code patterns throughout
  * _Robust_: all ptr args checked; extensive use of accessors; exit not permitted
  * _Tested_: over 140 regression tests provided for basic functions and applications; valgrind tested
  * _ANSI C_: automatically generated prototype header file
  * _Portable_: endian-independent; builds in Linux, macOS, MinGW, Cygwin, Windows
  * _Thread-safe_: uses atomic operations for reference counting
  * _Documentation_: large number of in-line comments; doxygen; web pages for further background
  * _Examples_: many programs provided to test and show usage of over 2700 functions in the library


## Open Source Projects that use Leptonica ##
  * [tesseract](https://github.com/tesseract-ocr/tesseract/) (optical character recognition)
  * [OpenCV](https://github.com/opencv/opencv) (computer vision library)
  * [jbig2enc](https://github.com/agl/jbig2enc) (encodes multipage binary image documents with jbig2 compression)

## Major contributors to Leptonica ##
  * Jeff Breidenbach: Jeff has built every Debian distribution for Leptonica. He has also made many improvements to formatted image I/O, including tiff, png and pdf. He is a continuous and practical advocate for simplification.
  * David Bryan: David has worked for years to support Leptonica on multiple platforms. He designed many nice features in Leptonica, such as the severity-based error messaging system, and has identified and fixed countless bugs. And he has built and tested many distribution on cross-compilers.
  * James Le Cuirot: James has written and supported the autotools scripts on Leptonica distributions for many years, and has helped test many recent distributions.
  * Egor Pugin: Egor is co-maintainer of Leptonica on GitHub. He ported everything, including all the old distributions, from Google Code when it shut down. He set Leptonica up for appveyor and travis testing, and has implemented the sw project, which simplifies building executables on Windows.
  * Stefan Weil: Stefan is a co-maintainer of Leptonica on GitHub. He has worked from the beginning to clean up the Leptonica GitHub distribution, including removing errors in the source code.  Among other things: he suggested and implemented the use of Coverity Scan; made the library thread-safe by implementing atomic operations for reference counting; and helped remove internal struct data from the public interface.
  * Zdenko Podobny: Zdenko is a co-maintainer of Leptonica on GitHub. He has worked, mostly behind the scenes as a primary maintainer of tesseract, to help with Leptonica builds on all platforms, and coordinate with its use in tesseract.
  * Jürgen Buchmüller: Jürgen wrote text converters to modify Leptonica source code so that it generates documentation using doxygen. He also wrote tiff wrappers for memory I/O.
  * Adam Korczynski: Adam is an expert in testing libraries for safety.  He has built most of the open source fuzzers for Leptonica in the oss-fuzz project, with significant code coverage.
  * Tom Powers: Tom supported the port of Leptonica to Windows for many years.  He made many contributions to code quality and documentation.

## Installing Leptonica (vcpkg)
  * You can build and install Leptonica using [vcpkg](https://github.com/Microsoft/vcpkg/) dependency manager:

  ``` sh or powershell
      git clone https://github.com/Microsoft/vcpkg.git
      cd vcpkg
      ./bootstrap-vcpkg.sh # "./bootstrap-vcpkg.bat" for powershell
      ./vcpkg integrate install
      ./vcpkg install leptonica
  ```

  * The Leptonica port in vcpkg is kept up to date by Microsoft team members and community contributors. If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.
