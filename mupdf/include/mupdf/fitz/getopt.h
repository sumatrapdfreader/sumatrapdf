#ifndef MUPDF_FITZ_GETOPT_H
#define MUPDF_FITZ_GETOPT_H

/*
	Simple functions/variables for use in tools.
*/
extern int fz_getopt(int nargc, char * const *nargv, const char *ostr);
extern int fz_optind;
extern char *fz_optarg;

/*
	Windows unicode versions.
*/
#ifdef _WIN32
extern int fz_getoptw(int nargc, wchar_t * const *nargv, const wchar_t *ostr);
extern int fz_optindw;
extern wchar_t *fz_optargw;
#endif

#endif
