// Copyright (C) 2025 Artifex Software, Inc.
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

typedef struct {
	JNIEnv *env;
	jobject hits;
	int error;
} search_state;

static int hit_callback(fz_context *ctx, void *opaque, int quads, fz_quad *quad, int chapter, int page)
{
	search_state *state = (search_state *) opaque;
	JNIEnv *env = state->env;
	jobjectArray arr;
	int i;
	jboolean changed = JNI_FALSE;

	arr = (*env)->NewObjectArray(env, quads, cls_Quad, NULL);
	if (!arr || (*env)->ExceptionCheck(env))
	{
		state->error = 1;
		return 1;
	}

	changed = (*env)->CallBooleanMethod(env, state->hits, mid_ArrayList_add, arr);
	if (!changed || (*env)->ExceptionCheck(env))
	{
		state->error = 1;
		return 1;
	}

	for (i = 0; i < quads; i++)
	{
		jobject jquad = to_Quad_safe(ctx, env, quad[i]);
		if (!jquad || (*env)->ExceptionCheck(env))
		{
			state->error = 1;
			return 1;
		}
		(*env)->SetObjectArrayElement(env, arr, i, jquad);
		if ((*env)->ExceptionCheck(env))
		{
			state->error = 1;
			return 1;
		}
		(*env)->DeleteLocalRef(env, jquad);
	}

	(*env)->DeleteLocalRef(env, arr);

	return 0;
}

typedef struct
{
	pdf_processor super;
	int extgstate;

	JNIEnv *env;
	jobject self;
} pdf_java_processor;

#define PROC_BEGIN(OP) \
	jobject jproc = ((pdf_java_processor*) proc)->self; \
	jboolean detach = JNI_FALSE; \
	JNIEnv *env = jni_attach_thread(&detach); \
	if (env == NULL) \
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in java_proc_%s", OP);

#define PROC_END(N) \
	if ((*env)->ExceptionCheck(env)) \
		fz_throw_java_and_detach_thread(ctx, env, detach); \
	jni_detach_thread(detach);

static int java_is_ascii(unsigned char *str, size_t len)
{
	size_t i, is_ascii = 1;
	for (i = 0; i < len; ++i)
		if (str[i] == 0 || str[i] > 127)
			is_ascii = 0;
	return is_ascii;
}

static jstring java_to_string(JNIEnv *env, fz_context *ctx, unsigned char *str, size_t len)
{
	jstring jstr = (*env)->NewStringUTF(env, (char *) str);
	if ((*env)->ExceptionCheck(env))
		return NULL;
	return jstr;
}

static jobject java_to_byte_array(JNIEnv *env, fz_context *ctx, unsigned char *str, size_t len)
{
	size_t i;
	jobject jarray = (*env)->NewByteArray(env, len);
	if ((*env)->ExceptionCheck(env))
		return NULL;
	for (i = 0; i < len; ++i)
	{
		jbyte v = str[i];
		(*env)->SetByteArrayRegion(env, jarray, i, 1, &v);
	}
	return jarray;
}

static void java_proc_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	PROC_BEGIN("w");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_w, linewidth);
	PROC_END("w");
}

static void java_proc_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	PROC_BEGIN("j");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_j, linejoin);
	PROC_END("j");
}

static void java_proc_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	PROC_BEGIN("J");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_J, linecap);
	PROC_END("J");
}

static void java_proc_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	PROC_BEGIN("M");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_M, miterlimit);
	PROC_END("M");
}

static void java_proc_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array_, float phase)
{
	jfloatArray jarray;
	int i, n;
	PROC_BEGIN("d");
	n = pdf_array_len(ctx, array_);
	jarray = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	if (!jarray)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate float array");
	for (i = 0; i < n; i++)
	{
		float v = pdf_array_get_real(ctx, array_, i);
		(*env)->SetFloatArrayRegion(env, jarray, i, 1, &v);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);
	}
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_d, jarray, phase);
	(*env)->DeleteLocalRef(env, jarray);
	PROC_END("d");
}

static void java_proc_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	PROC_BEGIN("i");
	jstring jintent = (*env)->NewStringUTF(env, intent);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_ri, jintent);
	(*env)->DeleteLocalRef(env, jintent);
	PROC_END("i");
}

static void java_proc_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	PROC_BEGIN("i");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_i, flatness);
	PROC_END("i");
}

static void java_proc_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	PROC_BEGIN("gs");
	((pdf_java_processor*)proc)->extgstate = 1;
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jextgstate = to_PDFObject_safe(ctx, env, extgstate);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_gs, jname, jextgstate);
	(*env)->DeleteLocalRef(env, jextgstate);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("gs");
}

static void java_proc_gs_end(fz_context *ctx, pdf_processor *proc)
{
	((pdf_java_processor*)proc)->extgstate = 0;
}

static void java_proc_q(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("q");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_q);
	PROC_END("q");
}

static void java_proc_Q(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("Q");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_q);
	PROC_END("Q");
}

static void java_proc_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PROC_BEGIN("cm");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_cm, a, b, c, d, e, f);
	PROC_END("cm");
}

static void java_proc_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PROC_BEGIN("m");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_m, x, y);
	PROC_END("m");
}

static void java_proc_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PROC_BEGIN("l");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_l, x, y);
	PROC_END("l");
}

static void java_proc_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	PROC_BEGIN("c");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_c, x1, y1, x2, y2, x3, y3);
	PROC_END("c");
}

static void java_proc_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	PROC_BEGIN("v");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_v, x2, y2, x3, y3);
	PROC_END("v");
}

static void java_proc_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	PROC_BEGIN("y");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_y, x1, y1, x3, y3);
	PROC_END("y");
}

static void java_proc_h(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("h");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_h);
	PROC_END("h");
}

static void java_proc_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	PROC_BEGIN("re");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_re, x, y, w, h);
	PROC_END("re");
}

static void java_proc_S(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("S");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_S);
	PROC_END("S");
}

static void java_proc_s(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("s");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_s);
	PROC_END("s");
}

static void java_proc_F(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("F");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_F);
	PROC_END("F");
}

static void java_proc_f(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("f");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_f);
	PROC_END("f");
}

static void java_proc_fstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("fstar");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_fstar);
	PROC_END("fstar");
}

static void java_proc_B(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("B");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_B);
	PROC_END("B");
}

static void java_proc_Bstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("Bstar");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Bstar);
	PROC_END("Bstar");
}

static void java_proc_b(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("b");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_b);
	PROC_END("b");
}

static void java_proc_bstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("bstar");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_bstar);
	PROC_END("bstar");
}

static void java_proc_n(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("n");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_n);
	PROC_END("n");
}

static void java_proc_W(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("W");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_W);
	PROC_END("W");
}

static void java_proc_Wstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("Wstar");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Wstar);
	PROC_END("Wstar");
}

static void java_proc_BT(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("BT");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_BT);
	PROC_END("BT");
}

static void java_proc_ET(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("ET");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_ET);
	PROC_END("ET");
}

static void java_proc_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	PROC_BEGIN("Tc");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tc, charspace);
	PROC_END("Tc");
}

static void java_proc_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	PROC_BEGIN("Tw");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tw, wordspace);
	PROC_END("Tw");
}

static void java_proc_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	PROC_BEGIN("Tz");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tz, scale);
	PROC_END("Tz");
}

static void java_proc_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	PROC_BEGIN("TL");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_TL, leading);
	PROC_END("TL");
}

static void java_proc_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	if (!((pdf_java_processor*) proc)->extgstate)
	{
		jobject jname;
		PROC_BEGIN("Tf");
		jname = (*env)->NewStringUTF(env, name);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tf, jname, size);
		(*env)->DeleteLocalRef(env, jname);
		PROC_END("Tf");
	}
}

static void java_proc_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	PROC_BEGIN("Tr");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tr, render);
	PROC_END("Tr");
}

static void java_proc_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	PROC_BEGIN("Ts");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Ts, rise);
	PROC_END("Ts");
}

static void java_proc_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PROC_BEGIN("Td");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Td, tx, ty);
	PROC_END("Td");
}

static void java_proc_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PROC_BEGIN("TD");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_TD, tx, ty);
	PROC_END("TD");
}

static void java_proc_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PROC_BEGIN("Tm");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tm, a, b, c, d, e, f);
	PROC_END("Tm");
}

static void java_proc_Tstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("Tstar");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tstar);
	PROC_END("Tstar");
}

static void java_proc_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	PROC_BEGIN("TJ");
	int i, n = pdf_array_len(ctx, array);
	pdf_obj *obj;
	jobject jarray = (*env)->NewObjectArray(env, n, cls_Object, NULL);
	for (i = 0; i < n; i++)
	{
		jobject jelem;
		obj = pdf_array_get(ctx, array, i);
		if (pdf_is_number(ctx, obj))
			jelem = (*env)->NewObject(env, cls_Float, mid_Float_init, pdf_to_real(ctx, obj));
		else
		{
			char *str = pdf_to_str_buf(ctx, obj);
			size_t len = pdf_to_str_len(ctx, obj);
			if (java_is_ascii((unsigned char *) str, len))
				jelem = java_to_string(env, ctx, (unsigned char *) str, len);
			else
				jelem = java_to_byte_array(env, ctx, (unsigned char *) str, len);
			if (!jelem)
				fz_throw_java_and_detach_thread(ctx, env, detach);
		}
		(*env)->SetObjectArrayElement(env, jarray, i, jelem);
		(*env)->DeleteLocalRef(env, jelem);
	}
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_TJ, jarray);
	(*env)->DeleteLocalRef(env, jarray);
	PROC_END("TJ");
}

static void java_proc_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PROC_BEGIN("Tj");

	if (java_is_ascii((unsigned char *) str, len))
	{
		jstring jstr = java_to_string(env, ctx, (unsigned char *) str, len);
		if (!jstr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tj_string, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	else
	{
		jobject jarr = java_to_byte_array(env, ctx, (unsigned char *) str, len);
		if (!jarr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Tj_byte_array, jarr);
		(*env)->DeleteLocalRef(env, jarr);
	}
	PROC_END("Tj");
}

static void java_proc_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PROC_BEGIN("dquote");
	if (java_is_ascii((unsigned char *) str, len))
	{
		jstring jstr = java_to_string(env, ctx, (unsigned char *) str, len);
		if (!jstr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_squote_string, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	else
	{
		jobject jarr = java_to_byte_array(env, ctx, (unsigned char *) str, len);
		if (!jarr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_squote_byte_array, jarr);
		(*env)->DeleteLocalRef(env, jarr);
	}
	PROC_END("squote");
}

static void java_proc_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	PROC_BEGIN("dquote");
	if (java_is_ascii((unsigned char *) str, len))
	{
		jstring jstr = java_to_string(env, ctx, (unsigned char *) str, len);
		if (!jstr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_dquote_string, aw, ac, jstr);
		(*env)->DeleteLocalRef(env, jstr);
	}
	else
	{
		jobject jarr = java_to_byte_array(env, ctx, (unsigned char *) str, len);
		if (!jarr)
			fz_throw_java_and_detach_thread(ctx, env, detach);
		(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_dquote_byte_array, aw, ac, jarr);
		(*env)->DeleteLocalRef(env, jarr);
	}
	PROC_END("dquote");
}

static void java_proc_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	PROC_BEGIN("d0");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_d0, wx, wy);
	PROC_END("d0");
}

static void java_proc_d1(fz_context *ctx, pdf_processor *proc,
	float wx, float wy, float llx, float lly, float urx, float ury)
{
	PROC_BEGIN("d1");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_d1, wx, wy, llx, lly, urx, ury);
	PROC_END("d1");
}

static void java_proc_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PROC_BEGIN("CS");
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jcs = to_ColorSpace_safe(ctx, env, cs);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_CS, jname, jcs);
	(*env)->DeleteLocalRef(env, jcs);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("CS");
}

static void java_proc_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PROC_BEGIN("cs");
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jcs = to_ColorSpace_safe(ctx, env, cs);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_cs, jname, jcs);
	(*env)->DeleteLocalRef(env, jcs);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("cs");
}

static void java_proc_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	PROC_BEGIN("SC_pattern");
	int i;
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jcolor = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	for (i = 0; i < n; ++i)
		(*env)->SetFloatArrayRegion(env, jcolor, i, 1, &color[i]);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_SC_pattern, jname, pat->id, jcolor);
	(*env)->DeleteLocalRef(env, jcolor);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("SC_pattern");
}

static void java_proc_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	PROC_BEGIN("sc_pattern");
	int i;
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jcolor = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	for (i = 0; i < n; ++i)
		(*env)->SetFloatArrayRegion(env, jcolor, i, 1, &color[i]);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_sc_pattern, jname, pat->id, jcolor);
	(*env)->DeleteLocalRef(env, jcolor);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("sc_pattern");
}

static void java_proc_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("SC_shade");
	jstring jname = (*env)->NewStringUTF(env, name);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_SC_shade, jname);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("SC_shade");
}

static void java_proc_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("sc_shade");
	jstring jname = (*env)->NewStringUTF(env, name);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_sc_shade, jname);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("sc_shade");
}

static void java_proc_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	PROC_BEGIN("SC_color");
	int i;
	jobject jcolor = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	for (i = 0; i < n; ++i)
		(*env)->SetFloatArrayRegion(env, jcolor, i, 1, &color[i]);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_SC_color, jcolor);
	(*env)->DeleteLocalRef(env, jcolor);
	PROC_END("SC_color");
}

static void java_proc_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	PROC_BEGIN("sc_color");
	int i;
	jobject jcolor = (*env)->NewFloatArray(env, n);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);
	for (i = 0; i < n; ++i)
		(*env)->SetFloatArrayRegion(env, jcolor, i, 1, &color[i]);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_sc_color, jcolor);
	(*env)->DeleteLocalRef(env, jcolor);
	PROC_END("sc_color");
}

static void java_proc_G(fz_context *ctx, pdf_processor *proc, float g)
{
	PROC_BEGIN("g");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_g, g);
	PROC_END("g");
}

static void java_proc_g(fz_context *ctx, pdf_processor *proc, float g)
{
	PROC_BEGIN("G");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_G, g);
	PROC_END("G");
}

static void java_proc_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PROC_BEGIN("RG");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_RG, r, g, b);
	PROC_END("RG");
}

static void java_proc_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PROC_BEGIN("rg");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_rg, r, g, b);
	PROC_END("rg");
}

static void java_proc_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PROC_BEGIN("K");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_K, c, m, y, k);
	PROC_END("K");
}

static void java_proc_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PROC_BEGIN("k");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_k, c, m, y, k);
	PROC_END("k");
}

static void java_proc_BI(fz_context *ctx, pdf_processor *proc, fz_image *img, const char *colorspace)
{
	PROC_BEGIN("BI");
	jobject jimg = to_Image_safe(ctx, env, img);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_BI, jimg);
	(*env)->DeleteLocalRef(env, jimg);
	PROC_END("BI");
}

static void java_proc_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("sh");
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jshade = to_Shade_safe(ctx, env, shade);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_sh, jname, jshade);
	(*env)->DeleteLocalRef(env, jshade);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("sh");
}

static void java_proc_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	PROC_BEGIN("Do_image");
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jimage = to_Image_safe(ctx, env, image);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Do_image, jname, jimage);
	(*env)->DeleteLocalRef(env, jimage);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("Do_image");
}

static void java_proc_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	PROC_BEGIN("Do_image");
	jstring jname = (*env)->NewStringUTF(env, name);
	jobject jform = to_PDFObject_safe(ctx, env, xobj);
	jobject jres = NULL;
	if (proc->rstack)
		jres = to_PDFObject_safe(ctx, env, proc->rstack->resources);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_Do_image, jname, jform, jres);
	(*env)->DeleteLocalRef(env, jres);
	(*env)->DeleteLocalRef(env, jform);
	(*env)->DeleteLocalRef(env, jname);
	PROC_END("Do_image");
}

static void java_proc_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PROC_BEGIN("MP");
	jobject jtag = (*env)->NewStringUTF(env, tag);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_MP, jtag);
	(*env)->DeleteLocalRef(env, jtag);
	PROC_END("MP");
}

static void java_proc_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PROC_BEGIN("DP");
	jstring jtag = (*env)->NewStringUTF(env, tag);
	jobject jraw = to_PDFObject_safe(ctx, env, raw);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_DP, jtag, jraw);
	(*env)->DeleteLocalRef(env, jraw);
	(*env)->DeleteLocalRef(env, jtag);
	PROC_END("DP");
}

static void java_proc_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PROC_BEGIN("BMC");
	jstring jtag = (*env)->NewStringUTF(env, tag);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_BMC, jtag);
	(*env)->DeleteLocalRef(env, jtag);
	PROC_END("BMC");
}

static void java_proc_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PROC_BEGIN("BDC");
	jstring jtag = (*env)->NewStringUTF(env, tag);
	jobject jraw = to_PDFObject_safe(ctx, env, raw);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_BDC, jtag, jraw);
	(*env)->DeleteLocalRef(env, jraw);
	(*env)->DeleteLocalRef(env, jtag);
	PROC_END("BDC");
}

static void java_proc_EMC(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("EMC");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_EMC);
	PROC_END("EMC");
}

static void java_proc_BX(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("BX");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_BX);
	PROC_END("BX");
}

static void java_proc_EX(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("EX");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_EX);
	PROC_END("EX");
}

static void java_proc_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	PROC_BEGIN("pushResources");
	jobject jres = to_PDFObject_safe(ctx, env, res);
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_pushResources, jres);
	(*env)->DeleteLocalRef(env, jres);
	PROC_END("pushResources");
}

static pdf_obj *java_proc_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("popResources");
	(*env)->CallVoidMethod(env, jproc, mid_PDFProcessor_op_popResources);
	PROC_END("popResources");
	return NULL;
}

pdf_processor *make_pdf_processor(JNIEnv *env, fz_context *ctx, jobject jproc)
{
	pdf_java_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	proc->super.close_processor = NULL;

	proc->super.push_resources = java_proc_push_resources;
	proc->super.pop_resources = java_proc_pop_resources;

	/* general graphics state */
	proc->super.op_w = java_proc_w;
	proc->super.op_j = java_proc_j;
	proc->super.op_J = java_proc_J;
	proc->super.op_M = java_proc_M;
	proc->super.op_d = java_proc_d;
	proc->super.op_ri = java_proc_ri;
	proc->super.op_i = java_proc_i;
	proc->super.op_gs_begin = java_proc_gs_begin;
	proc->super.op_gs_end = java_proc_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = NULL;
	proc->super.op_gs_CA = NULL;
	proc->super.op_gs_ca = NULL;
	proc->super.op_gs_SMask = NULL;

	/* special graphics state */
	proc->super.op_q = java_proc_q;
	proc->super.op_Q = java_proc_Q;
	proc->super.op_cm = java_proc_cm;

	/* path construction */
	proc->super.op_m = java_proc_m;
	proc->super.op_l = java_proc_l;
	proc->super.op_c = java_proc_c;
	proc->super.op_v = java_proc_v;
	proc->super.op_y = java_proc_y;
	proc->super.op_h = java_proc_h;
	proc->super.op_re = java_proc_re;

	/* path painting */
	proc->super.op_S = java_proc_S;
	proc->super.op_s = java_proc_s;
	proc->super.op_F = java_proc_F;
	proc->super.op_f = java_proc_f;
	proc->super.op_fstar = java_proc_fstar;
	proc->super.op_B = java_proc_B;
	proc->super.op_Bstar = java_proc_Bstar;
	proc->super.op_b = java_proc_b;
	proc->super.op_bstar = java_proc_bstar;
	proc->super.op_n = java_proc_n;

	/* clipping paths */
	proc->super.op_W = java_proc_W;
	proc->super.op_Wstar = java_proc_Wstar;

	/* text objects */
	proc->super.op_BT = java_proc_BT;
	proc->super.op_ET = java_proc_ET;

	/* text state */
	proc->super.op_Tc = java_proc_Tc;
	proc->super.op_Tw = java_proc_Tw;
	proc->super.op_Tz = java_proc_Tz;
	proc->super.op_TL = java_proc_TL;
	proc->super.op_Tf = java_proc_Tf;
	proc->super.op_Tr = java_proc_Tr;
	proc->super.op_Ts = java_proc_Ts;

	/* text positioning */
	proc->super.op_Td = java_proc_Td;
	proc->super.op_TD = java_proc_TD;
	proc->super.op_Tm = java_proc_Tm;
	proc->super.op_Tstar = java_proc_Tstar;

	/* text showing */
	proc->super.op_TJ = java_proc_TJ;
	proc->super.op_Tj = java_proc_Tj;
	proc->super.op_squote = java_proc_squote;
	proc->super.op_dquote = java_proc_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = java_proc_d0;
	proc->super.op_d1 = java_proc_d1;

	/* color */
	proc->super.op_CS = java_proc_CS;
	proc->super.op_cs = java_proc_cs;
	proc->super.op_SC_color = java_proc_SC_color;
	proc->super.op_sc_color = java_proc_sc_color;
	proc->super.op_SC_pattern = java_proc_SC_pattern;
	proc->super.op_sc_pattern = java_proc_sc_pattern;
	proc->super.op_SC_shade = java_proc_SC_shade;
	proc->super.op_sc_shade = java_proc_sc_shade;

	proc->super.op_G = java_proc_G;
	proc->super.op_g = java_proc_g;
	proc->super.op_RG = java_proc_RG;
	proc->super.op_rg = java_proc_rg;
	proc->super.op_K = java_proc_K;
	proc->super.op_k = java_proc_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = java_proc_BI;
	proc->super.op_sh = java_proc_sh;
	proc->super.op_Do_image = java_proc_Do_image;
	proc->super.op_Do_form = java_proc_Do_form;

	/* marked content */
	proc->super.op_MP = java_proc_MP;
	proc->super.op_DP = java_proc_DP;
	proc->super.op_BMC = java_proc_BMC;
	proc->super.op_BDC = java_proc_BDC;
	proc->super.op_EMC = java_proc_EMC;

	/* compatibility */
	proc->super.op_BX = java_proc_BX;
	proc->super.op_EX = java_proc_EX;

	/* extgstate */
	proc->super.op_gs_OP = NULL;
	proc->super.op_gs_op = NULL;
	proc->super.op_gs_OPM = NULL;
	proc->super.op_gs_UseBlackPtComp = NULL;

	proc->self = jproc;
	proc->env = env;

	return (pdf_processor*)proc;
}

/* Callbacks to implement fz_stream and fz_output using Java classes */

typedef struct
{
	jobject stream;
	jbyteArray array;
	jbyte buffer[8192];
}
SeekableStreamState;

static int SeekableInputStream_next(fz_context *ctx, fz_stream *stm, size_t max)
{
	SeekableStreamState *state = stm->state;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;
	int n, ch;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_next");

	n = (*env)->CallIntMethod(env, state->stream, mid_SeekableInputStream_read, state->array);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	if (n > 0)
	{
		(*env)->GetByteArrayRegion(env, state->array, 0, n, state->buffer);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);

		/* update stm->pos so fz_tell knows the current position */
		stm->rp = (unsigned char *)state->buffer;
		stm->wp = stm->rp + n;
		stm->pos += n;

		ch = *stm->rp++;
	}
	else if (n < 0)
	{
		ch = EOF;
	}
	else
		fz_throw_and_detach_thread(ctx, detach, FZ_ERROR_GENERIC, "no bytes read");

	jni_detach_thread(detach);
	return ch;
}

static void SeekableInputStream_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	SeekableStreamState *state = stm->state;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;
	int64_t pos;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableInputStream_seek");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	stm->pos = pos;
	stm->rp = stm->wp = (unsigned char *)state->buffer;

	jni_detach_thread(detach);
}

static void SeekableInputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
	{
		fz_warn(ctx, "cannot attach to JVM in SeekableInputStream_drop; leaking input stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_write(fz_context *ctx, void *streamState_, const void *buffer_, size_t count)
{
	SeekableStreamState *state = streamState_;
	const jbyte *buffer = buffer_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_write");

	while (count > 0)
	{
		size_t n = fz_minz(count, sizeof(state->buffer));

		(*env)->SetByteArrayRegion(env, state->array, 0, n, buffer);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);

		buffer += n;
		count -= n;

		(*env)->CallVoidMethod(env, state->stream, mid_SeekableOutputStream_write, state->array, 0, n);
		if ((*env)->ExceptionCheck(env))
			fz_throw_java_and_detach_thread(ctx, env, detach);
	}

	jni_detach_thread(detach);
}

static int64_t SeekableOutputStream_tell(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	int64_t pos = 0;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_tell");

	pos = (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_position);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);

	return pos;
}

static void SeekableOutputStream_truncate(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_truncate");

	(*env)->CallVoidMethod(env, state->stream, mid_SeekableOutputStream_truncate);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_seek(fz_context *ctx, void *streamState_, int64_t offset, int whence)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot attach to JVM in SeekableOutputStream_seek");

	(void) (*env)->CallLongMethod(env, state->stream, mid_SeekableStream_seek, offset, whence);
	if ((*env)->ExceptionCheck(env))
		fz_throw_java_and_detach_thread(ctx, env, detach);

	jni_detach_thread(detach);
}

static void SeekableOutputStream_drop(fz_context *ctx, void *streamState_)
{
	SeekableStreamState *state = streamState_;
	jboolean detach = JNI_FALSE;
	JNIEnv *env;

	env = jni_attach_thread(&detach);
	if (env == NULL)
	{
		fz_warn(ctx, "cannot attach to JVM in SeekableOutputStream_drop; leaking output stream");
		return;
	}

	(*env)->DeleteGlobalRef(env, state->stream);
	(*env)->DeleteGlobalRef(env, state->array);

	fz_free(ctx, state);

	jni_detach_thread(detach);
}
