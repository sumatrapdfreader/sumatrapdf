#include "fitz-internal.h"
#include "mupdf-internal.h"

struct pdf_js_s
{
	fz_context *ctx;
	pdf_js_event event;
};

pdf_js *pdf_new_js(pdf_document *doc)
{
	/* SumatraPDF: don't let pdf_init_document fail for nothing */
	return NULL;
}

void pdf_js_load_document_level(pdf_js *js)
{
}

void pdf_drop_js(pdf_js *js)
{
	if (js)
	{
		fz_free(js->ctx, js->event.value);
		fz_free(js->ctx, js);
	}
}

void pdf_js_setup_event(pdf_js *js, pdf_js_event *e)
{
}

pdf_js_event *pdf_js_get_event(pdf_js *js)
{
	return js ? &js->event : NULL;
}

void pdf_js_execute(pdf_js *js, char *code)
{
}

void pdf_js_execute_count(pdf_js *js, char *code, int count)
{
}

int pdf_js_supported()
{
	return 0;
}
