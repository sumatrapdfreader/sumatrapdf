/* C++ version of the pdf_jsimp api. C++ cannot safely call fz_throw,
 * so C++ implementations return explicit errors in char * form. */

fz_context *pdf_jsimp_ctx_cpp(pdf_jsimp *imp);
const char *pdf_new_jsimp_cpp(fz_context *ctx, void *jsctx, pdf_jsimp **imp);
const char *pdf_drop_jsimp_cpp(pdf_jsimp *imp);
const char *pdf_jsimp_new_type_cpp(pdf_jsimp *imp, pdf_jsimp_dtr *dtr, pdf_jsimp_type **type);
const char *pdf_jsimp_drop_type_cpp(pdf_jsimp *imp, pdf_jsimp_type *type);
const char *pdf_jsimp_addmethod_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_method *meth);
const char *pdf_jsimp_addproperty_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_getter *get, pdf_jsimp_setter *set);
const char *pdf_jsimp_set_global_type_cpp(pdf_jsimp *imp, pdf_jsimp_type *type);
const char *pdf_jsimp_new_obj_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, void *natobj, pdf_jsimp_obj **obj);
const char *pdf_jsimp_drop_obj_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj);
const char *pdf_jsimp_to_type_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int *type);
const char *pdf_jsimp_from_string_cpp(pdf_jsimp *imp, char *str, pdf_jsimp_obj **obj);
const char *pdf_jsimp_to_string_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, char **str);
const char *pdf_jsimp_from_number_cpp(pdf_jsimp *imp, double num, pdf_jsimp_obj **obj);
const char *pdf_jsimp_to_number_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, double *num);
const char *pdf_jsimp_array_len_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int *len);
const char *pdf_jsimp_array_item_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int i, pdf_jsimp_obj **item);
const char *pdf_jsimp_property_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, char *prop, pdf_jsimp_obj **pobj);
const char *pdf_jsimp_execute_cpp(pdf_jsimp *imp, char *code);
const char *pdf_jsimp_execute_count_cpp(pdf_jsimp *imp, char *code, int count);

/* Also when calling back into mupdf, all exceptions must be caught. The functions bellow
 * wrap these calls */
pdf_jsimp_obj *pdf_jsimp_call_method(pdf_jsimp *imp, pdf_jsimp_method *meth, void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[]);
pdf_jsimp_obj *pdf_jsimp_call_getter(pdf_jsimp *imp, pdf_jsimp_getter *get, void *jsctx, void *obj);
void pdf_jsimp_call_setter(pdf_jsimp *imp, pdf_jsimp_setter *set, void *jsctx, void *obj, pdf_jsimp_obj *val);
