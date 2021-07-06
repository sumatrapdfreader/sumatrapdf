
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
//    xgetopt.c  -- loosely based on System V getopt()
//
//    option ::= SW [optLetter]* [argLetter space* argument]
//   

#include <string.h>
#include <stdio.h>

int     xoptind = 1;   
char   *xoptarg;       

static  char   *nextArg = NULL;    

#define SW '-'


int xgetopt(int argc, char* argv[], char* optionS)
{
    unsigned char ch;
    char* optP;

    if (argc > xoptind)
    {

        if (nextArg == NULL)
        {
            if ((nextArg = argv[xoptind]) == NULL || *(nextArg++) != SW)  goto end_eof;
        }

        if ((ch = *(nextArg++)) == 0)
        {
            xoptind++;
            goto end_eof;
        }

        if (ch == ':' || (optP = strchr(optionS, ch)) == NULL)
            goto end_error;

        if (*(++optP) == ':')
        {
            xoptind++;

            if (*nextArg == 0)
            {
                if (argc <= xoptind)  goto  end_error;
                nextArg = argv[xoptind++];
            }

            xoptarg = nextArg;
            nextArg = NULL;

        }
        else
        {
            if (*nextArg == 0)
            {
                xoptind++;
                nextArg = NULL;
            }

            xoptarg = NULL;
        }

        return ch;
    }

end_eof:
    xoptarg = nextArg = NULL;
    return EOF;

end_error:
    xoptarg = NULL;
    return '?';
}
