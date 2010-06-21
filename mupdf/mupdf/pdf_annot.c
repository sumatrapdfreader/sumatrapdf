#include "fitz.h"
#include "mupdf.h"

pdf_link *
pdf_newlink(pdf_linkkind kind, fz_rect bbox, fz_obj *dest)
{
	pdf_link *link = fz_malloc(sizeof(pdf_link));
	link->kind = kind;
	link->rect = bbox;
	link->dest = fz_keepobj(dest);
	link->next = nil;
	return link;
}

void
pdf_freelink(pdf_link *link)
{
	if (!link)
		return;
	if (link->next)
		pdf_freelink(link->next);
	if (link->dest)
		fz_dropobj(link->dest);
	fz_free(link);
}

static fz_obj *
resolvedest(pdf_xref *xref, fz_obj *dest)
{
	if (fz_isname(dest) || fz_isstring(dest))
	{
		dest = pdf_lookupdest(xref, dest);
		return resolvedest(xref, dest);
	}

	else if (fz_isarray(dest))
	{
		return dest; /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=275 */
	}

	else if (fz_isdict(dest))
	{
		dest = fz_dictgets(dest, "D");
		return resolvedest(xref, dest);
	}

	else if (fz_isindirect(dest))
		return dest;

	return nil;
}

pdf_link *
pdf_loadlink(pdf_xref *xref, fz_obj *dict)
{
	fz_obj *dest;
	fz_obj *action;
	fz_obj *obj;
	fz_rect bbox;
	pdf_linkkind kind;

	pdf_logpage("load link {\n");

	dest = nil;

	obj = fz_dictgets(dict, "Rect");
	if (obj)
	{
		bbox = pdf_torect(obj);
		pdf_logpage("rect [%g %g %g %g]\n",
			bbox.x0, bbox.y0,
			bbox.x1, bbox.y1);
	}
	else
		bbox = fz_emptyrect;

	obj = fz_dictgets(dict, "Dest");
	if (obj)
	{
		kind = PDF_LGOTO;
		dest = resolvedest(xref, obj);
		pdf_logpage("dest (%d %d R)\n", fz_tonum(dest), fz_togen(dest));
	}

	action = fz_dictgets(dict, "A");
	if (action)
	{
		obj = fz_dictgets(action, "S");
		if (fz_isname(obj) && !strcmp(fz_toname(obj), "GoTo"))
		{
			kind = PDF_LGOTO;
			dest = resolvedest(xref, fz_dictgets(action, "D"));
			pdf_logpage("action goto (%d %d R)\n", fz_tonum(dest), fz_togen(dest));
		}
		else if (fz_isname(obj) && !strcmp(fz_toname(obj), "URI"))
		{
			kind = PDF_LURI;
			dest = fz_dictgets(action, "URI");
			pdf_logpage("action uri %s\n", fz_tostrbuf(dest));
		}
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=726 */
		else if (fz_isname(obj) && !strcmp(fz_toname(obj), "Launch"))
		{
			kind = PDF_LLAUNCH;
			dest = fz_dictgets(action, "F");
			pdf_logpage("action launch (%d %d R)\n", fz_tonum(dest), fz_togen(dest));
		}
		else
		{
			pdf_logpage("unhandled link action, ignoring link\n");
			dest = nil;
		}
	}

	pdf_logpage("}\n");

	if (dest)
	{
		return pdf_newlink(kind, bbox, dest);
	}

	return nil;
}

void
pdf_loadannots(pdf_link **lp, pdf_xref *xref, fz_obj *annots)
{
	pdf_link *link;
	fz_obj *subtype;
	fz_obj *obj;
	int i;

	link = nil;

	pdf_logpage("load annotations {\n");

	for (i = 0; i < fz_arraylen(annots); i++)
	{
		obj = fz_arrayget(annots, i);

		subtype = fz_dictgets(obj, "Subtype");
		if (fz_isname(subtype) && !strcmp(fz_toname(subtype), "Link"))
		{
			pdf_link *temp = pdf_loadlink(xref, obj);
			if (temp)
			{
				temp->next = link;
				link = temp;
			}
		}
	}

	pdf_logpage("}\n");

	*lp = link;
}

