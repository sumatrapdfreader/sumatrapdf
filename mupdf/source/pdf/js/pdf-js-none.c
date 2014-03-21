#include "mupdf/pdf.h"

void pdf_enable_js(pdf_document *doc)
{
}

void pdf_disable_js(pdf_document *doc)
{
}

int pdf_js_supported(pdf_document *doc)
{
	return 0;
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
