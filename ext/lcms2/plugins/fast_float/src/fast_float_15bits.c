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


//---------------------------------------------------------------------------------

//  The internal photoshop 16 bit format range is 1.15 fixed point, which goes 0..32768
// (NOT 32767) that means:
//
//         16 bits encoding            15 bit Photoshop encoding
//         ================            =========================
//
//              0x0000                       0x0000
//              0xFFFF                       0x8000
//
//  A nice (and fast) way to implement conversions is by using 64 bit values, which are
// native CPU word size in most today architectures.
// In CMYK, internal Photoshop format comes inverted, and this inversion happens after
// the resizing, so values 32769 to 65535 are never used in PhotoShop.

//---------------------------------------------------------------------------------

// This macro converts 16 bits to 15 bits by using a 64 bits value
cmsINLINE cmsUInt16Number From16To15(cmsUInt16Number x16)
{
       cmsUInt64Number r64 = (((cmsUInt64Number)x16 << 15)) / 0xFFFFL;
       return (cmsUInt16Number)r64;
}

// This macro converts 15 bits to 16 bits by using a 64 bit value. It is based in fixed 1.15 math
cmsINLINE cmsUInt16Number From15To16(cmsUInt16Number x15)
{
       cmsUInt64Number r64 = ((cmsUInt64Number) x15 * 0xFFFF + 0x4000L) >> 15;
       return (cmsUInt16Number)r64;
}

// Specialized 1-channel formatters
static
cmsUInt8Number* Unroll15bitsGray(cmsContext ContextID,
                                 CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                 CMSREGISTER cmsUInt16Number Values[],
                                 CMSREGISTER cmsUInt8Number*  Buffer,
                                 CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);
       UNUSED_PARAMETER(ContextID);

       Values[0] = From15To16(*(cmsUInt16Number*)Buffer);

       return Buffer + 2;
}


static
cmsUInt8Number* Pack15bitsGray(cmsContext ContextID,
                               CMSREGISTER struct _cmstransform_struct* CMMcargo,
                               CMSREGISTER cmsUInt16Number Values[],
                               CMSREGISTER cmsUInt8Number*  Buffer,
                               CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       *(cmsUInt16Number*)Buffer = From16To15(Values[0]);
       return Buffer + 2;
}

// Specialized 3-channels formatters
static
cmsUInt8Number* Unroll15bitsRGB(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       Values[0] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[1] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[2] = From15To16(*(cmsUInt16Number*)Buffer);

       return Buffer + 2;
}


static
cmsUInt8Number* Pack15bitsRGB(cmsContext ContextID,
                               CMSREGISTER struct _cmstransform_struct* CMMcargo,
                               CMSREGISTER cmsUInt16Number Values[],
                               CMSREGISTER cmsUInt8Number*  Buffer,
                               CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       *(cmsUInt16Number*)Buffer = From16To15(Values[0]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[1]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[2]);

       return Buffer + 2;
}


static
cmsUInt8Number* Unroll15bitsRGBA(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       Values[0] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[1] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[2] = From15To16(*(cmsUInt16Number*)Buffer);

       return Buffer + 4;
}


static
cmsUInt8Number* Pack15bitsRGBA(cmsContext ContextID,
                               CMSREGISTER struct _cmstransform_struct* CMMcargo,
                               CMSREGISTER cmsUInt16Number Values[],
                               CMSREGISTER cmsUInt8Number*  Buffer,
                               CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       *(cmsUInt16Number*)Buffer = From16To15(Values[0]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[1]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[2]);

       return Buffer + 4;
}


// Specialized 3 channels reversed formatters
static
cmsUInt8Number* Unroll15bitsBGR(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       Values[2] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[1] = From15To16(*(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[0] = From15To16(*(cmsUInt16Number*)Buffer);

       return Buffer + 2;
}


static
cmsUInt8Number* Pack15bitsBGR(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       *(cmsUInt16Number*)Buffer = From16To15(Values[2]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[1]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = From16To15(Values[0]);

       return Buffer+2;
}

// Specialized 4 channels CMYK formatters. Note Photoshop stores CMYK reversed
static
cmsUInt8Number* Unroll15bitsCMYK(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       Values[0] = From15To16(0x8000 - *(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[1] = From15To16(0x8000 - *(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[2] = From15To16(0x8000 - *(cmsUInt16Number*)Buffer);
       Buffer += 2;
       Values[3] = From15To16(0x8000 - *(cmsUInt16Number*)Buffer);

       return Buffer + 2;
}

static
cmsUInt8Number* Pack15bitsCMYK(cmsContext ContextID,
                               CMSREGISTER struct _cmstransform_struct* CMMcargo,
                               CMSREGISTER cmsUInt16Number Values[],
                               CMSREGISTER cmsUInt8Number*  Buffer,
                               CMSREGISTER cmsUInt32Number  Stride)
{
       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(CMMcargo);
       UNUSED_PARAMETER(Stride);

       *(cmsUInt16Number*)Buffer = 0x8000U - From16To15(Values[0]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = 0x8000U - From16To15(Values[1]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = 0x8000U - From16To15(Values[2]);
       Buffer += 2;
       *(cmsUInt16Number*)Buffer = 0x8000U - From16To15(Values[3]);

       return Buffer + 2;
}


// This macros does all handling for fallthrough cases
cmsINLINE cmsUInt16Number UnrollOne(cmsUInt16Number x, cmsBool Reverse, cmsBool SwapEndian)
{
       if (SwapEndian)
              x = (x << 8) | (x >> 8);

       if (Reverse)
              x = 0xffff - x;

       return From15To16(x);
}

cmsINLINE cmsUInt16Number PackOne(cmsUInt16Number x, cmsBool Reverse, cmsBool SwapEndian)
{
       x = From16To15(x);

       if (Reverse)
              x = 0xffff - x;

       if (SwapEndian)
              x = (x << 8) | (x >> 8);

       return x;
}

// Generic planar support
static
cmsUInt8Number* Unroll15bitsPlanar(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number wIn[],
                                   CMSREGISTER cmsUInt8Number* accum,
                                   CMSREGISTER cmsUInt32Number Stride)
{
       _xform_head* head = (_xform_head*) CMMcargo;
       int nChan      = T_CHANNELS(head->InputFormat);
       int DoSwap     = T_DOSWAP(head->InputFormat);
       int Reverse    = T_FLAVOR(head->InputFormat);
       int SwapEndian = T_ENDIAN16(head->InputFormat);
       int i;
       cmsUInt8Number* Init = accum;

       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(Stride);

       if (DoSwap) {
              accum += T_EXTRA(head->InputFormat) * Stride * 2;
       }

       for (i = 0; i < nChan; i++) {

              int index = DoSwap ? (nChan - i - 1) : i;

              wIn[index] = UnrollOne(*(cmsUInt16Number*)accum, Reverse, SwapEndian);

              accum += Stride * 2;
       }

       return (Init + 2);
}


static
cmsUInt8Number* Pack15bitsPlanar(cmsContext ContextID,
                                 CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                 CMSREGISTER cmsUInt16Number wOut[],
                                 CMSREGISTER cmsUInt8Number* output,
                                 CMSREGISTER cmsUInt32Number Stride)
{
       _xform_head* head = (_xform_head*)CMMcargo;
       int nChan = T_CHANNELS(head->OutputFormat);
       int DoSwap = T_DOSWAP(head->OutputFormat);
       int Reverse = T_FLAVOR(head->OutputFormat);
       int SwapEndian = T_ENDIAN16(head->OutputFormat);
       CMSREGISTER int i;
       cmsUInt8Number* Init = output;

       UNUSED_PARAMETER(ContextID);

       if (DoSwap) {
              output += T_EXTRA(head->OutputFormat) * Stride * 2;
       }

       for (i = 0; i < nChan; i++) {

              int index = DoSwap ? (nChan - i - 1) : i;

              *(cmsUInt16Number*)output = PackOne(wOut[index], Reverse, SwapEndian);
              output += (Stride * sizeof(cmsUInt16Number));
       }

       return (Init + sizeof(cmsUInt16Number));
}



// Generic falltrough
static
cmsUInt8Number* Unroll15bitsChunky(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       _xform_head* head = (_xform_head*) CMMcargo;

       int nChan = T_CHANNELS(head->InputFormat);
       int DoSwap = T_DOSWAP(head->InputFormat);
       int Reverse = T_FLAVOR(head->InputFormat);
       int SwapEndian = T_ENDIAN16(head->InputFormat);

	CMSREGISTER int i;

       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(Stride);

       if (DoSwap) {
              Buffer += T_EXTRA(head->OutputFormat) * 2;
       }

	for (i = 0; i < nChan; i++) {

              int index = DoSwap ? (nChan - i - 1) : i;

              Values[index] = UnrollOne(*(cmsUInt16Number*)Buffer, Reverse, SwapEndian);

              Buffer += 2;
	}


       return Buffer;
}


static
cmsUInt8Number* Pack15bitsChunky(cmsContext ContextID,
                           CMSREGISTER struct _cmstransform_struct* CMMcargo,
                           CMSREGISTER cmsUInt16Number Values[],
                           CMSREGISTER cmsUInt8Number*  Buffer,
                           CMSREGISTER cmsUInt32Number  Stride)
{
       _xform_head* head = (_xform_head*)CMMcargo;

       int nChan = T_CHANNELS(head->OutputFormat);
       int DoSwap = T_DOSWAP(head->OutputFormat);
       int Reverse = T_FLAVOR(head->OutputFormat);
       int SwapEndian = T_ENDIAN16(head->OutputFormat);

       CMSREGISTER int i;

       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(Stride);

       if (DoSwap) {
              Buffer += T_EXTRA(head->OutputFormat) * 2;
       }

       for (i = 0; i < nChan; i++) {

              int index = DoSwap ? (nChan - i - 1) : i;

              *(cmsUInt16Number*)Buffer = PackOne(Values[index], Reverse, SwapEndian);

              Buffer += 2;
       }

       return Buffer;
}



// Generic N-bytes plus dither 16-to-8 conversion.
static int err[cmsMAXCHANNELS];

static
cmsUInt8Number*  PackNBytesDither(cmsContext ContextID,
                                   CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                   CMSREGISTER cmsUInt16Number Values[],
                                   CMSREGISTER cmsUInt8Number*  Buffer,
                                   CMSREGISTER cmsUInt32Number  Stride)
{
       _xform_head* info = (_xform_head*)CMMcargo;

       int nChan = T_CHANNELS(info->OutputFormat);
       CMSREGISTER int i;
       unsigned int n, pe, pf;

       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(Stride);

       for (i = 0; i < nChan; i++) {

              n = Values[i] + err[i]; // Value

              pe = (n / 257);       // Whole part
              pf = (n % 257);       // Fractional part

              err[i] = pf;          // Store it for next pixel

              *Buffer++ = (cmsUInt8Number) pe;
       }

       return Buffer + T_EXTRA(info->OutputFormat);
}


static
cmsUInt8Number*  PackNBytesSwapDither(cmsContext ContextID,
                                          CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                          CMSREGISTER cmsUInt16Number Values[],
                                          CMSREGISTER cmsUInt8Number*  Buffer,
                                          CMSREGISTER cmsUInt32Number  Stride)
{
       _xform_head* info = (_xform_head*)CMMcargo;

       int nChan = T_CHANNELS(info->OutputFormat);
       CMSREGISTER int i;
       unsigned int n, pe, pf;

       UNUSED_PARAMETER(ContextID);
       UNUSED_PARAMETER(Stride);

       for (i = nChan - 1; i >= 0; --i) {

              n = Values[i] + err[i];   // Value

              pe = (n / 257);           // Whole part
              pf = (n % 257);           // Fractional part

              err[i] = pf;              // Store it for next pixel

              *Buffer++ = (cmsUInt8Number)pe;
       }


       return Buffer + T_EXTRA(info->OutputFormat);
}


// The factory for 15 bits. This function returns a pointer to specialized function
// that would deal with the asked format. It return a pointer to NULL if the format
// is not supported. This is tha basis of formatter plug-in for 15 bit formats.
CMSCHECKPOINT cmsFormatter CMSEXPORT Formatter_15Bit_Factory(cmsContext ContextID,
                                                             cmsUInt32Number Type,
                                                             cmsFormatterDirection Dir,
                                                             cmsUInt32Number dwFlags)
{
       cmsFormatter Result = { NULL };

       UNUSED_PARAMETER(ContextID);
	   UNUSED_PARAMETER(dwFlags);

       switch (Type) {

       // Simple Gray
       case TYPE_GRAY_15:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsGray : Pack15bitsGray;
              break;

       // 3 channels
       case TYPE_CMY_15:
       case TYPE_RGB_15:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsRGB : Pack15bitsRGB;
              break;

       // 3 channels reversed
       case TYPE_YMC_15:
       case TYPE_BGR_15:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsBGR : Pack15bitsBGR;
              break;

       // 3 Channels plus one alpha
       case TYPE_RGBA_15:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsRGBA : Pack15bitsRGBA;
              break;

       // 4 channels
       case TYPE_CMYK_15:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsCMYK : Pack15bitsCMYK;
              break;

       // Planar versions
       case TYPE_GRAYA_15_PLANAR:
       case TYPE_RGB_15_PLANAR:
       case TYPE_BGR_15_PLANAR:
       case TYPE_RGBA_15_PLANAR:
       case TYPE_ABGR_15_PLANAR:
       case TYPE_CMY_15_PLANAR:
       case TYPE_CMYK_15_PLANAR:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsPlanar : Pack15bitsPlanar;
              break;

       // Falltrough for remaining (corner) cases
       case TYPE_GRAY_15_REV:
       case TYPE_GRAY_15_SE:
       case TYPE_GRAYA_15:
       case TYPE_GRAYA_15_SE:
       case TYPE_RGB_15_SE:
       case TYPE_BGR_15_SE:
       case TYPE_RGBA_15_SE:
       case TYPE_ARGB_15:
       case TYPE_ABGR_15:
       case TYPE_ABGR_15_SE:
       case TYPE_BGRA_15:
       case TYPE_BGRA_15_SE:
       case TYPE_CMY_15_SE:
       case TYPE_CMYK_15_REV:
       case TYPE_CMYK_15_SE:
       case TYPE_KYMC_15:
       case TYPE_KYMC_15_SE:
       case TYPE_KCMY_15:
       case TYPE_KCMY_15_REV:
       case TYPE_KCMY_15_SE:
              Result.Fmt16 = (Dir == cmsFormatterInput) ? Unroll15bitsChunky : Pack15bitsChunky;
              break;

       case TYPE_GRAY_8_DITHER:
       case TYPE_RGB_8_DITHER:
       case TYPE_RGBA_8_DITHER:
       case TYPE_CMYK_8_DITHER:
              if (Dir == cmsFormatterOutput) {
                     Result.Fmt16 = PackNBytesDither;
              }
              break;

       case TYPE_ABGR_8_DITHER:
       case TYPE_BGR_8_DITHER:
       case TYPE_KYMC_8_DITHER:
              if (Dir == cmsFormatterOutput) {
                     Result.Fmt16 = PackNBytesSwapDither;
              }
              break;

       default:;
       }

       return Result;
}
