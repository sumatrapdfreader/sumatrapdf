#ifndef MUPDF_PDF_JAVASCRIPT_H
#define MUPDF_PDF_JAVASCRIPT_H

void pdf_enable_js(fz_context *ctx, pdf_document *doc);
void pdf_disable_js(fz_context *ctx, pdf_document *doc);
int pdf_js_supported(fz_context *ctx, pdf_document *doc);
void pdf_drop_js(fz_context *ctx, pdf_js *js);

void pdf_js_event_init(pdf_js *js, pdf_obj *target, const char *value, int willCommit);
int pdf_js_event_result(pdf_js *js);
char *pdf_js_event_value(pdf_js *js);
void pdf_js_event_init_keystroke(pdf_js *js, pdf_obj *target, pdf_keystroke_event *evt);
int pdf_js_event_result_keystroke(pdf_js *js, pdf_keystroke_event *evt);

void pdf_js_execute(pdf_js *js, const char *name, const char *code);

#endif
