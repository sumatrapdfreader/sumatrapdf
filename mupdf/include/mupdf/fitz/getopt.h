#ifndef MUPDF_FITZ_GETOPT_H
#define MUPDF_FITZ_GETOPT_H

/**
	Simple functions/variables for use in tools.
*/
extern int fz_getopt(int nargc, char * const *nargv, const char *ostr);
FZ_DATA extern int fz_optind;
FZ_DATA extern char *fz_optarg;

#endif
