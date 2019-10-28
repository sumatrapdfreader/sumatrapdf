/*
 * $Id: addXMLinJP2.c 46 2011-02-17 14:50:55Z kaori $
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2010-2011, Kaori Hagihara
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

/*! \file
 *  \brief addXMLinJP2 is a program to embed metadata into JP2 file
 *
 *  \section impinst Implementing instructions
 *  This program takes two arguments. \n
 *   -# Input/output image file in JP2 format, this JP2 file is being modified
 *   -# Input XML file with metadata contents\n
 *   % ./addXMLinJP2 image.jp2 metadata.xml\n
 *
 *  Currently, this program does not parse XML file, and the XML file contents is directly embedded as a XML Box.\n
 *  The following is an example of XML file contents specifying Region Of Interests with target names.\n
 *  <xmlbox>\n
 *    <roi name="island" x="1890" y="1950" w="770" h="310"/>\n
 *    <roi name="ship" x="750" y="330" w="100" h="60"/>\n
 *    <roi name="airport" x="650" y="1800" w="650" h="800"/>\n
 *    <roi name="harbor" x="4200" y="1650" w="130" h="130"/>\n
 *  </xmlbox>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/**
 * Open JP2 file with the check of JP2 header
 *
 * @param[in] filename file name string
 * @return             file descriptor
 */
FILE * open_jp2file(const char filename[]);


/**
 * read xml file without any format check for the moment
 *
 * @param[in]  filename file name string
 * @param[out] fsize    file byte size
 * @return              pointer to the xml file content buffer
 */
char * read_xmlfile(const char filename[], long *fsize);

int main(int argc, char *argv[])
{
    FILE *fp;
    char *xmldata, type[] = "xml ";
    long fsize, boxsize;

    if (argc < 3) {
        fprintf(stderr, "USAGE: %s modifing.jp2 adding.xml\n", argv[0]);
        return -1;
    }

    fp = open_jp2file(argv[1]);
    if (!fp) {
        return -1;
    }

    xmldata = read_xmlfile(argv[2], &fsize);
    if (fsize < 0) {
        return -1;
    }
    boxsize = fsize + 8;

    fputc((boxsize >> 24) & 0xff, fp);
    fputc((boxsize >> 16) & 0xff, fp);
    fputc((boxsize >> 8) & 0xff, fp);
    fputc(boxsize & 0xff, fp);
    fwrite(type, 4, 1, fp);
    fwrite(xmldata, (size_t)fsize, 1, fp);

    free(xmldata);
    fclose(fp);

    return 0;
}

FILE * open_jp2file(const char filename[])
{
    FILE *fp;
    char *data;

    if (!(fp = fopen(filename, "a+b"))) {
        fprintf(stderr, "Original JP2 %s not found\n", filename);
        return NULL;
    }
    /* Check resource is a JP family file. */
    if (fseek(fp, 0, SEEK_SET) == -1) {
        fclose(fp);
        fprintf(stderr, "Original JP2 %s broken (fseek error)\n", filename);
        return NULL;
    }

    data = (char *)malloc(12);  /* size of header */
    if (fread(data, 12, 1, fp) != 1) {
        free(data);
        fclose(fp);
        fprintf(stderr, "Original JP2 %s broken (read error)\n", filename);
        return NULL;
    }

    if (*data || *(data + 1) || *(data + 2) ||
            *(data + 3) != 12 || strncmp(data + 4, "jP  \r\n\x87\n", 8)) {
        free(data);
        fclose(fp);
        fprintf(stderr, "No JPEG 2000 Signature box in target %s\n", filename);
        return NULL;
    }
    free(data);
    return fp;
}

char * read_xmlfile(const char filename[], long *fsize)
{
    FILE *fp;
    char *data;

    /*  fprintf( stderr, "open %s\n", filename);*/
    if (!(fp = fopen(filename, "r"))) {
        fprintf(stderr, "XML file %s not found\n", filename);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) == -1) {
        fprintf(stderr, "XML file %s broken (seek error)\n", filename);
        fclose(fp);
        return NULL;
    }

    if ((*fsize = ftell(fp)) == -1) {
        fprintf(stderr, "XML file %s broken (seek error)\n", filename);
        fclose(fp);
        return NULL;
    }
    assert(*fsize >= 0);

    if (fseek(fp, 0, SEEK_SET) == -1) {
        fprintf(stderr, "XML file %s broken (seek error)\n", filename);
        fclose(fp);
        return NULL;
    }

    data = (char *)malloc((size_t) * fsize);

    if (fread(data, (size_t)*fsize, 1, fp) != 1) {
        fprintf(stderr, "XML file %s broken (read error)\n", filename);
        free(data);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    return data;
}
