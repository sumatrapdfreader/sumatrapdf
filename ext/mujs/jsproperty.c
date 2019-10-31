#include "jsi.h"
#include "jsvalue.h"

/*
	Use an AA-tree to quickly look up properties in objects:

	The level of every leaf node is one.
	The level of every left child is one less than its parent.
	The level of every right child is equal or one less than its parent.
	The level of every right grandchild is less than its grandparent.
	Every node of level greater than one has two children.

	A link where the child's level is equal to that of its parent is called a horizontal link.
	Individual right horizontal links are allowed, but consecutive ones are forbidden.
	Left horizontal links are forbidden.

	skew() fixes left horizontal links.
	split() fixes consecutive right horizontal links.
*/

static js_Property sentinel = {
	"",
	&sentinel, &sentinel,
	0, 0,
	{ {0}, {0}, JS_TUNDEFINED },
	NULL, NULL
};

static js_Property *newproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *node = js_malloc(J, sizeof *node);
	node->name = js_intern(J, name);
	node->left = node->right = &sentinel;
	node->level = 1;
	node->atts = 0;
	node->value.type = JS_TUNDEFINED;
	node->value.u.number = 0;
	node->getter = NULL;
	node->setter = NULL;
	++obj->count;
	return node;
}

static js_Property *lookup(js_Property *node, const char *name)
{
	while (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c == 0)
			return node;
		else if (c < 0)
			node = node->left;
		else
			node = node->right;
	}
	return NULL;
}

static js_Property *skew(js_Property *node)
{
	if (node->left->level == node->level) {
		js_Property *temp = node;
		node = node->left;
		temp->left = node->right;
		node->right = temp;
	}
	return node;
}

static js_Property *split(js_Property *node)
{
	if (node->right->right->level == node->level) {
		js_Property *temp = node;
		node = node->right;
		temp->right = node->left;
		node->left = temp;
		++node->level;
	}
	return node;
}

static js_Property *insert(js_State *J, js_Object *obj, js_Property *node, const char *name, js_Property **result)
{
	if (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c < 0)
			node->left = insert(J, obj, node->left, name, result);
		else if (c > 0)
			node->right = insert(J, obj, node->right, name, result);
		else
			return *result = node;
		node = skew(node);
		node = split(node);
		return node;
	}
	return *result = newproperty(J, obj, name);
}

static void freeproperty(js_State *J, js_Object *obj, js_Property *node)
{
	js_free(J, node);
	--obj->count;
}

static js_Property *delete(js_State *J, js_Object *obj, js_Property *node, const char *name)
{
	js_Property *temp, *succ;

	if (node != &sentinel) {
		int c = strcmp(name, node->name);
		if (c < 0) {
			node->left = delete(J, obj, node->left, name);
		} else if (c > 0) {
			node->right = delete(J, obj, node->right, name);
		} else {
			if (node->left == &sentinel) {
				temp = node;
				node = node->right;
				freeproperty(J, obj, temp);
			} else if (node->right == &sentinel) {
				temp = node;
				node = node->left;
				freeproperty(J, obj, temp);
			} else {
				succ = node->right;
				while (succ->left != &sentinel)
					succ = succ->left;
				node->name = succ->name;
				node->atts = succ->atts;
				node->value = succ->value;
				node->right = delete(J, obj, node->right, succ->name);
			}
		}

		if (node->left->level < node->level - 1 ||
			node->right->level < node->level - 1)
		{
			if (node->right->level > --node->level)
				node->right->level = node->level;
			node = skew(node);
			node->right = skew(node->right);
			node->right->right = skew(node->right->right);
			node = split(node);
			node->right = split(node->right);
		}
	}
	return node;
}

js_Object *jsV_newobject(js_State *J, enum js_Class type, js_Object *prototype)
{
	js_Object *obj = js_malloc(J, sizeof *obj);
	memset(obj, 0, sizeof *obj);
	obj->gcmark = 0;
	obj->gcnext = J->gcobj;
	J->gcobj = obj;
	++J->gccounter;

	obj->type = type;
	obj->properties = &sentinel;
	obj->prototype = prototype;
	obj->extensible = 1;
	return obj;
}

js_Property *jsV_getownproperty(js_State *J, js_Object *obj, const char *name)
{
	return lookup(obj->properties, name);
}

js_Property *jsV_getpropertyx(js_State *J, js_Object *obj, const char *name, int *own)
{
	*own = 1;
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref)
			return ref;
		obj = obj->prototype;
		*own = 0;
	} while (obj);
	return NULL;
}

js_Property *jsV_getproperty(js_State *J, js_Object *obj, const char *name)
{
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref)
			return ref;
		obj = obj->prototype;
	} while (obj);
	return NULL;
}

static js_Property *jsV_getenumproperty(js_State *J, js_Object *obj, const char *name)
{
	do {
		js_Property *ref = lookup(obj->properties, name);
		if (ref && !(ref->atts & JS_DONTENUM))
			return ref;
		obj = obj->prototype;
	} while (obj);
	return NULL;
}

js_Property *jsV_setproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *result;

	if (!obj->extensible) {
		result = lookup(obj->properties, name);
		if (J->strict && !result)
			js_typeerror(J, "object is non-extensible");
		return result;
	}

	obj->properties = insert(J, obj, obj->properties, name, &result);

	return result;
}

void jsV_delproperty(js_State *J, js_Object *obj, const char *name)
{
	obj->properties = delete(J, obj, obj->properties, name);
}

/* Flatten hierarchy of enumerable properties into an iterator object */

static js_Iterator *itwalk(js_State *J, js_Iterator *iter, js_Property *prop, js_Object *seen)
{
	if (prop->right != &sentinel)
		iter = itwalk(J, iter, prop->right, seen);
	if (!(prop->atts & JS_DONTENUM)) {
		if (!seen || !jsV_getenumproperty(J, seen, prop->name)) {
			js_Iterator *head = js_malloc(J, sizeof *head);
			head->name = prop->name;
			head->next = iter;
			iter = head;
		}
	}
	if (prop->left != &sentinel)
		iter = itwalk(J, iter, prop->left, seen);
	return iter;
}

static js_Iterator *itflatten(js_State *J, js_Object *obj)
{
	js_Iterator *iter = NULL;
	if (obj->prototype)
		iter = itflatten(J, obj->prototype);
	if (obj->properties != &sentinel)
		iter = itwalk(J, iter, obj->properties, obj->prototype);
	return iter;
}

js_Object *jsV_newiterator(js_State *J, js_Object *obj, int own)
{
	char buf[32];
	int k;
	js_Object *io = jsV_newobject(J, JS_CITERATOR, NULL);
	io->u.iter.target = obj;
	if (own) {
		io->u.iter.head = NULL;
		if (obj->properties != &sentinel)
			io->u.iter.head = itwalk(J, io->u.iter.head, obj->properties, NULL);
	} else {
		io->u.iter.head = itflatten(J, obj);
	}
	if (obj->type == JS_CSTRING) {
		js_Iterator *tail = io->u.iter.head;
		if (tail)
			while (tail->next)
				tail = tail->next;
		for (k = 0; k < obj->u.s.length; ++k) {
			js_itoa(buf, k);
			if (!jsV_getenumproperty(J, obj, buf)) {
				js_Iterator *node = js_malloc(J, sizeof *node);
				node->name = js_intern(J, js_itoa(buf, k));
				node->next = NULL;
				if (!tail)
					io->u.iter.head = tail = node;
				else {
					tail->next = node;
					tail = node;
				}
			}
		}
	}
	return io;
}

const char *jsV_nextiterator(js_State *J, js_Object *io)
{
	int k;
	if (io->type != JS_CITERATOR)
		js_typeerror(J, "not an iterator");
	while (io->u.iter.head) {
		js_Iterator *next = io->u.iter.head->next;
		const char *name = io->u.iter.head->name;
		js_free(J, io->u.iter.head);
		io->u.iter.head = next;
		if (jsV_getproperty(J, io->u.iter.target, name))
			return name;
		if (io->u.iter.target->type == JS_CSTRING)
			if (js_isarrayindex(J, name, &k) && k < io->u.iter.target->u.s.length)
				return name;
	}
	return NULL;
}

/* Walk all the properties and delete them one by one for arrays */

void jsV_resizearray(js_State *J, js_Object *obj, int newlen)
{
	char buf[32];
	const char *s;
	int k;
	if (newlen < obj->u.a.length) {
		if (obj->u.a.length > obj->count * 2) {
			js_Object *it = jsV_newiterator(J, obj, 1);
			while ((s = jsV_nextiterator(J, it))) {
				k = jsV_numbertointeger(jsV_stringtonumber(J, s));
				if (k >= newlen && !strcmp(s, jsV_numbertostring(J, buf, k)))
					jsV_delproperty(J, obj, s);
			}
		} else {
			for (k = newlen; k < obj->u.a.length; ++k) {
				jsV_delproperty(J, obj, js_itoa(buf, k));
			}
		}
	}
	obj->u.a.length = newlen;
}
