/* $Id: extract_chmLib.c,v 1.4 2002/10/10 03:24:51 jedwin Exp $ */
/***************************************************************************
 *          extract_chmLib.c - CHM archive extractor                       *
 *                           -------------------                           *
 *                                                                         *
 *  author:     Jed Wing <jedwin@ugcs.caltech.edu>                         *
 *  notes:      This is a quick-and-dirty chm archive extractor.           *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include "chm_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(X, Y) _mkdir(X)
#define snprintf _snprintf
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

struct extract_context
{
    const char *base_path;
};

static int dir_exists(const char *path)
{
#ifdef WIN32
        /* why doesn't this work?!? */
        HANDLE hFile;

        hFile = CreateFileA(path,
                        FILE_LIST_DIRECTORY,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
        CloseHandle(hFile);
        return 1;
        }
        else
        return 0;
#else
        struct stat statbuf;
        if (stat(path, &statbuf) != -1)
                return 1;
        else
                return 0;
#endif
}

static int rmkdir(char *path)
{
    /*
     * strip off trailing components unless we can stat the directory, or we
     * have run out of components
     */

    char *i = strrchr(path, '/');

    if(path[0] == '\0'  ||  dir_exists(path))
        return 0;

    if (i != NULL)
    {
        *i = '\0';
        rmkdir(path);
        *i = '/';
        mkdir(path, 0777);
    }

#ifdef WIN32
        return 0;
#else
    if (dir_exists(path))
        return 0;
    else
        return -1;
#endif
}

/*
 * callback function for enumerate API
 */
int _extract_callback(struct chmFile *h,
              struct chmUnitInfo *ui,
              void *context)
{
    LONGUINT64 ui_path_len;
    char buffer[32768];
    struct extract_context *ctx = (struct extract_context *)context;
    char *i;

    if (ui->path[0] != '/')
        return CHM_ENUMERATOR_CONTINUE;

    /* quick hack for security hole mentioned by Sven Tantau */
    if (strstr(ui->path, "/../") != NULL)
    {
        /* fprintf(stderr, "Not extracting %s (dangerous path)\n", ui->path); */
        return CHM_ENUMERATOR_CONTINUE;
    }

    if (snprintf(buffer, sizeof(buffer), "%s%s", ctx->base_path, ui->path) > 1024)
        return CHM_ENUMERATOR_FAILURE;

    /* Get the length of the path */
    ui_path_len = strlen(ui->path)-1;

    /* Distinguish between files and dirs */
    if (ui->path[ui_path_len] != '/' )
    {
        FILE *fout;
        LONGINT64 len, remain=ui->length;
        LONGUINT64 offset = 0;

        printf("--> %s\n", ui->path);
        if ((fout = fopen(buffer, "wb")) == NULL)
	{
	    /* make sure that it isn't just a missing directory before we abort */ 
	    char newbuf[32768];
	    strcpy(newbuf, buffer);
	    i = strrchr(newbuf, '/');
	    *i = '\0';
	    rmkdir(newbuf);
	    if ((fout = fopen(buffer, "wb")) == NULL)
              return CHM_ENUMERATOR_FAILURE;
	}

        while (remain != 0)
        {
            len = chm_retrieve_object(h, ui, (unsigned char *)buffer, offset, 32768);
            if (len > 0)
            {
                fwrite(buffer, 1, (size_t)len, fout);
                offset += len;
                remain -= len;
            }
            else
            {
                fprintf(stderr, "incomplete file: %s\n", ui->path);
                break;
            }
        }

        fclose(fout);
    }
    else
    {
        if (rmkdir(buffer) == -1)
            return CHM_ENUMERATOR_FAILURE;
    }

    return CHM_ENUMERATOR_CONTINUE;
}

int main(int c, char **v)
{
    struct chmFile *h;
    struct extract_context ec;

    if (c < 3)
    {
        fprintf(stderr, "usage: %s <chmfile> <outdir>\n", v[0]);
        exit(1);
    }

    h = chm_open(v[1]);
    if (h == NULL)
    {
        fprintf(stderr, "failed to open %s\n", v[1]);
        exit(1);
    }

    printf("%s:\n", v[1]);
    ec.base_path = v[2];
    if (! chm_enumerate(h,
                        CHM_ENUMERATE_ALL,
                        _extract_callback,
                        (void *)&ec))
        printf("   *** ERROR ***\n");

    chm_close(h);

    return 0;
}
