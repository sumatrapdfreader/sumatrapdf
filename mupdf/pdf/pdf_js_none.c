#include "fitz-internal.h"
#include "mupdf-internal.h"

struct pdf_js_s
{
	fz_context *ctx;
	pdf_js_event event;
};

pdf_js *pdf_new_js(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_js *js = fz_malloc_struct(ctx, pdf_js);

	fz_try(ctx)
	{
		js->ctx = doc->ctx;
		js->event.target = NULL;
		js->event.value = fz_strdup(ctx, "");
		js->event.rc = 1;
	}
	fz_catch(ctx)
	{
		pdf_drop_js(js);
	}

	return js;
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
