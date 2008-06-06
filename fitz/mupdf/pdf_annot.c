#include <fitz.h>
#include <mupdf.h>

static fz_error *
loadcomment(pdf_comment **commentp, pdf_xref *xref, fz_obj *dict)
{
	return fz_okay;
}

fz_error *
pdf_newlink(pdf_link **linkp, fz_rect bbox, fz_obj *dest)
{
	pdf_link *link;

	link = fz_malloc(sizeof(pdf_link));
	if (!link)
		return fz_outofmem;

	link->rect = bbox;
	link->dest = fz_keepobj(dest);
	link->next = nil;

	*linkp = link;
	return fz_okay;
}

void
pdf_droplink(pdf_link *link)
{
	if (link->next)
		pdf_droplink(link->next);
	if (link->dest)
		fz_dropobj(link->dest);
	fz_free(link);
}

static fz_obj *
resolvedest(pdf_xref *xref, fz_obj *dest)
{
	if (fz_isname(dest))
	{
		dest = fz_dictget(xref->dests, dest);
		if (dest)
			pdf_resolve(&dest, xref); /* XXX */
		return resolvedest(xref, dest);
	}

	else if (fz_isstring(dest))
	{
		dest = fz_dictget(xref->dests, dest);
		if (dest)
			pdf_resolve(&dest, xref); /* XXX */
		return resolvedest(xref, dest);
	}

	else if (fz_isarray(dest))
	{
		return fz_arrayget(dest, 0);
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

fz_error *
pdf_loadlink(pdf_link **linkp, pdf_xref *xref, fz_obj *dict)
{
	fz_error *error;
	pdf_link *link;
	fz_obj *dest;
	fz_obj *action;
	fz_obj *obj;
	fz_rect bbox;

	pdf_logpage("load link {\n");

	link = nil;
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
		error = pdf_resolve(&obj, xref);
		if (error)
			return fz_rethrow(error, "cannot resolve /Dest");
		dest = resolvedest(xref, obj);
		pdf_logpage("dest %d %d R\n", fz_tonum(dest), fz_togen(dest));
		fz_dropobj(obj);
	}

	action = fz_dictgets(dict, "A");
	if (action)
	{
		error = pdf_resolve(&action, xref);
		if (error)
			return fz_rethrow(error, "cannot resolve /A");

		obj = fz_dictgets(action, "S");
		if (!strcmp(fz_toname(obj), "GoTo"))
		{
			dest = resolvedest(xref, fz_dictgets(action, "D"));
			pdf_logpage("action goto %d %d R\n", fz_tonum(dest), fz_togen(dest));
		}
		else if (!strcmp(fz_toname(obj), "URI"))
		{
			dest = fz_dictgets(action, "URI");
			pdf_logpage("action uri %s\n", fz_tostrbuf(dest));
		}
		else
			pdf_logpage("action ... ?\n");

		fz_dropobj(action);
	}

	pdf_logpage("}\n");

	if (dest)
	{
		error = pdf_newlink(&link, bbox, dest);
		if (error)
			return fz_rethrow(error, "cannot create link");
		*linkp = link;
	}

	return fz_okay;
}

fz_error *
pdf_loadannots(pdf_comment **cp, pdf_link **lp, pdf_xref *xref, fz_obj *annots)
{
	fz_error *error;
	pdf_comment *comment;
	pdf_link *link;
	fz_obj *subtype;
	fz_obj *obj;
	int i;

	comment = nil;
	link = nil;

	pdf_logpage("load annotations {\n");

	for (i = 0; i < fz_arraylen(annots); i++)
	{
		obj = fz_arrayget(annots, i);
		error = pdf_resolve(&obj, xref);
		if (error)
		{
			if (link)
				pdf_droplink(link);
			return fz_rethrow(error, "cannot resolve annotation");
		}

		subtype = fz_dictgets(obj, "Subtype");
		if (!strcmp(fz_toname(subtype), "Link"))
		{
			pdf_link *temp = nil;

			error = pdf_loadlink(&temp, xref, obj);
			fz_dropobj(obj);
			if (error)
			{
				if (link)
					pdf_droplink(link);
				return fz_rethrow(error, "cannot load annotation link");
			}

			if (temp)
			{
				temp->next = link;
				link = temp;
			}
		}
		else
		{
			error = loadcomment(&comment, xref, obj);
			fz_dropobj(obj);
			if (error)
			{
				pdf_droplink(link);
				return fz_rethrow(error, "cannot load annotation comment");
			}
		}
	}

	pdf_logpage("}\n");

	*cp = comment;
	*lp = link;
	return fz_okay;
}

