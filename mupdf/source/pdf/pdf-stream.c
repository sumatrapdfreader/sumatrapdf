#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

/*
 * Check if an object is a stream or not.
 */
int
pdf_obj_num_is_stream(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *entry;

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		return 0;

	fz_try(ctx)
		entry = pdf_cache_object(ctx, doc, num);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		return 0;
	}

	return entry->stm_ofs != 0 || entry->stm_buf;
}

int
pdf_is_stream(fz_context *ctx, pdf_obj *ref)
{
	pdf_document *doc = pdf_get_indirect_document(ctx, ref);
	if (doc)
		return pdf_obj_num_is_stream(ctx, doc, pdf_to_num(ctx, ref));
	return 0;
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

	filters = pdf_dict_geta(ctx, stm, PDF_NAME(Filter), PDF_NAME(F));
	if (filters)
	{
		if (pdf_name_eq(ctx, filters, PDF_NAME(Crypt)))
			return 1;
		if (pdf_is_array(ctx, filters))
		{
			int n = pdf_array_len(ctx, filters);
			for (i = 0; i < n; i++)
			{
				obj = pdf_array_get(ctx, filters, i);
				if (pdf_name_eq(ctx, obj, PDF_NAME(Crypt)))
					return 1;
			}
		}
	}
	return 0;
}

static fz_jbig2_globals *
pdf_load_jbig2_globals(fz_context *ctx, pdf_obj *dict)
{
	fz_jbig2_globals *globals;
	fz_buffer *buf = NULL;

	fz_var(buf);

	if ((globals = pdf_find_item(ctx, fz_drop_jbig2_globals_imp, dict)) != NULL)
		return globals;

	if (pdf_mark_obj(ctx, dict))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cyclic reference when loading JBIG2 globals");

	fz_try(ctx)
	{
		buf = pdf_load_stream(ctx, dict);
		globals = fz_load_jbig2_globals(ctx, buf);
		pdf_store_item(ctx, dict, globals, fz_buffer_storage(ctx, buf, NULL));
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		pdf_unmark_obj(ctx, dict);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return globals;
}

static void
build_compression_params(fz_context *ctx, pdf_obj *f, pdf_obj *p, fz_compression_params *params)
{
	int predictor = pdf_dict_get_int(ctx, p, PDF_NAME(Predictor));
	pdf_obj *columns_obj = pdf_dict_get(ctx, p, PDF_NAME(Columns));
	int columns = pdf_to_int(ctx, columns_obj);
	int colors = pdf_dict_get_int(ctx, p, PDF_NAME(Colors));
	int bpc = pdf_dict_get_int(ctx, p, PDF_NAME(BitsPerComponent));

	params->type = FZ_IMAGE_RAW;

	if (pdf_name_eq(ctx, f, PDF_NAME(CCITTFaxDecode)) || pdf_name_eq(ctx, f, PDF_NAME(CCF)))
	{
		pdf_obj *k = pdf_dict_get(ctx, p, PDF_NAME(K));
		pdf_obj *eol = pdf_dict_get(ctx, p, PDF_NAME(EndOfLine));
		pdf_obj *eba = pdf_dict_get(ctx, p, PDF_NAME(EncodedByteAlign));
		pdf_obj *rows = pdf_dict_get(ctx, p, PDF_NAME(Rows));
		pdf_obj *eob = pdf_dict_get(ctx, p, PDF_NAME(EndOfBlock));
		pdf_obj *bi1 = pdf_dict_get(ctx, p, PDF_NAME(BlackIs1));

		params->type = FZ_IMAGE_FAX;
		params->u.fax.k = (k ? pdf_to_int(ctx, k) : 0);
		params->u.fax.end_of_line = (eol ? pdf_to_bool(ctx, eol) : 0);
		params->u.fax.encoded_byte_align = (eba ? pdf_to_bool(ctx, eba) : 0);
		params->u.fax.columns = (columns_obj ? columns : 1728);
		params->u.fax.rows = (rows ? pdf_to_int(ctx, rows) : 0);
		params->u.fax.end_of_block = (eob ? pdf_to_bool(ctx, eob) : 1);
		params->u.fax.black_is_1 = (bi1 ? pdf_to_bool(ctx, bi1) : 0);
	}
	else if (pdf_name_eq(ctx, f, PDF_NAME(DCTDecode)) || pdf_name_eq(ctx, f, PDF_NAME(DCT)))
	{
		pdf_obj *ct = pdf_dict_get(ctx, p, PDF_NAME(ColorTransform));

		params->type = FZ_IMAGE_JPEG;
		params->u.jpeg.color_transform = (ct ? pdf_to_int(ctx, ct) : -1);
	}
	else if (pdf_name_eq(ctx, f, PDF_NAME(RunLengthDecode)) || pdf_name_eq(ctx, f, PDF_NAME(RL)))
	{
		params->type = FZ_IMAGE_RLD;
	}
	else if (pdf_name_eq(ctx, f, PDF_NAME(FlateDecode)) || pdf_name_eq(ctx, f, PDF_NAME(Fl)))
	{
		params->type = FZ_IMAGE_FLATE;
		params->u.flate.predictor = predictor;
		params->u.flate.columns = columns;
		params->u.flate.colors = colors;
		params->u.flate.bpc = bpc;
	}
	else if (pdf_name_eq(ctx, f, PDF_NAME(LZWDecode)) || pdf_name_eq(ctx, f, PDF_NAME(LZW)))
	{
		pdf_obj *ec = pdf_dict_get(ctx, p, PDF_NAME(EarlyChange));

		params->type = FZ_IMAGE_LZW;
		params->u.lzw.predictor = predictor;
		params->u.lzw.columns = columns;
		params->u.lzw.colors = colors;
		params->u.lzw.bpc = bpc;
		params->u.lzw.early_change = (ec ? pdf_to_int(ctx, ec) : 1);
	}
	else if (pdf_name_eq(ctx, f, PDF_NAME(JBIG2Decode)))
	{
		pdf_obj *g = pdf_dict_get(ctx, p, PDF_NAME(JBIG2Globals));

		params->type = FZ_IMAGE_JBIG2;
		params->u.jbig2.globals = NULL;
		if (g)
		{
			if (!pdf_is_stream(ctx, g))
				fz_warn(ctx, "jbig2 globals is not a stream, skipping globals");
			else
				params->u.jbig2.globals = pdf_load_jbig2_globals(ctx, g);
		}
	}
}

/*
 * Create a filter given a name and param dictionary.
 */
static fz_stream *
build_filter(fz_context *ctx, fz_stream *chain, pdf_document *doc, pdf_obj *f, pdf_obj *p, int num, int gen, fz_compression_params *params)
{
	fz_compression_params local_params;

	local_params.u.jbig2.globals = NULL;
	if (params == NULL)
		params = &local_params;

	build_compression_params(ctx, f, p, params);

	/* If we were using params we were passed in, and we successfully
	 * recognised the image type, we can use the existing filter and
	 * shortstop here. */
	if (params != &local_params && params->type != FZ_IMAGE_RAW)
		return fz_keep_stream(ctx, chain); /* nothing to do */

	else if (params->type == FZ_IMAGE_JBIG2)
	{
		fz_stream *stm;
		fz_try(ctx)
			stm = fz_open_image_decomp_stream(ctx, chain, params, NULL);
		fz_always(ctx)
			fz_drop_jbig2_globals(ctx, local_params.u.jbig2.globals);
		fz_catch(ctx)
			fz_rethrow(ctx);
		return stm;
	}

	else if (params->type != FZ_IMAGE_RAW)
		return fz_open_image_decomp_stream(ctx, chain, params, NULL);

	else if (pdf_name_eq(ctx, f, PDF_NAME(ASCIIHexDecode)) || pdf_name_eq(ctx, f, PDF_NAME(AHx)))
		return fz_open_ahxd(ctx, chain);

	else if (pdf_name_eq(ctx, f, PDF_NAME(ASCII85Decode)) || pdf_name_eq(ctx, f, PDF_NAME(A85)))
		return fz_open_a85d(ctx, chain);

	else if (pdf_name_eq(ctx, f, PDF_NAME(JPXDecode)))
		return fz_keep_stream(ctx, chain); /* JPX decoding is special cased in the image loading code */

	else if (pdf_name_eq(ctx, f, PDF_NAME(Crypt)))
	{
		if (!doc->crypt)
			fz_warn(ctx, "crypt filter in unencrypted document");
		else
		{
			pdf_obj *name = pdf_dict_get(ctx, p, PDF_NAME(Name));
			if (pdf_is_name(ctx, name))
				return pdf_open_crypt_with_filter(ctx, chain, doc->crypt, name, num, gen);
		}
	}

	else
		fz_warn(ctx, "unknown filter name (%s)", pdf_to_name(ctx, f));

	return fz_keep_stream(ctx, chain);
}

/* Build filter, and assume ownership of chain */
static fz_stream *
build_filter_drop(fz_context *ctx, fz_stream *tail, pdf_document *doc, pdf_obj *f, pdf_obj *p, int num, int gen, fz_compression_params *params)
{
	fz_stream *head;
	fz_try(ctx)
		head = build_filter(ctx, tail, doc, f, p, num, gen, params);
	fz_always(ctx)
		fz_drop_stream(ctx, tail);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return head;
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If chain is given, start filter chain with it.
 * Assume ownership of chain.
 */
static fz_stream *
build_filter_chain_drop(fz_context *ctx, fz_stream *chain, pdf_document *doc, pdf_obj *fs, pdf_obj *ps, int num, int gen, fz_compression_params *params)
{
	fz_var(chain);
	fz_try(ctx)
	{
		int i, n = pdf_array_len(ctx, fs);
		for (i = 0; i < n; i++)
		{
			pdf_obj *f = pdf_array_get(ctx, fs, i);
			pdf_obj *p = pdf_array_get(ctx, ps, i);
			chain = build_filter_drop(ctx, chain, doc, f, p, num, gen, (i == n-1 ? params : NULL));
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	return chain;
}

static fz_stream *
build_filter_chain(fz_context *ctx, fz_stream *chain, pdf_document *doc, pdf_obj *fs, pdf_obj *ps, int num, int gen, fz_compression_params *params)
{
	return build_filter_chain_drop(ctx, fz_keep_stream(ctx, chain), doc, fs, ps, num, gen, params);
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
pdf_open_raw_filter(fz_context *ctx, fz_stream *file_stm, pdf_document *doc, pdf_obj *stmobj, int num, int *orig_num, int *orig_gen, int64_t offset)
{
	pdf_xref_entry *x = NULL;
	fz_stream *null_stm, *crypt_stm;
	int hascrypt;
	int len;

	if (num > 0 && num < pdf_xref_len(ctx, doc))
	{
		x = pdf_get_xref_entry(ctx, doc, num);
		*orig_num = x->num;
		*orig_gen = x->gen;
		if (x->stm_buf)
			return fz_open_buffer(ctx, x->stm_buf);
	}
	else
	{
		/* We only end up here when called from pdf_open_stream_with_offset to parse new format XRef sections. */
		/* New style XRef sections must have generation number 0. */
		*orig_num = num;
		*orig_gen = 0;
	}

	hascrypt = pdf_stream_has_crypt(ctx, stmobj);
	len = pdf_dict_get_int(ctx, stmobj, PDF_NAME(Length));
	null_stm = fz_open_endstream_filter(ctx, file_stm, len, offset);
	if (doc->crypt && !hascrypt)
	{
		fz_try(ctx)
			crypt_stm = pdf_open_crypt(ctx, null_stm, doc->crypt, *orig_num, *orig_gen);
		fz_always(ctx)
			fz_drop_stream(ctx, null_stm);
		fz_catch(ctx)
			fz_rethrow(ctx);
		return crypt_stm;
	}
	return null_stm;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_stream *
pdf_open_filter(fz_context *ctx, pdf_document *doc, fz_stream *file_stm, pdf_obj *stmobj, int num, int64_t offset, fz_compression_params *imparams)
{
	pdf_obj *filters = pdf_dict_geta(ctx, stmobj, PDF_NAME(Filter), PDF_NAME(F));
	pdf_obj *params = pdf_dict_geta(ctx, stmobj, PDF_NAME(DecodeParms), PDF_NAME(DP));
	int orig_num, orig_gen;
	fz_stream *rstm, *fstm;

	rstm = pdf_open_raw_filter(ctx, file_stm, doc, stmobj, num, &orig_num, &orig_gen, offset);
	fz_try(ctx)
	{
		if (pdf_is_name(ctx, filters))
			fstm = build_filter(ctx, rstm, doc, filters, params, orig_num, orig_gen, imparams);
		else if (pdf_array_len(ctx, filters) > 0)
			fstm = build_filter_chain(ctx, rstm, doc, filters, params, orig_num, orig_gen, imparams);
		else
			fstm = fz_keep_stream(ctx, rstm);
	}
	fz_always(ctx)
		fz_drop_stream(ctx, rstm);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fstm;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_stream *
pdf_open_inline_stream(fz_context *ctx, pdf_document *doc, pdf_obj *stmobj, int length, fz_stream *file_stm, fz_compression_params *imparams)
{
	pdf_obj *filters = pdf_dict_geta(ctx, stmobj, PDF_NAME(Filter), PDF_NAME(F));
	pdf_obj *params = pdf_dict_geta(ctx, stmobj, PDF_NAME(DecodeParms), PDF_NAME(DP));

	if (pdf_is_name(ctx, filters))
		return build_filter(ctx, file_stm, doc, filters, params, 0, 0, imparams);
	else if (pdf_array_len(ctx, filters) > 0)
		return build_filter_chain(ctx, file_stm, doc, filters, params, 0, 0, imparams);

	if (imparams)
		imparams->type = FZ_IMAGE_RAW;
	return fz_open_null_filter(ctx, file_stm, length, fz_tell(ctx, file_stm));
}

void
pdf_load_compressed_inline_image(fz_context *ctx, pdf_document *doc, pdf_obj *dict, int length, fz_stream *file_stm, int indexed, fz_compressed_image *image)
{
	fz_stream *istm = NULL, *leech = NULL, *decomp = NULL;
	fz_pixmap *pixmap = NULL;
	fz_compressed_buffer *bc;
	int dummy_l2factor = 0;

	fz_var(istm);
	fz_var(leech);
	fz_var(decomp);
	fz_var(pixmap);

	bc = fz_malloc_struct(ctx, fz_compressed_buffer);
	fz_try(ctx)
	{
		bc->buffer = fz_new_buffer(ctx, 1024);
		istm = pdf_open_inline_stream(ctx, doc, dict, length, file_stm, &bc->params);
		leech = fz_open_leecher(ctx, istm, bc->buffer);
		decomp = fz_open_image_decomp_stream(ctx, leech, &bc->params, &dummy_l2factor);
		pixmap = fz_decomp_image_from_stream(ctx, decomp, image, NULL, indexed, 0);
		fz_set_compressed_image_tile(ctx, image, pixmap);
		fz_set_compressed_image_buffer(ctx, image, bc);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, istm);
		fz_drop_stream(ctx, leech);
		fz_drop_stream(ctx, decomp);
		fz_drop_pixmap(ctx, pixmap);
	}
	fz_catch(ctx)
	{
		fz_drop_compressed_buffer(ctx, bc);
		fz_rethrow(ctx);
	}
}

/*
 * Open a stream for reading the raw (compressed but decrypted) data.
 */
fz_stream *
pdf_open_raw_stream_number(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *x;
	int orig_num, orig_gen;

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		fz_throw(ctx, FZ_ERROR_GENERIC, "object id out of range (%d 0 R)", num);

	x = pdf_cache_object(ctx, doc, num);
	if (x->stm_ofs == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");

	return pdf_open_raw_filter(ctx, doc->file, doc, x->obj, num, &orig_num, &orig_gen, x->stm_ofs);
}

static fz_stream *
pdf_open_image_stream(fz_context *ctx, pdf_document *doc, int num, fz_compression_params *params)
{
	pdf_xref_entry *x;

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		fz_throw(ctx, FZ_ERROR_GENERIC, "object id out of range (%d 0 R)", num);

	x = pdf_cache_object(ctx, doc, num);
	if (x->stm_ofs == 0 && x->stm_buf == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");

	return pdf_open_filter(ctx, doc, doc->file, x->obj, num, x->stm_ofs, params);
}

/*
 * Open a stream for reading uncompressed data.
 * Put the opened file in doc->stream.
 * Using doc->file while a stream is open is a Bad idea.
 */
fz_stream *
pdf_open_stream_number(fz_context *ctx, pdf_document *doc, int num)
{
	return pdf_open_image_stream(ctx, doc, num, NULL);
}

fz_stream *
pdf_open_stream_with_offset(fz_context *ctx, pdf_document *doc, int num, pdf_obj *dict, int64_t stm_ofs)
{
	if (stm_ofs == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");
	return pdf_open_filter(ctx, doc, doc->file, dict, num, stm_ofs, NULL);
}

/*
 * Load raw (compressed but decrypted) contents of a stream into buf.
 */
fz_buffer *
pdf_load_raw_stream_number(fz_context *ctx, pdf_document *doc, int num)
{
	fz_stream *stm;
	pdf_obj *dict;
	int len;
	fz_buffer *buf = NULL;
	pdf_xref_entry *x;

	if (num > 0 && num < pdf_xref_len(ctx, doc))
	{
		x = pdf_get_xref_entry(ctx, doc, num);
		if (x->stm_buf)
			return fz_keep_buffer(ctx, x->stm_buf);
	}

	dict = pdf_load_object(ctx, doc, num);

	fz_try(ctx)
		len = pdf_dict_get_int(ctx, dict, PDF_NAME(Length));
	fz_always(ctx)
		pdf_drop_obj(ctx, dict);
	fz_catch(ctx)
		fz_rethrow(ctx);

	stm = pdf_open_raw_stream_number(ctx, doc, num);

	fz_try(ctx)
		buf = fz_read_all(ctx, stm, len);
	fz_always(ctx)
		fz_drop_stream(ctx, stm);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return buf;
}

static int
pdf_guess_filter_length(int len, const char *filter)
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

/* Check if an entry has a cached stream and return whether it is directly
 * reusable. A buffer is directly reusable only if the stream is
 * uncompressed, or if it is compressed purely a compression method we can
 * return details of in fz_compression_params.
 *
 * If the stream is reusable return 1, and set params as required, otherwise
 * return 0. */
static int
can_reuse_buffer(fz_context *ctx, pdf_xref_entry *entry, fz_compression_params *params)
{
	pdf_obj *f;
	pdf_obj *p;

	if (!entry || !entry->obj || !entry->stm_buf)
		return 0;

	if (params)
		params->type = FZ_IMAGE_RAW;

	f = pdf_dict_geta(ctx, entry->obj, PDF_NAME(Filter), PDF_NAME(F));
	/* If there are no filters, it's uncompressed, and we can use it */
	if (!f)
		return 1;

	p = pdf_dict_geta(ctx, entry->obj, PDF_NAME(DecodeParms), PDF_NAME(DP));
	if (pdf_is_array(ctx, f))
	{
		int len = pdf_array_len(ctx, f);

		/* Empty array of filters. Its uncompressed. We can cope. */
		if (len == 0)
			return 1;
		/* 1 filter is the most we can hope to cope with - if more,*/
		if (len != 1)
			return 0;
		p = pdf_array_get(ctx, p, 0);
	}
	if (pdf_is_null(ctx, f))
		return 1; /* Null filter is uncompressed */
	if (!pdf_is_name(ctx, f))
		return 0;

	/* There are filters, so unless we have the option of shortstopping,
	 * we can't use the existing buffer. */
	if (!params)
		return 0;

	build_compression_params(ctx, f, p, params);

	return (params->type == FZ_IMAGE_RAW) ? 0 : 1;
}

static fz_buffer *
pdf_load_image_stream(fz_context *ctx, pdf_document *doc, int num, fz_compression_params *params, int *truncated)
{
	fz_stream *stm = NULL;
	pdf_obj *dict, *obj;
	int i, len, n;
	fz_buffer *buf;

	fz_var(buf);

	if (num > 0 && num < pdf_xref_len(ctx, doc))
	{
		pdf_xref_entry *entry = pdf_get_xref_entry(ctx, doc, num);
		/* Return ref to existing buffer, but only if uncompressed,
		 * or shortstoppable */
		if (can_reuse_buffer(ctx, entry, params))
			return fz_keep_buffer(ctx, entry->stm_buf);
	}

	dict = pdf_load_object(ctx, doc, num);
	fz_try(ctx)
	{
		len = pdf_dict_get_int(ctx, dict, PDF_NAME(Length));
		obj = pdf_dict_get(ctx, dict, PDF_NAME(Filter));
		len = pdf_guess_filter_length(len, pdf_to_name(ctx, obj));
		n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; i++)
			len = pdf_guess_filter_length(len, pdf_to_name(ctx, pdf_array_get(ctx, obj, i)));
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, dict);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	stm = pdf_open_image_stream(ctx, doc, num, params);

	fz_try(ctx)
	{
		if (truncated)
			buf = fz_read_best(ctx, stm, len, truncated);
		else
			buf = fz_read_all(ctx, stm, len);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return buf;
}

/*
 * Load uncompressed contents of a stream into buf.
 */
fz_buffer *
pdf_load_stream_number(fz_context *ctx, pdf_document *doc, int num)
{
	return pdf_load_image_stream(ctx, doc, num, NULL, NULL);
}

fz_compressed_buffer *
pdf_load_compressed_stream(fz_context *ctx, pdf_document *doc, int num)
{
	fz_compressed_buffer *bc = fz_malloc_struct(ctx, fz_compressed_buffer);

	fz_try(ctx)
	{
		bc->buffer = pdf_load_image_stream(ctx, doc, num, &bc->params, NULL);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, bc);
		fz_rethrow(ctx);
	}
	return bc;
}

static fz_stream *
pdf_open_object_array(fz_context *ctx, pdf_document *doc, pdf_obj *list)
{
	fz_stream *stm;
	int i, n;

	n = pdf_array_len(ctx, list);
	stm = fz_open_concat(ctx, n, 1);

	for (i = 0; i < n; i++)
	{
		pdf_obj *obj = pdf_array_get(ctx, list, i);
		fz_try(ctx)
			fz_concat_push_drop(ctx, stm, pdf_open_stream(ctx, obj));
		fz_catch(ctx)
		{
			if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
			{
				fz_drop_stream(ctx, stm);
				fz_rethrow(ctx);
			}
			fz_warn(ctx, "cannot load content stream part %d/%d", i + 1, n);
		}
	}

	return stm;
}

fz_stream *
pdf_open_contents_stream(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	int num;

	if (pdf_is_array(ctx, obj))
		return pdf_open_object_array(ctx, doc, obj);

	num = pdf_to_num(ctx, obj);
	if (pdf_is_stream(ctx, obj))
		return pdf_open_image_stream(ctx, doc, num, NULL);

	fz_warn(ctx, "content stream is not a stream (%d 0 R)", num);
	return fz_open_memory(ctx, (unsigned char *)"", 0);
}

fz_buffer *pdf_load_raw_stream(fz_context *ctx, pdf_obj *ref)
{
	if (pdf_is_stream(ctx, ref))
		return pdf_load_raw_stream_number(ctx, pdf_get_indirect_document(ctx, ref), pdf_to_num(ctx, ref));
	fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");
}

fz_buffer *pdf_load_stream(fz_context *ctx, pdf_obj *ref)
{
	if (pdf_is_stream(ctx, ref))
		return pdf_load_stream_number(ctx, pdf_get_indirect_document(ctx, ref), pdf_to_num(ctx, ref));
	fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");
}

fz_stream *pdf_open_raw_stream(fz_context *ctx, pdf_obj *ref)
{
	if (pdf_is_stream(ctx, ref))
		return pdf_open_raw_stream_number(ctx, pdf_get_indirect_document(ctx, ref), pdf_to_num(ctx, ref));
	fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");
}

fz_stream *pdf_open_stream(fz_context *ctx, pdf_obj *ref)
{
	if (pdf_is_stream(ctx, ref))
		return pdf_open_stream_number(ctx, pdf_get_indirect_document(ctx, ref), pdf_to_num(ctx, ref));
	fz_throw(ctx, FZ_ERROR_GENERIC, "object is not a stream");
}
