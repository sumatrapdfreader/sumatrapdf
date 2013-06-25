#ifndef MUPDF_FITZ_GETOPT_H
#define MUPDF_FITZ_GETOPT_H

/*
	getopt: Simple functions/variables for use in tools.
*/
extern int fz_getopt(int nargc, char * const *nargv, const char *ostr);
extern int fz_optind;
extern char *fz_optarg;

#endif
