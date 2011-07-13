/* $Id: test_chmLib.c,v 1.5 2002/10/09 12:38:12 jedwin Exp $ */
/***************************************************************************
 *          test_chmLib.c - CHM archive test driver                        *
 *                           -------------------                           *
 *                                                                         *
 *  author:     Jed Wing <jedwin@ugcs.caltech.edu>                         *
 *  notes:      This is the quick-and-dirty test driver for the chm lib    *
 *              routines.  The program takes as its inputs the path to a   *
 *              .chm file, a path within the .chm file, and a destination  *
 *              path.  It attempts to open the .chm file, locate the       *
 *              desired file in the archive, and extract to the specified  *
 *              destination.                                               *
 *                                                                         *
 *              It is not included as a particularly useful program, but   *
 *              rather as a sort of "simplest possible" example of how to  *
 *              use the resolve/retrieve portion of the API.               *
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

#ifdef WIN32
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>

int main(int c, char **v)
{
    struct chmFile *h;
    struct chmUnitInfo ui;

    if (c < 4)
    {
        fprintf(stderr, "usage: %s <chmfile> <filename> <destfile>\n", v[0]);
        exit(1);
    }

    h = chm_open(v[1]);
    if (h == NULL)
    {
        fprintf(stderr, "failed to open %s\n", v[1]);
        exit(1);
    }

    printf("resolving %s\n", v[2]);
    if (CHM_RESOLVE_SUCCESS == chm_resolve_object(h, 
                                                  v[2],
                                                  &ui))
    {
#ifdef WIN32
        unsigned char *buffer = (unsigned char *)alloca((unsigned int)ui.length);
#else
        unsigned char buffer[ui.length];
#endif
        LONGINT64 gotLen;
        FILE *fout;
        printf("    object: <%d, %lu, %lu>\n",
               ui.space,
               (unsigned long)ui.start,
               (unsigned long)ui.length);

        printf("extracting to '%s'\n", v[3]);
        gotLen = chm_retrieve_object(h, &ui, buffer, 0, ui.length);
        if (gotLen == 0)
        {
            printf("   extract failed\n");
            return 2;
        }
        else if ((fout = fopen(v[3], "wb")) == NULL)
        {
            printf("   create failed\n");
            return 3;
        }
        else
        {
            fwrite(buffer, 1, (unsigned int)ui.length, fout);
            fclose(fout);
            printf("   finished\n");
        }
    }
    else
        printf("    failed\n");

    return 0;
}
