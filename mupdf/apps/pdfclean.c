/*
 * PDF cleaning tool: general purpose pdf syntax washer.
 *
 * Rewrite PDF with pretty printed objects.
 * Garbage collect unreachable objects.
 * Inflate compressed streams.
 * Create subset documents.
 *
 * TODO: linearize document for fast web view
 */

#include "fitz.h"
#include "mupdf.h"

static FILE *out = NULL;

static char *uselist = NULL;
static int *ofslist = NULL;
static int *genlist = NULL;
static int *newnumlist = NULL;
static pdf_xrefentry *oldxreflist = NULL;

static int dogarbage = 0;
static int doexpand = 0;

static pdf_xref *xref = NULL;

void die(fz_error error)
{
	fz_catch(error, "aborting");
	if (xref)
		pdf_freexref(xref);
	exit(1);
}

static void usage(void)
{
	fprintf(stderr,
		"usage: pdfclean [options] input.pdf [output.pdf] [pages]\n"
		"\t-p -\tpassword\n"
		"\t-g\tgarbage collect unused objects\n"
		"\t-gg\tin addition to -g compact xref table\n"
		"\t-ggg\tin addition to -gg merge duplicate objects\n"
		"\t-d\tdecompress streams\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

/*
 * Garbage collect objects not reachable from the trailer.
 */

static void sweepref(fz_obj *ref);

static void sweepobj(fz_obj *obj)
{
	int i;

	if (fz_isindirect(obj))
		sweepref(obj);

	else if (fz_isdict(obj))
		for (i = 0; i < fz_dictlen(obj); i++)
			sweepobj(fz_dictgetval(obj, i));

	else if (fz_isarray(obj))
		for (i = 0; i < fz_arraylen(obj); i++)
			sweepobj(fz_arrayget(obj, i));
}

static void sweepref(fz_obj *obj)
{
	int num = fz_tonum(obj);

	if (num < 0 || num >= xref->len)
		return;
	if (uselist[num])
		return;

	uselist[num] = 1;

	/* Bake in /Length in stream objects */
	if (xref->table[num].stmofs)
	{
		fz_obj *len = fz_dictgets(obj, "Length");
		if (fz_isindirect(len))
		{
			len = fz_resolveindirect(len);
			fz_dictputs(obj, "Length", len);
		}
	}

	sweepobj(fz_resolveindirect(obj));
}

/*
 * Renumber objects to compact the xref table
 */

static void renumberobj(fz_obj *obj)
{
	int i;

	if (fz_isdict(obj))
	{
		for (i = 0; i < fz_dictlen(obj); i++)
		{
			fz_obj *key = fz_dictgetkey(obj, i);
			fz_obj *val = fz_dictgetval(obj, i);
			if (fz_isindirect(val))
			{
				val = fz_newindirect(newnumlist[fz_tonum(val)], 0, xref);
				fz_dictput(obj, key, val);
				fz_dropobj(val);
			}
			else
			{
				renumberobj(val);
			}
		}
	}

	if (fz_isarray(obj))
	{
		for (i = 0; i < fz_arraylen(obj); i++)
		{
			fz_obj *val = fz_arrayget(obj, i);
			if (fz_isindirect(val))
			{
				val = fz_newindirect(newnumlist[fz_tonum(val)], 0, xref);
				fz_arrayput(obj, i, val);
				fz_dropobj(val);
			}
			else
			{
				renumberobj(val);
			}
		}
	}
}

static void renumberxref(void)
{
	int num, newnum;

	newnumlist = fz_malloc(xref->len * sizeof(int));
	oldxreflist = fz_malloc(xref->len * sizeof(pdf_xrefentry));
	for (num = 0; num < xref->len; num++)
	{
		newnumlist[num] = -1;
		oldxreflist[num] = xref->table[num];
	}

	newnum = 1;
	for (num = 0; num < xref->len; num++)
	{
		if (xref->table[num].type == 'f')
			uselist[num] = 0;
		if (uselist[num])
			newnumlist[num] = newnum++;
	}

	renumberobj(xref->trailer);
	for (num = 0; num < xref->len; num++)
		renumberobj(xref->table[num].obj);

	for (num = 0; num < xref->len; num++)
		uselist[num] = 0;

	for (num = 0; num < xref->len; num++)
	{
		if (newnumlist[num] >= 0)
		{
			xref->table[newnumlist[num]] = oldxreflist[num];
			uselist[newnumlist[num]] = 1;
			xref->table[num].obj = nil;
		}
	}

	for (num = newnum; num < xref->len; num++)
	{
		if (xref->table[num].obj)
		{
			fz_dropobj(xref->table[num].obj);
			xref->table[num].obj = nil;
		}
	}

	fz_free(oldxreflist);
	fz_free(newnumlist);

	xref->len = newnum;
}

/*
 * Scan and remove duplicate objects (slow)
 */

static void removeduplicateobjs(void)
{
	int num, other;

	newnumlist = fz_malloc(xref->len * sizeof(int));
	for (num = 0; num < xref->len; num++)
		newnumlist[num] = num;

	for (num = 1; num < xref->len; num++)
	{
		for (other = 1; other < num; other++)
		{
			fz_obj *a, *b;

			/*
			pdf_isstream calls pdf_cacheobject and ensures
			that the xref table has the objects loaded
			*/
			if (num == other ||
				pdf_isstream(xref, num, 0) ||
				pdf_isstream(xref, other, 0))
				continue;

			a = xref->table[num].obj;
			b = xref->table[other].obj;

			a = fz_resolveindirect(a);
			b = fz_resolveindirect(b);

			if (fz_objcmp(a, b))
				continue;

			newnumlist[num] = MIN(num, other);
			break;
		}
	}

	renumberobj(xref->trailer);
	for (num = 0; num < xref->len; num++)
		renumberobj(xref->table[num].obj);

	fz_free(newnumlist);
}

/*
 * Recreate page tree to only retain specified pages.
 */

static void retainpages(int argc, char **argv)
{
	fz_error error;
	fz_obj *root, *pages, *type, *kids, *countobj;
	int count;

	/* Load the old page tree */
	error = pdf_loadpagetree(xref);
	if (error)
		die(fz_rethrow(error, "cannot load page tree"));

	/* Snatch pages entry from root dict */
	root = fz_dictgets(xref->trailer, "Root");
	pages = fz_keepobj(fz_dictgets(root, "Pages"));
	type = fz_keepobj(fz_dictgets(root, "Type"));

	/* Then empty the root dict */
	while (fz_dictlen(root) > 0)
	{
		fz_obj *key = fz_dictgetkey(root, 0);
		fz_dictdel(root, key);
	}

	/* And only retain pages and type entries */
	fz_dictputs(root, "Pages", pages);
	fz_dictputs(root, "Type", type);
	fz_dropobj(pages);
	fz_dropobj(type);

	/* Create a new kids array with only the pages we want to keep. */
	kids = fz_newarray(1);
	count = 0;

	/* Retain pages specified */
	while (argc - fz_optind)
	{
		int page, spage, epage;
		char *spec, *dash;
		char *pagelist = argv[fz_optind];

		spec = fz_strsep(&pagelist, ",");
		while (spec)
		{
			dash = strchr(spec, '-');

			if (dash == spec)
				spage = epage = 1;
			else
				spage = epage = atoi(spec);

			if (dash)
			{
				if (strlen(dash) > 1)
					epage = atoi(dash + 1);
				else
					epage = pdf_getpagecount(xref);
			}

			if (spage > epage)
				page = spage, spage = epage, epage = page;

			if (spage < 1)
				spage = 1;
			if (epage > pdf_getpagecount(xref))
				epage = pdf_getpagecount(xref);

			for (page = spage; page <= epage; page++)
			{
				fz_obj *pageobj = pdf_getpageobject(xref, page);
				fz_obj *pageref = pdf_getpageref(xref, page);

				/* Update parent reference */
				fz_dictputs(pageobj, "Parent", pages);

				/* Store page object in new kids array */
				fz_arraypush(kids, pageref);
				count++;

				fz_dropobj(pageref);
			}

			spec = fz_strsep(&pagelist, ",");
		}

		fz_optind++;
	}

	/* Update page count and kids array */
	countobj = fz_newint(count);
	fz_dictputs(pages, "Count", countobj);
	fz_dropobj(countobj);
	fz_dictputs(pages, "Kids", kids);
	fz_dropobj(kids);
}

/*
 * Make sure we have loaded objects from object streams.
 */

static void preloadobjstms(void)
{
	fz_error error;
	fz_obj *obj;
	int num;

	for (num = 0; num < xref->len; num++)
	{
		if (xref->table[num].type == 'o')
		{
			error = pdf_loadobject(&obj, xref, num, 0);
			if (error)
				die(error);
			fz_dropobj(obj);
		}
	}
}

/*
 * Save streams and objects to the output
 */

static void copystream(fz_obj *obj, int num, int gen)
{
	fz_error error;
	fz_buffer *buf;

	error = pdf_loadrawstream(&buf, xref, num, gen);
	if (error)
		die(error);

	fprintf(out, "%d %d obj\n", num, gen);
	fz_fprintobj(out, obj, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->rp, 1, buf->wp - buf->rp, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_dropbuffer(buf);
}

static void expandstream(fz_obj *obj, int num, int gen)
{
	fz_error error;
	fz_buffer *buf;
	fz_obj *newdict, *newlen;

	error = pdf_loadstream(&buf, xref, num, gen);
	if (error)
		die(error);

	newdict = fz_copydict(obj);
	fz_dictdels(newdict, "Filter");
	fz_dictdels(newdict, "DecodeParms");

	newlen = fz_newint(buf->wp - buf->rp);
	fz_dictputs(newdict, "Length", newlen);
	fz_dropobj(newlen);

	fprintf(out, "%d %d obj\n", num, gen);
	fz_fprintobj(out, newdict, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->rp, 1, buf->wp - buf->rp, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_dropobj(newdict);

	fz_dropbuffer(buf);
}

static void writeobject(int num, int gen)
{
	fz_error error;
	fz_obj *obj;
	fz_obj *type;

	error = pdf_loadobject(&obj, xref, num, gen);
	if (error)
		die(error);

	/* skip ObjStm and XRef objects */
	if (fz_isdict(obj))
	{
		type = fz_dictgets(obj, "Type");
		if (fz_isname(type) && !strcmp(fz_toname(type), "ObjStm"))
		{
			uselist[num] = 0;
			fz_dropobj(obj);
			return;
		}
		if (fz_isname(type) && !strcmp(fz_toname(type), "XRef"))
		{
			uselist[num] = 0;
			fz_dropobj(obj);
			return;
		}
	}

	if (!xref->table[num].stmofs)
	{
		fprintf(out, "%d %d obj\n", num, gen);
		fz_fprintobj(out, obj, !doexpand);
		fprintf(out, "endobj\n\n");
	}
	else
	{
		if (doexpand)
			expandstream(obj, num, gen);
		else
			copystream(obj, num, gen);
	}

	fz_dropobj(obj);
}

static void writexref(void)
{
	fz_obj *trailer;
	fz_obj *obj;
	int startxref;
	int num;

	startxref = ftell(out);

	fprintf(out, "xref\n0 %d\n", xref->len);
	for (num = 0; num < xref->len; num++)
	{
		if (uselist[num])
			fprintf(out, "%010d %05d n \n", ofslist[num], genlist[num]);
		else
			fprintf(out, "%010d %05d f \n", ofslist[num], genlist[num]);
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

	fz_dropobj(trailer);

	fprintf(out, "startxref\n%d\n%%%%EOF\n", startxref);
}

static void writepdf(void)
{
	int lastfree;
	int num;

	for (num = 0; num < xref->len; num++)
	{
		if (xref->table[num].type == 'f')
			uselist[num] = 0;

		if (xref->table[num].type == 'f')
			genlist[num] = xref->table[num].gen;
		if (xref->table[num].type == 'n')
			genlist[num] = xref->table[num].gen;
		if (xref->table[num].type == 'o')
			genlist[num] = 0;

		if (dogarbage && !uselist[num])
			continue;

		if (xref->table[num].type == 'n' || xref->table[num].type == 'o')
		{
			ofslist[num] = ftell(out);
			writeobject(num, genlist[num]);
		}
	}

	/* Construct linked list of free object slots */
	lastfree = 0;
	for (num = 0; num < xref->len; num++)
	{
		if (!uselist[num])
		{
			genlist[num]++;
			ofslist[lastfree] = num;
			lastfree = num;
		}
	}

	writexref();
}

int main(int argc, char **argv)
{
	fz_error error;
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c, num;
	int subset;

	while ((c = fz_getopt(argc, argv, "gdp:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'g': dogarbage ++; break;
		case 'd': doexpand ++; break;
		default: usage(); break;
		}
	}

	if (argc - fz_optind < 1)
		usage();

	infile = argv[fz_optind++];

	if (argc - fz_optind > 0 &&
		(strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF")))
	{
		outfile = argv[fz_optind++];
	}

	subset = 0;
	if (argc - fz_optind > 0)
		subset = 1;

	error = pdf_openxref(&xref, infile, password);
	if (error)
		die(fz_rethrow(error, "cannot open input file '%s'", infile));

	out = fopen(outfile, "wb");
	if (!out)
		die(fz_throw("cannot open output file '%s'", outfile));

	fprintf(out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
	fprintf(out, "%%\316\274\341\277\246\n\n");

	uselist = fz_malloc(sizeof (char) * (xref->len + 1));
	ofslist = fz_malloc(sizeof (int) * (xref->len + 1));
	genlist = fz_malloc(sizeof (int) * (xref->len + 1));

	for (num = 0; num < xref->len; num++)
	{
		uselist[num] = 0;
		ofslist[num] = 0;
		genlist[num] = 0;
	}

	/* Make sure any objects hidden in compressed streams have been loaded */
	preloadobjstms();

	/* Only retain the specified subset of the pages */
	if (subset)
		retainpages(argc, argv);

	/* Coalesce identical objects */
	if (dogarbage >= 3)
		removeduplicateobjs();

	/* Sweep & mark objects from the trailer */
	sweepobj(xref->trailer);

	/* Renumber objects to shorten xref */
	if (dogarbage >= 2)
		renumberxref();

	writepdf();

	if (fclose(out))
		die(fz_throw("cannot close output file '%s'", outfile));

	fz_free(uselist);
	fz_free(ofslist);
	fz_free(genlist);

	pdf_freexref(xref);

	return 0;
}
