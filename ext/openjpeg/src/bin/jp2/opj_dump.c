/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2010, Mathieu Malaterre, GDCM
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
#include "opj_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include "windirent.h"
#else
#include <dirent.h>
#endif /* _WIN32 */

#ifdef _WIN32
#include <windows.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif /* _WIN32 */

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
    const char *out_format;
    /** Enable option*/
    char set_imgdir;
    /** Enable Cod Format for output*/
    char set_out_format;

    int flag;
} img_fol_t;

/* -------------------------------------------------------------------------- */
/* Declarations                                                               */
static int get_num_images(char *imgdirpath);
static int load_images(dircnt_t *dirptr, char *imgdirpath);
static int get_file_format(const char *filename);
static char get_next_file(int imageno, dircnt_t *dirptr, img_fol_t *img_fol,
                          opj_dparameters_t *parameters);
static int infile_format(const char *fname);

static int parse_cmdline_decoder(int argc, char **argv,
                                 opj_dparameters_t *parameters, img_fol_t *img_fol);

/* -------------------------------------------------------------------------- */
static void decode_help_display(void)
{
    fprintf(stdout, "\nThis is the opj_dump utility from the OpenJPEG project.\n"
            "It dumps JPEG 2000 codestream info to stdout or a given file.\n"
            "It has been compiled against openjp2 library v%s.\n\n", opj_version());

    fprintf(stdout, "Parameters:\n");
    fprintf(stdout, "-----------\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "  -ImgDir <directory>\n");
    fprintf(stdout, "	Image file Directory path \n");
    fprintf(stdout, "  -i <compressed file>\n");
    fprintf(stdout,
            "    REQUIRED only if an Input image directory not specified\n");
    fprintf(stdout,
            "    Currently accepts J2K-files, JP2-files and JPT-files. The file type\n");
    fprintf(stdout, "    is identified based on its suffix.\n");
    fprintf(stdout, "  -o <output file>\n");
    fprintf(stdout, "    OPTIONAL\n");
    fprintf(stdout, "    Output file where file info will be dump.\n");
    fprintf(stdout, "    By default it will be in the stdout.\n");
    fprintf(stdout, "  -v "); /* FIXME WIP_MSD */
    fprintf(stdout, "    OPTIONAL\n");
    fprintf(stdout, "    Enable informative messages\n");
    fprintf(stdout, "    By default verbose mode is off.\n");
    fprintf(stdout, "\n");
}

/* -------------------------------------------------------------------------- */
static int get_num_images(char *imgdirpath)
{
    DIR *dir;
    struct dirent* content;
    int num_images = 0;

    /*Reading the input images from given input directory*/

    dir = opendir(imgdirpath);
    if (!dir) {
        fprintf(stderr, "Could not open Folder %s\n", imgdirpath);
        return 0;
    }

    while ((content = readdir(dir)) != NULL) {
        if (strcmp(".", content->d_name) == 0 || strcmp("..", content->d_name) == 0) {
            continue;
        }
        num_images++;
    }
    closedir(dir);
    return num_images;
}

/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
static int get_file_format(const char *filename)
{
    unsigned int i;
    static const char *extension[] = {"pgx", "pnm", "pgm", "ppm", "bmp", "tif", "raw", "tga", "png", "j2k", "jp2", "jpt", "j2c", "jpc"  };
    static const int format[] = { PGX_DFMT, PXM_DFMT, PXM_DFMT, PXM_DFMT, BMP_DFMT, TIF_DFMT, RAW_DFMT, TGA_DFMT, PNG_DFMT, J2K_CFMT, JP2_CFMT, JPT_CFMT, J2K_CFMT, J2K_CFMT };
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return -1;
    }
    ext++;
    if (ext) {
        for (i = 0; i < sizeof(format) / sizeof(*format); i++) {
            if (_strnicmp(ext, extension[i], 3) == 0) {
                return format[i];
            }
        }
    }

    return -1;
}

/* -------------------------------------------------------------------------- */
static char get_next_file(int imageno, dircnt_t *dirptr, img_fol_t *img_fol,
                          opj_dparameters_t *parameters)
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
    strcpy(temp_ofname, strtok(image_filename, "."));
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

/* -------------------------------------------------------------------------- */
#define JP2_RFC3745_MAGIC "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a"
#define JP2_MAGIC "\x0d\x0a\x87\x0a"
/* position 45: "\xff\x52" */
#define J2K_CODESTREAM_MAGIC "\xff\x4f\xff\x51"

static int infile_format(const char *fname)
{
    FILE *reader;
    const char *s, *magic_s;
    int ext_format, magic_format;
    unsigned char buf[12];
    size_t l_nb_read;

    reader = fopen(fname, "rb");

    if (reader == NULL) {
        return -1;
    }

    memset(buf, 0, 12);
    l_nb_read = fread(buf, 1, 12, reader);
    fclose(reader);
    if (l_nb_read != 12) {
        return -1;
    }



    ext_format = get_file_format(fname);

    if (ext_format == JPT_CFMT) {
        return JPT_CFMT;
    }

    if (memcmp(buf, JP2_RFC3745_MAGIC, 12) == 0 || memcmp(buf, JP2_MAGIC, 4) == 0) {
        magic_format = JP2_CFMT;
        magic_s = ".jp2";
    } else if (memcmp(buf, J2K_CODESTREAM_MAGIC, 4) == 0) {
        magic_format = J2K_CFMT;
        magic_s = ".j2k or .jpc or .j2c";
    } else {
        return -1;
    }

    if (magic_format == ext_format) {
        return ext_format;
    }

    s = fname + strlen(fname) - 4;

    fputs("\n===========================================\n", stderr);
    fprintf(stderr, "The extension of this file is incorrect.\n"
            "FOUND %s. SHOULD BE %s\n", s, magic_s);
    fputs("===========================================\n", stderr);

    return magic_format;
}
/* -------------------------------------------------------------------------- */
/**
 * Parse the command line
 */
/* -------------------------------------------------------------------------- */
static int parse_cmdline_decoder(int argc, char **argv,
                                 opj_dparameters_t *parameters, img_fol_t *img_fol)
{
    int totlen, c;
    opj_option_t long_option[] = {
        {"ImgDir", REQ_ARG, NULL, 'y'}
    };
    const char optlist[] = "i:o:f:hv";

    totlen = sizeof(long_option);
    img_fol->set_out_format = 0;
    do {
        c = opj_getopt_long(argc, argv, optlist, long_option, totlen);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'i': {         /* input file */
            char *infile = opj_optarg;
            parameters->decod_format = infile_format(infile);
            switch (parameters->decod_format) {
            case J2K_CFMT:
                break;
            case JP2_CFMT:
                break;
            case JPT_CFMT:
                break;
            default:
                fprintf(stderr,
                        "[ERROR] Unknown input file format: %s \n"
                        "        Known file formats are *.j2k, *.jp2, *.jpc or *.jpt\n",
                        infile);
                return 1;
            }
            if (opj_strcpy_s(parameters->infile, sizeof(parameters->infile), infile) != 0) {
                fprintf(stderr, "[ERROR] Path is too long\n");
                return 1;
            }
        }
        break;

        /* ------------------------------------------------------ */

        case 'o': {   /* output file */
            if (opj_strcpy_s(parameters->outfile, sizeof(parameters->outfile),
                             opj_optarg) != 0) {
                fprintf(stderr, "[ERROR] Path is too long\n");
                return 1;
            }
        }
        break;

        /* ----------------------------------------------------- */
        case 'f':             /* flag */
            img_fol->flag = atoi(opj_optarg);
            break;
        /* ----------------------------------------------------- */

        case 'h':           /* display an help description */
            decode_help_display();
            return 1;

        /* ------------------------------------------------------ */

        case 'y': {         /* Image Directory path */
            img_fol->imgdirpath = (char*)malloc(strlen(opj_optarg) + 1);
            if (img_fol->imgdirpath == NULL) {
                return 1;
            }
            strcpy(img_fol->imgdirpath, opj_optarg);
            img_fol->set_imgdir = 1;
        }
        break;

        /* ----------------------------------------------------- */

        case 'v': {         /* Verbose mode */
            parameters->m_verbose = 1;
        }
        break;

        /* ----------------------------------------------------- */
        default:
            fprintf(stderr, "[WARNING] An invalid option has been ignored.\n");
            break;
        }
    } while (c != -1);

    /* check for possible errors */
    if (img_fol->set_imgdir == 1) {
        if (!(parameters->infile[0] == 0)) {
            fprintf(stderr, "[ERROR] options -ImgDir and -i cannot be used together.\n");
            return 1;
        }
        if (img_fol->set_out_format == 0) {
            fprintf(stderr,
                    "[ERROR] When -ImgDir is used, -OutFor <FORMAT> must be used.\n");
            fprintf(stderr, "Only one format allowed.\n"
                    "Valid format are PGM, PPM, PNM, PGX, BMP, TIF, RAW and TGA.\n");
            return 1;
        }
        if (!(parameters->outfile[0] == 0)) {
            fprintf(stderr, "[ERROR] options -ImgDir and -o cannot be used together\n");
            return 1;
        }
    } else {
        if (parameters->infile[0] == 0) {
            fprintf(stderr, "[ERROR] Required parameter is missing\n");
            fprintf(stderr, "Example: %s -i image.j2k\n", argv[0]);
            fprintf(stderr, "   Help: %s -h\n", argv[0]);
            return 1;
        }
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

/* -------------------------------------------------------------------------- */
/**
 * OPJ_DUMP MAIN
 */
/* -------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    FILE *fout = NULL;

    opj_dparameters_t parameters;           /* Decompression parameters */
    opj_image_t* image = NULL;                  /* Image structure */
    opj_codec_t* l_codec = NULL;                /* Handle to a decompressor */
    opj_stream_t *l_stream = NULL;              /* Stream */
    opj_codestream_info_v2_t* cstr_info = NULL;
    opj_codestream_index_t* cstr_index = NULL;

    OPJ_INT32 num_images, imageno;
    img_fol_t img_fol;
    dircnt_t *dirptr = NULL;

    /* Set decoding parameters to default values */
    opj_set_default_decoder_parameters(&parameters);

    /* Initialize img_fol */
    memset(&img_fol, 0, sizeof(img_fol_t));
    img_fol.flag = OPJ_IMG_INFO | OPJ_J2K_MH_INFO | OPJ_J2K_MH_IND;

    /* Parse input and get user encoding parameters */
    if (parse_cmdline_decoder(argc, argv, &parameters, &img_fol) == 1) {
        if (img_fol.imgdirpath) {
            free(img_fol.imgdirpath);
        }

        return EXIT_FAILURE;
    }

    /* Initialize reading of directory */
    if (img_fol.set_imgdir == 1) {
        int it_image;
        num_images = get_num_images(img_fol.imgdirpath);

        dirptr = (dircnt_t*)malloc(sizeof(dircnt_t));
        if (!dirptr) {
            return EXIT_FAILURE;
        }
        dirptr->filename_buf = (char*)malloc((size_t)num_images * OPJ_PATH_LEN * sizeof(
                char)); /* Stores at max 10 image file names*/
        if (!dirptr->filename_buf) {
            free(dirptr);
            return EXIT_FAILURE;
        }
        dirptr->filename = (char**) malloc((size_t)num_images * sizeof(char*));

        if (!dirptr->filename) {
            goto fails;
        }

        for (it_image = 0; it_image < num_images; it_image++) {
            dirptr->filename[it_image] = dirptr->filename_buf + it_image * OPJ_PATH_LEN;
        }

        if (load_images(dirptr, img_fol.imgdirpath) == 1) {
            goto fails;
        }

        if (num_images == 0) {
            fprintf(stdout, "Folder is empty\n");
            goto fails;
        }
    } else {
        num_images = 1;
    }

    /* Try to open for writing the output file if necessary */
    if (parameters.outfile[0] != 0) {
        fout = fopen(parameters.outfile, "w");
        if (!fout) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", parameters.outfile);
            goto fails;
        }
    } else {
        fout = stdout;
    }

    /* Read the header of each image one by one */
    for (imageno = 0; imageno < num_images ; imageno++) {

        fprintf(stderr, "\n");

        if (img_fol.set_imgdir == 1) {
            if (get_next_file(imageno, dirptr, &img_fol, &parameters)) {
                fprintf(stderr, "skipping file...\n");
                continue;
            }
        }

        /* Read the input file and put it in memory */
        /* ---------------------------------------- */

        l_stream = opj_stream_create_default_file_stream(parameters.infile, 1);
        if (!l_stream) {
            fprintf(stderr, "ERROR -> failed to create the stream from the file %s\n",
                    parameters.infile);
            goto fails;
        }

        /* Read the JPEG2000 stream */
        /* ------------------------ */

        switch (parameters.decod_format) {
        case J2K_CFMT: { /* JPEG-2000 codestream */
            /* Get a decoder handle */
            l_codec = opj_create_decompress(OPJ_CODEC_J2K);
            break;
        }
        case JP2_CFMT: { /* JPEG 2000 compressed image data */
            /* Get a decoder handle */
            l_codec = opj_create_decompress(OPJ_CODEC_JP2);
            break;
        }
        case JPT_CFMT: { /* JPEG 2000, JPIP */
            /* Get a decoder handle */
            l_codec = opj_create_decompress(OPJ_CODEC_JPT);
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

        parameters.flags |= OPJ_DPARAMETERS_DUMP_FLAG;

        /* Setup the decoder decoding parameters using user parameters */
        if (!opj_setup_decoder(l_codec, &parameters)) {
            fprintf(stderr, "ERROR -> opj_dump: failed to setup the decoder\n");
            opj_stream_destroy(l_stream);
            opj_destroy_codec(l_codec);
            fclose(fout);
            goto fails;
        }

        /* Read the main header of the codestream and if necessary the JP2 boxes*/
        if (! opj_read_header(l_stream, l_codec, &image)) {
            fprintf(stderr, "ERROR -> opj_dump: failed to read the header\n");
            opj_stream_destroy(l_stream);
            opj_destroy_codec(l_codec);
            opj_image_destroy(image);
            fclose(fout);
            goto fails;
        }

        opj_dump_codec(l_codec, img_fol.flag, fout);

        cstr_info = opj_get_cstr_info(l_codec);

        cstr_index = opj_get_cstr_index(l_codec);

        /* close the byte stream */
        opj_stream_destroy(l_stream);

        /* free remaining structures */
        if (l_codec) {
            opj_destroy_codec(l_codec);
        }

        /* destroy the image header */
        opj_image_destroy(image);

        /* destroy the codestream index */
        opj_destroy_cstr_index(&cstr_index);

        /* destroy the codestream info */
        opj_destroy_cstr_info(&cstr_info);

    }

    /* Close the output file */
    fclose(fout);

    return EXIT_SUCCESS;

fails:
    if (dirptr) {
        if (dirptr->filename) {
            free(dirptr->filename);
        }
        if (dirptr->filename_buf) {
            free(dirptr->filename_buf);
        }
        free(dirptr);
    }
    return EXIT_FAILURE;
}
