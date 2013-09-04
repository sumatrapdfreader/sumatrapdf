#include "mupdf/pdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_is_stream(pdf_document *doc, int num, int gen)
{
	pdf_xref_entry *entry;

	if (num < 0 || num >= pdf_xref_len(doc))
		return 0;

	pdf_cache_object(doc, num, gen);

	entry = pdf_get_xref_entry(doc, num);
	return entry->stm_ofs != 0 || entry->stm_buf;
}

/*
 * Scan stream dictionary for an explicit /Crypt filter
 */
static int
pdf_stream_has_crypt(fz_context *ctx, pdf_obj *stm)
{
	pdf_obj *filters;
	pdf_obj *obj;
	int i;

	filters = pdf_dict_getsa(stm, "Filter", "F");
	if (filters)
	{
		if (!strcmp(pdf_to_name(filters), "Crypt"))
			return 1;
		if (pdf_is_array(filters))
		{
			int n = pdf_array_len(filters);
			for (i = 0; i < n; i++)
			{
				obj = pdf_array_get(filters, i);
				if (!strcmp(pdf_to_name(obj), "Crypt"))
					return 1;
			}
		}
	}
	return 0;
}

/* SumatraPDF: reuse JBIG2Globals */
static fz_jbig2_globals *
pdf_load_jbig2_globals(pdf_document *doc, pdf_obj *dict)
{
	fz_jbig2_globals *globals;
	fz_buffer *buf = NULL;
	fz_var(buf);

	if ((globals = pdf_find_item(doc->ctx, fz_free_jbig2_globals_imp, dict)))
	{
		return globals;
	}

	fz_try(doc->ctx)
	{
		buf = pdf_load_stream(doc, pdf_to_num(dict), pdf_to_gen(dict));
		globals = fz_load_jbig2_globals(doc->ctx, buf->data, buf->len);
		pdf_store_item(doc->ctx, dict, globals, buf->len);
	}
	fz_always(doc->ctx)
	{
		fz_drop_buffer(doc->ctx, buf);
	}
	fz_catch(doc->ctx)
	{
		fz_rethrow(doc->ctx);
	}

	return globals;
}

/*
 * Create a filter given a name and param dictionary.
 */
static fz_stream *
build_filter(fz_stream *chain, pdf_document *doc, pdf_obj *f, pdf_obj *p, int num, int gen, fz_compression_params *params)
{
	fz_context *ctx = chain->ctx;
	char *s = pdf_to_name(f);

	int predictor = pdf_to_int(pdf_dict_gets(p, "Predictor"));
	pdf_obj *columns_obj = pdf_dict_gets(p, "Columns");
	int columns = pdf_to_int(columns_obj);
	int colors = pdf_to_int(pdf_dict_gets(p, "Colors"));
	int bpc = pdf_to_int(pdf_dict_gets(p, "BitsPerComponent"));

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		return fz_open_ahxd(chain);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		return fz_open_a85d(chain);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
	{
		pdf_obj *k = pdf_dict_gets(p, "K");
		pdf_obj *eol = pdf_dict_gets(p, "EndOfLine");
		pdf_obj *eba = pdf_dict_gets(p, "EncodedByteAlign");
		pdf_obj *rows = pdf_dict_gets(p, "Rows");
		pdf_obj *eob = pdf_dict_gets(p, "EndOfBlock");
		pdf_obj *bi1 = pdf_dict_gets(p, "BlackIs1");
		if (params)
		{
			/* We will shortstop here */
			params->type = FZ_IMAGE_FAX;
			params->u.fax.k = (k ? pdf_to_int(k) : 0);
			params->u.fax.end_of_line = (eol ? pdf_to_bool(eol) : 0);
			params->u.fax.encoded_byte_align = (eba ? pdf_to_bool(eba) : 0);
			params->u.fax.columns = (columns_obj ? columns : 1728);
			params->u.fax.rows = (rows ? pdf_to_int(rows) : 0);
			params->u.fax.end_of_block = (eob ? pdf_to_bool(eob) : 1);
			params->u.fax.black_is_1 = (bi1 ? pdf_to_bool(bi1) : 0);
			return chain;
		}
		return fz_open_faxd(chain,
				k ? pdf_to_int(k) : 0,
				eol ? pdf_to_bool(eol) : 0,
				eba ? pdf_to_bool(eba) : 0,
				columns_obj ? columns : 1728,
				rows ? pdf_to_int(rows) : 0,
				eob ? pdf_to_bool(eob) : 1,
				bi1 ? pdf_to_bool(bi1) : 0);
	}

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
	{
		pdf_obj *ct = pdf_dict_gets(p, "ColorTransform");
		if (params)
		{
			/* We will shortstop here */
			params->type = FZ_IMAGE_JPEG;
			params->u.jpeg.color_transform = (ct ? pdf_to_int(ct) : -1);
			return chain;
		}
		return fz_open_dctd(chain, ct ? pdf_to_int(ct) : -1);
	}

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
	{
		if (params)
		{
			/* We will shortstop here */
			params->type = FZ_IMAGE_RLD;
			return chain;
		}
		return fz_open_rld(chain);
	}
	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		if (params)
		{
			/* We will shortstop here */
			params->type = FZ_IMAGE_FLATE;
			params->u.flate.predictor = predictor;
			params->u.flate.columns = columns;
			params->u.flate.colors = colors;
			params->u.flate.bpc = bpc;
			return chain;
		}
		chain = fz_open_flated(chain);
		if (predictor > 1)
			chain = fz_open_predict(chain, predictor, columns, colors, bpc);
		return chain;
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		pdf_obj *ec = pdf_dict_gets(p, "EarlyChange");
		if (params)
		{
			/* We will shortstop here */
			params->type = FZ_IMAGE_LZW;
			params->u.lzw.predictor = predictor;
			params->u.lzw.columns = columns;
			params->u.lzw.colors = colors;
			params->u.lzw.bpc = bpc;
			params->u.lzw.early_change = (ec ? pdf_to_int(ec) : 1);
			return chain;
		}
		chain = fz_open_lzwd(chain, ec ? pdf_to_int(ec) : 1);
		if (predictor > 1)
			chain = fz_open_predict(chain, predictor, columns, colors, bpc);
		return chain;
	}

	else if (!strcmp(s, "JBIG2Decode"))
	{
		/* SumatraPDF: reuse JBIG2Globals */
		fz_jbig2_globals *globals = NULL;
		pdf_obj *obj = pdf_dict_gets(p, "JBIG2Globals");
		if (pdf_is_indirect(obj))
			globals = pdf_load_jbig2_globals(doc, obj);
		/* fz_open_jbig2d takes possession of globals */
		return fz_open_jbig2d(chain, globals);
	}

	else if (!strcmp(s, "JPXDecode"))
		return chain; /* JPX decoding is special cased in the image loading code */

	else if (!strcmp(s, "Crypt"))
	{
		pdf_obj *name;

		if (!doc->crypt)
		{
			fz_warn(ctx, "crypt filter in unencrypted document");
			return chain;
		}

		name = pdf_dict_gets(p, "Name");
		if (pdf_is_name(name))
			return pdf_open_crypt_with_filter(chain, doc->crypt, pdf_to_name(name), num, gen);

		return chain;
	}

	fz_warn(ctx, "unknown filter name (%s)", s);
	return chain;
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If head is given, start filter chain with it.
 * Assume ownership of head.
 */
static fz_stream *
build_filter_chain(fz_stream *chain, pdf_document *doc, pdf_obj *fs, pdf_obj *ps, int num, int gen, fz_compression_params *params)
{
	pdf_obj *f;
	pdf_obj *p;
	int i, n;

	n = pdf_array_len(fs);
	for (i = 0; i < n; i++)
	{
		f = pdf_array_get(fs, i);
		p = pdf_array_get(ps, i);
		chain = build_filter(chain, doc, f, p, num, gen, (i == n-1 ? params : NULL));
	}

	return chain;
}

/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the stream length (and to
 * allow for other people accessing the file), followed by a decryption
 * filter.
 *
 * orig_num and orig_gen are used purely to seed the encryption.
 */
static fz_stream *
pdf_open_raw_filter(fz_stream *chain, pdf_document *doc, pdf_obj *stmobj, int num, int orig_num, int orig_gen, int offset)
{
	fz_context *ctx = chain->ctx;
	int hascrypt;
	int len;

	if (num > 0 && num < pdf_xref_len(doc))
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, num);
		if (entry->stm_buf)
			return fz_open_buffer(ctx, entry->stm_buf);
	}

	/* don't close chain when we close this filter */
	fz_keep_stream(chain);

	len = pdf_to_int(pdf_dict_gets(stmobj, "Length"));
	chain = fz_open_null(chain, len, offset);

	fz_try(ctx)
	{
		hascrypt = pdf_stream_has_crypt(ctx, stmobj);
		if (doc->crypt && !hascrypt)
			chain = pdf_open_crypt(chain, doc->crypt, orig_num, orig_gen);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return chain;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_stream *
pdf_open_filter(fz_stream *chain, pdf_document *doc, pdf_obj *stmobj, int num, int gen, int offset, fz_compression_params *imparams)
{
	pdf_obj *filters;
	pdf_obj *params;

	filters = pdf_dict_getsa(stmobj, "Filter", "F");
	params = pdf_dict_getsa(stmobj, "DecodeParms", "DP");

	chain = pdf_open_raw_filter(chain, doc, stmobj, num, num, gen, offset);

	fz_try(doc->ctx)
	{
		if (pdf_is_name(filters))
			chain = build_filter(chain, doc, filters, params, num, gen, imparams);
		else if (pdf_array_len(filters) > 0)
			chain = build_filter_chain(chain, doc, filters, params, num, gen, imparams);
	}
	fz_catch(doc->ctx)
	{
		fz_close(chain);
		fz_rethrow(doc->ctx);
	}

	return chain;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_stream *
pdf_open_inline_stream(pdf_document *doc, pdf_obj *stmobj, int length, fz_stream *chain, fz_compression_params *imparams)
{
	pdf_obj *filters;
	pdf_obj *params;

	filters = pdf_dict_getsa(stmobj, "Filter", "F");
	params = pdf_dict_getsa(stmobj, "DecodeParms", "DP");

	/* don't close chain when we close this filter */
	fz_keep_stream(chain);

	if (pdf_is_name(filters))
		return build_filter(chain, doc, filters, params, 0, 0, imparams);
	if (pdf_array_len(filters) > 0)
		return build_filter_chain(chain, doc, filters, params, 0, 0, imparams);

	return fz_open_null(chain, length, fz_tell(chain));
}

/*
 * Open a stream for reading the raw (compressed but decrypted) data.
 */
fz_stream *
pdf_open_raw_stream(pdf_document *doc, int num, int gen)
{
	return pdf_open_raw_renumbered_stream(doc, num, gen, num, gen);
}

fz_stream *
pdf_open_raw_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= pdf_xref_len(doc))
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object id out of range (%d %d R)", num, gen);

	x = pdf_get_xref_entry(doc, num);

	pdf_cache_object(doc, num, gen);

	if (x->stm_ofs == 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object is not a stream");

	return pdf_open_raw_filter(doc->file, doc, x->obj, num, orig_num, orig_gen, x->stm_ofs);
}

static fz_stream *
pdf_open_image_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen, fz_compression_params *params)
{
	pdf_xref_entry *x;

	if (num < 0 || num >= pdf_xref_len(doc))
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object id out of range (%d %d R)", num, gen);

	x = pdf_get_xref_entry(doc, num);

	pdf_cache_object(doc, num, gen);

	if (x->stm_ofs == 0 && x->stm_buf == NULL)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object is not a stream");

	return pdf_open_filter(doc->file, doc, x->obj, orig_num, orig_gen, x->stm_ofs, params);
}

/*
 * Open a stream for reading uncompressed data.
 * Put the opened file in doc->stream.
 * Using doc->file while a stream is open is a Bad idea.
 */
fz_stream *
pdf_open_stream(pdf_document *doc, int num, int gen)
{
	return pdf_open_image_stream(doc, num, gen, num, gen, NULL);
}

fz_stream *
pdf_open_stream_with_offset(pdf_document *doc, int num, int gen, pdf_obj *dict, int stm_ofs)
{
	if (stm_ofs == 0)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "object is not a stream");

	return pdf_open_filter(doc->file, doc, dict, num, gen, stm_ofs, NULL);
}

/*
 * Load raw (compressed but decrypted) contents of a stream into buf.
 */
fz_buffer *
pdf_load_raw_stream(pdf_document *doc, int num, int gen)
{
	return pdf_load_raw_renumbered_stream(doc, num, gen, num, gen);
}

fz_buffer *
pdf_load_raw_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen)
{
	fz_stream *stm;
	pdf_obj *dict;
	int len;
	fz_buffer *buf;

	if (num > 0 && num < pdf_xref_len(doc))
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, num);
		if (entry->stm_buf)
			return fz_keep_buffer(doc->ctx, entry->stm_buf);
	}

	dict = pdf_load_object(doc, num, gen);

	len = pdf_to_int(pdf_dict_gets(dict, "Length"));

	pdf_drop_obj(dict);

	stm = pdf_open_raw_renumbered_stream(doc, num, gen, orig_num, orig_gen);

	buf = fz_read_all(stm, len);

	fz_close(stm);
	return buf;
}

static int
pdf_guess_filter_length(int len, char *filter)
{
	if (!strcmp(filter, "ASCIIHexDecode"))
		return len / 2;
	if (!strcmp(filter, "ASCII85Decode"))
		return len * 4 / 5;
	if (!strcmp(filter, "FlateDecode"))
		return len * 3;
	if (!strcmp(filter, "RunLengthDecode"))
		return len * 3;
	if (!strcmp(filter, "LZWDecode"))
		return len * 2;
	return len;
}

static fz_buffer *
pdf_load_image_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen, fz_compression_params *params, int *truncated)
{
	fz_context *ctx = doc->ctx;
	fz_stream *stm = NULL;
	pdf_obj *dict, *obj;
	int i, len, n;
	fz_buffer *buf;

	fz_var(buf);

	if (num > 0 && num < pdf_xref_len(doc))
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(doc, num);
		if (entry->stm_buf)
			return fz_keep_buffer(doc->ctx, entry->stm_buf);
	}

	dict = pdf_load_object(doc, num, gen);

	len = pdf_to_int(pdf_dict_gets(dict, "Length"));
	obj = pdf_dict_gets(dict, "Filter");
	len = pdf_guess_filter_length(len, pdf_to_name(obj));
	n = pdf_array_len(obj);
	for (i = 0; i < n; i++)
		len = pdf_guess_filter_length(len, pdf_to_name(pdf_array_get(obj, i)));

	pdf_drop_obj(dict);

	stm = pdf_open_image_stream(doc, num, gen, orig_num, orig_gen, params);

	fz_try(ctx)
	{
		if (truncated)
			buf = fz_read_best(stm, len, truncated);
		else
			buf = fz_read_all(stm, len);
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "cannot read raw stream (%d %d R)", num, gen);
	}

	return buf;
}

/*
 * Load uncompressed contents of a stream into buf.
 */
fz_buffer *
pdf_load_stream(pdf_document *doc, int num, int gen)
{
	return pdf_load_image_stream(doc, num, gen, num, gen, NULL, NULL);
}

fz_buffer *
pdf_load_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen, int *truncated)
{
	return pdf_load_image_stream(doc, num, gen, orig_num, orig_gen, NULL, truncated);
}

fz_compressed_buffer *
pdf_load_compressed_stream(pdf_document *doc, int num, int gen)
{
	fz_context *ctx = doc->ctx;
	fz_compressed_buffer *bc = fz_malloc_struct(ctx, fz_compressed_buffer);

	fz_try(ctx)
	{
		bc->buffer = pdf_load_image_stream(doc, num, gen, num, gen, &bc->params, NULL);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, bc);
		fz_rethrow(ctx);
	}
	return bc;
}

static fz_stream *
pdf_open_object_array(pdf_document *doc, pdf_obj *list)
{
	int i, n;
	fz_context *ctx = doc->ctx;
	fz_stream *stm;

	n = pdf_array_len(list);
	/* SumatraPDF: force-close strings at PDF stream boundaries */
	stm = fz_open_concat(ctx, n, 5);

	fz_var(i); /* Workaround Mac compiler bug */
	for (i = 0; i < n; i++)
	{
		pdf_obj *obj = pdf_array_get(list, i);
		fz_try(ctx)
		{
			fz_concat_push(stm, pdf_open_stream(doc, pdf_to_num(obj), pdf_to_gen(obj)));
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "cannot load content stream part %d/%d", i + 1, n);
			continue;
		}
	}

	return stm;
}

fz_stream *
pdf_open_contents_stream(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	int num, gen;

	if (pdf_is_array(obj))
		return pdf_open_object_array(doc, obj);

	num = pdf_to_num(obj);
	gen = pdf_to_gen(obj);
	if (pdf_is_stream(doc, num, gen))
		return pdf_open_image_stream(doc, num, gen, num, gen, NULL);

	fz_warn(ctx, "pdf object stream missing (%d %d R)", num, gen);
	return NULL;
}
