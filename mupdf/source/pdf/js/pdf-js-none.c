#include "mupdf/pdf.h"

pdf_js *pdf_new_js(pdf_document *doc)
{
	return NULL;
}

void pdf_js_load_document_level(pdf_js *js)
{
}

void pdf_drop_js(pdf_js *js)
{
}

void pdf_js_setup_event(pdf_js *js, pdf_js_event *e)
{
}

pdf_js_event *pdf_js_get_event(pdf_js *js)
{
	return NULL;
}

void pdf_js_execute(pdf_js *js, char *code)
{
}

void pdf_js_execute_count(pdf_js *js, char *code, int count)
{
}

int pdf_js_supported(void)
{
	return 0;
}
