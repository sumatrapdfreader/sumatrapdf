/*
    getopt.c

*/

#include <errno.h>
#include <string.h>
#include <stdio.h>

int     xoptind = 1;    /* index of which argument is next  */
char   *xoptarg;        /* pointer to argument of current option */
int     xopterr = 0;    /* allow error message  */

static  char   *letP = NULL;    /* remember next option char's location */
char    SW = '-';				/* DOS switch character, either '-' or '/' */

/*
  Parse the command line options, System V style.

  Standard option syntax is:

    option ::= SW [optLetter]* [argLetter space* argument]

*/

int xgetopt(int argc, char *argv[], char *optionS)
{
    unsigned char ch;
    char *optP;

    if (SW == 0) {
        SW = '/';
    }

    if (argc > xoptind) {
        if (letP == NULL) {
            if ((letP = argv[xoptind]) == NULL ||
                *(letP++) != SW)  goto gopEOF;
            if (*letP == SW) {
                xoptind++;  goto gopEOF;
            }
        }
        if (0 == (ch = *(letP++))) {
            xoptind++;  goto gopEOF;
        }
        if (':' == ch  ||  (optP = strchr(optionS, ch)) == NULL)
            goto gopError;
        if (':' == *(++optP)) {
            xoptind++;
            if (0 == *letP) {
                if (argc <= xoptind)  goto  gopError;
                letP = argv[xoptind++];
            }
            xoptarg = letP;
            letP = NULL;
        } else {
            if (0 == *letP) {
                xoptind++;
                letP = NULL;
            }
            xoptarg = NULL;
        }
        return ch;
    }
gopEOF:
    xoptarg = letP = NULL;
    return EOF;

gopError:
    xoptarg = NULL;
    errno  = EINVAL;
    if (xopterr)
        perror ("get command line option");
    return ('?');
}
