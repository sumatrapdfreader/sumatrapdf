#include "pdftool.h"

char *basename = nil;
pdf_xref *xref = nil;
pdf_pagetree *pagetree = nil;
void (*cleanup)(void) = nil;

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

	if (xref->crypt)
	{
		okay = pdf_setpassword(xref->crypt, password);
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
}

void closexref(void)
{
	if (cleanup)
		cleanup();

	if (pagetree)
	{
		pdf_droppagetree(pagetree);
		pagetree = nil;
	}

	if (xref)
	{
		pdf_closexref(xref);
		xref = nil;
	}

	basename = nil;
}

void loadpagetree(void)
{
	fz_error error;

	error = pdf_loadpagetree(&pagetree, xref);
	if (error)
		die(error);
}

