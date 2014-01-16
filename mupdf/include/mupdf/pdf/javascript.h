#ifndef MUPDF_PDF_JAVASCRIPT_H
#define MUPDF_PDF_JAVASCRIPT_H

typedef struct pdf_js_event_s
{
	pdf_obj *target;
	char *value;
	int rc;
} pdf_js_event;

int pdf_js_supported(void);
pdf_js *pdf_new_js(pdf_document *doc);
void pdf_drop_js(pdf_js *js);
void pdf_js_load_document_level(pdf_js *js);
void pdf_js_setup_event(pdf_js *js, pdf_js_event *e);
pdf_js_event *pdf_js_get_event(pdf_js *js);
void pdf_js_execute(pdf_js *js, char *code);
void pdf_js_execute_count(pdf_js *js, char *code, int count);

/*
 * Javascript engine interface
 */
typedef struct pdf_jsimp_s pdf_jsimp;
typedef struct pdf_jsimp_type_s pdf_jsimp_type;
typedef struct pdf_jsimp_obj_s pdf_jsimp_obj;

typedef void (pdf_jsimp_dtr)(void *jsctx, void *obj);
typedef pdf_jsimp_obj *(pdf_jsimp_method)(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[]);
typedef pdf_jsimp_obj *(pdf_jsimp_getter)(void *jsctx, void *obj);
typedef void (pdf_jsimp_setter)(void *jsctx, void *obj, pdf_jsimp_obj *val);

enum
{
	JS_TYPE_UNKNOWN,
	JS_TYPE_NULL,
	JS_TYPE_STRING,
	JS_TYPE_NUMBER,
	JS_TYPE_ARRAY,
	JS_TYPE_BOOLEAN
};

pdf_jsimp *pdf_new_jsimp(fz_context *ctx, void *jsctx);
void pdf_drop_jsimp(pdf_jsimp *imp);

pdf_jsimp_type *pdf_jsimp_new_type(pdf_jsimp *imp, pdf_jsimp_dtr *dtr);
void pdf_jsimp_drop_type(pdf_jsimp *imp, pdf_jsimp_type *type);
void pdf_jsimp_addmethod(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_method *meth);
void pdf_jsimp_addproperty(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_getter *get, pdf_jsimp_setter *set);
void pdf_jsimp_set_global_type(pdf_jsimp *imp, pdf_jsimp_type *type);

pdf_jsimp_obj *pdf_jsimp_new_obj(pdf_jsimp *imp, pdf_jsimp_type *type, void *obj);
void pdf_jsimp_drop_obj(pdf_jsimp *imp, pdf_jsimp_obj *obj);

int pdf_jsimp_to_type(pdf_jsimp *imp, pdf_jsimp_obj *obj);

pdf_jsimp_obj *pdf_jsimp_from_string(pdf_jsimp *imp, char *str);
char *pdf_jsimp_to_string(pdf_jsimp *imp, pdf_jsimp_obj *obj);

pdf_jsimp_obj *pdf_jsimp_from_number(pdf_jsimp *imp, double num);
double pdf_jsimp_to_number(pdf_jsimp *imp, pdf_jsimp_obj *obj);

int pdf_jsimp_array_len(pdf_jsimp *imp, pdf_jsimp_obj *obj);
pdf_jsimp_obj *pdf_jsimp_array_item(pdf_jsimp *imp, pdf_jsimp_obj *obj, int i);

pdf_jsimp_obj *pdf_jsimp_property(pdf_jsimp *imp, pdf_jsimp_obj *obj, char *prop);

void pdf_jsimp_execute(pdf_jsimp *imp, char *code);
void pdf_jsimp_execute_count(pdf_jsimp *imp, char *code, int count);

#endif
