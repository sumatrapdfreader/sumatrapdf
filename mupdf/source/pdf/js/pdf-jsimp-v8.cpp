/*
 * This is a dummy JavaScript engine. It cheats by recognising the specific
 * strings in calc.pdf, and hence will work only for that file. It is for
 * testing only.
 */

extern "C" {
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "pdf-jsimp-cpp.h"
}

#include <vector>
#include <set>
#include <v8.h>

using namespace v8;
using namespace std;

struct PDFJSImp;

/* Object we pass to FunctionTemplate::New, which v8 passes back to us in
 * callMethod, allowing us to call our client's, passed-in method. */
struct PDFJSImpMethod
{
	PDFJSImp *imp;
	pdf_jsimp_method *meth;

	PDFJSImpMethod(PDFJSImp *imp, pdf_jsimp_method *meth) : imp(imp), meth(meth) {}
};

/* Object we pass to ObjectTemplate::SetAccessor, which v8 passes back to us in
 * setProp and getProp, allowing us to call our client's, passed-in set/get methods. */
struct PDFJSImpProperty
{
	PDFJSImp *imp;
	pdf_jsimp_getter *get;
	pdf_jsimp_setter *set;

	PDFJSImpProperty(PDFJSImp *imp, pdf_jsimp_getter *get, pdf_jsimp_setter *set) : imp(imp), get(get), set(set) {}
};

/* Internal representation of the pdf_jsimp_type object */
struct PDFJSImpType
{
	PDFJSImp                  *imp;
	Persistent<ObjectTemplate> templ;
	pdf_jsimp_dtr             *dtr;
	vector<PDFJSImpMethod *> methods;
	vector<PDFJSImpProperty *> properties;

	PDFJSImpType(PDFJSImp *imp, pdf_jsimp_dtr *dtr): imp(imp), dtr(dtr)
	{
		HandleScope scope;
		templ = Persistent<ObjectTemplate>::New(ObjectTemplate::New());
		templ->SetInternalFieldCount(1);
	}

	~PDFJSImpType()
	{
		vector<PDFJSImpMethod *>::iterator mit;
		for (mit = methods.begin(); mit < methods.end(); mit++)
			delete *mit;

		vector<PDFJSImpProperty *>::iterator pit;
		for (pit = properties.begin(); pit < properties.end(); pit++)
			delete *pit;

		templ.Dispose();
	}
};

/* Info via which we destroy the client side part of objects that
 * v8 garbage collects */
struct PDFJSImpGCObj
{
	Persistent<Object> pobj;
	PDFJSImpType *type;

	PDFJSImpGCObj(Handle<Object> obj, PDFJSImpType *type): type(type)
	{
		pobj = Persistent<Object>::New(obj);
	}

	~PDFJSImpGCObj()
	{
		pobj.Dispose();
	}
};

/* Internal representation of the pdf_jsimp object */
struct PDFJSImp
{
	fz_context			*ctx;
	void				*jsctx;
	Persistent<Context>	 context;
	vector<PDFJSImpType *> types;
	set<PDFJSImpGCObj *> gclist;

	PDFJSImp(fz_context *ctx, void *jsctx) : ctx(ctx), jsctx(jsctx)
	{
		HandleScope scope;
		context = Persistent<Context>::New(Context::New());
	}

	~PDFJSImp()
	{
		HandleScope scope;
		/* Tell v8 our context will not be used again */
		context.Dispose();

		/* Unlink and destroy all the objects that v8 has yet to gc */
		set<PDFJSImpGCObj *>::iterator oit;
		for (oit = gclist.begin(); oit != gclist.end(); oit++)
		{
			(*oit)->pobj.ClearWeak(); /* So that gcCallback wont get called */
			PDFJSImpType *vType = (*oit)->type;
			Local<External> owrap = Local<External>::Cast((*oit)->pobj->GetInternalField(0));
			vType->dtr(vType->imp->jsctx, owrap->Value());
			delete *oit;
		}

		vector<PDFJSImpType *>::iterator it;
		for (it = types.begin(); it < types.end(); it++)
			delete *it;
	}
};

/* Internal representation of the pdf_jsimp_obj object */
class PDFJSImpObject
{
	Persistent<Value>   pobj;
	String::Utf8Value  *utf8;

public:
	PDFJSImpObject(Handle<Value> obj): utf8(NULL)
	{
		pobj = Persistent<Value>::New(obj);
	}

	PDFJSImpObject(const char *str): utf8(NULL)
	{
		pobj = Persistent<Value>::New(String::New(str));
	}

	PDFJSImpObject(double num): utf8(NULL)
	{
		pobj = Persistent<Value>::New(Number::New(num));
	}

	~PDFJSImpObject()
	{
		delete utf8;
		pobj.Dispose();
	}

	int type()
	{
		if (pobj->IsNull())
			return JS_TYPE_NULL;
		else if (pobj->IsString() || pobj->IsStringObject())
			return JS_TYPE_STRING;
		else if (pobj->IsNumber() || pobj->IsNumberObject())
			return JS_TYPE_NUMBER;
		else if (pobj->IsArray())
			return JS_TYPE_ARRAY;
		else if (pobj->IsBoolean() || pobj->IsBooleanObject())
			return JS_TYPE_BOOLEAN;
		else
			return JS_TYPE_UNKNOWN;
	}

	char *toString()
	{
		delete utf8;
		utf8 = new String::Utf8Value(pobj);
		return **utf8;
	}

	double toNumber()
	{
		return pobj->NumberValue();
	}

	Handle<Value> toValue()
	{
		return pobj;
	}
};

extern "C" fz_context *pdf_jsimp_ctx_cpp(pdf_jsimp *imp)
{
	return reinterpret_cast<PDFJSImp *>(imp)->ctx;
}

extern "C" const char *pdf_new_jsimp_cpp(fz_context *ctx, void *jsctx, pdf_jsimp **imp)
{
	Locker lock;
	*imp = reinterpret_cast<pdf_jsimp *>(new PDFJSImp(ctx, jsctx));

	return NULL;
}

extern "C" const char *pdf_drop_jsimp_cpp(pdf_jsimp *imp)
{
	Locker lock;
	delete reinterpret_cast<PDFJSImp *>(imp);
	return NULL;
}

extern "C" const char *pdf_jsimp_new_type_cpp(pdf_jsimp *imp, pdf_jsimp_dtr *dtr, pdf_jsimp_type **type)
{
	Locker lock;
	PDFJSImp *vImp = reinterpret_cast<PDFJSImp *>(imp);
	PDFJSImpType *vType = new PDFJSImpType(vImp, dtr);
	vImp->types.push_back(vType);
	*type = reinterpret_cast<pdf_jsimp_type *>(vType);
	return NULL;
}

extern "C" const char *pdf_jsimp_drop_type_cpp(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	/* Types are recorded and destroyed as part of PDFJSImp */
	return NULL;
}

static Handle<Value> callMethod(const Arguments &args)
{
	HandleScope scope;
	Local<External> mwrap = Local<External>::Cast(args.Data());
	PDFJSImpMethod *m = (PDFJSImpMethod *)mwrap->Value();

	Local<Object> self = args.Holder();
	Local<External> owrap;
	void *nself = NULL;
	if (self->InternalFieldCount() > 0)
	{
		owrap = Local<External>::Cast(self->GetInternalField(0));
		nself = owrap->Value();
	}

	int c = args.Length();
	PDFJSImpObject **native_args = new PDFJSImpObject*[c];
	for (int i = 0; i < c; i++)
		native_args[i] = new PDFJSImpObject(args[i]);

	PDFJSImpObject *obj = reinterpret_cast<PDFJSImpObject *>(pdf_jsimp_call_method(reinterpret_cast<pdf_jsimp *>(m->imp), m->meth, m->imp->jsctx, nself, c, reinterpret_cast<pdf_jsimp_obj **>(native_args)));
	Handle<Value> val;
	if (obj)
		val = obj->toValue();
	delete obj;

	for (int i = 0; i < c; i++)
		delete native_args[i];

	delete native_args;

	return scope.Close(val);
}

extern "C" const char *pdf_jsimp_addmethod_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_method *meth)
{
	Locker lock;
	PDFJSImpType *vType = reinterpret_cast<PDFJSImpType *>(type);
	HandleScope scope;

	PDFJSImpMethod *pmeth = new PDFJSImpMethod(vType->imp, meth);
	vType->templ->Set(String::New(name), FunctionTemplate::New(callMethod, External::New(pmeth)));
	vType->methods.push_back(pmeth);
	return NULL;
}

static Handle<Value> getProp(Local<String> property, const AccessorInfo &info)
{
	HandleScope scope;
	Local<External> pwrap = Local<External>::Cast(info.Data());
	PDFJSImpProperty *p = reinterpret_cast<PDFJSImpProperty *>(pwrap->Value());

	Local<Object> self = info.Holder();
	Local<External> owrap;
	void *nself = NULL;
	if (self->InternalFieldCount() > 0)
	{
		Local<Value> val = self->GetInternalField(0);
		if (val->IsExternal())
		{
			owrap = Local<External>::Cast(val);
			nself = owrap->Value();
		}
	}

	PDFJSImpObject *obj = reinterpret_cast<PDFJSImpObject *>(pdf_jsimp_call_getter(reinterpret_cast<pdf_jsimp *>(p->imp), p->get, p->imp->jsctx, nself));
	Handle<Value> val;
	if (obj)
		val = obj->toValue();
	delete obj;
	return scope.Close(val);
}

static void setProp(Local<String> property, Local<Value> value, const AccessorInfo &info)
{
	HandleScope scope;
	Local<External> wrap = Local<External>::Cast(info.Data());
	PDFJSImpProperty *p = reinterpret_cast<PDFJSImpProperty *>(wrap->Value());

	Local<Object> self = info.Holder();
	Local<External> owrap;
	void *nself = NULL;
	if (self->InternalFieldCount() > 0)
	{
		owrap = Local<External>::Cast(self->GetInternalField(0));
		nself = owrap->Value();
	}

	PDFJSImpObject *obj = new PDFJSImpObject(value);

	pdf_jsimp_call_setter(reinterpret_cast<pdf_jsimp *>(p->imp), p->set, p->imp->jsctx, nself, reinterpret_cast<pdf_jsimp_obj *>(obj));
	delete obj;
}

extern "C" const char *pdf_jsimp_addproperty_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, char *name, pdf_jsimp_getter *get, pdf_jsimp_setter *set)
{
	Locker lock;
	PDFJSImpType *vType = reinterpret_cast<PDFJSImpType *>(type);
	HandleScope scope;

	PDFJSImpProperty *prop = new PDFJSImpProperty(vType->imp, get, set);
	vType->templ->SetAccessor(String::New(name), getProp, setProp, External::New(prop));
	vType->properties.push_back(prop);
	return NULL;
}

extern "C" const char *pdf_jsimp_set_global_type_cpp(pdf_jsimp *imp, pdf_jsimp_type *type)
{
	Locker lock;
	PDFJSImp	 *vImp  = reinterpret_cast<PDFJSImp *>(imp);
	PDFJSImpType *vType = reinterpret_cast<PDFJSImpType *>(type);
	HandleScope scope;

	vImp->context = Persistent<Context>::New(Context::New(NULL, vType->templ));
	return NULL;
}

static void gcCallback(Persistent<Value> val, void *parm)
{
	PDFJSImpGCObj *gco = reinterpret_cast<PDFJSImpGCObj *>(parm);
	PDFJSImpType *vType = gco->type;
	HandleScope scope;
	Persistent<Object> obj = Persistent<Object>::Cast(val);

	Local<External> owrap = Local<External>::Cast(obj->GetInternalField(0));
	vType->dtr(vType->imp->jsctx, owrap->Value());
	vType->imp->gclist.erase(gco);
	delete gco; /* Disposes of the persistent handle */
}

extern "C" const char *pdf_jsimp_new_obj_cpp(pdf_jsimp *imp, pdf_jsimp_type *type, void *natobj, pdf_jsimp_obj **robj)
{
	Locker lock;
	PDFJSImpType *vType = reinterpret_cast<PDFJSImpType *>(type);
	HandleScope scope;
	Local<Object> obj = vType->templ->NewInstance();
	obj->SetInternalField(0, External::New(natobj));

	/* Arrange for destructor to be called on the client-side object
	 * when the v8 object is garbage collected */
	if (vType->dtr)
	{
		/* Wrap obj in a PDFJSImpGCObj, which takes a persistent handle to
		 * obj, and stores its type with it. The persistent handle tells v8
		 * it cannot just destroy obj leaving the client-side object hanging */
		PDFJSImpGCObj *gco = new PDFJSImpGCObj(obj, vType);
		/* Keep the wrapped object in a list, so that we can take back control
		 * of destroying client-side objects when shutting down this context */
		vType->imp->gclist.insert(gco);
		/* Tell v8 that it can destroy the persistent handle to obj when it has
		 * no further need for it, but it must inform us via gcCallback */
		gco->pobj.MakeWeak(gco, gcCallback);
	}

	*robj = reinterpret_cast<pdf_jsimp_obj *>(new PDFJSImpObject(obj));
	return NULL;
}

extern "C" const char *pdf_jsimp_drop_obj_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj)
{
	Locker lock;
	delete reinterpret_cast<PDFJSImpObject *>(obj);
	return NULL;
}

extern "C" const char *pdf_jsimp_to_type_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int *type)
{
	Locker lock;
	*type = reinterpret_cast<PDFJSImpObject *>(obj)->type();
	return NULL;
}

extern "C" const char *pdf_jsimp_from_string_cpp(pdf_jsimp *imp, char *str, pdf_jsimp_obj **obj)
{
	Locker lock;
	*obj = reinterpret_cast<pdf_jsimp_obj *>(new PDFJSImpObject(str));
	return NULL;
}

extern "C" const char *pdf_jsimp_to_string_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, char **str)
{
	Locker lock;
	*str = reinterpret_cast<PDFJSImpObject *>(obj)->toString();
	return NULL;
}

extern "C" const char *pdf_jsimp_from_number_cpp(pdf_jsimp *imp, double num, pdf_jsimp_obj **obj)
{
	Locker lock;
	*obj = reinterpret_cast<pdf_jsimp_obj *>(new PDFJSImpObject(num));
	return NULL;
}

extern "C" const char *pdf_jsimp_to_number_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, double *num)
{
	Locker lock;
	*num = reinterpret_cast<PDFJSImpObject *>(obj)->toNumber();
	return NULL;
}

extern "C" const char *pdf_jsimp_array_len_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int *len)
{
	Locker lock;
	Local<Object> jsobj = reinterpret_cast<PDFJSImpObject *>(obj)->toValue()->ToObject();
	Local<Array> arr = Local<Array>::Cast(jsobj);
	*len = arr->Length();
	return NULL;
}

extern "C" const char *pdf_jsimp_array_item_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, int i, pdf_jsimp_obj **item)
{
	Locker lock;
	Local<Object> jsobj = reinterpret_cast<PDFJSImpObject *>(obj)->toValue()->ToObject();
	*item = reinterpret_cast<pdf_jsimp_obj *>(new PDFJSImpObject(jsobj->Get(Number::New(i))));
	return NULL;
}

extern "C" const char *pdf_jsimp_property_cpp(pdf_jsimp *imp, pdf_jsimp_obj *obj, char *prop, pdf_jsimp_obj **pobj)
{
	Locker lock;
	Local<Object> jsobj = reinterpret_cast<PDFJSImpObject *>(obj)->toValue()->ToObject();
	*pobj = reinterpret_cast<pdf_jsimp_obj *>(new PDFJSImpObject(jsobj->Get(String::New(prop))));
	return NULL;
}

extern "C" const char *pdf_jsimp_execute_cpp(pdf_jsimp *imp, char *code)
{
	Locker lock;
	PDFJSImp *vImp = reinterpret_cast<PDFJSImp *>(imp);
	HandleScope scope;
	Context::Scope context_scope(vImp->context);
	Handle<Script> script = Script::Compile(String::New(code));
	if (script.IsEmpty())
		return "compile failed in pdf_jsimp_execute";
	script->Run();
	return NULL;
}

extern "C" const char *pdf_jsimp_execute_count_cpp(pdf_jsimp *imp, char *code, int count)
{
	Locker lock;
	PDFJSImp *vImp = reinterpret_cast<PDFJSImp *>(imp);
	HandleScope scope;
	Context::Scope context_scope(vImp->context);
	Handle<Script> script = Script::Compile(String::New(code, count));
	if (script.IsEmpty())
		return "compile failed in pdf_jsimp_execute_count";
	script->Run();
	return NULL;
}
