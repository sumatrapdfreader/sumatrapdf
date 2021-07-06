//---------------------------------------------------------------------------------
//
//  Little Color Management System, fast floating point extensions
//  Copyright (c) 1998-2020 Marti Maria Saguer, all rights reserved
//
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------------

#include "fast_float_internal.h"

// Separable input. It just computes the distance from 
// each component to the next one in bytes. It gives components RGB in this order
// 
// Encoding  Starting      Increment   DoSwap   Swapfirst Extra 
// RGB,       012            333          0         0       0   
// RGBA,      012            444          0         0       1   
// ARGB,      123            444          0         1       1   
// BGR,       210            333          1         0       0   
// BGRA,      210            444          1         1       1   
// ABGR       321            444          1         0       1   
//
//
//  On planar configurations, the distance is the stride added to any non-negative
//
//  RGB       0, S, 2*S      111
//  RGBA      0, S, 2*S      111    (fourth plane is safely ignored)
//  ARGB      S, 2*S, 3*S    111
//  BGR       2*S, S, 0      111
//  BGRA      2*S, S, 0,     111    (fourth plane is safely ignored)
//  ABGR      3*S, 2*S, S    111
//
//----------------------------------------------------------------------------------------


// Return the size in bytes of a given formatter
static
int trueBytesSize(cmsUInt32Number Format)
{
       int fmt_bytes = T_BYTES(Format);

       // For double, the T_BYTES field returns zero
       if (fmt_bytes == 0)
              return sizeof(double);

       // Otherwise, it is already correct for all formats
       return fmt_bytes;
}

// RGBA -> normal
// ARGB -> swap first
// ABGR -> doSwap
// BGRA -> doSwap swapFirst

// This function computes the distance from each component to the next one in bytes. 
static
void ComputeIncrementsForChunky(cmsUInt32Number Format, 
                                cmsUInt32Number BytesPerPlane,
                                cmsUInt32Number* nChannels,
                                cmsUInt32Number* nAlpha,
                                cmsUInt32Number ComponentStartingOrder[], 
                                cmsUInt32Number ComponentPointerIncrements[])
{
       int extra = T_EXTRA(Format);
       int channels = T_CHANNELS(Format);
       int total_chans = channels + extra;
       int i;       
       int channelSize = trueBytesSize(Format);
       int pixelSize = channelSize * total_chans;
       
       UNUSED_PARAMETER(BytesPerPlane);

       // Setup the counts
       if (nChannels != NULL)
              *nChannels = channels;

       if (nAlpha != NULL)
              *nAlpha = extra;

       // Separation is independent of starting point and only depends on channel size
       for (i = 0; i < total_chans; i++)
              ComponentPointerIncrements[i] = pixelSize;

       // Handle do swap
       for (i = 0; i < total_chans; i++)
       {
              if (T_DOSWAP(Format)) {
                     ComponentStartingOrder[i] = total_chans - i - 1;
              }
              else {
                     ComponentStartingOrder[i] = i;
              }
       }

       // Handle swap first (ROL of positions), example CMYK -> KCMY | 0123 -> 3012
       if (T_SWAPFIRST(Format)) {
              
              cmsUInt32Number tmp = ComponentStartingOrder[0];
              for (i = 0; i < total_chans-1; i++)
                     ComponentStartingOrder[i] = ComponentStartingOrder[i + 1];

              ComponentStartingOrder[total_chans - 1] = tmp;
       }

       // Handle size
       if (channelSize > 1)
              for (i = 0; i < total_chans; i++) {
                     ComponentStartingOrder[i] *= channelSize;
              }
}



//  On planar configurations, the distance is the stride added to any non-negative
static
void ComputeIncrementsForPlanar(cmsUInt32Number Format, 
                                cmsUInt32Number BytesPerPlane,
                                cmsUInt32Number* nChannels,
                                cmsUInt32Number* nAlpha,
                                cmsUInt32Number ComponentStartingOrder[], 
                                cmsUInt32Number ComponentPointerIncrements[])
{
       int extra = T_EXTRA(Format);
       int channels = T_CHANNELS(Format);
       int total_chans = channels + extra;
       int i;
       int channelSize = trueBytesSize(Format);
       
       // Setup the counts
       if (nChannels != NULL) 
              *nChannels = channels;

       if (nAlpha != NULL) 
              *nAlpha = extra;

       // Separation is independent of starting point and only depends on channel size
       for (i = 0; i < total_chans; i++)
              ComponentPointerIncrements[i] = channelSize;

       // Handle do swap
       for (i = 0; i < total_chans; i++)
       {
              if (T_DOSWAP(Format)) {
                     ComponentStartingOrder[i] = total_chans - i - 1;
              }
              else {
                     ComponentStartingOrder[i] = i;
              }
       }

       // Handle swap first (ROL of positions), example CMYK -> KCMY | 0123 -> 3012
       if (T_SWAPFIRST(Format)) {

              cmsUInt32Number tmp = ComponentStartingOrder[0];
              for (i = 0; i < total_chans - 1; i++)
                     ComponentStartingOrder[i] = ComponentStartingOrder[i + 1];

              ComponentStartingOrder[total_chans - 1] = tmp;
       }

       // Handle size
       for (i = 0; i < total_chans; i++) {
              ComponentStartingOrder[i] *= BytesPerPlane;
       }
}



// Dispatcher por chunky and planar RGB
CMSCHECKPOINT void  CMSEXPORT _cmsComputeComponentIncrements(cmsUInt32Number Format,
                                     cmsUInt32Number BytesPerPlane,
                                     cmsUInt32Number* nChannels,
                                     cmsUInt32Number* nAlpha,
                                     cmsUInt32Number ComponentStartingOrder[], 
                                     cmsUInt32Number ComponentPointerIncrements[])
{
       if (T_PLANAR(Format)) {

              ComputeIncrementsForPlanar(Format,  BytesPerPlane, nChannels, nAlpha, ComponentStartingOrder, ComponentPointerIncrements);
       }
       else {
              ComputeIncrementsForChunky(Format,  BytesPerPlane, nChannels, nAlpha, ComponentStartingOrder, ComponentPointerIncrements);
       }

}


