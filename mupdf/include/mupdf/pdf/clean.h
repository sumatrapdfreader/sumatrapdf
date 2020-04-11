#ifndef MUPDF_PDF_CLEAN_H
#define MUPDF_PDF_CLEAN_H

/*
	Read infile, and write selected pages to outfile with the given options.
*/
void pdf_clean_file(fz_context *ctx, char *infile, char *outfile, char *password, pdf_write_options *opts, char *retainlist[], int retainlen);

#endif
