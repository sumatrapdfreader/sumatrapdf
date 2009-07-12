#include "fitz.h"
#include "mupdf.h"

extern char *basename;
extern pdf_xref *xref;
extern int pages;

void die(fz_error error);
void setcleanup(void (*cleanup)(void));

void openxref(char *filename, char *password, int dieonbadpass);
void closexref(void);

