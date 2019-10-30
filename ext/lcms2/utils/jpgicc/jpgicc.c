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

// This program does apply profiles to (some) JPEG files


#include "utils.h"

#include "jpeglib.h"
#include "iccjpeg.h"

// Flags
static cmsBool BlackPointCompensation = FALSE;
static cmsBool IgnoreEmbedded         = FALSE;
static cmsBool GamutCheck             = FALSE;
static cmsBool lIsITUFax              = FALSE;
static cmsBool lIsPhotoshopApp13      = FALSE;
static cmsBool lIsEXIF;
static cmsBool lIsDeviceLink          = FALSE;
static cmsBool EmbedProfile           = FALSE;

static const char* SaveEmbedded = NULL;

static int Intent                  = INTENT_PERCEPTUAL;
static int ProofingIntent          = INTENT_PERCEPTUAL;
static int PrecalcMode             = 1;

static int jpegQuality             = 75;

static cmsFloat64Number ObserverAdaptationState = 0;


static char *cInpProf  = NULL;
static char *cOutProf  = NULL;
static char *cProofing = NULL;

static FILE * InFile;
static FILE * OutFile;

static struct jpeg_decompress_struct Decompressor;
static struct jpeg_compress_struct   Compressor;


static struct my_error_mgr {

    struct  jpeg_error_mgr pub;   // "public" fields
    void*   Cargo;                // "private" fields

} ErrorHandler;


cmsUInt16Number Alarm[4] = {128,128,128,0};


static
void my_error_exit (j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];

  (*cinfo->err->format_message) (cinfo, buffer);
  FatalError(buffer);
}

/*
Definition of the APPn Markers Defined for continuous-tone G3FAX

The application code APP1 initiates identification of the image as
a G3FAX application and defines the spatial resolution and subsampling.
This marker directly follows the SOI marker. The data format will be as follows:

X'FFE1' (APP1), length, FAX identifier, version, spatial resolution.

The above terms are defined as follows:

Length: (Two octets) Total APP1 field octet count including the octet count itself, but excluding the APP1
marker.

FAX identifier: (Six octets) X'47', X'33', X'46', X'41', X'58', X'00'. This X'00'-terminated string "G3FAX"
uniquely identifies this APP1 marker.

Version: (Two octets) X'07CA'. This string specifies the year of approval of the standard, for identification
in the case of future revision (for example, 1994).

Spatial Resolution: (Two octets) Lightness pixel density in pels/25.4 mm. The basic value is 200. Allowed values are
100, 200, 300, 400, 600 and 1200 pels/25.4 mm, with square (or equivalent) pels.

NOTE - The functional equivalence of inch-based and mm-based resolutions is maintained. For example, the 200 x 200
*/

static
cmsBool IsITUFax(jpeg_saved_marker_ptr ptr)
{
    while (ptr)
    {
        if (ptr -> marker == (JPEG_APP0 + 1) && ptr -> data_length > 5) {

            const char* data = (const char*) ptr -> data;

            if (strcmp(data, "G3FAX") == 0) return TRUE;
        }

        ptr = ptr -> next;
    }

    return FALSE;
}

// Save a ITU T.42/Fax marker with defaults on boundaries. This is the only mode we support right now.
static
void SetITUFax(j_compress_ptr cinfo)
{
    unsigned char Marker[] = "G3FAX\x00\0x07\xCA\x00\xC8";

    jpeg_write_marker(cinfo, (JPEG_APP0 + 1), Marker, 10);
}


// Build a profile for decoding ITU T.42/Fax JPEG streams.
// The profile has an additional ability in the input direction of
// gamut compress values between 85 < a < -85 and -75 < b < 125. This conforms
// the default range for ITU/T.42 -- See RFC 2301, section 6.2.3 for details

//  L*  =   [0, 100]
//  a*  =   [-85, 85]
//  b*  =   [-75, 125]


// These functions does convert the encoding of ITUFAX to floating point
// and vice-versa. No gamut mapping is performed yet.

static
void ITU2Lab(const cmsUInt16Number In[3], cmsCIELab* Lab)
{
    Lab -> L = (double) In[0] / 655.35;
    Lab -> a = (double) 170.* (In[1] - 32768.) / 65535.;
    Lab -> b = (double) 200.* (In[2] - 24576.) / 65535.;
}

static
void Lab2ITU(const cmsCIELab* Lab, cmsUInt16Number Out[3])
{
    Out[0] = (cmsUInt16Number) floor((double) (Lab -> L / 100.)* 65535. );
    Out[1] = (cmsUInt16Number) floor((double) (Lab -> a / 170.)* 65535. + 32768. );
    Out[2] = (cmsUInt16Number) floor((double) (Lab -> b / 200.)* 65535. + 24576. );
}

// These are the samplers-- They are passed as callbacks to cmsStageSampleCLut16bit()
// then, cmsSample3DGrid() will sweel whole Lab gamut calling these functions
// once for each node. In[] will contain the Lab PCS value to convert to ITUFAX
// on PCS2ITU, or the ITUFAX value to convert to Lab in ITU2PCS
// You can change the number of sample points if desired, the algorithm will
// remain same. 33 points gives good accuracy, but you can reduce to 22 or less
// is space is critical

#define GRID_POINTS 33

static
int PCS2ITU(cmsContext ContextID, register const cmsUInt16Number In[], register cmsUInt16Number Out[], register void*  Cargo)
{
    cmsCIELab Lab;

    cmsLabEncoded2Float(NULL, &Lab, In);
    cmsDesaturateLab(NULL, &Lab, 85, -85, 125, -75);    // This function does the necessary gamut remapping
    Lab2ITU(&Lab, Out);
    return TRUE;

    UTILS_UNUSED_PARAMETER(Cargo);
    UTILS_UNUSED_PARAMETER(ContextID);
}


static
int ITU2PCS(cmsContext ContextID, register const cmsUInt16Number In[], register cmsUInt16Number Out[], register void*  Cargo)
{
    cmsCIELab Lab;

    ITU2Lab(In, &Lab);
    cmsFloat2LabEncoded(NULL, Out, &Lab);
    return TRUE;

    UTILS_UNUSED_PARAMETER(Cargo);
    UTILS_UNUSED_PARAMETER(ContextID);
}

// This function does create the virtual input profile, which decodes ITU to the profile connection space
static
cmsHPROFILE CreateITU2PCS_ICC(void)
{
    cmsHPROFILE hProfile;
    cmsPipeline* AToB0;
    cmsStage* ColorMap;

    AToB0 = cmsPipelineAlloc(0, 3, 3);
    if (AToB0 == NULL) return NULL;

    ColorMap = cmsStageAllocCLut16bit(0, GRID_POINTS, 3, 3, NULL);
    if (ColorMap == NULL) return NULL;

    cmsPipelineInsertStage(NULL, AToB0, cmsAT_BEGIN, ColorMap);
    cmsStageSampleCLut16bit(NULL, ColorMap, ITU2PCS, NULL, 0);

    hProfile = cmsCreateProfilePlaceholder(0);
    if (hProfile == NULL) {
        cmsPipelineFree(NULL, AToB0);
        return NULL;
    }

    cmsWriteTag(NULL, hProfile, cmsSigAToB0Tag, AToB0);
    cmsSetColorSpace(NULL, hProfile, cmsSigLabData);
    cmsSetPCS(NULL, hProfile, cmsSigLabData);
    cmsSetDeviceClass(NULL, hProfile, cmsSigColorSpaceClass);
    cmsPipelineFree(NULL, AToB0);

    return hProfile;
}


// This function does create the virtual output profile, with the necessary gamut mapping
static
cmsHPROFILE CreatePCS2ITU_ICC(void)
{
    cmsHPROFILE hProfile;
    cmsPipeline* BToA0;
    cmsStage* ColorMap;

    BToA0 = cmsPipelineAlloc(0, 3, 3);
    if (BToA0 == NULL) return NULL;

    ColorMap = cmsStageAllocCLut16bit(0, GRID_POINTS, 3, 3, NULL);
    if (ColorMap == NULL) return NULL;

    cmsPipelineInsertStage(NULL, BToA0, cmsAT_BEGIN, ColorMap);
    cmsStageSampleCLut16bit(NULL, ColorMap, PCS2ITU, NULL, 0);

    hProfile = cmsCreateProfilePlaceholder(0);
    if (hProfile == NULL) {
        cmsPipelineFree(NULL, BToA0);
        return NULL;
    }

    cmsWriteTag(NULL, hProfile, cmsSigBToA0Tag, BToA0);
    cmsSetColorSpace(NULL, hProfile, cmsSigLabData);
    cmsSetPCS(NULL, hProfile, cmsSigLabData);
    cmsSetDeviceClass(NULL, hProfile, cmsSigColorSpaceClass);

    cmsPipelineFree(NULL, BToA0);

    return hProfile;
}



#define PS_FIXED_TO_FLOAT(h, l) ((float) (h) + ((float) (l)/(1<<16)))

static
cmsBool ProcessPhotoshopAPP13(JOCTET FAR *data, int datalen)
{
    int i;

    for (i = 14; i < datalen; )
    {
        long len;
        unsigned int type;

        if (!(GETJOCTET(data[i]  ) == 0x38 &&
              GETJOCTET(data[i+1]) == 0x42 &&
              GETJOCTET(data[i+2]) == 0x49 &&
              GETJOCTET(data[i+3]) == 0x4D)) break; // Not recognized

        i += 4; // identifying string

        type = (unsigned int) (GETJOCTET(data[i]<<8) + GETJOCTET(data[i+1]));

        i += 2; // resource type

        i += GETJOCTET(data[i]) + ((GETJOCTET(data[i]) & 1) ? 1 : 2);   // resource name

        len = ((((GETJOCTET(data[i]<<8) + GETJOCTET(data[i+1]))<<8) +
                         GETJOCTET(data[i+2]))<<8) + GETJOCTET(data[i+3]);

        i += 4; // Size

        if (type == 0x03ED && len >= 16) {

            Decompressor.X_density = (UINT16) PS_FIXED_TO_FLOAT(GETJOCTET(data[i]<<8) + GETJOCTET(data[i+1]),
                                                 GETJOCTET(data[i+2]<<8) + GETJOCTET(data[i+3]));
            Decompressor.Y_density = (UINT16) PS_FIXED_TO_FLOAT(GETJOCTET(data[i+8]<<8) + GETJOCTET(data[i+9]),
                                                 GETJOCTET(data[i+10]<<8) + GETJOCTET(data[i+11]));

            // Set the density unit to 1 since the
            // Vertical and Horizontal resolutions
            // are specified in Pixels per inch

            Decompressor.density_unit = 0x01;
            return TRUE;

        }

        i += len + ((len & 1) ? 1 : 0);   // Alignment
    }
    return FALSE;
}


static
cmsBool HandlePhotoshopAPP13(jpeg_saved_marker_ptr ptr)
{
    while (ptr) {

        if (ptr -> marker == (JPEG_APP0 + 13) && ptr -> data_length > 9)
        {
            JOCTET FAR* data = ptr -> data;

            if(GETJOCTET(data[0]) == 0x50 &&
               GETJOCTET(data[1]) == 0x68 &&
               GETJOCTET(data[2]) == 0x6F &&
               GETJOCTET(data[3]) == 0x74 &&
               GETJOCTET(data[4]) == 0x6F &&
               GETJOCTET(data[5]) == 0x73 &&
               GETJOCTET(data[6]) == 0x68 &&
               GETJOCTET(data[7]) == 0x6F &&
               GETJOCTET(data[8]) == 0x70) {

                ProcessPhotoshopAPP13(data, ptr -> data_length);
                return TRUE;
            }
        }

        ptr = ptr -> next;
    }

    return FALSE;
}


typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define INTEL_BYTE_ORDER 0x4949
#define XRESOLUTION 0x011a
#define YRESOLUTION 0x011b
#define RESOLUTION_UNIT 0x128

// Read a 16-bit word
static
uint16_t read16(uint8_t* arr, int pos,  int swapBytes)
{
    uint8_t b1 = arr[pos];
    uint8_t b2 = arr[pos+1];

    return (swapBytes) ?  ((b2 << 8) | b1) : ((b1 << 8) | b2);
}


// Read a 32-bit word
static
uint32_t read32(uint8_t* arr, int pos,  int swapBytes)
{

    if(!swapBytes) {

        return (arr[pos]   << 24) |
               (arr[pos+1] << 16) |
               (arr[pos+2] << 8) |
                arr[pos+3];
    }

    return arr[pos] |
           (arr[pos+1] << 8) |
           (arr[pos+2] << 16) |
           (arr[pos+3] << 24);
}



static
int read_tag(uint8_t* arr, int pos,  int swapBytes, void* dest)
{
        // Format should be 5 over here (rational)
    uint32_t format = read16(arr, pos + 2, swapBytes);
    // Components should be 1
    uint32_t components = read32(arr, pos + 4, swapBytes);
    // Points to the value
    uint32_t offset;

    // sanity
    if (components != 1) return 0;

    if (format == 3)
        offset = pos + 8;
    else
        offset =  read32(arr, pos + 8, swapBytes);

    switch (format) {

    case 5: // Rational
          {
          double num = read32(arr, offset, swapBytes);
          double den = read32(arr, offset + 4, swapBytes);
          *(double *) dest = num / den;
          }
          break;

    case 3: // uint 16
        *(int*) dest = read16(arr, offset, swapBytes);
        break;

    default:  return 0;
    }

    return 1;
}



// Handler for EXIF data
static
    cmsBool HandleEXIF(struct jpeg_decompress_struct* cinfo)
{
    jpeg_saved_marker_ptr ptr;
    uint32_t ifd_ofs;
    int pos = 0, swapBytes = 0;
    uint32_t i, numEntries;
    double XRes = -1, YRes = -1;
    int Unit = 2; // Inches


    for (ptr = cinfo ->marker_list; ptr; ptr = ptr ->next) {

        if ((ptr ->marker == JPEG_APP0+1) && ptr ->data_length > 6) {
            JOCTET FAR* data = ptr -> data;

            if (memcmp(data, "Exif\0\0", 6) == 0) {

                data += 6; // Skip EXIF marker

                // 8 byte TIFF header
                // first two determine byte order
                pos = 0;
                if (read16(data, pos, 0) == INTEL_BYTE_ORDER) {
                    swapBytes = 1;
                }

                pos += 2;

                // next two bytes are always 0x002A (TIFF version)
                pos += 2;

                // offset to Image File Directory (includes the previous 8 bytes)
                ifd_ofs = read32(data, pos, swapBytes);

                // Search the directory for resolution tags
                numEntries = read16(data, ifd_ofs, swapBytes);

                for (i=0; i < numEntries; i++) {

                    uint32_t entryOffset = ifd_ofs + 2 + (12 * i);
                    uint32_t tag = read16(data, entryOffset, swapBytes);

                    switch (tag) {

                    case RESOLUTION_UNIT:
                        if (!read_tag(data, entryOffset, swapBytes, &Unit)) return FALSE;
                        break;

                    case XRESOLUTION:
                        if (!read_tag(data, entryOffset, swapBytes, &XRes)) return FALSE;
                        break;

                    case YRESOLUTION:
                        if (!read_tag(data, entryOffset, swapBytes, &YRes)) return FALSE;
                        break;

                    default:;
                    }

                }

                // Proceed if all found

                if (XRes != -1 && YRes != -1)
                {

                    // 1 = None
                    // 2 = inches
                    // 3 = cm

                    switch (Unit) {

                    case 2:

                        cinfo ->X_density = (UINT16) floor(XRes + 0.5);
                        cinfo ->Y_density = (UINT16) floor(YRes + 0.5);
                        break;

                    case 1:

                        cinfo ->X_density = (UINT16) floor(XRes * 2.54 + 0.5);
                        cinfo ->Y_density = (UINT16) floor(YRes * 2.54 + 0.5);
                        break;

                    default: return FALSE;
                    }

                    cinfo ->density_unit = 1;  /* 1 for dots/inch, or 2 for dots/cm.*/

                }


            }
        }
    }
    return FALSE;
}


static
cmsBool OpenInput(const char* FileName)
{
    int m;

    lIsITUFax = FALSE;
    InFile  = fopen(FileName, "rb");
    if (InFile == NULL) {
        FatalError("Cannot open '%s'", FileName);
    }

    // Now we can initialize the JPEG decompression object.
    Decompressor.err                 = jpeg_std_error(&ErrorHandler.pub);
    ErrorHandler.pub.error_exit      = my_error_exit;
    ErrorHandler.pub.output_message  = my_error_exit;

    jpeg_create_decompress(&Decompressor);
    jpeg_stdio_src(&Decompressor, InFile);

    for (m = 0; m < 16; m++)
        jpeg_save_markers(&Decompressor, JPEG_APP0 + m, 0xFFFF);

    // setup_read_icc_profile(&Decompressor);

    fseek(InFile, 0, SEEK_SET);
    jpeg_read_header(&Decompressor, TRUE);

    return TRUE;
}


static
cmsBool OpenOutput(const char* FileName)
{

    OutFile = fopen(FileName, "wb");
    if (OutFile == NULL) {
        FatalError("Cannot create '%s'", FileName);

    }

    Compressor.err                   = jpeg_std_error(&ErrorHandler.pub);
    ErrorHandler.pub.error_exit      = my_error_exit;
    ErrorHandler.pub.output_message  = my_error_exit;

    Compressor.input_components = Compressor.num_components = 4;

    jpeg_create_compress(&Compressor);
    jpeg_stdio_dest(&Compressor, OutFile);
    return TRUE;
}

static
cmsBool Done(void)
{
    jpeg_destroy_decompress(&Decompressor);
    jpeg_destroy_compress(&Compressor);
    return fclose(InFile) + fclose(OutFile);

}


// Build up the pixeltype descriptor

static
cmsUInt32Number GetInputPixelType(void)
{
     int space, bps, extra, ColorChannels, Flavor;

     lIsITUFax         = IsITUFax(Decompressor.marker_list);
     lIsPhotoshopApp13 = HandlePhotoshopAPP13(Decompressor.marker_list);
     lIsEXIF           = HandleEXIF(&Decompressor);

     ColorChannels = Decompressor.num_components;
     extra  = 0;            // Alpha = None
     bps    = 1;            // 8 bits
     Flavor = 0;            // Vanilla

     if (lIsITUFax) {

        space = PT_Lab;
        Decompressor.out_color_space = JCS_YCbCr;  // Fake to don't touch
     }
     else
     switch (Decompressor.jpeg_color_space) {

     case JCS_GRAYSCALE:        // monochrome
              space = PT_GRAY;
              Decompressor.out_color_space = JCS_GRAYSCALE;
              break;

     case JCS_RGB:             // red/green/blue
              space = PT_RGB;
              Decompressor.out_color_space = JCS_RGB;
              break;

     case JCS_YCbCr:               // Y/Cb/Cr (also known as YUV)
              space = PT_RGB;      // Let IJG code to do the conversion
              Decompressor.out_color_space = JCS_RGB;
              break;

     case JCS_CMYK:            // C/M/Y/K
              space = PT_CMYK;
              Decompressor.out_color_space = JCS_CMYK;
              if (Decompressor.saw_Adobe_marker)            // Adobe keeps CMYK inverted, so change flavor
                                Flavor = 1;                 // from vanilla to chocolate
              break;

     case JCS_YCCK:            // Y/Cb/Cr/K
              space = PT_CMYK;
              Decompressor.out_color_space = JCS_CMYK;
              if (Decompressor.saw_Adobe_marker)            // ditto
                                Flavor = 1;
              break;

     default:
              FatalError("Unsupported color space (0x%x)", Decompressor.jpeg_color_space);
              return 0;
     }

     return (EXTRA_SH(extra)|CHANNELS_SH(ColorChannels)|BYTES_SH(bps)|COLORSPACE_SH(space)|FLAVOR_SH(Flavor));
}


// Rearrange pixel type to build output descriptor
static
cmsUInt32Number ComputeOutputFormatDescriptor(cmsUInt32Number dwInput, int OutColorSpace)
{
    int IsPlanar  = T_PLANAR(dwInput);
    int Channels  = 0;
    int Flavor    = 0;

    switch (OutColorSpace) {

   case PT_GRAY:
       Channels = 1;
       break;
   case PT_RGB:
   case PT_CMY:
   case PT_Lab:
   case PT_YUV:
   case PT_YCbCr:
       Channels = 3;
       break;

   case PT_CMYK:
       if (Compressor.write_Adobe_marker)   // Adobe keeps CMYK inverted, so change flavor to chocolate
           Flavor = 1;
       Channels = 4;
       break;
   default:
       FatalError("Unsupported output color space");
    }

    return (COLORSPACE_SH(OutColorSpace)|PLANAR_SH(IsPlanar)|CHANNELS_SH(Channels)|BYTES_SH(1)|FLAVOR_SH(Flavor));
}


// Equivalence between ICC color spaces and lcms color spaces
static
int GetProfileColorSpace(cmsHPROFILE hProfile)
{
    cmsColorSpaceSignature ProfileSpace = cmsGetColorSpace(NULL, hProfile);

    return _cmsLCMScolorSpace(NULL, ProfileSpace);
}

static
int GetDevicelinkColorSpace(cmsHPROFILE hProfile)
{
    cmsColorSpaceSignature ProfileSpace = cmsGetPCS(NULL, hProfile);

    return _cmsLCMScolorSpace(NULL, ProfileSpace);
}


// From TRANSUPP

static
void jcopy_markers_execute(j_decompress_ptr srcinfo, j_compress_ptr dstinfo)
{
  jpeg_saved_marker_ptr marker;

  /* In the current implementation, we don't actually need to examine the
   * option flag here; we just copy everything that got saved.
   * But to avoid confusion, we do not output JFIF and Adobe APP14 markers
   * if the encoder library already wrote one.
   */
  for (marker = srcinfo->marker_list; marker != NULL; marker = marker->next) {

    if (dstinfo->write_JFIF_header &&
        marker->marker == JPEG_APP0 &&
        marker->data_length >= 5 &&
        GETJOCTET(marker->data[0]) == 0x4A &&
        GETJOCTET(marker->data[1]) == 0x46 &&
        GETJOCTET(marker->data[2]) == 0x49 &&
        GETJOCTET(marker->data[3]) == 0x46 &&
        GETJOCTET(marker->data[4]) == 0)
                          continue;         /* reject duplicate JFIF */

    if (dstinfo->write_Adobe_marker &&
        marker->marker == JPEG_APP0+14 &&
        marker->data_length >= 5 &&
        GETJOCTET(marker->data[0]) == 0x41 &&
        GETJOCTET(marker->data[1]) == 0x64 &&
        GETJOCTET(marker->data[2]) == 0x6F &&
        GETJOCTET(marker->data[3]) == 0x62 &&
        GETJOCTET(marker->data[4]) == 0x65)
                         continue;         /* reject duplicate Adobe */

     jpeg_write_marker(dstinfo, marker->marker,
                       marker->data, marker->data_length);
  }
}

static
void WriteOutputFields(int OutputColorSpace)
{
    J_COLOR_SPACE in_space, jpeg_space;
    int components;

    switch (OutputColorSpace) {

    case PT_GRAY: in_space = jpeg_space = JCS_GRAYSCALE;
                  components = 1;
                  break;

    case PT_RGB:  in_space = JCS_RGB;
                  jpeg_space = JCS_YCbCr;
                  components = 3;
                  break;       // red/green/blue

    case PT_YCbCr: in_space = jpeg_space = JCS_YCbCr;
                   components = 3;
                   break;               // Y/Cb/Cr (also known as YUV)

    case PT_CMYK: in_space = JCS_CMYK;
                  jpeg_space = JCS_YCCK;
                  components = 4;
                  break;      // C/M/Y/components

    case PT_Lab:  in_space = jpeg_space = JCS_YCbCr;
                  components = 3;
                  break;                // Fake to don't touch
    default:
                 FatalError("Unsupported output color space");
                 return;
    }


    if (jpegQuality >= 100) {

     // avoid destructive conversion when asking for lossless compression
        jpeg_space = in_space;
    }

    Compressor.in_color_space =  in_space;
    Compressor.jpeg_color_space = jpeg_space;
    Compressor.input_components = Compressor.num_components = components;
    jpeg_set_defaults(&Compressor);
    jpeg_set_colorspace(&Compressor, jpeg_space);


    // Make sure to pass resolution through
    if (OutputColorSpace == PT_CMYK)
        Compressor.write_JFIF_header = 1;

    // Avoid subsampling on high quality factor
    jpeg_set_quality(&Compressor, jpegQuality, 1);
    if (jpegQuality >= 70) {

      int i;
      for(i=0; i < Compressor.num_components; i++) {

            Compressor.comp_info[i].h_samp_factor = 1;
            Compressor.comp_info[i].v_samp_factor = 1;
      }

    }

}


static
void DoEmbedProfile(const char* ProfileFile)
{
    FILE* f;
    size_t size, EmbedLen;
    cmsUInt8Number* EmbedBuffer;

        f = fopen(ProfileFile, "rb");
        if (f == NULL) return;

        size = cmsfilelength(f);
        EmbedBuffer = (cmsUInt8Number*) malloc(size + 1);
        EmbedLen = fread(EmbedBuffer, 1, size, f);
        fclose(f);
        EmbedBuffer[EmbedLen] = 0;

        write_icc_profile (&Compressor, EmbedBuffer, (unsigned int) EmbedLen);
        free(EmbedBuffer);
}



static
int DoTransform(cmsHTRANSFORM hXForm, int OutputColorSpace)
{
    JSAMPROW ScanLineIn;
    JSAMPROW ScanLineOut;


       //Preserve resolution values from the original
       // (Thanks to Robert Bergs for finding out this bug)
       Compressor.density_unit = Decompressor.density_unit;
       Compressor.X_density    = Decompressor.X_density;
       Compressor.Y_density    = Decompressor.Y_density;

      //  Compressor.write_JFIF_header = 1;

       jpeg_start_decompress(&Decompressor);
       jpeg_start_compress(&Compressor, TRUE);

        if (OutputColorSpace == PT_Lab)
            SetITUFax(&Compressor);

       // Embed the profile if needed
       if (EmbedProfile && cOutProf)
           DoEmbedProfile(cOutProf);

       ScanLineIn  = (JSAMPROW) malloc(Decompressor.output_width * Decompressor.num_components);
       ScanLineOut = (JSAMPROW) malloc(Compressor.image_width * Compressor.num_components);

       while (Decompressor.output_scanline <
                            Decompressor.output_height) {

       jpeg_read_scanlines(&Decompressor, &ScanLineIn, 1);

       cmsDoTransform(NULL, hXForm, ScanLineIn, ScanLineOut, Decompressor.output_width);

       jpeg_write_scanlines(&Compressor, &ScanLineOut, 1);
       }

       free(ScanLineIn);
       free(ScanLineOut);

       jpeg_finish_decompress(&Decompressor);
       jpeg_finish_compress(&Compressor);

       return TRUE;
}



// Transform one image

static
int TransformImage(char *cDefInpProf, char *cOutputProf)
{
       cmsHPROFILE hIn, hOut, hProof;
       cmsHTRANSFORM xform;
       cmsUInt32Number wInput, wOutput;
       int OutputColorSpace;
       cmsUInt32Number dwFlags = 0;
       cmsUInt32Number EmbedLen;
       cmsUInt8Number* EmbedBuffer;


       cmsSetAdaptationState(ObserverAdaptationState);

       if (BlackPointCompensation) {

            dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
       }


       switch (PrecalcMode) {

       case 0: dwFlags |= cmsFLAGS_NOOPTIMIZE; break;
       case 2: dwFlags |= cmsFLAGS_HIGHRESPRECALC; break;
       case 3: dwFlags |= cmsFLAGS_LOWRESPRECALC; break;
       default:;
       }


       if (GamutCheck) {
            dwFlags |= cmsFLAGS_GAMUTCHECK;
            cmsSetAlarmCodes(Alarm);
       }

       // Take input color space
       wInput = GetInputPixelType();

        if (lIsDeviceLink) {

            hIn = cmsOpenProfileFromFile(cDefInpProf, "r");
            hOut = NULL;
            hProof = NULL;
       }
        else {

        if (!IgnoreEmbedded && read_icc_profile(&Decompressor, &EmbedBuffer, &EmbedLen))
        {
              hIn = cmsOpenProfileFromMem(EmbedBuffer, EmbedLen);

               if (Verbose) {

                  fprintf(stdout, " (Embedded profile found)\n");
                  PrintProfileInformation(NULL, hIn);
                  fflush(stdout);
              }

               if (hIn != NULL && SaveEmbedded != NULL)
                          SaveMemoryBlock(EmbedBuffer, EmbedLen, SaveEmbedded);

              free(EmbedBuffer);
        }
        else
        {
            // Default for ITU/Fax
            if (cDefInpProf == NULL && T_COLORSPACE(wInput) == PT_Lab)
                cDefInpProf = "*Lab";

            if (cDefInpProf != NULL && cmsstrcasecmp(cDefInpProf, "*lab") == 0)
                hIn = CreateITU2PCS_ICC();
            else
                hIn = OpenStockProfile(0, cDefInpProf);
       }

        if (cOutputProf != NULL && cmsstrcasecmp(cOutputProf, "*lab") == 0)
            hOut = CreatePCS2ITU_ICC();
        else
        hOut = OpenStockProfile(0, cOutputProf);

       hProof = NULL;
       if (cProofing != NULL) {

           hProof = OpenStockProfile(0, cProofing);
           if (hProof == NULL) {
            FatalError("Proofing profile couldn't be read.");
           }
           dwFlags |= cmsFLAGS_SOFTPROOFING;
          }
       }

        if (!hIn)
            FatalError("Input profile couldn't be read.");
        if (!lIsDeviceLink && !hOut)
            FatalError("Output profile couldn't be read.");

       // Assure both, input profile and input JPEG are on same colorspace
       if (cmsGetColorSpace(NULL, hIn) != _cmsICCcolorSpace(NULL, T_COLORSPACE(wInput)))
              FatalError("Input profile is not operating in proper color space");


       // Output colorspace is given by output profile

        if (lIsDeviceLink) {
            OutputColorSpace = GetDevicelinkColorSpace(hIn);
        }
        else {
            OutputColorSpace = GetProfileColorSpace(hOut);
        }

       jpeg_copy_critical_parameters(&Decompressor, &Compressor);

       WriteOutputFields(OutputColorSpace);

       wOutput      = ComputeOutputFormatDescriptor(wInput, OutputColorSpace);


       xform = cmsCreateProofingTransform(hIn, wInput,
                                          hOut, wOutput,
                                          hProof, Intent,
                                          ProofingIntent, dwFlags);
       if (xform == NULL)
                 FatalError("Cannot transform by using the profiles");

       DoTransform(xform, OutputColorSpace);


       jcopy_markers_execute(&Decompressor, &Compressor);

       cmsDeleteTransform(NULL, xform);
       cmsCloseProfile(NULL, hIn);
       cmsCloseProfile(NULL, hOut);
       if (hProof) cmsCloseProfile(NULL, hProof);

       return 1;
}


// Simply print help

static
void Help(int level)
{
     fprintf(stderr, "little cms ICC profile applier for JPEG - v3.2 [LittleCMS %2.2f]\n\n", LCMS_VERSION / 1000.0);

     switch(level) {

     default:
     case 0:

     fprintf(stderr, "usage: jpgicc [flags] input.jpg output.jpg\n");

     fprintf(stderr, "\nflags:\n\n");
     fprintf(stderr, "%cv - Verbose\n", SW);
     fprintf(stderr, "%ci<profile> - Input profile (defaults to sRGB)\n", SW);
     fprintf(stderr, "%co<profile> - Output profile (defaults to sRGB)\n", SW);

     PrintRenderingIntents(NULL);


     fprintf(stderr, "%cb - Black point compensation\n", SW);
     fprintf(stderr, "%cd<0..1> - Observer adaptation state (abs.col. only)\n", SW);
     fprintf(stderr, "%cn - Ignore embedded profile\n", SW);
     fprintf(stderr, "%ce - Embed destination profile\n", SW);
     fprintf(stderr, "%cs<new profile> - Save embedded profile as <new profile>\n", SW);

     fprintf(stderr, "\n");

     fprintf(stderr, "%cc<0,1,2,3> - Precalculates transform (0=Off, 1=Normal, 2=Hi-res, 3=LoRes) [defaults to 1]\n", SW);
     fprintf(stderr, "\n");

     fprintf(stderr, "%cp<profile> - Soft proof profile\n", SW);
     fprintf(stderr, "%cm<0,1,2,3> - SoftProof intent\n", SW);
     fprintf(stderr, "%cg - Marks out-of-gamut colors on softproof\n", SW);
     fprintf(stderr, "%c!<r>,<g>,<b> - Out-of-gamut marker channel values\n", SW);

     fprintf(stderr, "\n");
     fprintf(stderr, "%cq<0..100> - Output JPEG quality\n", SW);

     fprintf(stderr, "\n");
     fprintf(stderr, "%ch<0,1,2,3> - More help\n", SW);
     break;

     case 1:

     fprintf(stderr, "Examples:\n\n"
                     "To color correct from scanner to sRGB:\n"
                     "\tjpgicc %ciscanner.icm in.jpg out.jpg\n"
                     "To convert from monitor1 to monitor2:\n"
                     "\tjpgicc %cimon1.icm %comon2.icm in.jpg out.jpg\n"
                     "To make a CMYK separation:\n"
                     "\tjpgicc %coprinter.icm inrgb.jpg outcmyk.jpg\n"
                     "To recover sRGB from a CMYK separation:\n"
                     "\tjpgicc %ciprinter.icm incmyk.jpg outrgb.jpg\n"
                     "To convert from CIELab ITU/Fax JPEG to sRGB\n"
                     "\tjpgicc in.jpg out.jpg\n\n",
                     SW, SW, SW, SW, SW);
     break;

     case 2:
         PrintBuiltins();
         break;

     case 3:

     fprintf(stderr, "This program is intended to be a demo of the little cms\n"
                     "engine. Both lcms and this program are freeware. You can\n"
                     "obtain both in source code at http://www.littlecms.com\n"
                     "For suggestions, comments, bug reports etc. send mail to\n"
                     "marti@littlecms.com\n\n");
     break;
     }

     exit(0);
}


// The toggles stuff

static
void HandleSwitches(int argc, char *argv[])
{
    int s;

    while ((s=xgetopt(argc,argv,"bBnNvVGgh:H:i:I:o:O:P:p:t:T:c:C:Q:q:M:m:L:l:eEs:S:!:D:d:")) != EOF) {

        switch (s)
        {

        case 'b':
        case 'B':
            BlackPointCompensation = TRUE;
            break;

        case 'd':
        case 'D': ObserverAdaptationState = atof(xoptarg);
            if (ObserverAdaptationState < 0 ||
                ObserverAdaptationState > 1.0)
                FatalError("Adaptation state should be 0..1");
            break;

        case 'v':
        case 'V':
            Verbose = TRUE;
            break;

        case 'i':
        case 'I':
            if (lIsDeviceLink)
                FatalError("Device-link already specified");

            cInpProf = xoptarg;
            break;

        case 'o':
        case 'O':
            if (lIsDeviceLink)
                FatalError("Device-link already specified");

            cOutProf = xoptarg;
            break;

        case 'l':
        case 'L':
            if (cInpProf != NULL || cOutProf != NULL)
                FatalError("input/output profiles already specified");

            cInpProf = xoptarg;
            lIsDeviceLink = TRUE;
            break;

        case 'p':
        case 'P':
            cProofing = xoptarg;
            break;

        case 't':
        case 'T':
            Intent = atoi(xoptarg);
            break;

        case 'N':
        case 'n':
            IgnoreEmbedded = TRUE;
            break;

        case 'e':
        case 'E':
            EmbedProfile = TRUE;
            break;


        case 'g':
        case 'G':
            GamutCheck = TRUE;
            break;

        case 'c':
        case 'C':
            PrecalcMode = atoi(xoptarg);
            if (PrecalcMode < 0 || PrecalcMode > 2)
                FatalError("Unknown precalc mode '%d'", PrecalcMode);
            break;

        case 'H':
        case 'h':  {

            int a =  atoi(xoptarg);
            Help(a);
                   }
            break;

        case 'q':
        case 'Q':
            jpegQuality = atoi(xoptarg);
            if (jpegQuality > 100) jpegQuality = 100;
            if (jpegQuality < 0)   jpegQuality = 0;
            break;

        case 'm':
        case 'M':
            ProofingIntent = atoi(xoptarg);
            break;

        case 's':
        case 'S': SaveEmbedded = xoptarg;
            break;

        case '!':
            if (sscanf(xoptarg, "%hu,%hu,%hu", &Alarm[0], &Alarm[1], &Alarm[2]) == 3) {
                int i;
                for (i=0; i < 3; i++) {
                    Alarm[i] = (Alarm[i] << 8) | Alarm[i];
                }
            }
            break;

        default:

            FatalError("Unknown option - run without args to see valid ones");
        }

    }
}


int main(int argc, char* argv[])
{
    InitUtils(NULL, "jpgicc");

    HandleSwitches(argc, argv);

    if ((argc - xoptind) != 2) {
        Help(0);
    }

    OpenInput(argv[xoptind]);
    OpenOutput(argv[xoptind+1]);

    TransformImage(cInpProf, cOutProf);


    if (Verbose) { fprintf(stdout, "\n"); fflush(stdout); }

    Done();

    return 0;
}



