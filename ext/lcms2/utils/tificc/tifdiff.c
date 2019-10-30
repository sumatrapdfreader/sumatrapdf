//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2017 Marti Maria Saguer
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

#include "utils.h"
#include "tiffio.h"


// ------------------------------------------------------------------------

static TIFF *Tiff1, *Tiff2, *TiffDiff;
static const char* TiffDiffFilename;
static const char* CGATSout;

typedef struct {
                double  n, x, x2;
                double  Min, Peak;

    } STAT, *LPSTAT;


static STAT ColorantStat[4];
static STAT EuclideanStat;
static STAT ColorimetricStat;

static uint16 Channels;

static cmsHPROFILE hLab;


static
void ConsoleWarningHandler(const char* module, const char* fmt, va_list ap)
{
        char e[512] = { '\0' };
        if (module != NULL)
              strcat(strcpy(e, module), ": ");

        vsprintf(e+strlen(e), fmt, ap);
        strcat(e, ".");
        if (Verbose) {

              fprintf(stderr, "\nWarning");
              fprintf(stderr, " %s\n", e);
              fflush(stderr);
              }
}

static
void ConsoleErrorHandler(const char* module, const char* fmt, va_list ap)
{
       char e[512] = { '\0' };

       if (module != NULL)
              strcat(strcpy(e, module), ": ");

       vsprintf(e+strlen(e), fmt, ap);
       strcat(e, ".");
       fprintf(stderr, "\nError");
       fprintf(stderr, " %s\n", e);
       fflush(stderr);
}



static
void Help()
{
    fprintf(stderr, "Little cms TIFF compare utility. v1.0\n\n");

    fprintf(stderr, "usage: tiffdiff [flags] input.tif output.tif\n");

    fprintf(stderr, "\nflags:\n\n");


    fprintf(stderr, "%co<tiff>   - Output TIFF file\n", SW);
    fprintf(stderr, "%cg<CGATS>  - Output results in CGATS file\n", SW);

    fprintf(stderr, "\n");

    fprintf(stderr, "%cv - Verbose (show warnings)\n", SW);
    fprintf(stderr, "%ch - This help\n", SW);


    fflush(stderr);
    exit(0);
}



// The toggles stuff

static
void HandleSwitches(int argc, char *argv[])
{
       int s;

       while ((s=xgetopt(argc,argv,"o:O:hHvVg:G:")) != EOF) {

       switch (s) {


       case 'v':
       case 'V':
            Verbose = TRUE;
            break;

       case 'o':
       case 'O':
           TiffDiffFilename  = xoptarg;
           break;


        case 'H':
        case 'h':
            Help();
            break;

        case 'g':
        case 'G':
            CGATSout = xoptarg;
            break;

  default:

       FatalError("Unknown option - run without args to see valid ones");
    }
    }
}


static
void ClearStatistics(LPSTAT st)
{

    st ->n = st ->x = st->x2 = st->Peak = 0;
    st ->Min = 1E10;

}


static
void AddOnePixel(LPSTAT st, double dE)
{

    st-> x += dE; st ->x2 += (dE * dE); st->n  += 1.0;
    if (dE > st ->Peak) st ->Peak = dE;
    if (dE < st ->Min)  st ->Min= dE;
}

static
double Std(LPSTAT st)
{
    return sqrt((st->n * st->x2 - st->x * st->x) / (st->n*(st->n-1)));
}

static
double Mean(LPSTAT st)
{
    return st ->x/st ->n;
}


// Build up the pixeltype descriptor

static
cmsUInt32Number GetInputPixelType(TIFF *Bank)
{
     uint16 Photometric, bps, spp, extra, PlanarConfig, *info;
     uint16 Compression, reverse = 0;
     int ColorChannels, IsPlanar = 0, pt = 0;

     TIFFGetField(Bank,           TIFFTAG_PHOTOMETRIC,   &Photometric);
     TIFFGetFieldDefaulted(Bank,  TIFFTAG_BITSPERSAMPLE, &bps);

     if (bps == 1)
       FatalError("Sorry, bilevel TIFFs has nothig to do with ICC profiles");

     if (bps != 8 && bps != 16)
              FatalError("Sorry, 8 or 16 bits per sample only");

     TIFFGetFieldDefaulted(Bank, TIFFTAG_SAMPLESPERPIXEL, &spp);
     TIFFGetFieldDefaulted(Bank, TIFFTAG_PLANARCONFIG, &PlanarConfig);

     switch (PlanarConfig)
     {
     case PLANARCONFIG_CONTIG: IsPlanar = 0; break;
     case PLANARCONFIG_SEPARATE: FatalError("Planar TIFF are not supported");
     default:

     FatalError("Unsupported planar configuration (=%d) ", (int) PlanarConfig);
     }

     // If Samples per pixel == 1, PlanarConfiguration is irrelevant and need
     // not to be included.

     if (spp == 1) IsPlanar = 0;


     // Any alpha?

     TIFFGetFieldDefaulted(Bank, TIFFTAG_EXTRASAMPLES, &extra, &info);


     ColorChannels = spp - extra;

     switch (Photometric) {

     case PHOTOMETRIC_MINISWHITE:

            reverse = 1;

     case PHOTOMETRIC_MINISBLACK:

            pt = PT_GRAY;
            break;

     case PHOTOMETRIC_RGB:

            pt = PT_RGB;
            break;


     case PHOTOMETRIC_PALETTE:

            FatalError("Sorry, palette images not supported (at least on this version)");

     case PHOTOMETRIC_SEPARATED:
           pt = PixelTypeFromChanCount(ColorChannels);
           break;

     case PHOTOMETRIC_YCBCR:
           TIFFGetField(Bank, TIFFTAG_COMPRESSION, &Compression);
           {
                  uint16 subx, suby;

                  pt = PT_YCbCr;
                  TIFFGetFieldDefaulted(Bank, TIFFTAG_YCBCRSUBSAMPLING, &subx, &suby);
                  if (subx != 1 || suby != 1)
                         FatalError("Sorry, subsampled images not supported");

           }
           break;

     case 9:
     case PHOTOMETRIC_CIELAB:
           pt = PT_Lab;
           break;


     case PHOTOMETRIC_LOGLUV:      /* CIE Log2(L) (u',v') */

           TIFFSetField(Bank, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_16BIT);
           pt = PT_YUV;             // *ICCSpace = icSigLuvData;
           bps = 16;               // 16 bits forced by LibTiff
           break;

     default:
           FatalError("Unsupported TIFF color space (Photometric %d)", Photometric);
     }

     // Convert bits per sample to bytes per sample

     bps >>= 3;

     return (COLORSPACE_SH(pt)|PLANAR_SH(IsPlanar)|EXTRA_SH(extra)|CHANNELS_SH(ColorChannels)|BYTES_SH(bps)|FLAVOR_SH(reverse));
}



static
cmsUInt32Number OpenEmbedded(TIFF* tiff, cmsHPROFILE* PtrProfile, cmsHTRANSFORM* PtrXform)
{

    cmsUInt32Number EmbedLen, dwFormat = 0;
    cmsUInt8Number* EmbedBuffer;

    *PtrProfile = NULL;
    *PtrXform   = NULL;

    if (TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &EmbedLen, &EmbedBuffer)) {

              *PtrProfile = cmsOpenProfileFromMem(EmbedBuffer, EmbedLen);

              if (Verbose) {

				  fprintf(stdout, "Embedded profile found:\n");
				  PrintProfileInformation(NULL, *PtrProfile);

              }

              dwFormat  = GetInputPixelType(tiff);
              *PtrXform = cmsCreateTransform(*PtrProfile, dwFormat,
                                          hLab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);

      }

    return dwFormat;
}


static
size_t PixelSize(cmsUInt32Number dwFormat)
{
    return T_BYTES(dwFormat) * (T_CHANNELS(dwFormat) + T_EXTRA(dwFormat));
}


static
int CmpImages(TIFF* tiff1, TIFF* tiff2, TIFF* diff)
{
    cmsUInt8Number* buf1, *buf2, *buf3=NULL;
    int row, cols, imagewidth = 0, imagelength = 0;
    uint16   Photometric;
    double dE = 0;
    double dR, dG, dB, dC, dM, dY, dK;
    int rc = 0;
    cmsHPROFILE hProfile1 = 0, hProfile2 = 0;
    cmsHTRANSFORM xform1 = 0, xform2 = 0;
    cmsUInt32Number dwFormat1, dwFormat2;



      TIFFGetField(tiff1, TIFFTAG_PHOTOMETRIC, &Photometric);
      TIFFGetField(tiff1, TIFFTAG_IMAGEWIDTH,  &imagewidth);
      TIFFGetField(tiff1, TIFFTAG_IMAGELENGTH, &imagelength);
      TIFFGetField(tiff1, TIFFTAG_SAMPLESPERPIXEL, &Channels);

      dwFormat1 = OpenEmbedded(tiff1, &hProfile1, &xform1);
      dwFormat2 = OpenEmbedded(tiff2, &hProfile2, &xform2);



      buf1 = (cmsUInt8Number*)_TIFFmalloc(TIFFScanlineSize(tiff1));
      buf2 = (cmsUInt8Number*)_TIFFmalloc(TIFFScanlineSize(tiff2));

      if (diff) {

           TIFFSetField(diff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
           TIFFSetField(diff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
           TIFFSetField(diff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

           TIFFSetField(diff, TIFFTAG_IMAGEWIDTH,  imagewidth);
           TIFFSetField(diff, TIFFTAG_IMAGELENGTH, imagelength);

           TIFFSetField(diff, TIFFTAG_SAMPLESPERPIXEL, 1);
           TIFFSetField(diff, TIFFTAG_BITSPERSAMPLE, 8);

           buf3 = (cmsUInt8Number*)_TIFFmalloc(TIFFScanlineSize(diff));
      }



      for (row = 0; row < imagelength; row++) {

        if (TIFFReadScanline(tiff1, buf1, row, 0) < 0) goto Error;
        if (TIFFReadScanline(tiff2, buf2, row, 0) < 0) goto Error;


        for (cols = 0; cols < imagewidth; cols++) {


            switch (Photometric) {

            case PHOTOMETRIC_MINISWHITE:
            case PHOTOMETRIC_MINISBLACK:

                    dE = fabs(buf2[cols] - buf1[cols]);

                    AddOnePixel(&ColorantStat[0], dE);
                    AddOnePixel(&EuclideanStat, dE);
                    break;

            case PHOTOMETRIC_RGB:

                    {
                        int index = 3 * cols;

                        dR = fabs(buf2[index+0] - buf1[index+0]);
                        dG = fabs(buf2[index+1] - buf1[index+1]);
                        dB = fabs(buf2[index+2] - buf1[index+2]);

                        dE = sqrt(dR * dR + dG * dG + dB * dB) / sqrt(3.);
                    }

                    AddOnePixel(&ColorantStat[0], dR);
                    AddOnePixel(&ColorantStat[1], dG);
                    AddOnePixel(&ColorantStat[2], dB);
                    AddOnePixel(&EuclideanStat,   dE);
                    break;

            case PHOTOMETRIC_SEPARATED:

                {
                        int index = 4 * cols;

                        dC = fabs(buf2[index+0] - buf1[index+0]);
                        dM = fabs(buf2[index+1] - buf1[index+1]);
                        dY = fabs(buf2[index+2] - buf1[index+2]);
                        dK = fabs(buf2[index+3] - buf1[index+3]);

                        dE = sqrt(dC * dC + dM * dM + dY * dY + dK * dK) / 2.;
                    }
                    AddOnePixel(&ColorantStat[0], dC);
                    AddOnePixel(&ColorantStat[1], dM);
                    AddOnePixel(&ColorantStat[2], dY);
                    AddOnePixel(&ColorantStat[3], dK);
                    AddOnePixel(&EuclideanStat,   dE);
                    break;

            default:
                    FatalError("Unsupported channels: %d", Channels);
            }


            if (xform1 && xform2) {


                cmsCIELab Lab1, Lab2;
                size_t index1 = cols * PixelSize(dwFormat1);
                size_t index2 = cols * PixelSize(dwFormat2);

                cmsDoTransform(NULL, xform1, &buf1[index1], &Lab1,  1);
                cmsDoTransform(NULL, xform2, &buf2[index2], &Lab2,  1);

                dE = cmsDeltaE(NULL, &Lab1, &Lab2);
                AddOnePixel(&ColorimetricStat, dE);
            }


            if (diff) {
                buf3[cols] = (cmsUInt8Number) floor(dE + 0.5);
        }

        }

        if (diff) {

                if (TIFFWriteScanline(diff, buf3, row, 0) < 0) goto Error;
        }


      }

     rc = 1;

Error:

     if (hProfile1) cmsCloseProfile(NULL, hProfile1);
     if (hProfile2) cmsCloseProfile(NULL, hProfile2);
     if (xform1) cmsDeleteTransform(NULL, xform1);
     if (xform2) cmsDeleteTransform(NULL, xform2);
      _TIFFfree(buf1); _TIFFfree(buf2);
      if (diff) {
           TIFFWriteDirectory(diff);
          if (buf3 != NULL) _TIFFfree(buf3);
      }
      return rc;
}


static
void AssureShortTagIs(TIFF* tif1, TIFF* tiff2, int tag, int Val, const char* Error)
{
        uint16 v1;


        if (!TIFFGetField(tif1, tag, &v1)) goto Err;
        if (v1 != Val) goto Err;

        if (!TIFFGetField(tiff2, tag, &v1)) goto Err;
        if (v1 != Val) goto Err;

        return;
Err:
        FatalError("%s is not proper", Error);
}


static
int CmpShortTag(TIFF* tif1, TIFF* tif2, int tag)
{
        uint16 v1, v2;

        if (!TIFFGetField(tif1, tag, &v1)) return 0;
        if (!TIFFGetField(tif2, tag, &v2)) return 0;

        return v1 == v2;
}

static
int CmpLongTag(TIFF* tif1, TIFF* tif2, int tag)
{
        uint32 v1, v2;

        if (!TIFFGetField(tif1, tag, &v1)) return 0;
        if (!TIFFGetField(tif2, tag, &v2)) return 0;

        return v1 == v2;
}


static
void EqualShortTag(TIFF* tif1, TIFF* tif2, int tag, const char* Error)
{
    if (!CmpShortTag(tif1, tif2, tag))
        FatalError("%s is different", Error);
}



static
void EqualLongTag(TIFF* tif1, TIFF* tif2, int tag, const char* Error)
{
    if (!CmpLongTag(tif1, tif2, tag))
        FatalError("%s is different", Error);
}



static
void AddOneCGATSRow(cmsHANDLE hIT8, char *Name, LPSTAT st)
{

    double Per100 = 100.0 * ((255.0 - Mean(st)) / 255.0);

    cmsIT8SetData(NULL, hIT8,    Name, "SAMPLE_ID", Name);
    cmsIT8SetDataDbl(NULL, hIT8, Name, "PER100_EQUAL", Per100);
    cmsIT8SetDataDbl(NULL, hIT8, Name, "MEAN_DE", Mean(st));
    cmsIT8SetDataDbl(NULL, hIT8, Name, "STDEV_DE", Std(st));
    cmsIT8SetDataDbl(NULL, hIT8, Name, "MIN_DE", st ->Min);
    cmsIT8SetDataDbl(NULL, hIT8, Name, "MAX_DE", st ->Peak);

}


static
void CreateCGATS(const char* TiffName1, const char* TiffName2)
{
    cmsHANDLE hIT8 = cmsIT8Alloc(0);
    time_t ltime;
    char Buffer[256];

    cmsIT8SetSheetType(NULL, hIT8, "TIFFDIFF");


    sprintf(Buffer, "Differences between %s and %s", TiffName1, TiffName2);

    cmsIT8SetComment(NULL, hIT8, Buffer);

    cmsIT8SetPropertyStr(NULL, hIT8, "ORIGINATOR", "TIFFDIFF");
    time( &ltime );
    strcpy(Buffer, ctime(&ltime));
    Buffer[strlen(Buffer)-1] = 0;     // Remove the nasty "\n"

    cmsIT8SetPropertyStr(NULL, hIT8, "CREATED", Buffer);

    cmsIT8SetComment(NULL, hIT8, " ");

    cmsIT8SetPropertyDbl(NULL, hIT8, "NUMBER_OF_FIELDS", 6);


    cmsIT8SetDataFormat(NULL, hIT8, 0, "SAMPLE_ID");
    cmsIT8SetDataFormat(NULL, hIT8, 1, "PER100_EQUAL");
    cmsIT8SetDataFormat(NULL, hIT8, 2, "MEAN_DE");
    cmsIT8SetDataFormat(NULL, hIT8, 3, "STDEV_DE");
    cmsIT8SetDataFormat(NULL, hIT8, 4, "MIN_DE");
    cmsIT8SetDataFormat(NULL, hIT8, 5, "MAX_DE");


    switch (Channels) {

    case 1:
            cmsIT8SetPropertyDbl(NULL, hIT8, "NUMBER_OF_SETS", 3);
            AddOneCGATSRow(hIT8, "GRAY_PLANE", &ColorantStat[0]);
            break;

    case 3:
            cmsIT8SetPropertyDbl(NULL, hIT8, "NUMBER_OF_SETS", 5);
            AddOneCGATSRow(hIT8, "R_PLANE", &ColorantStat[0]);
            AddOneCGATSRow(hIT8, "G_PLANE", &ColorantStat[1]);
            AddOneCGATSRow(hIT8, "B_PLANE", &ColorantStat[2]);
            break;


    case 4:
            cmsIT8SetPropertyDbl(NULL, hIT8, "NUMBER_OF_SETS", 6);
            AddOneCGATSRow(hIT8, "C_PLANE", &ColorantStat[0]);
            AddOneCGATSRow(hIT8, "M_PLANE", &ColorantStat[1]);
            AddOneCGATSRow(hIT8, "Y_PLANE", &ColorantStat[2]);
            AddOneCGATSRow(hIT8, "K_PLANE", &ColorantStat[3]);
            break;

    default: FatalError("Internal error: Bad ColorSpace");

    }

    AddOneCGATSRow(hIT8, "EUCLIDEAN",    &EuclideanStat);
    AddOneCGATSRow(hIT8, "COLORIMETRIC", &ColorimetricStat);

    cmsIT8SaveToFile(NULL, hIT8, CGATSout);
    cmsIT8Free(NULL, hIT8);
}

int main(int argc, char* argv[])
{
      int i;

      Tiff1 = Tiff2 = TiffDiff = NULL;

      InitUtils(NULL, "tiffdiff");

      HandleSwitches(argc, argv);

      if ((argc - xoptind) != 2) {

              Help();
              }

      TIFFSetErrorHandler(ConsoleErrorHandler);
      TIFFSetWarningHandler(ConsoleWarningHandler);

      Tiff1 = TIFFOpen(argv[xoptind], "r");
      if (Tiff1 == NULL) FatalError("Unable to open '%s'", argv[xoptind]);

      Tiff2 = TIFFOpen(argv[xoptind+1], "r");
      if (Tiff2 == NULL) FatalError("Unable to open '%s'", argv[xoptind+1]);

      if (TiffDiffFilename) {

          TiffDiff = TIFFOpen(TiffDiffFilename, "w");
          if (TiffDiff == NULL) FatalError("Unable to create '%s'", TiffDiffFilename);

      }


      AssureShortTagIs(Tiff1, Tiff2, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG, "Planar Config");
      AssureShortTagIs(Tiff1, Tiff2, TIFFTAG_BITSPERSAMPLE, 8, "8 bit per sample");

      EqualLongTag(Tiff1, Tiff2, TIFFTAG_IMAGEWIDTH,  "Image width");
      EqualLongTag(Tiff1, Tiff2, TIFFTAG_IMAGELENGTH, "Image length");

      EqualShortTag(Tiff1, Tiff2, TIFFTAG_SAMPLESPERPIXEL, "Samples per pixel");


      hLab = cmsCreateLab4Profile(NULL);

      ClearStatistics(&EuclideanStat);
      for (i=0; i < 4; i++)
            ClearStatistics(&ColorantStat[i]);

      if (!CmpImages(Tiff1, Tiff2, TiffDiff))
                FatalError("Error comparing images");

      if (CGATSout) {
            CreateCGATS(argv[xoptind], argv[xoptind+1]);
      }
      else {

        double  Per100 = 100.0 * ((255.0 - Mean(&EuclideanStat)) / 255.0);

        printf("Digital counts  %g%% equal. mean %g, min %g, max %g, Std %g\n", Per100, Mean(&EuclideanStat),
                                                                                EuclideanStat.Min,
                                                                                EuclideanStat.Peak,
                                                                                Std(&EuclideanStat));

        if (ColorimetricStat.n > 0) {

            Per100 = 100.0 * ((255.0 - Mean(&ColorimetricStat)) / 255.0);

            printf("dE Colorimetric %g%% equal. mean %g, min %g, max %g, Std %g\n", Per100, Mean(&ColorimetricStat),
                                                                                    ColorimetricStat.Min,
                                                                                    ColorimetricStat.Peak,
                                                                                    Std(&ColorimetricStat));
        }

      }

      if (hLab)     cmsCloseProfile(NULL, hLab);
      if (Tiff1)    TIFFClose(Tiff1);
      if (Tiff2)    TIFFClose(Tiff2);
      if (TiffDiff) TIFFClose(TiffDiff);

      return 0;
}


