#include "fitz.h"
#include "mupdf-internal.h"

typedef struct pdf_write_options_s pdf_write_options;

struct pdf_write_options_s
{
	FILE *out;
	int doascii;
	int doexpand;
	int dogarbage;
	char *uselist;
	int *ofslist;
	int *genlist;
	int *renumbermap;
	int *revrenumbermap;
	int *revgenlist;
};

/*
 * Garbage collect objects not reachable from the trailer.
 */

static pdf_obj *sweepref(pdf_document *xref, pdf_write_options *opts, pdf_obj *obj)
{
	int num = pdf_to_num(obj);
	int gen = pdf_to_gen(obj);
	fz_context *ctx = xref->ctx;

	if (num < 0 || num >= xref->len)
		return NULL;
	if (opts->uselist[num])
		return NULL;

	opts->uselist[num] = 1;

	/* Bake in /Length in stream objects */
	fz_try(ctx)
	{
		if (pdf_is_stream(xref, num, gen))
		{
			pdf_obj *len = pdf_dict_gets(obj, "Length");
			if (pdf_is_indirect(len))
			{
				opts->uselist[pdf_to_num(len)] = 0;
				len = pdf_resolve_indirect(len);
				pdf_dict_puts(obj, "Length", len);
			}
		}
	}
	fz_catch(ctx)
	{
		/* Leave broken */
	}

	return pdf_resolve_indirect(obj);
}

static void sweepobj(pdf_document *xref, pdf_write_options *opts, pdf_obj *obj)
{
	int i;

	if (pdf_is_indirect(obj))
		obj = sweepref(xref, opts, obj);

	if (pdf_is_dict(obj))
	{
		int n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(xref, opts, pdf_dict_get_val(obj, i));
	}

	else if (pdf_is_array(obj))
	{
		int n = pdf_array_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(xref, opts, pdf_array_get(obj, i));
	}
}

/*
 * Scan for and remove duplicate objects (slow)
 */

static void removeduplicateobjs(pdf_document *xref, pdf_write_options *opts)
{
	int num, other;
	fz_context *ctx = xref->ctx;

	for (num = 1; num < xref->len; num++)
	{
		/* Only compare an object to objects preceding it */
		for (other = 1; other < num; other++)
		{
			pdf_obj *a, *b;
			int differ, newnum;

			if (num == other || !opts->uselist[num] || !opts->uselist[other])
				continue;

			/*
			 * Comparing stream objects data contents would take too long.
			 *
			 * pdf_is_stream calls pdf_cache_object and ensures
			 * that the xref table has the objects loaded.
			 */
			fz_try(ctx)
			{
				differ = (pdf_is_stream(xref, num, 0) || pdf_is_stream(xref, other, 0));
			}
			fz_catch(ctx)
			{
				/* Assume different */
				differ = 1;
			}
			if (differ)
				continue;

			a = xref->table[num].obj;
			b = xref->table[other].obj;

			a = pdf_resolve_indirect(a);
			b = pdf_resolve_indirect(b);

			if (pdf_objcmp(a, b))
				continue;

			/* Keep the lowest numbered object */
			newnum = MIN(num, other);
			opts->renumbermap[num] = newnum;
			opts->renumbermap[other] = newnum;
			opts->revrenumbermap[newnum] = num; /* Either will do */
			opts->uselist[MAX(num, other)] = 0;

			/* One duplicate was found, do not look for another */
			break;
		}
	}
}

/*
 * Renumber objects sequentially so the xref is more compact
 *
 * This code assumes that any opts->renumbermap[n] <= n for all n.
 */

static void compactxref(pdf_document *xref, pdf_write_options *opts)
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
		/* If it's not used, map it to zero */
		if (!opts->uselist[num])
		{
			opts->renumbermap[num] = 0;
		}
		/* If it's not moved, compact it. */
		else if (opts->renumbermap[num] == num)
		{
			opts->revrenumbermap[newnum] = opts->revrenumbermap[num];
			opts->revgenlist[newnum] = opts->revgenlist[num];
			opts->renumbermap[num] = newnum++;
		}
		/* Otherwise it's used, and moved. We know that it must have
		 * moved down, so the place it's moved to will be in the right
		 * place already. */
		else
		{
			opts->renumbermap[num] = opts->renumbermap[opts->renumbermap[num]];
		}
	}
}

/*
 * Update indirect objects according to renumbering established when
 * removing duplicate objects and compacting the xref.
 */

static void renumberobj(pdf_document *xref, pdf_write_options *opts, pdf_obj *obj)
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
				val = pdf_new_indirect(ctx, opts->renumbermap[pdf_to_num(val)], 0, xref);
				fz_dict_put(obj, key, val);
				pdf_drop_obj(val);
			}
			else
			{
				renumberobj(xref, opts, val);
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
				val = pdf_new_indirect(ctx, opts->renumbermap[pdf_to_num(val)], 0, xref);
				pdf_array_put(obj, i, val);
				pdf_drop_obj(val);
			}
			else
			{
				renumberobj(xref, opts, val);
			}
		}
	}
}

static void renumberobjs(pdf_document *xref, pdf_write_options *opts)
{
	pdf_xref_entry *oldxref;
	int newlen;
	int num;
	fz_context *ctx = xref->ctx;

	/* Apply renumber map to indirect references in all objects in xref */
	renumberobj(xref, opts, xref->trailer);
	for (num = 0; num < xref->len; num++)
	{
		pdf_obj *obj = xref->table[num].obj;

		if (pdf_is_indirect(obj))
		{
			obj = pdf_new_indirect(ctx, opts->renumbermap[pdf_to_num(obj)], 0, xref);
			pdf_update_object(xref, num, 0, obj);
			pdf_drop_obj(obj);
		}
		else
		{
			renumberobj(xref, opts, obj);
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
		if (opts->uselist[num])
		{
			if (newlen < opts->renumbermap[num])
				newlen = opts->renumbermap[num];
			xref->table[opts->renumbermap[num]] = oldxref[num];
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
		opts->uselist[num] = 1;
}

/*
 * Make sure we have loaded objects from object streams.
 */

static void preloadobjstms(pdf_document *xref)
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

static fz_buffer *hexbuf(fz_context *ctx, unsigned char *p, int n)
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

static void addhexfilter(pdf_document *xref, pdf_obj *dict)
{
	pdf_obj *f, *dp, *newf, *newdp;
	pdf_obj *ahx, *nullobj;
	fz_context *ctx = xref->ctx;

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

static void copystream(pdf_document *xref, pdf_write_options *opts, pdf_obj *obj, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;
	fz_context *ctx = xref->ctx;
	int orig_num = opts->revrenumbermap[num];
	int orig_gen = opts->revgenlist[num];

	buf = pdf_load_raw_renumbered_stream(xref, num, gen, orig_num, orig_gen);

	if (opts->doascii && isbinarystream(buf))
	{
		tmp = hexbuf(ctx, buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(xref, obj);

		newlen = pdf_new_int(ctx, buf->len);
		pdf_dict_puts(obj, "Length", newlen);
		pdf_drop_obj(newlen);
	}

	fprintf(opts->out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(opts->out, obj, opts->doexpand == 0);
	fprintf(opts->out, "stream\n");
	fwrite(buf->data, 1, buf->len, opts->out);
	fprintf(opts->out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
}

static void expandstream(pdf_document *xref, pdf_write_options *opts, pdf_obj *obj, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;
	fz_context *ctx = xref->ctx;
	int orig_num = opts->revrenumbermap[num];
	int orig_gen = opts->revgenlist[num];

	buf = pdf_load_renumbered_stream(xref, num, gen, orig_num, orig_gen);

	pdf_dict_dels(obj, "Filter");
	pdf_dict_dels(obj, "DecodeParms");

	if (opts->doascii && isbinarystream(buf))
	{
		tmp = hexbuf(ctx, buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(xref, obj);
	}

	newlen = pdf_new_int(ctx, buf->len);
	pdf_dict_puts(obj, "Length", newlen);
	pdf_drop_obj(newlen);

	fprintf(opts->out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(opts->out, obj, opts->doexpand == 0);
	fprintf(opts->out, "stream\n");
	fwrite(buf->data, 1, buf->len, opts->out);
	fprintf(opts->out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
}

static void writeobject(pdf_document *xref, pdf_write_options *opts, int num, int gen)
{
	pdf_obj *obj;
	pdf_obj *type;
	fz_context *ctx = xref->ctx;

	obj = pdf_load_object(xref, num, gen);

	/* skip ObjStm and XRef objects */
	if (pdf_is_dict(obj))
	{
		type = pdf_dict_gets(obj, "Type");
		if (pdf_is_name(type) && !strcmp(pdf_to_name(type), "ObjStm"))
		{
			opts->uselist[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
		if (pdf_is_name(type) && !strcmp(pdf_to_name(type), "XRef"))
		{
			opts->uselist[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
	}

	if (!pdf_is_stream(xref, num, gen))
	{
		fprintf(opts->out, "%d %d obj\n", num, gen);
		pdf_fprint_obj(opts->out, obj, opts->doexpand == 0);
		fprintf(opts->out, "endobj\n\n");
	}
	else
	{
		int dontexpand = 0;
		if (opts->doexpand != 0 && opts->doexpand != fz_expand_all)
		{
			pdf_obj *o;

			if ((o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "XObject")) &&
				(o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Image")))
				dontexpand = !(opts->doexpand & fz_expand_images);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "Font"))
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "FontDescriptor"))
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length1")) != NULL)
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length2")) != NULL)
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if ((o = pdf_dict_gets(obj, "Length3")) != NULL)
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Type1C"))
				dontexpand = !(opts->doexpand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "CIDFontType0C"))
				dontexpand = !(opts->doexpand & fz_expand_fonts);
		}
		if (opts->doexpand && !dontexpand && !pdf_is_jpx_image(ctx, obj))
			expandstream(xref, opts, obj, num, gen);
		else
			copystream(xref, opts, obj, num, gen);
	}

	pdf_drop_obj(obj);
}

static void writexref(pdf_document *xref, pdf_write_options *opts)
{
	pdf_obj *trailer;
	pdf_obj *obj;
	int startxref;
	int num;
	fz_context *ctx = xref->ctx;

	startxref = ftell(opts->out);

	fprintf(opts->out, "xref\n0 %d\n", xref->len);
	for (num = 0; num < xref->len; num++)
	{
		if (opts->uselist[num])
			fprintf(opts->out, "%010d %05d n \n", opts->ofslist[num], opts->genlist[num]);
		else
			fprintf(opts->out, "%010d %05d f \n", opts->ofslist[num], opts->genlist[num]);
	}
	fprintf(opts->out, "\n");

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

	fprintf(opts->out, "trailer\n");
	pdf_fprint_obj(opts->out, trailer, opts->doexpand == 0);
	fprintf(opts->out, "\n");

	pdf_drop_obj(trailer);

	fprintf(opts->out, "startxref\n%d\n%%%%EOF\n", startxref);
}

void pdf_write(pdf_document *xref, char *filename, fz_write_options *fz_opts)
{
	int lastfree;
	int num;
	pdf_write_options opts = { 0 };
	fz_context *ctx;

	if (!xref || !fz_opts)
		return;

	ctx = xref->ctx;

	opts.out = fopen(filename, "wb");
	if (!opts.out)
		fz_throw(ctx, "cannot open output file '%s'", filename);

	fz_try(ctx)
	{
		opts.doexpand = fz_opts ? fz_opts->doexpand : 0;
		opts.dogarbage = fz_opts ? fz_opts->dogarbage : 0;
		opts.doascii = fz_opts ? fz_opts->doascii: 0;
		opts.uselist = fz_malloc_array(ctx, xref->len + 1, sizeof(char));
		opts.ofslist = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
		opts.genlist = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
		opts.renumbermap = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
		opts.revrenumbermap = fz_malloc_array(ctx, xref->len + 1, sizeof(int));
		opts.revgenlist = fz_malloc_array(ctx, xref->len + 1, sizeof(int));

		fprintf(opts.out, "%%PDF-%d.%d\n", xref->version / 10, xref->version % 10);
		fprintf(opts.out, "%%\316\274\341\277\246\n\n");

		for (num = 0; num < xref->len; num++)
		{
			opts.uselist[num] = 0;
			opts.ofslist[num] = 0;
			opts.renumbermap[num] = num;
			opts.revrenumbermap[num] = num;
			opts.revgenlist[num] = xref->table[num].gen;
		}

		/* Make sure any objects hidden in compressed streams have been loaded */
		preloadobjstms(xref);

		/* Sweep & mark objects from the trailer */
		if (opts.dogarbage >= 1)
			sweepobj(xref, &opts, xref->trailer);

		/* Coalesce and renumber duplicate objects */
		if (opts.dogarbage >= 3)
			removeduplicateobjs(xref, &opts);

		/* Compact xref by renumbering and removing unused objects */
		if (opts.dogarbage >= 2)
			compactxref(xref, &opts);

		/* Make renumbering affect all indirect references and update xref */
		if (opts.dogarbage >= 2)
			renumberobjs(xref, &opts);

		for (num = 0; num < xref->len; num++)
		{
			if (xref->table[num].type == 'f')
				opts.genlist[num] = xref->table[num].gen;
			if (xref->table[num].type == 'n')
				opts.genlist[num] = xref->table[num].gen;
			if (xref->table[num].type == 'o')
				opts.genlist[num] = 0;

			/* SumatraPDF: reset generation numbers (Adobe Reader checks them) */
			if (opts.dogarbage >= 2)
				opts.genlist[num] = num == 0 && xref->table[num].type == 'f' ? 65535 : 0;

			if (opts.dogarbage && !opts.uselist[num])
				continue;

			if (xref->table[num].type == 'n' || xref->table[num].type == 'o')
			{
				opts.uselist[num] = 1;
				opts.ofslist[num] = ftell(opts.out);
				writeobject(xref, &opts, num, opts.genlist[num]);
			}
		}

		/* Construct linked list of free object slots */
		lastfree = 0;
		for (num = 0; num < xref->len; num++)
		{
			if (!opts.uselist[num])
			{
				opts.genlist[num]++;
				opts.ofslist[lastfree] = num;
				lastfree = num;
			}
		}

		writexref(xref, &opts);
	}
	fz_always(ctx)
	{
		fz_free(ctx, opts.uselist);
		fz_free(ctx, opts.ofslist);
		fz_free(ctx, opts.genlist);
		fz_free(ctx, opts.renumbermap);
		fz_free(ctx, opts.revrenumbermap);
		fz_free(ctx, opts.revgenlist);
		fclose(opts.out);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
