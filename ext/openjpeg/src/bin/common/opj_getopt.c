/*
 * The copyright in this software is being made available under the 3-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 1987, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* last review : october 29th, 2002 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)opj_getopt.c	8.3 (Berkeley) 4/27/95";
#endif              /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opj_getopt.h"

int opj_opterr = 1,         /* if error message should be printed */
    opj_optind = 1,            /* index into parent argv vector */
    opj_optopt,            /* character checked for validity */
    opj_optreset;          /* reset getopt */
char *opj_optarg;          /* argument associated with option */

#define BADCH   (int)'?'
#define BADARG  (int)':'
static char EMSG[] = {""};

/* As this class remembers its values from one Java call to the other, reset the values before each use */
void opj_reset_options_reading(void)
{
    opj_opterr = 1;
    opj_optind = 1;
}

/*
 * getopt --
 *  Parse argc/argv argument vector.
 */
int opj_getopt(int nargc, char *const *nargv, const char *ostr)
{
#  define __progname nargv[0]
    static char *place = EMSG;    /* option letter processing */
    const char *oli = NULL;   /* option letter list index */

    if (opj_optreset || !*place) {    /* update scanning pointer */
        opj_optreset = 0;
        if (opj_optind >= nargc || *(place = nargv[opj_optind]) != '-') {
            place = EMSG;
            return (-1);
        }
        if (place[1] && *++place == '-') {  /* found "--" */
            ++opj_optind;
            place = EMSG;
            return (-1);
        }
    }             /* option letter okay? */
    if ((opj_optopt = (int) * place++) == (int) ':' ||
            !(oli = strchr(ostr, opj_optopt))) {
        /*
         * if the user didn't specify '-' as an option,
         * assume it means -1.
         */
        if (opj_optopt == (int) '-') {
            return (-1);
        }
        if (!*place) {
            ++opj_optind;
        }
        if (opj_opterr && *ostr != ':') {
            fprintf(stderr,
                    "%s: illegal option -- %c\n", __progname, opj_optopt);
            return (BADCH);
        }
    }
    if (*++oli != ':') {      /* don't need argument */
        opj_optarg = NULL;
        if (!*place) {
            ++opj_optind;
        }
    } else {          /* need an argument */
        if (*place) {       /* no white space */
            opj_optarg = place;
        } else if (nargc <= ++opj_optind) { /* no arg */
            place = EMSG;
            if (*ostr == ':') {
                return (BADARG);
            }
            if (opj_opterr) {
                fprintf(stderr,
                        "%s: option requires an argument -- %c\n",
                        __progname, opj_optopt);
                return (BADCH);
            }
        } else {        /* white space */
            opj_optarg = nargv[opj_optind];
        }
        place = EMSG;
        ++opj_optind;
    }
    return (opj_optopt);      /* dump back option letter */
}


int opj_getopt_long(int argc, char * const argv[], const char *optstring,
                    const opj_option_t *longopts, int totlen)
{
    static int lastidx, lastofs;
    const char *tmp;
    int i, len;
    char param = 1;

again:
    if (opj_optind >= argc || !argv[opj_optind] || *argv[opj_optind] != '-') {
        return -1;
    }

    if (argv[opj_optind][0] == '-' && argv[opj_optind][1] == 0) {
        if (opj_optind >= (argc - 1)) { /* no more input parameters */
            param = 0;
        } else { /* more input parameters */
            if (argv[opj_optind + 1][0] == '-') {
                param = 0; /* Missing parameter after '-' */
            } else {
                param = 2;
            }
        }
    }

    if (param == 0) {
        ++opj_optind;
        return (BADCH);
    }

    if (argv[opj_optind][0] == '-') { /* long option */
        char* arg;
        const opj_option_t* o;
        o = longopts;
        len = sizeof(longopts[0]);

        if (param > 1) {
            if (opj_optind + 1 >= argc) {
                return -1;
            }
            arg = argv[opj_optind + 1];
            opj_optind++;
        } else {
            arg = argv[opj_optind] + 1;
        }

        if (strlen(arg) > 1) {
            for (i = 0; i < totlen; i = i + len, o++) {
                if (!strcmp(o->name, arg)) { /* match */
                    if (o->has_arg == 0) {
                        if ((argv[opj_optind + 1]) && (!(argv[opj_optind + 1][0] == '-'))) {
                            fprintf(stderr, "%s: option does not require an argument. Ignoring %s\n", arg,
                                    argv[opj_optind + 1]);
                            ++opj_optind;
                        }
                    } else {
                        opj_optarg = argv[opj_optind + 1];
                        if (opj_optarg) {
                            if (opj_optarg[0] ==
                                    '-') { /* Has read next input parameter: No arg for current parameter */
                                if (opj_opterr) {
                                    fprintf(stderr, "%s: option requires an argument\n", arg);
                                    return (BADCH);
                                }
                            }
                        }
                        if (!opj_optarg && o->has_arg == 1) { /* no argument there */
                            if (opj_opterr) {
                                fprintf(stderr, "%s: option requires an argument \n", arg);
                                return (BADCH);
                            }
                        }
                        ++opj_optind;
                    }
                    ++opj_optind;
                    if (o->flag) {
                        *(o->flag) = o->val;
                    } else {
                        return o->val;
                    }
                    return 0;
                }
            }/*(end for)String not found in the list*/
            fprintf(stderr, "Invalid option %s\n", arg);
            ++opj_optind;
            return (BADCH);
        } else { /*Single character input parameter*/
            if (*optstring == ':') {
                return ':';
            }
            if (lastidx != opj_optind) {
                lastidx = opj_optind;
                lastofs = 0;
            }
            opj_optopt = argv[opj_optind][lastofs + 1];
            if ((tmp = strchr(optstring, opj_optopt))) { /*Found input parameter in list*/
                if (*tmp == 0) { /* apparently, we looked for \0, i.e. end of argument */
                    ++opj_optind;
                    goto again;
                }
                if (tmp[1] == ':') { /* argument expected */
                    if (tmp[2] == ':' ||
                            argv[opj_optind][lastofs + 2]) { /* "-foo", return "oo" as opj_optarg */
                        if (!*(opj_optarg = argv[opj_optind] + lastofs + 2)) {
                            opj_optarg = 0;
                        }
                        goto found;
                    }
                    opj_optarg = argv[opj_optind + 1];
                    if (opj_optarg) {
                        if (opj_optarg[0] ==
                                '-') { /* Has read next input parameter: No arg for current parameter */
                            if (opj_opterr) {
                                fprintf(stderr, "%s: option requires an argument\n", arg);
                                ++opj_optind;
                                return (BADCH);
                            }
                        }
                    }
                    if (!opj_optarg) {  /* missing argument */
                        if (opj_opterr) {
                            fprintf(stderr, "%s: option requires an argument\n", arg);
                            ++opj_optind;
                            return (BADCH);
                        }
                    }
                    ++opj_optind;
                } else {/*Argument not expected*/
                    ++lastofs;
                    return opj_optopt;
                }
found:
                ++opj_optind;
                return opj_optopt;
            }   else {  /* not found */
                fprintf(stderr, "Invalid option %s\n", arg);
                ++opj_optind;
                return (BADCH);
            }/*end of not found*/

        }/* end of single character*/
    }/*end '-'*/
    fprintf(stderr, "Invalid option\n");
    ++opj_optind;
    return (BADCH);;
}/*end function*/
