/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

#ifndef  LEPTONICA_BILATERAL_H
#define  LEPTONICA_BILATERAL_H

/*!
 * \file bilateral.h
 *
 * <pre>
 *  Contains the following struct
 *      struct L_Bilateral
 *
 *
 *  For a tutorial introduction to bilateral filters, which apply a
 *  gaussian blur to smooth parts of the image while preserving edges, see
 *    http://people.csail.mit.edu/sparis/bf_course/slides/03_definition_bf.pdf
 *
 *  We give an implementation of a bilateral filtering algorithm given in:
 *    "Real-Time O(1) Bilateral Filtering," by Yang, Tan and Ahuja, CVPR 2009
 *  which is at:
 *    http://vision.ai.uiuc.edu/~qyang6/publications/cvpr-09-qingxiong-yang.pdf
 *  This is based on an earlier algorithm by Sylvain Paris and Frédo Durand:
 *    http://people.csail.mit.edu/sparis/publi/2006/eccv/
 *               Paris_06_Fast_Approximation.pdf
 *
 *  The kernel of the filter is a product of a spatial gaussian and a
 *  monotonically decreasing function of the difference in intensity
 *  between the source pixel and the neighboring pixel.  The intensity
 *  part of the filter gives higher influence for pixels with intensities
 *  that are near to the source pixel, and the spatial part of the
 *  filter gives higher weight to pixels that are near the source pixel.
 *  This combination smooths in relatively uniform regions, while
 *  maintaining edges.
 *
 *  The advantage of the appoach of Yang et al is that it is separable,
 *  so the computation time is linear in the gaussian filter size.
 *  Furthermore, it is possible to do much of the computation as a reduced
 *  scale, which gives a good approximation to the full resolution version
 *  but greatly speeds it up.
 *
 *  The bilateral filtered value at x is:
 *
 *            sum[y in N(x)]: spatial(|y - x|) * range(|I(x) - I(y)|) * I(y)
 *    I'(x) = --------------------------------------------------------------
 *            sum[y in N(x)]: spatial(|y - x|) * range(|I(x) - I(y)|)
 *
 *  where I() is the input image, I'() is the filtered image, N(x) is the
 *  set of pixels around x in the filter support, and spatial() and range()
 *  are gaussian functions:
 *          spatial(x) = exp(-x^2 / (2 * s_s^2))
 *          range(x) = exp(-x^2 / (2 * s_r^2))
 *  and s_s and s_r and the standard deviations of the two gaussians.
 *
 *  Yang et al use a separable approximation to this, by defining a set
 *  of related but separable functions J(k,x), that we call Principal
 *  Bilateral Components (PBC):
 *
 *             sum[y in N(x)]: spatial(|y - x|) * range(|k - I(y)|) * I(y)
 *    J(k,x) = -----------------------------------------------------------
 *             sum[y in N(x)]: spatial(|y - x|) * range(|k - I(y)|)
 *
 *  which are computed quickly for a set of n values k[p], p = 0 ... n-1.
 *  Then each output pixel is found using a linear interpolation:
 *
 *    I'(x) = (1 - q) * J(k[p],x) + q * J(k[p+1],x)
 *
 *  where J(k[p],x) and J(k[p+1],x) are PBC for which
 *    k[p] <= I(x) and k[p+1] >= I(x), and
 *    q = (I(x) - k[p]) / (k[p+1] - k[p]).
 *
 *  We can also subsample I(x), create subsampled versions of J(k,x),
 *  which are then interpolated between for I'(x).
 *
 *  We generate 'pixsc', by optionally downscaling the input image
 *  (using area mapping by the factor 'reduction'), and then adding
 *  a mirrored border to avoid boundary cases.  This is then used
 *  to compute 'ncomps' PBCs.
 *
 *  The 'spatial_stdev' is also downscaled by 'reduction'.  The size
 *  of the 'spatial' array is 4 * (reduced 'spatial_stdev') + 1.
 *  The size of the 'range' array is 256.
 * </pre>
 */


/*------------------------------------------------------------------------*
 *                          Bilateral filter                              *
 *------------------------------------------------------------------------*/

/*! Bilateral filter */
struct L_Bilateral
{
    struct Pix     *pixs;           /*!< clone of source pix                 */
    struct Pix     *pixsc;          /*!< downscaled pix with mirrored border */
    l_int32         reduction;      /*!< 1, 2 or 4x for intermediates        */
    l_float32       spatial_stdev;  /*!< stdev of spatial gaussian           */
    l_float32       range_stdev;    /*!< stdev of range gaussian             */
    l_float32      *spatial;        /*!< 1D gaussian spatial kernel          */
    l_float32      *range;          /*!< one-sided gaussian range kernel     */
    l_int32         minval;         /*!< min value in 8 bpp pix              */
    l_int32         maxval;         /*!< max value in 8 bpp pix              */
    l_int32         ncomps;         /*!< number of intermediate results      */
    l_int32        *nc;             /*!< set of k values (size ncomps)       */
    l_int32        *kindex;         /*!< mapping from intensity to lower k   */
    l_float32      *kfract;         /*!< mapping from intensity to fract k   */
    struct Pixa    *pixac;          /*!< intermediate result images (PBC)    */
    l_uint32     ***lineset;        /*!< lineptrs for pixac                  */
};
typedef struct L_Bilateral  L_BILATERAL;


#endif  /* LEPTONICA_BILATERAL_H */
