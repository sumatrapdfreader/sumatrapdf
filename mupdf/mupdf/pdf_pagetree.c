#include "fitz.h"
#include "mupdf.h"

struct stuff
{
	fz_obj *resources;
	fz_obj *mediabox;
	fz_obj *cropbox;
	fz_obj *rotate;
};

static fz_error
loadpagetree(pdf_xref *xref, pdf_pagetree *pages,
		struct stuff inherit, fz_obj *obj, fz_obj *ref, int *pagenum)
{
	fz_error error;
	fz_obj *type;
	fz_obj *kids;
	fz_obj *kref, *kobj;
	fz_obj *inh;
	char *typestr;
	int i;

	if (fz_isnull(obj))
		return fz_throw("pagetree node is missing");

	type = fz_dictgets(obj, "Type");
	if (!type)
	{
		fz_warn("pagetree node (%d %d R) lacks required type", fz_tonum(ref), fz_togen(ref));

		kids = fz_dictgets(obj, "Kids");
		if (kids)
		{
			fz_warn("guessing it may be a pagetree node, continuing...");
			typestr = "Pages";
		}
		else
		{
			fz_warn("guessing it may be a page, continuing...");
			typestr = "Page";
		}
	}
	else
		typestr = fz_toname(type);

	if (strcmp(typestr, "Page") == 0)
	{
		pdf_logpage("page %d (%d %d R)\n", *pagenum, ref->u.r.oid, ref->u.r.gid);
		(*pagenum)++;

		if (inherit.resources && !fz_dictgets(obj, "Resources"))
		{
			pdf_logpage("inherit resources (%d)\n", pages->cursor);
			error = fz_dictputs(obj, "Resources", inherit.resources);
			if (error) return fz_rethrow(error, "cannot inherit page tree resources");
		}

		if (inherit.mediabox && !fz_dictgets(obj, "MediaBox"))
		{
			pdf_logpage("inherit mediabox (%d)\n", pages->cursor);
			error = fz_dictputs(obj, "MediaBox", inherit.mediabox);
			if (error) return fz_rethrow(error, "cannot inherit page tree mediabox");
		}

		if (inherit.cropbox && !fz_dictgets(obj, "CropBox"))
		{
			pdf_logpage("inherit cropbox (%d)\n", pages->cursor);
			error = fz_dictputs(obj, "CropBox", inherit.cropbox);
			if (error) return fz_rethrow(error, "cannot inherit page tree cropbox");
		}

		if (inherit.rotate && !fz_dictgets(obj, "Rotate"))
		{
			pdf_logpage("inherit rotate (%d)\n", pages->cursor);
			error = fz_dictputs(obj, "Rotate", inherit.rotate);
			if (error) return fz_rethrow(error, "cannot inherit page tree rotate");
		}

		pages->pref[pages->cursor] = fz_keepobj(ref);
		pages->pobj[pages->cursor] = fz_keepobj(obj);
		pages->cursor ++;
	}

	else if (strcmp(typestr, "Pages") == 0)
	{
		inh = fz_dictgets(obj, "Resources");
		if (inh) inherit.resources = inh;

		inh = fz_dictgets(obj, "MediaBox");
		if (inh) inherit.mediabox = inh;

		inh = fz_dictgets(obj, "CropBox");
		if (inh) inherit.cropbox = inh;

		inh = fz_dictgets(obj, "Rotate");
		if (inh) inherit.rotate = inh;

		kids = fz_dictgets(obj, "Kids");
		error = pdf_resolve(&kids, xref);
		if (error)
			return fz_rethrow(error, "cannot resolve /Kids");

		pdf_logpage("subtree %d pages (%d %d R) {\n",
				fz_arraylen(kids), ref->u.r.oid, ref->u.r.gid);

		for (i = 0; i < fz_arraylen(kids); i++)
		{
			kref = fz_arrayget(kids, i);

			error = pdf_loadindirect(&kobj, xref, kref);
			if (error) { fz_dropobj(kids); return fz_rethrow(error, "cannot load kid"); }

			if (kobj == obj)
			{
				/* prevent infinite recursion possible in maliciously crafted PDFs */
				fz_dropobj(kids);
				return fz_throw("corrupted pdf file");
			}

			error = loadpagetree(xref, pages, inherit, kobj, kref, pagenum);
			fz_dropobj(kobj);
			if (error) { fz_dropobj(kids); return fz_rethrow(error, "cannot load pagesubtree (%d %d R)", fz_tonum(kref), fz_togen(kref)); }
		}

		fz_dropobj(kids);

		pdf_logpage("}\n");
	}

	return fz_okay;
}

void
pdf_debugpagetree(pdf_pagetree *pages)
{
	int i;
	printf("<<\n  /Type /Pages\n  /Count %d\n  /Kids [\n", pages->count);
	for (i = 0; i < pages->count; i++) {
		printf("    %% page %d\n", i + 1);
		printf("    ");
		fz_debugobj(pages->pref[i]);
	}
	printf("  ]\n>>\n");
}

fz_error
pdf_loadpagetree(pdf_pagetree **pp, pdf_xref *xref)
{
	fz_error error;
	struct stuff inherit;
	pdf_pagetree *p = nil;
	fz_obj *catalog = nil;
	fz_obj *pages = nil;
	fz_obj *trailer;
	fz_obj *ref;
	fz_obj *treeref;
	int count;
	int pagenum = 1;

	inherit.resources = nil;
	inherit.mediabox = nil;
	inherit.cropbox = nil;
	inherit.rotate = nil;

	trailer = xref->trailer;

	ref = fz_dictgets(trailer, "Root");
	error = pdf_loadindirect(&catalog, xref, ref);
	if (error) { error = fz_rethrow(error, "cannot load Root object"); goto cleanup; }

	treeref = fz_dictgets(catalog, "Pages");
	error = pdf_loadindirect(&pages, xref, treeref);
	if (error) { error = fz_rethrow(error, "cannot load Pages object"); goto cleanup; }

	ref = fz_dictgets(pages, "Count");
	count = fz_toint(ref);

	p = fz_malloc(sizeof(pdf_pagetree));
	if (!p) { error = fz_rethrow(-1, "out of memory: page tree struct"); goto cleanup; }

	pdf_logpage("load pagetree %d pages (%d %d R) ptr=%p {\n", count, treeref->u.r.oid, treeref->u.r.gid, p);

	p->pref = nil;
	p->pobj = nil;
	p->count = count;
	p->cursor = 0;

	p->pref = fz_malloc(sizeof(fz_obj*) * count);
	if (!p->pref) { error = fz_rethrow(-1, "out of memory: page tree reference array"); goto cleanup; }

	p->pobj = fz_malloc(sizeof(fz_obj*) * count);
	if (!p->pobj) { error = fz_rethrow(-1, "out of memory: page tree object array"); goto cleanup; }

	error = loadpagetree(xref, p, inherit, pages, treeref, &pagenum);
	if (error) { error = fz_rethrow(error, "cannot load pagetree (%d %d R)", fz_tonum(treeref), fz_togen(treeref)); goto cleanup; }

	fz_dropobj(pages);
	fz_dropobj(catalog);

	pdf_logpage("}\n", count);

	*pp = p;
	return fz_okay;

cleanup:
	if (pages) fz_dropobj(pages);
	if (catalog) fz_dropobj(catalog);
	if (p) {
		fz_free(p->pref);
		fz_free(p->pobj);
		fz_free(p);
	}
	return error; /* already rethrown */
}

int
pdf_getpagecount(pdf_pagetree *pages)
{
	return pages->count;
}

fz_obj *
pdf_getpageobject(pdf_pagetree *pages, int p)
{
	if (p < 0 || p >= pages->count)
		return nil;
	return pages->pobj[p];
}

fz_obj *
pdf_getpagereference(pdf_pagetree *pages, int p)
{
	if (p < 0 || p >= pages->count)
		return nil;
	return pages->pref[p];
}

void
pdf_droppagetree(pdf_pagetree *pages)
{
	int i;

	pdf_logpage("drop pagetree %p\n", pages);

	for (i = 0; i < pages->count; i++) {
		if (pages->pref[i])
			fz_dropobj(pages->pref[i]);
		if (pages->pobj[i])
			fz_dropobj(pages->pobj[i]);
	}

	fz_free(pages->pref);
	fz_free(pages->pobj);
	fz_free(pages);
}

