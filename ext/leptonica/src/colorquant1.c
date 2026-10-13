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

/*!
 * \file colorquant1.c
 * <pre>
 *
 *  Octcube color quantization
 *
 *  There are several different octcube/octree based quantizations.
 *  These can be classified, in the order in which they appear in this
 *  file, as follows:
 *
 *  -----------------------------------------------------------------
 *  (1) General adaptive octree
 *  (2) Adaptive octree by population at fixed level
 *  (3) Adaptive octree using population and with specified number
 *      of output colors
 *  (4) Octcube with colormap representation of mixed color/gray
 *  (5) 256 fixed octcubes covering color space
 *  (6) Octcubes at fixed level for ncolors <= 256
 *  (7) Octcubes at fixed level with RGB output
 *  (8) Quantizing an rgb image using a specified colormap
 *  -----------------------------------------------------------------
 *
 *  (1) Two-pass adaptive octree color quantization
 *          PIX              *pixOctreeColorQuant()
 *          PIX              *pixOctreeColorQuantGeneral()
 *
 *        which calls
 *          static CQCELL  ***octreeGenerateAndPrune()
 *          static PIX       *pixOctreeQuantizePixels()
 *
 *        which calls
 *          static l_int32    octreeFindColorCell()
 *
 *      Helper cqcell functions
 *          static CQCELL  ***cqcellTreeCreate()
 *          static void       cqcellTreeDestroy()
 *
 *      Helper index functions
 *          l_int32           makeRGBToIndexTables()
 *          void              getOctcubeIndexFromRGB()
 *          static void       getRGBFromOctcube()
 *          static l_int32    getOctcubeIndices()
 *          static l_int32    octcubeGetCount()
 *
 *  (2) Adaptive octree quantization based on population at a fixed level
 *          PIX              *pixOctreeQuantByPopulation()
 *          static l_int32    pixDitherOctindexWithCmap()
 *
 *  (3) Adaptive octree quantization to 4 and 8 bpp with specified
 *      number of output colors in colormap
 *          PIX              *pixOctreeQuantNumColors()
 *
 *  (4) Mixed color/gray quantization with specified number of colors
 *          PIX              *pixOctcubeQuantMixedWithGray()
 *
 *  (5) Fixed partition octcube quantization with 256 cells
 *          PIX              *pixFixedOctcubeQuant256()
 *
 *  (6) Fixed partition quantization for images with few colors
 *          PIX              *pixFewColorsOctcubeQuant1()
 *          PIX              *pixFewColorsOctcubeQuant2()
 *          PIX              *pixFewColorsOctcubeQuantMixed()
 *
 *  (7) Fixed partition octcube quantization at specified level
 *      with quantized output to RGB
 *          PIX              *pixFixedOctcubeQuantGenRGB()
 *
 *  (8) Color quantize RGB image using existing colormap
 *          PIX              *pixQuantFromCmap()  [high-level wrapper]
 *          PIX              *pixOctcubeQuantFromCmap()
 *          static PIX       *pixOctcubeQuantFromCmapLUT()
 *
 *      Generation of octcube histogram
 *          NUMA             *pixOctcubeHistogram()
 *
 *      Get filled octcube table from colormap
 *          l_int32          *pixcmapToOctcubeLUT()
 *
 *      Strip out unused elements in colormap
 *          l_int32           pixRemoveUnusedColors()
 *
 *      Find number of occupied octcubes at the specified level
 *          l_int32           pixNumberOccupiedOctcubes()
 *
 *  Notes:
 *        Leptonica also provides color quantization using a modified
 *        form of median cut.  See colorquant2.c for details.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

/*
 * <pre>
 *   This data structure is used for pixOctreeColorQuant(),
 *   a color octree that adjusts to the color distribution
 *   in the image that is being quantized.  The best settings
 *   are with CqNLevels = 6 and DITHERING set on.
 *
 * Notes:
 *      (1) the CTE (color table entry) index is sequentially
 *          assigned as the tree is pruned back
 *      (2) if 'bleaf' == 1, all pixels in that cube have been
 *          assigned to one or more CTEs.  But note that if
 *          all 8 subcubes have 'bleaf' == 1, it will have no
 *          pixels left for assignment and will not be a CTE.
 *      (3) 'nleaves', the number of leaves contained at the next
 *          lower level is some number between 0 and 8, inclusive.
 *          If it is zero, it means that all colors within this cube
 *          are part of a single growing cluster that has not yet
 *          been set aside as a leaf.  If 'nleaves' > 0, 'bleaf'
 *          will be set to 1 and all pixels not assigned to leaves
 *          at lower levels will be assigned to a CTE here.
 *          (However, as described above, if all pixels are already
 *          assigned, we set 'bleaf' = 1 but do not create a CTE
 *          at this level.)
 *      (4) To keep the maximum color error to a minimum, we
 *          prune the tree back to level 2, and require that
 *          all 64 level 2 cells are CTEs.
 *      (5) We reserve an extra set of colors to prevent running out
 *          of colors during the assignment of the final 64 level 2 cells.
 *          This is more likely to happen with small images.
 *      (6) When we run out of colors, the dithered image can be very
 *          poor, so we additionally prevent dithering if the image
 *          is small.
 *      (7) The color content of the image is measured, and if there
 *          is very little color, it is quantized in grayscale.
 * </pre>
 */
struct ColorQuantCell
{
    l_int32     rc, gc, bc;   /* center values                              */
    l_int32     n;            /* number of samples in this cell             */
    l_int32     index;        /* CTE (color table entry) index              */
    l_int32     nleaves;      /* # of leaves contained at next lower level  */
    l_int32     bleaf;        /* boolean: 0 if not a leaf, 1 if so          */
};
typedef struct ColorQuantCell    CQCELL;

    /* Constants for pixOctreeColorQuant() */
static const l_int32  CqNLevels = 5;   /* only 4, 5 and 6 are allowed */
static const l_int32  CqReservedColors = 64;     /* to allow for level 2 */
                                                 /* remainder CTEs */
static const l_int32  ExtraReservedColors = 25;  /* to avoid running out */
static const l_int32  TreeGenWidth = 350;      /* big enough for good stats */
static const l_int32  MinDitherSize = 250;     /* don't dither if smaller */

/*
 * <pre>
 *   This data structure is used for pixOctreeQuantNumColors(),
 *   a color octree that adjusts in a simple way to the to the color
 *   distribution in the image that is being quantized.  It outputs
 *   colormapped images, either 4 bpp or 8 bpp, depending on the
 *   max number of colors and the compression desired.
 *
 *   The number of samples is saved as a float in the first location,
 *   because this is required to use it as the key that orders the
 *   cells in the priority queue.
 * </pre>
 * */
struct OctcubeQuantCell
{
    l_float32  n;                  /* number of samples in this cell       */
    l_int32    octindex;           /* octcube index                        */
    l_int32    rcum, gcum, bcum;   /* cumulative values                    */
    l_int32    rval, gval, bval;   /* average values                       */
};
typedef struct OctcubeQuantCell    OQCELL;

/*
 * <pre>
 *   This data structure is using for heap sorting octcubes
 *   by population.  Sort order is decreasing.
 * </pre>
 */
struct L_OctcubePop
{
    l_float32        npix;    /* parameter on which to sort  */
    l_int32          index;   /* octcube index at assigned level */
    l_int32          rval;    /* mean red value of pixels in octcube */
    l_int32          gval;    /* mean green value of pixels in octcube */
    l_int32          bval;    /* mean blue value of pixels in octcube */
};
typedef struct L_OctcubePop  L_OCTCUBE_POP;

/*
 * <pre>
 *   In pixDitherOctindexWithCmap(), we use these default values.
     To get the max value of 'dif' in the dithering color transfer,
     divide these "DIF_CAP" values by 8.  However, a value of
     0 means that there is no cap (infinite cap).  A very small
     value is used for POP_DIF_CAP because dithering on the population
     generated colormap can be unstable without a tight cap.
 * </pre>
 */

static const l_int32  FIXED_DIF_CAP = 0;
static const l_int32  POP_DIF_CAP = 40;


    /* Static octree helper function */
static l_int32 octreeFindColorCell(l_int32 octindex, CQCELL ***cqcaa,
                                   l_int32 *pindex, l_int32 *prval,
                                   l_int32 *pgval, l_int32 *pbval);

    /* Static cqcell functions */
static CQCELL ***octreeGenerateAndPrune(PIX *pixs, l_int32 colors,
                                        l_int32 reservedcolors,
                                        PIXCMAP **pcmap);
static PIX *pixOctreeQuantizePixels(PIX *pixs, CQCELL ***cqcaa,
                                    l_int32 ditherflag);
static CQCELL ***cqcellTreeCreate(void);
static void cqcellTreeDestroy(CQCELL ****pcqcaa);

    /* Static helper octcube index functions */
static void getRGBFromOctcube(l_int32 cubeindex, l_int32 level,
                              l_int32 *prval, l_int32 *pgval, l_int32 *pbval);
static l_int32 getOctcubeIndices(l_int32 rgbindex, l_int32 level,
                                 l_int32 *pbindex, l_int32 *psindex);
static l_int32 octcubeGetCount(l_int32 level, l_int32 *psize);

    /* Static function to perform octcube-indexed dithering */
static l_int32 pixDitherOctindexWithCmap(PIX *pixs, PIX *pixd, l_uint32 *rtab,
                                         l_uint32 *gtab, l_uint32 *btab,
                                         l_int32 *carray, l_int32 difcap);

    /* Static function to perform octcube-based quantizing from colormap */
static PIX *pixOctcubeQuantFromCmapLUT(PIX *pixs, PIXCMAP *cmap,
                                       l_int32 mindepth, l_int32 *cmaptab,
                                       l_uint32 *rtab, l_uint32 *gtab,
                                       l_uint32 *btab);

#ifndef   NO_CONSOLE_IO
#define   DEBUG_COLORQUANT      0
#define   DEBUG_OCTINDEX        0
#define   DEBUG_OCTCUBE_CMAP    0
#define   DEBUG_POP             0
#define   DEBUG_FEW_COLORS      0
#define   PRINT_OCTCUBE_STATS   0
#endif   /* ~NO_CONSOLE_IO */

/*-------------------------------------------------------------------------*
 *                Two-pass adaptive octree color quantization              *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixOctreeColorQuant()
 *
 * \param[in]    pixs         32 bpp; 24-bit color
 * \param[in]    colors       in colormap; some number in range [128 ... 256];
 *                            the actual number of colors used will be smaller
 * \param[in]    ditherflag   1 to dither, 0 otherwise
 * \return  pixd 8 bpp with colormap, or NULL on error
 *
 * <pre>
 *  I found one description in the literature of octree color
 *  quantization, using progressive truncation of the octree,
 *  by M. Gervautz and W. Purgathofer in Graphics Gems, pp.
 *  287-293, ed. A. Glassner, Academic Press, 1990.
 *  Rather than setting up a fixed partitioning of the color
 *  space ab initio, as we do here, they allow the octree to be
 *  progressively truncated as new pixels are added.  They
 *  need to set up some data structures that are traversed
 *  with the addition of each 24 bit pixel, in order to decide
 *  either 1) in which cluster (sub-branch of the octree to put
 *  the pixel, or 2 whether to truncate the octree further
 *  to place the pixel in an existing cluster, or 3 which
 *  two existing clusters should be merged so that the pixel
 *  can be left to start a truncated leaf of the octree.  Such dynamic
 *  truncation is considerably more complicated, and Gervautz et
 *  al. did not explain how they did it in anywhere near the
 *  detail required to check their implementation.
 *
 *  The simple method in pixFixedOctcubeQuant256 is very
 *  fast, and with dithering the results are good, but you
 *  can do better if the color clusters are selected adaptively
 *  from the image.  We want a method that makes much better
 *  use of color samples in regions of color space with high
 *  pixel density, while also fairly representing small numbers
 *  of color pixels in low density regions.  Such adaptation
 *  requires two passes through the image: the first for generating
 *  the pruned tree of color cubes and the second for computing the index
 *  into the color table for each pixel.
 *
 *  A relatively simple adaptive method is pixOctreeQuantByPopulation.
 *  That function first determines if the image has very few colors,
 *  and, if so, quantizes to those colors.  If there are more than
 *  256 colors, it generates a histogram of octcube leaf occupancy
 *  at level 4, chooses the 192 most populated such leaves as
 *  the first 192 colors, and sets the remaining 64 colors to the
 *  residual average pixel values in each of the 64 level 2 octcubes.
 *  This is a bit faster than pixOctreeColorQuant, and does very
 *  well without dithering, but for most images with dithering it
 *  is clearly inferior.
 *
 *  We now describe pixOctreeColorQuant.  The first pass is done
 *  on a subsampled image, because we do not need to use all the
 *  pixels in the image to generate the tree.  Subsampling
 *  down to 0.25 1/16 of the pixels makes the program run
 *  about 1.3 times faster.
 *
 *  Instead of dividing the color space into 256 equal-sized
 *  regions, we initially divide it into 2^12 or 2^15 or 2^18
 *  equal-sized octcubes.  Suppose we choose to use 2^18 octcubes.
 *  This gives us 6 octree levels.  We then prune back,
 *  starting from level 6.  For every cube at level 6, there
 *  are 8 cubes at level 5.  Call the operation of putting a
 *  cube aside as a color table entry CTE a "saving."
 *  We use a in general level-dependent threshold, and save
 *  those level 6 cubes that are above threshold.
 *  The rest are combined into the containing level 5 cube.
 *  If between 1 and 7 level 6 cubes within a level 5
 *  cube have been saved by thresholding, then the remaining
 *  level 6 cubes in that level 5 cube are automatically
 *  saved as well, without applying a threshold.  This greatly
 *  simplifies both the description of the CTEs and the later
 *  classification of each pixel as belonging to a CTE.
 *  This procedure is iterated through every cube, starting at
 *  level 5, and then 4, 3, and 2, successively.  The result is that
 *  each CTE contains the entirety of a set of from 1 to 7 cubes
 *  from a given level that all belong to a single cube at the
 *  level above.   We classify the CTEs in terms of the
 *  condition in which they are made as either being "threshold"
 *  or "residual."  They are "threshold" CTEs if no subcubes
 *  are CTEs that is, they contain every pixel within the cube
 *  and the number of pixels exceeds the threshold for making
 *  a CTE.  They are "residual" CTEs if at least one but not more
 *  than 7 of the subcubes have already been determined to be CTEs;
 *  this happens automatically -- no threshold is applied.
 *  If all 8 subcubes are determined to be CTEs, the cube is
 *  marked as having all pixels accounted for 'bleaf' = 1 but
 *  is not saved as a CTE.
 *
 *  We stop the pruning at level 2, at which there are 64
 *  sub-cubes.  Any pixels not already claimed in a CTE are
 *  put in these cubes.
 *
 *  As the cubes are saved as color samples in the color table,
 *  the number of remaining pixels P and the number of
 *  remaining colors in the color table N are recomputed,
 *  along with the average number of pixels P/N ppc to go in
 *  each of the remaining colors.  This running average number is
 *  used to set the threshold at the current level.
 *
 *  Because we are going to very small cubes at levels 6 or 5,
 *  and will dither the colors for errors, it is not necessary
 *  to compute the color center of each cluster; we can simply
 *  use the center of the cube.  This gives us a minimax error
 *  condition: the maximum error is half the width of the
 *  level 2 cubes -- 32 color values out of 256 -- for each color
 *  sample.  In practice, most of the pixels will be very much
 *  closer to the center of their cells.  And with dithering,
 *  the average pixel color in a small region will be closer still.
 *  Thus with the octree quantizer, we are able to capture
 *  regions of high color pdf probability density function in small
 *  but accurate CTEs, and to have only a small number of pixels
 *  that end up a significant distance with a guaranteed maximum
 *  from their true color.
 *
 *  How should the threshold factor vary?  Threshold factors
 *  are required for levels 2, 3, 4 and 5 in the pruning stage.
 *  The threshold for level 5 is actually applied to cubes at
 *  level 6, etc.  From various experiments, it appears that
 *  the results do not vary appreciably for threshold values near 1.0.
 *  If you want more colors in smaller cubes, the threshold
 *  factors can be set lower than 1.0 for cubes at levels 4 and 5.
 *  However, if the factor is set much lower than 1.0 for
 *  levels 2 and 3, we can easily run out of colors.
 *  We put aside 64 colors in the calculation of the threshold
 *  values, because we must have 64 color centers at level 2,
 *  that will have very few pixels in most of them.
 *  If we reduce the factor for level 5 to 0.4, this will
 *  generate many level 6 CTEs, and consequently
 *  many residual cells will be formed up from those leaves,
 *  resulting in the possibility of running out of colors.
 *  Remember, the residual CTEs are mandatory, and are formed
 *  without using the threshold, regardless of the number of
 *  pixels that are absorbed.
 *
 *  The implementation logically has four parts:
 *
 *       1 accumulation into small, fixed cells
 *       2 pruning back into selected CTE cubes
 *       3 organizing the CTEs for fast search to find
 *           the CTE to which any image pixel belongs
 *       4 doing a second scan to code the image pixels by CTE
 *
 *  Step 1 is straightforward; we use 2^15 cells.
 *
 *  We've already discussed how the pruning step 2 will be performed.
 *
 *  Steps 3) and (4 are related, in that the organization
 *  used by step 3 determines how the search actually
 *  takes place for each pixel in step 4.
 *
 *  There are many ways to do step 3.  Let's explore a few.
 *
 *  a The simplest is to order the cubes from highest occupancy
 *      to lowest, and traverse the list looking for the deepest
 *      match.  To make this more efficient, so that we know when
 *      to stop looking, any cube that has separate CTE subcubes
 *      would be marked as such, so that we know when we hit a
 *      true leaf.
 *
 *  b Alternatively, we can order the cubes by highest
 *      occupancy separately each level, and work upward,
 *      starting at level 5, so that when we find a match we
 *      know that it will be correct.
 *
 *  c Another approach would be to order the cubes by
 *      "address" and use a hash table to find the cube
 *      corresponding to a pixel color.  I don't know how to
 *      do this with a variable length address, as each CTE
 *      will have 3*n bits, where n is the level.
 *
 *  d Another approach entirely is to put the CTE cubes into
 *      a tree, in such a way that starting from the root, and
 *      using 3 bits of address at a time, the correct branch of
 *      each octree can be taken until a leaf is found.  Because
 *      a given cube can be both a leaf and also have branches
 *      going to sub-cubes, the search stops only when no
 *      marked subcubes have addresses that match the given pixel.
 *
 *      In the tree method, we can start with a dense infrastructure,
 *      and place the leaves corresponding to the N colors
 *      in the tree, or we can grow from the root only those
 *      branches that end directly on leaves.
 *
 *  What we do here is to take approach d, and implement the tree
 *  "virtually", as a set of arrays, one array for each level
 *  of the tree.   Initially we start at level 5, an array with
 *  2^15 cubes, each with 8 subcubes.  We then build nodes at
 *  levels closer to the root; at level 4 there are 2^12 nodes
 *  each with 8 subcubes; etc.  Using these arrays has
 *  several advantages:
 *
 *     ~  We don't need to keep track of links between cubes
 *        and subcubes, because we can use the canonical
 *        addressing on the cell arrays directly to determine
 *        which nodes are parent cubes and which are sub-cubes.
 *
 *     ~  We can prune directly on this tree
 *
 *     ~  We can navigate the pruned tree quickly to classify
 *        each pixel in the image.
 *
 *  Canonical addressing guarantees that the i-th node at level k
 *  has 8 subnodes given by the 8*i ... 8*i+7 nodes at level k+1.
 *
 *  The pruning step works as follows.  We go from the lowest
 *  level up.  At each level, the threshold is found from the
 *  product of a factor near 1.0 and the ratio of unmarked pixels
 *  to remaining colors minus the 64.  We march through
 *  the space, sequentially considering a cube and its 8 subcubes.
 *  We first check those subcubes that are not already
 *  marked as CTE to see if any are above threshold, and if so,
 *  generate a CTE and mark them as such.
 *  We then determine if any of the subcubes have been marked.
 *  If so, and there are subcubes that are not marked,
 *  we generate a CTE for the cube from the remaining unmarked
 *  subcubes; this is mandatory and does not depend on how many
 *  pixels are in the set of subcubes.  If none of the subcubes
 *  are marked, we aggregate their pixels into the cube
 *  containing them, but do not mark it as a CTE; that
 *  will be determined when iterating through the next level up.
 *
 *  When all the pixels in a cube are accounted for in one or more
 *  colors, we set the boolean 'bleaf' to true.  This is the
 *  flag used to mark the cubes in the pruning step.  If a cube
 *  is marked, and all 8 subcubes are marked, then it is not
 *  itself given a CTE because all pixels have already been
 *  accounted for.
 *
 *  Note that the pruning of the tree and labelling of the CTEs
 *  step 2 accomplishes step 3 implicitly, because the marked
 *  and pruned tree is ready for use in labelling each pixel
 *  in step 4.  We now, for every pixel in the image, traverse
 *  the tree from the root, looking for the lowest cube that is a leaf.
 *  At each level we have a cube and subcube.  If we reach a subcube
 *  leaf that is marked 0, we know that the color is stored in the
 *  cube above, and we've found the CTE.  Otherwise, the subcube
 *  leaf is marked 1.  If we're at the last level, we've reached
 *  the final leaf and must use it.  Otherwise, continue the
 *  process at the next level down.
 *
 *  For robustness, efficiency and high quality output, we do the following:
 *
 *  (1) Measure the color content of the image.  If there is very little
 *      color, quantize in grayscale.
 *  (2) For efficiency, build the octree with a subsampled image if the
 *      image is larger than some threshold size.
 *  (3) Reserve an extra set of colors to prevent running out of colors
 *      when pruning the octree; specifically, during the assignment
 *      of those level 2 cells out of the 64 that have unassigned
 *      pixels.  The problem of running out is more likely to happen
 *      with small images, because the estimation we use for the
 *      number of pixels available is not accurate.
 *  (4) In the unlikely event that we run out of colors, the dithered
 *      image can be very poor.  As this would only happen with very
 *      small images, and dithering is not particularly noticeable with
 *      such images, turn it off.
 * </pre>
 */
PIX *
pixOctreeColorQuant(PIX     *pixs,
                    l_int32  colors,
                    l_int32  ditherflag)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (colors < 128 || colors > 240)  /* further restricted */
        return (PIX *)ERROR_PTR("colors must be in [128, 240]", __func__, NULL);

    return pixOctreeColorQuantGeneral(pixs, colors, ditherflag, 0.01f, 0.01f);
}


/*!
 * \brief   pixOctreeColorQuantGeneral()
 *
 * \param[in]    pixs          32 bpp; 24-bit color
 * \param[in]    colors        in colormap; some number in range [128 ... 240];
 *                             the actual number of colors used will be smaller
 * \param[in]    ditherflag    1 to dither, 0 otherwise
 * \param[in]    validthresh   minimum fraction of pixels neither near white
 *                             nor black, required for color quantization;
 *                             typically ~0.01, but smaller for images that have
 *                             color but are nearly all white
 * \param[in]    colorthresh   minimum fraction of pixels with color that are
 *                             not near white or black, that are required
 *                             for color quantization; typ. ~0.01, but smaller
 *                             for images that have color along with a
 *                             significant fraction of gray
 * \return  pixd 8 bit with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The parameters %validthresh and %colorthresh are used to
 *          determine if color quantization should be used on an image,
 *          or whether, instead, it should be quantized in grayscale.
 *          If the image has very few non-white and non-black pixels, or
 *          if those pixels that are non-white and non-black are all
 *          very close to either white or black, it is usually better
 *          to treat the color as accidental and to quantize the image
 *          to gray only.  These parameters are useful if you know
 *          something a priori about the image.  Perhaps you know that
 *          there is only a very small fraction of color pixels, but they're
 *          important to preserve; then you want to use a smaller value for
 *          these parameters.  To disable conversion to gray and force
 *          color quantization, use %validthresh = 0.0 and %colorthresh = 0.0.
 *      (2) See pixOctreeColorQuant() for algorithmic and implementation
 *          details.  This function has a more general interface.
 *      (3) See pixColorFraction() for computing the fraction of pixels
 *          that are neither white nor black, and the fraction of those
 *          pixels that have little color.  From the documentation there:
 *             If pixfract is very small, there are few pixels that are
 *             neither black nor white.  If colorfract is very small,
 *             the pixels that are neither black nor white have very
 *             little color content.  The product 'pixfract * colorfract'
 *             gives the fraction of pixels with significant color content.
 *          We test against the product %validthresh * %colorthresh
 *          to find color in images that have either very few
 *          intermediate gray pixels or that have many such gray pixels.
 * </pre>
 */
PIX *
pixOctreeColorQuantGeneral(PIX       *pixs,
                           l_int32    colors,
                           l_int32    ditherflag,
                           l_float32  validthresh,
                           l_float32  colorthresh)
{
l_int32    w, h, minside, factor, index, rval, gval, bval;
l_float32  scalefactor;
l_float32  pixfract;  /* fraction neither near white nor black */
l_float32  colorfract;  /* fraction with color of the pixfract population */
CQCELL  ***cqcaa;
PIX       *pixd, *pixsub;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (colors < 128 || colors > 240)
        return (PIX *)ERROR_PTR("colors must be in [128, 240]", __func__, NULL);

        /* Determine if the image has sufficient color content for
         *   octree quantization, based on the input thresholds.
         * If pixfract << 1, most pixels are close to black or white.
         * If colorfract << 1, the pixels that are not near
         *   black or white have very little color.
         * If with insufficient color, quantize with a grayscale colormap. */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (validthresh > 0.0 && colorthresh > 0.0) {
        minside = L_MIN(w, h);
        factor = L_MAX(1, minside / 400);
        pixColorFraction(pixs, 20, 244, 20, factor, &pixfract, &colorfract);
        if (pixfract * colorfract < validthresh * colorthresh) {
            L_INFO("\n  Pixel fraction neither white nor black = %6.3f"
                   "\n  Color fraction of those pixels = %6.3f"
                   "\n  Quantizing to 8 bpp gray\n",
                   __func__, pixfract, colorfract);
            return pixConvertTo8(pixs, 1);
        }
    } else {
        L_INFO("\n  Process in color by default\n", __func__);
    }

        /* Conditionally subsample to speed up the first pass */
    if (w > TreeGenWidth) {
        scalefactor = (l_float32)TreeGenWidth / (l_float32)w;
        pixsub = pixScaleBySampling(pixs, scalefactor, scalefactor);
    } else {
        pixsub = pixClone(pixs);
    }

        /* Drop the number of requested colors if image is very small */
    if (w < MinDitherSize && h < MinDitherSize)
        colors = L_MIN(colors, 220);

        /* Make the pruned octree */
    cqcaa = octreeGenerateAndPrune(pixsub, colors, CqReservedColors, &cmap);
    if (!cqcaa) {
        pixDestroy(&pixsub);
        return (PIX *)ERROR_PTR("tree not made", __func__, NULL);
    }
#if DEBUG_COLORQUANT
    L_INFO(" Colors requested = %d\n", __func__, colors);
    L_INFO(" Actual colors = %d\n", __func__, cmap->n);
#endif  /* DEBUG_COLORQUANT */

        /* Do not dither if image is very small */
    if (w < MinDitherSize && h < MinDitherSize && ditherflag == 1) {
        L_INFO("Small image: dithering turned off\n", __func__);
        ditherflag = 0;
    }

        /* Traverse tree from root, looking for lowest cube
         * that is a leaf, and set dest pix value to its
         * colortable index */
    if ((pixd = pixOctreeQuantizePixels(pixs, cqcaa, ditherflag)) == NULL) {
        pixDestroy(&pixsub);
        cqcellTreeDestroy(&cqcaa);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }

        /* Attach colormap and copy res */
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);

        /* Force darkest color to black if each component <= 4 */
    pixcmapGetRankIntensity(cmap, 0.0, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval < 5 && gval < 5 && bval < 5)
        pixcmapResetColor(cmap, index, 0, 0, 0);

        /* Force lightest color to white if each component >= 252 */
    pixcmapGetRankIntensity(cmap, 1.0, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval > 251 && gval > 251 && bval > 251)
        pixcmapResetColor(cmap, index, 255, 255, 255);

    cqcellTreeDestroy(&cqcaa);
    pixDestroy(&pixsub);
    return pixd;
}


/*!
 * \brief   octreeGenerateAndPrune()
 *
 * \param[in]    pixs
 * \param[in]    colors           number of colors to use between 128 and 256
 * \param[in]    reservedcolors   number of reserved colors
 * \param[out]   pcmap            colormap returned
 * \return  octree, colormap and number of colors used, or NULL
 *              on error
 *
 * <pre>
 * Notes:
 *      (1) The number of colors in the cmap may differ from the number
 *          of colors requested, but it will not be larger than 256
 * </pre>
 */
static CQCELL ***
octreeGenerateAndPrune(PIX       *pixs,
                       l_int32    colors,
                       l_int32    reservedcolors,
                       PIXCMAP  **pcmap)
{
l_int32    rval, gval, bval, cindex;
l_int32    level, ncells, octindex;
l_int32    w, h, wpls;
l_int32    i, j, isub;
l_int32    npix;  /* number of remaining pixels to be assigned */
l_int32    ncolor; /* number of remaining color cells to be used */
l_int32    ppc;  /* ave number of pixels left for each color cell */
l_int32    rv, gv, bv;
l_float32  thresholdFactor[] = {0.01f, 0.01f, 1.0f, 1.0f, 1.0f, 1.0f};
l_float32  thresh;  /* factor of ppc for this level */
l_uint32  *datas, *lines;
l_uint32  *rtab, *gtab, *btab;
CQCELL  ***cqcaa;   /* one array for each octree level */
CQCELL   **cqca, **cqcasub;
CQCELL    *cqc, *cqcsub;
PIXCMAP   *cmap;
NUMA      *nat;  /* accumulates levels for threshold cells */
NUMA      *nar;  /* accumulates levels for residual cells */

    if (!pixs)
        return (CQCELL ***)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (CQCELL ***)ERROR_PTR("pixs must be 32 bpp", __func__, NULL);
    if (colors < 128 || colors > 256)
        return (CQCELL ***)ERROR_PTR("colors not in [128,256]", __func__, NULL);
    if (!pcmap)
        return (CQCELL ***)ERROR_PTR("&cmap not defined", __func__, NULL);

    if ((cqcaa = cqcellTreeCreate()) == NULL)
        return (CQCELL ***)ERROR_PTR("cqcaa not made", __func__, NULL);

        /* Make the canonical index tables */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(CqNLevels, &rtab, &gtab, &btab);

        /* Generate an 8 bpp cmap (max size 256) */
    cmap = pixcmapCreate(8);
    *pcmap = cmap;

    pixGetDimensions(pixs, &w, &h, NULL);
    npix = w * h;  /* initialize to all pixels */
    ncolor = colors - reservedcolors - ExtraReservedColors;
    ppc = npix / ncolor;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

        /* Accumulate the centers of each cluster at level CqNLevels */
    ncells = 1 << (3 * CqNLevels);
    cqca = cqcaa[CqNLevels];
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            cqc = cqca[octindex];
            cqc->n++;
        }
    }

        /* Arrays for storing statistics */
    nat = numaCreate(0);
    nar = numaCreate(0);

        /* Prune back from the lowest level and generate the colormap */
    for (level = CqNLevels - 1; level >= 2; level--) {
        thresh = thresholdFactor[level];
        cqca = cqcaa[level];
        cqcasub = cqcaa[level + 1];
        ncells = 1 << (3 * level);
        for (i = 0; i < ncells; i++) {  /* i is octindex at level */
            cqc = cqca[i];
            for (j = 0; j < 8; j++) {  /* check all subnodes */
                isub = 8 * i + j;   /* isub is octindex at level+1 */
                cqcsub = cqcasub[isub];
                if (cqcsub->bleaf == 1) {  /* already a leaf? */
                    cqc->nleaves++;   /* count the subcube leaves */
                    continue;
                }
                if (cqcsub->n >= thresh * ppc) {  /* make it a true leaf? */
                    cqcsub->bleaf = 1;
                    if (cmap->n < 256) {
                        cqcsub->index = cmap->n;  /* assign the color index */
                        getRGBFromOctcube(isub, level + 1, &rv, &gv, &bv);
                        pixcmapAddColor(cmap, rv, gv, bv);
#if 1   /* save values */
                        cqcsub->rc = rv;
                        cqcsub->gc = gv;
                        cqcsub->bc = bv;
#endif
                    } else {
                            /* This doesn't seem to happen. Do something. */
                        L_ERROR("assigning pixels to wrong color\n", __func__);
                        pixcmapGetNearestIndex(cmap, 128, 128, 128, &cindex);
                        cqcsub->index = cindex;  /* assign to the nearest */
                        pixcmapGetColor(cmap, cindex, &rval, &gval, &bval);
                        cqcsub->rc = rval;
                        cqcsub->gc = gval;
                        cqcsub->bc = bval;
                    }
                    cqc->nleaves++;
                    npix -= cqcsub->n;
                    ncolor--;
                    if (ncolor > 0)
                        ppc = npix / ncolor;
                    else if (ncolor + reservedcolors > 0)
                        ppc = npix / (ncolor + reservedcolors);
                    else
                        ppc = 1000000;  /* make it big */
                    numaAddNumber(nat, level + 1);

#if  DEBUG_OCTCUBE_CMAP
    lept_stderr("Exceeds threshold: colors used = %d, colors remaining = %d\n",
                cmap->n, ncolor + reservedcolors);
    lept_stderr("  cell with %d pixels, npix = %d, ppc = %d\n",
                cqcsub->n, npix, ppc);
    lept_stderr("  index = %d, level = %d, subindex = %d\n",
                i, level, j);
    lept_stderr("  rv = %d, gv = %d, bv = %d\n", rv, gv, bv);
#endif  /* DEBUG_OCTCUBE_CMAP */

                }
            }
            if (cqc->nleaves > 0 || level == 2) { /* make the cube a leaf now */
                cqc->bleaf = 1;
                if (cqc->nleaves < 8) {  /* residual CTE cube: acquire the
                                          * remaining pixels */
                    for (j = 0; j < 8; j++) {  /* check all subnodes */
                        isub = 8 * i + j;
                        cqcsub = cqcasub[isub];
                        if (cqcsub->bleaf == 0)  /* absorb */
                            cqc->n += cqcsub->n;
                    }
                    if (cmap->n < 256) {
                        cqc->index = cmap->n;  /* assign the color index */
                        getRGBFromOctcube(i, level, &rv, &gv, &bv);
                        pixcmapAddColor(cmap, rv, gv, bv);
#if 1   /* save values */
                        cqc->rc = rv;
                        cqc->gc = gv;
                        cqc->bc = bv;
#endif
                    } else {
                        L_WARNING("possibly assigned pixels to wrong color\n",
                                  __func__);
                            /* This is very bad.  It will only cause trouble
                             * with dithering, and we try to avoid it with
                             * ExtraReservedColors. */
                        pixcmapGetNearestIndex(cmap, rv, gv, bv, &cindex);
                        cqc->index = cindex;  /* assign to the nearest */
                        pixcmapGetColor(cmap, cindex, &rval, &gval, &bval);
                        cqc->rc = rval;
                        cqc->gc = gval;
                        cqc->bc = bval;
                    }
                    npix -= cqc->n;
                    ncolor--;
                    if (ncolor > 0)
                        ppc = npix / ncolor;
                    else if (ncolor + reservedcolors > 0)
                        ppc = npix / (ncolor + reservedcolors);
                    else
                        ppc = 1000000;  /* make it big */
                    numaAddNumber(nar, level);

#if  DEBUG_OCTCUBE_CMAP
    lept_stderr("By remainder: colors used = %d, colors remaining = %d\n",
                cmap->n, ncolor + reservedcolors);
    lept_stderr("  cell with %d pixels, npix = %d, ppc = %d\n",
                cqc->n, npix, ppc);
    lept_stderr("  index = %d, level = %d\n", i, level);
    lept_stderr("  rv = %d, gv = %d, bv = %d\n", rv, gv, bv);
#endif  /* DEBUG_OCTCUBE_CMAP */

                }
            } else {  /* absorb all the subpixels but don't make it a leaf */
                for (j = 0; j < 8; j++) {  /* absorb from all subnodes */
                    isub = 8 * i + j;
                    cqcsub = cqcasub[isub];
                    cqc->n += cqcsub->n;
                }
            }
        }
    }

#if  PRINT_OCTCUBE_STATS
{
l_int32    tc[] = {0, 0, 0, 0, 0, 0, 0};
l_int32    rc[] = {0, 0, 0, 0, 0, 0, 0};
l_int32    nt, nr, ival;

    nt = numaGetCount(nat);
    nr = numaGetCount(nar);
    for (i = 0; i < nt; i++) {
        numaGetIValue(nat, i, &ival);
        tc[ival]++;
    }
    for (i = 0; i < nr; i++) {
        numaGetIValue(nar, i, &ival);
        rc[ival]++;
    }
    lept_stderr(" Threshold cells formed: %d\n", nt);
    for (i = 1; i < CqNLevels + 1; i++)
        lept_stderr("   level %d:  %d\n", i, tc[i]);
    lept_stderr("\n Residual cells formed: %d\n", nr);
    for (i = 0; i < CqNLevels ; i++)
        lept_stderr("   level %d:  %d\n", i, rc[i]);
}
#endif  /* PRINT_OCTCUBE_STATS */

    numaDestroy(&nat);
    numaDestroy(&nar);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);

    return cqcaa;
}


/*!
 * \brief   pixOctreeQuantizePixels()
 *
 * \param[in]    pixs         32 bpp
 * \param[in]    cqcaa        octree in array format
 * \param[in]    ditherflag   1 for dithering, 0 for no dithering
 * \return  pixd or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This routine doesn't need to use the CTEs (colormap
 *          table entries) because the color indices are embedded
 *          in the octree.  Thus, the calling program must make
 *          and attach the colormap to pixd after it is returned.
 *      (2) Dithering is performed in integers, effectively rounding
 *          to 1/8 sample increment.  The data in the integer buffers is
 *          64 times the sample values.  The 'dif' is 8 times the
 *          sample values, and this spread, multiplied by 8, to the
 *          integer buffers.  Because the dif is truncated to an
 *          integer, the dither is accurate to 1/8 of a sample increment,
 *          or 1/2048 of the color range.
 * </pre>
 */
static PIX *
pixOctreeQuantizePixels(PIX       *pixs,
                        CQCELL  ***cqcaa,
                        l_int32    ditherflag)
{
l_uint8   *bufu8r, *bufu8g, *bufu8b;
l_int32    rval, gval, bval;
l_int32    octindex, index;
l_int32    val1, val2, val3, dif;
l_int32    w, h, wpls, wpld, i, j, success;
l_int32    rc, gc, bc;
l_int32   *buf1r, *buf1g, *buf1b, *buf2r, *buf2g, *buf2b;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", __func__, NULL);
    if (!cqcaa)
        return (PIX *)ERROR_PTR("cqcaa not defined", __func__, NULL);

        /* Make output 8 bpp palette image */
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Make the canonical index tables */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(CqNLevels, &rtab, &gtab, &btab);

        /* Traverse tree from root, looking for lowest cube
         * that is a leaf, and set dest pix to its
         * colortable index value.  The results are far
         * better when dithering to get a more accurate
         * average color.  */
    if (ditherflag == 0) {    /* no dithering */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                extractRGBValues(lines[j], &rval, &gval, &bval);
                octindex = rtab[rval] | gtab[gval] | btab[bval];
                octreeFindColorCell(octindex, cqcaa, &index, &rc, &gc, &bc);
                SET_DATA_BYTE(lined, j, index);
            }
        }
    } else {  /* Dither */
        success = TRUE;
        bufu8r = bufu8g = bufu8b = NULL;
        buf1r = buf1g = buf1b = buf2r = buf2g = buf2b = NULL;
        bufu8r = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        bufu8g = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        bufu8b = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
        buf1r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf1g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf1b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        buf2b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
        if (!bufu8r || !bufu8g || !bufu8b || !buf1r || !buf1g ||
            !buf1b || !buf2r || !buf2g || !buf2b) {
            L_ERROR("buffer not made\n", __func__);
            success = FALSE;
            goto buffer_cleanup;
        }

            /* Start by priming buf2; line 1 is above line 2 */
        pixGetRGBLine(pixs, 0, bufu8r, bufu8g, bufu8b);
        for (j = 0; j < w; j++) {
            buf2r[j] = 64 * bufu8r[j];
            buf2g[j] = 64 * bufu8g[j];
            buf2b[j] = 64 * bufu8b[j];
        }

        for (i = 0; i < h - 1; i++) {
                /* Swap data 2 --> 1, and read in new line 2 */
            memcpy(buf1r, buf2r, 4 * w);
            memcpy(buf1g, buf2g, 4 * w);
            memcpy(buf1b, buf2b, 4 * w);
            pixGetRGBLine(pixs, i + 1, bufu8r, bufu8g, bufu8b);
            for (j = 0; j < w; j++) {
                buf2r[j] = 64 * bufu8r[j];
                buf2g[j] = 64 * bufu8g[j];
                buf2b[j] = 64 * bufu8b[j];
            }

                /* Dither */
            lined = datad + i * wpld;
            for (j = 0; j < w - 1; j++) {
                rval = buf1r[j] / 64;
                gval = buf1g[j] / 64;
                bval = buf1b[j] / 64;
                octindex = rtab[rval] | gtab[gval] | btab[bval];
                octreeFindColorCell(octindex, cqcaa, &index, &rc, &gc, &bc);
                SET_DATA_BYTE(lined, j, index);

                dif = buf1r[j] / 8 - 8 * rc;
                if (dif != 0) {
                    val1 = buf1r[j + 1] + 3 * dif;
                    val2 = buf2r[j] + 3 * dif;
                    val3 = buf2r[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1r[j + 1] = L_MIN(16383, val1);
                        buf2r[j] = L_MIN(16383, val2);
                        buf2r[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1r[j + 1] = L_MAX(0, val1);
                        buf2r[j] = L_MAX(0, val2);
                        buf2r[j + 1] = L_MAX(0, val3);
                    }
                }

                dif = buf1g[j] / 8 - 8 * gc;
                if (dif != 0) {
                    val1 = buf1g[j + 1] + 3 * dif;
                    val2 = buf2g[j] + 3 * dif;
                    val3 = buf2g[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1g[j + 1] = L_MIN(16383, val1);
                        buf2g[j] = L_MIN(16383, val2);
                        buf2g[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1g[j + 1] = L_MAX(0, val1);
                        buf2g[j] = L_MAX(0, val2);
                        buf2g[j + 1] = L_MAX(0, val3);
                    }
                }

                dif = buf1b[j] / 8 - 8 * bc;
                if (dif != 0) {
                    val1 = buf1b[j + 1] + 3 * dif;
                    val2 = buf2b[j] + 3 * dif;
                    val3 = buf2b[j + 1] + 2 * dif;
                    if (dif > 0) {
                        buf1b[j + 1] = L_MIN(16383, val1);
                        buf2b[j] = L_MIN(16383, val2);
                        buf2b[j + 1] = L_MIN(16383, val3);
                    } else {
                        buf1b[j + 1] = L_MAX(0, val1);
                        buf2b[j] = L_MAX(0, val2);
                        buf2b[j + 1] = L_MAX(0, val3);
                    }
                }
            }

                /* Get last pixel in row; no downward propagation */
            rval = buf1r[w - 1] / 64;
            gval = buf1g[w - 1] / 64;
            bval = buf1b[w - 1] / 64;
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            octreeFindColorCell(octindex, cqcaa, &index, &rc, &gc, &bc);
            SET_DATA_BYTE(lined, w - 1, index);
        }

            /* Get last row of pixels; no leftward propagation */
        lined = datad + (h - 1) * wpld;
        for (j = 0; j < w; j++) {
            rval = buf2r[j] / 64;
            gval = buf2g[j] / 64;
            bval = buf2b[j] / 64;
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            octreeFindColorCell(octindex, cqcaa, &index, &rc, &gc, &bc);
            SET_DATA_BYTE(lined, j, index);
        }

buffer_cleanup:
        LEPT_FREE(bufu8r);
        LEPT_FREE(bufu8g);
        LEPT_FREE(bufu8b);
        LEPT_FREE(buf1r);
        LEPT_FREE(buf1g);
        LEPT_FREE(buf1b);
        LEPT_FREE(buf2r);
        LEPT_FREE(buf2g);
        LEPT_FREE(buf2b);
        if (!success) pixDestroy(&pixd);
    }

    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return pixd;
}


/*!
 * \brief   octreeFindColorCell()
 *
 * \param[in]    octindex
 * \param[in]    cqcaa
 * \param[out]   pindex   index of CTE; returned to set pixel value
 * \param[out]   prval    of CTE
 * \param[out]   pgval    of CTE
 * \param[out]   pbval    of CTE
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) As this is in inner loop, we don't check input pointers!
 *      (2) This traverses from the root (well, actually from level 2,
 *          because the level 2 cubes are the largest CTE cubes),
 *          and finds the index number of the cell and the color values,
 *          which can be used either directly or in a (Floyd-Steinberg)
 *          error-diffusion dithering algorithm.
 * </pre>
 */
static l_int32
octreeFindColorCell(l_int32    octindex,
                    CQCELL  ***cqcaa,
                    l_int32   *pindex,
                    l_int32   *prval,
                    l_int32   *pgval,
                    l_int32   *pbval)
{
l_int32  level;
l_int32  baseindex, subindex;
CQCELL  *cqc, *cqcsub;

        /* Use rgb values stored in the cubes; a little faster */
    for (level = 2; level < CqNLevels; level++) {
        getOctcubeIndices(octindex, level, &baseindex, &subindex);
        cqc = cqcaa[level][baseindex];
        cqcsub = cqcaa[level + 1][subindex];
        if (cqcsub->bleaf == 0) {  /* use cell at level above */
            *pindex = cqc->index;
            *prval = cqc->rc;
            *pgval = cqc->gc;
            *pbval = cqc->bc;
            break;
        } else if (level == CqNLevels - 1) {  /* reached the bottom */
            *pindex = cqcsub->index;
            *prval = cqcsub->rc;
            *pgval = cqcsub->gc;
            *pbval = cqcsub->bc;
             break;
        }
    }

#if 0
        /* Generate rgb values for each cube on the fly; slower */
    for (level = 2; level < CqNLevels; level++) {
        l_int32  rv, gv, bv;
        getOctcubeIndices(octindex, level, &baseindex, &subindex);
        cqc = cqcaa[level][baseindex];
        cqcsub = cqcaa[level + 1][subindex];
        if (cqcsub->bleaf == 0) {  /* use cell at level above */
            getRGBFromOctcube(baseindex, level, &rv, &gv, &bv);
            *pindex = cqc->index;
            *prval = rv;
            *pgval = gv;
            *pbval = bv;
            break;
        } else if (level == CqNLevels - 1) {  /* reached the bottom */
            getRGBFromOctcube(subindex, level + 1, &rv, &gv, &bv);
           *pindex = cqcsub->index;
            *prval = rv;
            *pgval = gv;
            *pbval = bv;
            break;
        }
    }
#endif

    return 0;
}



/*------------------------------------------------------------------*
 *                      Helper cqcell functions                     *
 *------------------------------------------------------------------*/
/*!
 * \brief   cqcellTreeCreate()
 *
 * \return  cqcell array tree
 */
static CQCELL ***
cqcellTreeCreate(void)
{
l_int32    level, ncells, i;
CQCELL  ***cqcaa;
CQCELL   **cqca;   /* one array for each octree level */

        /* Make array of accumulation cell arrays from levels 1 to 5 */
    cqcaa = (CQCELL ***)LEPT_CALLOC(CqNLevels + 1, sizeof(CQCELL **));
    for (level = 0; level <= CqNLevels; level++) {
        ncells = 1 << (3 * level);
        cqca = (CQCELL **)LEPT_CALLOC(ncells, sizeof(CQCELL *));
        cqcaa[level] = cqca;
        for (i = 0; i < ncells; i++) {
            cqca[i] = (CQCELL *)LEPT_CALLOC(1, sizeof(CQCELL));
        }
    }

    return cqcaa;
}


/*!
 * \brief   cqcellTreeDestroy()
 *
 * \param[in,out]   pcqcaa   will be set to null before returning
 */
static void
cqcellTreeDestroy(CQCELL  ****pcqcaa)
{
l_int32    level, ncells, i;
CQCELL  ***cqcaa;
CQCELL   **cqca;

    if (pcqcaa == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }

    if ((cqcaa = *pcqcaa) == NULL)
        return;

    for (level = 0; level <= CqNLevels; level++) {
        cqca = cqcaa[level];
        ncells = 1 << (3 * level);
        for (i = 0; i < ncells; i++)
            LEPT_FREE(cqca[i]);
        LEPT_FREE(cqca);
    }
    LEPT_FREE(cqcaa);
    *pcqcaa = NULL;

    return;
}



/*------------------------------------------------------------------*
 *                       Helper index functions                     *
 *------------------------------------------------------------------*/
/*!
 * \brief   makeRGBToIndexTables()
 *
 * \param[in]    cqlevels               can be 1, 2, 3, 4, 5 or 6
 * \param[out]   prtab, pgtab, pbtab    tables
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Set up tables.  e.g., for cqlevels = 5, we need an integer 0 < i < 2^15:
 *      rtab = 0  i7  0   0  i6  0   0  i5  0   0   i4  0   0   i3  0   0
 *      gtab = 0  0   i7  0   0  i6  0   0  i5  0   0   i4  0   0   i3  0
 *      btab = 0  0   0   i7  0  0   i6  0  0   i5  0   0   i4  0   0   i3
 *
 *  The tables are then used to map from rbg --> index as follows:
 *      index = 0  r7  g7  b7  r6  g6  b6  r5  g5  b5  r4  g4  b4  r3  g3  b3
 *
 *    e.g., for cqlevels = 4, we map to
 *      index = 0  0   0   0   r7  g7  b7  r6  g6  b6  r5  g5  b5  r4  g4  b4
 *
 *  This may look a bit strange.  The notation 'r7' means the MSBit of
 *  the r value which has 8 bits, going down from r7 to r0.
 *  Keep in mind that r7 is actually the r component bit for level 1 of
 *  the octtree.  Level 1 is composed of 8 octcubes, represented by
 *  the bits r7 g7 b7, which divide the entire color space into
 *  8 cubes.  At level 2, each of these 8 octcubes is further divided into
 *  8 cubes, each labeled by the second most significant bits r6 g6 b6
 *  of the rgb color.
 * </pre>
 */
l_ok
makeRGBToIndexTables(l_int32     cqlevels,
                     l_uint32  **prtab,
                     l_uint32  **pgtab,
                     l_uint32  **pbtab)
{
l_int32    i;
l_uint32  *rtab, *gtab, *btab;

    if (cqlevels < 1 || cqlevels > 6)
        return ERROR_INT("cqlevels must be in {1,...6}", __func__, 1);
    if (!prtab || !pgtab || !pbtab)
        return ERROR_INT("not all &tabs defined", __func__, 1);

    rtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    gtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    btab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    if (!rtab || !gtab || !btab)
        return ERROR_INT("calloc fail for tab", __func__, 1);
    *prtab = rtab;
    *pgtab = gtab;
    *pbtab = btab;

    switch (cqlevels)
    {
    case 1:
        for (i = 0; i < 256; i++) {
            rtab[i] = (i >> 5) & 0x0004;
            gtab[i] = (i >> 6) & 0x0002;
            btab[i] = (i >> 7);
        }
        break;
    case 2:
        for (i = 0; i < 256; i++) {
            rtab[i] = ((i >> 2) & 0x0020) | ((i >> 4) & 0x0004);
            gtab[i] = ((i >> 3) & 0x0010) | ((i >> 5) & 0x0002);
            btab[i] = ((i >> 4) & 0x0008) | ((i >> 6) & 0x0001);
        }
        break;
    case 3:
        for (i = 0; i < 256; i++) {
            rtab[i] = ((i << 1) & 0x0100) | ((i >> 1) & 0x0020) |
                      ((i >> 3) & 0x0004);
            gtab[i] = (i & 0x0080) | ((i >> 2) & 0x0010) |
                      ((i >> 4) & 0x0002);
            btab[i] = ((i >> 1) & 0x0040) | ((i >> 3) & 0x0008) |
                      ((i >> 5) & 0x0001);
        }
        break;
    case 4:
        for (i = 0; i < 256; i++) {
            rtab[i] = ((i << 4) & 0x0800) | ((i << 2) & 0x0100) |
                      (i & 0x0020) | ((i >> 2) & 0x0004);
            gtab[i] = ((i << 3) & 0x0400) | ((i << 1) & 0x0080) |
                      ((i >> 1) & 0x0010) | ((i >> 3) & 0x0002);
            btab[i] = ((i << 2) & 0x0200) | (i & 0x0040) |
                      ((i >> 2) & 0x0008) | ((i >> 4) & 0x0001);
        }
        break;
    case 5:
        for (i = 0; i < 256; i++) {
            rtab[i] = ((i << 7) & 0x4000) | ((i << 5) & 0x0800) |
                      ((i << 3) & 0x0100) | ((i << 1) & 0x0020) |
                      ((i >> 1) & 0x0004);
            gtab[i] = ((i << 6) & 0x2000) | ((i << 4) & 0x0400) |
                      ((i << 2) & 0x0080) | (i & 0x0010) |
                      ((i >> 2) & 0x0002);
            btab[i] = ((i << 5) & 0x1000) | ((i << 3) & 0x0200) |
                      ((i << 1) & 0x0040) | ((i >> 1) & 0x0008) |
                      ((i >> 3) & 0x0001);
        }
        break;
    case 6:
        for (i = 0; i < 256; i++) {
            rtab[i] = ((i << 10) & 0x20000) | ((i << 8) & 0x4000) |
                      ((i << 6) & 0x0800) | ((i << 4) & 0x0100) |
                      ((i << 2) & 0x0020) | (i & 0x0004);
            gtab[i] = ((i << 9) & 0x10000) | ((i << 7) & 0x2000) |
                      ((i << 5) & 0x0400) | ((i << 3) & 0x0080) |
                      ((i << 1) & 0x0010) | ((i >> 1) & 0x0002);
            btab[i] = ((i << 8) & 0x8000) | ((i << 6) & 0x1000) |
                      ((i << 4) & 0x0200) | ((i << 2) & 0x0040) |
                      (i & 0x0008) | ((i >> 2) & 0x0001);
        }
        break;
    default:
        ERROR_INT("cqlevels not in [1...6]", __func__, 1);
        break;
    }

    return 0;
}


/*!
 * \brief   getOctcubeIndexFromRGB()
 *
 * \param[in]    rval, gval, bval
 * \param[in]    rtab, gtab, btab    generated with makeRGBToIndexTables()
 * \param[out]   pindex found index
 * \return  void
 *
 * <pre>
 * Notes:
 *      No error checking!
 * </pre>
 */
void
getOctcubeIndexFromRGB(l_int32    rval,
                       l_int32    gval,
                       l_int32    bval,
                       l_uint32  *rtab,
                       l_uint32  *gtab,
                       l_uint32  *btab,
                       l_uint32  *pindex)
{
    *pindex = rtab[rval] | gtab[gval] | btab[bval];
    return;
}


/*!
 * \brief   getRGBFromOctcube()
 *
 * \param[in]    cubeindex octcube index
 * \param[in]    level at which index is expressed
 * \param[out]   prval  r val of this cube
 * \param[out]   pgval  g val of this cube
 * \param[out]   pbval  b val of this cube
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) We can consider all octcube indices to represent a
 *          specific point in color space: namely, the location
 *          of the 'upper-left' corner of the cube, where indices
 *          increase down and to the right.  The upper left corner
 *          of the color space is then 00000....
 *      (2) The 'rgbindex' is a 24-bit representation of the location,
 *          in octcube notation, at the center of the octcube.
 *          To get to the center of an octcube, you choose the 111
 *          octcube at the next lower level.
 *      (3) For example, if the octcube index = 110101 (binary),
 *          which is a level 2 expression, then the rgbindex
 *          is the 24-bit representation of 110101111 (at level 3);
 *          namely, 000110101111000000000000.  The number is padded
 *          with 3 leading 0s (because the representation uses
 *          only 21 bits) and 12 trailing 0s (the default for
 *          levels 4-7, which are contained within each of the level3
 *          octcubes.  Then the rgb values for the center of the
 *          octcube are: rval = 11100000, gval = 10100000, bval = 01100000
 * </pre>
 */
static void
getRGBFromOctcube(l_int32   cubeindex,
                  l_int32   level,
                  l_int32  *prval,
                  l_int32  *pgval,
                  l_int32  *pbval)
{
l_int32  rgbindex;

        /* Bring to format in 21 bits: (r7 g7 b7 r6 g6 b6 ...) */
        /* This is valid for levels from 0 to 6 */
    rgbindex = cubeindex << (3 * (7 - level));  /* upper corner of cube */
    rgbindex |= (0x7 << (3 * (6 - level)));   /* index to center of cube */

        /* Extract separate pieces */
    *prval = ((rgbindex >> 13) & 0x80) |
             ((rgbindex >> 11) & 0x40) |
             ((rgbindex >> 9) & 0x20) |
             ((rgbindex >> 7) & 0x10) |
             ((rgbindex >> 5) & 0x08) |
             ((rgbindex >> 3) & 0x04) |
             ((rgbindex >> 1) & 0x02);
    *pgval = ((rgbindex >> 12) & 0x80) |
             ((rgbindex >> 10) & 0x40) |
             ((rgbindex >> 8) & 0x20) |
             ((rgbindex >> 6) & 0x10) |
             ((rgbindex >> 4) & 0x08) |
             ((rgbindex >> 2) & 0x04) |
             (rgbindex & 0x02);
    *pbval = ((rgbindex >> 11) & 0x80) |
             ((rgbindex >> 9) & 0x40) |
             ((rgbindex >> 7) & 0x20) |
             ((rgbindex >> 5) & 0x10) |
             ((rgbindex >> 3) & 0x08) |
             ((rgbindex >> 1) & 0x04) |
             ((rgbindex << 1) & 0x02);

    return;
}


/*!
 * \brief   getOctcubeIndices()
 *
 * \param[in]    rgbindex
 * \param[in]    level     octree level 0, 1, 2, 3, 4, 5
 * \param[out]   pbindex   base index index at the octree level
 * \param[out]   psindex   sub index index at the next lower level
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *  for CqNLevels = 6, the full RGB index is in the form:
 *     index = (0[13] 0 r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4 r3 g3 b3 r2 g2 b2)
 *  for CqNLevels = 5, the full RGB index is in the form:
 *     index = (0[16] 0 r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4 r3 g3 b3)
 *  for CqNLevels = 4, the full RGB index is in the form:
 *     index = (0[19] 0 r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4)
 *
 *  The base index is the index of the octcube at the level given,
 *  whereas the sub index is the index at the next level down.
 *
 *  For level 0: base index = 0
 *               sub index is the 3 bit number (r7 g7 b7)
 *  For level 1: base index = (r7 g7 b7)
 *               sub index = (r7 g7 b7 r6 g6 b6)
 *  For level 2: base index = (r7 g7 b7 r6 g6 b6)
 *               sub index = (r7 g7 b7 r6 g6 b6 r5 g5 b5)
 *  For level 3: base index = (r7 g7 b7 r6 g6 b6 r5 g5 b5)
 *               sub index = (r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4)
 *  For level 4: base index = (r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4)
 *               sub index = (r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4 r3 g3 b3)
 *  For level 5: base index = (r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4 r3 g3 b3)
 *               sub index = (r7 g7 b7 r6 g6 b6 r5 g5 b5 r4 g4 b4 r3 g3 b3
 *                            r2 g2 b2)
 * </pre>
 */
static l_int32
getOctcubeIndices(l_int32   rgbindex,
                  l_int32   level,
                  l_int32  *pbindex,
                  l_int32  *psindex)
{
    if (level < 0 || level > CqNLevels - 1)
        return ERROR_INT("level must be in e.g., [0 ... 5]", __func__, 1);
    if (!pbindex)
        return ERROR_INT("&bindex not defined", __func__, 1);
    if (!psindex)
        return ERROR_INT("&sindex not defined", __func__, 1);

    *pbindex = rgbindex >> (3 * (CqNLevels - level));
    *psindex = rgbindex >> (3 * (CqNLevels - 1 - level));
    return 0;
}


/*!
 * \brief   octcubeGetCount()
 *
 * \param[in]    level   valid values are in [1,...6]; there are 2^level
 *                       cubes along each side of the rgb cube
 * \param[out]   psize   2^(3 * level) cubes in the entire rgb cube
 * \return   0 if OK, 1 on error.  Caller must check!
 *
 * <pre>
 *     level:   1        2        3        4        5        6
 *     size:    8       64       512     4098     32784   262272
 * </pre>
 */
static l_int32
octcubeGetCount(l_int32   level,
                l_int32  *psize)
{
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (level < 1 || level > 6)
        return ERROR_INT("invalid level", __func__, 1);

    *psize = 1 << (3 * level);
    return 0;
}


/*---------------------------------------------------------------------------*
 *      Adaptive octree quantization based on population at a fixed level    *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixOctreeQuantByPopulation()
 *
 * \param[in]    pixs         32 bpp rgb
 * \param[in]    level        significant bits for each of RGB; valid for {3,4}.
 *                            Use 0 for default (level 4; recommended
 * \param[in]    ditherflag   1 to dither, 0 otherwise
 * \return  pixd quantized to octcubes or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This color quantization method works very well without
 *          dithering, using octcubes at two different levels:
 *            (a) the input %level, which is either 3 or 4
 *            (b) level 2 (64 octcubes to cover the entire color space)
 *      (2) For best results, using %level = 4 is recommended.
 *          Why do we provide an option for using level 3?  Because
 *          there are 512 octcubes at level 3, and for many images
 *          not more than 256 are filled.  As a result, on some images
 *          a very accurate quantized representation is possible using
 *          %level = 3.
 *      (3) This first breaks up the color space into octcubes at the
 *          input %level, and computes, for each octcube, the average
 *          value of the pixels that are in it.
 *      (4) Then there are two possible situations:
 *            (a) If there are not more than 256 populated octcubes,
 *                it returns a cmapped pix with those values assigned.
 *            (b) Otherwise, it selects 192 octcubes containing the largest
 *                number of pixels and quantizes pixels within those octcubes
 *                to their average.  Then, to handle the residual pixels
 *                that are not in those 192 octcubes, it generates a
 *                level 2 octree consisting of 64 octcubes, and within
 *                each octcube it quantizes the residual pixels to their
 *                average within each of those level 2 octcubes.
 *      (5) Unpopulated level 2 octcubes are represented in the colormap
 *          by their centers.  This, of course, has no effect unless
 *          dithering is used for the output image.
 *      (6) The depth of pixd is the minimum required to support the
 *          number of colors found at %level; namely, 2, 4 or 8.
 *      (7) This function works particularly well on images such as maps,
 *          where there are a relatively small number of well-populated
 *          colors, but due to antialiasing and compression artifacts
 *          there may be a large number of different colors.  This will
 *          pull out and represent accurately the highly populated colors,
 *          while still making a reasonable approximation for the others.
 *      (8) The highest level of octcubes allowed is 4.  Use of higher
 *          levels typically results in having a small fraction of
 *          pixels in the most populated 192 octcubes.  As a result,
 *          most of the pixels are represented at level 2, which is
 *          not sufficiently accurate.
 *      (9) Dithering shows artifacts on some images.  If you plan to
 *          dither, pixOctreeColorQuant() and pixFixedOctcubeQuant256()
 *          usually give better results.
 * </pre>
 */
PIX *
pixOctreeQuantByPopulation(PIX     *pixs,
                           l_int32  level,
                           l_int32  ditherflag)
{
l_int32         w, h, wpls, wpld, i, j, depth, size, ncolors, index;
l_int32         rval, gval, bval;
l_int32        *rarray, *garray, *barray, *narray, *iarray;
l_uint32        octindex, octindex2;
l_uint32       *rtab, *gtab, *btab, *rtab2, *gtab2, *btab2;
l_uint32       *lines, *lined, *datas, *datad;
L_OCTCUBE_POP  *opop;
L_HEAP         *lh;
PIX            *pixd;
PIXCMAP        *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (level == 0) level = 4;
    if (level < 3 || level > 4)
        return (PIX *)ERROR_PTR("level not in {3,4}", __func__, NULL);

        /* Do not dither if image is very small */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinDitherSize && h < MinDitherSize && ditherflag == 1) {
        L_INFO("Small image: dithering turned off\n", __func__);
        ditherflag = 0;
    }

    if (octcubeGetCount(level, &size))  /* array size = 2 ** (3 * level) */
        return (PIX *)ERROR_PTR("size not returned", __func__, NULL);
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);

    pixd = NULL;
    narray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    rarray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    garray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    barray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    if (!narray || !rarray || !garray || !barray)
        goto array_cleanup;

        /* Place the pixels in octcube leaves. */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            narray[octindex]++;
            rarray[octindex] += rval;
            garray[octindex] += gval;
            barray[octindex] += bval;
        }
    }

        /* Find the number of different colors */
    for (i = 0, ncolors = 0; i < size; i++) {
        if (narray[i] > 0)
            ncolors++;
    }
    if (ncolors <= 4)
        depth = 2;
    else if (ncolors <= 16)
        depth = 4;
    else
        depth = 8;
    pixd = pixCreate(w, h, depth);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    cmap = pixcmapCreate(depth);
    pixSetColormap(pixd, cmap);

        /* Average the colors in each octcube leaf. */
    for (i = 0; i < size; i++) {
        if (narray[i] > 0) {
            rarray[i] /= narray[i];
            garray[i] /= narray[i];
            barray[i] /= narray[i];
        }
    }

        /* If ncolors <= 256, finish immediately.  Do not dither.
         * Re-use narray to hold the colormap index + 1  */
    if (ncolors <= 256) {
        for (i = 0, index = 0; i < size; i++) {
            if (narray[i] > 0) {
                pixcmapAddColor(cmap, rarray[i], garray[i], barray[i]);
                narray[i] = index + 1;  /* to avoid storing 0 */
                index++;
            }
        }

            /* Set the cmap indices for each pixel */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                extractRGBValues(lines[j], &rval, &gval, &bval);
                octindex = rtab[rval] | gtab[gval] | btab[bval];
                switch (depth)
                {
                case 8:
                    SET_DATA_BYTE(lined, j, narray[octindex] - 1);
                    break;
                case 4:
                    SET_DATA_QBIT(lined, j, narray[octindex] - 1);
                    break;
                case 2:
                    SET_DATA_DIBIT(lined, j, narray[octindex] - 1);
                    break;
                default:
                    L_WARNING("shouldn't get here\n", __func__);
                }
            }
        }
        goto array_cleanup;
    }

        /* More complicated.  Sort by decreasing population */
    lh = lheapCreate(500, L_SORT_DECREASING);
    for (i = 0; i < size; i++) {
        if (narray[i] > 0) {
            opop = (L_OCTCUBE_POP *)LEPT_CALLOC(1, sizeof(L_OCTCUBE_POP));
            opop->npix = (l_float32)narray[i];
            opop->index = i;
            opop->rval = rarray[i];
            opop->gval = garray[i];
            opop->bval = barray[i];
            lheapAdd(lh, opop);
        }
    }

        /* Take the top 192.  These will form the first 192 colors
         * in the cmap.  iarray[i] holds the index into the cmap. */
    iarray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    for (i = 0; i < 192; i++) {
        opop = (L_OCTCUBE_POP*)lheapRemove(lh);
        if (!opop) break;
        pixcmapAddColor(cmap, opop->rval, opop->gval, opop->bval);
        iarray[opop->index] = i + 1;  /* +1 to avoid storing 0 */

#if DEBUG_POP
        lept_stderr("i = %d, n = %6.0f, (r,g,b) = (%d %d %d)\n",
                    i, opop->npix, opop->rval, opop->gval, opop->bval);
#endif  /* DEBUG_POP */

        LEPT_FREE(opop);
    }

        /* Make the octindex tables for level 2, and reuse rarray, etc. */
    rtab2 = gtab2 = btab2 = NULL;
    makeRGBToIndexTables(2, &rtab2, &gtab2, &btab2);
    for (i = 0; i < 64; i++) {
        narray[i] = 0;
        rarray[i] = 0;
        garray[i] = 0;
        barray[i] = 0;
    }

        /* Take the rest of the occupied octcubes, assigning the pixels
         * to these new colormap indices.  iarray[] is addressed
         * by %level octcube indices, and it now holds the
         * colormap indices for all pixels in pixs.  */
    for (i = 192; i < size; i++) {
        opop = (L_OCTCUBE_POP*)lheapRemove(lh);
        if (!opop) break;
        rval = opop->rval;
        gval = opop->gval;
        bval = opop->bval;
        octindex2 = rtab2[rval] | gtab2[gval] | btab2[bval];
        narray[octindex2] += (l_int32)opop->npix;
        rarray[octindex2] += (l_int32)opop->npix * rval;
        garray[octindex2] += (l_int32)opop->npix * gval;
        barray[octindex2] += (l_int32)opop->npix * bval;
        iarray[opop->index] = 192 + octindex2 + 1;  /* +1 to avoid storing 0 */
        LEPT_FREE(opop);
    }
    lheapDestroy(&lh, TRUE);

        /* To span the full color space, which is necessary for dithering,
         * set each iarray element whose value is still 0 at the input
         * level octcube leaves (because there were no pixels in those
         * octcubes) to the colormap index corresponding to its level 2
         * octcube. */
    if (ditherflag) {
        for (i = 0; i < size; i++) {
            if (iarray[i] == 0) {
                getRGBFromOctcube(i, level, &rval, &gval, &bval);
                octindex2 = rtab2[rval] | gtab2[gval] | btab2[bval];
                iarray[i] = 192 + octindex2 + 1;
            }
        }
    }
    LEPT_FREE(rtab2);
    LEPT_FREE(gtab2);
    LEPT_FREE(btab2);

        /* Average the colors from the residuals in each level 2 octcube,
         * and add these 64 values to the colormap. */
    for (i = 0; i < 64; i++) {
        if (narray[i] > 0) {
            rarray[i] /= narray[i];
            garray[i] /= narray[i];
            barray[i] /= narray[i];
        } else {  /* no pixels in this octcube; use center value */
            getRGBFromOctcube(i, 2, &rarray[i], &garray[i], &barray[i]);
        }
        pixcmapAddColor(cmap, rarray[i], garray[i], barray[i]);
    }

        /* Set the cmap indices for each pixel.  Subtract 1 from
         * the value in iarray[] because we added 1 earlier.  */
    if (ditherflag == 0) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                extractRGBValues(lines[j], &rval, &gval, &bval);
                octindex = rtab[rval] | gtab[gval] | btab[bval];
                SET_DATA_BYTE(lined, j, iarray[octindex] - 1);
            }
        }
    } else {  /* dither */
        pixDitherOctindexWithCmap(pixs, pixd, rtab, gtab, btab,
                                  iarray, POP_DIF_CAP);
    }

#if DEBUG_POP
    for (i = 0; i < size / 16; i++) {
        l_int32 j;
        for (j = 0; j < 16; j++)
            lept_stderr("%d ", iarray[16 * i + j]);
        lept_stderr("\n");
    }
#endif  /* DEBUG_POP */

    LEPT_FREE(iarray);

array_cleanup:
    LEPT_FREE(narray);
    LEPT_FREE(rarray);
    LEPT_FREE(garray);
    LEPT_FREE(barray);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);

    return pixd;
}


/*!
 * \brief   pixDitherOctindexWithCmap()
 *
 * \param[in]    pixs               32 bpp rgb
 * \param[in]    pixd               8 bpp cmapped
 * \param[in]    rtab, gtab, btab   tables from rval to octindex
 * \param[in]    indexmap           array mapping octindex to cmap index
 * \param[in]    difcap             max allowed dither transfer;
 *                                  use 0 for infinite cap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This performs dithering to generate the colormap indices
 *          in pixd.  The colormap has been calculated, along with
 *          four input LUTs that together give the inverse colormapping
 *          from RGB to colormap index.
 *      (2) For pixOctreeQuantByPopulation(), %indexmap maps from the
 *          standard octindex to colormap index (after subtracting 1).
 *          The basic pixel-level function, without dithering, is:
 *             extractRGBValues(lines[j], &rval, &gval, &bval);
 *             octindex = rtab[rval] | gtab[gval] | btab[bval];
 *             SET_DATA_BYTE(lined, j, indexmap[octindex] - 1);
 *      (3) This can be used in any situation where the general
 *          prescription for finding the colormap index from the rgb
 *          value is precisely this:
 *             cmapindex = indexmap[rtab[rval] | gtab[gval] | btab[bval]] - 1
 *          For example, in pixFixedOctcubeQuant256(), we don't use
 *          standard octcube indexing, the rtab (etc) LUTs map directly
 *          to the colormap index, and %indexmap just compensates for
 *          the 1-off indexing assumed to be in that table.
 * </pre>
 */
static l_int32
pixDitherOctindexWithCmap(PIX       *pixs,
                          PIX       *pixd,
                          l_uint32  *rtab,
                          l_uint32  *gtab,
                          l_uint32  *btab,
                          l_int32   *indexmap,
                          l_int32    difcap)
{
l_uint8   *bufu8r, *bufu8g, *bufu8b;
l_int32    i, j, w, h, wpld, octindex, cmapindex, success;
l_int32    rval, gval, bval, rc, gc, bc;
l_int32    dif, val1, val2, val3;
l_int32   *buf1r, *buf1g, *buf1b, *buf2r, *buf2g, *buf2b;
l_uint32  *datad, *lined;
PIXCMAP   *cmap;

    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs undefined or not 32 bpp", __func__, 1);
    if (!pixd || pixGetDepth(pixd) != 8)
        return ERROR_INT("pixd undefined or not 8 bpp", __func__, 1);
    if ((cmap = pixGetColormap(pixd)) == NULL)
        return ERROR_INT("pixd not cmapped", __func__, 1);
    if (!rtab || !gtab || !btab || !indexmap)
        return ERROR_INT("not all 4 tables defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (pixGetWidth(pixd) != w || pixGetHeight(pixd) != h)
        return ERROR_INT("pixs and pixd not same size", __func__, 1);

    success = TRUE;
    bufu8r = bufu8g = bufu8b = NULL;
    buf1r = buf1g = buf1b = buf2r = buf2g = buf2b = NULL;
    bufu8r = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
    bufu8g = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
    bufu8b = (l_uint8 *)LEPT_CALLOC(w, sizeof(l_uint8));
    buf1r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    buf1g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    buf1b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    buf2r = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    buf2g = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    buf2b = (l_int32 *)LEPT_CALLOC(w, sizeof(l_int32));
    if (!bufu8r || !bufu8g || !bufu8b || !buf1r || !buf1g ||
        !buf1b || !buf2r || !buf2g || !buf2b) {
        L_ERROR("buffer not made\n", __func__);
        success = FALSE;
        goto buffer_cleanup;
    }

        /* Start by priming buf2; line 1 is above line 2 */
    pixGetRGBLine(pixs, 0, bufu8r, bufu8g, bufu8b);
    for (j = 0; j < w; j++) {
        buf2r[j] = 64 * bufu8r[j];
        buf2g[j] = 64 * bufu8g[j];
        buf2b[j] = 64 * bufu8b[j];
    }

    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h - 1; i++) {
            /* Swap data 2 --> 1, and read in new line 2 */
        memcpy(buf1r, buf2r, 4 * w);
        memcpy(buf1g, buf2g, 4 * w);
        memcpy(buf1b, buf2b, 4 * w);
        pixGetRGBLine(pixs, i + 1, bufu8r, bufu8g, bufu8b);
        for (j = 0; j < w; j++) {
            buf2r[j] = 64 * bufu8r[j];
            buf2g[j] = 64 * bufu8g[j];
            buf2b[j] = 64 * bufu8b[j];
        }

            /* Dither */
        lined = datad + i * wpld;
        for (j = 0; j < w - 1; j++) {
            rval = buf1r[j] / 64;
            gval = buf1g[j] / 64;
            bval = buf1b[j] / 64;
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            cmapindex = indexmap[octindex] - 1;
            SET_DATA_BYTE(lined, j, cmapindex);
            pixcmapGetColor(cmap, cmapindex, &rc, &gc, &bc);

            dif = buf1r[j] / 8 - 8 * rc;
            if (difcap > 0) {
                if (dif > difcap) dif = difcap;
                if (dif < -difcap) dif = -difcap;
            }
            if (dif != 0) {
                val1 = buf1r[j + 1] + 3 * dif;
                val2 = buf2r[j] + 3 * dif;
                val3 = buf2r[j + 1] + 2 * dif;
                if (dif > 0) {
                    buf1r[j + 1] = L_MIN(16383, val1);
                    buf2r[j] = L_MIN(16383, val2);
                    buf2r[j + 1] = L_MIN(16383, val3);
                } else {
                    buf1r[j + 1] = L_MAX(0, val1);
                    buf2r[j] = L_MAX(0, val2);
                    buf2r[j + 1] = L_MAX(0, val3);
                }
            }

            dif = buf1g[j] / 8 - 8 * gc;
            if (difcap > 0) {
                if (dif > difcap) dif = difcap;
                if (dif < -difcap) dif = -difcap;
            }
            if (dif != 0) {
                val1 = buf1g[j + 1] + 3 * dif;
                val2 = buf2g[j] + 3 * dif;
                val3 = buf2g[j + 1] + 2 * dif;
                if (dif > 0) {
                    buf1g[j + 1] = L_MIN(16383, val1);
                    buf2g[j] = L_MIN(16383, val2);
                    buf2g[j + 1] = L_MIN(16383, val3);
                } else {
                    buf1g[j + 1] = L_MAX(0, val1);
                    buf2g[j] = L_MAX(0, val2);
                    buf2g[j + 1] = L_MAX(0, val3);
                }
            }

            dif = buf1b[j] / 8 - 8 * bc;
            if (difcap > 0) {
                if (dif > difcap) dif = difcap;
                if (dif < -difcap) dif = -difcap;
            }
            if (dif != 0) {
                val1 = buf1b[j + 1] + 3 * dif;
                val2 = buf2b[j] + 3 * dif;
                val3 = buf2b[j + 1] + 2 * dif;
                if (dif > 0) {
                    buf1b[j + 1] = L_MIN(16383, val1);
                    buf2b[j] = L_MIN(16383, val2);
                    buf2b[j + 1] = L_MIN(16383, val3);
                } else {
                    buf1b[j + 1] = L_MAX(0, val1);
                    buf2b[j] = L_MAX(0, val2);
                    buf2b[j + 1] = L_MAX(0, val3);
                }
            }
        }

            /* Get last pixel in row; no downward propagation */
        rval = buf1r[w - 1] / 64;
        gval = buf1g[w - 1] / 64;
        bval = buf1b[w - 1] / 64;
        octindex = rtab[rval] | gtab[gval] | btab[bval];
        cmapindex = indexmap[octindex] - 1;
        SET_DATA_BYTE(lined, w - 1, cmapindex);
    }

        /* Get last row of pixels; no leftward propagation */
    lined = datad + (h - 1) * wpld;
    for (j = 0; j < w; j++) {
        rval = buf2r[j] / 64;
        gval = buf2g[j] / 64;
        bval = buf2b[j] / 64;
        octindex = rtab[rval] | gtab[gval] | btab[bval];
        cmapindex = indexmap[octindex] - 1;
        SET_DATA_BYTE(lined, j, cmapindex);
    }

buffer_cleanup:
    LEPT_FREE(bufu8r);
    LEPT_FREE(bufu8g);
    LEPT_FREE(bufu8b);
    LEPT_FREE(buf1r);
    LEPT_FREE(buf1g);
    LEPT_FREE(buf1b);
    LEPT_FREE(buf2r);
    LEPT_FREE(buf2g);
    LEPT_FREE(buf2b);

    return (success) ? 0 : 1;
}


/*---------------------------------------------------------------------------*
 *         Adaptive octree quantization to 4 and 8 bpp with max colors       *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixOctreeQuantNumColors()
 *
 * \param[in]    pixs        32 bpp rgb
 * \param[in]    maxcolors   8 to 256; the actual number of colors used
 *                           may be less than this
 * \param[in]    subsample   factor for computing color distribution;
 *                           use 0 for default
 * \return  pixd 4 or 8 bpp, colormapped, or NULL on error
 *
 * <pre>
 *  pixOctreeColorQuant is very flexible in terms of the relative
 *  depth of different cubes of the octree.   By contrast, this function,
 *  pixOctreeQuantNumColors is also adaptive, but it supports octcube
 *  leaves at only two depths: a smaller depth that guarantees
 *  full coverage of the color space and octcubes at one level
 *  deeper for more accurate colors.  Its main virutes are simplicity
 *  and speed, which are both derived from the natural indexing of
 *  the octcubes from the RGB values.
 *
 *  Before describing pixOctreeQuantNumColors, consider an even simpler
 *  approach for 4 bpp with either 8 or 16 colors.  With 8 colors,
 *  you simply go to level 1 octcubes and use the average color
 *  found in each cube.  For 16 colors, you find which of the three
 *  colors has the largest variance at the second level, and use two
 *  indices for that color.  The result is quite poor, because 1 some
 *  of the cubes are nearly empty and 2 you don't get much color
 *  differentiation for the extra 8 colors.  Trust me, this method may
 *  be simple, but it isn't worth anything.
 *
 *  In pixOctreeQuantNumColors, we generate colormapped images at
 *  either 4 bpp or 8 bpp.  For 4 bpp, we have a minimum of 8 colors
 *  for the level 1 octcubes, plus up to 8 additional colors that
 *  are determined from the level 2 popularity.  If the number of colors
 *  is between 8 and 16, the output is a 4 bpp image.  If the number of
 *  colors is greater than 16, the output is a 8 bpp image.
 *
 *  We use a priority queue, implemented with a heap, to select the
 *  requisite number of most populated octcubes at the deepest level
 *  level 2 for 64 or fewer colors; level 3 for more than 64 colors.
 *  These are combined with one color for each octcube one level above,
 *  which is used to span the color space of octcubes that were not
 *  included at the deeper level.
 *
 *  If the deepest level is 2, we combine the popular level 2 octcubes
 *  out of a total of 64 with the 8 level 1 octcubes.  If the deepest
 *  level is 3, we combine the popular level 3 octcubes out of a
 *  total 512 with the 64 level 2 octcubes that span the color space.
 *  In the latter case, we require a minimum of 64 colors for the level 2
 *  octcubes, plus up to 192 additional colors determined from level 3
 *  popularity.
 *
 *  The parameter 'maxlevel' is the deepest octcube level that is used.
 *  The implementation also uses two LUTs, which are employed in
 *  two successive traversals of the dest image.  The first maps
 *  from the src octindex at 'maxlevel' to the color table index,
 *  which is the value that is stored in the 4 or 8 bpp dest pixel.
 *  The second LUT maps from that colormap value in the dest to a
 *  new colormap value for a minimum sized colormap, stored back in
 *  the dest.  It is used to remove any color map entries that
 *  correspond to color space regions that have no pixels in the
 *  source image.  These regions can be either from the higher level
 *  e.g., level 1 for 4 bpp, or from octcubes at 'maxlevel' that
 *  are unoccupied.  This remapping results in the minimum number
 *  of colors used according to the constraints induced by the
 *  input 'maxcolors'.  We also compute the average R, G and B color
 *  values in each region of the color space represented by a
 *  colormap entry, and store them in the colormap.
 *
 *  The maximum number of colors is input, which determines the
 *  following properties of the dest image and octcube regions used:
 *
 *     Number of colors      dest image depth      maxlevel
 *     ----------------      ----------------      --------
 *       8 to 16                  4 bpp               2
 *       17 to 64                 8 bpp               2
 *       65 to 256                8 bpp               3
 *
 *  It may turn out that the number of extra colors, beyond the
 *  minimum 8 and 64 for maxlevel 2 and 3, respectively, is larger
 *  than the actual number of occupied cubes at these levels
 *  In that case, all the pixels are contained in this
 *  subset of cubes at maxlevel, and no colormap colors are needed
 *  to represent the remainder pixels one level above.  Thus, for
 *  example, in use one often finds that the pixels in an image
 *  occupy less than 192 octcubes at level 3, so they can be represented
 *  by a colormap for octcubes at level 3 only.
 * </pre>
 */
PIX *
pixOctreeQuantNumColors(PIX     *pixs,
                        l_int32  maxcolors,
                        l_int32  subsample)
{
l_int32    w, h, minside, bpp, wpls, wpld, i, j, actualcolors;
l_int32    rval, gval, bval, nbase, nextra, maxlevel, ncubes, val;
l_int32   *lut1, *lut2;
l_uint32   index;
l_uint32  *lines, *lined, *datas, *datad, *pspixel;
l_uint32  *rtab, *gtab, *btab;
OQCELL    *oqc;
OQCELL   **oqca;
L_HEAP    *lh;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (maxcolors < 8) {
        L_WARNING("max colors < 8; setting to 8\n", __func__);
        maxcolors = 8;
    }
    if (maxcolors > 256) {
        L_WARNING("max colors > 256; setting to 256\n", __func__);
        maxcolors = 256;
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    minside = L_MIN(w, h);
    if (subsample <= 0) {
       subsample = L_MAX(1, minside / 200);
    }

    if (maxcolors <= 16) {
        bpp = 4;
        pixd = pixCreate(w, h, bpp);
        maxlevel = 2;
        ncubes = 64;   /* 2^6 */
        nbase = 8;
        nextra = maxcolors - nbase;
    } else if (maxcolors <= 64) {
        bpp = 8;
        pixd = pixCreate(w, h, bpp);
        maxlevel = 2;
        ncubes = 64;  /* 2^6 */
        nbase = 8;
        nextra = maxcolors - nbase;
    } else {  /* maxcolors <= 256 */
        bpp = 8;
        pixd = pixCreate(w, h, bpp);
        maxlevel = 3;
        ncubes = 512;  /* 2^9 */
        nbase = 64;
        nextra = maxcolors - nbase;
    }

    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);

        /*----------------------------------------------------------*
         * If we're using the minimum number of colors, it is       *
         * much simpler.  We just use 'nbase' octcubes.             *
         * For this case, we don't eliminate any extra colors.      *
         *----------------------------------------------------------*/
    if (nextra == 0) {
            /* prepare the OctcubeQuantCell array */
        if ((oqca = (OQCELL **)LEPT_CALLOC(nbase, sizeof(OQCELL *))) == NULL) {
            pixDestroy(&pixd);
            return (PIX *)ERROR_PTR("oqca not made", __func__, NULL);
        }
        for (i = 0; i < nbase; i++) {
            oqca[i] = (OQCELL *)LEPT_CALLOC(1, sizeof(OQCELL));
            oqca[i]->n = 0.0;
        }

        rtab = gtab = btab = NULL;
        makeRGBToIndexTables(maxlevel - 1, &rtab, &gtab, &btab);

            /* Go through the entire image, gathering statistics and
             * assigning pixels to their quantized value */
        datad = pixGetData(pixd);
        wpld = pixGetWpl(pixd);
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                pspixel = lines + j;
                extractRGBValues(*pspixel, &rval, &gval, &bval);
                getOctcubeIndexFromRGB(rval, gval, bval,
                                       rtab, gtab, btab, &index);
/*                lept_stderr("rval = %d, gval = %d, bval = %d,"
                              " index = %d\n", rval, gval, bval, index); */
                if (bpp == 4)
                    SET_DATA_QBIT(lined, j, index);
                else  /* bpp == 8 */
                    SET_DATA_BYTE(lined, j, index);
                oqca[index]->n += 1.0;
                oqca[index]->rcum += rval;
                oqca[index]->gcum += gval;
                oqca[index]->bcum += bval;
            }
        }

            /* Compute average color values in each octcube, and
             * generate colormap */
        cmap = pixcmapCreate(bpp);
        pixSetColormap(pixd, cmap);
        for (i = 0; i < nbase; i++) {
            oqc = oqca[i];
            if (oqc->n != 0) {
                oqc->rval = (l_int32)(oqc->rcum / oqc->n);
                oqc->gval = (l_int32)(oqc->gcum / oqc->n);
                oqc->bval = (l_int32)(oqc->bcum / oqc->n);
            } else {
                getRGBFromOctcube(i, maxlevel - 1, &oqc->rval,
                                  &oqc->gval, &oqc->bval);
            }
            pixcmapAddColor(cmap, oqc->rval, oqc->gval, oqc->bval);
        }

        for (i = 0; i < nbase; i++)
            LEPT_FREE(oqca[i]);
        LEPT_FREE(oqca);
        LEPT_FREE(rtab);
        LEPT_FREE(gtab);
        LEPT_FREE(btab);
        return pixd;
    }

        /*------------------------------------------------------------*
         * General case: we will use colors in octcubes at maxlevel.  *
         * We also remove any colors that are not populated from      *
         * the colormap.                                              *
         *------------------------------------------------------------*/
        /* Prepare the OctcubeQuantCell array */
    oqca = (OQCELL **)LEPT_CALLOC(ncubes, sizeof(OQCELL *));
    for (i = 0; i < ncubes; i++) {
        oqca[i] = (OQCELL *)LEPT_CALLOC(1, sizeof(OQCELL));
        oqca[i]->n = 0.0;
    }

        /* Make the tables to map color to the octindex,
         * of which there are 'ncubes' at 'maxlevel' */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(maxlevel, &rtab, &gtab, &btab);

        /* Estimate the color distribution; we want to find the
         * most popular nextra colors at 'maxlevel' */
    for (i = 0; i < h; i += subsample) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j += subsample) {
            pspixel = lines + j;
            extractRGBValues(*pspixel, &rval, &gval, &bval);
            getOctcubeIndexFromRGB(rval, gval, bval, rtab, gtab, btab, &index);
            oqca[index]->n += 1.0;
            oqca[index]->octindex = index;
            oqca[index]->rcum += rval;
            oqca[index]->gcum += gval;
            oqca[index]->bcum += bval;
        }
    }

        /* Transfer the OQCELL from the array, and order in a heap */
    lh = lheapCreate(512, L_SORT_DECREASING);
    for (i = 0; i < ncubes; i++)
        lheapAdd(lh, oqca[i]);
    LEPT_FREE(oqca);  /* don't need this array */

        /* Prepare a new OctcubeQuantCell array, with maxcolors cells  */
    oqca = (OQCELL **)LEPT_CALLOC(maxcolors, sizeof(OQCELL *));
    for (i = 0; i < nbase; i++) {  /* make nbase cells */
        oqca[i] = (OQCELL *)LEPT_CALLOC(1, sizeof(OQCELL));
        oqca[i]->n = 0.0;
    }

        /* Remove the nextra most populated ones, and put them in the array */
    for (i = 0; i < nextra; i++) {
        oqc = (OQCELL *)lheapRemove(lh);
        oqc->n = 0.0;  /* reinit */
        oqc->rcum = 0;
        oqc->gcum = 0;
        oqc->bcum = 0;
        oqca[nbase + i] = oqc;  /* store it in the array */
    }

        /* Destroy the heap and its remaining contents */
    lheapDestroy(&lh, TRUE);

        /* Generate a lookup table from octindex at maxlevel
         * to color table index */
    lut1 = (l_int32 *)LEPT_CALLOC(ncubes, sizeof(l_int32));
    for (i = 0; i < nextra; i++)
        lut1[oqca[nbase + i]->octindex] = nbase + i;
    for (index = 0; index < ncubes; index++) {
        if (lut1[index] == 0)  /* not one of the extras; need to assign */
            lut1[index] = index >> 3;  /* remove the least significant bits */
/*        lept_stderr("lut1[%d] = %d\n", index, lut1[index]); */
    }

        /* Go through the entire image, gathering statistics and
         * assigning pixels to their quantized value */
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pspixel = lines + j;
            extractRGBValues(*pspixel, &rval, &gval, &bval);
            getOctcubeIndexFromRGB(rval, gval, bval, rtab, gtab, btab, &index);
/*            lept_stderr("rval = %d, gval = %d, bval = %d, index = %d\n",
                          rval, gval, bval, index); */
            val = lut1[index];
            switch (bpp) {
            case 4:
                SET_DATA_QBIT(lined, j, val);
                break;
            case 8:
                SET_DATA_BYTE(lined, j, val);
                break;
            default:
                LEPT_FREE(oqca);
                LEPT_FREE(lut1);
                return (PIX *)ERROR_PTR("bpp not 4 or 8!", __func__, NULL);
            }
            oqca[val]->n += 1.0;
            oqca[val]->rcum += rval;
            oqca[val]->gcum += gval;
            oqca[val]->bcum += bval;
        }
    }

        /* Compute averages, set up a colormap, and make a second
         * lut that converts from the color values currently in
         * the image to a minimal set */
    lut2 = (l_int32 *)LEPT_CALLOC(ncubes, sizeof(l_int32));
    cmap = pixcmapCreate(bpp);
    pixSetColormap(pixd, cmap);
    for (i = 0, index = 0; i < maxcolors; i++) {
        oqc = oqca[i];
        lut2[i] = index;
        if (oqc->n == 0)  /* no occupancy; don't bump up index */
            continue;
        oqc->rval = (l_int32)(oqc->rcum / oqc->n);
        oqc->gval = (l_int32)(oqc->gcum / oqc->n);
        oqc->bval = (l_int32)(oqc->bcum / oqc->n);
        pixcmapAddColor(cmap, oqc->rval, oqc->gval, oqc->bval);
        index++;
    }
/*    pixcmapWriteStream(stderr, cmap); */
    actualcolors = pixcmapGetCount(cmap);
/*    lept_stderr("Number of different colors = %d\n", actualcolors); */

        /* Last time through the image; use the lookup table to
         * remap the pixel value to the minimal colormap */
    if (actualcolors < maxcolors) {
        for (i = 0; i < h; i++) {
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                switch (bpp) {
                case 4:
                    val = GET_DATA_QBIT(lined, j);
                    SET_DATA_QBIT(lined, j, lut2[val]);
                    break;
                case 8:
                    val = GET_DATA_BYTE(lined, j);
                    SET_DATA_BYTE(lined, j, lut2[val]);
                    break;
                }
            }
        }
    }

    if (oqca) {
        for (i = 0; i < maxcolors; i++)
            LEPT_FREE(oqca[i]);
    }
    LEPT_FREE(oqca);
    LEPT_FREE(lut1);
    LEPT_FREE(lut2);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return pixd;
}


/*-------------------------------------------------------------------------*
 *      Mixed color/gray quantization with specified number of colors      *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixOctcubeQuantMixedWithGray()
 *
 * \param[in]    pixs        32 bpp rgb
 * \param[in]    depth       of output pix
 * \param[in]    graylevels  graylevels (must be > 1)
 * \param[in]    delta       threshold for deciding if a pix is color or gray
 * \return  pixd     quantized to octcube and gray levels or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a colormapped image, where the colormap table values
 *          have two components: octcube values representing pixels with
 *          color content, and grayscale values for the rest.
 *      (2) The threshold (delta) is the maximum allowable difference of
 *          the max abs value of | r - g |, | r - b | and | g - b |.
 *      (3) The octcube values are the averages of all pixels that are
 *          found in the octcube, and that are far enough from gray to
 *          be considered color.  This can roughly be visualized as all
 *          the points in the rgb color cube that are not within a "cylinder"
 *          of diameter approximately 'delta' along the main diagonal.
 *      (4) We want to guarantee full coverage of the rgb color space; thus,
 *          if the output depth is 4, the octlevel is 1 (2 x 2 x 2 = 8 cubes)
 *          and if the output depth is 8, the octlevel is 2 (4 x 4 x 4
 *          = 64 cubes).
 *      (5) Consequently, we have the following constraint on the number
 *          of allowed gray levels: for 4 bpp, 8; for 8 bpp, 192.
 * </pre>
 */
PIX *
pixOctcubeQuantMixedWithGray(PIX     *pixs,
                             l_int32  depth,
                             l_int32  graylevels,
                             l_int32  delta)
{
l_int32    w, h, wpls, wpld, i, j, size, octlevels;
l_int32    rval, gval, bval, del, val, midval;
l_int32   *carray, *rarray, *garray, *barray;
l_int32   *tabval;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *lines, *lined, *datas, *datad;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (graylevels < 2)
        return (PIX *)ERROR_PTR("invalid graylevels", __func__, NULL);
    if (depth == 4) {
        octlevels = 1;
        size = 8;   /* 2 ** 3 */
        if (graylevels > 8)
            return (PIX *)ERROR_PTR("max 8 gray levels", __func__, NULL);
    } else if (depth == 8) {
        octlevels = 2;
        size = 64;   /* 2 ** 6 */
        if (graylevels > 192)
            return (PIX *)ERROR_PTR("max 192 gray levels", __func__, NULL);
    } else {
        return (PIX *)ERROR_PTR("output depth not 4 or 8 bpp", __func__, NULL);
    }

    pixd = NULL;

        /* Make octcube index tables */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(octlevels, &rtab, &gtab, &btab);

        /* Make octcube arrays for storing points in each cube */
    carray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    rarray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    garray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    barray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));

        /* Make lookup table, using computed thresholds  */
    tabval = makeGrayQuantIndexTable(graylevels);
    if (!rtab || !gtab || !btab ||
        !carray || !rarray || !garray || !barray || !tabval) {
        L_ERROR("calloc fail for an array\n", __func__);
        goto array_cleanup;
    }

        /* Make colormapped output pixd */
    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, depth)) == NULL) {
        L_ERROR("pixd not made\n", __func__);
        goto array_cleanup;
    }
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    cmap = pixcmapCreate(depth);
    for (j = 0; j < size; j++)  /* reserve octcube colors */
        pixcmapAddColor(cmap, 1, 1, 1);  /* a color that won't be used */
    for (j = 0; j < graylevels; j++) {  /* set grayscale colors */
        val = (255 * j) / (graylevels - 1);
        pixcmapAddColor(cmap, val, val, val);
    }
    pixSetColormap(pixd, cmap);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* Go through src image: assign dest pixels to colormap values
         * and compute average colors in each occupied octcube */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            if (rval > gval) {
                if (gval > bval) {   /* r > g > b */
                    del = rval - bval;
                    midval = gval;
                } else if (rval > bval) {  /* r > b > g */
                    del = rval - gval;
                    midval = bval;
                } else {  /* b > r > g */
                    del = bval - gval;
                    midval = rval;
                }
            } else {  /* gval >= rval */
                if (rval > bval) {  /* g > r > b */
                    del = gval - bval;
                    midval = rval;
                } else if (gval > bval) {  /* g > b > r */
                    del = gval - rval;
                    midval = bval;
                } else {  /* b > g > r */
                    del = bval - rval;
                    midval = gval;
                }
            }
            if (del > delta) {  /* assign to color */
                octindex = rtab[rval] | gtab[gval] | btab[bval];
                carray[octindex]++;
                rarray[octindex] += rval;
                garray[octindex] += gval;
                barray[octindex] += bval;
                if (depth == 4)
                    SET_DATA_QBIT(lined, j, octindex);
                else  /* depth == 8 */
                    SET_DATA_BYTE(lined, j, octindex);
            } else {  /* assign to grayscale */
                val = size + tabval[midval];
                if (depth == 4)
                    SET_DATA_QBIT(lined, j, val);
                else  /* depth == 8 */
                    SET_DATA_BYTE(lined, j, val);
            }
        }
    }

        /* Average the colors in each bin and reset the colormap */
    for (i = 0; i < size; i++) {
        if (carray[i] > 0) {
            rarray[i] /= carray[i];
            garray[i] /= carray[i];
            barray[i] /= carray[i];
            pixcmapResetColor(cmap, i, rarray[i], garray[i], barray[i]);
        }
    }

array_cleanup:
    LEPT_FREE(carray);
    LEPT_FREE(rarray);
    LEPT_FREE(garray);
    LEPT_FREE(barray);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    LEPT_FREE(tabval);

    return pixd;
}


/*-------------------------------------------------------------------------*
 *             Fixed partition octcube quantization with 256 cells         *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixFixedOctcubeQuant256()
 *
 * \param[in]    pixs         32 bpp; 24-bit color
 * \param[in]    ditherflag   1 for dithering; 0 for no dithering
 * \return  pixd 8 bit with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *  This simple 1-pass color quantization works by breaking the
 *  color space into 256 pieces, with 3 bits quantized for each of
 *  red and green, and 2 bits quantized for blue.  We shortchange
 *  blue because the eye is least sensitive to blue.  This
 *  division of the color space is into two levels of octrees,
 *  followed by a further division by 4 not 8, where both
 *  blue octrees have been combined in the third level.
 *
 *  The color map is generated from the 256 color centers by
 *  taking the representative color to be the center of the
 *  cell volume.  This gives a maximum error in the red and
 *  green values of 16 levels, and a maximum error in the
 *  blue sample of 32 levels.
 *
 *  Each pixel in the 24-bit color image is placed in its containing
 *  cell, given by the relevant MSbits of the red, green and blue
 *  samples.  An error-diffusion dithering is performed on each
 *  color sample to give the appearance of good average local color.
 *  Dithering is required; without it, the contouring and visible
 *  color errors are very bad.
 *
 *  I originally implemented this algorithm in two passes,
 *  where the first pass was used to compute the weighted average
 *  of each sample in each pre-allocated region of color space.
 *  The idea was to use these centroids in the dithering algorithm
 *  of the second pass, to reduce the average error that was
 *  being dithered.  However, with dithering, there is
 *  virtually no difference, so there is no reason to make the
 *  first pass.  Consequently, this 1-pass version just assigns
 *  the pixels to the centers of the pre-allocated cells.
 *  We use dithering to spread the difference between the sample
 *  value and the location of the center of the cell.  For speed
 *  and simplicity, we use integer dithering and propagate only
 *  to the right, down, and diagonally down-right, with ratios
 *  3/8, 3/8 and 1/4, respectively.  The results should be nearly
 *  as good, and a bit faster, with propagation only to the right
 *  and down.
 *
 *  The algorithm is very fast, because there is no search,
 *  only fast generation of the cell index for each pixel.
 *  We use a simple mapping from the three 8 bit rgb samples
 *  to the 8 bit cell index; namely, r7 r6 r5 g7 g6 g5 b7 b6.
 *  This is not in an octcube format, but it doesn't matter.
 *  There are no storage requirements.  We could keep a
 *  running average of the center of each sample in each
 *  cluster, rather than using the center of the cell, but
 *  this is just extra work, esp. with dithering.
 *
 *  This method gives surprisingly good results with dithering.
 *  However, without dithering, the loss of color accuracy is
 *  evident in regions that are very light or that have subtle
 *  blending of colors.
 * </pre>
 */
PIX *
pixFixedOctcubeQuant256(PIX     *pixs,
                        l_int32  ditherflag)
{
l_uint8    index;
l_int32    rval, gval, bval;
l_int32    w, h, wpls, wpld, i, j, cindex;
l_uint32  *rtab, *gtab, *btab;
l_int32   *itab;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);

        /* Do not dither if image is very small */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w < MinDitherSize && h < MinDitherSize && ditherflag == 1) {
        L_INFO("Small image: dithering turned off\n", __func__);
        ditherflag = 0;
    }

        /* Find the centers of the 256 cells, each of which represents
         * the 3 MSBits of the red and green components, and the
         * 2 MSBits of the blue component.  This gives a mapping
         * from a "cube index" to the rgb values.  Save all 256
         * rgb values of these centers in a colormap.
         * For example, to get the red color of the cell center,
         * you take the 3 MSBits of to the index and add the
         * offset to the center of the cell, which is 0x10. */
    cmap = pixcmapCreate(8);
    for (cindex = 0; cindex < 256; cindex++) {
        rval = (cindex & 0xe0) | 0x10;
        gval = ((cindex << 3) & 0xe0) | 0x10;
        bval = ((cindex << 6) & 0xc0) | 0x20;
        pixcmapAddColor(cmap, rval, gval, bval);
    }

        /* Make output 8 bpp palette image */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(w, h, 8)) == NULL) {
        pixcmapDestroy(&cmap);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Set dest pix values to colortable indices */
    if (ditherflag == 0) {   /* no dithering */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                extractRGBValues(lines[j], &rval, &gval, &bval);
                index = (rval & 0xe0) | ((gval >> 3) & 0x1c) | (bval >> 6);
                SET_DATA_BYTE(lined, j, index);
            }
        }
    } else {  /* ditherflag == 1 */
            /* Set up conversion tables from rgb directly to the colormap
             * index.  However, the dithering function expects these tables
             * to generate an octcube index (+1), and the table itab[] to
             * convert to the colormap index.  So we make a trivial
             * itab[], that simply compensates for the -1 in
             * pixDitherOctindexWithCmap().   No cap is required on
             * the propagated difference.  */
        rtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
        gtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
        btab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
        itab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
        if (!rtab || !gtab || !btab || !itab) {
            pixDestroy(&pixd);
            return (PIX *)ERROR_PTR("calloc fail for table", __func__, NULL);
        }
        for (i = 0; i < 256; i++) {
            rtab[i] = i & 0xe0;
            gtab[i] = (i >> 3) & 0x1c;
            btab[i] = i >> 6;
            itab[i] = i + 1;
        }
        pixDitherOctindexWithCmap(pixs, pixd, rtab, gtab, btab, itab,
                                  FIXED_DIF_CAP);
        LEPT_FREE(rtab);
        LEPT_FREE(gtab);
        LEPT_FREE(btab);
        LEPT_FREE(itab);
    }

    return pixd;
}


/*---------------------------------------------------------------------------*
 *           Nearly exact quantization for images with few colors            *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixFewColorsOctcubeQuant1()
 *
 * \param[in]    pixs    32 bpp rgb
 * \param[in]    level   significant bits for each of RGB; valid in [1...6]
 * \return  pixd quantized to octcube or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a colormapped image, where the colormap table values
 *          are the averages of all pixels that are found in the octcube.
 *      (2) This fails if there are more than 256 colors (i.e., more
 *          than 256 occupied octcubes).
 *      (3) Often level 3 (512 octcubes) will succeed because not more
 *          than half of them are occupied with 1 or more pixels.
 *      (4) The depth of the result, which is either 2, 4 or 8 bpp,
 *          is the minimum required to hold the number of colors that
 *          are found.
 *      (5) This can be useful for quantizing orthographically generated
 *          images such as color maps, where there may be more than 256 colors
 *          because of aliasing or jpeg artifacts on text or lines, but
 *          there are a relatively small number of solid colors.  Then,
 *          use with level = 3 can often generate a compact and accurate
 *          representation of the original RGB image.  For this purpose,
 *          it is better than pixFewColorsOctcubeQuant2(), because it
 *          uses the average value of pixels in the octcube rather
 *          than the first found pixel.  It is also simpler to use,
 *          because it generates the histogram internally.
 * </pre>
 */
PIX *
pixFewColorsOctcubeQuant1(PIX     *pixs,
                          l_int32  level)
{
l_int32    w, h, wpls, wpld, i, j, depth, size, ncolors, index;
l_int32    rval, gval, bval;
l_int32   *carray, *rarray, *garray, *barray;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *lines, *lined, *datas, *datad, *pspixel;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (level < 1 || level > 6)
        return (PIX *)ERROR_PTR("invalid level", __func__, NULL);

    pixd = NULL;

    if (octcubeGetCount(level, &size))  /* array size = 2 ** (3 * level) */
        return (PIX *)ERROR_PTR("size not returned", __func__, NULL);
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);

    carray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    rarray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    garray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    barray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32));
    if (!carray || !rarray || !garray || !barray) {
        L_ERROR("calloc fail for an array\n", __func__);
        goto array_cleanup;
    }

        /* Place the pixels in octcube leaves. */
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j++) {
            pspixel = lines + j;
            extractRGBValues(*pspixel, &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            carray[octindex]++;
            rarray[octindex] += rval;
            garray[octindex] += gval;
            barray[octindex] += bval;
        }
    }

        /* Find the number of different colors */
    for (i = 0, ncolors = 0; i < size; i++) {
        if (carray[i] > 0)
            ncolors++;
    }
    if (ncolors > 256) {
        L_WARNING("%d colors found; more than 256\n", __func__, ncolors);
        goto array_cleanup;
    }
    if (ncolors <= 4)
        depth = 2;
    else if (ncolors <= 16)
        depth = 4;
    else
        depth = 8;

        /* Average the colors in each octcube leaf and add to colormap table;
         * then use carray to hold the colormap index + 1  */
    cmap = pixcmapCreate(depth);
    for (i = 0, index = 0; i < size; i++) {
        if (carray[i] > 0) {
            rarray[i] /= carray[i];
            garray[i] /= carray[i];
            barray[i] /= carray[i];
            pixcmapAddColor(cmap, rarray[i], garray[i], barray[i]);
            carray[i] = index + 1;  /* to avoid storing 0 */
            index++;
        }
    }

    pixd = pixCreate(w, h, depth);
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pspixel = lines + j;
            extractRGBValues(*pspixel, &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            switch (depth)
            {
            case 2:
                SET_DATA_DIBIT(lined, j, carray[octindex] - 1);
                break;
            case 4:
                SET_DATA_QBIT(lined, j, carray[octindex] - 1);
                break;
            case 8:
                SET_DATA_BYTE(lined, j, carray[octindex] - 1);
                break;
            default:
                L_WARNING("shouldn't get here\n", __func__);
            }
        }
    }

array_cleanup:
    LEPT_FREE(carray);
    LEPT_FREE(rarray);
    LEPT_FREE(garray);
    LEPT_FREE(barray);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return pixd;
}


/*!
 * \brief   pixFewColorsOctcubeQuant2()
 *
 * \param[in]    pixs       32 bpp rgb
 * \param[in]    level      of octcube indexing, for histogram: 3, 4, 5, 6
 * \param[in]    na         histogram of pixel occupation in octree leaves
 *                          at given level
 * \param[in]    ncolors    number of occupied octree leaves at given level
 * \param[out]   pnerrors   [optional] num of pixels not exactly
 *                          represented in the colormap
 * \return  pixd 2, 4 or 8 bpp with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a colormapped image, where the colormap table values
 *          are the averages of all pixels that are found in the octcube.
 *      (2) This fails if there are more than 256 colors (i.e., more
 *          than 256 occupied octcubes).
 *      (3) Often level 3 (512 octcubes) will succeed because not more
 *          than half of them are occupied with 1 or more pixels.
 *      (4) For an image with not more than 256 colors, it is unlikely
 *          that two pixels of different color will fall in the same
 *          octcube at level = 4.   However it is possible, and this
 *          function optionally returns %nerrors, the number of pixels
 *          where, because more than one color is in the same octcube,
 *          the pixel color is not exactly reproduced in the colormap.
 *          The colormap for an occupied leaf of the octree contains
 *          the color of the first pixel encountered in that octcube.
 *      (5) This differs from pixFewColorsOctcubeQuant1(), which also
 *          requires not more than 256 occupied leaves, but represents
 *          the color of each leaf by an average over the pixels in
 *          that leaf.  This also requires precomputing the histogram
 *          of occupied octree leaves, which is generated using
 *          pixOctcubeHistogram().
 *      (6) This is used in pixConvertRGBToColormap() for images that
 *          are determined, by their histogram, to have relatively few
 *          colors.  This typically happens with orthographically
 *          produced images (as oppopsed to natural images), where
 *          it is expected that most of the pixels within a leaf
 *          octcube have exactly the same color, and quantization to
 *          that color is lossless.
 * </pre>
 */
PIX *
pixFewColorsOctcubeQuant2(PIX      *pixs,
                          l_int32   level,
                          NUMA     *na,
                          l_int32   ncolors,
                          l_int32  *pnerrors)
{
l_int32    w, h, wpls, wpld, i, j, nerrors;
l_int32    ncubes, depth, cindex, oval;
l_int32    rval, gval, bval;
l_int32   *octarray = NULL;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *lines, *lined, *datas, *datad, *ppixel;
l_uint32  *colorarray = NULL;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (level < 3 || level > 6)
        return (PIX *)ERROR_PTR("level not in {4, 5, 6}", __func__, NULL);
    if (ncolors > 256)
        return (PIX *)ERROR_PTR("ncolors > 256", __func__, NULL);
    if (pnerrors)
        *pnerrors = UNDEF;

    pixd = NULL;

        /* Represent the image with a set of leaf octcubes
         * at 'level', one for each color. */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);

        /* The octarray will give a ptr from the octcube to the colorarray */
    if ((ncubes = numaGetCount(na)) == 0) {
        L_ERROR("no slots in pixel occupation histogram", __func__);
        goto cleanup_arrays;
    }
    octarray = (l_int32 *)LEPT_CALLOC(ncubes, sizeof(l_int32));

        /* The colorarray will hold the colors of the first pixel
         * that lands in the leaf octcube.  After filling, it is
         * used to generate the colormap.  */
    colorarray = (l_uint32 *)LEPT_CALLOC(ncolors + 1, sizeof(l_uint32));
    if (!octarray || !colorarray) {
        L_ERROR("octarray or colorarray not made\n", __func__);
        goto cleanup_arrays;
    }

        /* Determine the output depth from the number of colors */
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (ncolors <= 4)
        depth = 2;
    else if (ncolors <= 16)
        depth = 4;
    else  /* ncolors <= 256 */
        depth = 8;

    if ((pixd = pixCreate(w, h, depth)) == NULL) {
        L_ERROR("pixd not made\n", __func__);
        goto cleanup_arrays;
    }
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* For each pixel, get the octree index for its leaf octcube.
         * Check if a pixel has already been found in this octcube.
         *   ~ If not yet found, save that color in the colorarray
         *     and save the cindex in the octarray.
         *   ~ If already found, compare the pixel color with the
         *     color in the colorarray, and note if it differs.
         * Then set the dest pixel value to the cindex - 1, which
         * will be the cmap index for this color.  */
    cindex = 1;  /* start with 1 */
    nerrors = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            ppixel = lines + j;
            extractRGBValues(*ppixel, &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            oval = octarray[octindex];
            if (oval == 0) {
                octarray[octindex] = cindex;
                colorarray[cindex] = *ppixel;
                setPixelLow(lined, j, depth, cindex - 1);
                cindex++;
            } else {  /* already have seen this color; is it unique? */
                setPixelLow(lined, j, depth, oval - 1);
                if (colorarray[oval] != *ppixel)
                    nerrors++;
            }
        }
    }
    if (pnerrors)
        *pnerrors = nerrors;

#if  DEBUG_FEW_COLORS
    lept_stderr("ncubes = %d, ncolors = %d\n", ncubes, ncolors);
    for (i = 0; i < ncolors; i++)
        lept_stderr("color[%d] = %x\n", i, colorarray[i + 1]);
#endif  /* DEBUG_FEW_COLORS */

        /* Make the colormap. */
    cmap = pixcmapCreate(depth);
    for (i = 0; i < ncolors; i++) {
        ppixel = colorarray + i + 1;
        extractRGBValues(*ppixel, &rval, &gval, &bval);
        pixcmapAddColor(cmap, rval, gval, bval);
    }
    pixSetColormap(pixd, cmap);

cleanup_arrays:
    LEPT_FREE(octarray);
    LEPT_FREE(colorarray);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);

    return pixd;
}


/*!
 * \brief   pixFewColorsOctcubeQuantMixed()
 *
 * \param[in]    pixs          32 bpp rgb
 * \param[in]    level         significant octcube bits for each of RGB;
 *                             valid in [1...6]; use 0 for default
 * \param[in]    darkthresh    threshold near black; if the lightest component
 *                             is below this, the pixel is not considered to
 *                             be gray or color; uses 0 for default
 * \param[in]    lightthresh   threshold near white; if the darkest component
 *                             is above this, the pixel is not considered to
 *                             be gray or color; use 0 for default
 * \param[in]    diffthresh    thresh for the max difference between component
 *                             values; for differences below this, the pixel
 *                             is considered to be gray; use 0 for default
 * \param[in]    minfract      min fraction of pixels for gray histo bin;
 *                             use 0.0 for default
 * \param[in]    maxspan       max size of gray histo bin; use 0 for default
 * \return  pixd 8 bpp, quantized to octcube for pixels that are
 *                    not gray; gray pixels are quantized separately
 *                    over the full gray range, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) First runs pixFewColorsOctcubeQuant1().  If this succeeds,
 *          it separates the color from gray(ish) entries in the cmap,
 *          and re-quantizes the gray pixels.  The result has some pixels
 *          in color and others in gray.
 *      (2) This fails if there are more than 256 colors (i.e., more
 *          than 256 occupied octcubes in the color quantization).
 *      (3) Level 3 (512 octcubes) will usually succeed because not more
 *          than half of them are occupied with 1 or more pixels.
 *      (4) This uses the criterion from pixColorFraction() for deciding
 *          if a colormap entry is color; namely, if the color components
 *          are not too close to either black or white, and the maximum
 *          difference between component values equals or exceeds a threshold.
 *      (5) For quantizing the gray pixels, it uses a histogram-based
 *          method where input parameters determining the buckets are
 *          the minimum population fraction and the maximum allowed size.
 *      (6) Recommended input parameters are:
 *              %level:  3 or 4  (3 is default)
 *              %darkthresh:  20
 *              %lightthresh: 244
 *              %diffthresh: 20
 *              %minfract: 0.05
 *              %maxspan: 15
 *          These numbers are intended to be conservative (somewhat over-
 *          sensitive) in color detection,  It's usually better to pay
 *          extra with octcube quantization of a grayscale image than
 *          to use grayscale quantization on an image that has some
 *          actual color.  Input 0 on any of these to get the default.
 *      (7) This can be useful for quantizing orthographically generated
 *          images such as color maps, where there may be more than 256 colors
 *          because of aliasing or jpeg artifacts on text or lines, but
 *          there are a relatively small number of solid colors.  It usually
 *          gives results that are better than pixOctcubeQuantMixedWithGray(),
 *          both in size and appearance.  But it is a bit slower.
 * </pre>
 */
PIX *
pixFewColorsOctcubeQuantMixed(PIX       *pixs,
                              l_int32    level,
                              l_int32    darkthresh,
                              l_int32    lightthresh,
                              l_int32    diffthresh,
                              l_float32  minfract,
                              l_int32    maxspan)
{
l_int32    i, j, w, h, wplc, wplm, wpld, ncolors, index;
l_int32    rval, gval, bval, val, minval, maxval;
l_int32   *lut;
l_uint32  *datac, *datam, *datad, *linec, *linem, *lined;
PIX       *pix1, *pixc, *pixm, *pixg, *pixd;
PIXCMAP   *cmap, *cmapd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (level <= 0) level = 3;
    if (level > 6)
        return (PIX *)ERROR_PTR("invalid level", __func__, NULL);
    if (darkthresh <= 0) darkthresh = 20;
    if (lightthresh <= 0) lightthresh = 244;
    if (diffthresh <= 0) diffthresh = 20;
    if (minfract <= 0.0) minfract = 0.05f;
    if (maxspan <= 2) maxspan = 15;

        /* Start with a simple fixed octcube quantizer. */
    if ((pix1 = pixFewColorsOctcubeQuant1(pixs, level)) == NULL)
        return (PIX *)ERROR_PTR("too many colors", __func__, NULL);
    pixc = pixConvertTo8(pix1, 1);  /* must be 8 bpp */
    pixDestroy(&pix1);

        /* Identify and save color entries in the colormap.  Set up a LUT
         * that returns -1 for any gray pixel. */
    cmap = pixGetColormap(pixc);
    ncolors = pixcmapGetCount(cmap);
    cmapd = pixcmapCreate(8);
    lut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++)
        lut[i] = -1;
    for (i = 0, index = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &rval, &gval, &bval);
        minval = L_MIN(rval, gval);
        minval = L_MIN(minval, bval);
        if (minval > lightthresh)  /* near white */
            continue;
        maxval = L_MAX(rval, gval);
        maxval = L_MAX(maxval, bval);
        if (maxval < darkthresh)  /* near black */
            continue;

            /* Use the max diff between components to test for color */
        if (maxval - minval >= diffthresh) {
            pixcmapAddColor(cmapd, rval, gval, bval);
            lut[i] = index;
            index++;
        }
    }

        /* Generate dest pix with just the color pixels set to their
         * colormap indices.  At the same time, make a 1 bpp mask
         * of the non-color pixels */
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 8);
    pixSetColormap(pixd, cmapd);
    pixm = pixCreate(w, h, 1);
    datac = pixGetData(pixc);
    datam = pixGetData(pixm);
    datad = pixGetData(pixd);
    wplc = pixGetWpl(pixc);
    wplm = pixGetWpl(pixm);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        linem = datam + i * wplm;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(linec, j);
            if (lut[val] == -1)
                SET_DATA_BIT(linem, j);
            else
                SET_DATA_BYTE(lined, j, lut[val]);
        }
    }

        /* Fill in the gray values.  Use a grayscale version of pixs
         * as input, along with the mask over the actual gray pixels. */
    pixg = pixConvertTo8(pixs, 0);
    pixGrayQuantFromHisto(pixd, pixg, pixm, minfract, maxspan);

    LEPT_FREE(lut);
    pixDestroy(&pixc);
    pixDestroy(&pixm);
    pixDestroy(&pixg);
    return pixd;
}


/*---------------------------------------------------------------------------*
 *           Fixed partition octcube quantization with RGB output            *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixFixedOctcubeQuantGenRGB()
 *
 * \param[in]    pixs    32 bpp rgb
 * \param[in]    level   significant bits for each of r,g,b
 * \return  pixd rgb; quantized to octcube centers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Unlike the other color quantization functions, this one
 *          generates an rgb image.
 *      (2) The pixel values are quantized to the center of each octcube
 *          (at the specified level) containing the pixel.  They are
 *          not quantized to the average of the pixels in that octcube.
 * </pre>
 */
PIX *
pixFixedOctcubeQuantGenRGB(PIX     *pixs,
                           l_int32  level)
{
l_int32    w, h, wpls, wpld, i, j;
l_int32    rval, gval, bval;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *lines, *lined, *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (level < 1 || level > 6)
        return (PIX *)ERROR_PTR("level not in {1,...6}", __func__, NULL);

    if (makeRGBToIndexTables(level, &rtab, &gtab, &btab))
        return (PIX *)ERROR_PTR("tables not made", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 32);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            getRGBFromOctcube(octindex, level, &rval, &gval, &bval);
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }

    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return pixd;
}


/*------------------------------------------------------------------*
 *          Color quantize RGB image using existing colormap        *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixQuantFromCmap()
 *
 * \param[in]    pixs       8 bpp grayscale without cmap, or 32 bpp rgb
 * \param[in]    cmap       to quantize to; insert copy into dest pix
 * \param[in]    mindepth   minimum depth of pixd: can be 2, 4 or 8 bpp
 * \param[in]    level      of octcube used for finding nearest color in cmap
 * \param[in]    metric     L_MANHATTAN_DISTANCE, L_EUCLIDEAN_DISTANCE
 * \return  pixd  2, 4 or 8 bpp, colormapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a top-level wrapper for quantizing either grayscale
 *          or rgb images to a specified colormap.
 *      (2) The actual output depth is constrained by %mindepth and
 *          by the number of colors in %cmap.
 *      (3) For grayscale, %level and %metric are ignored.
 *      (4) If the cmap has color and pixs is grayscale, the color is
 *          removed from the cmap before quantizing pixs.
 * </pre>
 */
PIX *
pixQuantFromCmap(PIX      *pixs,
                 PIXCMAP  *cmap,
                 l_int32   mindepth,
                 l_int32   level,
                 l_int32   metric)
{
l_int32  d;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (mindepth != 2 && mindepth != 4 && mindepth != 8)
        return (PIX *)ERROR_PTR("invalid mindepth", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d == 8)
        return pixGrayQuantFromCmap(pixs, cmap, mindepth);
    else if (d == 32)
        return pixOctcubeQuantFromCmap(pixs, cmap, mindepth,
                                       level, metric);
    else
        return (PIX *)ERROR_PTR("d not 8 or 32 bpp", __func__, NULL);
}



/*!
 * \brief   pixOctcubeQuantFromCmap()
 *
 * \param[in]    pixs       32 bpp rgb
 * \param[in]    cmap       to quantize to; insert copy into dest pix
 * \param[in]    mindepth   minimum depth of pixd: can be 2, 4 or 8 bpp
 * \param[in]    level      of octcube used for finding nearest color in cmap
 * \param[in]    metric     L_MANHATTAN_DISTANCE, L_EUCLIDEAN_DISTANCE
 * \return  pixd  2, 4 or 8 bpp, colormapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) In typical use, we are doing an operation, such as
 *          interpolative scaling, on a colormapped pix, where it is
 *          necessary to remove the colormap before the operation.
 *          We then want to re-quantize the RGB result using the same
 *          colormap.
 *      (2) The level is used to divide the color space into octcubes.
 *          Each input pixel is, in effect, placed at the center of an
 *          octcube at the given level, and it is mapped into the
 *          exact color (given in the colormap) that is the closest
 *          to that location.  We need to know that distance, for each color
 *          in the colormap.  The higher the level of the octtree, the smaller
 *          the octcubes in the color space, and hence the more accurately
 *          we can determine the closest color in the colormap; however,
 *          the size of the LUT, which is the total number of octcubes,
 *          increases by a factor of 8 for each increase of 1 level.
 *          The time required to acquire a level 4 mapping table, which has
 *          about 4K entries, is less than 1 msec, so that is the
 *          recommended minimum size to be used.  At that size, the
 *          octcubes have their centers 16 units apart in each (r,g,b)
 *          direction.  If two colors are in the same octcube, the one
 *          closest to the center will always be chosen.  The maximum
 *          error for any component occurs when the correct color is
 *          at a cube corner and there is an incorrect color just inside
 *          the cube next to the opposite corner, giving an error of
 *          14 units (out of 256) for each component.   Using a level 5
 *          mapping table reduces the maximum error to 6 units.
 *      (3) Typically you should use the Euclidean metric, because the
 *          resulting voronoi cells (which are generated using the actual
 *          colormap values as seeds) are convex for Euclidean distance
 *          but not for Manhattan distance.  In terms of the octcubes,
 *          convexity of the voronoi cells means that if the 8 corners
 *          of any cube (of which the octcubes are special cases)
 *          are all within a cell, then every point in the cube will
 *          lie within the cell.
 *      (4) The depth of the output pixd is equal to the maximum of
 *          (a) %mindepth and (b) the minimum (2, 4 or 8 bpp) necessary
 *          to hold the indices in the colormap.
 *      (5) We build a mapping table from octcube to colormap index so
 *          that this function can run in a time (otherwise) independent
 *          of the number of colors in the colormap.  This avoids a
 *          brute-force search for the closest colormap color to each
 *          pixel in the image.
 *      (6) This is similar to the function pixAssignToNearestColor()
 *          used for color segmentation.
 *      (7) Except for very small images or when using level > 4,
 *          it takes very little time to generate the tables,
 *          compared to the generation of the colormapped dest pix,
 *          so one would not typically use the low-level version.
 * </pre>
 */
PIX *
pixOctcubeQuantFromCmap(PIX      *pixs,
                        PIXCMAP  *cmap,
                        l_int32   mindepth,
                        l_int32   level,
                        l_int32   metric)
{
l_int32   *cmaptab;
l_uint32  *rtab, *gtab, *btab;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (!cmap)
        return (PIX *)ERROR_PTR("cmap not defined", __func__, NULL);
    if (mindepth != 2 && mindepth != 4 && mindepth != 8)
        return (PIX *)ERROR_PTR("invalid mindepth", __func__, NULL);
    if (level < 1 || level > 6)
        return (PIX *)ERROR_PTR("level not in {1...6}", __func__, NULL);
    if (metric != L_MANHATTAN_DISTANCE && metric != L_EUCLIDEAN_DISTANCE)
        return (PIX *)ERROR_PTR("invalid metric", __func__, NULL);

        /* Set up the tables to map rgb to the nearest colormap index */
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);
    cmaptab = pixcmapToOctcubeLUT(cmap, level, metric);

    pixd = pixOctcubeQuantFromCmapLUT(pixs, cmap, mindepth,
                                      cmaptab, rtab, gtab, btab);

    LEPT_FREE(cmaptab);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return pixd;
}


/*!
 * \brief   pixOctcubeQuantFromCmapLUT()
 *
 * \param[in]    pixs              32 bpp rgb
 * \param[in]    cmap              to quantize to; insert copy into dest pix
 * \param[in]    mindepth          minimum depth of pixd: can be 2, 4 or 8 bpp
 * \param[in]    cmaptab           table mapping from octindex to colormap index
 * \param[in]    rtab, gtab, btab  tables mapping from RGB to octindex
 * \return  pixd  2, 4 or 8 bpp, colormapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See the notes in the higher-level function
 *          pixOctcubeQuantFromCmap().  The octcube level for
 *          the generated octree is specified there, along with
 *          the distance metric for determining the closest
 *          color in the colormap to each octcube.
 *      (2) If the colormap, level and metric information have already
 *          been used to construct the set of mapping tables,
 *          this low-level function can be used directly (i.e.,
 *          independently of pixOctcubeQuantFromCmap()) to build
 *          a colormapped pix that uses the specified colormap.
 * </pre>
 */
static PIX *
pixOctcubeQuantFromCmapLUT(PIX       *pixs,
                           PIXCMAP   *cmap,
                           l_int32    mindepth,
                           l_int32   *cmaptab,
                           l_uint32  *rtab,
                           l_uint32  *gtab,
                           l_uint32  *btab)
{
l_int32    i, j, w, h, depth, wpls, wpld;
l_int32    rval, gval, bval, index;
l_uint32   octindex;
l_uint32  *lines, *lined, *datas, *datad;
PIX       *pixd;
PIXCMAP   *cmapc;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (!cmap)
        return (PIX *)ERROR_PTR("cmap not defined", __func__, NULL);
    if (mindepth != 2 && mindepth != 4 && mindepth != 8)
        return (PIX *)ERROR_PTR("invalid mindepth", __func__, NULL);
    if (!rtab || !gtab || !btab || !cmaptab)
        return (PIX *)ERROR_PTR("tables not all defined", __func__, NULL);

        /* Init dest pix (with minimum bpp depending on cmap) */
    pixcmapGetMinDepth(cmap, &depth);
    depth = L_MAX(depth, mindepth);
    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, depth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    cmapc = pixcmapCopy(cmap);
    pixSetColormap(pixd, cmapc);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);

        /* Insert the colormap index of the color nearest to the input pixel */
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
                /* Map from rgb to octcube index */
            getOctcubeIndexFromRGB(rval, gval, bval, rtab, gtab, btab,
                                   &octindex);
                /* Map from octcube index to nearest colormap index */
            index = cmaptab[octindex];
            if (depth == 2)
                SET_DATA_DIBIT(lined, j, index);
            else if (depth == 4)
                SET_DATA_QBIT(lined, j, index);
            else  /* depth == 8 */
                SET_DATA_BYTE(lined, j, index);
        }
    }

    return pixd;
}


/*---------------------------------------------------------------------------*
 *                       Generation of octcube histogram                     *
 *---------------------------------------------------------------------------*/
/*!
 * \brief   pixOctcubeHistogram()
 *
 * \param[in]    pixs       32 bpp rgb
 * \param[in]    level      significant bits for each of RGB; valid in [1...6]
 * \param[out]   pncolors   [optional] number of occupied cubes
 * \return  numa histogram of color pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Input NULL for &ncolors to prevent computation and return value.
 * </pre>
 */
NUMA *
pixOctcubeHistogram(PIX      *pixs,
                    l_int32   level,
                    l_int32  *pncolors)
{
l_int32     size, i, j, w, h, wpl, ncolors, val;
l_int32     rval, gval, bval;
l_uint32    octindex;
l_uint32   *rtab, *gtab, *btab;
l_uint32   *data, *line;
l_float32  *array;
NUMA       *na;

    if (pncolors) *pncolors = 0;
    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (NUMA *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);

    if (octcubeGetCount(level, &size))  /* array size = 2 ** (3 * level) */
        return (NUMA *)ERROR_PTR("size not returned", __func__, NULL);
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);

    if ((na = numaCreate(size)) == NULL) {
        L_ERROR("na not made\n", __func__);
        goto cleanup_arrays;
    }
    numaSetCount(na, size);
    array = numaGetFArray(na, L_NOCOPY);

    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            extractRGBValues(line[j], &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
#if DEBUG_OCTINDEX
            if ((level == 1 && octindex > 7) ||
                (level == 2 && octindex > 63) ||
                (level == 3 && octindex > 511) ||
                (level == 4 && octindex > 4097) ||
                (level == 5 && octindex > 32783) ||
                (level == 6 && octindex > 262271)) {
                lept_stderr("level = %d, octindex = %d, index error!\n",
                            level, octindex);
                continue;
            }
#endif  /* DEBUG_OCTINDEX */
              array[octindex] += 1.0;
        }
    }

    if (pncolors) {
        for (i = 0, ncolors = 0; i < size; i++) {
            numaGetIValue(na, i, &val);
            if (val > 0)
                ncolors++;
        }
        *pncolors = ncolors;
    }

cleanup_arrays:
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return na;
}


/*------------------------------------------------------------------*
 *              Get filled octcube table from colormap              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixcmapToOctcubeLUT()
 *
 * \param[in]    cmap
 * \param[in]    level    significant bits for each of RGB; valid in [1...6]
 * \param[in]    metric   L_MANHATTAN_DISTANCE, L_EUCLIDEAN_DISTANCE
 * \return  tab[2**3 * level]
 *
 * <pre>
 * Notes:
 *      (1) This function is used to quickly find the colormap color
 *          that is closest to any rgb color.  It is used to assign
 *          rgb colors to an existing colormap.  It can be very expensive
 *          to search through the entire colormap for the closest color
 *          to each pixel.  Instead, we first set up this table, which is
 *          populated by the colormap index nearest to each octcube
 *          color.  Then we go through the image; for each pixel,
 *          do two table lookups: first to generate the octcube index
 *          from rgb and second to use this table to read out the
 *          colormap index.
 *      (2) Do a slight modification for white and black.  For level = 4,
 *          each octcube size is 16.  The center of the whitest octcube
 *          is at (248, 248, 248), which is closer to 242 than 255.
 *          Consequently, any gray color between 242 and 254 will
 *          be selected, even if white (255, 255, 255) exists.  This is
 *          typically not optimal, because the original color was
 *          likely white.  Therefore, if white exists in the colormap,
 *          use it for any rgb color that falls into the most white octcube.
 *          Do the similar thing for black.
 *      (3) Here are the actual function calls for quantizing to a
 *          specified colormap:
 *            ~ first make the tables that map from rgb --> octcube index
 *                     makeRGBToIndexTables()
 *            ~ then for each pixel:
 *                * use the tables to get the octcube index
 *                     getOctcubeIndexFromRGB()
 *                * use this table to get the nearest color in the colormap
 *                     cmap_index = tab[index]
 *      (4) Distance can be either manhattan or euclidean.
 *      (5) In typical use, level = 4 gives reasonable results, and
 *          level = 5 is slightly better.  When this function is used
 *          for color segmentation, there are typically a small number
 *          of colors and the number of levels can be small (e.g., level = 3).
 * </pre>
 */
l_int32 *
pixcmapToOctcubeLUT(PIXCMAP  *cmap,
                    l_int32   level,
                    l_int32   metric)
{
l_int32    i, k, size, ncolors, mindist, dist, mincolor, index;
l_int32    rval, gval, bval;  /* color at center of the octcube */
l_int32   *rmap, *gmap, *bmap, *tab;

    if (!cmap)
        return (l_int32 *)ERROR_PTR("cmap not defined", __func__, NULL);
    if (level < 1 || level > 6)
        return (l_int32 *)ERROR_PTR("level not in {1...6}", __func__, NULL);
    if (metric != L_MANHATTAN_DISTANCE && metric != L_EUCLIDEAN_DISTANCE)
        return (l_int32 *)ERROR_PTR("invalid metric", __func__, NULL);

    if (octcubeGetCount(level, &size))  /* array size = 2 ** (3 * level) */
        return (l_int32 *)ERROR_PTR("size not returned", __func__, NULL);
    if ((tab = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32))) == NULL)
        return (l_int32 *)ERROR_PTR("tab not allocated", __func__, NULL);

    ncolors = pixcmapGetCount(cmap);
    pixcmapToArrays(cmap, &rmap, &gmap, &bmap, NULL);

        /* Assign based on the closest octcube center to the cmap color */
    for (i = 0; i < size; i++) {
        getRGBFromOctcube(i, level, &rval, &gval, &bval);
        mindist = 1000000;
        mincolor = 0;  /* irrelevant init */
        for (k = 0; k < ncolors; k++) {
            if (metric == L_MANHATTAN_DISTANCE) {
                dist = L_ABS(rval - rmap[k]) + L_ABS(gval - gmap[k]) +
                       L_ABS(bval - bmap[k]);
            } else {  /* L_EUCLIDEAN_DISTANCE */
                dist = (rval - rmap[k]) * (rval - rmap[k]) +
                       (gval - gmap[k]) * (gval - gmap[k]) +
                       (bval - bmap[k]) * (bval - bmap[k]);
            }
            if (dist < mindist) {
                mindist = dist;
                mincolor = k;
            }
        }
        tab[i] = mincolor;
    }

        /* Reset black and white if available in the colormap.
         * The darkest octcube is at octindex 0.
         * The lightest octcube is at the max octindex. */
    pixcmapGetNearestIndex(cmap, 0, 0, 0, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval < 7 && gval < 7 && bval < 7) {
        tab[0] = index;
    }
    pixcmapGetNearestIndex(cmap, 255, 255, 255, &index);
    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
    if (rval > 248 && gval > 248 && bval > 248) {
        tab[(1 << (3 * level)) - 1] = index;
    }

    LEPT_FREE(rmap);
    LEPT_FREE(gmap);
    LEPT_FREE(bmap);
    return tab;
}


/*------------------------------------------------------------------*
 *               Strip out unused elements in colormap              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRemoveUnusedColors()
 *
 * \param[in]    pixs   colormapped
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) If the image doesn't have a colormap, returns without error.
 *      (3) Unusued colors are removed from the colormap, and the
 *          image pixels are re-numbered.
 * </pre>
 */
l_ok
pixRemoveUnusedColors(PIX  *pixs)
{
l_int32     i, j, w, h, d, nc, wpls, val, newval, index, zerofound;
l_int32     rval, gval, bval;
l_uint32   *datas, *lines;
l_int32    *histo, *map1, *map2;
PIXCMAP    *cmap, *cmapd;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return 0;

    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("d not in {2, 4, 8}", __func__, 1);

        /* Find which indices are actually used */
    nc = pixcmapGetCount(cmap);
    if ((histo = (l_int32 *)LEPT_CALLOC(nc, sizeof(l_int32))) == NULL)
        return ERROR_INT("histo not made", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j++) {
            switch (d)
            {
            case 2:
                val = GET_DATA_DIBIT(lines, j);
                break;
            case 4:
                val = GET_DATA_QBIT(lines, j);
                break;
            case 8:
                val = GET_DATA_BYTE(lines, j);
                break;
            default:
                LEPT_FREE(histo);
                return ERROR_INT("switch ran off end!", __func__, 1);
            }
            if (val >= nc) {
                L_WARNING("cmap index out of bounds!\n", __func__);
                continue;
            }
            histo[val]++;
        }
    }

        /* Check if there are any zeroes.  If none, quit. */
    zerofound = FALSE;
    for (i = 0; i < nc; i++) {
        if (histo[i] == 0) {
            zerofound = TRUE;
            break;
        }
    }
    if (!zerofound) {
      LEPT_FREE(histo);
      return 0;
    }

        /* Generate mapping tables between indices */
    map1 = (l_int32 *)LEPT_CALLOC(nc, sizeof(l_int32));
    map2 = (l_int32 *)LEPT_CALLOC(nc, sizeof(l_int32));
    index = 0;
    for (i = 0; i < nc; i++) {
        if (histo[i] != 0) {
            map1[index] = i;  /* get old index from new */
            map2[i] = index;  /* get new index from old */
            index++;
        }
    }

        /* Generate new colormap and attach to pixs */
    cmapd = pixcmapCreate(d);
    for (i = 0; i < index; i++) {
        pixcmapGetColor(cmap, map1[i], &rval, &gval, &bval);
        pixcmapAddColor(cmapd, rval, gval, bval);
    }
    pixSetColormap(pixs, cmapd);

        /* Map pixel (index) values to new cmap */
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < w; j++) {
            switch (d)
            {
            case 2:
                val = GET_DATA_DIBIT(lines, j);
                newval = map2[val];
                SET_DATA_DIBIT(lines, j, newval);
                break;
            case 4:
                val = GET_DATA_QBIT(lines, j);
                newval = map2[val];
                SET_DATA_QBIT(lines, j, newval);
                break;
            case 8:
                val = GET_DATA_BYTE(lines, j);
                newval = map2[val];
                SET_DATA_BYTE(lines, j, newval);
                break;
            default:
                LEPT_FREE(histo);
                LEPT_FREE(map1);
                LEPT_FREE(map2);
                return ERROR_INT("switch ran off end!", __func__, 1);
            }
        }
    }

    LEPT_FREE(histo);
    LEPT_FREE(map1);
    LEPT_FREE(map2);
    return 0;
}


/*------------------------------------------------------------------*
 *      Find number of occupied octcubes at the specified level     *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixNumberOccupiedOctcubes()
 *
 * \param[in]    pix        32 bpp
 * \param[in]    level      of octcube
 * \param[in]    mincount   minimum num pixels in an octcube to be counted;
 *                          -1 to not use
 * \param[in]    minfract   minimum fract of pixels in an octcube to be
 *                          counted; -1 to not use
 * \param[out]   pncolors   number of occupied octcubes
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Exactly one of (%mincount, %minfract) must be -1, so, e.g.,
 *          if %mincount == -1, then we use %minfract.
 *      (2) If all occupied octcubes are to count, set %mincount == 1.
 *          Setting %minfract == 0.0 is taken to mean the same thing.
 * </pre>
 */
l_ok
pixNumberOccupiedOctcubes(PIX       *pix,
                          l_int32    level,
                          l_int32    mincount,
                          l_float32  minfract,
                          l_int32   *pncolors)
{
l_int32    i, j, w, h, d, wpl, ncolors, size, octindex;
l_int32    rval, gval, bval;
l_int32   *carray;
l_uint32  *data, *line, *rtab, *gtab, *btab;

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 32)
        return ERROR_INT("pix not 32 bpp", __func__, 1);
    if (level < 1 || level > 6)
        return ERROR_INT("invalid level", __func__, 1);
    if ((mincount < 0 && minfract < 0) || (mincount >= 0.0 && minfract >= 0.0))
        return ERROR_INT("invalid mincount/minfract", __func__, 1);
    if (mincount == 0 || minfract == 0.0)
        mincount = 1;
    else if (minfract > 0.0)
        mincount = L_MIN(1, (l_int32)(minfract * w * h));

    if (octcubeGetCount(level, &size))  /* array size = 2 ** (3 * level) */
        return ERROR_INT("size not returned", __func__, 1);
    rtab = gtab = btab = NULL;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);
    if ((carray = (l_int32 *)LEPT_CALLOC(size, sizeof(l_int32))) == NULL) {
        L_ERROR("carray not made\n", __func__);
        goto cleanup_arrays;
    }

        /* Mark the occupied octcube leaves */
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            extractRGBValues(line[j], &rval, &gval, &bval);
            octindex = rtab[rval] | gtab[gval] | btab[bval];
            carray[octindex]++;
        }
    }

        /* Count them */
    for (i = 0, ncolors = 0; i < size; i++) {
        if (carray[i] >= mincount)
            ncolors++;
    }
    *pncolors = ncolors;

cleanup_arrays:
    LEPT_FREE(carray);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return 0;
}
