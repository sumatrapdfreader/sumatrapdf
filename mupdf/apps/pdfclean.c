/*
 * PDF cleaning tool: general purpose pdf syntax washer.
 *
 * Rewrite PDF with pretty printed objects.
 * Garbage collect unreachable objects.
 * Inflate compressed streams.
 * Encrypt output.
 */

#include "pdftool.h"

static FILE *out = NULL;

static char *uselist = NULL;
static int *ofslist = NULL;
static int *genlist = NULL;

static int dogarbage = 0;
static int doexpand = 0;

/*
 * Garbage collect objects not reachable from the trailer.
 */

static fz_error sweepref(pdf_xref *xref, fz_obj *ref);

static fz_error sweepobj(pdf_xref *xref, fz_obj *obj)
{
	fz_error error;
	int i;

	if (fz_isindirect(obj))
		return sweepref(xref, obj);

	if (fz_isdict(obj))
	{
		for (i = 0; i < fz_dictlen(obj); i++)
		{
			error = sweepobj(xref, fz_dictgetval(obj, i));
			if (error)
				return error; /* too deeply nested for rethrow */
		}
	}

	if (fz_isarray(obj))
	{
		for (i = 0; i < fz_arraylen(obj); i++)
		{
			error = sweepobj(xref, fz_arrayget(obj, i));
			if (error)
				return error; /* too deeply nested for rethrow */
		}
	}

	return fz_okay;
}

static fz_error sweepref(pdf_xref *xref, fz_obj *ref)
{
	fz_error error;
	fz_obj *obj;
	fz_obj *len;
	int oid, gen;

	oid = fz_tonum(ref);
	gen = fz_tonum(ref);

	if (oid < 0 || oid >= xref->len)
		return fz_throw("object out of range (%d %d R)", oid, gen);

	if (uselist[oid])
		return fz_okay;

	uselist[oid] = 1;

	obj = fz_resolveindirect(ref);

	/* Bake in /Length in stream objects */
	if (xref->table[oid].stmofs)
	{
		len = fz_dictgets(obj, "Length");
		if (fz_isindirect(len))
		{
			len = fz_resolveindirect(len);
			fz_dictputs(obj, "Length", len);
		}
	}

	error = sweepobj(xref, obj);
	if (error)
	{
		fz_dropobj(obj);
		return error; /* too deeply nested for rethrow */
	}

	return fz_okay;
}

static void preloadobjstms(void)
{
	fz_error error;
	fz_obj *obj;
	int oid;

	for (oid = 0; oid < xref->len; oid++)
	{
		if (xref->table[oid].type == 'o')
		{
			error = pdf_loadobject(&obj, xref, oid, 0);
			if (error)
				die(error);
			fz_dropobj(obj);
		}
	}
}

static void copystream(fz_obj *obj, int oid, int gen)
{
	fz_error error;
	fz_buffer *buf;

	error = pdf_loadrawstream(&buf, xref, oid, gen);
	if (error)
		die(error);

	fprintf(out, "%d %d obj\n", oid, gen);
	fz_fprintobj(out, obj, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->rp, 1, buf->wp - buf->rp, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_dropbuffer(buf);
}

static void expandstream(fz_obj *obj, int oid, int gen)
{
	fz_error error;
	fz_buffer *buf;
	fz_obj *newdict, *newlen;

	error = pdf_loadstream(&buf, xref, oid, gen);
	if (error)
		die(error);

	newdict = fz_copydict(obj);
	fz_dictdels(newdict, "Filter");
	fz_dictdels(newdict, "DecodeParms");

	newlen = fz_newint(buf->wp - buf->rp);
	fz_dictputs(newdict, "Length", newlen);
	fz_dropobj(newlen);

	fprintf(out, "%d %d obj\n", oid, gen);
	fz_fprintobj(out, newdict, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->rp, 1, buf->wp - buf->rp, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_dropobj(newdict);

	fz_dropbuffer(buf);
}

static void saveobject(int oid, int gen)
{
	fz_error error;
	fz_obj *obj;
	fz_obj *type;

	error = pdf_loadobject(&obj, xref, oid, gen);
	if (error)
		die(error);

	/* skip ObjStm and XRef objects */
	if (fz_isdict(obj))
	{
		type = fz_dictgets(obj, "Type");
		if (fz_isname(type) && !strcmp(fz_toname(type), "ObjStm"))
		{
			uselist[oid] = 0;
			fz_dropobj(obj);
			return;
		}
		if (fz_isname(type) && !strcmp(fz_toname(type), "XRef"))
		{
			uselist[oid] = 0;
			fz_dropobj(obj);
			return;
		}
	}


	if (!xref->table[oid].stmofs)
	{
		fprintf(out, "%d %d obj\n", oid, gen);
		fz_fprintobj(out, obj, !doexpand);
		fprintf(out, "endobj\n\n");
	}
	else
	{
		if (doexpand)
			expandstream(obj, oid, gen);
		else
			copystream(obj, oid, gen);
	}


	fz_dropobj(obj);
}

static void savexref(void)
{
	fz_obj *trailer;
	fz_obj *obj;
	int startxref;
	int oid;

	startxref = ftell(out);

	fprintf(out, "xref\n0 %d\n", xref->len);
	for (oid = 0; oid < xref->len; oid++)
	{
		if (uselist[oid])
			fprintf(out, "%010d %05d n \n", ofslist[oid], genlist[oid]);
		else
			fprintf(out, "%010d %05d f \n", ofslist[oid], genlist[oid]);
	}
	fprintf(out, "\n");

	trailer = fz_newdict(5);

	obj = fz_newint(xref->len);
	fz_dictputs(trailer, "Size", obj);
	fz_dropobj(obj);

	obj = fz_dictgets(xref->trailer, "Info");
	if (obj)
		fz_dictputs(trailer, "Info", obj);

	obj = fz_dictgets(xref->trailer, "Root");
	if (obj)
		fz_dictputs(trailer, "Root", obj);

	obj = fz_dictgets(xref->trailer, "ID");
	if (obj)
		fz_dictputs(trailer, "ID", obj);

	fprintf(out, "trailer\n");
	fz_fprintobj(out, trailer, !doexpand);
	fprintf(out, "\n");

	fprintf(out, "startxref\n%d\n%%%%EOF\n", startxref);
}

static void cleanusage(void)
{
	fprintf(stderr,
		"usage: pdfclean [options] input.pdf [outfile.pdf]\n"
		"  -p -\tpassword for decryption\n"
		"  -g  \tgarbage collect unused objects\n"
		"  -x  \texpand compressed streams\n");
	exit(1);
}

int main(int argc, char **argv)
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	fz_error error;
	int c, oid;
	int lastfree;

	while ((c = fz_getopt(argc, argv, "gxp:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'g': dogarbage ++; break;
		case 'x': doexpand ++; break;
		default: cleanusage(); break;
		}
	}

	if (argc - fz_optind < 1)
		cleanusage();

	infile = argv[fz_optind++];
	if (argc - fz_optind > 0)
		outfile = argv[fz_optind++];

	openxref(infile, password, 0);

	out = fopen(outfile, "wb");
	if (!out)
		die(fz_throw("cannot open output file '%s'", outfile));

	fprintf(out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
	fprintf(out, "%%\342\343\317\323\n\n");

	uselist = malloc(sizeof (char) * (xref->len + 1));
	ofslist = malloc(sizeof (int) * (xref->len + 1));
	genlist = malloc(sizeof (int) * (xref->len + 1));

	for (oid = 0; oid < xref->len; oid++)
	{
		uselist[oid] = 0;
		ofslist[oid] = 0;
		genlist[oid] = 0;
	}

	/* Make sure any objects hidden in compressed streams have been loaded */
	preloadobjstms();

	/* Sweep & mark objects from the trailer */
	error = sweepobj(xref, xref->trailer);
	if (error)
		die(fz_rethrow(error, "cannot mark used objects"));

	for (oid = 0; oid < xref->len; oid++)
	{
		if (xref->table[oid].type == 'f')
			uselist[oid] = 0;

		if (xref->table[oid].type == 'f')
			genlist[oid] = xref->table[oid].gen;
		if (xref->table[oid].type == 'n')
			genlist[oid] = xref->table[oid].gen;
		if (xref->table[oid].type == 'o')
			genlist[oid] = 0;

		if (dogarbage && !uselist[oid])
			continue;

		if (xref->table[oid].type == 'n' || xref->table[oid].type == 'o')
		{
			ofslist[oid] = ftell(out);
			saveobject(oid, genlist[oid]);
		}
	}

	/* construct linked list of free object slots */
	lastfree = 0;
	for (oid = 0; oid < xref->len; oid++)
	{
		if (!uselist[oid])
		{
			genlist[oid]++;
			ofslist[lastfree] = oid;
			lastfree = oid;
		}
	}

	savexref();

	closexref();
}

