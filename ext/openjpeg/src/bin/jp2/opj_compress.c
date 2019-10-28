/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * Copyright (c) 2008, Jerome Fimes, Communications & Systemes <jerome.fimes@c-s.fr>
 * Copyright (c) 2011-2012, Centre National d'Etudes Spatiales (CNES), France
 * Copyright (c) 2012, CS Systemes d'Information, France
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#ifdef _WIN32
#include "windirent.h"
#else
#include <dirent.h>
#endif /* _WIN32 */

#ifdef _WIN32
#include <windows.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/times.h>
#endif /* _WIN32 */

#include "opj_apps_config.h"
#include "openjpeg.h"
#include "opj_getopt.h"
#include "convert.h"
#include "index.h"

#include "format_defs.h"
#include "opj_string.h"

typedef struct dircnt {
    /** Buffer for holding images read from Directory*/
    char *filename_buf;
    /** Pointer to the buffer*/
    char **filename;
} dircnt_t;

typedef struct img_folder {
    /** The directory path of the folder containing input images*/
    char *imgdirpath;
    /** Output format*/
    char *out_format;
    /** Enable option*/
    char set_imgdir;
    /** Enable Cod Format for output*/
    char set_out_format;
} img_fol_t;

static void encode_help_display(void)
{
    fprintf(stdout,
            "\nThis is the opj_compress utility from the OpenJPEG project.\n"
            "It compresses various image formats with the JPEG 2000 algorithm.\n"
            "It has been compiled against openjp2 library v%s.\n\n", opj_version());

    fprintf(stdout, "Default encoding options:\n");
    fprintf(stdout, "-------------------------\n");
    fprintf(stdout, "\n");
    fprintf(stdout, " * Lossless\n");
    fprintf(stdout, " * 1 tile\n");
    fprintf(stdout, " * RGB->YCC conversion if at least 3 components\n");
    fprintf(stdout, " * Size of precinct : 2^15 x 2^15 (means 1 precinct)\n");
    fprintf(stdout, " * Size of code-block : 64 x 64\n");
    fprintf(stdout, " * Number of resolutions: 6\n");
    fprintf(stdout, " * No SOP marker in the codestream\n");
    fprintf(stdout, " * No EPH marker in the codestream\n");
    fprintf(stdout, " * No sub-sampling in x or y direction\n");
    fprintf(stdout, " * No mode switch activated\n");
    fprintf(stdout, " * Progression order: LRCP\n");
#ifdef FIXME_INDEX
    fprintf(stdout, " * No index file\n");
#endif /* FIXME_INDEX */
    fprintf(stdout, " * No ROI upshifted\n");
    fprintf(stdout, " * No offset of the origin of the image\n");
    fprintf(stdout, " * No offset of the origin of the tiles\n");
    fprintf(stdout, " * Reversible DWT 5-3\n");
    /* UniPG>> */
#ifdef USE_JPWL
    fprintf(stdout, " * No JPWL protection\n");
#endif /* USE_JPWL */
    /* <<UniPG */
    fprintf(stdout, "\n");

    fprintf(stdout, "Note:\n");
    fprintf(stdout, "-----\n");
    fprintf(stdout, "\n");
    fprintf(stdout,
            "The markers written to the main_header are : SOC SIZ COD QCD COM.\n");
    fprintf(stdout, "COD and QCD never appear in the tile_header.\n");
    fprintf(stdout, "\n");

    fprintf(stdout, "Parameters:\n");
    fprintf(stdout, "-----------\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Required Parameters (except with -h):\n");
    fprintf(stdout, "One of the two options -ImgDir or -i must be used\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "-i <file>\n");
    fprintf(stdout, "    Input file\n");
    fprintf(stdout,
            "    Known extensions are <PBM|PGM|PPM|PNM|PAM|PGX|PNG|BMP|TIF|RAW|RAWL|TGA>\n");
    fprintf(stdout, "    If used, '-o <file>' must be provided\n");
    fprintf(stdout, "-o <compressed file>\n");
    fprintf(stdout, "    Output file (accepted extensions are j2k or jp2).\n");
    fprintf(stdout, "-ImgDir <dir>\n");
    fprintf(stdout, "    Image file Directory path (example ../Images) \n");
    fprintf(stdout, "    When using this option -OutFor must be used\n");
    fprintf(stdout, "-OutFor <J2K|J2C|JP2>\n");
    fprintf(stdout, "    Output format for compressed files.\n");
    fprintf(stdout, "    Required only if -ImgDir is used\n");
    fprintf(stdout,
            "-F <width>,<height>,<ncomp>,<bitdepth>,{s,u}@<dx1>x<dy1>:...:<dxn>x<dyn>\n");
    fprintf(stdout, "    Characteristics of the raw input image\n");
    fprintf(stdout,
            "    If subsampling is omitted, 1x1 is assumed for all components\n");
    fprintf(stdout, "      Example: -F 512,512,3,8,u@1x1:2x2:2x2\n");
    fprintf(stdout,
            "               for raw 512x512 image with 4:2:0 subsampling\n");
    fprintf(stdout, "    Required only if RAW or RAWL input file is provided.\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Optional Parameters:\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "-h\n");
    fprintf(stdout, "    Display the help information.\n");
    fprintf(stdout, "-r <compression ratio>,<compression ratio>,...\n");
    fprintf(stdout, "    Different compression ratios for successive layers.\n");
    fprintf(stdout,
            "    The rate specified for each quality level is the desired\n");
    fprintf(stdout, "    compression factor (use 1 for lossless)\n");
    fprintf(stdout, "    Decreasing ratios required.\n");
    fprintf(stdout, "      Example: -r 20,10,1 means \n");
    fprintf(stdout, "            quality layer 1: compress 20x, \n");
    fprintf(stdout, "            quality layer 2: compress 10x \n");
    fprintf(stdout, "            quality layer 3: compress lossless\n");
    fprintf(stdout, "    Options -r and -q cannot be used together.\n");
    fprintf(stdout, "-q <psnr value>,<psnr value>,<psnr value>,...\n");
    fprintf(stdout, "    Different psnr for successive layers (-q 30,40,50).\n");
    fprintf(stdout, "    Increasing PSNR values required, except 0 which can\n");
    fprintf(stdout, "    be used for the last layer to indicate it is lossless.\n");
    fprintf(stdout, "    Options -r and -q cannot be used together.\n");
    fprintf(stdout, "-n <number of resolutions>\n");
    fprintf(stdout, "    Number of resolutions.\n");
    fprintf(stdout,
            "    It corresponds to the number of DWT decompositions +1. \n");
    fprintf(stdout, "    Default: 6.\n");
    fprintf(stdout, "-b <cblk width>,<cblk height>\n");
    fprintf(stdout,
            "    Code-block size. The dimension must respect the constraint \n");
    fprintf(stdout,
            "    defined in the JPEG-2000 standard (no dimension smaller than 4 \n");
    fprintf(stdout,
            "    or greater than 1024, no code-block with more than 4096 coefficients).\n");
    fprintf(stdout, "    The maximum value authorized is 64x64. \n");
    fprintf(stdout, "    Default: 64x64.\n");
    fprintf(stdout,
            "-c [<prec width>,<prec height>],[<prec width>,<prec height>],...\n");
    fprintf(stdout, "    Precinct size. Values specified must be power of 2. \n");
    fprintf(stdout,
            "    Multiple records may be supplied, in which case the first record refers\n");
    fprintf(stdout,
            "    to the highest resolution level and subsequent records to lower \n");
    fprintf(stdout,
            "    resolution levels. The last specified record is right-shifted for each \n");
    fprintf(stdout, "    remaining lower resolution levels.\n");
    fprintf(stdout, "    Default: 215x215 at each resolution.\n");
    fprintf(stdout, "-t <tile width>,<tile height>\n");
    fprintf(stdout, "    Tile size.\n");
    fprintf(stdout,
            "    Default: the dimension of the whole image, thus only one tile.\n");
    fprintf(stdout, "-p <LRCP|RLCP|RPCL|PCRL|CPRL>\n");
    fprintf(stdout, "    Progression order.\n");
    fprintf(stdout, "    Default: LRCP.\n");
    fprintf(stdout, "-s  <subX,subY>\n");
    fprintf(stdout, "    Subsampling factor.\n");
    fprintf(stdout, "    Subsampling bigger than 2 can produce error\n");
    fprintf(stdout, "    Default: no subsampling.\n");
    fprintf(stdout,
            "-POC <progression order change>/<progression order change>/...\n");
    fprintf(stdout, "    Progression order change.\n");
    fprintf(stdout,
            "    The syntax of a progression order change is the following:\n");
    fprintf(stdout,
            "    T<tile>=<resStart>,<compStart>,<layerEnd>,<resEnd>,<compEnd>,<progOrder>\n");
    fprintf(stdout, "      Example: -POC T1=0,0,1,5,3,CPRL/T1=5,0,1,6,3,CPRL\n");
    fprintf(stdout, "-SOP\n");
    fprintf(stdout, "    Write SOP marker before each packet.\n");
    fprintf(stdout, "-EPH\n");
    fprintf(stdout, "    Write EPH marker after each header packet.\n");
    fprintf(stdout, "-M <key value>\n");
    fprintf(stdout, "    Mode switch.\n");
    fprintf(stdout, "    [1=BYPASS(LAZY) 2=RESET 4=RESTART(TERMALL)\n");
    fprintf(stdout, "    8=VSC 16=ERTERM(SEGTERM) 32=SEGMARK(SEGSYM)]\n");
    fprintf(stdout, "    Indicate multiple modes by adding their values.\n");
    fprintf(stdout,
            "      Example: RESTART(4) + RESET(2) + SEGMARK(32) => -M 38\n");
    fprintf(stdout, "-TP <R|L|C>\n");
    fprintf(stdout, "    Divide packets of every tile into tile-parts.\n");
    fprintf(stdout,
            "    Division is made by grouping Resolutions (R), Layers (L)\n");
    fprintf(stdout, "    or Components (C).\n");
#ifdef FIXME_INDEX
    fprintf(stdout, "-x  <index file>\n");
    fprintf(stdout, "    Create an index file.\n");
#endif /*FIXME_INDEX*/
    fprintf(stdout, "-ROI c=<component index>,U=<upshifting value>\n");
    fprintf(stdout, "    Quantization indices upshifted for a component. \n");
    fprintf(stdout,
            "    Warning: This option does not implement the usual ROI (Region of Interest).\n");
    fprintf(stdout,
            "    It should be understood as a 'Component of Interest'. It offers the \n");
    fprintf(stdout,
            "    possibility to upshift the value of a component during quantization step.\n");
    fprintf(stdout,
            "    The value after c= is the component number [0, 1, 2, ...] and the value \n");
    fprintf(stdout,
            "    after U= is the value of upshifting. U must be in the range [0, 37].\n");
    fprintf(stdout, "-d <image offset X,image offset Y>\n");
    fprintf(stdout, "    Offset of the origin of the image.\n");
    fprintf(stdout, "-T <tile offset X,tile offset Y>\n");
    fprintf(stdout, "    Offset of the origin of the tiles.\n");
    fprintf(stdout, "-I\n");
    fprintf(stdout, "    Use the irreversible DWT 9-7.\n");
    fprintf(stdout, "-mct <0|1|2>\n");
    fprintf(stdout,
            "    Explicitly specifies if a Multiple Component Transform has to be used.\n");
    fprintf(stdout, "    0: no MCT ; 1: RGB->YCC conversion ; 2: custom MCT.\n");
    fprintf(stdout,
            "    If custom MCT, \"-m\" option has to be used (see hereunder).\n");
    fprintf(stdout,
            "    By default, RGB->YCC conversion is used if there are 3 components or more,\n");
    fprintf(stdout, "    no conversion otherwise.\n");
    fprintf(stdout, "-m <file>\n");
    fprintf(stdout,
            "    Use array-based MCT, values are coma separated, line by line\n");
    fprintf(stdout,
            "    No specific separators between lines, no space allowed between values.\n");
    fprintf(stdout,
            "    If this option is used, it automatically sets \"-mct\" option to 2.\n");
    fprintf(stdout, "-cinema2K <24|48>\n");
    fprintf(stdout, "    Digital Cinema 2K profile compliant codestream.\n");
    fprintf(stdout,
            "	Need to specify the frames per second for a 2K resolution.\n");
    fprintf(stdout, "    Only 24 or 48 fps are currently allowed.\n");
    fprintf(stdout, "-cinema4K\n");
    fprintf(stdout, "    Digital Cinema 4K profile compliant codestream.\n");
    fprintf(stdout, "	Frames per second not required. Default value is 24fps.\n");
    fprintf(stdout, "-jpip\n");
    fprintf(stdout, "    Write jpip codestream index box in JP2 output file.\n");
    fprintf(stdout, "    Currently supports only RPCL order.\n");
    fprintf(stdout, "-C <comment>\n");
    fprintf(stdout, "    Add <comment> in the comment marker segment.\n");
    /* UniPG>> */
#ifdef USE_JPWL
    fprintf(stdout, "-W <params>\n");
    fprintf(stdout, "    Adoption of JPWL (Part 11) capabilities (-W params)\n");
    fprintf(stdout,
            "    The <params> field can be written and repeated in any order:\n");
    fprintf(stdout, "    [h<tilepart><=type>,s<tilepart><=method>,a=<addr>,...\n");
    fprintf(stdout, "    ...,z=<size>,g=<range>,p<tilepart:pack><=type>]\n");
    fprintf(stdout,
            "     h selects the header error protection (EPB): 'type' can be\n");
    fprintf(stdout,
            "       [0=none 1,absent=predefined 16=CRC-16 32=CRC-32 37-128=RS]\n");
    fprintf(stdout,
            "       if 'tilepart' is absent, it is for main and tile headers\n");
    fprintf(stdout, "       if 'tilepart' is present, it applies from that tile\n");
    fprintf(stdout,
            "         onwards, up to the next h<> spec, or to the last tilepart\n");
    fprintf(stdout, "         in the codestream (max. %d specs)\n",
            JPWL_MAX_NO_TILESPECS);
    fprintf(stdout,
            "     p selects the packet error protection (EEP/UEP with EPBs)\n");
    fprintf(stdout, "      to be applied to raw data: 'type' can be\n");
    fprintf(stdout,
            "       [0=none 1,absent=predefined 16=CRC-16 32=CRC-32 37-128=RS]\n");
    fprintf(stdout,
            "       if 'tilepart:pack' is absent, it is from tile 0, packet 0\n");
    fprintf(stdout,
            "       if 'tilepart:pack' is present, it applies from that tile\n");
    fprintf(stdout,
            "         and that packet onwards, up to the next packet spec\n");
    fprintf(stdout,
            "         or to the last packet in the last tilepart in the stream\n");
    fprintf(stdout, "         (max. %d specs)\n", JPWL_MAX_NO_PACKSPECS);
    fprintf(stdout,
            "     s enables sensitivity data insertion (ESD): 'method' can be\n");
    fprintf(stdout,
            "       [-1=NO ESD 0=RELATIVE ERROR 1=MSE 2=MSE REDUCTION 3=PSNR\n");
    fprintf(stdout, "        4=PSNR INCREMENT 5=MAXERR 6=TSE 7=RESERVED]\n");
    fprintf(stdout, "       if 'tilepart' is absent, it is for main header only\n");
    fprintf(stdout, "       if 'tilepart' is present, it applies from that tile\n");
    fprintf(stdout,
            "         onwards, up to the next s<> spec, or to the last tilepart\n");
    fprintf(stdout, "         in the codestream (max. %d specs)\n",
            JPWL_MAX_NO_TILESPECS);
    fprintf(stdout, "     g determines the addressing mode: <range> can be\n");
    fprintf(stdout, "       [0=PACKET 1=BYTE RANGE 2=PACKET RANGE]\n");
    fprintf(stdout,
            "     a determines the size of data addressing: <addr> can be\n");
    fprintf(stdout,
            "       2/4 bytes (small/large codestreams). If not set, auto-mode\n");
    fprintf(stdout,
            "     z determines the size of sensitivity values: <size> can be\n");
    fprintf(stdout,
            "       1/2 bytes, for the transformed pseudo-floating point value\n");
    fprintf(stdout, "     ex.:\n");
    fprintf(stdout,
            "       h,h0=64,h3=16,h5=32,p0=78,p0:24=56,p1,p3:0=0,p3:20=32,s=0,\n");
    fprintf(stdout, "         s0=6,s3=-1,a=0,g=1,z=1\n");
    fprintf(stdout, "     means\n");
    fprintf(stdout,
            "       predefined EPB in MH, rs(64,32) from TPH 0 to TPH 2,\n");
    fprintf(stdout,
            "       CRC-16 in TPH 3 and TPH 4, CRC-32 in remaining TPHs,\n");
    fprintf(stdout, "       UEP rs(78,32) for packets 0 to 23 of tile 0,\n");
    fprintf(stdout,
            "       UEP rs(56,32) for packs. 24 to the last of tilepart 0,\n");
    fprintf(stdout, "       UEP rs default for packets of tilepart 1,\n");
    fprintf(stdout, "       no UEP for packets 0 to 19 of tilepart 3,\n");
    fprintf(stdout,
            "       UEP CRC-32 for packs. 20 of tilepart 3 to last tilepart,\n");
    fprintf(stdout, "       relative sensitivity ESD for MH,\n");
    fprintf(stdout,
            "       TSE ESD from TPH 0 to TPH 2, byte range with automatic\n");
    fprintf(stdout,
            "       size of addresses and 1 byte for each sensitivity value\n");
    fprintf(stdout, "     ex.:\n");
    fprintf(stdout, "           h,s,p\n");
    fprintf(stdout, "     means\n");
    fprintf(stdout,
            "       default protection to headers (MH and TPHs) as well as\n");
    fprintf(stdout, "       data packets, one ESD in MH\n");
    fprintf(stdout,
            "     N.B.: use the following recommendations when specifying\n");
    fprintf(stdout, "           the JPWL parameters list\n");
    fprintf(stdout,
            "       - when you use UEP, always pair the 'p' option with 'h'\n");
#endif /* USE_JPWL */
    /* <<UniPG */
    fprintf(stdout, "\n");
#ifdef FIXME_INDEX
    fprintf(stdout, "Index structure:\n");
    fprintf(stdout, "----------------\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Image_height Image_width\n");
    fprintf(stdout, "progression order\n");
    fprintf(stdout, "Tiles_size_X Tiles_size_Y\n");
    fprintf(stdout, "Tiles_nb_X Tiles_nb_Y\n");
    fprintf(stdout, "Components_nb\n");
    fprintf(stdout, "Layers_nb\n");
    fprintf(stdout, "decomposition_levels\n");
    fprintf(stdout, "[Precincts_size_X_res_Nr Precincts_size_Y_res_Nr]...\n");
    fprintf(stdout, "   [Precincts_size_X_res_0 Precincts_size_Y_res_0]\n");
    fprintf(stdout, "Main_header_start_position\n");
    fprintf(stdout, "Main_header_end_position\n");
    fprintf(stdout, "Codestream_size\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "INFO ON TILES\n");
    fprintf(stdout,
            "tileno start_pos end_hd end_tile nbparts disto nbpix disto/nbpix\n");
    fprintf(stdout,
            "Tile_0 start_pos end_Theader end_pos NumParts TotalDisto NumPix MaxMSE\n");
    fprintf(stdout,
            "Tile_1   ''           ''        ''        ''       ''    ''      ''\n");
    fprintf(stdout, "...\n");
    fprintf(stdout,
            "Tile_Nt   ''           ''        ''        ''       ''    ''     ''\n");
    fprintf(stdout, "...\n");
    fprintf(stdout, "TILE 0 DETAILS\n");
    fprintf(stdout, "part_nb tileno num_packs start_pos end_tph_pos end_pos\n");
    fprintf(stdout, "...\n");
    fprintf(stdout, "Progression_string\n");
    fprintf(stdout,
            "pack_nb tileno layno resno compno precno start_pos end_ph_pos end_pos disto\n");
    fprintf(stdout,
            "Tpacket_0 Tile layer res. comp. prec. start_pos end_pos disto\n");
    fprintf(stdout, "...\n");
    fprintf(stdout,
            "Tpacket_Np ''   ''    ''   ''    ''       ''       ''     ''\n");
    fprintf(stdout, "MaxDisto\n");
    fprintf(stdout, "TotalDisto\n\n");
#endif /*FIXME_INDEX*/
}

static OPJ_PROG_ORDER give_progression(const char progression[4])
{
    if (strncmp(progression, "LRCP", 4) == 0) {
        return OPJ_LRCP;
    }
    if (strncmp(progression, "RLCP", 4) == 0) {
        return OPJ_RLCP;
    }
    if (strncmp(progression, "RPCL", 4) == 0) {
        return OPJ_RPCL;
    }
    if (strncmp(progression, "PCRL", 4) == 0) {
        return OPJ_PCRL;
    }
    if (strncmp(progression, "CPRL", 4) == 0) {
        return OPJ_CPRL;
    }

    return OPJ_PROG_UNKNOWN;
}

static unsigned int get_num_images(char *imgdirpath)
{
    DIR *dir;
    struct dirent* content;
    unsigned int num_images = 0;

    /*Reading the input images from given input directory*/

    dir = opendir(imgdirpath);
    if (!dir) {
        fprintf(stderr, "Could not open Folder %s\n", imgdirpath);
        return 0;
    }

    num_images = 0;
    while ((content = readdir(dir)) != NULL) {
        if (strcmp(".", content->d_name) == 0 || strcmp("..", content->d_name) == 0) {
            continue;
        }
        num_images++;
    }
    closedir(dir);
    return num_images;
}

static int load_images(dircnt_t *dirptr, char *imgdirpath)
{
    DIR *dir;
    struct dirent* content;
    int i = 0;

    /*Reading the input images from given input directory*/

    dir = opendir(imgdirpath);
    if (!dir) {
        fprintf(stderr, "Could not open Folder %s\n", imgdirpath);
        return 1;
    } else   {
        fprintf(stderr, "Folder opened successfully\n");
    }

    while ((content = readdir(dir)) != NULL) {
        if (strcmp(".", content->d_name) == 0 || strcmp("..", content->d_name) == 0) {
            continue;
        }

        strcpy(dirptr->filename[i], content->d_name);
        i++;
    }
    closedir(dir);
    return 0;
}

static int get_file_format(char *filename)
{
    unsigned int i;
    static const char *extension[] = {
        "pgx", "pnm", "pgm", "ppm", "pbm", "pam", "bmp", "tif", "raw", "rawl", "tga", "png", "j2k", "jp2", "j2c", "jpc"
    };
    static const int format[] = {
        PGX_DFMT, PXM_DFMT, PXM_DFMT, PXM_DFMT, PXM_DFMT, PXM_DFMT, BMP_DFMT, TIF_DFMT, RAW_DFMT, RAWL_DFMT, TGA_DFMT, PNG_DFMT, J2K_CFMT, JP2_CFMT, J2K_CFMT, J2K_CFMT
    };
    char * ext = strrchr(filename, '.');
    if (ext == NULL) {
        return -1;
    }
    ext++;
    for (i = 0; i < sizeof(format) / sizeof(*format); i++) {
        if (strcasecmp(ext, extension[i]) == 0) {
            return format[i];
        }
    }
    return -1;
}

static char * get_file_name(char *name)
{
    char *fname = strtok(name, ".");
    return fname;
}

static char get_next_file(int imageno, dircnt_t *dirptr, img_fol_t *img_fol,
                          opj_cparameters_t *parameters)
{
    char image_filename[OPJ_PATH_LEN], infilename[OPJ_PATH_LEN],
         outfilename[OPJ_PATH_LEN], temp_ofname[OPJ_PATH_LEN];
    char *temp_p, temp1[OPJ_PATH_LEN] = "";

    strcpy(image_filename, dirptr->filename[imageno]);
    fprintf(stderr, "File Number %d \"%s\"\n", imageno, image_filename);
    parameters->decod_format = get_file_format(image_filename);
    if (parameters->decod_format == -1) {
        return 1;
    }
    sprintf(infilename, "%s/%s", img_fol->imgdirpath, image_filename);
    if (opj_strcpy_s(parameters->infile, sizeof(parameters->infile),
                     infilename) != 0) {
        return 1;
    }

    /*Set output file*/
    strcpy(temp_ofname, get_file_name(image_filename));
    while ((temp_p = strtok(NULL, ".")) != NULL) {
        strcat(temp_ofname, temp1);
        sprintf(temp1, ".%s", temp_p);
    }
    if (img_fol->set_out_format == 1) {
        sprintf(outfilename, "%s/%s.%s", img_fol->imgdirpath, temp_ofname,
                img_fol->out_format);
        if (opj_strcpy_s(parameters->outfile, sizeof(parameters->outfile),
                         outfilename) != 0) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------------------ */

static int parse_cmdline_encoder(int argc, char **argv,
                                 opj_cparameters_t *parameters,
                                 img_fol_t *img_fol, raw_cparameters_t *raw_cp, char *indexfilename,
                                 size_t indexfilename_size)
{
    OPJ_UINT32 i, j;
    int totlen, c;
    opj_option_t long_option[] = {
        {"cinema2K", REQ_ARG, NULL, 'w'},
        {"cinema4K", NO_ARG, NULL, 'y'},
        {"ImgDir", REQ_ARG, NULL, 'z'},
        {"TP", REQ_ARG, NULL, 'u'},
        {"SOP", NO_ARG, NULL, 'S'},
        {"EPH", NO_ARG, NULL, 'E'},
        {"OutFor", REQ_ARG, NULL, 'O'},
        {"POC", REQ_ARG, NULL, 'P'},
        {"ROI", REQ_ARG, NULL, 'R'},
        {"jpip", NO_ARG, NULL, 'J'},
        {"mct", REQ_ARG, NULL, 'Y'}
    };

    /* parse the command line */
    const char optlist[] = "i:o:r:q:n:b:c:t:p:s:SEM:x:R:d:T:If:P:C:F:u:JY:"
#ifdef USE_JPWL
                           "W:"
#endif /* USE_JPWL */
                           "h";

    totlen = sizeof(long_option);
    img_fol->set_out_format = 0;
    raw_cp->rawWidth = 0;

    do {
        c = opj_getopt_long(argc, argv, optlist, long_option, totlen);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'i': {         /* input file */
            char *infile = opj_optarg;
            parameters->decod_format = get_file_format(infile);
            switch (parameters->decod_format) {
            case PGX_DFMT:
            case PXM_DFMT:
            case BMP_DFMT:
            case TIF_DFMT:
            case RAW_DFMT:
            case RAWL_DFMT:
            case TGA_DFMT:
            case PNG_DFMT:
                break;
            default:
                fprintf(stderr,
                        "[ERROR] Unknown input file format: %s \n"
                        "        Known file formats are *.pnm, *.pgm, *.ppm, *.pgx, *png, *.bmp, *.tif, *.raw or *.tga\n",
                        infile);
                return 1;
            }
            if (opj_strcpy_s(parameters->infile, sizeof(parameters->infile), infile) != 0) {
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */

        case 'o': {         /* output file */
            char *outfile = opj_optarg;
            parameters->cod_format = get_file_format(outfile);
            switch (parameters->cod_format) {
            case J2K_CFMT:
            case JP2_CFMT:
                break;
            default:
                fprintf(stderr,
                        "Unknown output format image %s [only *.j2k, *.j2c or *.jp2]!! \n", outfile);
                return 1;
            }
            if (opj_strcpy_s(parameters->outfile, sizeof(parameters->outfile),
                             outfile) != 0) {
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */
        case 'O': {         /* output format */
            char outformat[50];
            char *of = opj_optarg;
            sprintf(outformat, ".%s", of);
            img_fol->set_out_format = 1;
            parameters->cod_format = get_file_format(outformat);
            switch (parameters->cod_format) {
            case J2K_CFMT:
            case JP2_CFMT:
                img_fol->out_format = opj_optarg;
                break;
            default:
                fprintf(stderr, "Unknown output format image [only j2k, j2c, jp2]!! \n");
                return 1;
            }
        }
        break;


        /* ----------------------------------------------------- */


        case 'r': {         /* rates rates/distorsion */
            char *s = opj_optarg;
            parameters->tcp_numlayers = 0;
            while (sscanf(s, "%f", &parameters->tcp_rates[parameters->tcp_numlayers]) ==
                    1) {
                parameters->tcp_numlayers++;
                while (*s && *s != ',') {
                    s++;
                }
                if (!*s) {
                    break;
                }
                s++;
            }
            parameters->cp_disto_alloc = 1;
        }
        break;

        /* ----------------------------------------------------- */


        case 'F': {         /* Raw image format parameters */
            OPJ_BOOL wrong = OPJ_FALSE;
            char *substr1;
            char *substr2;
            char *sep;
            char signo;
            int width, height, bitdepth, ncomp;
            OPJ_UINT32 len;
            OPJ_BOOL raw_signed = OPJ_FALSE;
            substr2 = strchr(opj_optarg, '@');
            if (substr2 == NULL) {
                len = (OPJ_UINT32) strlen(opj_optarg);
            } else {
                len = (OPJ_UINT32)(substr2 - opj_optarg);
                substr2++; /* skip '@' character */
            }
            substr1 = (char*) malloc((len + 1) * sizeof(char));
            if (substr1 == NULL) {
                return 1;
            }
            memcpy(substr1, opj_optarg, len);
            substr1[len] = '\0';
            if (sscanf(substr1, "%d,%d,%d,%d,%c", &width, &height, &ncomp, &bitdepth,
                       &signo) == 5) {
                if (signo == 's') {
                    raw_signed = OPJ_TRUE;
                } else if (signo == 'u') {
                    raw_signed = OPJ_FALSE;
                } else {
                    wrong = OPJ_TRUE;
                }
            } else {
                wrong = OPJ_TRUE;
            }
            if (!wrong) {
                int compno;
                int lastdx = 1;
                int lastdy = 1;
                raw_cp->rawWidth = width;
                raw_cp->rawHeight = height;
                raw_cp->rawComp = ncomp;
                raw_cp->rawBitDepth = bitdepth;
                raw_cp->rawSigned  = raw_signed;
                raw_cp->rawComps = (raw_comp_cparameters_t*) malloc(((OPJ_UINT32)(
                                       ncomp)) * sizeof(raw_comp_cparameters_t));
                if (raw_cp->rawComps == NULL) {
                    free(substr1);
                    return 1;
                }
                for (compno = 0; compno < ncomp && !wrong; compno++) {
                    if (substr2 == NULL) {
                        raw_cp->rawComps[compno].dx = lastdx;
                        raw_cp->rawComps[compno].dy = lastdy;
                    } else {
                        int dx, dy;
                        sep = strchr(substr2, ':');
                        if (sep == NULL) {
                            if (sscanf(substr2, "%dx%d", &dx, &dy) == 2) {
                                lastdx = dx;
                                lastdy = dy;
                                raw_cp->rawComps[compno].dx = dx;
                                raw_cp->rawComps[compno].dy = dy;
                                substr2 = NULL;
                            } else {
                                wrong = OPJ_TRUE;
                            }
                        } else {
                            if (sscanf(substr2, "%dx%d:%s", &dx, &dy, substr2) == 3) {
                                raw_cp->rawComps[compno].dx = dx;
                                raw_cp->rawComps[compno].dy = dy;
                            } else {
                                wrong = OPJ_TRUE;
                            }
                        }
                    }
                }
            }
            free(substr1);
            if (wrong) {
                fprintf(stderr, "\nError: invalid raw image parameters\n");
                fprintf(stderr, "Please use the Format option -F:\n");
                fprintf(stderr,
                        "-F <width>,<height>,<ncomp>,<bitdepth>,{s,u}@<dx1>x<dy1>:...:<dxn>x<dyn>\n");
                fprintf(stderr,
                        "If subsampling is omitted, 1x1 is assumed for all components\n");
                fprintf(stderr,
                        "Example: -i image.raw -o image.j2k -F 512,512,3,8,u@1x1:2x2:2x2\n");
                fprintf(stderr, "         for raw 512x512 image with 4:2:0 subsampling\n");
                fprintf(stderr, "Aborting.\n");
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */

        case 'q': {         /* add fixed_quality */
            char *s = opj_optarg;
            while (sscanf(s, "%f", &parameters->tcp_distoratio[parameters->tcp_numlayers])
                    == 1) {
                parameters->tcp_numlayers++;
                while (*s && *s != ',') {
                    s++;
                }
                if (!*s) {
                    break;
                }
                s++;
            }
            parameters->cp_fixed_quality = 1;
        }
        break;

        /* dda */
        /* ----------------------------------------------------- */

        case 'f': {         /* mod fixed_quality (before : -q) */
            int *row = NULL, *col = NULL;
            OPJ_UINT32 numlayers = 0, numresolution = 0, matrix_width = 0;

            char *s = opj_optarg;
            sscanf(s, "%u", &numlayers);
            s++;
            if (numlayers > 9) {
                s++;
            }

            parameters->tcp_numlayers = (int)numlayers;
            numresolution = (OPJ_UINT32)parameters->numresolution;
            matrix_width = numresolution * 3;
            parameters->cp_matrice = (int *) malloc(numlayers * matrix_width * sizeof(int));
            if (parameters->cp_matrice == NULL) {
                return 1;
            }
            s = s + 2;

            for (i = 0; i < numlayers; i++) {
                row = &parameters->cp_matrice[i * matrix_width];
                col = row;
                parameters->tcp_rates[i] = 1;
                sscanf(s, "%d,", &col[0]);
                s += 2;
                if (col[0] > 9) {
                    s++;
                }
                col[1] = 0;
                col[2] = 0;
                for (j = 1; j < numresolution; j++) {
                    col += 3;
                    sscanf(s, "%d,%d,%d", &col[0], &col[1], &col[2]);
                    s += 6;
                    if (col[0] > 9) {
                        s++;
                    }
                    if (col[1] > 9) {
                        s++;
                    }
                    if (col[2] > 9) {
                        s++;
                    }
                }
                if (i < numlayers - 1) {
                    s++;
                }
            }
            parameters->cp_fixed_alloc = 1;
        }
        break;

        /* ----------------------------------------------------- */

        case 't': {         /* tiles */
            sscanf(opj_optarg, "%d,%d", &parameters->cp_tdx, &parameters->cp_tdy);
            parameters->tile_size_on = OPJ_TRUE;
        }
        break;

        /* ----------------------------------------------------- */

        case 'n': {         /* resolution */
            sscanf(opj_optarg, "%d", &parameters->numresolution);
        }
        break;

        /* ----------------------------------------------------- */
        case 'c': {         /* precinct dimension */
            char sep;
            int res_spec = 0;

            char *s = opj_optarg;
            int ret;
            do {
                sep = 0;
                ret = sscanf(s, "[%d,%d]%c", &parameters->prcw_init[res_spec],
                             &parameters->prch_init[res_spec], &sep);
                if (!(ret == 2 && sep == 0) && !(ret == 3 && sep == ',')) {
                    fprintf(stderr, "\nError: could not parse precinct dimension: '%s' %x\n", s,
                            sep);
                    fprintf(stderr, "Example: -i lena.raw -o lena.j2k -c [128,128],[128,128]\n");
                    return 1;
                }
                parameters->csty |= 0x01;
                res_spec++;
                s = strpbrk(s, "]") + 2;
            } while (sep == ',');
            parameters->res_spec = res_spec;
        }
        break;

        /* ----------------------------------------------------- */

        case 'b': {         /* code-block dimension */
            int cblockw_init = 0, cblockh_init = 0;
            sscanf(opj_optarg, "%d,%d", &cblockw_init, &cblockh_init);
            if (cblockw_init > 1024 || cblockw_init < 4 ||
                    cblockh_init > 1024 || cblockh_init < 4 ||
                    cblockw_init * cblockh_init > 4096) {
                fprintf(stderr,
                        "!! Size of code_block error (option -b) !!\n\nRestriction :\n"
                        "    * width*height<=4096\n    * 4<=width,height<= 1024\n\n");
                return 1;
            }
            parameters->cblockw_init = cblockw_init;
            parameters->cblockh_init = cblockh_init;
        }
        break;

        /* ----------------------------------------------------- */

        case 'x': {         /* creation of index file */
            if (opj_strcpy_s(indexfilename, indexfilename_size, opj_optarg) != 0) {
                return 1;
            }
            /* FIXME ADE INDEX >> */
            fprintf(stderr,
                    "[WARNING] Index file generation is currently broken.\n"
                    "          '-x' option ignored.\n");
            /* << FIXME ADE INDEX */
        }
        break;

        /* ----------------------------------------------------- */

        case 'p': {         /* progression order */
            char progression[4];

            strncpy(progression, opj_optarg, 4);
            parameters->prog_order = give_progression(progression);
            if (parameters->prog_order == -1) {
                fprintf(stderr, "Unrecognized progression order "
                        "[LRCP, RLCP, RPCL, PCRL, CPRL] !!\n");
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */

        case 's': {         /* subsampling factor */
            if (sscanf(opj_optarg, "%d,%d", &parameters->subsampling_dx,
                       &parameters->subsampling_dy) != 2) {
                fprintf(stderr, "'-s' sub-sampling argument error !  [-s dx,dy]\n");
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */

        case 'd': {         /* coordonnate of the reference grid */
            if (sscanf(opj_optarg, "%d,%d", &parameters->image_offset_x0,
                       &parameters->image_offset_y0) != 2) {
                fprintf(stderr, "-d 'coordonnate of the reference grid' argument "
                        "error !! [-d x0,y0]\n");
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */

        case 'h':           /* display an help description */
            encode_help_display();
            return 1;

        /* ----------------------------------------------------- */

        case 'P': {         /* POC */
            int numpocs = 0;        /* number of progression order change (POC) default 0 */
            opj_poc_t *POC = NULL;  /* POC : used in case of Progression order change */

            char *s = opj_optarg;
            POC = parameters->POC;

            while (sscanf(s, "T%u=%u,%u,%u,%u,%u,%4s", &POC[numpocs].tile,
                          &POC[numpocs].resno0, &POC[numpocs].compno0,
                          &POC[numpocs].layno1, &POC[numpocs].resno1,
                          &POC[numpocs].compno1, POC[numpocs].progorder) == 7) {
                POC[numpocs].prg1 = give_progression(POC[numpocs].progorder);
                numpocs++;
                while (*s && *s != '/') {
                    s++;
                }
                if (!*s) {
                    break;
                }
                s++;
            }
            parameters->numpocs = (OPJ_UINT32)numpocs;
        }
        break;

        /* ------------------------------------------------------ */

        case 'S': {         /* SOP marker */
            parameters->csty |= 0x02;
        }
        break;

        /* ------------------------------------------------------ */

        case 'E': {         /* EPH marker */
            parameters->csty |= 0x04;
        }
        break;

        /* ------------------------------------------------------ */

        case 'M': {         /* Mode switch pas tous au point !! */
            int value = 0;
            if (sscanf(opj_optarg, "%d", &value) == 1) {
                for (i = 0; i <= 5; i++) {
                    int cache = value & (1 << i);
                    if (cache) {
                        parameters->mode |= (1 << i);
                    }
                }
            }
        }
        break;

        /* ------------------------------------------------------ */

        case 'R': {         /* ROI */
            if (sscanf(opj_optarg, "c=%d,U=%d", &parameters->roi_compno,
                       &parameters->roi_shift) != 2) {
                fprintf(stderr, "ROI error !! [-ROI c='compno',U='shift']\n");
                return 1;
            }
        }
        break;

        /* ------------------------------------------------------ */

        case 'T': {         /* Tile offset */
            if (sscanf(opj_optarg, "%d,%d", &parameters->cp_tx0,
                       &parameters->cp_ty0) != 2) {
                fprintf(stderr, "-T 'tile offset' argument error !! [-T X0,Y0]");
                return 1;
            }
        }
        break;

        /* ------------------------------------------------------ */

        case 'C': {         /* add a comment */
            parameters->cp_comment = (char*)malloc(strlen(opj_optarg) + 1);
            if (parameters->cp_comment) {
                strcpy(parameters->cp_comment, opj_optarg);
            }
        }
        break;


        /* ------------------------------------------------------ */

        case 'I': {         /* reversible or not */
            parameters->irreversible = 1;
        }
        break;

        /* ------------------------------------------------------ */

        case 'u': {         /* Tile part generation*/
            parameters->tp_flag = opj_optarg[0];
            parameters->tp_on = 1;
        }
        break;

        /* ------------------------------------------------------ */

        case 'z': {         /* Image Directory path */
            img_fol->imgdirpath = (char*)malloc(strlen(opj_optarg) + 1);
            if (img_fol->imgdirpath == NULL) {
                return 1;
            }
            strcpy(img_fol->imgdirpath, opj_optarg);
            img_fol->set_imgdir = 1;
        }
        break;

        /* ------------------------------------------------------ */

        case 'w': {         /* Digital Cinema 2K profile compliance*/
            int fps = 0;
            sscanf(opj_optarg, "%d", &fps);
            if (fps == 24) {
                parameters->rsiz = OPJ_PROFILE_CINEMA_2K;
                parameters->max_comp_size = OPJ_CINEMA_24_COMP;
                parameters->max_cs_size = OPJ_CINEMA_24_CS;
            } else if (fps == 48) {
                parameters->rsiz = OPJ_PROFILE_CINEMA_2K;
                parameters->max_comp_size = OPJ_CINEMA_48_COMP;
                parameters->max_cs_size = OPJ_CINEMA_48_CS;
            } else {
                fprintf(stderr, "Incorrect value!! must be 24 or 48\n");
                return 1;
            }
            fprintf(stdout, "CINEMA 2K profile activated\n"
                    "Other options specified could be overridden\n");

        }
        break;

        /* ------------------------------------------------------ */

        case 'y': {         /* Digital Cinema 4K profile compliance*/
            parameters->rsiz = OPJ_PROFILE_CINEMA_4K;
            fprintf(stdout, "CINEMA 4K profile activated\n"
                    "Other options specified could be overridden\n");
        }
        break;

        /* ------------------------------------------------------ */

        case 'Y': {         /* Shall we do an MCT ? 0:no_mct;1:rgb->ycc;2:custom mct (-m option required)*/
            int mct_mode = 0;
            sscanf(opj_optarg, "%d", &mct_mode);
            if (mct_mode < 0 || mct_mode > 2) {
                fprintf(stderr,
                        "MCT incorrect value!! Current accepted values are 0, 1 or 2.\n");
                return 1;
            }
            parameters->tcp_mct = (char) mct_mode;
        }
        break;

        /* ------------------------------------------------------ */


        case 'm': {         /* mct input file */
            char *lFilename = opj_optarg;
            char *lMatrix;
            char *lCurrentPtr ;
            float *lCurrentDoublePtr;
            float *lSpace;
            int *l_int_ptr;
            int lNbComp = 0, lTotalComp, lMctComp, i2;
            size_t lStrLen, lStrFread;

            /* Open file */
            FILE * lFile = fopen(lFilename, "r");
            if (lFile == NULL) {
                return 1;
            }

            /* Set size of file and read its content*/
            fseek(lFile, 0, SEEK_END);
            lStrLen = (size_t)ftell(lFile);
            fseek(lFile, 0, SEEK_SET);
            lMatrix = (char *) malloc(lStrLen + 1);
            if (lMatrix == NULL) {
                fclose(lFile);
                return 1;
            }
            lStrFread = fread(lMatrix, 1, lStrLen, lFile);
            fclose(lFile);
            if (lStrLen != lStrFread) {
                free(lMatrix);
                return 1;
            }

            lMatrix[lStrLen] = 0;
            lCurrentPtr = lMatrix;

            /* replace ',' by 0 */
            while (*lCurrentPtr != 0) {
                if (*lCurrentPtr == ' ') {
                    *lCurrentPtr = 0;
                    ++lNbComp;
                }
                ++lCurrentPtr;
            }
            ++lNbComp;
            lCurrentPtr = lMatrix;

            lNbComp = (int)(sqrt(4 * lNbComp + 1) / 2. - 0.5);
            lMctComp = lNbComp * lNbComp;
            lTotalComp = lMctComp + lNbComp;
            lSpace = (float *) malloc((size_t)lTotalComp * sizeof(float));
            if (lSpace == NULL) {
                free(lMatrix);
                return 1;
            }
            lCurrentDoublePtr = lSpace;
            for (i2 = 0; i2 < lMctComp; ++i2) {
                lStrLen = strlen(lCurrentPtr) + 1;
                *lCurrentDoublePtr++ = (float) atof(lCurrentPtr);
                lCurrentPtr += lStrLen;
            }

            l_int_ptr = (int*) lCurrentDoublePtr;
            for (i2 = 0; i2 < lNbComp; ++i2) {
                lStrLen = strlen(lCurrentPtr) + 1;
                *l_int_ptr++ = atoi(lCurrentPtr);
                lCurrentPtr += lStrLen;
            }

            /* TODO should not be here ! */
            opj_set_MCT(parameters, lSpace, (int *)(lSpace + lMctComp),
                        (OPJ_UINT32)lNbComp);

            /* Free memory*/
            free(lSpace);
            free(lMatrix);
        }
        break;


            /* ------------------------------------------------------ */

            /* UniPG>> */
#ifdef USE_JPWL
        /* ------------------------------------------------------ */

        case 'W': {         /* JPWL capabilities switched on */
            char *token = NULL;
            int hprot, pprot, sens, addr, size, range;

            /* we need to enable indexing */
            if (!indexfilename || !*indexfilename) {
                if (opj_strcpy_s(indexfilename, indexfilename_size,
                                 JPWL_PRIVATEINDEX_NAME) != 0) {
                    return 1;
                }
            }

            /* search for different protection methods */

            /* break the option in comma points and parse the result */
            token = strtok(opj_optarg, ",");
            while (token != NULL) {

                /* search header error protection method */
                if (*token == 'h') {

                    static int tile = 0, tilespec = 0, lasttileno = 0;

                    hprot = 1; /* predefined method */

                    if (sscanf(token, "h=%d", &hprot) == 1) {
                        /* Main header, specified */
                        if (!((hprot == 0) || (hprot == 1) || (hprot == 16) || (hprot == 32) ||
                                ((hprot >= 37) && (hprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid main header protection method h = %d\n",
                                    hprot);
                            return 1;
                        }
                        parameters->jpwl_hprot_MH = hprot;

                    } else if (sscanf(token, "h%d=%d", &tile, &hprot) == 2) {
                        /* Tile part header, specified */
                        if (!((hprot == 0) || (hprot == 1) || (hprot == 16) || (hprot == 32) ||
                                ((hprot >= 37) && (hprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid tile part header protection method h = %d\n",
                                    hprot);
                            return 1;
                        }
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method t = %d\n", tile);
                            return 1;
                        }
                        if (tilespec < JPWL_MAX_NO_TILESPECS) {
                            parameters->jpwl_hprot_TPH_tileno[tilespec] = lasttileno = tile;
                            parameters->jpwl_hprot_TPH[tilespec++] = hprot;
                        }

                    } else if (sscanf(token, "h%d", &tile) == 1) {
                        /* Tile part header, unspecified */
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method t = %d\n", tile);
                            return 1;
                        }
                        if (tilespec < JPWL_MAX_NO_TILESPECS) {
                            parameters->jpwl_hprot_TPH_tileno[tilespec] = lasttileno = tile;
                            parameters->jpwl_hprot_TPH[tilespec++] = hprot;
                        }


                    } else if (!strcmp(token, "h")) {
                        /* Main header, unspecified */
                        parameters->jpwl_hprot_MH = hprot;

                    } else {
                        fprintf(stderr, "ERROR -> invalid protection method selection = %s\n", token);
                        return 1;
                    };

                }

                /* search packet error protection method */
                if (*token == 'p') {

                    static int pack = 0, tile = 0, packspec = 0;

                    pprot = 1; /* predefined method */

                    if (sscanf(token, "p=%d", &pprot) == 1) {
                        /* Method for all tiles and all packets */
                        if (!((pprot == 0) || (pprot == 1) || (pprot == 16) || (pprot == 32) ||
                                ((pprot >= 37) && (pprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid default packet protection method p = %d\n",
                                    pprot);
                            return 1;
                        }
                        parameters->jpwl_pprot_tileno[0] = 0;
                        parameters->jpwl_pprot_packno[0] = 0;
                        parameters->jpwl_pprot[0] = pprot;

                    } else if (sscanf(token, "p%d=%d", &tile, &pprot) == 2) {
                        /* method specified from that tile on */
                        if (!((pprot == 0) || (pprot == 1) || (pprot == 16) || (pprot == 32) ||
                                ((pprot >= 37) && (pprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid packet protection method p = %d\n", pprot);
                            return 1;
                        }
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method p = %d\n", tile);
                            return 1;
                        }
                        if (packspec < JPWL_MAX_NO_PACKSPECS) {
                            parameters->jpwl_pprot_tileno[packspec] = tile;
                            parameters->jpwl_pprot_packno[packspec] = 0;
                            parameters->jpwl_pprot[packspec++] = pprot;
                        }

                    } else if (sscanf(token, "p%d:%d=%d", &tile, &pack, &pprot) == 3) {
                        /* method fully specified from that tile and that packet on */
                        if (!((pprot == 0) || (pprot == 1) || (pprot == 16) || (pprot == 32) ||
                                ((pprot >= 37) && (pprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid packet protection method p = %d\n", pprot);
                            return 1;
                        }
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method p = %d\n", tile);
                            return 1;
                        }
                        if (pack < 0) {
                            fprintf(stderr, "ERROR -> invalid packet number on protection method p = %d\n",
                                    pack);
                            return 1;
                        }
                        if (packspec < JPWL_MAX_NO_PACKSPECS) {
                            parameters->jpwl_pprot_tileno[packspec] = tile;
                            parameters->jpwl_pprot_packno[packspec] = pack;
                            parameters->jpwl_pprot[packspec++] = pprot;
                        }

                    } else if (sscanf(token, "p%d:%d", &tile, &pack) == 2) {
                        /* default method from that tile and that packet on */
                        if (!((pprot == 0) || (pprot == 1) || (pprot == 16) || (pprot == 32) ||
                                ((pprot >= 37) && (pprot <= 128)))) {
                            fprintf(stderr, "ERROR -> invalid packet protection method p = %d\n", pprot);
                            return 1;
                        }
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method p = %d\n", tile);
                            return 1;
                        }
                        if (pack < 0) {
                            fprintf(stderr, "ERROR -> invalid packet number on protection method p = %d\n",
                                    pack);
                            return 1;
                        }
                        if (packspec < JPWL_MAX_NO_PACKSPECS) {
                            parameters->jpwl_pprot_tileno[packspec] = tile;
                            parameters->jpwl_pprot_packno[packspec] = pack;
                            parameters->jpwl_pprot[packspec++] = pprot;
                        }

                    } else if (sscanf(token, "p%d", &tile) == 1) {
                        /* default from a tile on */
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on protection method p = %d\n", tile);
                            return 1;
                        }
                        if (packspec < JPWL_MAX_NO_PACKSPECS) {
                            parameters->jpwl_pprot_tileno[packspec] = tile;
                            parameters->jpwl_pprot_packno[packspec] = 0;
                            parameters->jpwl_pprot[packspec++] = pprot;
                        }


                    } else if (!strcmp(token, "p")) {
                        /* all default */
                        parameters->jpwl_pprot_tileno[0] = 0;
                        parameters->jpwl_pprot_packno[0] = 0;
                        parameters->jpwl_pprot[0] = pprot;

                    } else {
                        fprintf(stderr, "ERROR -> invalid protection method selection = %s\n", token);
                        return 1;
                    };

                }

                /* search sensitivity method */
                if (*token == 's') {

                    static int tile = 0, tilespec = 0, lasttileno = 0;

                    sens = 0; /* predefined: relative error */

                    if (sscanf(token, "s=%d", &sens) == 1) {
                        /* Main header, specified */
                        if ((sens < -1) || (sens > 7)) {
                            fprintf(stderr, "ERROR -> invalid main header sensitivity method s = %d\n",
                                    sens);
                            return 1;
                        }
                        parameters->jpwl_sens_MH = sens;

                    } else if (sscanf(token, "s%d=%d", &tile, &sens) == 2) {
                        /* Tile part header, specified */
                        if ((sens < -1) || (sens > 7)) {
                            fprintf(stderr, "ERROR -> invalid tile part header sensitivity method s = %d\n",
                                    sens);
                            return 1;
                        }
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on sensitivity method t = %d\n", tile);
                            return 1;
                        }
                        if (tilespec < JPWL_MAX_NO_TILESPECS) {
                            parameters->jpwl_sens_TPH_tileno[tilespec] = lasttileno = tile;
                            parameters->jpwl_sens_TPH[tilespec++] = sens;
                        }

                    } else if (sscanf(token, "s%d", &tile) == 1) {
                        /* Tile part header, unspecified */
                        if (tile < 0) {
                            fprintf(stderr,
                                    "ERROR -> invalid tile part number on sensitivity method t = %d\n", tile);
                            return 1;
                        }
                        if (tilespec < JPWL_MAX_NO_TILESPECS) {
                            parameters->jpwl_sens_TPH_tileno[tilespec] = lasttileno = tile;
                            parameters->jpwl_sens_TPH[tilespec++] = hprot;
                        }

                    } else if (!strcmp(token, "s")) {
                        /* Main header, unspecified */
                        parameters->jpwl_sens_MH = sens;

                    } else {
                        fprintf(stderr, "ERROR -> invalid sensitivity method selection = %s\n", token);
                        return 1;
                    };

                    parameters->jpwl_sens_size = 2; /* 2 bytes for default size */
                }

                /* search addressing size */
                if (*token == 'a') {


                    addr = 0; /* predefined: auto */

                    if (sscanf(token, "a=%d", &addr) == 1) {
                        /* Specified */
                        if ((addr != 0) && (addr != 2) && (addr != 4)) {
                            fprintf(stderr, "ERROR -> invalid addressing size a = %d\n", addr);
                            return 1;
                        }
                        parameters->jpwl_sens_addr = addr;

                    } else if (!strcmp(token, "a")) {
                        /* default */
                        parameters->jpwl_sens_addr = addr; /* auto for default size */

                    } else {
                        fprintf(stderr, "ERROR -> invalid addressing selection = %s\n", token);
                        return 1;
                    };

                }

                /* search sensitivity size */
                if (*token == 'z') {


                    size = 1; /* predefined: 1 byte */

                    if (sscanf(token, "z=%d", &size) == 1) {
                        /* Specified */
                        if ((size != 0) && (size != 1) && (size != 2)) {
                            fprintf(stderr, "ERROR -> invalid sensitivity size z = %d\n", size);
                            return 1;
                        }
                        parameters->jpwl_sens_size = size;

                    } else if (!strcmp(token, "a")) {
                        /* default */
                        parameters->jpwl_sens_size = size; /* 1 for default size */

                    } else {
                        fprintf(stderr, "ERROR -> invalid size selection = %s\n", token);
                        return 1;
                    };

                }

                /* search range method */
                if (*token == 'g') {


                    range = 0; /* predefined: 0 (packet) */

                    if (sscanf(token, "g=%d", &range) == 1) {
                        /* Specified */
                        if ((range < 0) || (range > 3)) {
                            fprintf(stderr, "ERROR -> invalid sensitivity range method g = %d\n", range);
                            return 1;
                        }
                        parameters->jpwl_sens_range = range;

                    } else if (!strcmp(token, "g")) {
                        /* default */
                        parameters->jpwl_sens_range = range;

                    } else {
                        fprintf(stderr, "ERROR -> invalid range selection = %s\n", token);
                        return 1;
                    };

                }

                /* next token or bust */
                token = strtok(NULL, ",");
            };


            /* some info */
            fprintf(stdout, "Info: JPWL capabilities enabled\n");
            parameters->jpwl_epc_on = OPJ_TRUE;

        }
        break;
#endif /* USE_JPWL */
        /* <<UniPG */
        /* ------------------------------------------------------ */

        case 'J': {         /* jpip on */
            parameters->jpip_on = OPJ_TRUE;
        }
        break;
        /* ------------------------------------------------------ */


        default:
            fprintf(stderr, "[WARNING] An invalid option has been ignored\n");
            break;
        }
    } while (c != -1);

    if (img_fol->set_imgdir == 1) {
        if (!(parameters->infile[0] == 0)) {
            fprintf(stderr, "[ERROR] options -ImgDir and -i cannot be used together !!\n");
            return 1;
        }
        if (img_fol->set_out_format == 0) {
            fprintf(stderr,
                    "[ERROR] When -ImgDir is used, -OutFor <FORMAT> must be used !!\n");
            fprintf(stderr, "Only one format allowed! Valid formats are j2k and jp2!!\n");
            return 1;
        }
        if (!((parameters->outfile[0] == 0))) {
            fprintf(stderr, "[ERROR] options -ImgDir and -o cannot be used together !!\n");
            fprintf(stderr, "Specify OutputFormat using -OutFor<FORMAT> !!\n");
            return 1;
        }
    } else {
        if ((parameters->infile[0] == 0) || (parameters->outfile[0] == 0)) {
            fprintf(stderr, "[ERROR] Required parameters are missing\n"
                    "Example: %s -i image.pgm -o image.j2k\n", argv[0]);
            fprintf(stderr, "   Help: %s -h\n", argv[0]);
            return 1;
        }
    }

    if ((parameters->decod_format == RAW_DFMT && raw_cp->rawWidth == 0)
            || (parameters->decod_format == RAWL_DFMT && raw_cp->rawWidth == 0)) {
        fprintf(stderr, "[ERROR] invalid raw image parameters\n");
        fprintf(stderr, "Please use the Format option -F:\n");
        fprintf(stderr,
                "-F rawWidth,rawHeight,rawComp,rawBitDepth,s/u (Signed/Unsigned)\n");
        fprintf(stderr, "Example: -i lena.raw -o lena.j2k -F 512,512,3,8,u\n");
        fprintf(stderr, "Aborting\n");
        return 1;
    }

    if ((parameters->cp_disto_alloc || parameters->cp_fixed_alloc ||
            parameters->cp_fixed_quality)
            && (!(parameters->cp_disto_alloc ^ parameters->cp_fixed_alloc ^
                  parameters->cp_fixed_quality))) {
        fprintf(stderr, "[ERROR] options -r -q and -f cannot be used together !!\n");
        return 1;
    }               /* mod fixed_quality */


    /* if no rate entered, lossless by default */
    /* Note: post v2.2.0, this is no longer necessary, but for released */
    /* versions at the time of writing, this is needed to avoid crashes */
    if (parameters->tcp_numlayers == 0) {
        parameters->tcp_rates[0] = 0;
        parameters->tcp_numlayers++;
        parameters->cp_disto_alloc = 1;
    }

    if ((parameters->cp_tx0 > parameters->image_offset_x0) ||
            (parameters->cp_ty0 > parameters->image_offset_y0)) {
        fprintf(stderr,
                "[ERROR] Tile offset dimension is unnappropriate --> TX0(%d)<=IMG_X0(%d) TYO(%d)<=IMG_Y0(%d) \n",
                parameters->cp_tx0, parameters->image_offset_x0, parameters->cp_ty0,
                parameters->image_offset_y0);
        return 1;
    }

    for (i = 0; i < parameters->numpocs; i++) {
        if (parameters->POC[i].prg == -1) {
            fprintf(stderr,
                    "Unrecognized progression order in option -P (POC n %d) [LRCP, RLCP, RPCL, PCRL, CPRL] !!\n",
                    i + 1);
        }
    }

    /* If subsampled image is provided, automatically disable MCT */
    if (((parameters->decod_format == RAW_DFMT) ||
            (parameters->decod_format == RAWL_DFMT))
            && (((raw_cp->rawComp > 1) && ((raw_cp->rawComps[1].dx > 1) ||
                                           (raw_cp->rawComps[1].dy > 1)))
                || ((raw_cp->rawComp > 2) && ((raw_cp->rawComps[2].dx > 1) ||
                        (raw_cp->rawComps[2].dy > 1)))
               )) {
        parameters->tcp_mct = 0;
    }

    return 0;

}

/* -------------------------------------------------------------------------- */

/**
sample error debug callback expecting no client object
*/
static void error_callback(const char *msg, void *client_data)
{
    (void)client_data;
    fprintf(stdout, "[ERROR] %s", msg);
}
/**
sample warning debug callback expecting no client object
*/
static void warning_callback(const char *msg, void *client_data)
{
    (void)client_data;
    fprintf(stdout, "[WARNING] %s", msg);
}
/**
sample debug callback expecting no client object
*/
static void info_callback(const char *msg, void *client_data)
{
    (void)client_data;
    fprintf(stdout, "[INFO] %s", msg);
}

OPJ_FLOAT64 opj_clock(void)
{
#ifdef _WIN32
    /* _WIN32: use QueryPerformance (very accurate) */
    LARGE_INTEGER freq, t ;
    /* freq is the clock speed of the CPU */
    QueryPerformanceFrequency(&freq) ;
    /* cout << "freq = " << ((double) freq.QuadPart) << endl; */
    /* t is the high resolution performance counter (see MSDN) */
    QueryPerformanceCounter(& t) ;
    return freq.QuadPart ? ((OPJ_FLOAT64) t.QuadPart / (OPJ_FLOAT64) freq.QuadPart)
           : 0 ;
#else
    /* Unix or Linux: use resource usage */
    struct rusage t;
    OPJ_FLOAT64 procTime;
    /* (1) Get the rusage data structure at this moment (man getrusage) */
    getrusage(0, &t);
    /* (2) What is the elapsed time ? - CPU time = User time + System time */
    /* (2a) Get the seconds */
    procTime = (OPJ_FLOAT64)(t.ru_utime.tv_sec + t.ru_stime.tv_sec);
    /* (2b) More precisely! Get the microseconds part ! */
    return (procTime + (OPJ_FLOAT64)(t.ru_utime.tv_usec + t.ru_stime.tv_usec) *
            1e-6) ;
#endif
}


/* -------------------------------------------------------------------------- */
/**
 * OPJ_COMPRESS MAIN
 */
/* -------------------------------------------------------------------------- */
int main(int argc, char **argv)
{

    opj_cparameters_t parameters;   /* compression parameters */

    opj_stream_t *l_stream = 00;
    opj_codec_t* l_codec = 00;
    opj_image_t *image = NULL;
    raw_cparameters_t raw_cp;
    OPJ_SIZE_T num_compressed_files = 0;

    char indexfilename[OPJ_PATH_LEN];   /* index file name */

    unsigned int i, num_images, imageno;
    img_fol_t img_fol;
    dircnt_t *dirptr = NULL;

    OPJ_BOOL bSuccess;
    OPJ_BOOL bUseTiles = OPJ_FALSE; /* OPJ_TRUE */
    OPJ_UINT32 l_nb_tiles = 4;
    OPJ_FLOAT64 t = opj_clock();

    /* set encoding parameters to default values */
    opj_set_default_encoder_parameters(&parameters);

    /* Initialize indexfilename and img_fol */
    *indexfilename = 0;
    memset(&img_fol, 0, sizeof(img_fol_t));

    /* raw_cp initialization */
    raw_cp.rawBitDepth = 0;
    raw_cp.rawComp = 0;
    raw_cp.rawComps = 0;
    raw_cp.rawHeight = 0;
    raw_cp.rawSigned = 0;
    raw_cp.rawWidth = 0;

    /* parse input and get user encoding parameters */
    parameters.tcp_mct = (char)
                         255; /* This will be set later according to the input image or the provided option */
    if (parse_cmdline_encoder(argc, argv, &parameters, &img_fol, &raw_cp,
                              indexfilename, sizeof(indexfilename)) == 1) {
        goto fails;
    }

    /* Read directory if necessary */
    if (img_fol.set_imgdir == 1) {
        num_images = get_num_images(img_fol.imgdirpath);
        dirptr = (dircnt_t*)malloc(sizeof(dircnt_t));
        if (dirptr) {
            dirptr->filename_buf = (char*)malloc(num_images * OPJ_PATH_LEN * sizeof(
                    char)); /* Stores at max 10 image file names*/
            dirptr->filename = (char**) malloc(num_images * sizeof(char*));
            if (!dirptr->filename_buf) {
                return 0;
            }
            for (i = 0; i < num_images; i++) {
                dirptr->filename[i] = dirptr->filename_buf + i * OPJ_PATH_LEN;
            }
        }
        if (load_images(dirptr, img_fol.imgdirpath) == 1) {
            return 0;
        }
        if (num_images == 0) {
            fprintf(stdout, "Folder is empty\n");
            return 0;
        }
    } else {
        num_images = 1;
    }
    /*Encoding image one by one*/
    for (imageno = 0; imageno < num_images; imageno++) {
        image = NULL;
        fprintf(stderr, "\n");

        if (img_fol.set_imgdir == 1) {
            if (get_next_file((int)imageno, dirptr, &img_fol, &parameters)) {
                fprintf(stderr, "skipping file...\n");
                continue;
            }
        }

        switch (parameters.decod_format) {
        case PGX_DFMT:
            break;
        case PXM_DFMT:
            break;
        case BMP_DFMT:
            break;
        case TIF_DFMT:
            break;
        case RAW_DFMT:
        case RAWL_DFMT:
            break;
        case TGA_DFMT:
            break;
        case PNG_DFMT:
            break;
        default:
            fprintf(stderr, "skipping file...\n");
            continue;
        }

        /* decode the source image */
        /* ----------------------- */

        switch (parameters.decod_format) {
        case PGX_DFMT:
            image = pgxtoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load pgx file\n");
                return 1;
            }
            break;

        case PXM_DFMT:
            image = pnmtoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load pnm file\n");
                return 1;
            }
            break;

        case BMP_DFMT:
            image = bmptoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load bmp file\n");
                return 1;
            }
            break;

#ifdef OPJ_HAVE_LIBTIFF
        case TIF_DFMT:
            image = tiftoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load tiff file\n");
                return 1;
            }
            break;
#endif /* OPJ_HAVE_LIBTIFF */

        case RAW_DFMT:
            image = rawtoimage(parameters.infile, &parameters, &raw_cp);
            if (!image) {
                fprintf(stderr, "Unable to load raw file\n");
                return 1;
            }
            break;

        case RAWL_DFMT:
            image = rawltoimage(parameters.infile, &parameters, &raw_cp);
            if (!image) {
                fprintf(stderr, "Unable to load raw file\n");
                return 1;
            }
            break;

        case TGA_DFMT:
            image = tgatoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load tga file\n");
                return 1;
            }
            break;

#ifdef OPJ_HAVE_LIBPNG
        case PNG_DFMT:
            image = pngtoimage(parameters.infile, &parameters);
            if (!image) {
                fprintf(stderr, "Unable to load png file\n");
                return 1;
            }
            break;
#endif /* OPJ_HAVE_LIBPNG */
        }

        /* Can happen if input file is TIFF or PNG
        * and OPJ_HAVE_LIBTIF or OPJ_HAVE_LIBPNG is undefined
        */
        if (!image) {
            fprintf(stderr, "Unable to load file: got no image\n");
            return 1;
        }

        /* Decide if MCT should be used */
        if (parameters.tcp_mct == (char)
                255) { /* mct mode has not been set in commandline */
            parameters.tcp_mct = (image->numcomps >= 3) ? 1 : 0;
        } else {            /* mct mode has been set in commandline */
            if ((parameters.tcp_mct == 1) && (image->numcomps < 3)) {
                fprintf(stderr, "RGB->YCC conversion cannot be used:\n");
                fprintf(stderr, "Input image has less than 3 components\n");
                return 1;
            }
            if ((parameters.tcp_mct == 2) && (!parameters.mct_data)) {
                fprintf(stderr, "Custom MCT has been set but no array-based MCT\n");
                fprintf(stderr, "has been provided. Aborting.\n");
                return 1;
            }
        }

        /* encode the destination image */
        /* ---------------------------- */

        switch (parameters.cod_format) {
        case J2K_CFMT: { /* JPEG-2000 codestream */
            /* Get a decoder handle */
            l_codec = opj_create_compress(OPJ_CODEC_J2K);
            break;
        }
        case JP2_CFMT: { /* JPEG 2000 compressed image data */
            /* Get a decoder handle */
            l_codec = opj_create_compress(OPJ_CODEC_JP2);
            break;
        }
        default:
            fprintf(stderr, "skipping file..\n");
            opj_stream_destroy(l_stream);
            continue;
        }

        /* catch events using our callbacks and give a local context */
        opj_set_info_handler(l_codec, info_callback, 00);
        opj_set_warning_handler(l_codec, warning_callback, 00);
        opj_set_error_handler(l_codec, error_callback, 00);

        if (bUseTiles) {
            parameters.cp_tx0 = 0;
            parameters.cp_ty0 = 0;
            parameters.tile_size_on = OPJ_TRUE;
            parameters.cp_tdx = 512;
            parameters.cp_tdy = 512;
        }
        if (! opj_setup_encoder(l_codec, &parameters, image)) {
            fprintf(stderr, "failed to encode image: opj_setup_encoder\n");
            opj_destroy_codec(l_codec);
            opj_image_destroy(image);
            return 1;
        }

        /* open a byte stream for writing and allocate memory for all tiles */
        l_stream = opj_stream_create_default_file_stream(parameters.outfile, OPJ_FALSE);
        if (! l_stream) {
            return 1;
        }

        /* encode the image */
        bSuccess = opj_start_compress(l_codec, image, l_stream);
        if (!bSuccess)  {
            fprintf(stderr, "failed to encode image: opj_start_compress\n");
        }
        if (bSuccess && bUseTiles) {
            OPJ_BYTE *l_data;
            OPJ_UINT32 l_data_size = 512 * 512 * 3;
            l_data = (OPJ_BYTE*) calloc(1, l_data_size);
            if (l_data == NULL) {
                goto fails;
            }
            for (i = 0; i < l_nb_tiles; ++i) {
                if (! opj_write_tile(l_codec, i, l_data, l_data_size, l_stream)) {
                    fprintf(stderr, "ERROR -> test_tile_encoder: failed to write the tile %d!\n",
                            i);
                    opj_stream_destroy(l_stream);
                    opj_destroy_codec(l_codec);
                    opj_image_destroy(image);
                    return 1;
                }
            }
            free(l_data);
        } else {
            bSuccess = bSuccess && opj_encode(l_codec, l_stream);
            if (!bSuccess)  {
                fprintf(stderr, "failed to encode image: opj_encode\n");
            }
        }
        bSuccess = bSuccess && opj_end_compress(l_codec, l_stream);
        if (!bSuccess)  {
            fprintf(stderr, "failed to encode image: opj_end_compress\n");
        }

        if (!bSuccess)  {
            opj_stream_destroy(l_stream);
            opj_destroy_codec(l_codec);
            opj_image_destroy(image);
            fprintf(stderr, "failed to encode image\n");
            remove(parameters.outfile);
            return 1;
        }

        num_compressed_files++;
        fprintf(stdout, "[INFO] Generated outfile %s\n", parameters.outfile);
        /* close and free the byte stream */
        opj_stream_destroy(l_stream);

        /* free remaining compression structures */
        opj_destroy_codec(l_codec);

        /* free image data */
        opj_image_destroy(image);

    }

    /* free user parameters structure */
    if (parameters.cp_comment) {
        free(parameters.cp_comment);
    }
    if (parameters.cp_matrice) {
        free(parameters.cp_matrice);
    }
    if (raw_cp.rawComps) {
        free(raw_cp.rawComps);
    }

    t = opj_clock() - t;
    if (num_compressed_files) {
        fprintf(stdout, "encode time: %d ms \n",
                (int)((t * 1000.0) / (OPJ_FLOAT64)num_compressed_files));
    }

    return 0;

fails:
    if (parameters.cp_comment) {
        free(parameters.cp_comment);
    }
    if (parameters.cp_matrice) {
        free(parameters.cp_matrice);
    }
    if (raw_cp.rawComps) {
        free(raw_cp.rawComps);
    }
    if (img_fol.imgdirpath) {
        free(img_fol.imgdirpath);
    }
    if (dirptr) {
        if (dirptr->filename_buf) {
            free(dirptr->filename_buf);
        }
        if (dirptr->filename) {
            free(dirptr->filename);
        }
        free(dirptr);
    }
    return 1;
}
