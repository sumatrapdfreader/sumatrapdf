#include "pdftool.h"

char *basename = nil;
pdf_xref *xref = nil;
int pagecount = 0;
static void (*cleanup)(void) = nil;

void closexref(void);

void die(fz_error error)
{
	fz_catch(error, "aborting");
	if (cleanup)
		cleanup();
	closexref();
	exit(1);
}

void setcleanup(void (*func)(void))
{
	cleanup = func;
}

void openxref(char *filename, char *password, int dieonbadpass)
{
	fz_error error;
	fz_obj *obj;
	int okay;

	basename = strrchr(filename, '/');
	if (!basename)
		basename = filename;
	else
		basename++;

	error = pdf_newxref(&xref);
	if (error)
		die(error);

	error = pdf_loadxref(xref, filename);
	if (error)
	{
		fz_catch(error, "trying to repair");
		error = pdf_repairxref(xref, filename);
		if (error)
			die(error);
	}

	error = pdf_decryptxref(xref);
	if (error)
		die(error);

	if (pdf_needspassword(xref))
	{
		okay = pdf_authenticatepassword(xref, password);
		if (!okay && !dieonbadpass)
			fz_warn("invalid password, attempting to continue.");
		else if (!okay && dieonbadpass)
			die(fz_throw("invalid password"));
	}

	obj = fz_dictgets(xref->trailer, "Root");
	xref->root = fz_resolveindirect(obj);
	if (xref->root)
		fz_keepobj(xref->root);

	obj = fz_dictgets(xref->trailer, "Info");
	xref->info = fz_resolveindirect(obj);
	if (xref->info)
		fz_keepobj(xref->info);

	error = pdf_getpagecount(xref, &pagecount);
	if (error)
	{
		fz_catch(error, "cannot determine page count, attempting to continue.");
		pagecount = 0;
	}
}

void closexref(void)
{
	if (cleanup)
		cleanup();

	if (xref)
	{
		pdf_closexref(xref);
		xref = nil;
	}

	basename = nil;
}

