/* This file contains wrapper functions for pdf_jsimp functions implemented
 * in Javascriptcore */

#include <JavaScriptCore/JavaScriptCore.h>
#include "mupdf/pdf.h"

#define STRING_BUF_SIZE (256)
#define FUNCTION_PREAMBLE_LEN (9)

/*
	We need only a single JSClassRef because we store property and method information
	in the private data of each object. The JSClassRef is set up to know how to access
	that data.
*/
struct pdf_jsimp_s
{
	fz_context *ctx;
	void *nat_ctx;
	JSGlobalContextRef jscore_ctx;
	JSClassRef class_ref;
};

enum
{
	PROP_FN,
	PROP_VAL
};

typedef struct prop_fn_s
{
	pdf_jsimp_method *meth;
} prop_fn;

typedef struct prop_val_s
{
	pdf_jsimp_getter *get;
	pdf_jsimp_setter *set;
} prop_val;

typedef struct prop_s
{
	char *name;
	int type;
	union
	{
		prop_fn fn;
		prop_val val;
	} u;
} prop;

typedef struct prop_list_s prop_list;

struct prop_list_s
{
	prop prop;
	prop_list *next;
};

struct pdf_jsimp_type_s
{
	pdf_jsimp *imp;
	pdf_jsimp_dtr *dtr;
	prop_list *props;
};

/*
	When we create a JavaScriptCore object, we store in its private data the MuPDF
	native object pointer and a pointer to the type. The type has a list of the
	properties and methods
*/
typedef struct priv_data_s
{
	pdf_jsimp_type *type;
	void *natobj;
} priv_data;

struct pdf_jsimp_obj_s
{
	JSValueRef ref;
	char *str;
};

static prop *find_prop(prop_list *list, char *name)
{
	while (list)
	{
		if (strcmp(name, list->prop.name) == 0)
			return &list->prop;

		list = list->next;
	}

	return NULL;
}

static pdf_jsimp_obj *wrap_val(pdf_jsimp *imp, JSValueRef ref)
{
	pdf_jsimp_obj *obj = fz_malloc_struct(imp->ctx, pdf_jsimp_obj);
	obj->ref = ref;
	JSValueProtect(imp->jscore_ctx, ref);

	return obj;
}

static JSValueRef callMethod(JSContextRef jscore_ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef *exception)
{
	pdf_jsimp *imp;
	fz_context *ctx;
	pdf_jsimp_obj *res = NULL;
	JSValueRef resref = NULL;
	int i;
	pdf_jsimp_obj **args = NULL;
	pdf_jsimp_method *meth = JSObjectGetPrivate(function);
	priv_data *pdata = JSObjectGetPrivate(thisObject);
	if (meth == NULL)
	{
		/*
			The attempt to store the method pointer as private data failed, so we
			turn the function into a string, which will have the form "function name() xxx",
			and then lookup the name.
		*/
		char name[STRING_BUF_SIZE];
		char *np;
		char *bp;
		JSStringRef jname = JSValueToStringCopy(jscore_ctx, function, NULL);
		prop *p;
		JSStringGetUTF8CString(jname, name, STRING_BUF_SIZE);
		if (strlen(name) >= FUNCTION_PREAMBLE_LEN)
		{
			np = name + FUNCTION_PREAMBLE_LEN; /* strlen("function "); */
			bp = strchr(np, '(');
			if (bp)
				*bp = 0;
			p = find_prop(pdata->type->props, np);
			if (p && p->type == PROP_FN)
			{
				meth = p->u.fn.meth;
			}
		}
		JSStringRelease(jname);
	}
	if (meth == NULL || pdata == NULL)
		return JSValueMakeUndefined(jscore_ctx);

	imp = pdata->type->imp;
	ctx = imp->ctx;

	fz_var(args);
	fz_var(res);
	fz_try(ctx)
	{
		args = fz_malloc_array(ctx, argumentCount, sizeof(pdf_jsimp_obj));
		for (i = 0; i < argumentCount; i++)
			args[i] = wrap_val(imp, arguments[i]);

		res = meth(imp->nat_ctx, pdata->natobj, argumentCount, args);
		if (res)
			resref = res->ref;
	}
	fz_always(ctx)
	{
		if (args)
		{
			for (i = 0; i < argumentCount; i++)
				pdf_jsimp_drop_obj(imp, args[i]);
			fz_free(ctx, args);
		}
		pdf_jsimp_drop_obj(imp, res);
	}
	fz_catch(ctx)
	{
		return JSValueMakeUndefined(jscore_ctx);
	}

	return resref;
}

static JSValueRef getProperty(JSContextRef jscore_ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception)
{
	pdf_jsimp *imp;
	char buf[STRING_BUF_SIZE];
	prop *p;
	JSValueRef res = NULL;

	priv_data *pdata = JSObjectGetPrivate(object);
	if (pdata == NULL)
		return NULL;

	JSStringGetUTF8CString(propertyName, buf, STRING_BUF_SIZE);
	p = find_prop(pdata->type->props, buf);
	if (p == NULL)
		return NULL;

	imp = pdata->type->imp;

	switch(p->type)
	{
		case PROP_FN:
			{
				/*
					For some reason passing the method pointer as private data doesn't work: the data comes back
					NULL when interrogated in callMethod above. So we also specify the method name when
					creating the function so that we can look it up again in callMethod. Not ideal, but
					will do until we can find a better solution.
				*/
				JSObjectRef ores = JSObjectMakeFunctionWithCallback(jscore_ctx, propertyName, callMethod);
				JSObjectSetPrivate(ores, p->u.fn.meth);
				res = ores;
			}
			break;

		case PROP_VAL:
			{
				pdf_jsimp_obj *pres = p->u.val.get(imp->nat_ctx, pdata->natobj);
				res = pres->ref;
				pdf_jsimp_drop_obj(imp, pres);
			}
			break;
	}

	return res;
}

static bool setProperty(JSContextRef jscore_ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef *exception)
{
	pdf_jsimp *imp;
	char buf[STRING_BUF_SIZE];
	prop *p;

	priv_data *pdata = JSObjectGetPrivate(object);
	if (pdata == NULL)
		return false;

	JSStringGetUTF8CString(propertyName, buf, STRING_BUF_SIZE);
	p = find_prop(pdata->type->props, buf);
	if (p == NULL)
		return false;

	imp = pdata->type->imp;

	switch(p->type)
	{
		case PROP_FN:
			break;

		case PROP_VAL:
			{
				pdf_jsimp_obj *pval = wrap_val(imp, value);
				p->u.val.set(imp->nat_ctx, pdata->natobj, pval);
				pdf_jsimp_drop_obj(imp, pval);
			}
			break;
	}

	return true;
}

pdf_jsimp *pdf_new_jsimp(fz_context *ctx, void *jsctx)
{
	pdf_jsimp *imp = fz_malloc_struct(ctx, pdf_jsimp);

	fz_try(ctx)
	{
		JSClassDefinition classDef = kJSClassDefinitionEmpty;

		classDef.getProperty = getProperty;
		classDef.setProperty = setProperty;

		imp->nat_ctx = jsctx;
		imp->class_ref = JSClassCreate(&classDef);
		imp->jscore_ctx = JSGlobalContextCreate(imp->class_ref);
		if (imp->jscore_ctx == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "JSGlobalContextCreate failed");
	}
	fz_catch(ctx)
	{
		pdf_drop_jsimp(imp);
		fz_rethrow(ctx);
	}

	imp->ctx = ctx;

	return imp;
}

void pdf_drop_jsimp(pdf_jsimp *imp)
{
	if (imp)
	{
		JSGlobalContextRelease(imp->jscore_ctx);
		JSClassRelease(imp->class_ref);
		fz_free(imp->ctx, imp);
	}
}

pdf_jsimp_type *pdf_jsimp_new_type(pdf_jsimp *imp, pdf_jsimp_dtr *dtr, char *name)
{
	pdf_jsimp_type *type = fz_malloc_struct(imp->ctx, pdf_jsimp_type);
	type->imp = imp;
	type->dtr = dtr;
	return type;
}

void pdf_jsimp_drop_type(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	if (imp && type)
	{
		fz_context *ctx = imp->ctx;
		prop_list *node;

		while (type->props)
		{
			node = type->props;
			type->props = node->next;
			fz_free(ctx, node->prop.name);
			fz_free(ctx, node);
		}

		fz_free(ctx, type);
	}
}

void pdf_jsimp_addmethod(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_method *meth)
{
	fz_context *ctx = imp->ctx;
	prop_list *node = fz_malloc_struct(ctx, prop_list);

	fz_try(ctx)
	{
		node->prop.name = fz_strdup(imp->ctx, name);
		node->prop.type = PROP_FN;
		node->prop.u.fn.meth = meth;

		node->next = type->props;
		type->props = node;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, node);
		fz_rethrow(ctx);
	}
}

void pdf_jsimp_addproperty(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_getter *get, pdf_jsimp_setter *set)
{
	fz_context *ctx = imp->ctx;
	prop_list *node = fz_malloc_struct(ctx, prop_list);

	fz_try(ctx)
	{
		node->prop.name = fz_strdup(imp->ctx, name);
		node->prop.type = PROP_VAL;
		node->prop.u.val.get = get;
		node->prop.u.val.set = set;

		node->next = type->props;
		type->props = node;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, node);
		fz_rethrow(ctx);
	}
}

void pdf_jsimp_set_global_type(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	fz_context *ctx = imp->ctx;
	priv_data *pdata;
	JSObjectRef gobj = JSContextGetGlobalObject(imp->jscore_ctx);
	if (gobj == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "JSContextGetGlobalObject failed");

	pdata = fz_malloc_struct(ctx, priv_data);
	pdata->type = type;
	pdata->natobj = NULL;
	JSObjectSetPrivate(gobj, pdata);
}

pdf_jsimp_obj *pdf_jsimp_new_obj(pdf_jsimp *imp, pdf_jsimp_type *type, void *natobj)
{
	fz_context *ctx = imp->ctx;
	pdf_jsimp_obj *obj = fz_malloc_struct(ctx, pdf_jsimp_obj);
	priv_data *pdata = NULL;

	fz_var(pdata);
	fz_try(ctx)
	{
		pdata = fz_malloc_struct(ctx, priv_data);
		pdata->type = type;
		pdata->natobj = natobj;
		obj->ref = JSObjectMake(imp->jscore_ctx, imp->class_ref, pdata);
		if (obj->ref == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "JSObjectMake failed");

		JSValueProtect(imp->jscore_ctx, obj->ref);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, pdata);
		fz_free(ctx, obj);
		fz_rethrow(ctx);
	}

	return obj;
}

void pdf_jsimp_drop_obj(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	if (imp && obj)
	{
		JSValueUnprotect(imp->jscore_ctx, obj->ref);
		fz_free(imp->ctx, obj->str);
		fz_free(imp->ctx, obj);
	}
}

int pdf_jsimp_to_type(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	switch (JSValueGetType(imp->jscore_ctx, obj->ref))
	{
		case kJSTypeNull: return JS_TYPE_NULL;
		case kJSTypeBoolean: return JS_TYPE_BOOLEAN;
		case kJSTypeNumber: return JS_TYPE_NUMBER;
		case kJSTypeString: return JS_TYPE_STRING;
		case kJSTypeObject: return JS_TYPE_ARRAY;
		default: return JS_TYPE_UNKNOWN;
	}
}

pdf_jsimp_obj *pdf_jsimp_from_string(pdf_jsimp *imp, char *str)
{
	JSStringRef sref = JSStringCreateWithUTF8CString(str);
	JSValueRef vref = JSValueMakeString(imp->jscore_ctx, sref);
	JSStringRelease(sref);

	return wrap_val(imp, vref);
}

char *pdf_jsimp_to_string(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	fz_context *ctx = imp->ctx;
	JSStringRef jstr = JSValueToStringCopy(imp->jscore_ctx, obj->ref, NULL);
	int len;

	if (jstr == NULL)
		return "";

	fz_try(ctx)
	{
		len = JSStringGetMaximumUTF8CStringSize(jstr);
		fz_free(ctx, obj->str);
		obj->str = NULL;
		obj->str = fz_malloc(ctx, len+1);
		JSStringGetUTF8CString(jstr, obj->str, len+1);
	}
	fz_always(ctx)
	{
		JSStringRelease(jstr);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return obj->str;
}

pdf_jsimp_obj *pdf_jsimp_from_number(pdf_jsimp *imp, double num)
{
	return wrap_val(imp, JSValueMakeNumber(imp->jscore_ctx, num));
}

double pdf_jsimp_to_number(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	return JSValueToNumber(imp->jscore_ctx, obj->ref, NULL);
}

int pdf_jsimp_array_len(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	pdf_jsimp_obj *lobj = pdf_jsimp_property(imp, obj, "length");
	int num = (int)pdf_jsimp_to_number(imp, lobj);

	pdf_jsimp_drop_obj(imp, lobj);

	return num;
}

pdf_jsimp_obj *pdf_jsimp_array_item(pdf_jsimp *imp, pdf_jsimp_obj *obj, int i)
{
	return wrap_val(imp, JSObjectGetPropertyAtIndex(imp->jscore_ctx, JSValueToObject(imp->jscore_ctx, obj->ref, NULL), i, NULL));
}

pdf_jsimp_obj *pdf_jsimp_property(pdf_jsimp *imp, pdf_jsimp_obj *obj, char *prop)
{
	JSStringRef jprop = JSStringCreateWithUTF8CString(prop);
	JSValueRef jval = JSObjectGetProperty(imp->jscore_ctx, JSValueToObject(imp->jscore_ctx, obj->ref, NULL), jprop, NULL);

	JSStringRelease(jprop);

	return wrap_val(imp, jval);
}

void pdf_jsimp_execute(pdf_jsimp *imp, char *code)
{
	JSStringRef jcode = JSStringCreateWithUTF8CString(code);
	JSEvaluateScript(imp->jscore_ctx, jcode, NULL, NULL, 0, NULL);
	JSStringRelease(jcode);
}

void pdf_jsimp_execute_count(pdf_jsimp *imp, char *code, int count)
{
	char *terminated = fz_malloc(imp->ctx, count+1);
	memcpy(terminated, code, count);
	terminated[count] = 0;
	pdf_jsimp_execute(imp, terminated);
	fz_free(imp->ctx, terminated);
}
