#include "mupdf/pdf.h"

#include <mujs.h>

#define MAXARGS 16

#define OBJ(i) ((pdf_jsimp_obj*)((intptr_t)(i)))
#define IDX(p) ((intptr_t)(p))
#define NEWOBJ(J,x) OBJ(js_gettop(J) + (x))

struct pdf_jsimp_s
{
	fz_context *ctx;
	void *jsctx;
	js_State *J;
};

static void *alloc(void *ud, void *ptr, unsigned int n)
{
	fz_context *ctx = ud;
	if (n == 0) {
		fz_free(ctx, ptr);
		return NULL;
	}
	if (ptr)
		return fz_resize_array(ctx, ptr, n, 1);
	return fz_malloc_array(ctx, n, 1);
}

pdf_jsimp *pdf_new_jsimp(fz_context *ctx, void *jsctx)
{
	js_State *J;
	pdf_jsimp *imp;

	J = js_newstate(alloc, ctx, 0);
	js_setcontext(J, jsctx);

	imp = fz_malloc_struct(ctx, pdf_jsimp);
	imp->ctx = ctx;
	imp->jsctx = jsctx;
	imp->J = J;
	return imp;
}

void pdf_drop_jsimp(pdf_jsimp *imp)
{
	if (imp)
	{
		js_freestate(imp->J);
		fz_free(imp->ctx, imp);
	}
}

pdf_jsimp_type *pdf_jsimp_new_type(pdf_jsimp *imp, pdf_jsimp_dtr *dtr, char *name)
{
	js_State *J = imp->J;
	js_newobject(J);
	js_setregistry(J, name);
	return (pdf_jsimp_type*)name;
}

void pdf_jsimp_drop_type(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	if (imp && type)
	{
		js_State *J = imp->J;
		js_delregistry(J, (const char *)type);
	}
}

static void wrapmethod(js_State *J)
{
	pdf_jsimp_obj *args[MAXARGS];
	pdf_jsimp_obj *ret;
	pdf_jsimp_method *meth;
	const char *type;
	void *jsctx;
	void *obj;
	int i;

	int argc = js_gettop(J) - 1;

	jsctx = js_getcontext(J);

	js_currentfunction(J);
	{
		js_getproperty(J, -1, "__call");
		meth = js_touserdata(J, -1, "method");
		js_pop(J, 1);

		js_getproperty(J, -1, "__type");
		type = js_tostring(J, -1);
		js_pop(J, 1);
	}
	js_pop(J, 1);

	if (js_isuserdata(J, 0, type))
		obj = js_touserdata(J, 0, type);
	else
		obj = NULL;

	if (argc > MAXARGS)
		js_rangeerror(J, "too many arguments");

	for (i = 0; i < argc; ++i)
		args[i] = OBJ(i+1);
	ret = meth(jsctx, obj, argc, args);
	if (ret)
		js_copy(J, IDX(ret));
	else
		js_pushundefined(J);
}

static void wrapgetter(js_State *J)
{
	pdf_jsimp_obj *ret;
	pdf_jsimp_getter *get;
	const char *type;
	void *jsctx;
	void *obj;

	jsctx = js_getcontext(J);

	js_currentfunction(J);
	{
		js_getproperty(J, -1, "__get");
		get = js_touserdata(J, -1, "getter");
		js_pop(J, 1);

		js_getproperty(J, -1, "__type");
		type = js_tostring(J, -1);
		js_pop(J, 1);
	}
	js_pop(J, 1);

	if (js_isuserdata(J, 0, type))
		obj = js_touserdata(J, 0, type);
	else
		obj = NULL;

	ret = get(jsctx, obj);
	if (ret)
		js_copy(J, IDX(ret));
	else
		js_pushundefined(J);
}

static void wrapsetter(js_State *J)
{
	pdf_jsimp_setter *set;
	const char *type;
	void *jsctx;
	void *obj;

	jsctx = js_getcontext(J);

	js_currentfunction(J);
	{
		js_getproperty(J, -1, "__set");
		set = js_touserdata(J, -1, "setter");
		js_pop(J, 1);

		js_getproperty(J, -1, "__type");
		type = js_tostring(J, -1);
		js_pop(J, 1);
	}
	js_pop(J, 1);

	if (js_isuserdata(J, 0, type))
		obj = js_touserdata(J, 0, type);
	else
		obj = NULL;

	set(jsctx, obj, OBJ(1));

	js_pushundefined(J);
}

void pdf_jsimp_addmethod(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_method *meth)
{
	js_State *J = imp->J;
	js_getregistry(J, (const char *)type);
	{
		js_newcfunction(J, wrapmethod, name, 0);
		{
			js_pushnull(J);
			js_newuserdata(J, "method", meth, NULL);
			js_defproperty(J, -2, "__call", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
			js_pushstring(J, (const char *)type);
			js_defproperty(J, -2, "__type", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		}
		js_defproperty(J, -2, name, JS_READONLY | JS_DONTCONF);
	}
	js_pop(J, 1);
}

void pdf_jsimp_addproperty(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_getter *get, pdf_jsimp_setter *set)
{
	js_State *J = imp->J;
	js_getregistry(J, (const char *)type);
	{
		js_newcfunction(J, wrapgetter, name, 0);
		{
			js_pushnull(J);
			js_newuserdata(J, "getter", get, NULL);
			js_defproperty(J, -2, "__get", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
			js_pushstring(J, (const char *)type);
			js_defproperty(J, -2, "__type", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		}
		js_newcfunction(J, wrapsetter, name, 0);
		{
			js_pushnull(J);
			js_newuserdata(J, "setter", set, NULL);
			js_defproperty(J, -2, "__set", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
			js_pushstring(J, (const char *)type);
			js_defproperty(J, -2, "__type", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		}
		js_defaccessor(J, -3, name, JS_READONLY | JS_DONTCONF);
	}
	js_pop(J, 1);
}

void pdf_jsimp_set_global_type(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	js_State *J = imp->J;
	const char *name;

	js_getregistry(J, (const char *)type);
	js_pushiterator(J, -1, 1);
	while ((name = js_nextiterator(J, -1)))
	{
		js_getproperty(J, -2, name);
		js_setglobal(J, name);
	}
}

pdf_jsimp_obj *pdf_jsimp_new_obj(pdf_jsimp *imp, pdf_jsimp_type *type, void *natobj)
{
	js_State *J = imp->J;
	js_getregistry(J, (const char *)type);
	js_newuserdata(J, (const char *)type, natobj, NULL);
	return NEWOBJ(J, -1);
}

void pdf_jsimp_drop_obj(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
}

int pdf_jsimp_to_type(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	js_State *J = imp->J;
	if (js_isnull(J, IDX(obj))) return JS_TYPE_NULL;
	if (js_isboolean(J, IDX(obj))) return JS_TYPE_BOOLEAN;
	if (js_isnumber(J, IDX(obj))) return JS_TYPE_NUMBER;
	if (js_isstring(J, IDX(obj))) return JS_TYPE_STRING;
	if (js_isarray(J, IDX(obj))) return JS_TYPE_ARRAY;
	return JS_TYPE_UNKNOWN;
}

pdf_jsimp_obj *pdf_jsimp_from_string(pdf_jsimp *imp, char *str)
{
	js_State *J = imp->J;
	js_pushstring(J, str);
	return NEWOBJ(J, -1);
}

char *pdf_jsimp_to_string(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	/* cast away const :( */
	return (char*)js_tostring(imp->J, IDX(obj));
}

pdf_jsimp_obj *pdf_jsimp_from_number(pdf_jsimp *imp, double num)
{
	js_State *J = imp->J;
	js_pushnumber(J, num);
	return NEWOBJ(J, -1);
}

double pdf_jsimp_to_number(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	return js_tonumber(imp->J, IDX(obj));
}

int pdf_jsimp_array_len(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	js_State *J = imp->J;
	return js_getlength(J, IDX(obj));
}

pdf_jsimp_obj *pdf_jsimp_array_item(pdf_jsimp *imp, pdf_jsimp_obj *obj, int i)
{
	js_State *J = imp->J;
	js_getindex(J, IDX(obj), i);
	return NEWOBJ(J, -1);
}

pdf_jsimp_obj *pdf_jsimp_property(pdf_jsimp *imp, pdf_jsimp_obj *obj, char *prop)
{
	js_State *J = imp->J;
	js_getproperty(J, IDX(obj), prop);
	return NEWOBJ(J, -1);
}

void pdf_jsimp_execute(pdf_jsimp *imp, char *code)
{
	js_State *J = imp->J;
	js_dostring(J, code, 0);
}

void pdf_jsimp_execute_count(pdf_jsimp *imp, char *code, int count)
{
	char *terminated = fz_malloc(imp->ctx, count+1);
	memcpy(terminated, code, count);
	terminated[count] = 0;
	pdf_jsimp_execute(imp, terminated);
	fz_free(imp->ctx, terminated);
}
