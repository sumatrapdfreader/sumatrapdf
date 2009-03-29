#include "fitz.h"
#include "mupdf.h"

/*
 * Load CMap stream in PDF file
 */
fz_error
pdf_loadembeddedcmap(pdf_cmap **cmapp, pdf_xref *xref, fz_obj *stmref)
{
    fz_error error = fz_okay;
    fz_obj *stmobj = stmref;
    fz_stream *file = nil;
    pdf_cmap *cmap = nil;
    pdf_cmap *usecmap;
    fz_obj *wmode;
    fz_obj *obj;

    if ((*cmapp = pdf_finditem(xref->store, PDF_KCMAP, stmref)))
    {
	pdf_keepcmap(*cmapp);
	return fz_okay;
    }

    pdf_logfont("load embedded cmap (%d %d R) {\n", fz_tonum(stmref), fz_togen(stmref));

    error = pdf_resolve(&stmobj, xref);
    if (error)
	return fz_rethrow(error, "cannot resolve cmap object");

    error = pdf_openstream(&file, xref, fz_tonum(stmref), fz_togen(stmref));
    if (error)
    {
	error = fz_rethrow(error, "cannot open cmap stream");
	goto cleanup;
    }

    error = pdf_parsecmap(&cmap, file);
    if (error)
    {
	error = fz_rethrow(error, "cannot parse cmap stream");
	goto cleanup;
    }

    fz_dropstream(file);

    wmode = fz_dictgets(stmobj, "WMode");
    if (fz_isint(wmode))
    {
	pdf_logfont("wmode %d\n", wmode);
	pdf_setwmode(cmap, fz_toint(wmode));
    }

    obj = fz_dictgets(stmobj, "UseCMap");
    if (fz_isname(obj))
    {
	pdf_logfont("usecmap /%s\n", fz_toname(obj));
	error = pdf_loadsystemcmap(&usecmap, fz_toname(obj));
	if (error)
	{
	    error = fz_rethrow(error, "cannot load system usecmap '%s'", fz_toname(obj));
	    goto cleanup;
	}
	pdf_setusecmap(cmap, usecmap);
	pdf_dropcmap(usecmap);
    }
    else if (fz_isindirect(obj))
    {
	pdf_logfont("usecmap (%d %d R)\n", fz_tonum(obj), fz_togen(obj));
	error = pdf_loadembeddedcmap(&usecmap, xref, obj);
	if (error)
	{
	    error = fz_rethrow(error, "cannot load embedded usecmap");
	    goto cleanup;
	}
	pdf_setusecmap(cmap, usecmap);
	pdf_dropcmap(usecmap);
    }

    pdf_logfont("}\n");

    error = pdf_storeitem(xref->store, PDF_KCMAP, stmref, cmap);
    if (error)
    {
	error = fz_rethrow(error, "cannot store cmap resource");
	goto cleanup;
    }

    fz_dropobj(stmobj);

    *cmapp = cmap;
    return fz_okay;

cleanup:
    if (file)
	fz_dropstream(file);
    if (cmap)
	pdf_dropcmap(cmap);
    fz_dropobj(stmobj);
    return error; /* already rethrown */
}

/*
 * Create an Identity-* CMap (for both 1 and 2-byte encodings)
 */
fz_error
pdf_newidentitycmap(pdf_cmap **cmapp, int wmode, int bytes)
{
    fz_error error;
    pdf_cmap *cmap;

    error = pdf_newcmap(&cmap);
    if (error)
	return fz_rethrow(error, "cannot create cmap");

    sprintf(cmap->cmapname, "Identity-%c", wmode ? 'V' : 'H');

    error = pdf_addcodespace(cmap, 0x0000, 0xffff, bytes);
    if (error) {
	pdf_dropcmap(cmap);
	return fz_rethrow(error, "cannot add code space");
    }

    error = pdf_maprangetorange(cmap, 0x0000, 0xffff, 0);
    if (error) {
	pdf_dropcmap(cmap);
	return fz_rethrow(error, "cannot map <0000> to <ffff>");
    }

    error = pdf_sortcmap(cmap);
    if (error) {
	pdf_dropcmap(cmap);
	return fz_rethrow(error, "cannot sort cmap");
    }

    pdf_setwmode(cmap, wmode);

    *cmapp = cmap;
    return fz_okay;
}

/*
 * Load predefined CMap from system.
 */
fz_error
pdf_loadsystemcmap(pdf_cmap **cmapp, char *cmapname)
{
    fz_error error;
    pdf_cmap *usecmap;
    pdf_cmap *cmap;
    int i;

    pdf_logfont("loading system cmap %s\n", cmapname);

    for (i = 0; pdf_cmaptable[i]; i++)
    {
	if (!strcmp(cmapname, pdf_cmaptable[i]->cmapname))
	{
	    cmap = pdf_cmaptable[i];
	    if (cmap->usecmapname[0] && !cmap->usecmap)
	    {
		error = pdf_loadsystemcmap(&usecmap, cmap->usecmapname);
		if (error)
		    return fz_rethrow(error, "could not load usecmap: %s", cmap->usecmapname);
		pdf_setusecmap(cmap, usecmap);
	    }
	    *cmapp = cmap;
	    return fz_okay;
	}
    }

    return fz_throw("no builtin cmap file: %s", cmapname);
}

