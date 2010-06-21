#include "fitz.h"
#include "mupdf.h"

extern char *basename;
extern pdf_xref *xref;
extern int pagecount;

void die(fz_error error);
void setcleanup(void (*cleanup)(void));

void openxref(char *filename, char *password, int dieonbadpass, int loadpages);
void flushxref(void);
void closexref(void);

