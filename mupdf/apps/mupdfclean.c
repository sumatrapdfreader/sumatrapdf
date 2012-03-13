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
#include "mupdf-internal.h"

static FILE *out = NULL;

enum
{
	expand_images = 1,
	expand_fonts = 2,
	expand_all = -1
};

static char *uselist = NULL;
static int *ofslist = NULL;
static int *genlist = NULL;
static int *renumbermap = NULL;

static int dogarbage = 0;
static int doexpand = 0;
static int doascii = 0;

static pdf_document *xref = NULL;
static fz_context *ctx = NULL;

static void usage(void)
{
	fprintf(stderr,
		"usage: pdfclean [options] input.pdf [output.pdf] [pages]\n"
		"\t-p -\tpassword\n"
		"\t-g\tgarbage collect unused objects\n"
		"\t-gg\tin addition to -g compact xref table\n"
		"\t-ggg\tin addition to -gg merge duplicate objects\n"
		"\t-d\tdecompress all streams\n"
		"\t-i\ttoggle decompression of image streams\n"
		"\t-f\ttoggle decompression of font streams\n"
		"\t-a\tascii hex encode binary streams\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

/*
 * Garbage collect objects not reachable from the trailer.
 */

static void sweepref(pdf_obj *ref);

static void sweepobj(pdf_obj *obj)
{
	int i;

	if (pdf_is_indirect(obj))
		sweepref(obj);

	else if (pdf_is_dict(obj))
	{
		int n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(pdf_dict_get_val(obj, i));
	}

	else if (pdf_is_array(obj))
	{
		int n = pdf_array_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(pdf_array_get(obj, i));
	}
}

static void sweepref(pdf_obj *obj)
{
	int num = pdf_to_num(obj);
	int gen = pdf_to_gen(obj);

	if (num < 0 || num >= xref->len)
		return;
	if (uselist[num])
		return;

	uselist[num] = 1;

	/* Bake in /Length in stream objects */
	fz_try(ctx)
	{
		if (pdf_is_stream(xref, num, gen))
		{
			pdf_obj *len = pdf_dict_gets(obj, "Length");
			if (pdf_is_indirect(len))
			{
				uselist[pdf_to_num(len)] = 0;
				len = pdf_resolve_indirect(len);
				pdf_dict_puts(obj, "Length", len);
			}
		}
	}
	fz_catch(ctx)
	{
		/* Leave broken */
	}

	sweepobj(pdf_resolve_indirect(obj));
}

/*
 * Scan for and remove duplicate objects (slow)
 */

static void removeduplicateobjs(void)
{
	int num, other;

	for (num = 1; num < xref->len; num++)
	{
		/* Only compare an object to objects preceding it */
		for (other = 1; other < num; other++)
		{
			pdf_obj *a, *b;

			if (num == other || !uselist[num] || !uselist[other])
				continue;

			/*
			 * Comparing stream objects data contents would take too long.
			 *
			 * pdf_is_stream calls pdf_cache_object and ensures
			 * that the xref table has the objects loaded.
			 */
			fz_try(ctx)
			{
				if (pdf_is_stream(xref, num, 0) || pdf_is_stream(xref, other, 0))
					continue;
			}
			fz_catch(ctx)
			{
				/* Assume different */
			}

			a = xref->table[num].obj;
			b = xref->table[other].obj;

			a = pdf_resolve_indirect(a);
			b = pdf_resolve_indirect(b);

			if (pdf_objcmp(a, b))
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

static void renumberobj(pdf_obj *obj)
{
	int i;
	fz_context *ctx = xref->ctx;

	if (pdf_is_dict(obj))
	{
		int n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(obj, i);
			pdf_obj *val = pdf_dict_get_val(obj, i);
			if (pdf_is_indirect(val))
			{
				val = pdf_new_indirect(ctx, renumbermap[pdf_to_num(val)], 0, xref);
				fz_dict_put(obj, key, val);
				pdf_drop_obj(val);
			}
			else
			{
				renumberobj(val);
			}
		}
	}

	else if (pdf_is_array(obj))
	{
		int n = pdf_array_len(obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *val = pdf_array_get(obj, i);
			if (pdf_is_indirect(val))
			{
				val = pdf_new_indirect(ctx, renumbermap[pdf_to_num(val)], 0, xref);
				pdf_array_put(obj, i, val);
				pdf_drop_obj(val);
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
		pdf_obj *obj = xref->table[num].obj;

		if (pdf_is_indirect(obj))
		{
			obj = pdf_new_indirect(ctx, renumbermap[pdf_to_num(obj)], 0, xref);
			pdf_update_object(xref, num, 0, obj);
			pdf_drop_obj(obj);
		}
		else
		{
			renumberobj(obj);
		}
	}

	/* Create new table for the reordered, compacted xref */
	oldxref = xref->table;
	xref->table = fz_malloc_array(xref->ctx, xref->len, sizeof(pdf_xref_entry));
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
				pdf_drop_obj(oldxref[num].obj);
		}
	}

	fz_free(xref->ctx, oldxref);

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
	pdf_obj *oldroot, *root, *pages, *kids, *countobj, *parent, *olddests;

	/* Keep only pages/type and (reduced) dest entries to avoid
	 * references to unretained pages */
	oldroot = pdf_dict_gets(xref->trailer, "Root");
	pages = pdf_dict_gets(oldroot, "Pages");
	olddests = pdf_load_name_tree(xref, "Dests");

	root = pdf_new_dict(ctx, 2);
	pdf_dict_puts(root, "Type", pdf_dict_gets(oldroot, "Type"));
	pdf_dict_puts(root, "Pages", pdf_dict_gets(oldroot, "Pages"));

	pdf_update_object(xref, pdf_to_num(oldroot), pdf_to_gen(oldroot), root);

	pdf_drop_obj(root);

	/* Create a new kids array with only the pages we want to keep */
	parent = pdf_new_indirect(ctx, pdf_to_num(pages), pdf_to_gen(pages), xref);
	kids = pdf_new_array(ctx, 1);

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
				pdf_obj *pageobj = xref->page_objs[page-1];
				pdf_obj *pageref = xref->page_refs[page-1];

				pdf_dict_puts(pageobj, "Parent", parent);

				/* Store page object in new kids array */
				pdf_array_push(kids, pageref);
			}

			spec = fz_strsep(&pagelist, ",");
		}

		fz_optind++;
	}

	pdf_drop_obj(parent);

	/* Update page count and kids array */
	countobj = pdf_new_int(ctx, pdf_array_len(kids));
	pdf_dict_puts(pages, "Count", countobj);
	pdf_drop_obj(countobj);
	pdf_dict_puts(pages, "Kids", kids);
	pdf_drop_obj(kids);

	/* Also preserve the (partial) Dests name tree */
	if (olddests)
	{
		int i;
		pdf_obj *names = pdf_new_dict(ctx, 1);
		pdf_obj *dests = pdf_new_dict(ctx, 1);
		pdf_obj *names_list = pdf_new_array(ctx, 32);

		for (i = 0; i < pdf_dict_len(olddests); i++)
		{
			pdf_obj *key = pdf_dict_get_key(olddests, i);
			pdf_obj *val = pdf_dict_get_val(olddests, i);
			pdf_obj *key_str = pdf_new_string(ctx, pdf_to_name(key), strlen(pdf_to_name(key)));
			pdf_obj *dest = pdf_dict_gets(val, "D");

			dest = pdf_array_get(dest ? dest : val, 0);
			if (pdf_array_contains(pdf_dict_gets(pages, "Kids"), dest))
			{
				pdf_array_push(names_list, key_str);
				pdf_array_push(names_list, val);
			}
			pdf_drop_obj(key_str);
		}

		root = pdf_dict_gets(xref->trailer, "Root");
		pdf_dict_puts(dests, "Names", names_list);
		pdf_dict_puts(names, "Dests", dests);
		pdf_dict_puts(root, "Names", names);

		pdf_drop_obj(names);
		pdf_drop_obj(dests);
		pdf_drop_obj(names_list);
		pdf_drop_obj(olddests);
	}
}

/*
 * Make sure we have loaded objects from object streams.
 */

static void preloadobjstms(void)
{
	pdf_obj *obj;
	int num;

	for (num = 0; num < xref->len; num++)
	{
		if (xref->table[num].type == 'o')
		{
			obj = pdf_load_object(xref, num, 0);
			pdf_drop_obj(obj);
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

	buf = fz_new_buffer(ctx, n * 2 + (n / 32) + 2);

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

static void addhexfilter(pdf_obj *dict)
{
	pdf_obj *f, *dp, *newf, *newdp;
	pdf_obj *ahx, *nullobj;

	ahx = fz_new_name(ctx, "ASCIIHexDecode");
	nullobj = pdf_new_null(ctx);
	newf = newdp = NULL;

	f = pdf_dict_gets(dict, "Filter");
	dp = pdf_dict_gets(dict, "DecodeParms");

	if (pdf_is_name(f))
	{
		newf = pdf_new_array(ctx, 2);
		pdf_array_push(newf, ahx);
		pdf_array_push(newf, f);
		f = newf;
		if (pdf_is_dict(dp))
		{
			newdp = pdf_new_array(ctx, 2);
			pdf_array_push(newdp, nullobj);
			pdf_array_push(newdp, dp);
			dp = newdp;
		}
	}
	else if (pdf_is_array(f))
	{
		pdf_array_insert(f, ahx);
		if (pdf_is_array(dp))
			pdf_array_insert(dp, nullobj);
	}
	else
		f = ahx;

	pdf_dict_puts(dict, "Filter", f);
	if (dp)
		pdf_dict_puts(dict, "DecodeParms", dp);

	pdf_drop_obj(ahx);
	pdf_drop_obj(nullobj);
	if (newf)
		pdf_drop_obj(newf);
	if (newdp)
		pdf_drop_obj(newdp);
}

static void copystream(pdf_obj *obj, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;

	buf = pdf_load_raw_stream(xref, num, gen);

	if (doascii && isbinarystream(buf))
	{
		tmp = hexbuf(buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(obj);

		newlen = pdf_new_int(ctx, buf->len);
		pdf_dict_puts(obj, "Length", newlen);
		pdf_drop_obj(newlen);
	}

	fprintf(out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(out, obj, doexpand == 0);
	fprintf(out, "stream\n");
	fwrite(buf->data, 1, buf->len, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
}

static void expandstream(pdf_obj *obj, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;

	buf = pdf_load_stream(xref, num, gen);

	pdf_dict_dels(obj, "Filter");
	pdf_dict_dels(obj, "DecodeParms");

	if (doascii && isbinarystream(buf))
	{
		tmp = hexbuf(buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(obj);
	}

	newlen = pdf_new_int(ctx, buf->len);
	pdf_dict_puts(obj, "Length", newlen);
	pdf_drop_obj(newlen);

	fprintf(out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(out, obj, doexpand == 0);
	fprintf(out, "stream\n");
	fwrite(buf->data, 1, buf->len, out);
	fprintf(out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
}

static void writeobject(int num, int gen)
{
	pdf_obj *obj;
	pdf_obj *type;

	obj = pdf_load_object(xref, num, gen);

	/* skip ObjStm and XRef objects */
	if (pdf_is_dict(obj))
	{
		type = pdf_dict_gets(obj, "Type");
		if (pdf_is_name(type) && !strcmp(pdf_to_name(type), "ObjStm"))
		{
			uselist[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
		if (pdf_is_name(type) && !strcmp(pdf_to_name(type), "XRef"))
		{
			uselist[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
	}

	if (!pdf_is_stream(xref, num, gen))
	{
		fprintf(out, "%d %d obj\n", num, gen);
		pdf_fprint_obj(out, obj, doexpand == 0);
		fprintf(out, "endobj\n\n");
	}
	else
	{
		int dontexpand = 0;
		if (doexpand != 0 && doexpand != expand_all)
		{
			pdf_obj *o;

			if ((o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "XObject")) &&
				(o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Image")))
				dontexpand = !(doexpand & expand_images);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "Font"))
				dontexpand = !(doexpand & expand_fonts);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "FontDescriptor"))
				dontexpand = !(doexpand & expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length1")) != NULL)
				dontexpand = !(doexpand & expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length2")) != NULL)
				dontexpand = !(doexpand & expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length3")) != NULL)
				dontexpand = !(doexpand & expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Type1C"))
				dontexpand = !(doexpand & expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "CIDFontType0C"))
				dontexpand = !(doexpand & expand_fonts);
		}
		if (doexpand && !dontexpand && !pdf_is_jpx_image(ctx, obj))
			expandstream(obj, num, gen);
		else
			copystream(obj, num, gen);
	}

	pdf_drop_obj(obj);
}

static void writexref(void)
{
	pdf_obj *trailer;
	pdf_obj *obj;
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

	trailer = pdf_new_dict(ctx, 5);

	obj = pdf_new_int(ctx, xref->len);
	pdf_dict_puts(trailer, "Size", obj);
	pdf_drop_obj(obj);

	obj = pdf_dict_gets(xref->trailer, "Info");
	if (obj)
		pdf_dict_puts(trailer, "Info", obj);

	obj = pdf_dict_gets(xref->trailer, "Root");
	if (obj)
		pdf_dict_puts(trailer, "Root", obj);

	obj = pdf_dict_gets(xref->trailer, "ID");
	if (obj)
		pdf_dict_puts(trailer, "ID", obj);

	fprintf(out, "trailer\n");
	pdf_fprint_obj(out, trailer, doexpand == 0);
	fprintf(out, "\n");

	pdf_drop_obj(trailer);

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

#ifdef MUPDF_COMBINED_EXE
int pdfclean_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c, num;
	int subset;

	while ((c = fz_getopt(argc, argv, "adfgip:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'g': dogarbage ++; break;
		case 'd': doexpand ^= expand_all; break;
		case 'f': doexpand ^= expand_fonts; break;
		case 'i': doexpand ^= expand_images; break;
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

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	xref = pdf_open_document(ctx, infile);
	if (pdf_needs_password(xref))
		if (!pdf_authenticate_password(xref, password))
			fz_throw(ctx, "cannot authenticate password: %s", infile);

	out = fopen(outfile, "wb");
	if (!out)
		fz_throw(ctx, "cannot open output file '%s'", outfile);

	fprintf(out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
	fprintf(out, "%%\316\274\341\277\246\n\n");

	uselist = fz_malloc_array(ctx, xref->len + 1, sizeof(char));
	ofslist = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
	genlist = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
	renumbermap = fz_malloc_array(ctx, xref->len + 1, sizeof(int));

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
	/* Do not renumber objects if encryption is in use, as the object
	 * numbers are baked into the streams/strings, and we can't currently
	 * cope with moving them. See bug 692627. */
	if (dogarbage >= 2 && !xref->crypt)
		renumberobjs();

	writepdf();

	if (fclose(out))
		fz_throw(ctx, "cannot close output file '%s'", outfile);

	fz_free(xref->ctx, uselist);
	fz_free(xref->ctx, ofslist);
	fz_free(xref->ctx, genlist);
	fz_free(xref->ctx, renumbermap);

	pdf_close_document(xref);
	fz_free_context(ctx);
	return 0;
}
