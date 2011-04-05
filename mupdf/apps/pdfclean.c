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
static int *renumbermap = NULL;

static int dogarbage = 0;
static int doexpand = 0;
static int doascii = 0;

static pdf_xref *xref = NULL;

void die(fz_error error)
{
	fz_catch(error, "aborting");
	if (xref)
		pdf_free_xref(xref);
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
		"\t-a\tascii hex encode binary streams\n"
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

	if (fz_is_indirect(obj))
		sweepref(obj);

	else if (fz_is_dict(obj))
		for (i = 0; i < fz_dict_len(obj); i++)
			sweepobj(fz_dict_get_val(obj, i));

	else if (fz_is_array(obj))
		for (i = 0; i < fz_array_len(obj); i++)
			sweepobj(fz_array_get(obj, i));
}

static void sweepref(fz_obj *obj)
{
	int num = fz_to_num(obj);
	int gen = fz_to_gen(obj);

	if (num < 0 || num >= xref->len)
		return;
	if (uselist[num])
		return;

	uselist[num] = 1;

	/* Bake in /Length in stream objects */
	if (pdf_is_stream(xref, num, gen))
	{
		fz_obj *len = fz_dict_gets(obj, "Length");
		if (fz_is_indirect(len))
		{
			uselist[fz_to_num(len)] = 0;
			len = fz_resolve_indirect(len);
			fz_dict_puts(obj, "Length", len);
		}
	}

	sweepobj(fz_resolve_indirect(obj));
}

/*
 * Scan for and remove duplicate objects (slow)
 */

static void removeduplicateobjs(void)
{
	int num, other;

	for (num = 1; num < xref->len; num++)
	{
		/* Only compare an object to objects preceeding it */
		for (other = 1; other < num; other++)
		{
			fz_obj *a, *b;

			if (num == other || !uselist[num] || !uselist[other])
				continue;

			/*
			 * Comparing stream objects data contents would take too long.
			 *
			 * pdf_is_stream calls pdf_cache_object and ensures
			 * that the xref table has the objects loaded.
			 */
			if (pdf_is_stream(xref, num, 0) || pdf_is_stream(xref, other, 0))
				continue;

			a = xref->table[num].obj;
			b = xref->table[other].obj;

			a = fz_resolve_indirect(a);
			b = fz_resolve_indirect(b);

			if (fz_objcmp(a, b))
				continue;

			/* Keep the lowest numbered object */
			renumbermap[num] = MIN(num, other);
			renumbermap[other] = MIN(num, other);
			uselist[MAX(num, other)] = 0;

			/* One duplicate was found, do not look for another */
			break;
		}
	}
}

/*
 * Renumber objects sequentially so the xref is more compact
 */

static void compactxref(void)
{
	int num, newnum;

	/*
	 * Update renumbermap in-place, clustering all used
	 * objects together at low object ids. Objects that
	 * already should be renumbered will have their new
	 * object ids be updated to reflect the compaction.
	 */

	newnum = 1;
	for (num = 1; num < xref->len; num++)
	{
		if (uselist[num] && renumbermap[num] == num)
			renumbermap[num] = newnum++;
		else if (renumbermap[num] != num)
			renumbermap[num] = renumbermap[renumbermap[num]];
	}
}

/*
 * Update indirect objects according to renumbering established when
 * removing duplicate objects and compacting the xref.
 */

static void renumberobj(fz_obj *obj)
{
	int i;

	if (fz_is_dict(obj))
	{
		for (i = 0; i < fz_dict_len(obj); i++)
		{
			fz_obj *key = fz_dict_get_key(obj, i);
			fz_obj *val = fz_dict_get_val(obj, i);
			if (fz_is_indirect(val))
			{
				val = fz_new_indirect(renumbermap[fz_to_num(val)], 0, xref);
				fz_dict_put(obj, key, val);
				fz_drop_obj(val);
			}
			else
			{
				renumberobj(val);
			}
		}
	}

	else if (fz_is_array(obj))
	{
		for (i = 0; i < fz_array_len(obj); i++)
		{
			fz_obj *val = fz_array_get(obj, i);
			if (fz_is_indirect(val))
			{
				val = fz_new_indirect(renumbermap[fz_to_num(val)], 0, xref);
				fz_array_put(obj, i, val);
				fz_drop_obj(val);
			}
			else
			{
				renumberobj(val);
			}
		}
	}
}

static void renumberobjs(void)
{
	pdf_xref_entry *oldxref;
	int newlen;
	int num;

	/* Apply renumber map to indirect references in all objects in xref */
	renumberobj(xref->trailer);
	for (num = 0; num < xref->len; num++)
	{
		fz_obj *obj = xref->table[num].obj;

		if (fz_is_indirect(obj))
		{
			obj = fz_new_indirect(renumbermap[fz_to_num(obj)], 0, xref);
			pdf_update_object(xref, num, 0, obj);
			fz_drop_obj(obj);
		}
		else
		{
			renumberobj(obj);
		}
	}

	/* Create new table for the reordered, compacted xref */
	oldxref = xref->table;
	xref->table = fz_calloc(xref->len, sizeof(pdf_xref_entry));
	xref->table[0] = oldxref[0];

	/* Move used objects into the new compacted xref */
	newlen = 0;
	for (num = 1; num < xref->len; num++)
	{
		if (uselist[num])
		{
			if (newlen < renumbermap[num])
				newlen = renumbermap[num];
			xref->table[renumbermap[num]] = oldxref[num];
		}
		else
		{
			if (oldxref[num].obj)
				fz_drop_obj(oldxref[num].obj);
		}
	}

	fz_free(oldxref);

	/* Update the used objects count in compacted xref */
	xref->len = newlen + 1;

	/* Update list of used objects to fit with compacted xref */
	for (num = 1; num < xref->len; num++)
		uselist[num] = 1;
}

/*
 * Recreate page tree to only retain specified pages.
 */

static void retainpages(int argc, char **argv)
{
	fz_error error;
	fz_obj *oldroot, *root, *pages, *kids, *countobj, *parent;

	/* Load the old page tree */
	error = pdf_load_page_tree(xref);
	if (error)
		die(fz_rethrow(error, "cannot load page tree"));

	/* Keep only pages/type entry to avoid references to unretained pages */
	oldroot = fz_dict_gets(xref->trailer, "Root");
	pages = fz_dict_gets(oldroot, "Pages");

	root = fz_new_dict(2);
	fz_dict_puts(root, "Type", fz_dict_gets(oldroot, "Type"));
	fz_dict_puts(root, "Pages", fz_dict_gets(oldroot, "Pages"));

	pdf_update_object(xref, fz_to_num(oldroot), fz_to_gen(oldroot), root);

	fz_drop_obj(root);

	/* Create a new kids array with only the pages we want to keep */
	parent = fz_new_indirect(fz_to_num(pages), fz_to_gen(pages), xref);
	kids = fz_new_array(1);

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
				spage = epage = pdf_count_pages(xref);
			else
				spage = epage = atoi(spec);

			if (dash)
			{
				if (strlen(dash) > 1)
					epage = atoi(dash + 1);
				else
					epage = pdf_count_pages(xref);
			}

			if (spage > epage)
				page = spage, spage = epage, epage = page;

			if (spage < 1)
				spage = 1;
			if (epage > pdf_count_pages(xref))
				epage = pdf_count_pages(xref);

			for (page = spage; page <= epage; page++)
			{
				fz_obj *pageobj = xref->page_objs[page-1];
				fz_obj *pageref = xref->page_refs[page-1];

				fz_dict_puts(pageobj, "Parent", parent);

				/* Store page object in new kids array */
				fz_array_push(kids, pageref);
			}

			spec = fz_strsep(&pagelist, ",");
		}

		fz_optind++;
	}

	fz_drop_obj(parent);

	/* Update page count and kids array */
	countobj = fz_new_int(fz_array_len(kids));
	fz_dict_puts(pages, "Count", countobj);
	fz_drop_obj(countobj);
	fz_dict_puts(pages, "Kids", kids);
	fz_drop_obj(kids);
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
			error = pdf_load_object(&obj, xref, num, 0);
			if (error)
				die(error);
			fz_drop_obj(obj);
		}
	}
}

/*
 * Save streams and objects to the output
 */

static inline int isbinary(int c)
{
	if (c == '\n' || c == '\r' || c == '\t')
		return 0;
	return c < 32 || c > 127;
}

static int isbinarystream(fz_buffer *buf)
{
	int i;
	for (i = 0; i < buf->len; i++)
		if (isbinary(buf->data[i]))
			return 1;
	return 0;
}

static fz_buffer *hexbuf(unsigned char *p, int n)
{
	static const char hex[16] = "0123456789abcdef";
	fz_buffer *buf;
	int x = 0;

	buf = fz_new_buffer(n * 2 + (n / 32) + 2);

	while (n--)
	{
		buf->data[buf->len++] = hex[*p >> 4];
		buf->data[buf->len++] = hex[*p & 15];
		if (++x == 32)
		{
			buf->data[buf->len++] = '\n';
			x = 0;
		}
		p++;
	}

	buf->data[buf->len++] = '>';
	buf->data[buf->len++] = '\n';

	return buf;
}

static void addhexfilter(fz_obj *dict)
{
	fz_obj *f, *dp, *newf, *newdp;
	fz_obj *ahx, *nullobj;

	ahx = fz_new_name("ASCIIHexDecode");
	nullobj = fz_new_null();
	newf = newdp = NULL;

	f = fz_dict_gets(dict, "Filter");
	dp = fz_dict_gets(dict, "DecodeParms");

	if (fz_is_name(f))
	{
		newf = fz_new_array(2);
		fz_array_push(newf, ahx);
		fz_array_push(newf, f);
		f = newf;
		if (fz_is_dict(dp))
		{
			newdp = fz_new_array(2);
			fz_array_push(newdp, nullobj);
			fz_array_push(newdp, dp);
			dp = newdp;
		}
	}
	else if (fz_is_array(f))
	{
		fz_array_insert(f, ahx);
		if (fz_is_array(dp))
			fz_array_insert(dp, nullobj);
	}
	else
		f = ahx;

	fz_dict_puts(dict, "Filter", f);
	if (dp)
		fz_dict_puts(dict, "DecodeParms", dp);

	fz_drop_obj(ahx);
	fz_drop_obj(nullobj);
	if (newf)
		fz_drop_obj(newf);
	if (newdp)
		fz_drop_obj(newdp);
}

static void copystream(fz_obj *obj, int num, int gen)
{
	fz_error error;
	fz_buffer *buf, *tmp;
	fz_obj *newlen;

	error = pdf_load_raw_stream(&buf, xref, num, gen);
	if (error)
		die(error);

	if (doascii && isbinarystream(buf))
	{
		tmp = hexbuf(buf->data, buf->len);
		fz_drop_buffer(buf);
		buf = tmp;

		addhexfilter(obj);

		newlen = fz_new_int(buf->len);
		fz_dict_puts(obj, "Length", newlen);
		fz_drop_obj(newlen);
	}

	fprintf(out, "%d %d obj\n", num, gen);
	fz_fprint_obj(out, obj, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->data, 1, buf->len, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_drop_buffer(buf);
}

static void expandstream(fz_obj *obj, int num, int gen)
{
	fz_error error;
	fz_buffer *buf, *tmp;
	fz_obj *newlen;

	error = pdf_load_stream(&buf, xref, num, gen);
	if (error)
		die(error);

	fz_dict_dels(obj, "Filter");
	fz_dict_dels(obj, "DecodeParms");

	if (doascii && isbinarystream(buf))
	{
		tmp = hexbuf(buf->data, buf->len);
		fz_drop_buffer(buf);
		buf = tmp;

		addhexfilter(obj);
	}

	newlen = fz_new_int(buf->len);
	fz_dict_puts(obj, "Length", newlen);
	fz_drop_obj(newlen);

	fprintf(out, "%d %d obj\n", num, gen);
	fz_fprint_obj(out, obj, !doexpand);
	fprintf(out, "stream\n");
	fwrite(buf->data, 1, buf->len, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_drop_buffer(buf);
}

static void writeobject(int num, int gen)
{
	fz_error error;
	fz_obj *obj;
	fz_obj *type;

	error = pdf_load_object(&obj, xref, num, gen);
	if (error)
		die(error);

	/* skip ObjStm and XRef objects */
	if (fz_is_dict(obj))
	{
		type = fz_dict_gets(obj, "Type");
		if (fz_is_name(type) && !strcmp(fz_to_name(type), "ObjStm"))
		{
			uselist[num] = 0;
			fz_drop_obj(obj);
			return;
		}
		if (fz_is_name(type) && !strcmp(fz_to_name(type), "XRef"))
		{
			uselist[num] = 0;
			fz_drop_obj(obj);
			return;
		}
	}

	if (!pdf_is_stream(xref, num, gen))
	{
		fprintf(out, "%d %d obj\n", num, gen);
		fz_fprint_obj(out, obj, !doexpand);
		fprintf(out, "endobj\n\n");
	}
	else
	{
		if (doexpand && !pdf_is_jpx_image(obj))
			expandstream(obj, num, gen);
		else
			copystream(obj, num, gen);
	}

	fz_drop_obj(obj);
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

	trailer = fz_new_dict(5);

	obj = fz_new_int(xref->len);
	fz_dict_puts(trailer, "Size", obj);
	fz_drop_obj(obj);

	obj = fz_dict_gets(xref->trailer, "Info");
	if (obj)
		fz_dict_puts(trailer, "Info", obj);

	obj = fz_dict_gets(xref->trailer, "Root");
	if (obj)
		fz_dict_puts(trailer, "Root", obj);

	obj = fz_dict_gets(xref->trailer, "ID");
	if (obj)
		fz_dict_puts(trailer, "ID", obj);

	fprintf(out, "trailer\n");
	fz_fprint_obj(out, trailer, !doexpand);
	fprintf(out, "\n");

	fz_drop_obj(trailer);

	fprintf(out, "startxref\n%d\n%%%%EOF\n", startxref);
}

static void writepdf(void)
{
	int lastfree;
	int num;

	for (num = 0; num < xref->len; num++)
	{
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
			uselist[num] = 1;
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

	while ((c = fz_getopt(argc, argv, "adgp:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'g': dogarbage ++; break;
		case 'd': doexpand ++; break;
		case 'a': doascii ++; break;
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

	error = pdf_open_xref(&xref, infile, password);
	if (error)
		die(fz_rethrow(error, "cannot open input file '%s'", infile));

	out = fopen(outfile, "wb");
	if (!out)
		die(fz_throw("cannot open output file '%s'", outfile));

	fprintf(out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
	fprintf(out, "%%\316\274\341\277\246\n\n");

	uselist = fz_calloc(xref->len + 1, sizeof(char));
	ofslist = fz_calloc(xref->len + 1, sizeof(int));
	genlist = fz_calloc(xref->len + 1, sizeof(int));
	renumbermap = fz_calloc(xref->len + 1, sizeof(int));

	for (num = 0; num < xref->len; num++)
	{
		uselist[num] = 0;
		ofslist[num] = 0;
		genlist[num] = 0;
		renumbermap[num] = num;
	}

	/* Make sure any objects hidden in compressed streams have been loaded */
	preloadobjstms();

	/* Only retain the specified subset of the pages */
	if (subset)
		retainpages(argc, argv);

	/* Sweep & mark objects from the trailer */
	if (dogarbage >= 1)
		sweepobj(xref->trailer);

	/* Coalesce and renumber duplicate objects */
	if (dogarbage >= 3)
		removeduplicateobjs();

	/* Compact xref by renumbering and removing unused objects */
	if (dogarbage >= 2)
		compactxref();

	/* Make renumbering affect all indirect references and update xref */
	if (dogarbage >= 2)
		renumberobjs();

	writepdf();

	if (fclose(out))
		die(fz_throw("cannot close output file '%s'", outfile));

	fz_free(uselist);
	fz_free(ofslist);
	fz_free(genlist);
	fz_free(renumbermap);

	pdf_free_xref(xref);

	fz_flush_warnings();

	return 0;
}
