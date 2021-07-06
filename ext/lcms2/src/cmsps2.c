//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2020 Marti Maria Saguer
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//---------------------------------------------------------------------------------
//

#include "lcms2_internal.h"

// PostScript ColorRenderingDictionary and ColorSpaceArray


#define MAXPSCOLS   60      // Columns on tables

/*
    Implementation
    --------------

  PostScript does use XYZ as its internal PCS. But since PostScript
  interpolation tables are limited to 8 bits, I use Lab as a way to
  improve the accuracy, favoring perceptual results. So, for the creation
  of each CRD, CSA the profiles are converted to Lab via a device
  link between  profile -> Lab or Lab -> profile. The PS code necessary to
  convert Lab <-> XYZ is also included.



  Color Space Arrays (CSA)
  ==================================================================================

  In order to obtain precision, code chooses between three ways to implement
  the device -> XYZ transform. These cases identifies monochrome profiles (often
  implemented as a set of curves), matrix-shaper and Pipeline-based.

  Monochrome
  -----------

  This is implemented as /CIEBasedA CSA. The prelinearization curve is
  placed into /DecodeA section, and matrix equals to D50. Since here is
  no interpolation tables, I do the conversion directly to XYZ

  NOTE: CLUT-based monochrome profiles are NOT supported. So, cmsFLAGS_MATRIXINPUT
  flag is forced on such profiles.

    [ /CIEBasedA
      <<
            /DecodeA { transfer function } bind
            /MatrixA [D50]
            /RangeLMN [ 0.0 cmsD50X 0.0 cmsD50Y 0.0 cmsD50Z ]
            /WhitePoint [D50]
            /BlackPoint [BP]
            /RenderingIntent (intent)
      >>
    ]

   On simpler profiles, the PCS is already XYZ, so no conversion is required.


   Matrix-shaper based
   -------------------

   This is implemented both with /CIEBasedABC or /CIEBasedDEF depending on the
   profile implementation. Since here there are no interpolation tables, I do
   the conversion directly to XYZ



    [ /CIEBasedABC
            <<
                /DecodeABC [ {transfer1} {transfer2} {transfer3} ]
                /MatrixABC [Matrix]
                /RangeLMN [ 0.0 cmsD50X 0.0 cmsD50Y 0.0 cmsD50Z ]
                /DecodeLMN [ { / 2} dup dup ]
                /WhitePoint [D50]
                /BlackPoint [BP]
                /RenderingIntent (intent)
            >>
    ]


    CLUT based
    ----------

     Lab is used in such cases.

    [ /CIEBasedDEF
            <<
            /DecodeDEF [ <prelinearization> ]
            /Table [ p p p [<...>]]
            /RangeABC [ 0 1 0 1 0 1]
            /DecodeABC[ <postlinearization> ]
            /RangeLMN [ -0.236 1.254 0 1 -0.635 1.640 ]
               % -128/500 1+127/500 0 1  -127/200 1+128/200
            /MatrixABC [ 1 1 1 1 0 0 0 0 -1]
            /WhitePoint [D50]
            /BlackPoint [BP]
            /RenderingIntent (intent)
    ]


  Color Rendering Dictionaries (CRD)
  ==================================
  These are always implemented as CLUT, and always are using Lab. Since CRD are expected to
  be used as resources, the code adds the definition as well.

  <<
    /ColorRenderingType 1
    /WhitePoint [ D50 ]
    /BlackPoint [BP]
    /MatrixPQR [ Bradford ]
    /RangePQR [-0.125 1.375 -0.125 1.375 -0.125 1.375 ]
    /TransformPQR [
    {4 index 3 get div 2 index 3 get mul exch pop exch pop exch pop exch pop } bind
    {4 index 4 get div 2 index 4 get mul exch pop exch pop exch pop exch pop } bind
    {4 index 5 get div 2 index 5 get mul exch pop exch pop exch pop exch pop } bind
    ]
    /MatrixABC <...>
    /EncodeABC <...>
    /RangeABC  <.. used for  XYZ -> Lab>
    /EncodeLMN
    /RenderTable [ p p p [<...>]]

    /RenderingIntent (Perceptual)
  >>
  /Current exch /ColorRendering defineresource pop


  The following stages are used to convert from XYZ to Lab
  --------------------------------------------------------

  Input is given at LMN stage on X, Y, Z

  Encode LMN gives us f(X/Xn), f(Y/Yn), f(Z/Zn)

  /EncodeLMN [

    { 0.964200  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind
    { 1.000000  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind
    { 0.824900  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind

    ]


  MatrixABC is used to compute f(Y/Yn), f(X/Xn) - f(Y/Yn), f(Y/Yn) - f(Z/Zn)

  | 0  1  0|
  | 1 -1  0|
  | 0  1 -1|

  /MatrixABC [ 0 1 0 1 -1 1 0 0 -1 ]

 EncodeABC finally gives Lab values.

  /EncodeABC [
    { 116 mul  16 sub 100 div  } bind
    { 500 mul 128 add 255 div  } bind
    { 200 mul 128 add 255 div  } bind
    ]

  The following stages are used to convert Lab to XYZ
  ----------------------------------------------------

    /RangeABC [ 0 1 0 1 0 1]
    /DecodeABC [ { 100 mul 16 add 116 div } bind
                 { 255 mul 128 sub 500 div } bind
                 { 255 mul 128 sub 200 div } bind
               ]

    /MatrixABC [ 1 1 1 1 0 0 0 0 -1]
    /DecodeLMN [
                {dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse 0.964200 mul} bind
                {dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse } bind
                {dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse 0.824900 mul} bind
                ]


*/

/*

 PostScript algorithms discussion.
 =========================================================================================================

  1D interpolation algorithm


  1D interpolation (float)
  ------------------------

    val2 = Domain * Value;

    cell0 = (int) floor(val2);
    cell1 = (int) ceil(val2);

    rest = val2 - cell0;

    y0 = LutTable[cell0] ;
    y1 = LutTable[cell1] ;

    y = y0 + (y1 - y0) * rest;



  PostScript code                   Stack
  ================================================

  {                                 % v
    <check 0..1.0>
    [array]                         % v tab
    dup                             % v tab tab
    length 1 sub                    % v tab dom

    3 -1 roll                       % tab dom v

    mul                             % tab val2
    dup                             % tab val2 val2
    dup                             % tab val2 val2 val2
    floor cvi                       % tab val2 val2 cell0
    exch                            % tab val2 cell0 val2
    ceiling cvi                     % tab val2 cell0 cell1

    3 index                         % tab val2 cell0 cell1 tab
    exch                            % tab val2 cell0 tab cell1
    get                             % tab val2 cell0 y1

    4 -1 roll                       % val2 cell0 y1 tab
    3 -1 roll                       % val2 y1 tab cell0
    get                             % val2 y1 y0

    dup                             % val2 y1 y0 y0
    3 1 roll                        % val2 y0 y1 y0

    sub                             % val2 y0 (y1-y0)
    3 -1 roll                       % y0 (y1-y0) val2
    dup                             % y0 (y1-y0) val2 val2
    floor cvi                       % y0 (y1-y0) val2 floor(val2)
    sub                             % y0 (y1-y0) rest
    mul                             % y0 t1
    add                             % y
    65535 div                       % result

  } bind


*/


// This struct holds the memory block currently being write
typedef struct {
    _cmsStageCLutData* Pipeline;
    cmsIOHANDLER* m;

    int FirstComponent;
    int SecondComponent;

    const char* PreMaj;
    const char* PostMaj;
    const char* PreMin;
    const char* PostMin;

    int  FixWhite;    // Force mapping of pure white

    cmsColorSpaceSignature  ColorSpace;  // ColorSpace of profile


} cmsPsSamplerCargo;

static int _cmsPSActualColumn = 0;


// Convert to byte
static
cmsUInt8Number Word2Byte(cmsUInt16Number w)
{
    return (cmsUInt8Number) floor((cmsFloat64Number) w / 257.0 + 0.5);
}


// Write a cooked byte
static
void WriteByte(cmsContext ContextID, cmsIOHANDLER* m, cmsUInt8Number b)
{
    _cmsIOPrintf(ContextID, m, "%02x", b);
    _cmsPSActualColumn += 2;

    if (_cmsPSActualColumn > MAXPSCOLS) {

        _cmsIOPrintf(ContextID, m, "\n");
        _cmsPSActualColumn = 0;
    }
}

// ----------------------------------------------------------------- PostScript generation


// Removes offending carriage returns

static
char* RemoveCR(const char* txt)
{
    static char Buffer[2048];
    char* pt;

    strncpy(Buffer, txt, 2047);
    Buffer[2047] = 0;
    for (pt = Buffer; *pt; pt++)
            if (*pt == '\n' || *pt == '\r') *pt = ' ';

    return Buffer;

}

static
void EmitHeader(cmsContext ContextID, cmsIOHANDLER* m, const char* Title, cmsHPROFILE hProfile)
{
    time_t timer;
    cmsMLU *Description, *Copyright;
    char DescASCII[256], CopyrightASCII[256];

    time(&timer);

    Description = (cmsMLU*) cmsReadTag(ContextID, hProfile, cmsSigProfileDescriptionTag);
    Copyright   = (cmsMLU*) cmsReadTag(ContextID, hProfile, cmsSigCopyrightTag);

    DescASCII[0] = DescASCII[255] = 0;
    CopyrightASCII[0] = CopyrightASCII[255] = 0;

    if (Description != NULL) cmsMLUgetASCII(ContextID, Description,  cmsNoLanguage, cmsNoCountry, DescASCII,       255);
    if (Copyright != NULL)   cmsMLUgetASCII(ContextID, Copyright,    cmsNoLanguage, cmsNoCountry, CopyrightASCII,  255);

    _cmsIOPrintf(ContextID, m, "%%!PS-Adobe-3.0\n");
    _cmsIOPrintf(ContextID, m, "%%\n");
    _cmsIOPrintf(ContextID, m, "%% %s\n", Title);
    _cmsIOPrintf(ContextID, m, "%% Source: %s\n", RemoveCR(DescASCII));
    _cmsIOPrintf(ContextID, m, "%%         %s\n", RemoveCR(CopyrightASCII));
    _cmsIOPrintf(ContextID, m, "%% Created: %s", ctime(&timer)); // ctime appends a \n!!!
    _cmsIOPrintf(ContextID, m, "%%\n");
    _cmsIOPrintf(ContextID, m, "%%%%BeginResource\n");

}


// Emits White & Black point. White point is always D50, Black point is the device
// Black point adapted to D50.

static
void EmitWhiteBlackD50(cmsContext ContextID, cmsIOHANDLER* m, cmsCIEXYZ* BlackPoint)
{

    _cmsIOPrintf(ContextID, m, "/BlackPoint [%f %f %f]\n", BlackPoint -> X,
                                          BlackPoint -> Y,
                                          BlackPoint -> Z);

    _cmsIOPrintf(ContextID, m, "/WhitePoint [%f %f %f]\n", cmsD50_XYZ(ContextID)->X,
                                          cmsD50_XYZ(ContextID)->Y,
                                          cmsD50_XYZ(ContextID)->Z);
}


static
void EmitRangeCheck(cmsContext ContextID, cmsIOHANDLER* m)
{
    _cmsIOPrintf(ContextID, m, "dup 0.0 lt { pop 0.0 } if "
                    "dup 1.0 gt { pop 1.0 } if ");

}

// Does write the intent

static
void EmitIntent(cmsContext ContextID, cmsIOHANDLER* m, cmsUInt32Number RenderingIntent)
{
    const char *intent;

    switch (RenderingIntent) {

        case INTENT_PERCEPTUAL:            intent = "Perceptual"; break;
        case INTENT_RELATIVE_COLORIMETRIC: intent = "RelativeColorimetric"; break;
        case INTENT_ABSOLUTE_COLORIMETRIC: intent = "AbsoluteColorimetric"; break;
        case INTENT_SATURATION:            intent = "Saturation"; break;

        default: intent = "Undefined"; break;
    }

    _cmsIOPrintf(ContextID, m, "/RenderingIntent (%s)\n", intent );
}

//
//  Convert L* to Y
//
//      Y = Yn*[ (L* + 16) / 116] ^ 3   if (L*) >= 6 / 29
//        = Yn*( L* / 116) / 7.787      if (L*) < 6 / 29
//

// Lab -> XYZ, see the discussion above

static
void EmitLab2XYZ(cmsContext ContextID, cmsIOHANDLER* m)
{
    _cmsIOPrintf(ContextID, m, "/RangeABC [ 0 1 0 1 0 1]\n");
    _cmsIOPrintf(ContextID, m, "/DecodeABC [\n");
    _cmsIOPrintf(ContextID, m, "{100 mul  16 add 116 div } bind\n");
    _cmsIOPrintf(ContextID, m, "{255 mul 128 sub 500 div } bind\n");
    _cmsIOPrintf(ContextID, m, "{255 mul 128 sub 200 div } bind\n");
    _cmsIOPrintf(ContextID, m, "]\n");
    _cmsIOPrintf(ContextID, m, "/MatrixABC [ 1 1 1 1 0 0 0 0 -1]\n");
    _cmsIOPrintf(ContextID, m, "/RangeLMN [ -0.236 1.254 0 1 -0.635 1.640 ]\n");
    _cmsIOPrintf(ContextID, m, "/DecodeLMN [\n");
    _cmsIOPrintf(ContextID, m, "{dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse 0.964200 mul} bind\n");
    _cmsIOPrintf(ContextID, m, "{dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse } bind\n");
    _cmsIOPrintf(ContextID, m, "{dup 6 29 div ge {dup dup mul mul} {4 29 div sub 108 841 div mul} ifelse 0.824900 mul} bind\n");
    _cmsIOPrintf(ContextID, m, "]\n");
}

static
void EmitSafeGuardBegin(cmsContext ContextID, cmsIOHANDLER* m, const char* name)
{
    _cmsIOPrintf(ContextID, m, "%%LCMS2: Save previous definition of %s on the operand stack\n", name);
    _cmsIOPrintf(ContextID, m, "currentdict /%s known { /%s load } { null } ifelse\n", name, name);
}

static
void EmitSafeGuardEnd(cmsContext ContextID, cmsIOHANDLER* m, const char* name, int depth)
{
    _cmsIOPrintf(ContextID, m, "%%LCMS2: Restore previous definition of %s\n", name);
    if (depth > 1) {
        // cycle topmost items on the stack to bring the previous definition to the front
        _cmsIOPrintf(ContextID, m, "%d -1 roll ", depth);
    }
    _cmsIOPrintf(ContextID, m, "dup null eq { pop currentdict /%s undef } { /%s exch def } ifelse\n", name, name);
}

// Outputs a table of words. It does use 16 bits

static
void Emit1Gamma(cmsContext ContextID, cmsIOHANDLER* m, cmsToneCurve* Table, const char* name)
{
    cmsUInt32Number i;
    cmsFloat64Number gamma;

    if (Table == NULL) return; // Error

    if (Table ->nEntries <= 0) return;  // Empty table

    // Suppress whole if identity
    if (cmsIsToneCurveLinear(ContextID, Table)) return;

    // Check if is really an exponential. If so, emit "exp"
    gamma = cmsEstimateGamma(ContextID, Table, 0.001);
     if (gamma > 0) {
            _cmsIOPrintf(ContextID, m, "/%s { %g exp } bind def\n", name, gamma);
            return;
     }

    EmitSafeGuardBegin(ContextID, m, "lcms2gammatable");
    _cmsIOPrintf(ContextID, m, "/lcms2gammatable [");

    for (i=0; i < Table->nEntries; i++) {
	if (i % 10 == 0)
            _cmsIOPrintf(ContextID, m, "\n  ");
        _cmsIOPrintf(ContextID, m, "%d ", Table->Table16[i]);
    }

    _cmsIOPrintf(ContextID, m, "] def\n");


    // Emit interpolation code

    // PostScript code                            Stack
    // ===============                            ========================
                                            	  // v
    _cmsIOPrintf(ContextID, m, "/%s {\n  ", name);

    // Bounds check
    EmitRangeCheck(ContextID, m);

    _cmsIOPrintf(ContextID, m, "\n  //lcms2gammatable ");    // v tab
    _cmsIOPrintf(ContextID, m, "dup ");                      // v tab tab
    _cmsIOPrintf(ContextID, m, "length 1 sub ");             // v tab dom
    _cmsIOPrintf(ContextID, m, "3 -1 roll ");                // tab dom v
    _cmsIOPrintf(ContextID, m, "mul ");                      // tab val2
    _cmsIOPrintf(ContextID, m, "dup ");                      // tab val2 val2
    _cmsIOPrintf(ContextID, m, "dup ");                      // tab val2 val2 val2
    _cmsIOPrintf(ContextID, m, "floor cvi ");                // tab val2 val2 cell0
    _cmsIOPrintf(ContextID, m, "exch ");                     // tab val2 cell0 val2
    _cmsIOPrintf(ContextID, m, "ceiling cvi ");              // tab val2 cell0 cell1
    _cmsIOPrintf(ContextID, m, "3 index ");                  // tab val2 cell0 cell1 tab
    _cmsIOPrintf(ContextID, m, "exch ");                     // tab val2 cell0 tab cell1
    _cmsIOPrintf(ContextID, m, "get\n  ");                   // tab val2 cell0 y1
    _cmsIOPrintf(ContextID, m, "4 -1 roll ");                // val2 cell0 y1 tab
    _cmsIOPrintf(ContextID, m, "3 -1 roll ");                // val2 y1 tab cell0
    _cmsIOPrintf(ContextID, m, "get ");                      // val2 y1 y0
    _cmsIOPrintf(ContextID, m, "dup ");                      // val2 y1 y0 y0
    _cmsIOPrintf(ContextID, m, "3 1 roll ");                 // val2 y0 y1 y0
    _cmsIOPrintf(ContextID, m, "sub ");                      // val2 y0 (y1-y0)
    _cmsIOPrintf(ContextID, m, "3 -1 roll ");                // y0 (y1-y0) val2
    _cmsIOPrintf(ContextID, m, "dup ");                      // y0 (y1-y0) val2 val2
    _cmsIOPrintf(ContextID, m, "floor cvi ");                // y0 (y1-y0) val2 floor(val2)
    _cmsIOPrintf(ContextID, m, "sub ");                      // y0 (y1-y0) rest
    _cmsIOPrintf(ContextID, m, "mul ");                      // y0 t1
    _cmsIOPrintf(ContextID, m, "add ");                      // y
    _cmsIOPrintf(ContextID, m, "65535 div\n");               // result

    _cmsIOPrintf(ContextID, m, "} bind def\n");

    EmitSafeGuardEnd(ContextID, m, "lcms2gammatable", 1);
}


// Compare gamma table

static
cmsBool GammaTableEquals(cmsUInt16Number* g1, cmsUInt16Number* g2, cmsUInt32Number nG1, cmsUInt32Number nG2)
{
    if (nG1 != nG2) return FALSE;
    return memcmp(g1, g2, nG1 * sizeof(cmsUInt16Number)) == 0;
}


// Does write a set of gamma curves

static
void EmitNGamma(cmsContext ContextID, cmsIOHANDLER* m, cmsUInt32Number n, cmsToneCurve* g[], const char* nameprefix)
{
    cmsUInt32Number i;
    static char buffer[2048];

    for( i=0; i < n; i++ )
    {
        if (g[i] == NULL) return; // Error

        if (i > 0 && GammaTableEquals(g[i-1]->Table16, g[i]->Table16, g[i-1]->nEntries, g[i]->nEntries)) {

            _cmsIOPrintf(ContextID, m, "/%s%d /%s%d load def\n", nameprefix, i, nameprefix, i-1);
        }
        else {
            snprintf(buffer, sizeof(buffer), "%s%d", nameprefix, (int) i);
	    buffer[sizeof(buffer)-1] = '\0';
            Emit1Gamma(ContextID, m, g[i], buffer);
        }
    }

}


// Following code dumps a LUT onto memory stream


// This is the sampler. Intended to work in SAMPLER_INSPECT mode,
// that is, the callback will be called for each knot with
//
//          In[]  The grid location coordinates, normalized to 0..ffff
//          Out[] The Pipeline values, normalized to 0..ffff
//
//  Returning a value other than 0 does terminate the sampling process
//
//  Each row contains Pipeline values for all but first component. So, I
//  detect row changing by keeping a copy of last value of first
//  component. -1 is used to mark beginning of whole block.

static
int OutputValueSampler(cmsContext ContextID, CMSREGISTER const cmsUInt16Number In[], CMSREGISTER cmsUInt16Number Out[], CMSREGISTER void* Cargo)
{
    cmsPsSamplerCargo* sc = (cmsPsSamplerCargo*) Cargo;
    cmsUInt32Number i;


    if (sc -> FixWhite) {

        if (In[0] == 0xFFFF) {  // Only in L* = 100, ab = [-8..8]

            if ((In[1] >= 0x7800 && In[1] <= 0x8800) &&
                (In[2] >= 0x7800 && In[2] <= 0x8800)) {

                cmsUInt16Number* Black;
                cmsUInt16Number* White;
                cmsUInt32Number nOutputs;

                if (!_cmsEndPointsBySpace(sc ->ColorSpace, &White, &Black, &nOutputs))
                        return 0;

                for (i=0; i < nOutputs; i++)
                        Out[i] = White[i];
            }


        }
    }


    // Hadle the parenthesis on rows

    if (In[0] != sc ->FirstComponent) {

            if (sc ->FirstComponent != -1) {

                    _cmsIOPrintf(ContextID, sc ->m, sc ->PostMin);
                    sc ->SecondComponent = -1;
                    _cmsIOPrintf(ContextID, sc ->m, sc ->PostMaj);
            }

            // Begin block
            _cmsPSActualColumn = 0;

            _cmsIOPrintf(ContextID, sc ->m, sc ->PreMaj);
            sc ->FirstComponent = In[0];
    }


      if (In[1] != sc ->SecondComponent) {

            if (sc ->SecondComponent != -1) {

                    _cmsIOPrintf(ContextID, sc ->m, sc ->PostMin);
            }

            _cmsIOPrintf(ContextID, sc ->m, sc ->PreMin);
            sc ->SecondComponent = In[1];
    }

      // Dump table.

      for (i=0; i < sc -> Pipeline ->Params->nOutputs; i++) {

          cmsUInt16Number wWordOut = Out[i];
          cmsUInt8Number wByteOut;           // Value as byte


          // We always deal with Lab4

          wByteOut = Word2Byte(wWordOut);
          WriteByte(ContextID, sc -> m, wByteOut);
      }

      return 1;
}

// Writes a Pipeline on memstream. Could be 8 or 16 bits based

static
void WriteCLUT(cmsContext ContextID, cmsIOHANDLER* m, cmsStage* mpe, const char* PreMaj,
                                               const char* PostMaj,
                                               const char* PreMin,
                                               const char* PostMin,
                                               int FixWhite,
                                               cmsColorSpaceSignature ColorSpace)
{
    cmsUInt32Number i;
    cmsPsSamplerCargo sc;

    sc.FirstComponent = -1;
    sc.SecondComponent = -1;
    sc.Pipeline = (_cmsStageCLutData *) mpe ->Data;
    sc.m   = m;
    sc.PreMaj = PreMaj;
    sc.PostMaj= PostMaj;

    sc.PreMin   = PreMin;
    sc.PostMin  = PostMin;
    sc.FixWhite = FixWhite;
    sc.ColorSpace = ColorSpace;

    _cmsIOPrintf(ContextID, m, "[");

    for (i=0; i < sc.Pipeline->Params->nInputs; i++)
        _cmsIOPrintf(ContextID, m, " %d ", sc.Pipeline->Params->nSamples[i]);

    _cmsIOPrintf(ContextID, m, " [\n");

    cmsStageSampleCLut16bit(ContextID, mpe, OutputValueSampler, (void*) &sc, SAMPLER_INSPECT);

    _cmsIOPrintf(ContextID, m, PostMin);
    _cmsIOPrintf(ContextID, m, PostMaj);
    _cmsIOPrintf(ContextID, m, "] ");

}


// Dumps CIEBasedA Color Space Array

static
int EmitCIEBasedA(cmsContext ContextID, cmsIOHANDLER* m, cmsToneCurve* Curve, cmsCIEXYZ* BlackPoint)
{

    _cmsIOPrintf(ContextID, m, "[ /CIEBasedA\n");
    _cmsIOPrintf(ContextID, m, "  <<\n");

    EmitSafeGuardBegin(ContextID, m, "lcms2gammaproc");
    Emit1Gamma(ContextID, m, Curve, "lcms2gammaproc");

    _cmsIOPrintf(ContextID, m, "/DecodeA /lcms2gammaproc load\n");
    EmitSafeGuardEnd(ContextID, m, "lcms2gammaproc", 3);

    _cmsIOPrintf(ContextID, m, "/MatrixA [ 0.9642 1.0000 0.8249 ]\n");
    _cmsIOPrintf(ContextID, m, "/RangeLMN [ 0.0 0.9642 0.0 1.0000 0.0 0.8249 ]\n");

    EmitWhiteBlackD50(ContextID, m, BlackPoint);
    EmitIntent(ContextID, m, INTENT_PERCEPTUAL);

    _cmsIOPrintf(ContextID, m, ">>\n");
    _cmsIOPrintf(ContextID, m, "]\n");

    return 1;
}


// Dumps CIEBasedABC Color Space Array

static
int EmitCIEBasedABC(cmsContext ContextID, cmsIOHANDLER* m, cmsFloat64Number* Matrix, cmsToneCurve** CurveSet, cmsCIEXYZ* BlackPoint)
{
    int i;

    _cmsIOPrintf(ContextID, m, "[ /CIEBasedABC\n");
    _cmsIOPrintf(ContextID, m, "<<\n");

    EmitSafeGuardBegin(ContextID, m, "lcms2gammaproc0");
    EmitSafeGuardBegin(ContextID, m, "lcms2gammaproc1");
    EmitSafeGuardBegin(ContextID, m, "lcms2gammaproc2");
    EmitNGamma(ContextID, m, 3, CurveSet, "lcms2gammaproc");
    _cmsIOPrintf(ContextID, m, "/DecodeABC [\n");
    _cmsIOPrintf(ContextID, m, "   /lcms2gammaproc0 load\n");
    _cmsIOPrintf(ContextID, m, "   /lcms2gammaproc1 load\n");
    _cmsIOPrintf(ContextID, m, "   /lcms2gammaproc2 load\n");
    _cmsIOPrintf(ContextID, m, "]\n");
    EmitSafeGuardEnd(ContextID, m, "lcms2gammaproc2", 3);
    EmitSafeGuardEnd(ContextID, m, "lcms2gammaproc1", 3);
    EmitSafeGuardEnd(ContextID, m, "lcms2gammaproc0", 3);

    _cmsIOPrintf(ContextID, m, "/MatrixABC [ " );

    for( i=0; i < 3; i++ ) {

        _cmsIOPrintf(ContextID, m, "%.6f %.6f %.6f ", Matrix[i + 3*0],
                                           Matrix[i + 3*1],
                                           Matrix[i + 3*2]);
    }


    _cmsIOPrintf(ContextID, m, "]\n");

    _cmsIOPrintf(ContextID, m, "/RangeLMN [ 0.0 0.9642 0.0 1.0000 0.0 0.8249 ]\n");

    EmitWhiteBlackD50(ContextID, m, BlackPoint);
    EmitIntent(ContextID, m, INTENT_PERCEPTUAL);

    _cmsIOPrintf(ContextID, m, ">>\n");
    _cmsIOPrintf(ContextID, m, "]\n");


    return 1;
}


static
int EmitCIEBasedDEF(cmsContext ContextID, cmsIOHANDLER* m, cmsPipeline* Pipeline, cmsUInt32Number Intent, cmsCIEXYZ* BlackPoint)
{
    const char* PreMaj;
    const char* PostMaj;
    const char* PreMin, * PostMin;
    cmsStage* mpe;
    int i, numchans;
    static char buffer[2048];

    mpe = Pipeline->Elements;

    switch (cmsStageInputChannels(ContextID, mpe)) {
    case 3:
        _cmsIOPrintf(ContextID, m, "[ /CIEBasedDEF\n");
        PreMaj = "<";
        PostMaj = ">\n";
        PreMin = PostMin = "";
        break;

    case 4:
        _cmsIOPrintf(ContextID, m, "[ /CIEBasedDEFG\n");
        PreMaj = "[";
        PostMaj = "]\n";
        PreMin = "<";
        PostMin = ">\n";
        break;

    default:
        return 0;

    }

    _cmsIOPrintf(ContextID, m, "<<\n");

    if (cmsStageType(ContextID, mpe) == cmsSigCurveSetElemType) {

        numchans = (int) cmsStageOutputChannels(ContextID, mpe);
        for (i = 0; i < numchans; ++i) {
            snprintf(buffer, sizeof(buffer), "lcms2gammaproc%d", i);
            buffer[sizeof(buffer) - 1] = '\0';
            EmitSafeGuardBegin(ContextID, m, buffer);
        }
        EmitNGamma(ContextID, m, cmsStageOutputChannels(ContextID, mpe), _cmsStageGetPtrToCurveSet(mpe), "lcms2gammaproc");
        _cmsIOPrintf(ContextID, m, "/DecodeDEF [\n");
        for (i = 0; i < numchans; ++i) {
            snprintf(buffer, sizeof(buffer), "  /lcms2gammaproc%d load\n", i);
            buffer[sizeof(buffer) - 1] = '\0';
            _cmsIOPrintf(ContextID, m, buffer);
        }
        _cmsIOPrintf(ContextID, m, "]\n");
        for (i = numchans - 1; i >= 0; --i) {
            snprintf(buffer, sizeof(buffer), "lcms2gammaproc%d", i);
            buffer[sizeof(buffer) - 1] = '\0';
            EmitSafeGuardEnd(ContextID, m, buffer, 3);
        }

        mpe = mpe->Next;
    }

    if (cmsStageType(ContextID, mpe) == cmsSigCLutElemType) {

        _cmsIOPrintf(ContextID, m, "/Table ");
        WriteCLUT(ContextID, m, mpe, PreMaj, PostMaj, PreMin, PostMin, FALSE, (cmsColorSpaceSignature)0);
        _cmsIOPrintf(ContextID, m, "]\n");
    }

    EmitLab2XYZ(ContextID, m);
    EmitWhiteBlackD50(ContextID, m, BlackPoint);
    EmitIntent(ContextID, m, Intent);

    _cmsIOPrintf(ContextID, m, "   >>\n");
    _cmsIOPrintf(ContextID, m, "]\n");

    return 1;
}

// Generates a curve from a gray profile

static
cmsToneCurve* ExtractGray2Y(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent)
{
    cmsToneCurve* Out = cmsBuildTabulatedToneCurve16(ContextID, 256, NULL);
    cmsHPROFILE hXYZ  = cmsCreateXYZProfile(ContextID);
    cmsHTRANSFORM xform = cmsCreateTransform(ContextID, hProfile, TYPE_GRAY_8, hXYZ, TYPE_XYZ_DBL, Intent, cmsFLAGS_NOOPTIMIZE);
    int i;

    if (Out != NULL && xform != NULL) {
        for (i=0; i < 256; i++) {

            cmsUInt8Number Gray = (cmsUInt8Number) i;
            cmsCIEXYZ XYZ;

            cmsDoTransform(ContextID, xform, &Gray, &XYZ, 1);

            Out ->Table16[i] =_cmsQuickSaturateWord(XYZ.Y * 65535.0);
        }
    }

    if (xform) cmsDeleteTransform(ContextID, xform);
    if (hXYZ) cmsCloseProfile(ContextID, hXYZ);
    return Out;
}



// Because PostScript has only 8 bits in /Table, we should use
// a more perceptually uniform space... I do choose Lab.

static
int WriteInputLUT(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags)
{
    cmsHPROFILE hLab;
    cmsHTRANSFORM xform;
    cmsUInt32Number nChannels;
    cmsUInt32Number InputFormat;
    int rc;
    cmsHPROFILE Profiles[2];
    cmsCIEXYZ BlackPointAdaptedToD50;

    // Does create a device-link based transform.
    // The DeviceLink is next dumped as working CSA.

    InputFormat = cmsFormatterForColorspaceOfProfile(ContextID, hProfile, 2, FALSE);
    nChannels   = T_CHANNELS(InputFormat);


    cmsDetectBlackPoint(ContextID, &BlackPointAdaptedToD50, hProfile, Intent, 0);

    // Adjust output to Lab4
    hLab = cmsCreateLab4Profile(ContextID, NULL);

    Profiles[0] = hProfile;
    Profiles[1] = hLab;

    xform = cmsCreateMultiprofileTransform(ContextID, Profiles, 2,  InputFormat, TYPE_Lab_DBL, Intent, 0);
    cmsCloseProfile(ContextID, hLab);

    if (xform == NULL) {

        cmsSignalError(ContextID, cmsERROR_COLORSPACE_CHECK, "Cannot create transform Profile -> Lab");
        return 0;
    }

    // Only 1, 3 and 4 channels are allowed

    switch (nChannels) {

    case 1: {
            cmsToneCurve* Gray2Y = ExtractGray2Y(ContextID, hProfile, Intent);
            EmitCIEBasedA(ContextID, m, Gray2Y, &BlackPointAdaptedToD50);
            cmsFreeToneCurve(ContextID, Gray2Y);
            }
            break;

    case 3:
    case 4: {
            cmsUInt32Number OutFrm = TYPE_Lab_16;
            cmsPipeline* DeviceLink;
            _cmsTRANSFORM* v = (_cmsTRANSFORM*) xform;

            DeviceLink = cmsPipelineDup(ContextID, v ->core->Lut);
            if (DeviceLink == NULL) return 0;

            dwFlags |= cmsFLAGS_FORCE_CLUT;
            _cmsOptimizePipeline(ContextID, &DeviceLink, Intent, &InputFormat, &OutFrm, &dwFlags);

            rc = EmitCIEBasedDEF(ContextID, m, DeviceLink, Intent, &BlackPointAdaptedToD50);
            cmsPipelineFree(ContextID, DeviceLink);
            if (rc == 0) return 0;
            }
            break;

    default:

        cmsSignalError(ContextID, cmsERROR_COLORSPACE_CHECK, "Only 3, 4 channels are supported for CSA. This profile has %d channels.", nChannels);
        return 0;
    }


    cmsDeleteTransform(ContextID, xform);

    return 1;
}

static
cmsFloat64Number* GetPtrToMatrix(const cmsStage* mpe)
{
    _cmsStageMatrixData* Data = (_cmsStageMatrixData*) mpe ->Data;

    return Data -> Double;
}


// Does create CSA based on matrix-shaper. Allowed types are gray and RGB based
static
int WriteInputMatrixShaper(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hProfile, cmsStage* Matrix, cmsStage* Shaper)
{
    cmsColorSpaceSignature ColorSpace;
    int rc;
    cmsCIEXYZ BlackPointAdaptedToD50;

    ColorSpace = cmsGetColorSpace(ContextID, hProfile);

    cmsDetectBlackPoint(ContextID, &BlackPointAdaptedToD50, hProfile, INTENT_RELATIVE_COLORIMETRIC, 0);

    if (ColorSpace == cmsSigGrayData) {

        cmsToneCurve** ShaperCurve = _cmsStageGetPtrToCurveSet(Shaper);
        rc = EmitCIEBasedA(ContextID, m, ShaperCurve[0], &BlackPointAdaptedToD50);

    }
    else
        if (ColorSpace == cmsSigRgbData) {

            cmsMAT3 Mat;
            int i, j;

            memmove(&Mat, GetPtrToMatrix(Matrix), sizeof(Mat));

            for (i = 0; i < 3; i++)
                for (j = 0; j < 3; j++)
                    Mat.v[i].n[j] *= MAX_ENCODEABLE_XYZ;

            rc = EmitCIEBasedABC(ContextID, m, (cmsFloat64Number *) &Mat,
                _cmsStageGetPtrToCurveSet(Shaper),
                &BlackPointAdaptedToD50);
        }
        else {

            cmsSignalError(ContextID, cmsERROR_COLORSPACE_CHECK, "Profile is not suitable for CSA. Unsupported colorspace.");
            return 0;
        }

    return rc;
}



// Creates a PostScript color list from a named profile data.
// This is a HP extension, and it works in Lab instead of XYZ

static
int WriteNamedColorCSA(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hNamedColor, cmsUInt32Number Intent)
{
    cmsHTRANSFORM xform;
    cmsHPROFILE   hLab;
    cmsUInt32Number i, nColors;
    char ColorName[cmsMAX_PATH];
    cmsNAMEDCOLORLIST* NamedColorList;

    hLab  = cmsCreateLab4Profile(ContextID, NULL);
    xform = cmsCreateTransform(ContextID, hNamedColor, TYPE_NAMED_COLOR_INDEX, hLab, TYPE_Lab_DBL, Intent, 0);
    if (xform == NULL) return 0;

    NamedColorList = cmsGetNamedColorList(xform);
    if (NamedColorList == NULL) return 0;

    _cmsIOPrintf(ContextID, m, "<<\n");
    _cmsIOPrintf(ContextID, m, "(colorlistcomment) (%s)\n", "Named color CSA");
    _cmsIOPrintf(ContextID, m, "(Prefix) [ (Pantone ) (PANTONE ) ]\n");
    _cmsIOPrintf(ContextID, m, "(Suffix) [ ( CV) ( CVC) ( C) ]\n");

    nColors   = cmsNamedColorCount(ContextID, NamedColorList);


    for (i=0; i < nColors; i++) {

        cmsUInt16Number In[1];
        cmsCIELab Lab;

        In[0] = (cmsUInt16Number) i;

        if (!cmsNamedColorInfo(ContextID, NamedColorList, i, ColorName, NULL, NULL, NULL, NULL))
                continue;

        cmsDoTransform(ContextID, xform, In, &Lab, 1);
        _cmsIOPrintf(ContextID, m, "  (%s) [ %.3f %.3f %.3f ]\n", ColorName, Lab.L, Lab.a, Lab.b);
    }



    _cmsIOPrintf(ContextID, m, ">>\n");

    cmsDeleteTransform(ContextID, xform);
    cmsCloseProfile(ContextID, hLab);
    return 1;
}


// Does create a Color Space Array on XYZ colorspace for PostScript usage
static
cmsUInt32Number GenerateCSA(cmsContext ContextID,
                            cmsHPROFILE hProfile,
                            cmsUInt32Number Intent,
                            cmsUInt32Number dwFlags,
                            cmsIOHANDLER* mem)
{
    cmsUInt32Number dwBytesUsed;
    cmsPipeline* lut = NULL;
    cmsStage* Matrix, *Shaper;


    // Is a named color profile?
    if (cmsGetDeviceClass(ContextID, hProfile) == cmsSigNamedColorClass) {

        if (!WriteNamedColorCSA(ContextID, mem, hProfile, Intent)) goto Error;
    }
    else {


        // Any profile class are allowed (including devicelink), but
        // output (PCS) colorspace must be XYZ or Lab
        cmsColorSpaceSignature ColorSpace = cmsGetPCS(ContextID, hProfile);

        if (ColorSpace != cmsSigXYZData &&
            ColorSpace != cmsSigLabData) {

                cmsSignalError(ContextID, cmsERROR_COLORSPACE_CHECK, "Invalid output color space");
                goto Error;
        }


        // Read the lut with all necessary conversion stages
        lut = _cmsReadInputLUT(ContextID, hProfile, Intent);
        if (lut == NULL) goto Error;


        // Tone curves + matrix can be implemented without any LUT
        if (cmsPipelineCheckAndRetreiveStages(ContextID, lut, 2, cmsSigCurveSetElemType, cmsSigMatrixElemType, &Shaper, &Matrix)) {

            if (!WriteInputMatrixShaper(ContextID, mem, hProfile, Matrix, Shaper)) goto Error;

        }
        else {
           // We need a LUT for the rest
           if (!WriteInputLUT(ContextID, mem, hProfile, Intent, dwFlags)) goto Error;
        }
    }


    // Done, keep memory usage
    dwBytesUsed = mem ->UsedSpace;

    // Get rid of LUT
    if (lut != NULL) cmsPipelineFree(ContextID, lut);

    // Finally, return used byte count
    return dwBytesUsed;

Error:
    if (lut != NULL) cmsPipelineFree(ContextID, lut);
    return 0;
}

// ------------------------------------------------------ Color Rendering Dictionary (CRD)



/*

  Black point compensation plus chromatic adaptation:

  Step 1 - Chromatic adaptation
  =============================

          WPout
    X = ------- PQR
          Wpin

  Step 2 - Black point compensation
  =================================

          (WPout - BPout)*X - WPout*(BPin - BPout)
    out = ---------------------------------------
                        WPout - BPin


  Algorithm discussion
  ====================

  TransformPQR(WPin, BPin, WPout, BPout, PQR)

  Wpin,etc= { Xws Yws Zws Pws Qws Rws }


  Algorithm             Stack 0...n
  ===========================================================
                        PQR BPout WPout BPin WPin
  4 index 3 get         WPin PQR BPout WPout BPin WPin
  div                   (PQR/WPin) BPout WPout BPin WPin
  2 index 3 get         WPout (PQR/WPin) BPout WPout BPin WPin
  mult                  WPout*(PQR/WPin) BPout WPout BPin WPin

  2 index 3 get         WPout WPout*(PQR/WPin) BPout WPout BPin WPin
  2 index 3 get         BPout WPout WPout*(PQR/WPin) BPout WPout BPin WPin
  sub                   (WPout-BPout) WPout*(PQR/WPin) BPout WPout BPin WPin
  mult                  (WPout-BPout)* WPout*(PQR/WPin) BPout WPout BPin WPin

  2 index 3 get         WPout (BPout-WPout)* WPout*(PQR/WPin) BPout WPout BPin WPin
  4 index 3 get         BPin WPout (BPout-WPout)* WPout*(PQR/WPin) BPout WPout BPin WPin
  3 index 3 get         BPout BPin WPout (BPout-WPout)* WPout*(PQR/WPin) BPout WPout BPin WPin

  sub                   (BPin-BPout) WPout (BPout-WPout)* WPout*(PQR/WPin) BPout WPout BPin WPin
  mult                  (BPin-BPout)*WPout (BPout-WPout)* WPout*(PQR/WPin) BPout WPout BPin WPin
  sub                   (BPout-WPout)* WPout*(PQR/WPin)-(BPin-BPout)*WPout BPout WPout BPin WPin

  3 index 3 get         BPin (BPout-WPout)* WPout*(PQR/WPin)-(BPin-BPout)*WPout BPout WPout BPin WPin
  3 index 3 get         WPout BPin (BPout-WPout)* WPout*(PQR/WPin)-(BPin-BPout)*WPout BPout WPout BPin WPin
  exch
  sub                   (WPout-BPin) (BPout-WPout)* WPout*(PQR/WPin)-(BPin-BPout)*WPout BPout WPout BPin WPin
  div

  exch pop
  exch pop
  exch pop
  exch pop

*/


static
void EmitPQRStage(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hProfile, int DoBPC, int lIsAbsolute)
{


        if (lIsAbsolute) {

            // For absolute colorimetric intent, encode back to relative
            // and generate a relative Pipeline

            // Relative encoding is obtained across XYZpcs*(D50/WhitePoint)

            cmsCIEXYZ White;

            _cmsReadMediaWhitePoint(ContextID, &White, hProfile);

            _cmsIOPrintf(ContextID, m,"/MatrixPQR [1 0 0 0 1 0 0 0 1 ]\n");
            _cmsIOPrintf(ContextID, m,"/RangePQR [ -0.5 2 -0.5 2 -0.5 2 ]\n");

            _cmsIOPrintf(ContextID, m, "%% Absolute colorimetric -- encode to relative to maximize LUT usage\n"
                      "/TransformPQR [\n"
                      "{0.9642 mul %g div exch pop exch pop exch pop exch pop} bind\n"
                      "{1.0000 mul %g div exch pop exch pop exch pop exch pop} bind\n"
                      "{0.8249 mul %g div exch pop exch pop exch pop exch pop} bind\n]\n",
                      White.X, White.Y, White.Z);
            return;
        }


        _cmsIOPrintf(ContextID, m,"%% Bradford Cone Space\n"
                 "/MatrixPQR [0.8951 -0.7502 0.0389 0.2664 1.7135 -0.0685 -0.1614 0.0367 1.0296 ] \n");

        _cmsIOPrintf(ContextID, m, "/RangePQR [ -0.5 2 -0.5 2 -0.5 2 ]\n");


        // No BPC

        if (!DoBPC) {

            _cmsIOPrintf(ContextID, m, "%% VonKries-like transform in Bradford Cone Space\n"
                      "/TransformPQR [\n"
                      "{exch pop exch 3 get mul exch pop exch 3 get div} bind\n"
                      "{exch pop exch 4 get mul exch pop exch 4 get div} bind\n"
                      "{exch pop exch 5 get mul exch pop exch 5 get div} bind\n]\n");
        } else {

            // BPC

            _cmsIOPrintf(ContextID, m, "%% VonKries-like transform in Bradford Cone Space plus BPC\n"
                      "/TransformPQR [\n");

            _cmsIOPrintf(ContextID, m, "{4 index 3 get div 2 index 3 get mul "
                    "2 index 3 get 2 index 3 get sub mul "
                    "2 index 3 get 4 index 3 get 3 index 3 get sub mul sub "
                    "3 index 3 get 3 index 3 get exch sub div "
                    "exch pop exch pop exch pop exch pop } bind\n");

            _cmsIOPrintf(ContextID, m, "{4 index 4 get div 2 index 4 get mul "
                    "2 index 4 get 2 index 4 get sub mul "
                    "2 index 4 get 4 index 4 get 3 index 4 get sub mul sub "
                    "3 index 4 get 3 index 4 get exch sub div "
                    "exch pop exch pop exch pop exch pop } bind\n");

            _cmsIOPrintf(ContextID, m, "{4 index 5 get div 2 index 5 get mul "
                    "2 index 5 get 2 index 5 get sub mul "
                    "2 index 5 get 4 index 5 get 3 index 5 get sub mul sub "
                    "3 index 5 get 3 index 5 get exch sub div "
                    "exch pop exch pop exch pop exch pop } bind\n]\n");

        }
}


static
void EmitXYZ2Lab(cmsContext ContextID, cmsIOHANDLER* m)
{
    _cmsIOPrintf(ContextID, m, "/RangeLMN [ -0.635 2.0 0 2 -0.635 2.0 ]\n");
    _cmsIOPrintf(ContextID, m, "/EncodeLMN [\n");
    _cmsIOPrintf(ContextID, m, "{ 0.964200  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind\n");
    _cmsIOPrintf(ContextID, m, "{ 1.000000  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind\n");
    _cmsIOPrintf(ContextID, m, "{ 0.824900  div dup 0.008856 le {7.787 mul 16 116 div add}{1 3 div exp} ifelse } bind\n");
    _cmsIOPrintf(ContextID, m, "]\n");
    _cmsIOPrintf(ContextID, m, "/MatrixABC [ 0 1 0 1 -1 1 0 0 -1 ]\n");
    _cmsIOPrintf(ContextID, m, "/EncodeABC [\n");


    _cmsIOPrintf(ContextID, m, "{ 116 mul  16 sub 100 div  } bind\n");
    _cmsIOPrintf(ContextID, m, "{ 500 mul 128 add 256 div  } bind\n");
    _cmsIOPrintf(ContextID, m, "{ 200 mul 128 add 256 div  } bind\n");


    _cmsIOPrintf(ContextID, m, "]\n");


}

// Due to impedance mismatch between XYZ and almost all RGB and CMYK spaces
// I choose to dump LUTS in Lab instead of XYZ. There is still a lot of wasted
// space on 3D CLUT, but since space seems not to be a problem here, 33 points
// would give a reasonable accuracy. Note also that CRD tables must operate in
// 8 bits.

static
int WriteOutputLUT(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags)
{
    cmsHPROFILE hLab;
    cmsHTRANSFORM xform;
    cmsUInt32Number i, nChannels;
    cmsUInt32Number OutputFormat;
    _cmsTRANSFORM* v;
    cmsPipeline* DeviceLink;
    cmsHPROFILE Profiles[3];
    cmsCIEXYZ BlackPointAdaptedToD50;
    cmsBool lDoBPC = (cmsBool) (dwFlags & cmsFLAGS_BLACKPOINTCOMPENSATION);
    cmsBool lFixWhite = (cmsBool) !(dwFlags & cmsFLAGS_NOWHITEONWHITEFIXUP);
    cmsUInt32Number InFrm = TYPE_Lab_16;
    cmsUInt32Number RelativeEncodingIntent;
    cmsColorSpaceSignature ColorSpace;


    hLab = cmsCreateLab4Profile(ContextID, NULL);
    if (hLab == NULL) return 0;

    OutputFormat = cmsFormatterForColorspaceOfProfile(ContextID, hProfile, 2, FALSE);
    nChannels    = T_CHANNELS(OutputFormat);

    ColorSpace = cmsGetColorSpace(ContextID, hProfile);

    // For absolute colorimetric, the LUT is encoded as relative in order to preserve precision.

    RelativeEncodingIntent = Intent;
    if (RelativeEncodingIntent == INTENT_ABSOLUTE_COLORIMETRIC)
        RelativeEncodingIntent = INTENT_RELATIVE_COLORIMETRIC;


    // Use V4 Lab always
    Profiles[0] = hLab;
    Profiles[1] = hProfile;

    xform = cmsCreateMultiprofileTransform(ContextID,
                                              Profiles, 2, TYPE_Lab_DBL,
                                              OutputFormat, RelativeEncodingIntent, 0);
    cmsCloseProfile(ContextID, hLab);

    if (xform == NULL) {

        cmsSignalError(ContextID, cmsERROR_COLORSPACE_CHECK, "Cannot create transform Lab -> Profile in CRD creation");
        return 0;
    }

    // Get a copy of the internal devicelink
    v = (_cmsTRANSFORM*) xform;
    DeviceLink = cmsPipelineDup(ContextID, v ->core->Lut);
    if (DeviceLink == NULL) return 0;


    // We need a CLUT
    dwFlags |= cmsFLAGS_FORCE_CLUT;
    _cmsOptimizePipeline(ContextID, &DeviceLink, RelativeEncodingIntent, &InFrm, &OutputFormat, &dwFlags);

    _cmsIOPrintf(ContextID, m, "<<\n");
    _cmsIOPrintf(ContextID, m, "/ColorRenderingType 1\n");


    cmsDetectBlackPoint(ContextID, &BlackPointAdaptedToD50, hProfile, Intent, 0);

    // Emit headers, etc.
    EmitWhiteBlackD50(ContextID, m, &BlackPointAdaptedToD50);
    EmitPQRStage(ContextID, m, hProfile, lDoBPC, Intent == INTENT_ABSOLUTE_COLORIMETRIC);
    EmitXYZ2Lab(ContextID, m);


    // FIXUP: map Lab (100, 0, 0) to perfect white, because the particular encoding for Lab
    // does map a=b=0 not falling into any specific node. Since range a,b goes -128..127,
    // zero is slightly moved towards right, so assure next node (in L=100 slice) is mapped to
    // zero. This would sacrifice a bit of highlights, but failure to do so would cause
    // scum dot. Ouch.

    if (Intent == INTENT_ABSOLUTE_COLORIMETRIC)
            lFixWhite = FALSE;

    _cmsIOPrintf(ContextID, m, "/RenderTable ");


    WriteCLUT(ContextID, m, cmsPipelineGetPtrToFirstStage(ContextID, DeviceLink), "<", ">\n", "", "", lFixWhite, ColorSpace);

    _cmsIOPrintf(ContextID, m, " %d {} bind ", nChannels);

    for (i=1; i < nChannels; i++)
            _cmsIOPrintf(ContextID, m, "dup ");

    _cmsIOPrintf(ContextID, m, "]\n");


    EmitIntent(ContextID, m, Intent);

    _cmsIOPrintf(ContextID, m, ">>\n");

    if (!(dwFlags & cmsFLAGS_NODEFAULTRESOURCEDEF)) {

        _cmsIOPrintf(ContextID, m, "/Current exch /ColorRendering defineresource pop\n");
    }

    cmsPipelineFree(ContextID, DeviceLink);
    cmsDeleteTransform(ContextID, xform);

    return 1;
}


// Builds a ASCII string containing colorant list in 0..1.0 range
static
void BuildColorantList(char *Colorant, cmsUInt32Number nColorant, cmsUInt16Number Out[])
{
    char Buff[32];
    cmsUInt32Number j;

    Colorant[0] = 0;
    if (nColorant > cmsMAXCHANNELS)
        nColorant = cmsMAXCHANNELS;

    for (j = 0; j < nColorant; j++) {

        snprintf(Buff, 31, "%.3f", Out[j] / 65535.0);
        Buff[31] = 0;
        strcat(Colorant, Buff);
        if (j < nColorant - 1)
            strcat(Colorant, " ");

    }
}


// Creates a PostScript color list from a named profile data.
// This is a HP extension.

static
int WriteNamedColorCRD(cmsContext ContextID, cmsIOHANDLER* m, cmsHPROFILE hNamedColor, cmsUInt32Number Intent, cmsUInt32Number dwFlags)
{
    cmsHTRANSFORM xform;
    cmsUInt32Number i, nColors, nColorant;
    cmsUInt32Number OutputFormat;
    char ColorName[cmsMAX_PATH];
    char Colorant[512];
    cmsNAMEDCOLORLIST* NamedColorList;


    OutputFormat = cmsFormatterForColorspaceOfProfile(ContextID, hNamedColor, 2, FALSE);
    nColorant    = T_CHANNELS(OutputFormat);


    xform = cmsCreateTransform(ContextID, hNamedColor, TYPE_NAMED_COLOR_INDEX, NULL, OutputFormat, Intent, dwFlags);
    if (xform == NULL) return 0;


    NamedColorList = cmsGetNamedColorList(xform);
    if (NamedColorList == NULL) return 0;

    _cmsIOPrintf(ContextID, m, "<<\n");
    _cmsIOPrintf(ContextID, m, "(colorlistcomment) (%s) \n", "Named profile");
    _cmsIOPrintf(ContextID, m, "(Prefix) [ (Pantone ) (PANTONE ) ]\n");
    _cmsIOPrintf(ContextID, m, "(Suffix) [ ( CV) ( CVC) ( C) ]\n");

    nColors   = cmsNamedColorCount(ContextID, NamedColorList);

    for (i=0; i < nColors; i++) {

        cmsUInt16Number In[1];
        cmsUInt16Number Out[cmsMAXCHANNELS];

        In[0] = (cmsUInt16Number) i;

        if (!cmsNamedColorInfo(ContextID, NamedColorList, i, ColorName, NULL, NULL, NULL, NULL))
                continue;

        cmsDoTransform(ContextID, xform, In, Out, 1);
        BuildColorantList(Colorant, nColorant, Out);
        _cmsIOPrintf(ContextID, m, "  (%s) [ %s ]\n", ColorName, Colorant);
    }

    _cmsIOPrintf(ContextID, m, "   >>");

    if (!(dwFlags & cmsFLAGS_NODEFAULTRESOURCEDEF)) {

    _cmsIOPrintf(ContextID, m, " /Current exch /HPSpotTable defineresource pop\n");
    }

    cmsDeleteTransform(ContextID, xform);
    return 1;
}



// This one does create a Color Rendering Dictionary.
// CRD are always LUT-Based, no matter if profile is
// implemented as matrix-shaper.

static
cmsUInt32Number  GenerateCRD(cmsContext ContextID,
                             cmsHPROFILE hProfile,
                             cmsUInt32Number Intent, cmsUInt32Number dwFlags,
                             cmsIOHANDLER* mem)
{
    cmsUInt32Number dwBytesUsed;

    if (!(dwFlags & cmsFLAGS_NODEFAULTRESOURCEDEF)) {

        EmitHeader(ContextID, mem, "Color Rendering Dictionary (CRD)", hProfile);
    }


    // Is a named color profile?
    if (cmsGetDeviceClass(ContextID, hProfile) == cmsSigNamedColorClass) {

        if (!WriteNamedColorCRD(ContextID, mem, hProfile, Intent, dwFlags)) {
            return 0;
        }
    }
    else {

        // CRD are always implemented as LUT

        if (!WriteOutputLUT(ContextID, mem, hProfile, Intent, dwFlags)) {
            return 0;
        }
    }

    if (!(dwFlags & cmsFLAGS_NODEFAULTRESOURCEDEF)) {

        _cmsIOPrintf(ContextID, mem, "%%%%EndResource\n");
        _cmsIOPrintf(ContextID, mem, "\n%% CRD End\n");
    }

    // Done, keep memory usage
    dwBytesUsed = mem ->UsedSpace;

    // Finally, return used byte count
    return dwBytesUsed;
}




cmsUInt32Number CMSEXPORT cmsGetPostScriptColorResource(cmsContext ContextID,
                                                               cmsPSResourceType Type,
                                                               cmsHPROFILE hProfile,
                                                               cmsUInt32Number Intent,
                                                               cmsUInt32Number dwFlags,
                                                               cmsIOHANDLER* io)
{
    cmsUInt32Number  rc;


    switch (Type) {

        case cmsPS_RESOURCE_CSA:
            rc = GenerateCSA(ContextID, hProfile, Intent, dwFlags, io);
            break;

        default:
        case cmsPS_RESOURCE_CRD:
            rc = GenerateCRD(ContextID, hProfile, Intent, dwFlags, io);
            break;
    }

    return rc;
}



cmsUInt32Number CMSEXPORT cmsGetPostScriptCRD(cmsContext ContextID,
                              cmsHPROFILE hProfile,
                              cmsUInt32Number Intent, cmsUInt32Number dwFlags,
                              void* Buffer, cmsUInt32Number dwBufferLen)
{
    cmsIOHANDLER* mem;
    cmsUInt32Number dwBytesUsed;

    // Set up the serialization engine
    if (Buffer == NULL)
        mem = cmsOpenIOhandlerFromNULL(ContextID);
    else
        mem = cmsOpenIOhandlerFromMem(ContextID, Buffer, dwBufferLen, "w");

    if (!mem) return 0;

    dwBytesUsed =  cmsGetPostScriptColorResource(ContextID, cmsPS_RESOURCE_CRD, hProfile, Intent, dwFlags, mem);

    // Get rid of memory stream
    cmsCloseIOhandler(ContextID, mem);

    return dwBytesUsed;
}



// Does create a Color Space Array on XYZ colorspace for PostScript usage
cmsUInt32Number CMSEXPORT cmsGetPostScriptCSA(cmsContext ContextID,
                                              cmsHPROFILE hProfile,
                                              cmsUInt32Number Intent,
                                              cmsUInt32Number dwFlags,
                                              void* Buffer,
                                              cmsUInt32Number dwBufferLen)
{
    cmsIOHANDLER* mem;
    cmsUInt32Number dwBytesUsed;

    if (Buffer == NULL)
        mem = cmsOpenIOhandlerFromNULL(ContextID);
    else
        mem = cmsOpenIOhandlerFromMem(ContextID, Buffer, dwBufferLen, "w");

    if (!mem) return 0;

    dwBytesUsed =  cmsGetPostScriptColorResource(ContextID, cmsPS_RESOURCE_CSA, hProfile, Intent, dwFlags, mem);

    // Get rid of memory stream
    cmsCloseIOhandler(ContextID, mem);

    return dwBytesUsed;

}
