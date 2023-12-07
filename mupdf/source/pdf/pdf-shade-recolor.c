// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <math.h>
#include <float.h>

typedef struct
{
	void *opaque;
	pdf_recolor_vertex *recolor;
	fz_colorspace *dst_cs;
	fz_colorspace *src_cs;
	int funcs;
} recolor_details;

#define FUNSEGS 64 /* size of sampled mesh for function-based shadings */
#define FUNBPS 8 /* Bits per sample in output functions */

static void
fz_recolor_shade_type1(fz_context *ctx, pdf_obj *shade, pdf_function **func, recolor_details *rd)
{
	float x0 = 0;
	float y0 = 0;
	float x1 = 1;
	float y1 = 1;
	float in[FZ_MAX_COLORS] = { 0 };
	float out[(FUNSEGS+1)*(FUNSEGS+1)*FZ_MAX_COLORS];
	float *p;
	float fv[2];
	int xx, yy;
	pdf_obj *obj;
	int n_in = rd->src_cs->n;
	int n_out = rd->dst_cs->n;
	pdf_obj *fun_obj = NULL;
	float range[FZ_MAX_COLORS];
	int i;
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	pdf_obj *ref = NULL;
	fz_buffer *buf = NULL;
	fz_output *output = NULL;

	obj = pdf_dict_get(ctx, shade, PDF_NAME(Domain));
	if (obj)
	{
		x0 = pdf_array_get_real(ctx, obj, 0);
		x1 = pdf_array_get_real(ctx, obj, 1);
		y0 = pdf_array_get_real(ctx, obj, 2);
		y1 = pdf_array_get_real(ctx, obj, 3);
	}

	if (rd->funcs != 1 && rd->funcs != n_in)
	{
		fz_throw(ctx, FZ_ERROR_SYNTAX, "Unexpected function-arity.");
	}

	/* Sample the function, rewriting it. */
	for (i = 0; i < n_out; i++)
	{
		range[2 * i] = FLT_MAX;
		range[2 * i + 1] = -FLT_MAX;
	}
	p = out;
	for (yy = 0; yy <= FUNSEGS; yy++)
	{
		fv[1] = y0 + (y1 - y0) * yy / FUNSEGS;

		for (xx = 0; xx <= FUNSEGS; xx++)
		{
			fv[0] = x0 + (x1 - x0) * xx / FUNSEGS;

			if (rd->funcs == 1)
				pdf_eval_function(ctx, func[0], fv, 2, in, n_in);
			else
			{
				int zz;
				for (zz = 0; zz < n_in; zz++)
					pdf_eval_function(ctx, func[zz], fv, 2, &in[zz], 1);
			}

			rd->recolor(ctx, rd->opaque, rd->dst_cs, p, rd->src_cs, in);

			for (i = 0; i < n_out; i++)
			{
				if (range[2 * i] > p[i])
					range[2 * i] = p[i];
				if (range[2 * i + 1] < p[i])
					range[2 * i + 1] = p[i];
			}
			p += n_out;
		}
	}

	/* Now write the function out again. */
	fun_obj = pdf_new_dict(ctx, doc, 3);
	pdf_dict_put_int(ctx, fun_obj, PDF_NAME(FunctionType), 0);

	/* Domain */
	obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Domain), 4);
	pdf_array_push_real(ctx, obj, x0);
	pdf_array_push_real(ctx, obj, x1);
	pdf_array_push_real(ctx, obj, y0);
	pdf_array_push_real(ctx, obj, y1);

	/* Range */
	obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Range), 4);
	for (i = 0; i < 2*n_out; i++)
		pdf_array_push_real(ctx, obj, range[i]);

	/* Size */
	obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Size), 2);
	pdf_array_push_int(ctx, obj, FUNSEGS+1);
	pdf_array_push_int(ctx, obj, FUNSEGS+1);

	/* BitsPerSample */
	pdf_dict_put_int(ctx, fun_obj, PDF_NAME(BitsPerSample), FUNBPS);

	buf = fz_new_buffer(ctx, 1);
	output = fz_new_output_with_buffer(ctx, buf);

	p = out;
	for (yy = 0; yy <= FUNSEGS; yy++)
	{
		for (xx = 0; xx <= FUNSEGS; xx++)
		{
			for (i = 0; i < n_out; i++)
			{
				float v = p[i];
				float d = range[2 * i + 1] - range[2 * i];
				int iv;

				v -= range[2 * i];
				if (d != 0)
					v = v * ((1<<FUNBPS)-1) / d;
				iv = (int)(v + 0.5f);
				fz_write_bits(ctx, output, iv, FUNBPS);
			}
			p += n_out;
		}
	}
	fz_write_bits_sync(ctx, output);
	fz_close_output(ctx, output);
	fz_drop_output(ctx, output);

	ref = pdf_add_object(ctx, doc, fun_obj);
	pdf_update_stream(ctx, doc, ref, buf, 0);
	fz_drop_buffer(ctx, buf);
	pdf_dict_put(ctx, shade, PDF_NAME(Function), ref);
}

static void
fz_recolor_shade_function(fz_context *ctx, pdf_obj *shade, float samples[256][FZ_MAX_COLORS+1], recolor_details *rd)
{
	int i;
	int n_in = fz_colorspace_n(ctx, rd->src_cs);
	int n_out = fz_colorspace_n(ctx, rd->dst_cs);
	float localp[256*FZ_MAX_COLORS];
	float *q = localp;
	float p[FZ_MAX_COLORS];
	pdf_obj *fun_obj = NULL;
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	pdf_obj *obj;
	float t0 = 0;
	float t1 = 1;
	float range[FZ_MAX_COLORS];
	pdf_obj *ref = NULL;
	fz_buffer *buf = NULL;
	fz_output *output = NULL;
	int t;

	obj = pdf_dict_get(ctx, shade, PDF_NAME(Domain));
	if (obj)
	{
		t0 = pdf_array_get_real(ctx, obj, 0);
		t1 = pdf_array_get_real(ctx, obj, 1);
	}

	for (i = 0; i < n_out; i++)
	{
		range[2 * i] = FLT_MAX;
		range[2 * i + 1] = -FLT_MAX;
	}
	for (t = 0; t < 256; t++)
	{
		for (i = 0; i < n_in; i++)
			p[i] = samples[t][i];

		rd->recolor(ctx, rd->opaque, rd->dst_cs, q, rd->src_cs, p);

		for (i = 0; i < n_out; i++)
		{
			if (range[2 * i] > q[i])
				range[2 * i] = q[i];
			if (range[2 * i + 1] < q[i])
				range[2 * i + 1] = q[i];
		}
		q += n_out;
	}

	fz_var(ref);
	fz_var(output);

	/* Now write the function out again. */
	fun_obj = pdf_new_dict(ctx, doc, 3);
	fz_try(ctx)
	{
		pdf_dict_put_int(ctx, fun_obj, PDF_NAME(FunctionType), 0);

		/* Domain */
		obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Domain), 2);
		pdf_array_push_real(ctx, obj, t0);
		pdf_array_push_real(ctx, obj, t1);

		/* Range */
		obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Range), 2 * n_out);
		for (i = 0; i < 2 * n_out; i++)
			pdf_array_push_real(ctx, obj, range[i]);

		obj = pdf_dict_put_array(ctx, fun_obj, PDF_NAME(Size), 1);
		pdf_array_push_int(ctx, obj, 256);

		pdf_dict_put_int(ctx, fun_obj, PDF_NAME(BitsPerSample), FUNBPS);

		buf = fz_new_buffer(ctx, 1);
		output = fz_new_output_with_buffer(ctx, buf);

		q = localp;
		for (t = 0; t < 256; t++)
		{
			for (i = 0; i < n_out; i++)
			{
				float v = q[i];
				float d = range[2 * i + 1] - range[2 * i];
				int iv;

				v -= range[2 * i];
				if (d != 0)
					v = v * ((1<<FUNBPS)-1) / d;
				iv = (int)(v + 0.5f);
				fz_write_bits(ctx, output, iv, FUNBPS);
			}
			q += n_out;
		}
		fz_write_bits_sync(ctx, output);
		fz_close_output(ctx, output);

		ref = pdf_add_object(ctx, doc, fun_obj);
		pdf_update_stream(ctx, doc, ref, buf, 0);
		pdf_dict_put(ctx, shade, PDF_NAME(Function), ref);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, output);
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, fun_obj);
		pdf_drop_obj(ctx, ref);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static inline float read_sample(fz_context *ctx, fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1 / (powf(2, bits) - 1);
	return min + fz_read_bits(ctx, stream, bits) * (max - min) * bitscale;
}

static inline void write_sample(fz_context *ctx, fz_output *out, int bits, float min, float max, float val)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = (powf(2, bits) - 1);
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
	val -= min;
	if (max != min)
		val /= (max - min);
	/* Now 0 <= val <= 1 */
	fz_write_bits(ctx, out, (int)(val * bitscale), bits);
}

typedef struct
{
	float *p;
	int len;
	int max;
	int pos;
} float_queue;

static void
float_queue_push(fz_context *ctx, float_queue *p, float f)
{
	if (p->len == p->max)
	{
		int new_max = p->max * 2;
		if (new_max == 0)
			new_max = 32;
		p->p = fz_realloc(ctx, p->p, sizeof(float) * new_max);
		p->max = new_max;
	}
	p->p[p->len++] = f;
}

static float
float_queue_pop(fz_context *ctx, float_queue *p)
{
	return p->p[p->pos++];
}

static void
float_queue_drop(fz_context *ctx, float_queue *p)
{
	fz_free(ctx, p->p);
}

static void
read_decode(fz_context *ctx, pdf_obj *shade, int n_in, float *c_min, float *c_max, int n_out, float *d_min, float *d_max)
{
	int i;
	pdf_obj *obj = pdf_dict_get(ctx, shade, PDF_NAME(Decode));

	for (i = 0; i < n_in; i++)
	{
		c_min[i] = pdf_array_get_int(ctx, obj, 2 * i + 4);
		c_max[i] = pdf_array_get_int(ctx, obj, 2 * i + 5);
	}
	for (i = 0; i < n_out; i++)
	{
		d_min[i] = FLT_MAX;
		d_max[i] = -FLT_MAX;
	}
}

static void
rewrite_decode(fz_context *ctx, pdf_obj *shade, int n_out, float *d_min, float *d_max)
{
	int i;
	pdf_obj *obj = pdf_keep_obj(ctx, pdf_dict_get(ctx, shade, PDF_NAME(Decode)));
	pdf_obj *obj2;

	fz_try(ctx)
	{
		obj2 = pdf_dict_put_array(ctx, shade, PDF_NAME(Decode), 4);

		for (i = 0; i < 4; i++)
		{
			pdf_array_push(ctx, obj2, pdf_array_get(ctx, obj, i));
		}
		for (i = 0; i < n_out; i++)
		{
			pdf_array_push_real(ctx, obj2, d_min[i]);
			pdf_array_push_real(ctx, obj2, d_max[i]);
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_recolor_shade_type4(fz_context *ctx, pdf_obj *shade, recolor_details *rd)
{
	fz_stream *stream;
	int i, n_in = rd->src_cs->n;
	int n_out = rd->dst_cs->n;
	int bpflag = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerFlag));
	int bpcoord = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerCoordinate));
	int bpcomp = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerComponent));
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	float c[FZ_MAX_COLORS];
	float d[FZ_MAX_COLORS];
	float c_min[FZ_MAX_COLORS];
	float c_max[FZ_MAX_COLORS];
	float d_min[FZ_MAX_COLORS];
	float d_max[FZ_MAX_COLORS];
	fz_buffer *outbuf = NULL;
	fz_output *out = NULL;
	float_queue fq = { 0 };

	fz_var(outbuf);
	fz_var(out);
	fz_var(stream);

	read_decode(ctx, shade, n_in, c_min, c_max, n_out, d_min, d_max);

	stream = pdf_open_stream(ctx, shade);
	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			/* flag */ (void)fz_read_bits(ctx, stream, bpflag);
			/* x_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
			/* y_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
			for (i = 0; i < n_in; i++)
				c[i] = read_sample(ctx, stream, bpcomp, c_min[i], c_max[i]);

			rd->recolor(ctx, rd->opaque, rd->dst_cs, d, rd->src_cs, c);

			for (i = 0; i < n_out; i++)
			{
				if (d[i] < d_min[i])
					d_min[i] = d[i];
				if (d[i] > d_max[i])
					d_max[i] = d[i];
				float_queue_push(ctx, &fq, d[i]);
			}
		}
		fz_drop_stream(ctx, stream);
		stream = NULL;

		rewrite_decode(ctx, shade, n_out, d_min, d_max);

		stream = pdf_open_stream(ctx, shade);
		outbuf = fz_new_buffer(ctx, 1);
		out = fz_new_output_with_buffer(ctx, outbuf);
		while (!fz_is_eof_bits(ctx, stream))
		{
			unsigned int flag = fz_read_bits(ctx, stream, bpflag);
			unsigned int x_bits = fz_read_bits(ctx, stream, bpcoord);
			unsigned int y_bits = fz_read_bits(ctx, stream, bpcoord);
			for (i = 0; i < n_in; i++)
				(void)fz_read_bits(ctx, stream, bpcomp);

			fz_write_bits(ctx, out, flag, bpflag);
			fz_write_bits(ctx, out, x_bits, bpcoord);
			fz_write_bits(ctx, out, y_bits, bpcoord);

			for (i = 0; i < n_out; i++)
			{
				float f = float_queue_pop(ctx, &fq);
				write_sample(ctx, out, 8, d_min[i], d_max[i], f);
			}
		}
		fz_write_bits_sync(ctx, out);
		fz_close_output(ctx, out);

		pdf_dict_put_int(ctx, shade, PDF_NAME(BitsPerComponent), 8);

		pdf_update_stream(ctx, doc, shade, outbuf, 0);
	}
	fz_always(ctx)
	{
		float_queue_drop(ctx, &fq);
		fz_drop_stream(ctx, stream);
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, outbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_recolor_shade_type5(fz_context *ctx, pdf_obj *shade, recolor_details *rd)
{
	fz_stream *stream;
	int i, k, n_in = rd->src_cs->n;
	int n_out = rd->dst_cs->n;
	int bpcoord = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerCoordinate));
	int bpcomp = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerComponent));
	int vprow = pdf_dict_get_int(ctx, shade, PDF_NAME(VerticesPerRow));
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	float c[FZ_MAX_COLORS];
	float d[FZ_MAX_COLORS];
	float c_min[FZ_MAX_COLORS];
	float c_max[FZ_MAX_COLORS];
	float d_min[FZ_MAX_COLORS];
	float d_max[FZ_MAX_COLORS];
	fz_buffer *outbuf = NULL;
	fz_output *out = NULL;
	float_queue fq = { 0 };

	fz_var(outbuf);
	fz_var(out);
	fz_var(stream);

	read_decode(ctx, shade, n_in, c_min, c_max, n_out, d_min, d_max);

	stream = pdf_open_stream(ctx, shade);
	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			for (i = 0; i < vprow; i++)
			{
				/* x_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
				/* y_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
				for (k = 0; k < n_in; k++)
					c[k] = read_sample(ctx, stream, bpcomp, c_min[k], c_max[k]);

				rd->recolor(ctx, rd->opaque, rd->dst_cs, d, rd->src_cs, c);

				for (k = 0; k < n_out; k++)
				{
					if (d[k] < d_min[k])
						d_min[k] = d[k];
					if (d[k] > d_max[k])
						d_max[k] = d[k];
					float_queue_push(ctx, &fq, d[k]);
				}
			}
		}
		fz_drop_stream(ctx, stream);
		stream = NULL;

		rewrite_decode(ctx, shade, n_out, d_min, d_max);

		stream = pdf_open_stream(ctx, shade);
		outbuf = fz_new_buffer(ctx, 1);
		out = fz_new_output_with_buffer(ctx, outbuf);
		while (!fz_is_eof_bits(ctx, stream))
		{
			for (i = 0; i < vprow; i++)
			{
				unsigned int x_bits = fz_read_bits(ctx, stream, bpcoord);
				unsigned int y_bits = fz_read_bits(ctx, stream, bpcoord);
				for (k = 0; k < n_in; k++)
					(void)fz_read_bits(ctx, stream, bpcomp);

				fz_write_bits(ctx, out, x_bits, bpcoord);
				fz_write_bits(ctx, out, y_bits, bpcoord);
				for (k = 0; k < n_out; k++)
				{
					float f = float_queue_pop(ctx, &fq);
					write_sample(ctx, out, 8, d_min[k], d_max[k], f);
				}
			}
		}
		fz_write_bits_sync(ctx, out);
		fz_close_output(ctx, out);

		pdf_dict_put_int(ctx, shade, PDF_NAME(BitsPerComponent), 8);

		pdf_update_stream(ctx, doc, shade, outbuf, 0);
	}
	fz_always(ctx)
	{
		float_queue_drop(ctx, &fq);
		fz_drop_stream(ctx, stream);
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, outbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_recolor_shade_type6(fz_context *ctx, pdf_obj *shade, recolor_details *rd)
{
	fz_stream *stream;
	int i, k, n_in = rd->src_cs->n;
	int n_out = rd->dst_cs->n;
	int bpflag = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerFlag));
	int bpcoord = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerCoordinate));
	int bpcomp = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerComponent));
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	float c[FZ_MAX_COLORS];
	float d[FZ_MAX_COLORS];
	float c_min[FZ_MAX_COLORS];
	float c_max[FZ_MAX_COLORS];
	float d_min[FZ_MAX_COLORS];
	float d_max[FZ_MAX_COLORS];
	fz_buffer *outbuf = NULL;
	fz_output *out = NULL;
	float_queue fq = { 0 };

	fz_var(outbuf);
	fz_var(out);
	fz_var(stream);

	read_decode(ctx, shade, n_in, c_min, c_max, n_out, d_min, d_max);

	stream = pdf_open_stream(ctx, shade);
	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			int startcolor;
			int startpt;

			int flag = fz_read_bits(ctx, stream, bpflag);

			if (flag == 0)
			{
				startpt = 0;
				startcolor = 0;
			}
			else
			{
				startpt = 4;
				startcolor = 2;
			}

			for (i = startpt; i < 12; i++)
			{
				unsigned int x_bits = fz_read_bits(ctx, stream, bpcoord);
				unsigned int y_bits = fz_read_bits(ctx, stream, bpcoord);
				fz_write_bits(ctx, out, x_bits, bpcoord);
				fz_write_bits(ctx, out, y_bits, bpcoord);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < n_in; k++)
					c[k] = read_sample(ctx, stream, bpcomp, c_min[k], c_max[k]);

				rd->recolor(ctx, rd->opaque, rd->dst_cs, d, rd->src_cs, c);

				for (k = 0; k < n_out; k++)
				{
					if (d[k] < d_min[k])
						d_min[k] = d[k];
					if (d[k] > d_max[k])
						d_max[k] = d[k];
					float_queue_push(ctx, &fq, d[k]);
				}
			}
		}
		fz_drop_stream(ctx, stream);
		stream = NULL;

		rewrite_decode(ctx, shade, n_out, d_min, d_max);

		stream = pdf_open_stream(ctx, shade);
		outbuf = fz_new_buffer(ctx, 1);
		out = fz_new_output_with_buffer(ctx, outbuf);
		while (!fz_is_eof_bits(ctx, stream))
		{
			int startcolor;
			int startpt;

			int flag = fz_read_bits(ctx, stream, bpflag);

			fz_write_bits(ctx, out, flag, bpflag);

			if (flag == 0)
			{
				startpt = 0;
				startcolor = 0;
			}
			else
			{
				startpt = 4;
				startcolor = 2;
			}

			for (i = startpt; i < 12; i++)
			{
				unsigned int x_bits = fz_read_bits(ctx, stream, bpcoord);
				unsigned int y_bits = fz_read_bits(ctx, stream, bpcoord);
				fz_write_bits(ctx, out, x_bits, bpcoord);
				fz_write_bits(ctx, out, y_bits, bpcoord);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < n_in; k++)
					(void)fz_read_bits(ctx, stream, bpcomp);

				for (k = 0; k < n_out; k++)
				{
					float f = float_queue_pop(ctx, &fq);
					write_sample(ctx, out, 8, d_min[k], d_max[k], f);
				}
			}
		}
		fz_write_bits_sync(ctx, out);
		fz_close_output(ctx, out);

		pdf_dict_put_int(ctx, shade, PDF_NAME(BitsPerComponent), 8);

		pdf_update_stream(ctx, doc, shade, outbuf, 0);
	}
	fz_always(ctx)
	{
		float_queue_drop(ctx, &fq);
		fz_drop_stream(ctx, stream);
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, outbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_recolor_shade_type7(fz_context *ctx, pdf_obj *shade, recolor_details *rd)
{
	fz_stream *stream;
	int i, k, n_in = rd->src_cs->n;
	int n_out = rd->dst_cs->n;
	int bpflag = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerFlag));
	int bpcoord = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerCoordinate));
	int bpcomp = pdf_dict_get_int(ctx, shade, PDF_NAME(BitsPerComponent));
	pdf_document *doc = pdf_get_bound_document(ctx, shade);
	float c[FZ_MAX_COLORS];
	float d[FZ_MAX_COLORS];
	float c_min[FZ_MAX_COLORS];
	float c_max[FZ_MAX_COLORS];
	float d_min[FZ_MAX_COLORS];
	float d_max[FZ_MAX_COLORS];
	fz_buffer *outbuf = NULL;
	fz_output *out = NULL;
	float_queue fq = { 0 };

	fz_var(outbuf);
	fz_var(out);
	fz_var(stream);

	read_decode(ctx, shade, n_in, c_min, c_max, n_out, d_min, d_max);

	stream = pdf_open_stream(ctx, shade);
	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			int startcolor;
			int startpt;

			int flag = fz_read_bits(ctx, stream, bpflag);

			if (flag == 0)
			{
				startpt = 0;
				startcolor = 0;
			}
			else
			{
				startpt = 4;
				startcolor = 2;
			}

			for (i = startpt; i < 16; i++)
			{
				/* x_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
				/* y_bits */ (void)fz_read_bits(ctx, stream, bpcoord);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < n_in; k++)
					c[k] = read_sample(ctx, stream, bpcomp, c_min[k], c_max[k]);

				rd->recolor(ctx, rd->opaque, rd->dst_cs, d, rd->src_cs, c);

				for (k = 0; k < n_out; k++)
				{
					if (d[k] < d_min[k])
						d_min[k] = d[k];
					if (d[k] > d_max[k])
						d_max[k] = d[k];
					float_queue_push(ctx, &fq, d[k]);
				}
			}
		}
		fz_drop_stream(ctx, stream);
		stream = NULL;

		rewrite_decode(ctx, shade, n_out, d_min, d_max);

		stream = pdf_open_stream(ctx, shade);
		outbuf = fz_new_buffer(ctx, 1);
		out = fz_new_output_with_buffer(ctx, outbuf);
		while (!fz_is_eof_bits(ctx, stream))
		{
			int startcolor;
			int startpt;

			int flag = fz_read_bits(ctx, stream, bpflag);

			fz_write_bits(ctx, out, flag, bpflag);

			if (flag == 0)
			{
				startpt = 0;
				startcolor = 0;
			}
			else
			{
				startpt = 4;
				startcolor = 2;
			}

			for (i = startpt; i < 16; i++)
			{
				unsigned int x_bits = fz_read_bits(ctx, stream, bpcoord);
				unsigned int y_bits = fz_read_bits(ctx, stream, bpcoord);
				fz_write_bits(ctx, out, x_bits, bpcoord);
				fz_write_bits(ctx, out, y_bits, bpcoord);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < n_in; k++)
					(void)fz_read_bits(ctx, stream, bpcomp);

				for (k = 0; k < n_out; k++)
				{
					float f = float_queue_pop(ctx, &fq);
					write_sample(ctx, out, 8, d_min[k], d_max[k], f);
				}
			}

		}
		fz_write_bits_sync(ctx, out);
		fz_close_output(ctx, out);

		pdf_dict_put_int(ctx, shade, PDF_NAME(BitsPerComponent), 8);

		pdf_update_stream(ctx, doc, shade, outbuf, 0);
	}
	fz_always(ctx)
	{
		float_queue_drop(ctx, &fq);
		fz_drop_stream(ctx, stream);
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, outbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

pdf_obj *
pdf_new_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	switch (fz_colorspace_type(ctx, cs))
	{
	case FZ_COLORSPACE_GRAY:
		return PDF_NAME(DeviceGray);
	case FZ_COLORSPACE_RGB:
		return PDF_NAME(DeviceRGB);
	case FZ_COLORSPACE_CMYK:
		return PDF_NAME(DeviceCMYK);
	default:
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unimplemented colorspace");
	}

	return NULL;
}

pdf_obj *
pdf_recolor_shade(fz_context *ctx, pdf_obj *shade, pdf_shade_recolorer *reshade, void *opaque)
{
	recolor_details rd;
	fz_colorspace *src_cs;
	pdf_obj *background, *new_bg = NULL;
	pdf_obj *function;
	pdf_obj *rewritten = NULL;
	pdf_obj *obj;
	int type, i;
	pdf_function *func[FZ_MAX_COLORS] = { NULL };
	float d0, d1;
	float samples[256][FZ_MAX_COLORS + 1];
	pdf_document *doc = pdf_get_bound_document(ctx, shade);

	src_cs = pdf_load_colorspace(ctx, pdf_dict_get(ctx, shade, PDF_NAME(ColorSpace)));

	fz_var(rewritten);

	rd.funcs = 0;

	fz_try(ctx)
	{
		rd.recolor = reshade(ctx, opaque, src_cs, &rd.dst_cs);
		if (rd.recolor == NULL)
			break;

		rd.src_cs = src_cs;
		rd.opaque = opaque;

		rewritten = pdf_deep_copy_obj(ctx, shade);

		type = pdf_dict_get_int(ctx, shade, PDF_NAME(ShadingType));

		pdf_dict_put_drop(ctx, rewritten, PDF_NAME(ColorSpace), pdf_new_colorspace(ctx, rd.dst_cs));

		background = pdf_dict_get(ctx, shade, PDF_NAME(Background));
		if (background)
		{
			int i, n = pdf_array_len(ctx, background);
			float bg[FZ_MAX_COLORS];
			float nbg[FZ_MAX_COLORS];

			if (n > FZ_MAX_COLORS)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "Too many background components");
			if (n != src_cs->n)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "Wrong background dimension");

			for (i = 0; i < n; i++)
				bg[i] = pdf_array_get_real(ctx, background, i);

			rd.recolor(ctx, rd.opaque, rd.dst_cs, nbg, src_cs, bg);

			new_bg = pdf_dict_put_array(ctx, rewritten, PDF_NAME(Background), rd.dst_cs->n);
			for (i = 0; i < n; i++)
				pdf_array_put_real(ctx, new_bg, i, bg[i]);
			pdf_dict_put(ctx, rewritten, PDF_NAME(Background), new_bg);
		}

		d0 = 0;
		d1 = 1;
		obj = pdf_dict_get(ctx, shade, PDF_NAME(Domain));
		if (obj)
		{
			d0 = pdf_array_get_real(ctx, obj, 0);
			d1 = pdf_array_get_real(ctx, obj, 1);
		}

		function = pdf_dict_get(ctx, shade, PDF_NAME(Function));
		if (pdf_is_dict(ctx, function))
		{
			rd.funcs = 1;
			func[0] = pdf_load_function(ctx, function, type == 1 ? 2 : 1, src_cs->n);
			if (!func[0])
				fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot load shading function (%d 0 R)", pdf_to_num(ctx, obj));

			if (type != 1)
				pdf_sample_shade_function(ctx, samples, src_cs->n, 1, func, d0, d1);
		}
		else if (pdf_is_array(ctx, function))
		{
			int in, i;

			rd.funcs = pdf_array_len(ctx, function);

			if (rd.funcs != 1 && rd.funcs != src_cs->n)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "incorrect number of shading functions");
			if (rd.funcs > FZ_MAX_COLORS)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "too many shading functions");
			if (type == 1)
				in = 2;
			else
				in = 1;

			for (i = 0; i < rd.funcs; i++)
			{
				func[i] = pdf_load_function(ctx, pdf_array_get(ctx, function, i), in, 1);
				if (!func[i])
					fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot load shading function (%d 0 R)", pdf_to_num(ctx, obj));
			}

			if (type != 1)
				pdf_sample_shade_function(ctx, samples, src_cs->n, rd.funcs, func, d0, d1);
		}
		else if (type < 4)
		{
			/* Functions are compulsory for types 1,2,3 */
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot load shading function (%d 0 R)", pdf_to_num(ctx, obj));
		}

		/* For function based shadings, we rewrite the 2d function. */
		if (type == 1)
		{
			fz_recolor_shade_type1(ctx, rewritten, func, &rd);
			break;
		}

		/* For all other function based shadings, we just rewrite the 1d function. */
		if (rd.funcs)
		{
			fz_recolor_shade_function(ctx, rewritten, samples, &rd);
			break;
		}

		/* From here on in, we're changing the mesh, which means altering a stream.
		 * We'll need to be an indirect for that to work. */
		obj = pdf_add_object(ctx, doc, rewritten);
		pdf_drop_obj(ctx, rewritten);
		rewritten = obj;

		switch (type)
		{
		case FZ_FUNCTION_BASED:
			/* Can never reach here. */
			break;
		case FZ_LINEAR:
		case FZ_RADIAL:
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Linear/Radial shadings must use functions");
			break;
		case FZ_MESH_TYPE4:
			fz_recolor_shade_type4(ctx, rewritten, &rd);
			break;
		case FZ_MESH_TYPE5:
			fz_recolor_shade_type5(ctx, rewritten, &rd);
			break;
		case FZ_MESH_TYPE6:
			fz_recolor_shade_type6(ctx, rewritten, &rd);
			break;
		case FZ_MESH_TYPE7:
			fz_recolor_shade_type7(ctx, rewritten, &rd);
			break;
		default:
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Unexpected mesh type %d\n", type);
		}
	}
	fz_always(ctx)
	{
		for (i = 0; i < rd.funcs; i++)
			pdf_drop_function(ctx, func[i]);
		fz_drop_colorspace(ctx, src_cs);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, rewritten);
		fz_rethrow(ctx);
	}

	return rewritten ? rewritten : shade;
}
