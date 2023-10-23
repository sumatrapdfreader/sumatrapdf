// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#if FZ_ENABLE_PDF
#include "mupdf/pdf.h"
#include "mupdf/helpers/pkcs7-openssl.h"
#endif

#if FZ_ENABLE_JS

#include "mujs.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define PS1 "> "

FZ_NORETURN static void rethrow(js_State *J)
{
	js_newerror(J, fz_caught_message(js_getcontext(J)));
	js_throw(J);
}

FZ_NORETURN static void rethrow_as_fz(js_State *J)
{
	fz_throw(js_getcontext(J), FZ_ERROR_GENERIC, "%s", js_tostring(J, -1));
}

static void *alloc(void *actx, void *ptr, int n)
{
	return fz_realloc_no_throw(actx, ptr, n);
}

static int eval_print(js_State *J, const char *source)
{
	if (js_ploadstring(J, "[string]", source)) {
		fprintf(stderr, "%s\n", js_trystring(J, -1, "Error"));
		js_pop(J, 1);
		return 1;
	}
	js_pushundefined(J);
	if (js_pcall(J, 0)) {
		fprintf(stderr, "%s\n", js_trystring(J, -1, "Error"));
		js_pop(J, 1);
		return 1;
	}
	if (js_isdefined(J, -1)) {
		printf("%s\n", js_tryrepr(J, -1, "can't convert to string"));
	}
	js_pop(J, 1);
	return 0;
}

static void jsB_propfun(js_State *J, const char *name, js_CFunction cfun, int n)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_newcfunction(J, cfun, name, n);
	js_defproperty(J, -2, realname, JS_DONTENUM);
}

static void jsB_propcon(js_State *J, const char *tag, const char *name, js_CFunction cfun, int n)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_getregistry(J, tag);
	js_newcconstructor(J, cfun, cfun, name, n);
	js_defproperty(J, -2, realname, JS_DONTENUM);
}

static void jsB_gc(js_State *J)
{
	int report = js_toboolean(J, 1);
	js_gc(J, report);
}

static void jsB_load(js_State *J)
{
	const char *filename = js_tostring(J, 1);
	int rv = js_dofile(J, filename);
	js_pushboolean(J, !rv);
}

static void jsB_print(js_State *J)
{
	unsigned int i, top = js_gettop(J);
	for (i = 1; i < top; ++i) {
		const char *s = js_tostring(J, i);
		if (i > 1) putchar(' ');
		fputs(s, stdout);
	}
	putchar('\n');
}

static void jsB_write(js_State *J)
{
	unsigned int i, top = js_gettop(J);
	for (i = 1; i < top; ++i) {
		const char *s = js_tostring(J, i);
		if (i > 1) putchar(' ');
		fputs(s, stdout);
	}
}

static void jsB_read(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *filename = js_tostring(J, 1);
	FILE *f;
	char *s;
	long n;
	size_t t;

	f = fopen(filename, "rb");
	if (!f) {
		js_error(J, "cannot open file: '%s'", filename);
	}

	if (fseek(f, 0, SEEK_END) < 0) {
		fclose(f);
		js_error(J, "cannot seek in file: '%s'", filename);
	}

	n = ftell(f);
	if (n < 0) {
		fclose(f);
		js_error(J, "cannot tell in file: '%s'", filename);
	}

	if (fseek(f, 0, SEEK_SET) < 0) {
		fclose(f);
		js_error(J, "cannot seek in file: '%s'", filename);
	}

	s = fz_malloc(ctx, n + 1);
	if (!s) {
		fclose(f);
		js_error(J, "cannot allocate storage for file contents: '%s'", filename);
	}

	t = fread(s, 1, n, f);
	if (t != (size_t) n) {
		fz_free(ctx, s);
		fclose(f);
		js_error(J, "cannot read data from file: '%s'", filename);
	}
	s[n] = 0;

	js_pushstring(J, s);
	fz_free(ctx, s);
	fclose(f);
}

static void jsB_readline(js_State *J)
{
	char line[256];
	size_t n;
	if (!fgets(line, sizeof line, stdin))
		js_error(J, "cannot read line from stdin");
	n = strlen(line);
	if (n > 0 && line[n-1] == '\n')
		line[n-1] = 0;
	js_pushstring(J, line);
}

static void jsB_repr(js_State *J)
{
	js_repr(J, 1);
}

JS_NORETURN
static void jsB_quit(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	int status = js_tonumber(J, 1);
	js_freestate(J);
	fz_drop_context(ctx);
	exit(status);
}

static const char *prefix_js =
	"var console = { log: print, info: print, error: print }\n"
	"function require(name) {\n"
	"	var cache = require.cache\n"
	"	if (name in cache) return cache[name]\n"
	"	var exports = cache[name] = {}\n"
	"	Function('exports', read(name+'.js'))(exports)\n"
	"	return exports\n"
	"}\n"
	"require.cache = Object.create(null)\n"
	"Error.prototype.toString = function() {\n"
	"	if (this.stackTrace) return this.name + ': ' + this.message + this.stackTrace\n"
	"	return this.name + ': ' + this.message\n"
	"}\n"
	;

const char *postfix_js =
	"require.cache.mupdf = mupdf\n"
	"require.cache.fs = {\n"
	"	readFileSync: readFile,\n"
	"	writeFileSync: function (fn, buf) { buf.save(fn) }\n"
	"}\n"
	"\n"
	"mupdf.Matrix = {\n"
	"	identity: [ 1, 0, 0, 1, 0, 0 ],\n"
	"	scale: function (sx, sy) {\n"
	"		return [ sx, 0, 0, sy, 0, 0 ]\n"
	"	},\n"
	"	translate: function (tx, ty) {\n"
	"		return [ 1, 0, 0, 1, tx, ty ]\n"
	"	},\n"
	"	rotate: function (d) {\n"
	"		while (d < 0)\n"
	"			d += 360\n"
	"		while (d >= 360)\n"
	"			d -= 360\n"
	"		var s = Math.sin((d * Math.PI) / 180)\n"
	"		var c = Math.cos((d * Math.PI) / 180)\n"
	"		return [ c, s, -s, c, 0, 0 ]\n"
	"	},\n"
	"	invert: function (m) {\n"
	"		var det = m[0] * m[3] - m[1] * m[2]\n"
	"		if (det > -1e-23 && det < 1e-23)\n"
	"			return m\n"
	"		var rdet = 1 / det\n"
	"		var inva = m[3] * rdet\n"
	"		var invb = -m[1] * rdet\n"
	"		var invc = -m[2] * rdet\n"
	"		var invd = m[0] * rdet\n"
	"		var inve = -m[4] * inva - m[5] * invc\n"
	"		var invf = -m[4] * invb - m[5] * invd\n"
	"		return [ inva, invb, invc, invd, inve, invf ]\n"
	"	},\n"
	"	concat: function (one, two) {\n"
	"		return [\n"
	"			one[0] * two[0] + one[1] * two[2],\n"
	"			one[0] * two[1] + one[1] * two[3],\n"
	"			one[2] * two[0] + one[3] * two[2],\n"
	"			one[2] * two[1] + one[3] * two[3],\n"
	"			one[4] * two[0] + one[5] * two[2] + two[4],\n"
	"			one[4] * two[1] + one[5] * two[3] + two[5],\n"
	"		]\n"
	"	},\n"
	"}\n"
	"\n"
	"mupdf.Rect = {\n"
	"	isEmpty: function (rect) {\n"
	"		return rect[0] >= rect[2] || rect[1] >= rect[3]\n"
	"	},\n"
	"	isValid: function (rect) {\n"
	"		return rect[0] <= rect[2] && rect[1] <= rect[3]\n"
	"	},\n"
	"	isInfinite: function (rect) {\n"
	"		return (\n"
	"			rect[0] === 0x80000000 &&\n"
	"			rect[1] === 0x80000000 &&\n"
	"			rect[2] === 0x7fffff80 &&\n"
	"			rect[3] === 0x7fffff80\n"
	"		)\n"
	"	},\n"
	"	transform: function (rect, matrix) {\n"
	"		var t\n"
	"		if (Rect.isInfinite(rect))\n"
	"			return rect\n"
	"		if (!Rect.isValid(rect))\n"
	"			return rect\n"
	"		var ax0 = rect[0] * matrix[0]\n"
	"		var ax1 = rect[2] * matrix[0]\n"
	"		if (ax0 > ax1)\n"
	"			t = ax0, ax0 = ax1, ax1 = t\n"
	"		var cy0 = rect[1] * matrix[2]\n"
	"		var cy1 = rect[3] * matrix[2]\n"
	"		if (cy0 > cy1)\n"
	"			t = cy0, cy0 = cy1, cy1 = t\n"
	"		ax0 += cy0 + matrix[4]\n"
	"		ax1 += cy1 + matrix[4]\n"
	"		var bx0 = rect[0] * matrix[1]\n"
	"		var bx1 = rect[2] * matrix[1]\n"
	"		if (bx0 > bx1)\n"
	"			t = bx0, bx0 = bx1, bx1 = t\n"
	"		var dy0 = rect[1] * matrix[3]\n"
	"		var dy1 = rect[3] * matrix[3]\n"
	"		if (dy0 > dy1)\n"
	"			t = dy0, dy0 = dy1, dy1 = t\n"
	"		bx0 += dy0 + matrix[5]\n"
	"		bx1 += dy1 + matrix[5]\n"
	"		return [ ax0, bx0, ax1, bx1 ]\n"
	"	},\n"
	"}\n"
	"\n"
	"mupdf.PDFDocument.prototype.getEmbeddedFiles = function () {\n"
	"        function _getEmbeddedFilesRec(result, N) {\n"
	"                var i, n\n"
	"                if (N) {\n"
	"                        var NN = N.get('Names')\n"
	"                        if (NN)\n"
	"                                for (i = 0, n = NN.length; i < n; i += 2)\n"
	"                                        result[NN.get(i+0).asString()] = NN.get(i+1)\n"
	"                        var NK = N.get('Kids')\n"
	"                        if (NK)\n"
	"                                for (i = 0, n = NK.length; i < n; i += 1)\n"
	"                                        _getEmbeddedFilesRec(result, NK.get(i))\n"
	"                }\n"
	"                return result\n"
	"        }\n"
	"        return _getEmbeddedFilesRec({}, this.getTrailer().get('Root', 'Names', 'EmbeddedFiles'))\n"
	"}\n"
;

struct event_cb_data
{
	js_State *J;
	const char *listener;
};

/* destructors */

static void ffi_gc_fz_archive(js_State *J, void *arch)
{
	fz_context *ctx = js_getcontext(J);
	fz_try(ctx)
		fz_drop_archive(ctx, arch);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_gc_fz_buffer(js_State *J, void *buf)
{
	fz_context *ctx = js_getcontext(J);
	fz_try(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_gc_fz_document(js_State *J, void *doc)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_document(ctx, doc);
}

static void ffi_gc_fz_page(js_State *J, void *page)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_page(ctx, page);
}

static void ffi_gc_fz_colorspace(js_State *J, void *colorspace)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_colorspace(ctx, colorspace);
}

static void ffi_gc_fz_default_colorspaces(js_State *J, void *default_cs)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_default_colorspaces(ctx, default_cs);
}

static void ffi_gc_fz_pixmap(js_State *J, void *pixmap)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_pixmap(ctx, pixmap);
}

static void ffi_gc_fz_path(js_State *J, void *path)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_path(ctx, path);
}

static void ffi_gc_fz_text(js_State *J, void *text)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_text(ctx, text);
}

static void ffi_gc_fz_font(js_State *J, void *font)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_font(ctx, font);
}

static void ffi_gc_fz_shade(js_State *J, void *shade)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_shade(ctx, shade);
}

static void ffi_gc_fz_image(js_State *J, void *image)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_image(ctx, image);
}

static void ffi_gc_fz_display_list(js_State *J, void *list)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_display_list(ctx, list);
}

static void ffi_gc_fz_stext_page(js_State *J, void *text)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_stext_page(ctx, text);
}

static void ffi_gc_fz_device(js_State *J, void *device)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_device(ctx, device);
}

static void ffi_gc_fz_document_writer(js_State *J, void *wri)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_document_writer(ctx, wri);
}

static void ffi_gc_fz_outline_iterator(js_State *J, void *iter)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_outline_iterator(ctx, iter);
}

static void ffi_gc_fz_story(js_State *J, void *story)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_story(ctx, story);
}

static void ffi_gc_fz_xml(js_State *J, void *xml)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_xml(ctx, xml);
}

static void ffi_pushoutlineiterator(js_State *J, fz_outline_iterator *iter)
{
	js_getregistry(J, "fz_outline_iterator");
	js_newuserdata(J, "fz_outline_iterator", iter, ffi_gc_fz_outline_iterator);
}

static void ffi_pushdom(js_State *J, fz_xml *dom)
{
	if (dom)
	{
		js_getregistry(J, "fz_xml");
		js_newuserdata(J, "fz_xml", dom, ffi_gc_fz_xml);
	}
	else
		js_pushnull(J);
}

static void ffi_pushpixmap(js_State *J, fz_pixmap *pixmap)
{
	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
}

static fz_pixmap *ffi_topixmap(js_State *J, int idx)
{
	return (fz_pixmap *) js_touserdata(J, idx, "fz_pixmap");
}

#if FZ_ENABLE_PDF

static void ffi_pushobj(js_State *J, pdf_obj *obj);

static void ffi_gc_pdf_annot(js_State *J, void *annot)
{
	fz_context *ctx = js_getcontext(J);
	pdf_drop_annot(ctx, annot);
}

static void ffi_gc_pdf_document(js_State *J, void *doc)
{
	fz_context *ctx = js_getcontext(J);
	pdf_drop_document(ctx, doc);
}

static void ffi_gc_pdf_obj(js_State *J, void *obj)
{
	fz_context *ctx = js_getcontext(J);
	pdf_drop_obj(ctx, obj);
}

static void ffi_gc_pdf_graft_map(js_State *J, void *map)
{
	fz_context *ctx = js_getcontext(J);
	pdf_drop_graft_map(ctx, map);
}

static void ffi_gc_pdf_pkcs7_signer(js_State *J, void *signer_)
{
	fz_context *ctx = js_getcontext(J);
	pdf_pkcs7_signer *signer = (pdf_pkcs7_signer *)signer_;
	if (signer)
		signer->drop(ctx, signer);
}

static fz_document *ffi_todocument(js_State *J, int idx)
{
	if (js_isuserdata(J, idx, "pdf_document"))
		return js_touserdata(J, idx, "pdf_document");
	return js_touserdata(J, idx, "fz_document");
}

static void ffi_pushdocument(js_State *J, fz_document *document)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdocument = pdf_document_from_fz_document(ctx, document);
	if (pdocument) {
		js_getregistry(J, "pdf_document");
		js_newuserdata(J, "pdf_document", document, ffi_gc_pdf_document);
	} else {
		js_getregistry(J, "fz_document");
		js_newuserdata(J, "fz_document", document, ffi_gc_fz_document);
	}
}

static void ffi_pushsigner(js_State *J, pdf_pkcs7_signer *signer)
{
	js_getregistry(J, "pdf_pkcs7_signer");
	js_newuserdata(J, "pdf_pkcs7_signer", signer, ffi_gc_pdf_pkcs7_signer);
}

static fz_page *ffi_topage(js_State *J, int idx)
{
	if (js_isuserdata(J, idx, "pdf_page"))
		return js_touserdata(J, idx, "pdf_page");
	return js_touserdata(J, idx, "fz_page");
}

static pdf_annot *ffi_toannot(js_State *J, int idx)
{
	if (js_isuserdata(J, idx, "pdf_widget"))
		return js_touserdata(J, idx, "pdf_widget");
	return js_touserdata(J, idx, "pdf_annot");
}

static void ffi_pushpage(js_State *J, fz_page *page)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *ppage = pdf_page_from_fz_page(ctx, page);
	if (ppage) {
		js_getregistry(J, "pdf_page");
		js_newuserdata(J, "pdf_page", page, ffi_gc_fz_page);
	} else {
		js_getregistry(J, "fz_page");
		js_newuserdata(J, "fz_page", page, ffi_gc_fz_page);
	}
}

#else

static fz_document *ffi_todocument(js_State *J, int idx)
{
	return js_touserdata(J, idx, "fz_document");
}

static void ffi_pushdocument(js_State *J, fz_document *document)
{
	js_getregistry(J, "fz_document");
	js_newuserdata(J, "fz_document", document, ffi_gc_fz_document);
}

static fz_page *ffi_topage(js_State *J, int idx)
{
	return js_touserdata(J, idx, "fz_page");
}

static void ffi_pushpage(js_State *J, fz_page *page)
{
	js_getregistry(J, "fz_page");
	js_newuserdata(J, "fz_page", page, ffi_gc_fz_page);
}

#endif /* FZ_ENABLE_PDF */

/* type conversions */

struct color {
	fz_colorspace *colorspace;
	float color[FZ_MAX_COLORS];
	float alpha;
};

static fz_matrix ffi_tomatrix(js_State *J, int idx)
{
	if (js_iscoercible(J, idx))
	{
		fz_matrix matrix;
		js_getindex(J, idx, 0); matrix.a = js_tonumber(J, -1); js_pop(J, 1);
		js_getindex(J, idx, 1); matrix.b = js_tonumber(J, -1); js_pop(J, 1);
		js_getindex(J, idx, 2); matrix.c = js_tonumber(J, -1); js_pop(J, 1);
		js_getindex(J, idx, 3); matrix.d = js_tonumber(J, -1); js_pop(J, 1);
		js_getindex(J, idx, 4); matrix.e = js_tonumber(J, -1); js_pop(J, 1);
		js_getindex(J, idx, 5); matrix.f = js_tonumber(J, -1); js_pop(J, 1);
		return matrix;
	}
	return fz_identity;
}

static void ffi_pushmatrix(js_State *J, fz_matrix matrix)
{
	js_newarray(J);
	js_pushnumber(J, matrix.a); js_setindex(J, -2, 0);
	js_pushnumber(J, matrix.b); js_setindex(J, -2, 1);
	js_pushnumber(J, matrix.c); js_setindex(J, -2, 2);
	js_pushnumber(J, matrix.d); js_setindex(J, -2, 3);
	js_pushnumber(J, matrix.e); js_setindex(J, -2, 4);
	js_pushnumber(J, matrix.f); js_setindex(J, -2, 5);
}

static fz_point ffi_topoint(js_State *J, int idx)
{
	fz_point point;
	js_getindex(J, idx, 0); point.x = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 1); point.y = js_tonumber(J, -1); js_pop(J, 1);
	return point;
}

static void ffi_pushpoint(js_State *J, fz_point point)
{
	js_newarray(J);
	js_pushnumber(J, point.x); js_setindex(J, -2, 0);
	js_pushnumber(J, point.y); js_setindex(J, -2, 1);
}

static fz_rect ffi_torect(js_State *J, int idx)
{
	fz_rect rect;
	js_getindex(J, idx, 0); rect.x0 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 1); rect.y0 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 2); rect.x1 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 3); rect.y1 = js_tonumber(J, -1); js_pop(J, 1);
	return rect;
}

static void ffi_pushrect(js_State *J, fz_rect rect)
{
	js_newarray(J);
	js_pushnumber(J, rect.x0); js_setindex(J, -2, 0);
	js_pushnumber(J, rect.y0); js_setindex(J, -2, 1);
	js_pushnumber(J, rect.x1); js_setindex(J, -2, 2);
	js_pushnumber(J, rect.y1); js_setindex(J, -2, 3);
}

#if FZ_ENABLE_PDF

static fz_quad ffi_toquad(js_State *J, int idx)
{
	fz_quad quad;
	js_getindex(J, idx, 0); quad.ul.x = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 1); quad.ul.y = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 2); quad.ur.x = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 3); quad.ur.y = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 4); quad.ll.x = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 5); quad.ll.y = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 6); quad.lr.x = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 7); quad.lr.y = js_tonumber(J, -1); js_pop(J, 1);
	return quad;
}

#endif /* FZ_ENABLE_PDF */

static void ffi_pushquad(js_State *J, fz_quad quad)
{
	js_newarray(J);
	js_pushnumber(J, quad.ul.x); js_setindex(J, -2, 0);
	js_pushnumber(J, quad.ul.y); js_setindex(J, -2, 1);
	js_pushnumber(J, quad.ur.x); js_setindex(J, -2, 2);
	js_pushnumber(J, quad.ur.y); js_setindex(J, -2, 3);
	js_pushnumber(J, quad.ll.x); js_setindex(J, -2, 4);
	js_pushnumber(J, quad.ll.y); js_setindex(J, -2, 5);
	js_pushnumber(J, quad.lr.x); js_setindex(J, -2, 6);
	js_pushnumber(J, quad.lr.y); js_setindex(J, -2, 7);
}

static fz_irect ffi_toirect(js_State *J, int idx)
{
	fz_irect irect;
	js_getindex(J, idx, 0); irect.x0 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 1); irect.y0 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 2); irect.x1 = js_tonumber(J, -1); js_pop(J, 1);
	js_getindex(J, idx, 3); irect.y1 = js_tonumber(J, -1); js_pop(J, 1);
	return irect;
}

static void ffi_pusharray(js_State *J, const float *v, int n)
{
	int i;
	js_newarray(J);
	for (i = 0; i < n; ++i) {
		js_pushnumber(J, v[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_pushcolorspace(js_State *J, fz_colorspace *colorspace)
{
	fz_context *ctx = js_getcontext(J);
	if (colorspace == NULL)
		js_pushnull(J);
	else if (colorspace == fz_device_rgb(ctx))
		js_getregistry(J, "DeviceRGB");
	else if (colorspace == fz_device_bgr(ctx))
		js_getregistry(J, "DeviceBGR");
	else if (colorspace == fz_device_gray(ctx))
		js_getregistry(J, "DeviceGray");
	else if (colorspace == fz_device_cmyk(ctx))
		js_getregistry(J, "DeviceCMYK");
	else if (colorspace == fz_device_lab(ctx))
		js_getregistry(J, "DeviceLab");
	else {
		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, colorspace), ffi_gc_fz_colorspace);
	}
}

static void ffi_pushcolor(js_State *J, fz_colorspace *colorspace, const float *color, float alpha)
{
	fz_context *ctx = js_getcontext(J);
	if (colorspace) {
		ffi_pushcolorspace(J, colorspace);
		ffi_pusharray(J, color, fz_colorspace_n(ctx, colorspace));
	} else {
		js_pushnull(J);
		js_pushnull(J);
	}
	js_pushnumber(J, alpha);
}

static struct color ffi_tocolor(js_State *J, int idx)
{
	struct color c;
	int n, i;
	fz_context *ctx = js_getcontext(J);
	c.colorspace = js_touserdata(J, idx, "fz_colorspace");
	if (c.colorspace) {
		n = fz_colorspace_n(ctx, c.colorspace);
		for (i=0; i < n; ++i) {
			js_getindex(J, idx + 1, i);
			c.color[i] = js_tonumber(J, -1);
			js_pop(J, 1);
		}
	}
	c.alpha = js_tonumber(J, idx + 2);
	return c;
}

static const char *string_from_ri(uint8_t ri)
{
	switch (ri) {
	default:
	case 0: return "Perceptual";
	case 1: return "RelativeColorimetric";
	case 2: return "Saturation";
	case 3: return "AbsoluteColorimetric";
	}
}

static void ffi_pushcolorparams(js_State *J, fz_color_params color_params)
{
	js_newobject(J);
	js_pushstring(J, string_from_ri(color_params.ri));
	js_setproperty(J, -2, "renderingIntent");
	js_pushboolean(J, color_params.bp);
	js_setproperty(J, -2, "blackPointCompensation");
	js_pushboolean(J, color_params.op);
	js_setproperty(J, -2, "overPrinting");
	js_pushboolean(J, color_params.opm);
	js_setproperty(J, -2, "overPrintMode");
}

static fz_color_params ffi_tocolorparams(js_State *J, int idx)
{
	fz_color_params color_params = { 0 };

	if (!js_iscoercible(J, idx))
		return fz_default_color_params;

	if (js_hasproperty(J, idx, "renderingIntent"))
	{
		js_getproperty(J, idx, "renderingIntent");
		color_params.ri = js_tointeger(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "blackPointCompensation"))
	{
		js_getproperty(J, idx, "blackPointCompensation");
		color_params.ri = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "overPrinting"))
	{
		js_getproperty(J, idx, "overPrinting");
		color_params.ri = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "overPrintMode"))
	{
		js_getproperty(J, idx, "overPrintMode");
		color_params.ri = js_toboolean(J, -1);
		js_pop(J, 1);
	}

	return color_params;
}

static void ffi_pushdefaultcolorspaces(js_State *J, fz_default_colorspaces *default_cs)
{
	js_getregistry(J, "fz_default_colorspaces");
	js_newuserdata(J, "fz_default_colorspaces", default_cs, ffi_gc_fz_default_colorspaces);
}

static fz_default_colorspaces *ffi_todefaultcolorspaces(js_State *J, int idx)
{
	return (fz_default_colorspaces *) js_touserdata(J, idx, "fz_default_colorspaces");
}

static struct {
	int flag;
	const char *name;
} render_flags[] = {
	{ FZ_DEVFLAG_MASK, "mask" },
	{ FZ_DEVFLAG_COLOR, "color" },
	{ FZ_DEVFLAG_UNCACHEABLE, "uncacheable" },
	{ FZ_DEVFLAG_FILLCOLOR_UNDEFINED, "fillcolor-undefined" },
	{ FZ_DEVFLAG_STROKECOLOR_UNDEFINED, "strokecolor-undefined" },
	{ FZ_DEVFLAG_STARTCAP_UNDEFINED, "startcap-undefined" },
	{ FZ_DEVFLAG_DASHCAP_UNDEFINED, "dashcap-undefined" },
	{ FZ_DEVFLAG_ENDCAP_UNDEFINED, "endcap-undefined" },
	{ FZ_DEVFLAG_LINEJOIN_UNDEFINED, "linejoin-undefined" },
	{ FZ_DEVFLAG_MITERLIMIT_UNDEFINED, "miterlimit-undefined" },
	{ FZ_DEVFLAG_LINEWIDTH_UNDEFINED, "linewidth-undefined" },
	{ FZ_DEVFLAG_BBOX_DEFINED, "bbox-defined" },
	{ FZ_DEVFLAG_GRIDFIT_AS_TILED, "gridfit-as-tiled" },
};

static void ffi_pushrenderflags(js_State *J, int flags)
{
	js_newarray(J);
	int idx = 0;
	size_t i;
	for (i = 0; i < nelem(render_flags); ++i)
	{
		if (flags & render_flags[i].flag)
		{
			js_pushstring(J, render_flags[i].name);
			js_setindex(J, -2, idx++);
		}
	}
}

static int ffi_torenderflags(js_State *J, int idx)
{
	int flags = 0;
	const char *name;
	int i, n = js_getlength(J, idx);
	size_t k;
	for (i = 0; i < n; ++i) {
		js_getindex(J, idx, i);
		name = js_tostring(J, -1);
		for (k = 0; k < nelem(render_flags); ++k)
			if (!strcmp(name, render_flags[k].name))
				flags |= render_flags[k].flag;
		js_pop(J, 1);
	}
	return flags;
}

static const char *string_from_metatext(fz_metatext meta_text)
{
	switch (meta_text) {
	default:
	case FZ_METATEXT_ACTUALTEXT: return "ActualText";
	case FZ_METATEXT_ALT: return "Alt";
	case FZ_METATEXT_ABBREVIATION: return "Abbreviation";
	case FZ_METATEXT_TITLE: return "Title";
	}
}

static fz_metatext metatext_from_string(const char *str)
{
	if (!strcmp(str, "ActualText")) return FZ_METATEXT_ACTUALTEXT;
	if (!strcmp(str, "Alt")) return FZ_METATEXT_ALT;
	if (!strcmp(str, "Abbreviation")) return FZ_METATEXT_ABBREVIATION;
	if (!strcmp(str, "Title")) return FZ_METATEXT_TITLE;
	return FZ_METATEXT_ACTUALTEXT;
}

static fz_link_dest_type link_dest_type_from_string(const char *str);

static fz_link_dest ffi_tolinkdest(js_State *J, int idx)
{
	fz_link_dest dest = fz_make_link_dest_none();

	if (js_hasproperty(J, idx, "chapter")) {
		dest.loc.chapter = js_tointeger(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "page")) {
		dest.loc.page = js_tointeger(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "type")) {
		dest.type = link_dest_type_from_string(js_tostring(J, -1));
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "x")) {
		dest.x = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "y")) {
		dest.y = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "width")) {
		dest.w = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "height")) {
		dest.h = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "zoom")) {
		dest.zoom = js_tonumber(J, -1);
		js_pop(J, 1);
	}

	return dest;
}

static fz_outline_item ffi_tooutlineitem(js_State *J, int idx)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_item item = { NULL, NULL, 0 };

	if (js_hasproperty(J, idx, "title")) {
		if (js_iscoercible(J, -1)) {
			const char *title = js_tostring(J, -1);
			fz_try(ctx)
				item.title = fz_strdup(ctx, title);
			fz_always(ctx)
				js_pop(J, 1);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		else
			item.title = NULL;

	}
	if (js_hasproperty(J, idx, "open")) {
		item.is_open = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "uri")) {
		if (js_iscoercible(J, -1)) {
			const char *uri = js_tostring(J, -1);
			fz_try(ctx)
				item.uri = fz_strdup(ctx, uri);
			fz_always(ctx)
				js_pop(J, 1);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		else
			item.uri = NULL;
	}

	return item;
}

static const char *string_from_cap(fz_linecap cap)
{
	switch (cap) {
	default:
	case FZ_LINECAP_BUTT: return "Butt";
	case FZ_LINECAP_ROUND: return "Round";
	case FZ_LINECAP_SQUARE: return "Square";
	case FZ_LINECAP_TRIANGLE: return "Triangle";
	}
}

static const char *string_from_join(fz_linejoin join)
{
	switch (join) {
	default:
	case FZ_LINEJOIN_MITER: return "Miter";
	case FZ_LINEJOIN_ROUND: return "Round";
	case FZ_LINEJOIN_BEVEL: return "Bevel";
	case FZ_LINEJOIN_MITER_XPS: return "MiterXPS";
	}
}

#if FZ_ENABLE_PDF

static const char *string_from_border_style(enum pdf_border_style style)
{
	switch (style) {
	default:
	case PDF_BORDER_STYLE_SOLID: return "Solid";
	case PDF_BORDER_STYLE_DASHED: return "Dashed";
	case PDF_BORDER_STYLE_BEVELED: return "Beveled";
	case PDF_BORDER_STYLE_INSET: return "Inset";
	case PDF_BORDER_STYLE_UNDERLINE: return "Underline";
	}
}

static const char *string_from_border_effect(enum pdf_border_effect effect)
{
	switch (effect) {
	default:
	case PDF_BORDER_EFFECT_NONE: return "None";
	case PDF_BORDER_EFFECT_CLOUDY: return "Cloudy";
	}
}

static const char *string_from_line_ending(enum pdf_line_ending style)
{
	switch (style) {
	default:
	case PDF_ANNOT_LE_NONE: return "None";
	case PDF_ANNOT_LE_SQUARE: return "Square";
	case PDF_ANNOT_LE_CIRCLE: return "Circle";
	case PDF_ANNOT_LE_DIAMOND: return "Diamond";
	case PDF_ANNOT_LE_OPEN_ARROW: return "OpenArrow";
	case PDF_ANNOT_LE_CLOSED_ARROW: return "ClosedArrow";
	case PDF_ANNOT_LE_BUTT: return "Butt";
	case PDF_ANNOT_LE_R_OPEN_ARROW: return "ROpenArrow";
	case PDF_ANNOT_LE_R_CLOSED_ARROW: return "RClosedArrow";
	case PDF_ANNOT_LE_SLASH: return "Slash";
	}
}

#endif

static const char *string_from_destination_type(fz_link_dest_type type)
{
	switch (type) {
	default:
	case FZ_LINK_DEST_FIT: return "Fit";
	case FZ_LINK_DEST_XYZ: return "XYZ";
	case FZ_LINK_DEST_FIT_H: return "FitH";
	case FZ_LINK_DEST_FIT_V: return "FitV";
	case FZ_LINK_DEST_FIT_R: return "FitR";
	case FZ_LINK_DEST_FIT_B: return "FitB";
	case FZ_LINK_DEST_FIT_BH: return "FitBH";
	case FZ_LINK_DEST_FIT_BV: return "FitBV";
	}
}

static fz_linecap cap_from_string(const char *str)
{
	if (!strcmp(str, "Round")) return FZ_LINECAP_ROUND;
	if (!strcmp(str, "Square")) return FZ_LINECAP_SQUARE;
	if (!strcmp(str, "Triangle")) return FZ_LINECAP_TRIANGLE;
	return FZ_LINECAP_BUTT;
}

static fz_linejoin join_from_string(const char *str)
{
	if (!strcmp(str, "Round")) return FZ_LINEJOIN_ROUND;
	if (!strcmp(str, "Bevel")) return FZ_LINEJOIN_BEVEL;
	if (!strcmp(str, "MiterXPS")) return FZ_LINEJOIN_MITER_XPS;
	return FZ_LINEJOIN_MITER;
}

#if FZ_ENABLE_PDF

static enum pdf_border_style border_style_from_string(const char *str)
{
	if (!strcmp(str, "Solid")) return PDF_BORDER_STYLE_SOLID;
	if (!strcmp(str, "Dashed")) return PDF_BORDER_STYLE_DASHED;
	if (!strcmp(str, "Beveled")) return PDF_BORDER_STYLE_INSET;
	if (!strcmp(str, "Inset")) return PDF_BORDER_STYLE_INSET;
	if (!strcmp(str, "Underline")) return PDF_BORDER_STYLE_UNDERLINE;
	return PDF_BORDER_STYLE_SOLID;
}

static enum pdf_border_effect border_effect_from_string(const char *str)
{
	if (!strcmp(str, "None")) return PDF_BORDER_EFFECT_NONE;
	if (!strcmp(str, "Cloudy")) return PDF_BORDER_EFFECT_CLOUDY;
	return PDF_BORDER_EFFECT_NONE;
}

static enum pdf_line_ending line_ending_from_string(const char *str)
{
	if (!strcmp(str, "None")) return PDF_ANNOT_LE_NONE;
	if (!strcmp(str, "Square")) return PDF_ANNOT_LE_SQUARE;
	if (!strcmp(str, "Circle")) return PDF_ANNOT_LE_CIRCLE;
	if (!strcmp(str, "Diamond")) return PDF_ANNOT_LE_DIAMOND;
	if (!strcmp(str, "OpenArrow")) return PDF_ANNOT_LE_OPEN_ARROW;
	if (!strcmp(str, "ClosedArrow")) return PDF_ANNOT_LE_CLOSED_ARROW;
	if (!strcmp(str, "Butt")) return PDF_ANNOT_LE_BUTT;
	if (!strcmp(str, "ROpenArrow")) return PDF_ANNOT_LE_R_OPEN_ARROW;
	if (!strcmp(str, "RClosedArrow")) return PDF_ANNOT_LE_R_CLOSED_ARROW;
	if (!strcmp(str, "Slash")) return PDF_ANNOT_LE_SLASH;
	return PDF_ANNOT_LE_NONE;
}

#endif

static fz_link_dest_type link_dest_type_from_string(const char *str)
{
	if (!strcmp(str, "XYZ")) return FZ_LINK_DEST_XYZ;
	if (!strcmp(str, "Fit")) return FZ_LINK_DEST_FIT;
	if (!strcmp(str, "FitH")) return FZ_LINK_DEST_FIT_H;
	if (!strcmp(str, "FitV")) return FZ_LINK_DEST_FIT_V;
	if (!strcmp(str, "FitR")) return FZ_LINK_DEST_FIT_R;
	if (!strcmp(str, "FitB")) return FZ_LINK_DEST_FIT_B;
	if (!strcmp(str, "FitBH")) return FZ_LINK_DEST_FIT_BH;
	if (!strcmp(str, "FitBV")) return FZ_LINK_DEST_FIT_BV;
	return FZ_LINK_DEST_FIT;
}

static void ffi_gc_fz_link(js_State *J, void *link)
{
	fz_context *ctx = js_getcontext(J);
	fz_drop_link(ctx, link);
}

static fz_link *ffi_tolink(js_State *J, int idx)
{
	return js_touserdata(J, idx, "fz_link");
}

static void ffi_pushlink(js_State *J, fz_link *link)
{
	js_getregistry(J, "fz_link");
	js_newuserdata(J, "fz_link", link, ffi_gc_fz_link);
}

static void ffi_pushlinkdest(js_State *J, const fz_link_dest dest)
{
	js_newobject(J);

	js_pushnumber(J, dest.loc.chapter);
	js_setproperty(J, -2, "chapter");
	js_pushnumber(J, dest.loc.page);
	js_setproperty(J, -2, "page");

	js_pushliteral(J, string_from_destination_type(dest.type));
	js_setproperty(J, -2, "type");

	switch (dest.type)
	{
	default:
	case FZ_LINK_DEST_FIT:
	case FZ_LINK_DEST_FIT_B:
		break;
	case FZ_LINK_DEST_FIT_H:
	case FZ_LINK_DEST_FIT_BH:
		js_pushnumber(J, dest.y);
		js_setproperty(J, -2, "y");
		break;
	case FZ_LINK_DEST_FIT_V:
	case FZ_LINK_DEST_FIT_BV:
		js_pushnumber(J, dest.x);
		js_setproperty(J, -2, "x");
		break;
	case FZ_LINK_DEST_XYZ:
		js_pushnumber(J, dest.x);
		js_setproperty(J, -2, "x");
		js_pushnumber(J, dest.y);
		js_setproperty(J, -2, "y");
		js_pushnumber(J, dest.zoom);
		js_setproperty(J, -2, "zoom");
		break;
	case FZ_LINK_DEST_FIT_R:
		js_pushnumber(J, dest.x);
		js_setproperty(J, -2, "x");
		js_pushnumber(J, dest.y);
		js_setproperty(J, -2, "y");
		js_pushnumber(J, dest.w);
		js_setproperty(J, -2, "width");
		js_pushnumber(J, dest.h);
		js_setproperty(J, -2, "height");
		break;
	}
}

static void ffi_pushstroke(js_State *J, const fz_stroke_state *stroke)
{
	js_newobject(J);
	js_pushliteral(J, string_from_cap(stroke->start_cap));
	js_setproperty(J, -2, "startCap");
	js_pushliteral(J, string_from_cap(stroke->dash_cap));
	js_setproperty(J, -2, "dashCap");
	js_pushliteral(J, string_from_cap(stroke->end_cap));
	js_setproperty(J, -2, "endCap");
	js_pushliteral(J, string_from_join(stroke->linejoin));
	js_setproperty(J, -2, "lineJoin");
	js_pushnumber(J, stroke->linewidth);
	js_setproperty(J, -2, "lineWidth");
	js_pushnumber(J, stroke->miterlimit);
	js_setproperty(J, -2, "miterLimit");
	js_pushnumber(J, stroke->dash_phase);
	js_setproperty(J, -2, "dashPhase");
	ffi_pusharray(J, stroke->dash_list, stroke->dash_len);
	js_setproperty(J, -2, "dashes");
}

static fz_stroke_state ffi_tostroke(js_State *J, int idx)
{
	fz_stroke_state stroke = fz_default_stroke_state;
	if (js_hasproperty(J, idx, "lineCap")) {
		stroke.start_cap = cap_from_string(js_tostring(J, -1));
		stroke.dash_cap = stroke.start_cap;
		stroke.end_cap = stroke.start_cap;
	}
	if (js_hasproperty(J, idx, "startCap")) {
		stroke.start_cap = cap_from_string(js_tostring(J, -1));
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "dashCap")) {
		stroke.dash_cap = cap_from_string(js_tostring(J, -1));
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "endCap")) {
		stroke.end_cap = cap_from_string(js_tostring(J, -1));
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "lineJoin")) {
		stroke.linejoin = join_from_string(js_tostring(J, -1));
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "lineWidth")) {
		stroke.linewidth = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "miterLimit")) {
		stroke.miterlimit = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "dashPhase")) {
		stroke.dash_phase = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	if (js_hasproperty(J, idx, "dashes")) {
		int i, n = js_getlength(J, -1);
		if (n > (int)nelem(stroke.dash_list))
			n = nelem(stroke.dash_list);
		stroke.dash_len = n;
		for (i = 0; i < n; ++i) {
			js_getindex(J, -1, i);
			stroke.dash_list[i] = js_tonumber(J, -1);
			js_pop(J, 1);
		}
	}
	return stroke;
}

static void ffi_pushtext(js_State *J, const fz_text *text)
{
	fz_context *ctx = js_getcontext(J);
	js_getregistry(J, "fz_text");
	js_newuserdata(J, "fz_text", fz_keep_text(ctx, text), ffi_gc_fz_text);
}

static void ffi_pushpath(js_State *J, const fz_path *path)
{
	fz_context *ctx = js_getcontext(J);
	js_getregistry(J, "fz_path");
	js_newuserdata(J, "fz_path", fz_keep_path(ctx, path), ffi_gc_fz_path);
}

static void ffi_pushfont(js_State *J, fz_font *font)
{
	fz_context *ctx = js_getcontext(J);
	js_getregistry(J, "fz_font");
	js_newuserdata(J, "fz_font", fz_keep_font(ctx, font), ffi_gc_fz_font);
}

static void ffi_pushshade(js_State *J, fz_shade *shade)
{
	fz_context *ctx = js_getcontext(J);
	js_getregistry(J, "fz_shade");
	js_newuserdata(J, "fz_shade", fz_keep_shade(ctx, shade), ffi_gc_fz_shade);
}

static void ffi_pushimage(js_State *J, fz_image *image)
{
	fz_context *ctx = js_getcontext(J);
	js_getregistry(J, "fz_image");
	js_newuserdata(J, "fz_image", fz_keep_image(ctx, image), ffi_gc_fz_image);
}

static void ffi_pushimage_own(js_State *J, fz_image *image)
{
	js_getregistry(J, "fz_image");
	js_newuserdata(J, "fz_image", image, ffi_gc_fz_image);
}

static int is_number(const char *key, int *idx)
{
	char *end;
	*idx = strtol(key, &end, 10);
	return *end == 0;
}

static fz_archive *ffi_toarchive(js_State *J, int idx)
{
	if (js_isuserdata(J, idx, "fz_tree_archive"))
		return js_touserdata(J, idx, "fz_tree_archive");
	if (js_isuserdata(J, idx, "fz_multi_archive"))
		return js_touserdata(J, idx, "fz_multi_archive");
	return js_touserdata(J, idx, "fz_archive");
}

static void ffi_pusharchive(js_State *J, fz_archive *arch)
{
	js_getregistry(J, "fz_archive");
	js_newuserdata(J, "fz_archive", arch, ffi_gc_fz_archive);
}

static void ffi_pushmultiarchive(js_State *J, fz_archive *arch)
{
	js_getregistry(J, "fz_multi_archive");
	js_newuserdata(J, "fz_multi_archive", arch, ffi_gc_fz_archive);
}

static void ffi_pushtreearchive(js_State *J, fz_archive *arch)
{
	js_getregistry(J, "fz_tree_archive");
	js_newuserdata(J, "fz_tree_archive", arch, ffi_gc_fz_archive);
}

static int ffi_buffer_has(js_State *J, void *buf_, const char *key)
{
	fz_buffer *buf = buf_;
	int idx;
	unsigned char *data;
	size_t len = fz_buffer_storage(js_getcontext(J), buf, &data);
	if (is_number(key, &idx)) {
		if (idx < 0 || (size_t)idx >= len)
			js_rangeerror(J, "index out of bounds");
		js_pushnumber(J, data[idx]);
		return 1;
	}
	if (!strcmp(key, "length")) {
		js_pushnumber(J, len);
		return 1;
	}
	return 0;
}

static int ffi_buffer_put(js_State *J, void *buf_, const char *key)
{
	fz_buffer *buf = buf_;
	int idx;
	unsigned char *data;
	size_t len = fz_buffer_storage(js_getcontext(J), buf, &data);
	if (is_number(key, &idx)) {
		if (idx < 0 || (size_t)idx >= len)
			js_rangeerror(J, "index out of bounds");
		data[idx] = js_tonumber(J, -1);
		return 1;
	}
	if (!strcmp(key, "length"))
		js_typeerror(J, "buffer length is read-only");
	return 0;
}

static void ffi_pushbuffer(js_State *J, fz_buffer *buf)
{
	js_getregistry(J, "fz_buffer");
	js_newuserdatax(J, "fz_buffer", buf,
			ffi_buffer_has, ffi_buffer_put, NULL,
			ffi_gc_fz_buffer);
}

static fz_buffer *ffi_tobuffer(js_State *J, int idx)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = NULL;

	if (js_isuserdata(J, idx, "fz_buffer"))
		buf = fz_keep_buffer(ctx, js_touserdata(J, idx, "fz_buffer"));
	else if (!js_iscoercible(J, idx)) {
		fz_try(ctx)
			buf = fz_new_buffer(ctx, 1);
		fz_catch(ctx)
			rethrow(J);
	}
	else {
		const char *str = js_tostring(J, idx);
		fz_try(ctx)
			buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)str, strlen(str));
		fz_catch(ctx)
			rethrow(J);
	}

	return buf;
}

/* device calling into js from c */

typedef struct
{
	fz_device super;
	js_State *J;
} js_device;

static void
js_dev_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "fillPath")) {
		js_copy(J, -2);
		ffi_pushpath(J, path);
		js_pushboolean(J, even_odd);
		ffi_pushmatrix(J, ctm);
		ffi_pushcolor(J, colorspace, color, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 7);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_rect scissor)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "clipPath")) {
		js_copy(J, -2);
		ffi_pushpath(J, path);
		js_pushboolean(J, even_odd);
		ffi_pushmatrix(J, ctm);
		js_call(J, 3);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path,
	const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "strokePath")) {
		js_copy(J, -2);
		ffi_pushpath(J, path);
		ffi_pushstroke(J, stroke);
		ffi_pushmatrix(J, ctm);
		ffi_pushcolor(J, colorspace, color, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 7);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_rect scissor)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "clipStrokePath")) {
		js_copy(J, -2);
		ffi_pushpath(J, path);
		ffi_pushstroke(J, stroke);
		ffi_pushmatrix(J, ctm);
		js_call(J, 3);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "fillText")) {
		js_copy(J, -2);
		ffi_pushtext(J, text);
		ffi_pushmatrix(J, ctm);
		ffi_pushcolor(J, colorspace, color, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 6);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "strokeText")) {
		js_copy(J, -2);
		ffi_pushtext(J, text);
		ffi_pushstroke(J, stroke);
		ffi_pushmatrix(J, ctm);
		ffi_pushcolor(J, colorspace, color, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 7);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "clipText")) {
		js_copy(J, -2);
		ffi_pushtext(J, text);
		ffi_pushmatrix(J, ctm);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_rect scissor)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "clipStrokeText")) {
		js_copy(J, -2);
		ffi_pushtext(J, text);
		ffi_pushstroke(J, stroke);
		ffi_pushmatrix(J, ctm);
		js_call(J, 3);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "ignoreText")) {
		js_copy(J, -2);
		ffi_pushtext(J, text);
		ffi_pushmatrix(J, ctm);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "fillShade")) {
		js_copy(J, -2);
		ffi_pushshade(J, shade);
		ffi_pushmatrix(J, ctm);
		js_pushnumber(J, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 4);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "fillImage")) {
		js_copy(J, -2);
		ffi_pushimage(J, image);
		ffi_pushmatrix(J, ctm);
		js_pushnumber(J, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 4);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "fillImageMask")) {
		js_copy(J, -2);
		ffi_pushimage(J, image);
		ffi_pushmatrix(J, ctm);
		ffi_pushcolor(J, colorspace, color, alpha);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 6);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "clipImageMask")) {
		js_copy(J, -2);
		ffi_pushimage(J, image);
		ffi_pushmatrix(J, ctm);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_pop_clip(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "popClip")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_begin_mask(fz_context *ctx, fz_device *dev, fz_rect bbox, int luminosity,
	fz_colorspace *colorspace, const float *color, fz_color_params color_params)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginMask")) {
		js_copy(J, -2);
		ffi_pushrect(J, bbox);
		js_pushboolean(J, luminosity);
		ffi_pushcolor(J, colorspace, color, 1);
		ffi_pushcolorparams(J, color_params);
		js_call(J, 6);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_end_mask(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endMask")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_begin_group(fz_context *ctx, fz_device *dev, fz_rect bbox,
	fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginGroup")) {
		js_copy(J, -2);
		ffi_pushrect(J, bbox);
		js_pushboolean(J, isolated);
		js_pushboolean(J, knockout);
		js_pushliteral(J, fz_blendmode_name(blendmode));
		js_pushnumber(J, alpha);
		js_call(J, 5);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_end_group(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endGroup")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static int
js_dev_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view,
	float xstep, float ystep, fz_matrix ctm, int id)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginTile")) {
		int n;
		js_copy(J, -2);
		ffi_pushrect(J, area);
		ffi_pushrect(J, view);
		js_pushnumber(J, xstep);
		js_pushnumber(J, ystep);
		ffi_pushmatrix(J, ctm);
		js_pushnumber(J, id);
		js_call(J, 6);
		n = js_tointeger(J, -1);
		js_pop(J, 1);
		return n;
	}
	js_endtry(J);
	return 0;
}

static void
js_dev_end_tile(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endTile")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "renderFlags")) {
		js_copy(J, -2);
		ffi_pushrenderflags(J, set);
		ffi_pushrenderflags(J, clear);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *default_cs)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "setDefaultColorSpaces")) {
		js_copy(J, -2);
		ffi_pushdefaultcolorspaces(J, fz_keep_default_colorspaces(ctx, default_cs));
		js_call(J, 1);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_begin_layer(fz_context *ctx, fz_device *dev, const char *name)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginLayer")) {
		js_copy(J, -2);
		js_pushstring(J, name);
		js_call(J, 1);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_end_layer(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endLayer")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_begin_structure(fz_context *ctx, fz_device *dev, fz_structure standard, const char *raw, int uid)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginStructure")) {
		js_copy(J, -2);
		js_pushstring(J, fz_structure_to_string(standard));
		js_pushstring(J, raw);
		js_pushnumber(J, uid);
		js_call(J, 3);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_end_structure(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endStructure")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *text)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "beginMetatext")) {
		js_copy(J, -2);
		js_pushstring(J, string_from_metatext(meta));
		js_pushstring(J, text);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void
js_dev_end_metatext(fz_context *ctx, fz_device *dev)
{
	js_State *J = ((js_device*)dev)->J;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, -1, "endMetatext")) {
		js_copy(J, -2);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static fz_device *new_js_device(fz_context *ctx, js_State *J)
{
	js_device *dev = fz_new_derived_device(ctx, js_device);

	dev->super.fill_path = js_dev_fill_path;
	dev->super.stroke_path = js_dev_stroke_path;
	dev->super.clip_path = js_dev_clip_path;
	dev->super.clip_stroke_path = js_dev_clip_stroke_path;

	dev->super.fill_text = js_dev_fill_text;
	dev->super.stroke_text = js_dev_stroke_text;
	dev->super.clip_text = js_dev_clip_text;
	dev->super.clip_stroke_text = js_dev_clip_stroke_text;
	dev->super.ignore_text = js_dev_ignore_text;

	dev->super.fill_shade = js_dev_fill_shade;
	dev->super.fill_image = js_dev_fill_image;
	dev->super.fill_image_mask = js_dev_fill_image_mask;
	dev->super.clip_image_mask = js_dev_clip_image_mask;

	dev->super.pop_clip = js_dev_pop_clip;

	dev->super.begin_mask = js_dev_begin_mask;
	dev->super.end_mask = js_dev_end_mask;
	dev->super.begin_group = js_dev_begin_group;
	dev->super.end_group = js_dev_end_group;

	dev->super.begin_tile = js_dev_begin_tile;
	dev->super.end_tile = js_dev_end_tile;

	dev->super.render_flags = js_dev_render_flags;
	dev->super.set_default_colorspaces = js_dev_set_default_colorspaces;

	dev->super.begin_layer = js_dev_begin_layer;
	dev->super.end_layer = js_dev_end_layer;

	dev->super.begin_structure = js_dev_begin_structure;
	dev->super.end_structure = js_dev_end_structure;

	dev->super.begin_metatext = js_dev_begin_metatext;
	dev->super.end_metatext = js_dev_end_metatext;

	dev->J = J;
	return (fz_device*)dev;
}

/* PDF operator processor */

#if FZ_ENABLE_PDF

typedef struct resources_stack
{
	struct resources_stack *next;
	pdf_obj *resources;
} resources_stack;

typedef struct
{
	pdf_processor super;
	js_State *J;
	resources_stack *rstack;
	int extgstate;
} pdf_js_processor;

#define PROC_BEGIN(OP) \
	{ js_State *J = ((pdf_js_processor*)proc)->J; \
	if (js_try(J)) \
		rethrow_as_fz(J); \
	if (js_hasproperty(J, 1, OP)) { \
		js_copy(J, 1);

#define PROC_END(N) \
		js_call(J, N); \
		js_pop(J, 1); \
	} \
	js_endtry(J); }

static void js_proc_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_w");
		js_pushnumber(J, linewidth);
		PROC_END(1);
	}
}

static void js_proc_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_j");
		js_pushnumber(J, linejoin);
		PROC_END(1);
	}
}

static void js_proc_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_J");
		js_pushnumber(J, linecap);
		PROC_END(1);
	}
}

static void js_proc_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_M");
		js_pushnumber(J, miterlimit);
		PROC_END(1);
	}
}

static void js_proc_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	int i, n = pdf_array_len(ctx, array);
	PROC_BEGIN("op_d");
	{
		js_newarray(J);
		for (i = 0; i < n; ++i)
		{
			/* we know the array only holds numbers and strings, so we are safe from exceptions here */
			js_pushnumber(J, pdf_array_get_real(ctx, array, i));
			js_setindex(J, -2, i);
		}
		js_pushnumber(J, phase);
	}
	PROC_END(2);
}

static void js_proc_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_ri");
		js_pushstring(J, intent);
		PROC_END(1);
	}
}

static void js_proc_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_i");
		js_pushnumber(J, flatness);
		PROC_END(1);
	}
}

static void js_proc_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	((pdf_js_processor*)proc)->extgstate = 1;
	PROC_BEGIN("op_gs");
	js_pushstring(J, name);
	ffi_pushobj(J, pdf_keep_obj(ctx, extgstate));
	PROC_END(2);
}

static void js_proc_gs_end(fz_context *ctx, pdf_processor *proc)
{
	((pdf_js_processor*)proc)->extgstate = 0;
}

static void js_proc_q(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_q");
	PROC_END(0);
}

static void js_proc_Q(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_Q");
	PROC_END(0);
}

static void js_proc_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PROC_BEGIN("op_cm");
	js_pushnumber(J, a);
	js_pushnumber(J, b);
	js_pushnumber(J, c);
	js_pushnumber(J, d);
	js_pushnumber(J, e);
	js_pushnumber(J, f);
	PROC_END(6);
}

static void js_proc_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PROC_BEGIN("op_m");
	js_pushnumber(J, x);
	js_pushnumber(J, y);
	PROC_END(2);
}

static void js_proc_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PROC_BEGIN("op_l");
	js_pushnumber(J, x);
	js_pushnumber(J, y);
	PROC_END(2);
}

static void js_proc_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	PROC_BEGIN("op_c");
	js_pushnumber(J, x1);
	js_pushnumber(J, y1);
	js_pushnumber(J, x2);
	js_pushnumber(J, y2);
	js_pushnumber(J, x3);
	js_pushnumber(J, y3);
	PROC_END(6);
}

static void js_proc_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	PROC_BEGIN("op_v");
	js_pushnumber(J, x2);
	js_pushnumber(J, y2);
	js_pushnumber(J, x3);
	js_pushnumber(J, y3);
	PROC_END(4);
}

static void js_proc_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	PROC_BEGIN("op_y");
	js_pushnumber(J, x1);
	js_pushnumber(J, y1);
	js_pushnumber(J, x3);
	js_pushnumber(J, y3);
	PROC_END(4);
}

static void js_proc_h(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_h");
	PROC_END(0);
}

static void js_proc_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	PROC_BEGIN("op_re");
	js_pushnumber(J, x);
	js_pushnumber(J, y);
	js_pushnumber(J, w);
	js_pushnumber(J, h);
	PROC_END(4);
}

static void js_proc_S(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_S");
	PROC_END(0);
}

static void js_proc_s(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_s");
	PROC_END(0);
}

static void js_proc_F(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_F");
	PROC_END(0);
}

static void js_proc_f(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_f");
	PROC_END(0);
}

static void js_proc_fstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_fstar");
	PROC_END(0);
}

static void js_proc_B(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_B");
	PROC_END(0);
}

static void js_proc_Bstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_Bstar");
	PROC_END(0);
}

static void js_proc_b(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_b");
	PROC_END(0);
}

static void js_proc_bstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_bstar");
	PROC_END(0);
}

static void js_proc_n(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_n");
	PROC_END(0);
}

static void js_proc_W(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_W");
	PROC_END(0);
}

static void js_proc_Wstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_Wstar");
	PROC_END(0);
}

static void js_proc_BT(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_BT");
	PROC_END(0);
}

static void js_proc_ET(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_ET");
	PROC_END(0);
}

static void js_proc_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	PROC_BEGIN("op_Tc");
	js_pushnumber(J, charspace);
	PROC_END(1);
}

static void js_proc_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	PROC_BEGIN("op_Tw");
	js_pushnumber(J, wordspace);
	PROC_END(1);
}

static void js_proc_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	PROC_BEGIN("op_Tz");
	js_pushnumber(J, scale);
	PROC_END(1);
}

static void js_proc_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	PROC_BEGIN("op_TL");
	js_pushnumber(J, leading);
	PROC_END(1);
}

static void js_proc_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	if (!((pdf_js_processor*)proc)->extgstate)
	{
		PROC_BEGIN("op_Tf");
		js_pushstring(J, name);
		js_pushnumber(J, size);
		PROC_END(2);
	}
}

static void js_proc_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	PROC_BEGIN("op_Tr");
	js_pushnumber(J, render);
	PROC_END(1);
}

static void js_proc_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	PROC_BEGIN("op_Ts");
	js_pushnumber(J, rise);
	PROC_END(1);
}

static void js_proc_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PROC_BEGIN("op_Td");
	js_pushnumber(J, tx);
	js_pushnumber(J, ty);
	PROC_END(2);
}

static void js_proc_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PROC_BEGIN("op_TD");
	js_pushnumber(J, tx);
	js_pushnumber(J, ty);
	PROC_END(2);
}

static void js_proc_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PROC_BEGIN("op_Tm");
	js_pushnumber(J, a);
	js_pushnumber(J, b);
	js_pushnumber(J, c);
	js_pushnumber(J, d);
	js_pushnumber(J, e);
	js_pushnumber(J, f);
	PROC_END(6);
}

static void js_proc_Tstar(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_Tstar");
	PROC_END(0);
}

static void push_byte_string(js_State *J, unsigned char *str, size_t len)
{
	size_t i, is_ascii = 1;
	for (i = 0; i < len; ++i)
		if (str[i] == 0 || str[i] > 127)
			is_ascii = 0;
	if (is_ascii)
		js_pushstring(J, (char*)str);
	else
	{
		js_newarray(J);
		for (i = 0; i < len; ++i)
		{
			js_pushnumber(J, str[i]);
			js_setindex(J, -2, (int)i);
		}
	}
}

static void js_proc_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	int i, n = pdf_array_len(ctx, array);
	pdf_obj *obj;
	PROC_BEGIN("op_TJ");
	{
		/* we know the array only holds numbers and strings, so we are safe from exceptions here */
		js_newarray(J);
		for (i = 0; i < n; ++i)
		{
			obj = pdf_array_get(ctx, array, i);
			if (pdf_is_number(ctx, obj))
				js_pushnumber(J, pdf_to_real(ctx, obj));
			else
			{
				push_byte_string(J, (unsigned char *)pdf_to_str_buf(ctx, obj), pdf_to_str_len(ctx, obj));
			}
			js_setindex(J, -2, i);
		}
	}
	PROC_END(1);
}

static void js_proc_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PROC_BEGIN("op_Tj");
	push_byte_string(J, (unsigned char *)str, len);
	PROC_END(1);
}

static void js_proc_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PROC_BEGIN("op_squote");
	push_byte_string(J, (unsigned char *)str, len);
	PROC_END(1);
}

static void js_proc_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	PROC_BEGIN("op_dquote");
	js_pushnumber(J, aw);
	js_pushnumber(J, ac);
	push_byte_string(J, (unsigned char *)str, len);
	PROC_END(1);
}

static void js_proc_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	PROC_BEGIN("op_d0");
	js_pushnumber(J, wx);
	js_pushnumber(J, wy);
	PROC_END(2);
}

static void js_proc_d1(fz_context *ctx, pdf_processor *proc,
	float wx, float wy, float llx, float lly, float urx, float ury)
{
	PROC_BEGIN("op_d1");
	js_pushnumber(J, wx);
	js_pushnumber(J, wy);
	js_pushnumber(J, llx);
	js_pushnumber(J, lly);
	js_pushnumber(J, urx);
	js_pushnumber(J, ury);
	PROC_END(6);
}

static void js_proc_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PROC_BEGIN("op_CS");
	js_pushstring(J, name);
	ffi_pushcolorspace(J, cs);
	PROC_END(2);
}

static void js_proc_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PROC_BEGIN("op_cs");
	js_pushstring(J, name);
	ffi_pushcolorspace(J, cs);
	PROC_END(2);
}

static void js_proc_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	int i;
	PROC_BEGIN("op_SC_pattern");
	js_pushstring(J, name);
	js_pushnumber(J, pat->id); /* TODO: pdf_obj instead! */
	js_newarray(J);
	for (i = 0; i < n; ++i)
	{
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
	PROC_END(3);
}

static void js_proc_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	int i;
	PROC_BEGIN("op_sc_pattern");
	js_pushstring(J, name);
	js_pushnumber(J, pat->id); /* TODO: pdf_obj instead! */
	js_newarray(J);
	for (i = 0; i < n; ++i)
	{
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
	PROC_END(3);
}

static void js_proc_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("op_SC_shade");
	js_pushstring(J, name);
	ffi_pushshade(J, shade);
	PROC_END(2);
}

static void js_proc_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("op_sc_shade");
	js_pushstring(J, name);
	ffi_pushshade(J, shade);
	PROC_END(2);
}

static void js_proc_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	int i;
	PROC_BEGIN("op_SC_color");
	js_newarray(J);
	for (i = 0; i < n; ++i)
	{
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
	PROC_END(1);
}

static void js_proc_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	int i;
	PROC_BEGIN("op_sc_color");
	js_newarray(J);
	for (i = 0; i < n; ++i)
	{
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
	PROC_END(1);
}

static void js_proc_G(fz_context *ctx, pdf_processor *proc, float g)
{
	PROC_BEGIN("op_G");
	js_pushnumber(J, g);
	PROC_END(1);
}

static void js_proc_g(fz_context *ctx, pdf_processor *proc, float g)
{
	PROC_BEGIN("op_g");
	js_pushnumber(J, g);
	PROC_END(1);
}

static void js_proc_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PROC_BEGIN("op_RG");
	js_pushnumber(J, r);
	js_pushnumber(J, g);
	js_pushnumber(J, b);
	PROC_END(3);
}

static void js_proc_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PROC_BEGIN("op_rg");
	js_pushnumber(J, r);
	js_pushnumber(J, g);
	js_pushnumber(J, b);
	PROC_END(3);
}

static void js_proc_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PROC_BEGIN("op_K");
	js_pushnumber(J, c);
	js_pushnumber(J, m);
	js_pushnumber(J, y);
	js_pushnumber(J, k);
	PROC_END(4);
}

static void js_proc_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PROC_BEGIN("op_k");
	js_pushnumber(J, c);
	js_pushnumber(J, m);
	js_pushnumber(J, y);
	js_pushnumber(J, k);
	PROC_END(4);
}

static void js_proc_BI(fz_context *ctx, pdf_processor *proc, fz_image *img, const char *colorspace)
{
	PROC_BEGIN("op_BI");
	ffi_pushimage(J, img);
	js_pushstring(J, colorspace);
	PROC_END(2);
}

static void js_proc_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PROC_BEGIN("op_sh");
	js_pushstring(J, name);
	ffi_pushshade(J, shade);
	PROC_END(2);
}

static void js_proc_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	PROC_BEGIN("op_Do_image");
	js_pushstring(J, name);
	ffi_pushimage(J, image);
	PROC_END(2);
}

static void js_proc_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	PROC_BEGIN("op_Do_form");
	js_pushstring(J, name);
	ffi_pushobj(J, pdf_keep_obj(ctx, xobj));
	ffi_pushobj(J, pdf_keep_obj(ctx, ((pdf_js_processor*)proc)->rstack->resources));
	PROC_END(3);
}

static void js_proc_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PROC_BEGIN("op_MP");
	js_pushstring(J, tag);
	PROC_END(1);
}

static void js_proc_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PROC_BEGIN("op_DP");
	js_pushstring(J, tag);
	ffi_pushobj(J, pdf_keep_obj(ctx, raw));
	PROC_END(2);
}

static void js_proc_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PROC_BEGIN("op_BMC");
	js_pushstring(J, tag);
	PROC_END(1);
}

static void js_proc_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PROC_BEGIN("op_BDC");
	js_pushstring(J, tag);
	ffi_pushobj(J, pdf_keep_obj(ctx, raw));
	PROC_END(2);
}

static void js_proc_EMC(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_EMC");
	PROC_END(0);
}

static void js_proc_BX(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_BX");
	PROC_END(0);
}

static void js_proc_EX(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("op_EX");
	PROC_END(0);
}

static void js_proc_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	PROC_BEGIN("push_resources");
	ffi_pushobj(J, pdf_keep_obj(ctx, res));
	PROC_END(1);
}

static pdf_obj *js_proc_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	PROC_BEGIN("pop_resources");
	PROC_END(0);
	return NULL;
}

static void js_proc_drop(fz_context *ctx, pdf_processor *proc)
{
	pdf_js_processor *pr = (pdf_js_processor *)proc;

	while (pr->rstack)
	{
		resources_stack *stk = pr->rstack;
		pr->rstack = stk->next;
		pdf_drop_obj(ctx, stk->resources);
		fz_free(ctx, stk);
	}
}

static pdf_processor *new_js_processor(fz_context *ctx, js_State *J)
{
	pdf_js_processor *proc = pdf_new_processor(ctx, sizeof *proc);

	proc->super.close_processor = NULL;
	proc->super.drop_processor = js_proc_drop;

	proc->super.push_resources = js_proc_push_resources;
	proc->super.pop_resources = js_proc_pop_resources;

	/* general graphics state */
	proc->super.op_w = js_proc_w;
	proc->super.op_j = js_proc_j;
	proc->super.op_J = js_proc_J;
	proc->super.op_M = js_proc_M;
	proc->super.op_d = js_proc_d;
	proc->super.op_ri = js_proc_ri;
	proc->super.op_i = js_proc_i;
	proc->super.op_gs_begin = js_proc_gs_begin;
	proc->super.op_gs_end = js_proc_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = NULL;
	proc->super.op_gs_CA = NULL;
	proc->super.op_gs_ca = NULL;
	proc->super.op_gs_SMask = NULL;

	/* special graphics state */
	proc->super.op_q = js_proc_q;
	proc->super.op_Q = js_proc_Q;
	proc->super.op_cm = js_proc_cm;

	/* path construction */
	proc->super.op_m = js_proc_m;
	proc->super.op_l = js_proc_l;
	proc->super.op_c = js_proc_c;
	proc->super.op_v = js_proc_v;
	proc->super.op_y = js_proc_y;
	proc->super.op_h = js_proc_h;
	proc->super.op_re = js_proc_re;

	/* path painting */
	proc->super.op_S = js_proc_S;
	proc->super.op_s = js_proc_s;
	proc->super.op_F = js_proc_F;
	proc->super.op_f = js_proc_f;
	proc->super.op_fstar = js_proc_fstar;
	proc->super.op_B = js_proc_B;
	proc->super.op_Bstar = js_proc_Bstar;
	proc->super.op_b = js_proc_b;
	proc->super.op_bstar = js_proc_bstar;
	proc->super.op_n = js_proc_n;

	/* clipping paths */
	proc->super.op_W = js_proc_W;
	proc->super.op_Wstar = js_proc_Wstar;

	/* text objects */
	proc->super.op_BT = js_proc_BT;
	proc->super.op_ET = js_proc_ET;

	/* text state */
	proc->super.op_Tc = js_proc_Tc;
	proc->super.op_Tw = js_proc_Tw;
	proc->super.op_Tz = js_proc_Tz;
	proc->super.op_TL = js_proc_TL;
	proc->super.op_Tf = js_proc_Tf;
	proc->super.op_Tr = js_proc_Tr;
	proc->super.op_Ts = js_proc_Ts;

	/* text positioning */
	proc->super.op_Td = js_proc_Td;
	proc->super.op_TD = js_proc_TD;
	proc->super.op_Tm = js_proc_Tm;
	proc->super.op_Tstar = js_proc_Tstar;

	/* text showing */
	proc->super.op_TJ = js_proc_TJ;
	proc->super.op_Tj = js_proc_Tj;
	proc->super.op_squote = js_proc_squote;
	proc->super.op_dquote = js_proc_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = js_proc_d0;
	proc->super.op_d1 = js_proc_d1;

	/* color */
	proc->super.op_CS = js_proc_CS;
	proc->super.op_cs = js_proc_cs;
	proc->super.op_SC_color = js_proc_SC_color;
	proc->super.op_sc_color = js_proc_sc_color;
	proc->super.op_SC_pattern = js_proc_SC_pattern;
	proc->super.op_sc_pattern = js_proc_sc_pattern;
	proc->super.op_SC_shade = js_proc_SC_shade;
	proc->super.op_sc_shade = js_proc_sc_shade;

	proc->super.op_G = js_proc_G;
	proc->super.op_g = js_proc_g;
	proc->super.op_RG = js_proc_RG;
	proc->super.op_rg = js_proc_rg;
	proc->super.op_K = js_proc_K;
	proc->super.op_k = js_proc_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = js_proc_BI;
	proc->super.op_sh = js_proc_sh;
	proc->super.op_Do_image = js_proc_Do_image;
	proc->super.op_Do_form = js_proc_Do_form;

	/* marked content */
	proc->super.op_MP = js_proc_MP;
	proc->super.op_DP = js_proc_DP;
	proc->super.op_BMC = js_proc_BMC;
	proc->super.op_BDC = js_proc_BDC;
	proc->super.op_EMC = js_proc_EMC;

	/* compatibility */
	proc->super.op_BX = js_proc_BX;
	proc->super.op_EX = js_proc_EX;

	/* extgstate */
	proc->super.op_gs_OP = NULL;
	proc->super.op_gs_op = NULL;
	proc->super.op_gs_OPM = NULL;
	proc->super.op_gs_UseBlackPtComp = NULL;

	proc->J = J;

	return (pdf_processor*)proc;
}

#endif /* FZ_ENABLE_PDF */

/* device calling into c from js */

static void ffi_Device_close(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_close_device(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_fillPath(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_path *path = js_touserdata(J, 1, "fz_path");
	int even_odd = js_toboolean(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	struct color c = ffi_tocolor(J, 4);
	fz_color_params color_params = ffi_tocolorparams(J, 7);
	fz_try(ctx)
		fz_fill_path(ctx, dev, path, even_odd, ctm, c.colorspace, c.color, c.alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_strokePath(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_path *path = js_touserdata(J, 1, "fz_path");
	fz_stroke_state stroke = ffi_tostroke(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	struct color c = ffi_tocolor(J, 4);
	fz_color_params color_params = ffi_tocolorparams(J, 7);
	fz_try(ctx)
		fz_stroke_path(ctx, dev, path, &stroke, ctm, c.colorspace, c.color, c.alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_clipPath(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_path *path = js_touserdata(J, 1, "fz_path");
	int even_odd = js_toboolean(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	fz_try(ctx)
		fz_clip_path(ctx, dev, path, even_odd, ctm, fz_infinite_rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_clipStrokePath(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_path *path = js_touserdata(J, 1, "fz_path");
	fz_stroke_state stroke = ffi_tostroke(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	fz_try(ctx)
		fz_clip_stroke_path(ctx, dev, path, &stroke, ctm, fz_infinite_rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_fillText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_text *text = js_touserdata(J, 1, "fz_text");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	struct color c = ffi_tocolor(J, 3);
	fz_color_params color_params = ffi_tocolorparams(J, 6);
	fz_try(ctx)
		fz_fill_text(ctx, dev, text, ctm, c.colorspace, c.color, c.alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_strokeText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_text *text = js_touserdata(J, 1, "fz_text");
	fz_stroke_state stroke = ffi_tostroke(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	struct color c = ffi_tocolor(J, 4);
	fz_color_params color_params = ffi_tocolorparams(J, 7);
	fz_try(ctx)
		fz_stroke_text(ctx, dev, text, &stroke, ctm, c.colorspace, c.color, c.alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_clipText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_text *text = js_touserdata(J, 1, "fz_text");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	fz_try(ctx)
		fz_clip_text(ctx, dev, text, ctm, fz_infinite_rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_clipStrokeText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_text *text = js_touserdata(J, 1, "fz_text");
	fz_stroke_state stroke = ffi_tostroke(J, 2);
	fz_matrix ctm = ffi_tomatrix(J, 3);
	fz_try(ctx)
		fz_clip_stroke_text(ctx, dev, text, &stroke, ctm, fz_infinite_rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_ignoreText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_text *text = js_touserdata(J, 1, "fz_text");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	fz_try(ctx)
		fz_ignore_text(ctx, dev, text, ctm);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_fillShade(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_shade *shade = js_touserdata(J, 1, "fz_shade");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	float alpha = js_tonumber(J, 3);
	fz_color_params color_params = ffi_tocolorparams(J, 4);
	fz_try(ctx)
		fz_fill_shade(ctx, dev, shade, ctm, alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_fillImage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_image *image = js_touserdata(J, 1, "fz_image");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	float alpha = js_tonumber(J, 3);
	fz_color_params color_params = ffi_tocolorparams(J, 4);
	fz_try(ctx)
		fz_fill_image(ctx, dev, image, ctm, alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_fillImageMask(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_image *image = js_touserdata(J, 1, "fz_image");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	struct color c = ffi_tocolor(J, 3);
	fz_color_params color_params = ffi_tocolorparams(J, 6);
	fz_try(ctx)
		fz_fill_image_mask(ctx, dev, image, ctm, c.colorspace, c.color, c.alpha, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_clipImageMask(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_image *image = js_touserdata(J, 1, "fz_image");
	fz_matrix ctm = ffi_tomatrix(J, 2);
	fz_try(ctx)
		fz_clip_image_mask(ctx, dev, image, ctm, fz_infinite_rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_popClip(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_pop_clip(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginMask(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_rect area = ffi_torect(J, 1);
	int luminosity = js_toboolean(J, 2);
	struct color c = ffi_tocolor(J, 3);
	fz_color_params color_params = ffi_tocolorparams(J, 6);
	fz_try(ctx)
		fz_begin_mask(ctx, dev, area, luminosity, c.colorspace, c.color, color_params);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_endMask(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_mask(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginGroup(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_rect area = ffi_torect(J, 1);
	int isolated = js_toboolean(J, 2);
	int knockout = js_toboolean(J, 3);
	int blendmode = fz_lookup_blendmode(js_tostring(J, 4));
	float alpha = js_tonumber(J, 5);
	fz_try(ctx)
		fz_begin_group(ctx, dev, area, NULL, isolated, knockout, blendmode, alpha);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_endGroup(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_group(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginTile(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_rect area = ffi_torect(J, 1);
	fz_rect view = ffi_torect(J, 2);
	float xstep = js_tonumber(J, 3);
	float ystep = js_tonumber(J, 4);
	fz_matrix ctm = ffi_tomatrix(J, 5);
	int id = js_tonumber(J, 6);
	int n = 0;
	fz_try(ctx)
		n = fz_begin_tile_id(ctx, dev, area, view, xstep, ystep, ctm, id);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, n);
}

static void ffi_Device_endTile(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_tile(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginLayer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	const char *name = js_tostring(J, 1);
	fz_try(ctx)
		fz_begin_layer(ctx, dev, name);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_endLayer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_layer(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_renderFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	int set = ffi_torenderflags(J, 1);
	int clear = ffi_torenderflags(J, 2);
	fz_try(ctx)
		fz_render_flags(ctx, dev, set, clear);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_setDefaultColorSpaces(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 1);
	fz_try(ctx)
		fz_set_default_colorspaces(ctx, dev, default_cs);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginStructure(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_structure str = js_iscoercible(J, 1) ? fz_structure_from_string(js_tostring(J, 1)) : FZ_STRUCTURE_INVALID;
	const char *raw = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	int uid = js_tointeger(J, 3);

	fz_try(ctx)
		fz_begin_structure(ctx, dev, str, raw, uid);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_endStructure(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_structure(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_beginMetatext(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_metatext meta = metatext_from_string(js_tostring(J, 1));
	const char *meta_text = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;

	fz_try(ctx)
		fz_begin_metatext(ctx, dev, meta, meta_text);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Device_endMetatext(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_device *dev = js_touserdata(J, 0, "fz_device");
	fz_try(ctx)
		fz_end_metatext(ctx, dev);
	fz_catch(ctx)
		rethrow(J);
}

/* mupdf module */

static void ffi_enableICC(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_try(ctx)
		fz_enable_icc(ctx);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_disableICC(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_try(ctx)
		fz_disable_icc(ctx);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_readFile(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *filename = js_tostring(J, 1);
	fz_buffer *buf = NULL;
	fz_try(ctx)
		buf = fz_read_file(ctx, filename);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushbuffer(J, buf);
}

static void ffi_setUserCSS(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *user_css = js_tostring(J, 1);
	int use_doc_css = js_iscoercible(J, 2) ? js_toboolean(J, 2) : 1;
	fz_try(ctx) {
		fz_set_user_css(ctx, user_css);
		fz_set_use_document_css(ctx, use_doc_css);
	} fz_catch(ctx)
		rethrow(J);
}

static void ffi_new_Archive(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *path = js_tostring(J, 1);
	fz_archive *arch = NULL;
	fz_try(ctx)
		if (fz_is_directory(ctx, path))
			arch = fz_open_directory(ctx, path);
		else
			arch = fz_open_archive(ctx, path);
	fz_catch(ctx)
		rethrow(J);
	ffi_pusharchive(J, arch);
}

static void ffi_Archive_getFormat(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = ffi_toarchive(J, 0);
	const char *format = NULL;
	fz_try(ctx)
		format = fz_archive_format(ctx, arch);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, format);
}

static void ffi_Archive_countEntries(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = ffi_toarchive(J, 0);
	int count = 0;
	fz_try(ctx)
		count = fz_count_archive_entries(ctx, arch);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, count);
}

static void ffi_Archive_listEntry(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = ffi_toarchive(J, 0);
	int idx = js_tointeger(J, 1);
	const char *name = NULL;
	fz_try(ctx)
		name = fz_list_archive_entry(ctx, arch, idx);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, name);
}

static void ffi_Archive_hasEntry(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = ffi_toarchive(J, 0);
	const char *name = js_tostring(J, 1);
	int has = 0;
	fz_try(ctx)
		has = fz_has_archive_entry(ctx, arch, name);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_Archive_readEntry(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = ffi_toarchive(J, 0);
	const char *name = js_tostring(J, 1);
	fz_buffer *buf = NULL;
	fz_try(ctx)
		buf = fz_read_archive_entry(ctx, arch, name);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushbuffer(J, buf);
}

static void ffi_new_MultiArchive(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = NULL;
	fz_try(ctx)
		arch = fz_new_multi_archive(ctx);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushmultiarchive(J, arch);
}

static void ffi_new_TreeArchive(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = NULL;
	fz_try(ctx)
		arch = fz_new_tree_archive(ctx, NULL);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushtreearchive(J, arch);
}

static void ffi_MultiArchive_mountArchive(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = js_touserdata(J, 0, "fz_multi_archive");
	fz_archive *sub = ffi_toarchive(J, 1);
	const char *path = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	fz_try(ctx)
		fz_mount_multi_archive(ctx, arch, sub, path);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_TreeArchive_add(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_archive *arch = js_touserdata(J, 0, "fz_tree_archive");
	const char *name = js_tostring(J, 1);
	fz_buffer *buf = ffi_tobuffer(J, 2);
	fz_try(ctx)
		fz_tree_archive_add_buffer(ctx, arch, name, buf);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_new_Buffer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	int n = js_isdefined(J, 1) ? js_tonumber(J, 1) : 0;
	fz_buffer *buf = NULL;
	fz_try(ctx)
		buf = fz_new_buffer(ctx, n);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushbuffer(J, buf);
}

static void ffi_Buffer_writeByte(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	unsigned char val = js_tonumber(J, 1);
	fz_try(ctx)
		fz_append_byte(ctx, buf, val);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Buffer_writeRune(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	int val = js_tonumber(J, 1);
	fz_try(ctx)
		fz_append_rune(ctx, buf, val);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Buffer_write(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	int i, n = js_gettop(J);

	for (i = 1; i < n; ++i) {
		const char *s = js_tostring(J, i);
		fz_try(ctx) {
			if (i > 1)
				fz_append_byte(ctx, buf, ' ');
			fz_append_string(ctx, buf, s);
		} fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_Buffer_writeLine(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	ffi_Buffer_write(J);
	fz_try(ctx)
		fz_append_byte(ctx, buf, '\n');
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Buffer_writeBuffer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	fz_buffer *cat = js_touserdata(J, 1, "fz_buffer");
	fz_try(ctx)
		fz_append_buffer(ctx, buf, cat);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Buffer_save(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	const char *filename = js_tostring(J, 1);
	fz_try(ctx)
		fz_save_buffer(ctx, buf, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Buffer_slice(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = js_touserdata(J, 0, "fz_buffer");
	size_t size = fz_buffer_storage(ctx, buf, NULL);
	int64_t start = js_tointeger(J, 1);
	int64_t end = js_iscoercible(J, 2) ? js_tointeger(J, 2) : (int64_t) size;
	fz_buffer *copy = NULL;

	fz_try(ctx)
		copy = fz_slice_buffer(ctx, buf, start, end);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushbuffer(J, copy);
}

static void ffi_Document_openDocument(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = NULL;

	if (js_isuserdata(J, 1, "fz_buffer"))
	{
		const char *magic = js_tostring(J, 2);
		fz_buffer *buf = ffi_tobuffer(J, 1);
		fz_stream *stm = NULL;
		fz_var(stm);
		fz_try(ctx)
		{
			stm = fz_open_buffer(ctx, buf);
			doc = fz_open_document_with_stream(ctx, magic, stm);
		}
		fz_always(ctx)
		{
			fz_drop_stream(ctx, stm);
			fz_drop_buffer(ctx, buf);
		}
		fz_catch(ctx)
			rethrow(J);
	}
	else
	{
		const char *filename = js_tostring(J, 1);
		fz_try(ctx)
			doc = fz_open_document(ctx, filename);
		fz_catch(ctx)
			rethrow(J);
	}

	ffi_pushdocument(J, doc);
}

static void ffi_Document_isPDF(js_State *J)
{
	js_pushboolean(J, js_isuserdata(J, 0, "pdf_document"));
}

static void ffi_Document_formatLinkURI(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	fz_link_dest dest = ffi_tolinkdest(J, 1);
	char *uri = NULL;

	fz_try(ctx)
		uri = fz_format_link_uri(ctx, doc, dest);
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_free(ctx, uri);
		js_throw(J);
	}
	js_pushstring(J, uri);
	js_endtry(J);
	fz_free(ctx, uri);
}

static void ffi_Document_countPages(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	int count = 0;

	fz_try(ctx)
		count = fz_count_pages(ctx, doc);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, count);
}

static void ffi_Document_loadPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	int number = js_tointeger(J, 1);
	fz_page *page = NULL;

	fz_try(ctx)
		page = fz_load_page(ctx, doc, number);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpage(J, page);
}

static void ffi_Document_needsPassword(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	int b = 0;

	fz_try(ctx)
		b = fz_needs_password(ctx, doc);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, b);
}

static void ffi_Document_authenticatePassword(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *password = js_tostring(J, 1);
	int b = 0;

	fz_try(ctx)
		b = fz_authenticate_password(ctx, doc, password);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, b);
}

static void ffi_Document_hasPermission(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *perm = js_tostring(J, 1);
	int flag = 0;
	int result = 0;

	if (!strcmp(perm, "print")) flag = FZ_PERMISSION_PRINT;
	else if (!strcmp(perm, "edit")) flag = FZ_PERMISSION_EDIT;
	else if (!strcmp(perm, "copy")) flag = FZ_PERMISSION_COPY;
	else if (!strcmp(perm, "annotate")) flag = FZ_PERMISSION_ANNOTATE;
	else if (!strcmp(perm, "form")) flag = FZ_PERMISSION_FORM;
	else if (!strcmp(perm, "accessibility")) flag = FZ_PERMISSION_ACCESSIBILITY;
	else if (!strcmp(perm, "assemble")) flag = FZ_PERMISSION_ASSEMBLE;
	else if (!strcmp(perm, "print-hq")) flag = FZ_PERMISSION_PRINT_HQ;
	else js_error(J, "invalid permission name");

	fz_try(ctx)
		result = fz_has_permission(ctx, doc, flag);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, result);
}

static void ffi_Document_getMetaData(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *key = js_tostring(J, 1);
	char info[256];
	int found;

	fz_try(ctx)
		found = fz_lookup_metadata(ctx, doc, key, info, sizeof info);
	fz_catch(ctx)
		rethrow(J);

	if (found)
		js_pushstring(J, info);
}

static void ffi_Document_setMetaData(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *key = js_tostring(J, 1);
	const char *value = js_tostring(J, 2);
	fz_try(ctx)
		fz_set_metadata(ctx, doc, key, value);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Document_resolveLink(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *uri = js_tostring(J, 1);
	fz_location dest = fz_make_location(0, 0);

	fz_try(ctx)
		dest = fz_resolve_link(ctx, doc, uri, NULL, NULL);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, fz_page_number_from_location(ctx, doc, dest));
}

static void ffi_Document_resolveLinkDestination(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	const char *uri = js_tostring(J, 1);
	fz_link_dest dest = fz_make_link_dest_none();

	fz_try(ctx)
		dest = fz_resolve_link_dest(ctx, doc, uri);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushlinkdest(J, dest);
}

static void ffi_Document_isReflowable(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	int is_reflowable = 0;

	fz_try(ctx)
		is_reflowable = fz_is_document_reflowable(ctx, doc);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, is_reflowable);
}

static void ffi_Document_layout(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	float w = js_tonumber(J, 1);
	float h = js_tonumber(J, 2);
	float em = js_tonumber(J, 3);

	fz_try(ctx)
		fz_layout_document(ctx, doc, w, h, em);
	fz_catch(ctx)
		rethrow(J);
}

static void to_outline(js_State *J, fz_outline *outline)
{
	int i = 0;
	js_newarray(J);
	while (outline) {
		js_newobject(J);

		if (outline->title) {
			js_pushstring(J, outline->title);
			js_setproperty(J, -2, "title");
		}

		if (outline->uri) {
			js_pushstring(J, outline->uri);
			js_setproperty(J, -2, "uri");
		}

#if 0 /* FIXME: */
		if (outline->page >= 0) {
			js_pushnumber(J, outline->page);
			js_setproperty(J, -2, "page")
		}
#endif

		if (outline->down) {
			to_outline(J, outline->down);
			js_setproperty(J, -2, "down");
		}

		js_setindex(J, -2, i++);
		outline = outline->next;
	}
}

static void ffi_Document_loadOutline(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	fz_outline *outline = NULL;

	fz_try(ctx)
		outline = fz_load_outline(ctx, doc);
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_drop_outline(ctx, outline);
		js_throw(J);
	}

	to_outline(J, outline);

	js_endtry(J);
	fz_drop_outline(ctx, outline);
}

static void ffi_Document_outlineIterator(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document *doc = ffi_todocument(J, 0);
	fz_outline_iterator *iter = NULL;

	fz_try(ctx)
		iter = fz_new_outline_iterator(ctx, doc);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushoutlineiterator(J, iter);
}

static void ffi_OutlineIterator_item(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	fz_outline_item *item;

	fz_try(ctx)
		item = fz_outline_iterator_item(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	if (!item)
	{
		js_pushundefined(J);
		return;
	}

	js_newobject(J);

	if (item->title)
		js_pushliteral(J, item->title);
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "title");

	if (item->uri)
		js_pushliteral(J, item->uri);
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "uri");

	js_pushboolean(J, item->is_open);
	js_setproperty(J, -2, "open");
}

static void ffi_OutlineIterator_next(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_next(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_prev(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_prev(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_up(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_up(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_down(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_down(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_insert(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	fz_outline_item item = ffi_tooutlineitem(J, 1);
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_insert(ctx, iter, &item);
	fz_always(ctx)
	{
		fz_free(ctx, item.title);
		fz_free(ctx, item.uri);
	}
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_delete(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	int result;

	fz_try(ctx)
		result = fz_outline_iterator_delete(ctx, iter);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, result);
}

static void ffi_OutlineIterator_update(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_outline_iterator *iter = js_touserdata(J, 0, "fz_outline_iterator");
	fz_outline_item item = ffi_tooutlineitem(J, 1);

	fz_try(ctx)
		fz_outline_iterator_update(ctx, iter, &item);
	fz_always(ctx)
	{
		fz_free(ctx, item.title);
		fz_free(ctx, item.uri);
	}
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Page_isPDF(js_State *J)
{
	js_pushboolean(J, js_isuserdata(J, 0, "pdf_page"));
}

static void ffi_Page_getBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_rect bounds;

	fz_try(ctx)
		bounds = fz_bound_page(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushrect(J, bounds);
}

static void ffi_Page_run_imp(js_State *J, void(*run)(fz_context *, fz_page *, fz_device *, fz_matrix, fz_cookie *))
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_device *device = NULL;
	fz_matrix ctm = ffi_tomatrix(J, 2);

	if (js_isuserdata(J, 1, "fz_device")) {
		device = js_touserdata(J, 1, "fz_device");
		fz_try(ctx)
			run(ctx, page, device, ctm, NULL);
		fz_catch(ctx)
			rethrow(J);
	} else {
		device = new_js_device(ctx, J);
		js_copy(J, 1); /* put the js device on the top so the callbacks know where to get it */
		fz_try(ctx)
			run(ctx, page, device, ctm, NULL);
		fz_always(ctx)
			fz_drop_device(ctx, device);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_Page_run(js_State *J) {
	ffi_Page_run_imp(J, fz_run_page);
}

static void ffi_Page_runPageContents(js_State *J) {
	ffi_Page_run_imp(J, fz_run_page_contents);
}

static void ffi_Page_runPageAnnots(js_State *J) {
	ffi_Page_run_imp(J, fz_run_page_annots);
}

static void ffi_Page_runPageWidgets(js_State *J) {
	ffi_Page_run_imp(J, fz_run_page_widgets);
}

static void ffi_Page_toDisplayList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	int extra = js_isdefined(J, 1) ? js_toboolean(J, 1) : 1;
	fz_display_list *list = NULL;

	fz_try(ctx)
		if (extra)
			list = fz_new_display_list_from_page(ctx, page);
		else
			list = fz_new_display_list_from_page_contents(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_display_list");
	js_newuserdata(J, "fz_display_list", list, ffi_gc_fz_display_list);
}

static void ffi_Page_toPixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_colorspace *colorspace = js_touserdata(J, 2, "fz_colorspace");
	int alpha = js_toboolean(J, 3);
	int extra = js_isdefined(J, 4) ? js_toboolean(J, 4) : 1;
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		if (extra)
			pixmap = fz_new_pixmap_from_page(ctx, page, ctm, colorspace, alpha);
		else
			pixmap = fz_new_pixmap_from_page_contents(ctx, page, ctm, colorspace, alpha);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_Page_toStructuredText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	const char *options = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	fz_stext_options so;
	fz_stext_page *text = NULL;

	fz_try(ctx) {
		fz_parse_stext_options(ctx, &so, options);
		text = fz_new_stext_page_from_page(ctx, page, &so);
	}
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_stext_page");
	js_newuserdata(J, "fz_stext_page", text, ffi_gc_fz_stext_page);
}

static void ffi_pushsearch(js_State *J, int *marks, fz_quad *hits, int n)
{
	int a = 0;
	js_newarray(J);
	if (n > 0) {
		int i, k = 0;
		js_newarray(J);
		for (i = 0; i < n; ++i) {
			if (i > 0 && marks[i]) {
				js_setindex(J, -2, a++);
				js_newarray(J);
				k = 0;
			}
			ffi_pushquad(J, hits[i]);
			js_setindex(J, -2, k++);
		}
		js_setindex(J, -2, a);
	}
}

static void ffi_Page_search(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	const char *needle = js_tostring(J, 1);
	fz_quad hits[500];
	int marks[500];
	int n = 0;

	fz_try(ctx)
		n = fz_search_page(ctx, page, needle, marks, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	ffi_pushsearch(J, marks, hits, n);
}

static void ffi_Page_getLinks(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_link *link, *links = NULL;
	int i = 0;

	fz_try(ctx)
		links = fz_load_links(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_drop_link(ctx, links);
		js_throw(J);
	}

	js_newarray(J);
	for (link = links; link; link = link->next) {
		ffi_pushlink(J, fz_keep_link(ctx, link));
		js_setindex(J, -2, i++);
	}

	js_endtry(J);
	fz_drop_link(ctx, links);
}

static void ffi_Page_createLink(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_link *link = NULL;
	fz_rect rect = js_iscoercible(J, 1) ? ffi_torect(J, 1) : fz_empty_rect;
	const char *uri = js_iscoercible(J, 2) ? js_tostring(J, 2) : "";

	fz_try(ctx)
		link = fz_create_link(ctx, page, rect, uri);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushlink(J, link);
}

static void ffi_Page_deleteLink(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_link *link = ffi_tolink(J, 1);

	fz_try(ctx)
		fz_delete_link(ctx, page, link);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Page_getLabel(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	char buf[100];

	fz_try(ctx)
		fz_page_label(ctx, page, buf, sizeof buf);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, buf);
}

static void ffi_Link_getBounds(js_State *J)
{
	fz_link *link = js_touserdata(J, 0, "fz_link");
	ffi_pushrect(J, link->rect);
}

static void ffi_Link_setBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_link *link = js_touserdata(J, 0, "fz_link");
	fz_rect rect = ffi_torect(J, 1);

	fz_try(ctx)
		fz_set_link_rect(ctx, link, rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Link_getURI(js_State *J)
{
	fz_link *link = js_touserdata(J, 0, "fz_link");
	js_pushstring(J, link->uri);
}

static void ffi_Link_setURI(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_link *link = js_touserdata(J, 0, "fz_link");
	const char *uri = js_tostring(J, 1);

	fz_try(ctx)
		fz_set_link_uri(ctx, link, uri);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Link_isExternal(js_State *J)
{
	fz_link *link = js_touserdata(J, 0, "fz_link");
	fz_context *ctx = js_getcontext(J);
	int external = 0;

	fz_try(ctx)
		external = fz_is_external_link(ctx, link->uri);
	fz_catch(ctx)
		fz_rethrow(ctx);

	js_pushboolean(J, external);
}

static void ffi_ColorSpace_getNumberOfComponents(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushnumber(J, fz_colorspace_n(ctx, colorspace));
}

static void ffi_ColorSpace_getType(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	int t = fz_colorspace_type(ctx, colorspace);
	switch (t)
	{
	default:
	case FZ_COLORSPACE_NONE: js_pushstring(J, "None"); break;
	case FZ_COLORSPACE_GRAY: js_pushstring(J, "Gray"); break;
	case FZ_COLORSPACE_RGB: js_pushstring(J, "RGB"); break;
	case FZ_COLORSPACE_BGR: js_pushstring(J, "BGR"); break;
	case FZ_COLORSPACE_CMYK: js_pushstring(J, "CMYK"); break;
	case FZ_COLORSPACE_LAB: js_pushstring(J, "Lab"); break;
	case FZ_COLORSPACE_INDEXED: js_pushstring(J, "Indexed"); break;
	case FZ_COLORSPACE_SEPARATION: js_pushstring(J, "Separation"); break;
	}
}

static void ffi_ColorSpace_toString(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushstring(J, fz_colorspace_name(ctx, colorspace));
}

static void ffi_ColorSpace_isGray(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_gray(ctx, colorspace));
}

static void ffi_ColorSpace_isRGB(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_rgb(ctx, colorspace));
}

static void ffi_ColorSpace_isCMYK(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_cmyk(ctx, colorspace));
}

static void ffi_ColorSpace_isIndexed(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_indexed(ctx, colorspace));
}

static void ffi_ColorSpace_isLab(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_lab(ctx, colorspace));
}

static void ffi_ColorSpace_isDeviceN(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_device_n(ctx, colorspace));
}

static void ffi_ColorSpace_isSubtractive(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushboolean(J, fz_colorspace_is_subtractive(ctx, colorspace));
}

static void ffi_DefaultColorSpaces_getDefaultGray(js_State *J)
{
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	ffi_pushcolorspace(J, default_cs->gray);
}

static void ffi_DefaultColorSpaces_getDefaultRGB(js_State *J)
{
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	ffi_pushcolorspace(J, default_cs->rgb);
}

static void ffi_DefaultColorSpaces_getDefaultCMYK(js_State *J)
{
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	ffi_pushcolorspace(J, default_cs->cmyk);
}

static void ffi_DefaultColorSpaces_getOutputIntent(js_State *J)
{
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	ffi_pushcolorspace(J, default_cs->oi);
}

static void ffi_DefaultColorSpaces_setDefaultGray(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	fz_colorspace *cs = js_touserdata(J, 1, "fz_colorspace");
	fz_drop_colorspace(ctx, default_cs->gray);
	default_cs->gray = fz_keep_colorspace(ctx, cs);
}

static void ffi_DefaultColorSpaces_setDefaultRGB(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	fz_colorspace *cs = js_touserdata(J, 1, "fz_colorspace");
	fz_drop_colorspace(ctx, default_cs->rgb);
	default_cs->rgb = fz_keep_colorspace(ctx, cs);
}

static void ffi_DefaultColorSpaces_setDefaultCMYK(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	fz_colorspace *cs = js_touserdata(J, 1, "fz_colorspace");
	fz_drop_colorspace(ctx, default_cs->cmyk);
	default_cs->cmyk = fz_keep_colorspace(ctx, cs);
}

static void ffi_DefaultColorSpaces_setOutputIntent(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_default_colorspaces *default_cs = ffi_todefaultcolorspaces(J, 0);
	fz_colorspace *cs = js_touserdata(J, 1, "fz_colorspace");
	fz_drop_colorspace(ctx, default_cs->oi);
	default_cs->oi = fz_keep_colorspace(ctx, cs);
}

static void ffi_new_Pixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = NULL;

	if (js_isuserdata(J, 1, "fz_pixmap")) {
		fz_pixmap *pix = ffi_topixmap(J, 1);
		fz_pixmap *mask = ffi_topixmap(J, 2);

		fz_try(ctx)
			pixmap = fz_new_pixmap_from_color_and_mask(ctx, pix, mask);
		fz_catch(ctx)
			rethrow(J);
	} else {
		fz_colorspace *colorspace = js_touserdata(J, 1, "fz_colorspace");
		fz_irect bounds = ffi_toirect(J, 2);
		int alpha = js_toboolean(J, 3);

		fz_try(ctx)
			pixmap = fz_new_pixmap_with_bbox(ctx, colorspace, bounds, 0, alpha);
		fz_catch(ctx)
			rethrow(J);
	}

	ffi_pushpixmap(J, pixmap);
}

static void ffi_Pixmap_invert(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);

	fz_try(ctx)
		fz_invert_pixmap(ctx, pixmap);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_invertLuminance(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);

	fz_try(ctx)
		fz_invert_pixmap_luminance(ctx, pixmap);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_gamma(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	float gamma = js_tonumber(J, 1);

	fz_try(ctx)
		fz_gamma_pixmap(ctx, pixmap, gamma);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_tint(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	int black = js_tointeger(J, 1);
	int white = js_tointeger(J, 2);

	fz_try(ctx)
		fz_tint_pixmap(ctx, pixmap, black, white);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_warp(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	/* 1 = array of 8 floats for points */
	int w = js_tonumber(J, 2);
	int h = js_tonumber(J, 3);
	fz_pixmap *dest = NULL;
	fz_point points[4];
	int i;

	for (i = 0; i < 8; i++)
	{
		float *f = i&1 ? &points[i>>1].y : &points[i>>1].x;
		js_getindex(J, 1, i);
		*f = js_isdefined(J, -1) ? js_tonumber(J, -1) : 0;
		js_pop(J, 1);
	}

	fz_try(ctx)
		dest = fz_warp_pixmap(ctx, pixmap, points, w, h);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, dest);
}

static void ffi_Pixmap_asPNG(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	fz_buffer *buf = NULL;

	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_png(ctx, pixmap, fz_default_color_params);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushbuffer(J, buf);
}

static void ffi_Pixmap_saveAsPNG(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_png(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_saveAsJPEG(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);
	int quality = js_isdefined(J, 2) ? js_tointeger(J, 2) : 90;

	fz_try(ctx)
		fz_save_pixmap_as_jpeg(ctx, pixmap, filename, quality);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_saveAsPAM(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_pam(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_saveAsPNM(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_pnm(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_saveAsPBM(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_pbm(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_saveAsPKM(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_pkm(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_convertToColorSpace(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	fz_colorspace *cs = js_touserdata(J, 1, "fz_colorspace");
	fz_colorspace *proof = js_iscoercible(J, 2) ? js_touserdata(J, 2, "fz_colorspace") : NULL;
	fz_default_colorspaces *default_cs = js_iscoercible(J, 3) ? ffi_todefaultcolorspaces(J, 3) : NULL;
	fz_color_params color_params = ffi_tocolorparams(J, 4);
	int keep_alpha = js_isdefined(J, 5) ? js_toboolean(J, 5) : 0;
	fz_pixmap *dst = NULL;

	fz_try(ctx)
		dst = fz_convert_pixmap(ctx, pixmap, cs, proof, default_cs, color_params, keep_alpha);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, dst);
}

static void ffi_Pixmap_getBounds(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	fz_rect bounds;

	// fz_irect and fz_pixmap_bbox instead
	bounds.x0 = pixmap->x;
	bounds.y0 = pixmap->y;
	bounds.x1 = pixmap->x + pixmap->w;
	bounds.y1 = pixmap->y + pixmap->h;

	ffi_pushrect(J, bounds);
}

static void ffi_Pixmap_clear(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	if (js_isdefined(J, 1)) {
		int value = js_tonumber(J, 1);
		fz_try(ctx)
			fz_clear_pixmap_with_value(ctx, pixmap, value);
		fz_catch(ctx)
			rethrow(J);
	} else {
		fz_try(ctx)
			fz_clear_pixmap(ctx, pixmap);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_Pixmap_getX(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->x);
}

static void ffi_Pixmap_getY(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->y);
}

static void ffi_Pixmap_getWidth(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->w);
}

static void ffi_Pixmap_getHeight(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->h);
}

static void ffi_Pixmap_getNumberOfComponents(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->n);
}

static void ffi_Pixmap_getAlpha(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->alpha);
}

static void ffi_Pixmap_getStride(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->stride);
}

static void ffi_Pixmap_getSample(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	int x = js_tointeger(J, 1);
	int y = js_tointeger(J, 2);
	int k = js_tointeger(J, 3);
	if (x < 0 || x >= pixmap->w) js_rangeerror(J, "X out of range");
	if (y < 0 || y >= pixmap->h) js_rangeerror(J, "Y out of range");
	if (k < 0 || k >= pixmap->n) js_rangeerror(J, "N out of range");
	js_pushnumber(J, pixmap->samples[(x + y * pixmap->w) * pixmap->n + k]);
}

static void ffi_Pixmap_getXResolution(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->xres);
}

static void ffi_Pixmap_getYResolution(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	js_pushnumber(J, pixmap->yres);
}

static void ffi_Pixmap_getColorSpace(js_State *J)
{
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	ffi_pushcolorspace(J, pixmap->colorspace);
}

static void ffi_Pixmap_setResolution(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = ffi_topixmap(J, 0);
	int xres = js_tointeger(J, 1);
	int yres = js_tointeger(J, 2);

	fz_set_pixmap_resolution(ctx, pixmap, xres, yres);
}

static void ffi_new_Image(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_image *image = NULL;

	fz_image *mask = NULL;
	if (js_isuserdata(J, 2, "fz_image"))
		mask = js_touserdata(J, 2, "fz_image");

	if (js_isuserdata(J, 1, "fz_pixmap")) {
		fz_pixmap *pixmap = ffi_topixmap(J, 1);
		fz_try(ctx)
			image = fz_new_image_from_pixmap(ctx, pixmap, mask);
		fz_catch(ctx)
			rethrow(J);
	} else if (js_isuserdata(J, 1, "fz_buffer")) {
		fz_buffer *buffer = ffi_tobuffer(J, 1);
		fz_buffer *globals = js_isdefined(J, 2) ? ffi_tobuffer(J, 2) : NULL;
		fz_buffer *allocated = NULL;

		fz_var(allocated);

		fz_try(ctx)
		{
			if (globals)
			{
				allocated = fz_new_buffer(ctx, buffer->len + globals->len);
				fz_append_buffer(ctx, allocated, globals);
				fz_append_buffer(ctx, allocated, buffer);
				buffer = allocated;
			}
			image = fz_new_image_from_buffer(ctx, buffer);
		}
		fz_always(ctx)
			fz_drop_buffer(ctx, allocated);
		fz_catch(ctx)
			rethrow(J);
	} else {
		const char *name = js_tostring(J, 1);
		fz_try(ctx)
		{
			image = fz_new_image_from_file(ctx, name);
			if (mask)
				image->mask = fz_keep_image(ctx, mask);
		}
		fz_catch(ctx)
			rethrow(J);
	}

	ffi_pushimage_own(J, image);
}

static void ffi_Image_getWidth(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->w);
}

static void ffi_Image_getHeight(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->h);
}

static void ffi_Image_getXResolution(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->xres);
}

static void ffi_Image_getYResolution(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->yres);
}

static void ffi_Image_getNumberOfComponents(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->n);
}

static void ffi_Image_getBitsPerComponent(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, image->bpc);
}

static void ffi_Image_getInterpolate(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushboolean(J, image->interpolate);
}

static void ffi_Image_getOrientation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushnumber(J, fz_image_orientation(ctx, image));
}

static void ffi_Image_getImageMask(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	js_pushboolean(J, image->imagemask);
}

static void ffi_Image_getMask(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	if (image->mask)
		ffi_pushimage(J, image->mask);
	else
		js_pushnull(J);
}

static void ffi_Image_getColorSpace(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	ffi_pushcolorspace(J, image->colorspace);
}

static void ffi_Image_toPixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_image *image = js_touserdata(J, 0, "fz_image");
	fz_matrix matrix_, *matrix = NULL;
	fz_pixmap *pixmap = NULL;

	if (js_isnumber(J, 1) && js_isnumber(J, 2)) {
		matrix_ = fz_scale(js_tonumber(J, 1), js_tonumber(J, 2));
		matrix = &matrix_;
	}

	fz_try(ctx)
		pixmap = fz_get_pixmap_from_image(ctx, image, NULL, matrix, NULL, NULL);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_Image_getColorKey(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	int i;
	if (image->use_colorkey)
	{
		js_newarray(J);
		for (i = 0; i < 2 * image->n; ++i)
		{
			js_pushnumber(J, image->colorkey[i]);
			js_setindex(J, -2, i);
		}
	}
	else
		js_pushnull(J);
}

static void ffi_Image_getDecode(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	int i;
	if (image->use_decode)
	{
		js_newarray(J);
		for (i = 0; i < 2 * image->n; ++i)
		{
			js_pushnumber(J, image->decode[i]);
			js_setindex(J, -2, i);
		}
	}
	else
		js_pushnull(J);
}

static void ffi_Image_setOrientation(js_State *J)
{
	fz_image *image = js_touserdata(J, 0, "fz_image");
	int orientation = js_tointeger(J, 1);
	if (orientation < 0 || orientation > 8)
		js_rangeerror(J, "orientation out of range");
	image->orientation = js_tointeger(J, 1);
}

static void ffi_Shade_getBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_shade *shade = js_touserdata(J, 0, "fz_shade");
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_rect bounds;

	fz_try(ctx)
		bounds = fz_bound_shade(ctx, shade, ctm);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushrect(J, bounds);
}

static void ffi_new_Font(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *name = js_tostring(J, 1);
	int index = js_isnumber(J, 2) ? js_tonumber(J, 2) : 0;
	const unsigned char *data;
	int size;
	fz_font *font = NULL;

	fz_try(ctx) {
		data = fz_lookup_base14_font(ctx, name, &size);
		if (!data)
			data = fz_lookup_cjk_font_by_language(ctx, name, &size, &index);
		if (data)
			font = fz_new_font_from_memory(ctx, name, data, size, index, 0);
		else
			font = fz_new_font_from_file(ctx, name, name, index, 0);
	}
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_font");
	js_newuserdata(J, "fz_font", font, ffi_gc_fz_font);
}

static void ffi_Font_getName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	js_pushstring(J, fz_font_name(ctx, font));
}

static void ffi_Font_isMono(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	js_pushboolean(J, fz_font_is_monospaced(ctx, font));
}

static void ffi_Font_isSerif(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	js_pushboolean(J, fz_font_is_serif(ctx, font));
}

static void ffi_Font_isBold(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	js_pushboolean(J, fz_font_is_bold(ctx, font));
}

static void ffi_Font_isItalic(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	js_pushboolean(J, fz_font_is_italic(ctx, font));
}

static void ffi_Font_encodeCharacter(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	int unicode = js_tonumber(J, 1);
	int glyph = 0;
	fz_try(ctx)
		glyph = fz_encode_character(ctx, font, unicode);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, glyph);
}

static void ffi_Font_advanceGlyph(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_font *font = js_touserdata(J, 0, "fz_font");
	int glyph = js_tonumber(J, 1);
	int wmode = js_isdefined(J, 2) ? js_toboolean(J, 2) : 0;

	float advance = 0;
	fz_try(ctx)
		advance = fz_advance_glyph(ctx, font, glyph, wmode);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, advance);
}

static void ffi_new_Text(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_text *text = NULL;

	fz_try(ctx)
		text = fz_new_text(ctx);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_text");
	js_newuserdata(J, "fz_text", text, ffi_gc_fz_text);
}

static void ffi_Text_walk(js_State *J)
{
	fz_text *text = js_touserdata(J, 0, "fz_text");
	char buf[8];
	fz_text_span *span;
	fz_matrix trm;
	int i;

	for (span = text->head; span; span = span->next) {
		ffi_pushfont(J, span->font);
		trm = span->trm;
		if (js_hasproperty(J, 1, "beginSpan")) {
			js_copy(J, 1); // this
			js_copy(J, -3); // font
			ffi_pushmatrix(J, trm);
			js_pushboolean(J, span->wmode);
			js_pushnumber(J, span->bidi_level);
			js_pushnumber(J, span->markup_dir);
			js_pushstring(J, fz_string_from_text_language(buf, span->language));
			js_call(J, 6);
			js_pop(J, 1);
		}
		for (i = 0; i < span->len; ++i) {
			trm.e = span->items[i].x;
			trm.f = span->items[i].y;
			if (js_hasproperty(J, 1, "showGlyph")) {
				js_copy(J, 1); /* object for this binding */
				js_copy(J, -3); /* font */
				ffi_pushmatrix(J, trm);
				js_pushnumber(J, span->items[i].gid);
				js_pushnumber(J, span->items[i].ucs);
				js_pushnumber(J, span->wmode);
				js_pushnumber(J, span->bidi_level);
				js_call(J, 6);
				js_pop(J, 1);
			}
		}
		js_pop(J, 1); /* pop font object */
		if (js_hasproperty(J, 1, "endSpan")) {
			js_copy(J, 1); // this
			js_call(J, 0);
			js_pop(J, 1);
		}
	}
	js_pop(J, 1); /* pop showGlyph function */
}

static void ffi_Text_showGlyph(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_text *text = js_touserdata(J, 0, "fz_text");
	fz_font *font = js_touserdata(J, 1, "fz_font");
	fz_matrix trm = ffi_tomatrix(J, 2);
	int glyph = js_tointeger(J, 3);
	int unicode = js_tointeger(J, 4);
	int wmode = js_isdefined(J, 5) ? js_toboolean(J, 5) : 0;

	fz_try(ctx)
		fz_show_glyph(ctx, text, font, trm, glyph, unicode, wmode, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Text_showString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_text *text = js_touserdata(J, 0, "fz_text");
	fz_font *font = js_touserdata(J, 1, "fz_font");
	fz_matrix trm = ffi_tomatrix(J, 2);
	const char *s = js_tostring(J, 3);
	int wmode = js_isdefined(J, 4) ? js_toboolean(J, 4) : 0;

	fz_try(ctx)
		trm = fz_show_string(ctx, text, font, trm, s, wmode, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
	fz_catch(ctx)
		rethrow(J);

	/* update matrix with new pen position */
	js_pushnumber(J, trm.e);
	js_setindex(J, 2, 4);
	js_pushnumber(J, trm.f);
	js_setindex(J, 2, 5);
}

static void ffi_new_Path(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = NULL;

	fz_try(ctx)
		path = fz_new_path(ctx);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_path");
	js_newuserdata(J, "fz_path", path, ffi_gc_fz_path);
}

static void ffi_Path_walk_moveTo(fz_context *ctx, void *arg, float x, float y)
{
	js_State *J = arg;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, 1, "moveTo")) {
		js_copy(J, 1);
		js_pushnumber(J, x);
		js_pushnumber(J, y);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void ffi_Path_walk_lineTo(fz_context *ctx, void *arg, float x, float y)
{
	js_State *J = arg;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, 1, "lineTo")) {
		js_copy(J, 1);
		js_pushnumber(J, x);
		js_pushnumber(J, y);
		js_call(J, 2);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void ffi_Path_walk_curveTo(fz_context *ctx, void *arg,
		float x1, float y1, float x2, float y2, float x3, float y3)
{
	js_State *J = arg;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, 1, "curveTo")) {
		js_copy(J, 1);
		js_pushnumber(J, x1);
		js_pushnumber(J, y1);
		js_pushnumber(J, x2);
		js_pushnumber(J, y2);
		js_pushnumber(J, x3);
		js_pushnumber(J, y3);
		js_call(J, 6);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void ffi_Path_walk_closePath(fz_context *ctx, void *arg)
{
	js_State *J = arg;
	if (js_try(J))
		rethrow_as_fz(J);
	if (js_hasproperty(J, 1, "closePath")) {
		js_copy(J, 1);
		js_call(J, 0);
		js_pop(J, 1);
	}
	js_endtry(J);
}

static void ffi_Path_walk(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	fz_path_walker walker = {
		ffi_Path_walk_moveTo,
		ffi_Path_walk_lineTo,
		ffi_Path_walk_curveTo,
		ffi_Path_walk_closePath,
	};

	fz_try(ctx)
		fz_walk_path(ctx, path, &walker, J);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_moveTo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float x = js_tonumber(J, 1);
	float y = js_tonumber(J, 2);

	fz_try(ctx)
		fz_moveto(ctx, path, x, y);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_lineTo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float x = js_tonumber(J, 1);
	float y = js_tonumber(J, 2);

	fz_try(ctx)
		fz_lineto(ctx, path, x, y);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_curveTo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float x1 = js_tonumber(J, 1);
	float y1 = js_tonumber(J, 2);
	float x2 = js_tonumber(J, 3);
	float y2 = js_tonumber(J, 4);
	float x3 = js_tonumber(J, 5);
	float y3 = js_tonumber(J, 6);

	fz_try(ctx)
		fz_curveto(ctx, path, x1, y1, x2, y2, x3, y3);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_curveToV(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float cx = js_tonumber(J, 1);
	float cy = js_tonumber(J, 2);
	float ex = js_tonumber(J, 3);
	float ey = js_tonumber(J, 4);

	fz_try(ctx)
		fz_curvetov(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_curveToY(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float cx = js_tonumber(J, 1);
	float cy = js_tonumber(J, 2);
	float ex = js_tonumber(J, 3);
	float ey = js_tonumber(J, 4);

	fz_try(ctx)
		fz_curvetoy(ctx, path, cx, cy, ex, ey);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_closePath(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");

	fz_try(ctx)
		fz_closepath(ctx, path);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_rect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	float x1 = js_tonumber(J, 1);
	float y1 = js_tonumber(J, 2);
	float x2 = js_tonumber(J, 3);
	float y2 = js_tonumber(J, 4);

	fz_try(ctx)
		fz_rectto(ctx, path, x1, y1, x2, y2);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Path_getBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	fz_stroke_state stroke = ffi_tostroke(J, 1);
	fz_matrix ctm = ffi_tomatrix(J, 2);
	fz_rect bounds;

	fz_try(ctx)
		bounds = fz_bound_path(ctx, path, &stroke, ctm);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushrect(J, bounds);
}

static void ffi_Path_transform(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_path *path = js_touserdata(J, 0, "fz_path");
	fz_matrix ctm = ffi_tomatrix(J, 1);

	fz_try(ctx)
		fz_transform_path(ctx, path, ctm);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_new_DisplayList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_rect mediabox = js_iscoercible(J, 1) ? ffi_torect(J, 1) : fz_empty_rect;
	fz_display_list *list = NULL;

	fz_try(ctx)
		list = fz_new_display_list(ctx, mediabox);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_display_list");
	js_newuserdata(J, "fz_display_list", list, ffi_gc_fz_display_list);
}

static void ffi_DisplayList_getBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
	fz_rect bounds;

	fz_try(ctx)
		bounds = fz_bound_display_list(ctx, list);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushrect(J, bounds);
}

static void ffi_DisplayList_run(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
	fz_device *device = NULL;
	fz_matrix ctm = ffi_tomatrix(J, 2);

	if (js_isuserdata(J, 1, "fz_device")) {
		device = js_touserdata(J, 1, "fz_device");
		fz_try(ctx)
			fz_run_display_list(ctx, list, device, ctm, fz_infinite_rect, NULL);
		fz_catch(ctx)
			rethrow(J);
	} else {
		device = new_js_device(ctx, J);
		js_copy(J, 1);
		fz_try(ctx)
			fz_run_display_list(ctx, list, device, ctm, fz_infinite_rect, NULL);
		fz_always(ctx)
			fz_drop_device(ctx, device);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_DisplayList_toPixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_colorspace *colorspace = js_touserdata(J, 2, "fz_colorspace");
	int alpha = js_isdefined(J, 3) ? js_toboolean(J, 3) : 0;
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		pixmap = fz_new_pixmap_from_display_list(ctx, list, ctm, colorspace, alpha);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_DisplayList_toStructuredText(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
	const char *options = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	fz_stext_options so;
	fz_stext_page *text = NULL;

	fz_try(ctx) {
		fz_parse_stext_options(ctx, &so, options);
		text = fz_new_stext_page_from_display_list(ctx, list, &so);
	}
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_stext_page");
	js_newuserdata(J, "fz_stext_page", text, ffi_gc_fz_stext_page);
}

static void ffi_DisplayList_search(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
	const char *needle = js_tostring(J, 1);
	fz_quad hits[500];
	int marks[500];
	int n = 0;

	fz_try(ctx)
		n = fz_search_display_list(ctx, list, needle, marks, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	ffi_pushsearch(J, marks, hits, n);
}

static void ffi_StructuredText_walk(js_State *J)
{
	fz_stext_page *page = js_touserdata(J, 0, "fz_stext_page");
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
		{
			if (js_hasproperty(J, 1, "onImageBlock"))
			{
				js_pushnull(J);
				ffi_pushrect(J, block->bbox);
				ffi_pushmatrix(J, block->u.i.transform);
				ffi_pushimage(J, block->u.i.image);
				js_call(J, 3);
				js_pop(J, 1);
			}
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			if (js_hasproperty(J, 1, "beginTextBlock"))
			{
				js_pushnull(J);
				ffi_pushrect(J, block->bbox);
				js_call(J, 1);
				js_pop(J, 1);
			}

			for (line = block->u.t.first_line; line; line = line->next)
			{
				if (js_hasproperty(J, 1, "beginLine"))
				{
					js_pushnull(J);
					ffi_pushrect(J, line->bbox);
					js_pushboolean(J, line->wmode);
					ffi_pushpoint(J, line->dir);
					js_call(J, 3);
					js_pop(J, 1);
				}

				for (ch = line->first_char; ch; ch = ch->next)
				{
					if (js_hasproperty(J, 1, "onChar"))
					{
						char utf[10];
						js_pushnull(J);
						utf[fz_runetochar(utf, ch->c)] = 0;
						js_pushstring(J, utf);
						ffi_pushpoint(J, ch->origin);
						ffi_pushfont(J, ch->font);
						js_pushnumber(J, ch->size);
						ffi_pushquad(J, ch->quad);
						js_pushnumber(J, ch->color);
						js_call(J, 6);
						js_pop(J, 1);
					}
				}

				if (js_hasproperty(J, 1, "endLine"))
				{
					js_pushnull(J);
					js_call(J, 0);
					js_pop(J, 1);
				}
			}

			if (js_hasproperty(J, 1, "endTextBlock"))
			{
				js_pushnull(J);
				js_call(J, 0);
				js_pop(J, 1);
			}
		}
	}
}

static void ffi_StructuredText_search(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_stext_page *text = js_touserdata(J, 0, "fz_stext_page");
	const char *needle = js_tostring(J, 1);
	fz_quad hits[500];
	int marks[500];
	int n = 0;

	fz_try(ctx)
		n = fz_search_stext_page(ctx, text, needle, marks, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	ffi_pushsearch(J, marks, hits, n);
}

static void ffi_StructuredText_highlight(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_stext_page *text = js_touserdata(J, 0, "fz_stext_page");
	fz_point a = ffi_topoint(J, 1);
	fz_point b = ffi_topoint(J, 2);
	fz_quad hits[256];
	int i, n = 0;

	fz_try(ctx)
		n = fz_highlight_selection(ctx, text, a, b, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		ffi_pushquad(J, hits[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_StructuredText_copy(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_stext_page *text = js_touserdata(J, 0, "fz_stext_page");
	fz_point a = ffi_topoint(J, 1);
	fz_point b = ffi_topoint(J, 2);
	char *s = NULL;

	fz_try(ctx)
		s = fz_copy_selection(ctx, text, a, b, 0);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, s);

	fz_try(ctx)
		fz_free(ctx, s);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_new_DisplayListDevice(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_display_list *list = js_touserdata(J, 1, "fz_display_list");
	fz_device *device = NULL;

	fz_try(ctx)
		device = fz_new_list_device(ctx, list);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_device");
	js_newuserdata(J, "fz_device", device, ffi_gc_fz_device);
}

static void ffi_new_DrawDevice(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_matrix transform = ffi_tomatrix(J, 1);
	fz_pixmap *pixmap = ffi_topixmap(J, 2);
	fz_device *device = NULL;

	fz_try(ctx)
		device = fz_new_draw_device(ctx, transform, pixmap);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_device");
	js_newuserdata(J, "fz_device", device, ffi_gc_fz_device);
}

static void ffi_new_DocumentWriter(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *filename = js_tostring(J, 1);
	const char *format = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	const char *options = js_iscoercible(J, 3) ? js_tostring(J, 3) : NULL;
	fz_document_writer *wri = NULL;

	fz_try(ctx)
		wri = fz_new_document_writer(ctx, filename, format, options);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_document_writer");
	js_newuserdata(J, "fz_document_writer", wri, ffi_gc_fz_document_writer);
}

static void ffi_DocumentWriter_beginPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document_writer *wri = js_touserdata(J, 0, "fz_document_writer");
	fz_rect mediabox = ffi_torect(J, 1);
	fz_device *device = NULL;

	fz_try(ctx)
		device = fz_begin_page(ctx, wri, mediabox);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_device");
	js_newuserdata(J, "fz_device", fz_keep_device(ctx, device), ffi_gc_fz_device);
}

static void ffi_DocumentWriter_endPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document_writer *wri = js_touserdata(J, 0, "fz_document_writer");
	fz_try(ctx)
		fz_end_page(ctx, wri);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DocumentWriter_close(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_document_writer *wri = js_touserdata(J, 0, "fz_document_writer");
	fz_try(ctx)
		fz_close_document_writer(ctx, wri);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_new_Story(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *user_css = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	double em = js_isdefined(J, 3) ? js_tonumber(J, 3) : 12;
	fz_archive *arch = js_iscoercible(J, 4) ? ffi_toarchive(J, 4) : NULL;
	fz_buffer *contents = ffi_tobuffer(J, 1);
	fz_story *story = NULL;

	fz_try(ctx)
		story = fz_new_story(ctx, contents, user_css, em, arch);
	fz_always(ctx)
		fz_drop_buffer(ctx, contents);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_story");
	js_newuserdata(J, "fz_story", story, ffi_gc_fz_story);
}

static void ffi_Story_place(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_story *story = js_touserdata(J, 0, "fz_story");
	fz_rect rect = ffi_torect(J, 1);
	fz_rect filled = fz_empty_rect;
	int more;

	fz_try(ctx)
		more = fz_place_story(ctx, story, rect, &filled);
	fz_catch(ctx)
		rethrow(J);

	js_newobject(J);

	ffi_pushrect(J, filled);
	js_setproperty(J, -2, "filled");

	js_pushboolean(J, !!more);
	js_setproperty(J, -2, "more");
}

static void ffi_Story_draw(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_story *story = js_touserdata(J, 0, "fz_story");
	fz_device *device;
	int drop = 1;
	fz_matrix ctm = ffi_tomatrix(J, 2);

	if (js_isuserdata(J, 1, "fz_device")) {
		device = js_touserdata(J, 1, "fz_device");
		drop = 0;
	} else {
		device = new_js_device(ctx, J);
		js_copy(J, 1);
	}

	fz_try(ctx) {
		fz_draw_story(ctx, story, device, ctm);
	}
	fz_always(ctx)
	{
		if (drop)
			fz_drop_device(ctx, device);
	}
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Story_document(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_story *story = js_touserdata(J, 0, "fz_story");
	fz_xml *dom;

	fz_try(ctx)
		dom = fz_story_document(ctx, story);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_body(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_body(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_documentElement(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_document_element(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_createElement(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *tag = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;

	fz_try(ctx)
		dom = fz_dom_create_element(ctx, dom, tag);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}


static void ffi_DOM_createTextNode(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *text = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;

	fz_try(ctx)
		dom = fz_dom_create_text_node(ctx, dom, text);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_find(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *tag = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *att = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	const char *val = js_iscoercible(J, 3) ? js_tostring(J, 3) : NULL;

	fz_try(ctx)
		dom = fz_dom_find(ctx, dom, tag, att, val);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_findNext(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *tag = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *att = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	const char *val = js_iscoercible(J, 3) ? js_tostring(J, 3) : NULL;

	fz_try(ctx)
		dom = fz_dom_find_next(ctx, dom, tag, att, val);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_appendChild(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	fz_xml *child = js_touserdata(J, 1, "fz_xml");

	fz_try(ctx)
		fz_dom_append_child(ctx, dom, child);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DOM_insertBefore(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	fz_xml *elt = js_touserdata(J, 1, "fz_xml");

	fz_try(ctx)
		fz_dom_insert_before(ctx, dom, elt);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DOM_insertAfter(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	fz_xml *elt = js_touserdata(J, 1, "fz_xml");

	fz_try(ctx)
		fz_dom_insert_after(ctx, dom, elt);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DOM_remove(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		fz_dom_remove(ctx, dom);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DOM_clone(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_clone(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_firstChild(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_first_child(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_parent(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_parent(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_next(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_next(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_previous(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");

	fz_try(ctx)
		dom = fz_dom_previous(ctx, dom);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_addAttribute(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *att = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *val = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;

	fz_try(ctx)
		fz_dom_add_attribute(ctx, dom, att, val);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdom(J, fz_keep_xml(ctx, dom));
}

static void ffi_DOM_removeAttribute(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *att = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;

	fz_try(ctx)
		fz_dom_remove_attribute(ctx, dom, att);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_DOM_attribute(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *att = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *val;

	fz_try(ctx)
		val = fz_dom_attribute(ctx, dom, att);
	fz_catch(ctx)
		rethrow(J);

	if (val)
		js_pushstring(J, val);
	else
		js_pushnull(J);
}

static void ffi_DOM_getAttributes(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_xml *dom = js_touserdata(J, 0, "fz_xml");
	const char *att;
	const char *val;
	int i;

	js_newobject(J);

	i = 0;
	while (1)
	{
		fz_try(ctx)
			val = fz_dom_get_attribute(ctx, dom, i, &att);
		fz_catch(ctx)
			rethrow(J);
		if (att == NULL)
			break;
		js_pushstring(J, val);
		js_setproperty(J, -2, att);
		i++;
	}
}

/* PDF specifics */

#if FZ_ENABLE_PDF

static pdf_obj *ffi_toobj(js_State *J, pdf_document *pdf, int idx)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = NULL;

	/* make sure index is absolute */
	if (idx < 0)
		idx += js_gettop(J);

	if (js_isuserdata(J, idx, "pdf_obj"))
		return pdf_keep_obj(ctx, js_touserdata(J, idx, "pdf_obj"));

	if (js_isnumber(J, idx)) {
		float f = js_tonumber(J, idx);
		fz_try(ctx)
			if (f == (int)f)
				obj = pdf_new_int(ctx, f);
			else
				obj = pdf_new_real(ctx, f);
		fz_catch(ctx)
			rethrow(J);
		return obj;
	}

	if (js_isstring(J, idx)) {
		const char *s = js_tostring(J, idx);
		fz_try(ctx)
			if (s[0] == '(' && s[1] != 0)
				obj = pdf_new_string(ctx, s+1, strlen(s)-2);
			else
				obj = pdf_new_name(ctx, s);
		fz_catch(ctx)
			rethrow(J);
		return obj;
	}

	if (js_isboolean(J, idx)) {
		return js_toboolean(J, idx) ? PDF_TRUE : PDF_FALSE;
	}

	if (js_isnull(J, idx)) {
		return PDF_NULL;
	}

	if (js_isarray(J, idx)) {
		int i, n = js_getlength(J, idx);
		pdf_obj *val;
		fz_try(ctx)
			obj = pdf_new_array(ctx, pdf, n);
		fz_catch(ctx)
			rethrow(J);
		if (js_try(J)) {
			pdf_drop_obj(ctx, obj);
			js_throw(J);
		}
		for (i = 0; i < n; ++i) {
			js_getindex(J, idx, i);
			val = ffi_toobj(J, pdf, -1);
			fz_try(ctx)
				pdf_array_push_drop(ctx, obj, val);
			fz_catch(ctx)
				rethrow(J);
			js_pop(J, 1);
		}
		js_endtry(J);
		return obj;
	}

	if (js_isobject(J, idx)) {
		const char *key;
		pdf_obj *val;
		fz_try(ctx)
			obj = pdf_new_dict(ctx, pdf, 0);
		fz_catch(ctx)
			rethrow(J);
		if (js_try(J)) {
			pdf_drop_obj(ctx, obj);
			js_throw(J);
		}
		js_pushiterator(J, idx, 1);
		while ((key = js_nextiterator(J, -1))) {
			js_getproperty(J, idx, key);
			val = ffi_toobj(J, pdf, -1);
			fz_try(ctx)
				pdf_dict_puts_drop(ctx, obj, key, val);
			fz_catch(ctx)
				rethrow(J);
			js_pop(J, 1);
		}
		js_pop(J, 1);
		js_endtry(J);
		return obj;
	}

	js_error(J, "cannot convert JS type to PDF");
}

static int ffi_pdf_obj_has(js_State *J, void *obj, const char *key)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *val = NULL;
	int idx, len = 0;

	if (!strcmp(key, "length")) {
		fz_try(ctx)
			len = pdf_array_len(ctx, obj);
		fz_catch(ctx)
			rethrow(J);
		js_pushnumber(J, len);
		return 1;
	}

	if (is_number(key, &idx)) {
		fz_try(ctx)
			val = pdf_array_get(ctx, obj, idx);
		fz_catch(ctx)
			rethrow(J);
	} else {
		fz_try(ctx)
			val = pdf_dict_gets(ctx, obj, key);
		fz_catch(ctx)
			rethrow(J);
	}
	if (val) {
		ffi_pushobj(J, pdf_keep_obj(ctx, val));
		return 1;
	}
	return 0;
}

static int ffi_pdf_obj_put(js_State *J, void *obj, const char *key)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = NULL;
	pdf_obj *val;
	int idx;

	fz_try(ctx)
		pdf = pdf_get_bound_document(ctx, obj);
	fz_catch(ctx)
		rethrow(J);

	val = ffi_toobj(J, pdf, -1);

	if (is_number(key, &idx)) {
		fz_try(ctx)
			pdf_array_put(ctx, obj, idx, val);
		fz_always(ctx)
			pdf_drop_obj(ctx, val);
		fz_catch(ctx)
			rethrow(J);
	} else {
		fz_try(ctx)
			pdf_dict_puts(ctx, obj, key, val);
		fz_always(ctx)
			pdf_drop_obj(ctx, val);
		fz_catch(ctx)
			rethrow(J);
	}
	return 1;
}

static int ffi_pdf_obj_delete(js_State *J, void *obj, const char *key)
{
	fz_context *ctx = js_getcontext(J);
	int idx;

	if (is_number(key, &idx)) {
		fz_try(ctx)
			pdf_array_delete(ctx, obj, idx);
		fz_catch(ctx)
			rethrow(J);
	} else {
		fz_try(ctx)
			pdf_dict_dels(ctx, obj, key);
		fz_catch(ctx)
			rethrow(J);
	}
	return 1;
}

static void ffi_pushobj(js_State *J, pdf_obj *obj)
{
	if (obj) {
		js_getregistry(J, "pdf_obj");
		js_newuserdatax(J, "pdf_obj", obj,
				ffi_pdf_obj_has, ffi_pdf_obj_put, ffi_pdf_obj_delete,
				ffi_gc_pdf_obj);
	} else {
		js_pushnull(J);
	}
}

static void ffi_new_PDFDocument(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *filename = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	pdf_document *pdf = NULL;

	fz_try(ctx)
		if (filename)
			pdf = pdf_open_document(ctx, filename);
		else
			pdf = pdf_create_document(ctx);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "pdf_document");
	js_newuserdata(J, "pdf_document", pdf, ffi_gc_pdf_document);
}

static void ffi_PDFDocument_getVersion(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int version;

	fz_try(ctx)
		version = pdf_version(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);

	js_newobject(J);
	js_pushnumber(J, version / 10);
	js_setproperty(J, -2, "major");
	js_pushnumber(J, version % 10);
	js_setproperty(J, -2, "minor");
}

static void ffi_PDFDocument_getTrailer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *trailer = NULL;

	fz_try(ctx)
		trailer = pdf_trailer(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, pdf_keep_obj(ctx, trailer));
}

static void ffi_PDFDocument_countObjects(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int count = 0;

	fz_try(ctx)
		count = pdf_xref_len(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, count);
}

static void ffi_PDFDocument_createObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *ind = NULL;

	fz_try(ctx)
		ind = pdf_new_indirect(ctx, pdf, pdf_create_object(ctx, pdf), 0);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_deleteObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *ind = js_isuserdata(J, 1, "pdf_obj") ? js_touserdata(J, 1, "pdf_obj") : NULL;
	int num = ind ? pdf_to_num(ctx, ind) : js_tonumber(J, 1);

	fz_try(ctx)
		pdf_delete_object(ctx, pdf, num);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_addObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = ffi_toobj(J, pdf, 1);
	pdf_obj *ind = NULL;

	fz_try(ctx)
		ind = pdf_add_object_drop(ctx, pdf, obj);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_addStream_imp(js_State *J, int compressed)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = js_iscoercible(J, 2) ? ffi_toobj(J, pdf, 2) : NULL;
	fz_buffer *buf = ffi_tobuffer(J, 1);
	pdf_obj *ind = NULL;

	fz_try(ctx)
		ind = pdf_add_stream(ctx, pdf, buf, obj, compressed);
	fz_always(ctx) {
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, obj);
	} fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_addStream(js_State *J)
{
	ffi_PDFDocument_addStream_imp(J, 0);
}

static void ffi_PDFDocument_addRawStream(js_State *J)
{
	ffi_PDFDocument_addStream_imp(J, 1);
}

static void ffi_PDFDocument_addEmbeddedFile(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	const char *filename = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *mimetype = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	fz_buffer *contents = ffi_tobuffer(J, 3);
	double created = js_trynumber(J, 4, -1);
	double modified = js_trynumber(J, 5, -1);
	int add_checksum = js_tryboolean(J, 6, 0);
	pdf_obj *ind = NULL;

	if (created >= 0) created /= 1000;
	if (modified >= 0) modified /= 1000;

	fz_try(ctx)
		ind = pdf_add_embedded_file(ctx, pdf, filename, mimetype, contents,
			created, modified, add_checksum);
	fz_always(ctx)
		fz_drop_buffer(ctx, contents);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, ind);
}

static void ffi_pushembeddedfileparams(js_State *J, pdf_embedded_file_params *params)
{
	js_newobject(J);
	js_pushstring(J, params->filename);
	js_setproperty(J, -2, "filename");
	if (params->mimetype)
		js_pushstring(J, params->mimetype);
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "mimetype");
	js_pushnumber(J, params->size);
	js_setproperty(J, -2, "size");
	if (params->created >= 0)
	{
		js_getglobal(J, "Date");
		js_pushnumber(J, params->created * 1000);
		js_construct(J, 1);
	}
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "creationDate");
	if (params->modified >= 0)
	{
		js_getglobal(J, "Date");
		js_pushnumber(J, params->modified * 1000);
		js_construct(J, 1);
	}
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "modificationDate");
}

static void ffi_PDFDocument_getEmbeddedFileParams(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *fs = ffi_toobj(J, pdf, 1);
	pdf_embedded_file_params params;

	fz_try(ctx)
		pdf_get_embedded_file_params(ctx, fs, &params);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushembeddedfileparams(J, &params);
}

static void ffi_PDFDocument_getEmbeddedFileContents(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *fs = ffi_toobj(J, pdf, 1);
	fz_buffer *contents = NULL;

	fz_try(ctx)
		contents = pdf_load_embedded_file_contents(ctx, fs);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushbuffer(J, contents);
}

static void ffi_PDFDocument_verifyEmbeddedFileChecksum(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *fs = ffi_toobj(J, pdf, 1);
	int valid = 0;

	fz_try(ctx)
		valid = pdf_verify_embedded_file_checksum(ctx, fs);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, valid);
}

static void ffi_PDFDocument_addImage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_image *image = js_touserdata(J, 1, "fz_image");
	pdf_obj *ind = NULL;

	fz_try(ctx)
		ind = pdf_add_image(ctx, pdf, image);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_loadImage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = ffi_toobj(J, pdf, 1);
	fz_image *img = NULL;

	fz_try(ctx)
		img = pdf_load_image(ctx, pdf, obj);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushimage_own(J, img);
}

static void ffi_PDFDocument_addSimpleFont(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_font *font = js_touserdata(J, 1, "fz_font");
	const char *encname = js_tostring(J, 2);
	pdf_obj *ind = NULL;
	int enc = PDF_SIMPLE_ENCODING_LATIN;

	if (!strcmp(encname, "Latin") || !strcmp(encname, "Latn"))
		enc = PDF_SIMPLE_ENCODING_LATIN;
	else if (!strcmp(encname, "Greek") || !strcmp(encname, "Grek"))
		enc = PDF_SIMPLE_ENCODING_GREEK;
	else if (!strcmp(encname, "Cyrillic") || !strcmp(encname, "Cyrl"))
		enc = PDF_SIMPLE_ENCODING_CYRILLIC;

	fz_try(ctx)
		ind = pdf_add_simple_font(ctx, pdf, font, enc);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_addCJKFont(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_font *font = js_touserdata(J, 1, "fz_font");
	const char *lang = js_tostring(J, 2);
	const char *wm = js_tostring(J, 3);
	const char *ss = js_tostring(J, 4);
	int ordering;
	int wmode = 0;
	int serif = 1;
	pdf_obj *ind = NULL;

	ordering = fz_lookup_cjk_ordering_by_language(lang);

	if (!strcmp(wm, "V"))
		wmode = 1;
	if (!strcmp(ss, "sans") || !strcmp(ss, "sans-serif"))
		serif = 0;

	fz_try(ctx)
		ind = pdf_add_cjk_font(ctx, pdf, font, ordering, wmode, serif);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_addFont(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_font *font = js_touserdata(J, 1, "fz_font");
	pdf_obj *ind = NULL;

	fz_try(ctx)
		ind = pdf_add_cid_font(ctx, pdf, font);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_addPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_rect mediabox = ffi_torect(J, 1);
	int rotate = js_tonumber(J, 2);
	pdf_obj *resources = ffi_toobj(J, pdf, 3);
	fz_buffer *contents = NULL;
	pdf_obj *ind = NULL;

	if (js_try(J)) {
		pdf_drop_obj(ctx, resources);
		js_throw(J);
	}

	contents = ffi_tobuffer(J, 4);
	js_endtry(J);

	fz_try(ctx)
		ind = pdf_add_page(ctx, pdf, mediabox, rotate, resources, contents);
	fz_always(ctx) {
		fz_drop_buffer(ctx, contents);
		pdf_drop_obj(ctx, resources);
	} fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, ind);
}

static void ffi_PDFDocument_insertPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int at = js_tonumber(J, 1);
	pdf_obj *obj = ffi_toobj(J, pdf, 2);

	fz_try(ctx)
		pdf_insert_page(ctx, pdf, at, obj);
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_deletePage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int at = js_tonumber(J, 1);

	fz_try(ctx)
		pdf_delete_page(ctx, pdf, at);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_countPages(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int count = 0;

	fz_try(ctx)
		count = pdf_count_pages(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, count);
}

static void ffi_PDFDocument_findPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int at = js_tonumber(J, 1);
	pdf_obj *obj = NULL;

	fz_try(ctx)
		obj = pdf_lookup_page_obj(ctx, pdf, at);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, pdf_keep_obj(ctx, obj));
}

static void ffi_PDFDocument_findPageNumber(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *ref = js_touserdata(J, 1, "pdf_obj");
	int num = 0;

	fz_try(ctx)
		num = pdf_lookup_page_number(ctx, pdf, ref);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, num);
}

static void ffi_PDFDocument_lookupDest(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *needle = ffi_toobj(J, pdf, 1);
	pdf_obj *obj = NULL;

	fz_try(ctx)
		obj = pdf_lookup_dest(ctx, pdf, needle);
	fz_always(ctx)
		pdf_drop_obj(ctx, needle);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, pdf_keep_obj(ctx, obj));
}

static void ffi_PDFDocument_save(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	const char *filename = js_tostring(J, 1);
	const char *options = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	pdf_write_options pwo;

	fz_try(ctx) {
		pdf_parse_write_options(ctx, &pwo, options);
		pdf_save_document(ctx, pdf, filename, &pwo);
	} fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_newNull(js_State *J)
{
	ffi_pushobj(J, PDF_NULL);
}

static void ffi_PDFDocument_newBoolean(js_State *J)
{
	int val = js_toboolean(J, 1);
	ffi_pushobj(J, val ? PDF_TRUE : PDF_FALSE);
}

static void ffi_PDFDocument_newInteger(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	int val = js_tointeger(J, 1);
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_int(ctx, val);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newReal(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	float val = js_tonumber(J, 1);
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_real(ctx, val);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *val = js_tostring(J, 1);
	pdf_obj *obj = NULL;

	fz_try(ctx)
		obj = pdf_new_text_string(ctx, val);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newByteString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	int n, i;
	char *buf;
	pdf_obj *obj = NULL;

	n = js_getlength(J, 1);
	if (n < 0)
		n = 0;

	fz_try(ctx)
		buf = fz_malloc(ctx, n);
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_free(ctx, buf);
		js_throw(J);
	}

	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		buf[i] = js_tonumber(J, -1);
		js_pop(J, 1);
	}

	js_endtry(J);

	fz_try(ctx)
		obj = pdf_new_string(ctx, buf, n);
	fz_always(ctx)
		fz_free(ctx, buf);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *val = js_tostring(J, 1);
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_name(ctx, val);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newIndirect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int num = js_tointeger(J, 1);
	int gen = js_tointeger(J, 2);
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_indirect(ctx, pdf, num, gen);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newArray(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_array(ctx, pdf, 0);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_newDictionary(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = NULL;
	fz_try(ctx)
		obj = pdf_new_dict(ctx, pdf, 0);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_enableJS(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_enable_js(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_disableJS(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_disable_js(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_isJSSupported(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int supported = 0;
	fz_try(ctx)
		supported = pdf_js_supported(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, supported);
}

static void ffi_PDFDocument_countVersions(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_count_versions(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, val);
}

static void ffi_PDFDocument_countUnsavedVersions(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_count_unsaved_versions(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, val);
}

static void ffi_PDFDocument_validateChangeHistory(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_validate_change_history(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, val);
}

static void ffi_PDFDocument_wasPureXFA(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_was_pure_xfa(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static void free_event_cb_data(fz_context *ctx, void *data)
{
	js_State *J = ((struct event_cb_data *) data)->J;
	const char *listener = ((struct event_cb_data *) data)->listener;

	if (listener)
		js_unref(J, listener);
	fz_free(ctx, data);
}

static void event_cb(fz_context *ctx, pdf_document *doc, pdf_doc_event *evt, void *data)
{
	js_State *J = ((struct event_cb_data *) data)->J;
	const char *listener = ((struct event_cb_data *) data)->listener;

	switch (evt->type)
	{
	case PDF_DOCUMENT_EVENT_ALERT:
		{
			pdf_alert_event *alert = pdf_access_alert_event(ctx, evt);

			if (js_try(J))
				rethrow_as_fz(J);

			js_getregistry(J, listener);
			if (js_hasproperty(J, -1, "onAlert"))
			{
				js_pushnull(J);
				js_pushstring(J, alert->message);
				js_pcall(J, 1);
				js_pop(J, 1);
			}
			js_endtry(J);
		}
		break;

	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "event not yet implemented");
		break;
	}
}

static void ffi_PDFDocument_setJSEventListener(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	struct event_cb_data *data = NULL;

	fz_try(ctx)
		data = fz_calloc(ctx, 1, sizeof (struct event_cb_data));
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		if (data->listener)
			js_unref(J, data->listener);
		fz_free(ctx, data);
		js_throw(J);
	}
	js_copy(J, 1);
	data->listener = js_ref(J);
	data->J = J;
	js_endtry(J);

	fz_try(ctx)
		pdf_set_doc_event_callback(ctx, pdf, event_cb, free_event_cb_data, data);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_hasUnsavedChanges(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_has_unsaved_changes(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static void ffi_PDFDocument_wasRepaired(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_was_repaired(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static void ffi_PDFDocument_canBeSavedIncrementally(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int val = 0;
	fz_try(ctx)
		val = pdf_can_be_saved_incrementally(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static void ffi_PDFDocument_newGraftMap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	pdf_graft_map *map = NULL;
	fz_try(ctx)
		map = pdf_new_graft_map(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_getregistry(J, "pdf_graft_map");
	js_newuserdata(J, "pdf_graft_map", map, ffi_gc_pdf_graft_map);
}

static void ffi_PDFDocument_graftObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *dst = js_touserdata(J, 0, "pdf_document");
	pdf_obj *obj = js_touserdata(J, 1, "pdf_obj");
	fz_try(ctx)
		obj = pdf_graft_object(ctx, dst, obj);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFDocument_graftPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *dst = js_touserdata(J, 0, "pdf_document");
	int to = js_tonumber(J, 1);
	pdf_document *src = js_touserdata(J, 2, "pdf_document");
	int from = js_tonumber(J, 3);
	fz_try(ctx)
		pdf_graft_page(ctx, dst, to, src, from);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_enableJournal(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_enable_journal(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_beginOperation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	const char *operation = js_tostring(J, 1);
	fz_try(ctx)
		pdf_begin_operation(ctx, pdf, operation);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_beginImplicitOperation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_begin_implicit_operation(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_endOperation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_end_operation(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_abandonOperation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_abandon_operation(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_canUndo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int can;
	fz_try(ctx)
		can = pdf_can_undo(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, can);
}

static void ffi_PDFDocument_canRedo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int can;
	fz_try(ctx)
		can = pdf_can_redo(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, can);
}

static void ffi_PDFDocument_getJournal(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	const char *name;
	int i, position, count;

	js_newobject(J);

	fz_try(ctx)
		position = pdf_undoredo_state(ctx, pdf, &count);
	fz_catch(ctx)
		rethrow(J);

	js_pushnumber(J, position);
	js_setproperty(J, -2, "position");

	js_newarray(J);
	for (i = 0; i < count; ++i)
	{
		fz_try(ctx)
			name = pdf_undoredo_step(ctx, pdf, i);
		fz_catch(ctx)
			rethrow(J);
		js_pushstring(J, name);
		js_setindex(J, -2, i);
	}
	js_setproperty(J, -2, "steps");
}

static void ffi_PDFDocument_undo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_undo(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_redo(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	fz_try(ctx)
		pdf_redo(ctx, pdf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_setPageLabels(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int index = js_tointeger(J, 1);
	const char *s = js_tostring(J, 2);
	const char *p = js_tostring(J, 3);
	int st = js_iscoercible(J, 4) ? js_tonumber(J, 4) : 1;
	fz_try(ctx)
		pdf_set_page_labels(ctx, pdf, index, s[0], p, st);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFDocument_deletePageLabels(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_document *pdf = js_touserdata(J, 0, "pdf_document");
	int index = js_tointeger(J, 1);
	fz_try(ctx)
		pdf_delete_page_labels(ctx, pdf, index);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_appendDestToURI(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *url = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *name = NULL;
	fz_link_dest dest = { 0 };
	char *uri = NULL;

	if (js_isobject(J, 2))
	{
		dest = ffi_tolinkdest(J, 2);
		fz_try(ctx)
			uri = pdf_append_explicit_dest_to_uri(ctx, url, dest);
		fz_catch(ctx)
			rethrow(J);
	}
	else if (js_isnumber(J, 2))
	{
		dest = fz_make_link_dest_xyz(0, js_tonumber(J, 2) - 1, NAN, NAN, NAN);
		fz_try(ctx)
			uri = pdf_append_explicit_dest_to_uri(ctx, url, dest);
		fz_catch(ctx)
			rethrow(J);
	}
	else
	{
		name = js_tostring(J, 2);
		fz_try(ctx)
			uri = pdf_append_named_dest_to_uri(ctx, url, name);
		fz_catch(ctx)
			rethrow(J);
	}

	if (js_try(J)) {
		fz_free(ctx, uri);
		js_throw(J);
	}
	if (uri)
		js_pushstring(J, uri);
	else
		js_pushnull(J);
	js_endtry(J);
	fz_free(ctx, uri);
}

static void ffi_formatURIFromPathAndDest(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *path = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *name = NULL;
	fz_link_dest dest = { 0 };
	char *uri = NULL;

	if (js_isobject(J, 2))
	{
		dest = ffi_tolinkdest(J, 2);
		fz_try(ctx)
			uri = pdf_new_uri_from_path_and_explicit_dest(ctx, path, dest);
		fz_catch(ctx)
			rethrow(J);
	}
	else if (js_isnumber(J, 2))
	{
		dest = fz_make_link_dest_xyz(0, js_tonumber(J, 2) - 1, NAN, NAN, NAN);
		fz_try(ctx)
			uri = pdf_new_uri_from_path_and_explicit_dest(ctx, path, dest);
		fz_catch(ctx)
			rethrow(J);
	}
	else
	{
		name = js_tostring(J, 2);
		fz_try(ctx)
			uri = pdf_new_uri_from_path_and_named_dest(ctx, path, name);
		fz_catch(ctx)
			rethrow(J);
	}

	if (js_try(J)) {
		fz_free(ctx, uri);
		js_throw(J);
	}
	if (uri)
		js_pushstring(J, uri);
	else
		js_pushnull(J);
	js_endtry(J);
	fz_free(ctx, uri);
}

static void ffi_PDFGraftMap_graftObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_graft_map *map = js_touserdata(J, 0, "pdf_graft_map");
	pdf_obj *obj = js_touserdata(J, 1, "pdf_obj");
	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, obj);
}

static void ffi_PDFGraftMap_graftPage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_graft_map *map = js_touserdata(J, 0, "pdf_graft_map");
	int to = js_tointeger(J, 1);
	pdf_document *src = js_touserdata(J, 2, "pdf_document");
	int from = js_tointeger(J, 3);
	fz_try(ctx)
		pdf_graft_mapped_page(ctx, map, to, src, from);
	fz_catch(ctx)
		rethrow(J);
}

static pdf_obj *ffi_PDFObject_get_imp(js_State *J, int inheritable)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	pdf_obj *val = NULL;
	int i, n = js_gettop(J);


	for (i = 1; i < n && obj; ++i) {
		if (pdf_is_array(ctx, obj)) {
			int key = js_tonumber(J, 1);
			fz_try(ctx)
				obj = val = pdf_array_get(ctx, obj, key);
			fz_catch(ctx)
				rethrow(J);
		} else if (js_isuserdata(J, i, "pdf_obj")) {
			pdf_obj *key = js_touserdata(J, i, "pdf_obj");
			fz_try(ctx)
				if (inheritable)
					obj = val = pdf_dict_get_inheritable(ctx, obj, key);
				else
					obj = val = pdf_dict_get(ctx, obj, key);
			fz_catch(ctx)
				rethrow(J);
		} else if (inheritable) {
			const char *key = js_tostring(J, i);
			fz_try(ctx)
				obj = val = pdf_dict_gets_inheritable(ctx, obj, key);
			fz_catch(ctx)
				rethrow(J);
		} else {
			const char *key = js_tostring(J, i);
			fz_try(ctx)
				obj = val = pdf_dict_gets(ctx, obj, key);
			fz_catch(ctx)
				rethrow(J);
		}
	}

	return val;
}

static void ffi_PDFObject_get(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *val = ffi_PDFObject_get_imp(J, 0);
	if (val)
		ffi_pushobj(J, pdf_keep_obj(ctx, val));
	else
		js_pushnull(J);
}

static void ffi_PDFObject_getInheritable(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *val = ffi_PDFObject_get_imp(J, 1);
	if (val)
		ffi_pushobj(J, pdf_keep_obj(ctx, val));
	else
		js_pushnull(J);
}

static void ffi_PDFObject_getNumber(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = ffi_PDFObject_get_imp(J, 0);
	float num = 0;
	fz_try(ctx)
		if (pdf_is_int(ctx, obj))
			num = pdf_to_int(ctx, obj);
		else
			num = pdf_to_real(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, num);
}

static void ffi_PDFObject_getName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = ffi_PDFObject_get_imp(J, 0);
	const char *name = NULL;
	fz_try(ctx)
		name = pdf_to_name(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, name);
}

static void ffi_PDFObject_getString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = ffi_PDFObject_get_imp(J, 0);
	const char *string = NULL;
	fz_try(ctx)
		string = pdf_to_text_string(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, string);
}

static void ffi_PDFObject_put(js_State *J)
{
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *key = js_tostring(J, 1);
	js_copy(J, 2);
	ffi_pdf_obj_put(J, obj, key);
}

static void ffi_PDFObject_delete(js_State *J)
{
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *key = js_tostring(J, 1);
	ffi_pdf_obj_delete(J, obj, key);
}

static void ffi_PDFObject_push(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	pdf_document *pdf = pdf_get_bound_document(ctx, obj);
	pdf_obj *item = ffi_toobj(J, pdf, 1);
	fz_try(ctx)
		pdf_array_push(ctx, obj, item);
	fz_always(ctx)
		pdf_drop_obj(ctx, item);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFObject_resolve(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	pdf_obj *ind = NULL;
	fz_try(ctx)
		ind = pdf_resolve_indirect(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushobj(J, pdf_keep_obj(ctx, ind));
}

static void ffi_PDFObject_toString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int tight = js_isdefined(J, 1) ? js_toboolean(J, 1) : 1;
	int ascii = js_isdefined(J, 2) ? js_toboolean(J, 2) : 0;
	char *s = NULL;
	size_t n;

	fz_try(ctx)
		s = pdf_sprint_obj(ctx, NULL, 0, &n, obj, tight, ascii);
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_free(ctx, s);
		js_throw(J);
	}
	js_pushstring(J, s);
	js_endtry(J);
	fz_free(ctx, s);
}

static void ffi_PDFObject_valueOf(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	if (pdf_is_indirect(ctx, obj))
		js_pushstring(J, "R");
	else if (pdf_is_null(ctx, obj))
		js_pushnull(J);
	else if (pdf_is_bool(ctx, obj))
		js_pushboolean(J, pdf_to_bool(ctx, obj));
	else if (pdf_is_int(ctx, obj))
		js_pushnumber(J, pdf_to_int(ctx, obj));
	else if (pdf_is_real(ctx, obj))
		js_pushnumber(J, pdf_to_real(ctx, obj));
	else if (pdf_is_string(ctx, obj))
		js_pushlstring(J, pdf_to_str_buf(ctx, obj), (int)pdf_to_str_len(ctx, obj));
	else if (pdf_is_name(ctx, obj))
		js_pushstring(J, pdf_to_name(ctx, obj));
	else
		js_copy(J, 0);
}

static void ffi_PDFObject_isArray(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_array(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_isDictionary(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_dict(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_isIndirect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_indirect(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_asIndirect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int num = 0;
	fz_try(ctx)
		num = pdf_to_num(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, num);
}

static void ffi_PDFObject_isNull(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_null(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_isBoolean(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_bool(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_asBoolean(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_to_bool(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_isNumber(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_number(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_asNumber(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	float num = 0;
	fz_try(ctx)
		if (pdf_is_int(ctx, obj))
			num = pdf_to_int(ctx, obj);
		else
			num = pdf_to_real(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, num);
}

static void ffi_PDFObject_isName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_name(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_asName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *name = NULL;
	fz_try(ctx)
		name = pdf_to_name(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, name);
}

static void ffi_PDFObject_isString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_string(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_asString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *string = NULL;

	fz_try(ctx)
		string = pdf_to_text_string(ctx, obj);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, string);
}

static void ffi_PDFObject_asByteString(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *buf;
	size_t i, len = 0;

	fz_try(ctx)
		buf = pdf_to_string(ctx, obj, &len);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < len; ++i) {
		js_pushnumber(J, (unsigned char)buf[i]);
		js_setindex(J, -2, (int)i);
	}
}

static void ffi_PDFObject_isStream(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	int b = 0;
	fz_try(ctx)
		b = pdf_is_stream(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, b);
}

static void ffi_PDFObject_readStream(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	fz_buffer *buf = NULL;
	fz_try(ctx)
		buf = pdf_load_stream(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushbuffer(J, buf);
}

static void ffi_PDFObject_readRawStream(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	fz_buffer *buf = NULL;
	fz_try(ctx)
		buf = pdf_load_raw_stream(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushbuffer(J, buf);
}

static void ffi_PDFObject_writeObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *ref = js_touserdata(J, 0, "pdf_obj");
	pdf_document *pdf = pdf_get_bound_document(ctx, ref);
	pdf_obj *obj = ffi_toobj(J, pdf, 1);
	fz_try(ctx)
		pdf_update_object(ctx, pdf, pdf_to_num(ctx, ref), obj);
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFObject_writeStream(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	fz_buffer *buf = ffi_tobuffer(J, 1);
	fz_try(ctx)
		pdf_update_stream(ctx, pdf_get_bound_document(ctx, obj), obj, buf, 0);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFObject_writeRawStream(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	fz_buffer *buf = ffi_tobuffer(J, 1);
	fz_try(ctx)
		pdf_update_stream(ctx, pdf_get_bound_document(ctx, obj), obj, buf, 1);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFObject_forEach(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	pdf_obj *val = NULL;
	const char *key = NULL;
	int i, n = 0;

	fz_try(ctx)
		obj = pdf_resolve_indirect_chain(ctx, obj);
	fz_catch(ctx)
		rethrow(J);

	if (pdf_is_array(ctx, obj)) {
		fz_try(ctx)
			n = pdf_array_len(ctx, obj);
		fz_catch(ctx)
			rethrow(J);
		for (i = 0; i < n; ++i) {
			fz_try(ctx)
				val = pdf_array_get(ctx, obj, i);
			fz_catch(ctx)
				rethrow(J);
			js_copy(J, 1);
			js_pushnull(J);
			ffi_pushobj(J, pdf_keep_obj(ctx, val));
			js_pushnumber(J, i);
			js_copy(J, 0);
			js_call(J, 3);
			js_pop(J, 1);
		}
		return;
	}

	if (pdf_is_dict(ctx, obj)) {
		fz_try(ctx)
			n = pdf_dict_len(ctx, obj);
		fz_catch(ctx)
			rethrow(J);
		for (i = 0; i < n; ++i) {
			fz_try(ctx) {
				key = pdf_to_name(ctx, pdf_dict_get_key(ctx, obj, i));
				val = pdf_dict_get_val(ctx, obj, i);
			} fz_catch(ctx)
				rethrow(J);
			js_copy(J, 1);
			js_pushnull(J);
			ffi_pushobj(J, pdf_keep_obj(ctx, val));
			js_pushstring(J, key);
			js_copy(J, 0);
			js_call(J, 3);
			js_pop(J, 1);
		}
		return;
	}
}

static void ffi_PDFObject_compare(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	pdf_obj *other = js_touserdata(J, 1, "pdf_obj");
	int result = 0;

	fz_try(ctx)
		result = pdf_objcmp(ctx, obj, other);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, result);
}

static void ffi_PDFPage_getObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	ffi_pushobj(J, pdf_keep_obj(ctx, page->obj));
}

static void ffi_PDFPage_getWidgets(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_annot *widget = NULL;
	int i = 0;

	fz_try(ctx)
		widget = pdf_first_widget(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);

	while (widget) {
		js_getregistry(J, "pdf_widget");
		js_newuserdata(J, "pdf_widget", pdf_keep_widget(ctx, widget), ffi_gc_pdf_annot);
		js_setindex(J, -2, i++);

		fz_try(ctx)
			widget = pdf_next_widget(ctx, widget);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFPage_getAnnotations(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_annot *annot = NULL;
	int i = 0;

	fz_try(ctx)
		annot = pdf_first_annot(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);

	while (annot) {
		js_getregistry(J, "pdf_annot");
		js_newuserdata(J, "pdf_annot", pdf_keep_annot(ctx, annot), ffi_gc_pdf_annot);
		js_setindex(J, -2, i++);

		fz_try(ctx)
			annot = pdf_next_annot(ctx, annot);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFPage_createAnnotation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	const char *name = js_tostring(J, 1);
	pdf_annot *annot = NULL;
	int type;

	fz_try(ctx)
	{
		type = pdf_annot_type_from_string(ctx, name);
		annot = pdf_create_annot(ctx, page, type);
	}
	fz_catch(ctx)
		rethrow(J);
	js_getregistry(J, "pdf_annot");
	js_newuserdata(J, "pdf_annot", annot, ffi_gc_pdf_annot);
}

static void ffi_PDFPage_deleteAnnotation(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_annot *annot = ffi_toannot(J, 1);
	fz_try(ctx)
		pdf_delete_annot(ctx, page, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFPage_update(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	int changed = 0;
	fz_try(ctx)
		changed = pdf_update_page(ctx, page);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, changed);
}

static void ffi_PDFPage_applyRedactions(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_redact_options opts = { 1, PDF_REDACT_IMAGE_PIXELS };
	if (js_isdefined(J, 1)) opts.black_boxes = js_toboolean(J, 1);
	if (js_isdefined(J, 2)) opts.image_method = js_tointeger(J, 2);
	fz_try(ctx)
		pdf_redact_page(ctx, page->doc, page, &opts);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFPage_process(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_processor *proc = new_js_processor(ctx, J);
	fz_try(ctx)
	{
		pdf_obj *resources = pdf_page_resources(ctx, page);
		pdf_obj *contents = pdf_page_contents(ctx, page);
		pdf_process_contents(ctx, proc, page->doc, resources, contents, NULL, NULL);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
		pdf_drop_processor(ctx, proc);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFPage_toPixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = pdf_page_from_fz_page(ctx, ffi_topage(J, 0));
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_colorspace *colorspace = js_touserdata(J, 2, "fz_colorspace");
	int alpha = js_toboolean(J, 3);
	int extra = js_isdefined(J, 4) ? js_toboolean(J, 4) : 1;
	const char *usage = js_isdefined(J, 5) ? js_tostring(J, 5) : "View";
	const char *box_name = js_isdefined(J, 6) ? js_tostring(J, 6) : NULL;
	fz_box_type box = FZ_CROP_BOX;
	fz_pixmap *pixmap = NULL;

	if (box_name) {
		box = fz_box_type_from_string(box_name);
		if (box == FZ_UNKNOWN_BOX)
			js_error(J, "invalid page box name");
	}

	fz_try(ctx)
		if (extra)
			pixmap = pdf_new_pixmap_from_page_with_usage(ctx, page, ctm, colorspace, alpha, usage, box);
		else
			pixmap = pdf_new_pixmap_from_page_contents_with_usage(ctx, page, ctm, colorspace, alpha, usage, box);

	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_PDFPage_getTransform(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = pdf_page_from_fz_page(ctx, ffi_topage(J, 0));
	fz_matrix ctm;

	fz_try(ctx)
		pdf_page_transform(ctx, page, NULL, &ctm);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushmatrix(J, ctm);
}

static void ffi_PDFAnnotation_getBounds(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_rect bounds;

	fz_try(ctx)
		bounds = pdf_bound_annot(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushrect(J, bounds);
}

static void ffi_PDFAnnotation_run(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_device *device = NULL;
	fz_matrix ctm = ffi_tomatrix(J, 2);

	if (js_isuserdata(J, 1, "fz_device")) {
		device = js_touserdata(J, 1, "fz_device");
		fz_try(ctx)
			pdf_run_annot(ctx, annot, device, ctm, NULL);
		fz_catch(ctx)
			rethrow(J);
	} else {
		device = new_js_device(ctx, J);
		js_copy(J, 1); /* put the js device on the top so the callbacks know where to get it */
		fz_try(ctx)
			pdf_run_annot(ctx, annot, device, ctm, NULL);
		fz_always(ctx)
			fz_drop_device(ctx, device);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_toDisplayList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_display_list *list = NULL;

	fz_try(ctx)
		list = pdf_new_display_list_from_annot(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_display_list");
	js_newuserdata(J, "fz_display_list", list, ffi_gc_fz_display_list);
}

static void ffi_PDFAnnotation_toPixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_colorspace *colorspace = js_touserdata(J, 2, "fz_colorspace");
	int alpha = js_toboolean(J, 3);
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		pixmap = pdf_new_pixmap_from_annot(ctx, annot, ctm, colorspace, NULL, alpha);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_PDFAnnotation_getObject(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	pdf_obj *obj;

	obj = pdf_annot_obj(ctx, annot);
	ffi_pushobj(J, pdf_keep_obj(ctx, obj));
}

static void ffi_PDFAnnotation_getType(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int type;
	const char *typestr = NULL;
	fz_try(ctx)
	{
		type = pdf_annot_type(ctx, annot);
		typestr = pdf_string_from_annot_type(ctx, type);
	}
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, typestr);
}

static void ffi_PDFAnnotation_getFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int flags = 0;
	fz_try(ctx)
		flags = pdf_annot_flags(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, flags);
}

static void ffi_PDFAnnotation_setFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int flags = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getContents(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *contents = NULL;

	fz_try(ctx)
		contents = pdf_annot_contents(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, contents);
}

static void ffi_PDFAnnotation_setContents(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *contents = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, contents);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getRect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_rect rect;
	fz_try(ctx)
		rect = pdf_annot_rect(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushrect(J, rect);
}

static void ffi_PDFAnnotation_setRect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_rect rect = ffi_torect(J, 1);
	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getBorder(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float border = 0;
	fz_try(ctx)
		border = pdf_annot_border(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, border);
}

static void ffi_PDFAnnotation_setBorder(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float border = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_border(ctx, annot, border);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = 0;
	float color[4];
	fz_try(ctx)
		pdf_annot_color(ctx, annot, &n, color);
	fz_catch(ctx)
		rethrow(J);
	js_newarray(J);
	for (i = 0; i < n; ++i) {
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFAnnotation_setColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = js_getlength(J, 1);
	float color[4];
	for (i = 0; i < n && i < 4; ++i) {
		js_getindex(J, 1, i);
		color[i] = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	fz_try(ctx)
		pdf_set_annot_color(ctx, annot, n, color);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasInteriorColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_interior_color(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getInteriorColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = 0;
	float color[4];
	fz_try(ctx)
		pdf_annot_interior_color(ctx, annot, &n, color);
	fz_catch(ctx)
		rethrow(J);
	js_newarray(J);
	for (i = 0; i < n; ++i) {
		js_pushnumber(J, color[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFAnnotation_setInteriorColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = js_getlength(J, 1);
	float color[4];
	for (i = 0; i < n && i < 4; ++i) {
		js_getindex(J, 1, i);
		color[i] = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	fz_try(ctx)
		pdf_set_annot_interior_color(ctx, annot, n, color);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getOpacity(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float opacity;
	fz_try(ctx)
		opacity = pdf_annot_opacity(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, opacity);
}

static void ffi_PDFAnnotation_setOpacity(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float opacity = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_opacity(ctx, annot, opacity);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasQuadPoints(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_quad_points(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getQuadPoints(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_quad q;
	int i, n = 0;

	fz_try(ctx)
		n = pdf_annot_quad_point_count(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		fz_try(ctx)
			q = pdf_annot_quad_point(ctx, annot, i);
		fz_catch(ctx)
			rethrow(J);
		ffi_pushquad(J, q);
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFAnnotation_setQuadPoints(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_quad *qp = NULL;
	int i, n;

	n = js_getlength(J, 1);

	fz_try(ctx)
		qp = fz_malloc_array(ctx, n, fz_quad);
	fz_catch(ctx)
		rethrow(J);

	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		qp[i] = ffi_toquad(J, -1);
		js_pop(J, 1);
	}

	fz_try(ctx)
		pdf_set_annot_quad_points(ctx, annot, n, qp);
	fz_always(ctx)
		fz_free(ctx, qp);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_clearQuadPoints(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);

	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addQuadPoint(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_quad q = ffi_toquad(J, 1);

	fz_try(ctx)
		pdf_add_annot_quad_point(ctx, annot, q);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasVertices(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_vertices(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getVertices(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point p;
	int i, n = 0;

	fz_try(ctx)
		n = pdf_annot_vertex_count(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		fz_try(ctx)
			p = pdf_annot_vertex(ctx, annot, i);
		fz_catch(ctx)
			rethrow(J);
		ffi_pushpoint(J, p);
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFAnnotation_setVertices(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point p;
	int i, n;

	n = js_getlength(J, 1);

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		p = ffi_topoint(J, -1);
		js_pop(J, 1);

		fz_try(ctx)
			pdf_add_annot_vertex(ctx, annot, p);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_clearVertices(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addVertex(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point p = ffi_topoint(J, 1);

	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, p);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_ink_list(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

/* Returns an array of strokes, where each stroke is an array of points, where
each point is a two element array consisting of the point's x and y coordinates. */
static void ffi_PDFAnnotation_getInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, k, m = 0, n = 0;
	fz_point pt;

	js_newarray(J);

	fz_try(ctx)
		n = pdf_annot_ink_list_count(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	for (i = 0; i < n; ++i) {
		fz_try(ctx)
			m = pdf_annot_ink_list_stroke_count(ctx, annot, i);
		fz_catch(ctx)
			rethrow(J);

		js_newarray(J);
		for (k = 0; k < m; ++k) {
			fz_try(ctx)
				pt = pdf_annot_ink_list_stroke_vertex(ctx, annot, i, k);
			fz_catch(ctx)
				rethrow(J);
			ffi_pushpoint(J, pt);
			js_setindex(J, -2, k);
		}
		js_setindex(J, -2, i);
	}
}

#define MAX_INK_STROKE 256
#define MAX_INK_POINT 16384

/* Takes an argument on the same format as getInkList returns. */
static void ffi_PDFAnnotation_setInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point *points = NULL;
	int *counts = NULL;
	int n, m, nv, k, i, v;

	fz_var(counts);
	fz_var(points);

	n = js_getlength(J, 1);
	if (n > MAX_INK_STROKE)
		js_rangeerror(J, "too many strokes in ink annotation");
	nv = 0;
	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		m = js_getlength(J, -1);
		if (m > MAX_INK_POINT)
			js_rangeerror(J, "too many points in ink annotation stroke");
		nv += m;
		if (nv > MAX_INK_POINT)
			js_rangeerror(J, "too many points in ink annotation");
		js_pop(J, 1);
	}

	fz_try(ctx) {
		counts = fz_malloc(ctx, n * sizeof(int));
		points = fz_malloc(ctx, nv * sizeof(fz_point));
	} fz_catch(ctx) {
		fz_free(ctx, counts);
		fz_free(ctx, points);
		rethrow(J);
	}

	if (js_try(J)) {
		fz_free(ctx, counts);
		fz_free(ctx, points);
		js_throw(J);
	}
	for (i = v = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		counts[i] = js_getlength(J, -1);
		for (k = 0; k < counts[i]; ++k) {
			js_getindex(J, -1, k);
			points[v] = ffi_topoint(J, -1);
			js_pop(J, 1);
			++v;
		}
		js_pop(J, 1);
	}
	js_endtry(J);

	fz_try(ctx)
		pdf_set_annot_ink_list(ctx, annot, n, counts, points);
	fz_always(ctx) {
		fz_free(ctx, counts);
		fz_free(ctx, points);
	}
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_clearInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_try(ctx)
		pdf_clear_annot_ink_list(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}
/* Takes a stroke argument being an array of points, where each
point is a two element array of the point's x and y coordinates. */
static void ffi_PDFAnnotation_addInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = js_getlength(J, 1);
	fz_point pt;

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		pt = ffi_topoint(J, -1);
		fz_try(ctx)
			pdf_add_annot_ink_list_stroke_vertex(ctx, annot, pt);
		fz_catch(ctx)
			rethrow(J);
		js_pop(J, 1);
	}
}

static void ffi_PDFAnnotation_addInkListStroke(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

/* Takes a point argument which is a two element array
consisting of the point's x and y coordinates. */
static void ffi_PDFAnnotation_addInkListStrokeVertex(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point pt = ffi_topoint(J, 1);
	fz_try(ctx)
		pdf_add_annot_ink_list_stroke_vertex(ctx, annot, pt);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasAuthor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_author(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getAuthor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *author = NULL;

	fz_try(ctx)
		author = pdf_annot_author(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, author);
}

static void ffi_PDFAnnotation_setAuthor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *author = js_tostring(J, 1);

	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getCreationDate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	double time;

	fz_try(ctx)
		time = pdf_annot_creation_date(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_getglobal(J, "Date");
	js_pushnumber(J, time * 1000);
	js_construct(J, 1);
}

static void ffi_PDFAnnotation_setCreationDate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	double time = js_tonumber(J, 1);

	fz_try(ctx)
		pdf_set_annot_creation_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getModificationDate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	double time;

	fz_try(ctx)
		time = pdf_annot_modification_date(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_getglobal(J, "Date");
	js_pushnumber(J, time * 1000);
	js_construct(J, 1);
}

static void ffi_PDFAnnotation_setModificationDate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	double time = js_tonumber(J, 1);

	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasLineEndingStyles(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_line_ending_styles(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getLineEndingStyles(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	enum pdf_line_ending start, end;

	js_newarray(J);

	fz_try(ctx)
		pdf_annot_line_ending_styles(ctx, annot, &start, &end);
	fz_catch(ctx)
		rethrow(J);

	js_newobject(J);
	js_pushliteral(J, string_from_line_ending(start));
	js_setproperty(J, -2, "start");
	js_pushliteral(J, string_from_line_ending(end));
	js_setproperty(J, -2, "end");
}

static void ffi_PDFAnnotation_setLineEndingStyles(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	enum pdf_line_ending start = line_ending_from_string(js_tostring(J, 1));
	enum pdf_line_ending end = line_ending_from_string(js_tostring(J, 2));

	fz_try(ctx)
		pdf_set_annot_line_ending_styles(ctx, annot, start, end);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasBorder(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_border(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getBorderWidth(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float width;
	fz_try(ctx)
		width = pdf_annot_border_width(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, width);
}

static void ffi_PDFAnnotation_setBorderWidth(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float width = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_border_width(ctx, annot, width);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getBorderStyle(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	enum pdf_border_style style;
	fz_try(ctx)
		style = pdf_annot_border_style(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, string_from_border_style(style));
}

static void ffi_PDFAnnotation_setBorderStyle(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *str = js_iscoercible(J, 1) ? js_tostring(J, 1) : "Solid";
	enum pdf_border_style style = border_style_from_string(str);
	fz_try(ctx)
		pdf_set_annot_border_style(ctx, annot, style);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getBorderDashCount(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int count;
	fz_try(ctx)
		count = pdf_annot_border_dash_count(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, count);
}

static void ffi_PDFAnnotation_getBorderDashItem(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i = js_tointeger(J, 1);
	float length;
	fz_try(ctx)
		length = pdf_annot_border_dash_item(ctx, annot, i);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, length);
}

static void ffi_PDFAnnotation_clearBorderDash(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_try(ctx)
		pdf_clear_annot_border_dash(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addBorderDashItem(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float length = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_add_annot_border_dash_item(ctx, annot, length);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_setBorderDashPattern(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int i, n = js_getlength(J, 1);
	float length;

	fz_try(ctx)
		pdf_clear_annot_border_dash(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	for (i = 0; i < n; ++i)
	{
		js_getindex(J, 1, i);
		length = js_tonumber(J, -1);
		js_pop(J, 1);
		fz_try(ctx)
			pdf_add_annot_border_dash_item(ctx, annot, length);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_hasBorderEffect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_border_effect(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getBorderEffect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	enum pdf_border_effect effect;
	fz_try(ctx)
		effect = pdf_annot_border_effect(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, string_from_border_effect(effect));
}

static void ffi_PDFAnnotation_setBorderEffect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *str = js_iscoercible(J, 1) ? js_tostring(J, 1) : "None";
	enum pdf_border_effect effect = border_effect_from_string(str);
	fz_try(ctx)
		pdf_set_annot_border_effect(ctx, annot, effect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getBorderEffectIntensity(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float intensity;
	fz_try(ctx)
		intensity = pdf_annot_border_effect_intensity(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, intensity);
}

static void ffi_PDFAnnotation_setBorderEffectIntensity(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	float intensity = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_border_effect_intensity(ctx, annot, intensity);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasIcon(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_icon_name(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getIcon(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *name;
	fz_try(ctx)
		name = pdf_annot_icon_name(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, name);
}

static void ffi_PDFAnnotation_setIcon(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *name = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_annot_icon_name(ctx, annot, name);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getQuadding(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int quadding;
	fz_try(ctx)
		quadding = pdf_annot_quadding(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, quadding);
}

static void ffi_PDFAnnotation_setQuadding(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int quadding = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_quadding(ctx, annot, quadding);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getLanguage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	char lang[8];
	fz_try(ctx)
		fz_string_from_text_language(lang, pdf_annot_language(ctx, annot));
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, lang);
}

static void ffi_PDFAnnotation_setLanguage(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *lang = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_annot_language(ctx, annot, fz_text_language_from_string(lang));
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasLine(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_line(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

/* Returns an array of two point denoting the end points of the
line annotation. Each point is a two element array consisting
of the point's x and y coordinates. */
static void ffi_PDFAnnotation_getLine(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point a, b;
	fz_try(ctx)
		pdf_annot_line(ctx, annot, &a, &b);
	fz_catch(ctx)
		rethrow(J);
	js_newarray(J);
	ffi_pushpoint(J, a);
	js_setindex(J, -2, 0);
	ffi_pushpoint(J, b);
	js_setindex(J, -2, 1);
}

/* Takes two point arguments denoting the end points of the
line annotation. Each point is a two element array consisting
of the point's x and y coordinates. */
static void ffi_PDFAnnotation_setLine(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_point a = ffi_topoint(J, 1);
	fz_point b = ffi_topoint(J, 2);
	fz_try(ctx)
		pdf_set_annot_line(ctx, annot, a, b);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getDefaultAppearance(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *font = NULL;
	float size = 0.0f;
	float color[4] = { 0.0f };
	int n = 0;
	fz_try(ctx)
		pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	fz_catch(ctx)
		rethrow(J);
	js_newobject(J);
	js_pushstring(J, font);
	js_setproperty(J, -2, "font");
	js_pushnumber(J, size);
	js_setproperty(J, -2, "size");
	if (n > 0)
		ffi_pusharray(J, color, n);
	else
		js_pushundefined(J);
	js_setproperty(J, -2, "color");
}

static void ffi_PDFAnnotation_setDefaultAppearance(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *font = js_tostring(J, 1);
	float size = js_tonumber(J, 2);
	int i, n = js_getlength(J, 3);
	float color[4] = { 0.0f };
	for (i = 0; i < n && i < (int) nelem(color); ++i) {
		js_getindex(J, 3, i);
		color[i] = js_tonumber(J, -1);
		js_pop(J, 1);
	}
	fz_try(ctx)
		pdf_set_annot_default_appearance(ctx, annot, font, size, n, color);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_setAppearance(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	const char *appearance = js_iscoercible(J, 1) ? js_tostring(J, 1) : NULL;
	const char *state = js_iscoercible(J, 2) ? js_tostring(J, 2) : NULL;
	fz_matrix ctm = ffi_tomatrix(J, 3);

	if (js_isarray(J, 4))
	{
		const char *contents;
		pdf_document *pdf;
		fz_buffer *buf;
		pdf_obj *res;
		fz_rect bbox;

		fz_try(ctx)
			pdf = pdf_get_bound_document(ctx, pdf_annot_obj(ctx, annot));
		fz_catch(ctx)
			rethrow(J);

		bbox = ffi_torect(J, 4);
		res = ffi_toobj(J, pdf, 5);
		contents = js_tostring(J, 6);

		fz_var(buf);

		fz_try(ctx)
		{
			buf = fz_new_buffer_from_copied_data(ctx, (unsigned char *) contents, strlen(contents));
			pdf_set_annot_appearance(ctx, annot, appearance, state, ctm, bbox, res, buf);
		}
		fz_always(ctx)
		{
			fz_drop_buffer(ctx, buf);
			pdf_drop_obj(ctx, res);
		}
		fz_catch(ctx)
			rethrow(J);
	}
	else
	{
		fz_display_list *list = js_touserdata(J, 4, "fz_display_list");
		fz_try(ctx)
			pdf_set_annot_appearance_from_display_list(ctx, annot, appearance, state, ctm, list);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_hasFilespec(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_filespec(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getFilespec(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	pdf_obj *fs = NULL;

	fz_try(ctx)
		fs = pdf_annot_filespec(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushobj(J, fs);
}

static void ffi_PDFAnnotation_setFilespec(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	pdf_page *page = pdf_annot_page(ctx, annot);
	pdf_document *pdf = page->doc;
	pdf_obj *fs = ffi_toobj(J, pdf, 1);

	fz_try(ctx)
		pdf_set_annot_filespec(ctx, annot, fs);
	fz_always(ctx)
		pdf_drop_obj(ctx, fs);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_requestResynthesis(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_try(ctx)
		pdf_annot_request_resynthesis(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_requestSynthesis(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	fz_try(ctx)
		pdf_annot_request_synthesis(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_update(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int changed = 0;
	fz_try(ctx)
		changed = pdf_update_annot(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, changed);
}

static void ffi_PDFAnnotation_applyRedaction(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	pdf_redact_options opts = { 1, PDF_REDACT_IMAGE_PIXELS };
	if (js_isdefined(J, 1)) opts.black_boxes = js_toboolean(J, 1);
	if (js_isdefined(J, 2)) opts.image_method = js_tointeger(J, 2);
	fz_try(ctx)
		pdf_apply_redaction(ctx, annot, &opts);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_process(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	pdf_processor *proc = new_js_processor(ctx, J);
	fz_try(ctx)
	{
		pdf_processor_push_resources(ctx, proc, pdf_page_resources(ctx, pdf_annot_page(ctx, annot)));
		pdf_process_annot(ctx, proc, annot, NULL);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
	{
		pdf_processor_pop_resources(ctx, proc);
		pdf_drop_processor(ctx, proc);
	}
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getHot(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int hot;
	fz_try(ctx)
		hot = pdf_annot_hot(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, hot);
}

static void ffi_PDFAnnotation_setHot(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int hot = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_hot(ctx, annot, hot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_hasOpen(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int has;
	fz_try(ctx)
		has = pdf_annot_has_open(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, has);
}

static void ffi_PDFAnnotation_getIsOpen(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int isopen;
	fz_try(ctx)
		isopen = pdf_annot_is_open(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, isopen);
}

static void ffi_PDFAnnotation_setIsOpen(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = ffi_toannot(J, 0);
	int isopen = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_is_open(ctx, annot, isopen);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getHiddenForEditing(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int hidden = 0;

	fz_try(ctx)
		hidden = pdf_annot_hidden_for_editing(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, hidden);
}

static void ffi_PDFAnnotation_setHiddenForEditing(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int hidden = js_toboolean(J, 1);

	fz_try(ctx)
		pdf_set_annot_hidden_for_editing(ctx, annot, hidden);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_getFieldType(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	const char *type;
	fz_try(ctx)
		type = pdf_field_type_string(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, type);
}

static void ffi_PDFWidget_getFieldFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int flags;
	fz_try(ctx)
		flags = pdf_field_flags(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, flags);
}

static void ffi_PDFWidget_getValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value;
	fz_try(ctx)
		value = pdf_annot_field_value(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, value);
}

static void ffi_PDFWidget_setTextValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_text_field_value(ctx, widget, value);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_setChoiceValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_choice_field_value(ctx, widget, value);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_toggle(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int changed = 0;
	fz_try(ctx)
		changed = pdf_toggle_widget(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, changed);
}

static void ffi_PDFWidget_getMaxLen(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int maxLen = 0;
	fz_try(ctx)
		maxLen = pdf_text_widget_max_len(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, maxLen);
}

static void ffi_PDFWidget_getOptions(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int export = js_toboolean(J, 1);
	const char *opt;
	int i, n;
	fz_try(ctx)
		n = pdf_choice_field_option_count(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		rethrow(J);
	js_newarray(J);
	for (i = 0; i < n; ++i) {
		fz_try(ctx)
			opt = pdf_choice_field_option(ctx, pdf_annot_obj(ctx, widget), export, i);
		fz_catch(ctx)
			rethrow(J);
		js_pushstring(J, opt);
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFWidget_eventEnter(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_enter(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventExit(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_exit(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventDown(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_down(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventUp(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_up(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventFocus(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_focus(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventBlur(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_blur(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_validateSignature(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int val = 0;
	fz_try(ctx)
		val = pdf_validate_signature(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, val);
}

static void ffi_PDFWidget_checkCertificate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	pdf_pkcs7_verifier *verifier = NULL;
	int val = 0;
	fz_var(verifier);
	fz_try(ctx)
	{
		verifier = pkcs7_openssl_new_verifier(ctx);
		val = pdf_check_widget_certificate(ctx, verifier, widget);
	}
	fz_always(ctx)
		pdf_drop_verifier(ctx, verifier);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, pdf_signature_error_description(val));
}

static void ffi_PDFWidget_checkDigest(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	pdf_pkcs7_verifier *verifier = NULL;
	pdf_signature_error val = 0;
	fz_var(verifier);
	fz_try(ctx)
	{
		verifier = pkcs7_openssl_new_verifier(ctx);
		val = pdf_check_widget_digest(ctx, verifier, widget);
	}
	fz_always(ctx)
		pdf_drop_verifier(ctx, verifier);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, pdf_signature_error_description(val));
}

static void ffi_PDFWidget_getSignatory(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	pdf_pkcs7_verifier *verifier = NULL;
	pdf_pkcs7_distinguished_name *dn = NULL;
	char buf[500];
	fz_var(verifier);
	fz_var(dn);
	fz_try(ctx)
	{
		verifier = pkcs7_openssl_new_verifier(ctx);
		dn = pdf_signature_get_widget_signatory(ctx, verifier, widget);
		if (dn)
		{
			char *s = pdf_signature_format_distinguished_name(ctx, dn);
			fz_strlcpy(buf, s, sizeof buf);
			fz_free(ctx, s);
		}
		else
		{
			fz_strlcpy(buf, "Signature information missing.", sizeof buf);
		}
	}
	fz_always(ctx)
	{
		pdf_signature_drop_distinguished_name(ctx, dn);
		pdf_drop_verifier(ctx, verifier);
	}
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, buf);
}

static void ffi_PDFWidget_isSigned(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int val = 0;
	fz_try(ctx)
		val = pdf_widget_is_signed(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static void ffi_PDFWidget_isReadOnly(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int val = 0;
	fz_try(ctx)
		val = pdf_widget_is_readonly(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, val);
}

static int check_option(js_State *J, int idx, const char *name) {
	int result = 0;
	if (js_hasproperty(J, idx, name)) {
		if (js_toboolean(J, -1))
			result = 1;
		js_pop(J, 1);
	}
	return result;
}

static void ffi_PDFWidget_sign(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	pdf_pkcs7_signer *signer = js_touserdata(J, 1, "pdf_pkcs7_signer");
	int flags = 0;
	fz_image *graphic = js_iscoercible(J, 3) ? js_touserdata(J, 3, "fz_image") : NULL;
	const char *reason = js_iscoercible(J, 4) ? js_tostring(J, 4) : NULL;
	const char *location = js_iscoercible(J, 5) ? js_tostring(J, 5) : NULL;

	if (js_isobject(J, 2)) {
		if (check_option(J, 2, "showLabels"))
			flags |= PDF_SIGNATURE_SHOW_LABELS;
		if (check_option(J, 2, "showDN"))
			flags |= PDF_SIGNATURE_SHOW_DN;
		if (check_option(J, 2, "showDate"))
			flags |= PDF_SIGNATURE_SHOW_DATE;
		if (check_option(J, 2, "showTextName"))
			flags |= PDF_SIGNATURE_SHOW_TEXT_NAME;
		if (check_option(J, 2, "showGraphicName"))
			flags |= PDF_SIGNATURE_SHOW_GRAPHIC_NAME;
		if (check_option(J, 2, "showLogo"))
			flags |= PDF_SIGNATURE_SHOW_LOGO;
	} else {
		flags = PDF_SIGNATURE_DEFAULT_APPEARANCE;
	}

	fz_try(ctx)
		pdf_sign_signature(ctx, widget, signer,
			flags,
			graphic,
			reason,
			location);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_previewSignature(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	pdf_pkcs7_signer *signer = NULL;
	int flags = 0;
	fz_image *graphic = js_iscoercible(J, 3) ? js_touserdata(J, 3, "fz_image") : NULL;
	const char *reason = js_iscoercible(J, 4) ? js_tostring(J, 4) : NULL;
	const char *location = js_iscoercible(J, 5) ? js_tostring(J, 5) : NULL;
	fz_pixmap *pixmap;

	if (js_isuserdata(J, 1, "pdf_pkcs7_signer"))
		signer = js_touserdata(J, 1, "pdf_pkcs7_signer");

	if (js_isobject(J, 2)) {
		if (check_option(J, 2, "showLabels"))
			flags |= PDF_SIGNATURE_SHOW_LABELS;
		if (check_option(J, 2, "showDN"))
			flags |= PDF_SIGNATURE_SHOW_DN;
		if (check_option(J, 2, "showDate"))
			flags |= PDF_SIGNATURE_SHOW_DATE;
		if (check_option(J, 2, "showTextName"))
			flags |= PDF_SIGNATURE_SHOW_TEXT_NAME;
		if (check_option(J, 2, "showGraphicName"))
			flags |= PDF_SIGNATURE_SHOW_GRAPHIC_NAME;
		if (check_option(J, 2, "showLogo"))
			flags |= PDF_SIGNATURE_SHOW_LOGO;
	} else {
		flags = PDF_SIGNATURE_DEFAULT_APPEARANCE;
	}

	fz_try(ctx) {
		fz_rect rect = pdf_annot_rect(ctx, widget);
		fz_text_language lang = pdf_annot_language(ctx, widget);
		pixmap = pdf_preview_signature_as_pixmap(ctx,
			rect.x1-rect.x0, rect.y1-rect.y0, lang,
			signer,
			flags,
			graphic,
			reason,
			location);
	}
	fz_catch(ctx)
		rethrow(J);

	ffi_pushpixmap(J, pixmap);
}

static void ffi_PDFWidget_getEditingState(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int state = 0;

	fz_try(ctx)
		state = pdf_get_widget_editing_state(ctx, widget);
	fz_catch(ctx)
		rethrow(J);

	js_pushboolean(J, state);
}

static void ffi_PDFWidget_setEditingState(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	int state = js_toboolean(J, 1);

	fz_try(ctx)
		pdf_set_widget_editing_state(ctx, widget, state);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_clearSignature(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");

	fz_try(ctx)
		pdf_clear_signature(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_getLabel(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	const char *label = NULL;

	fz_try(ctx)
		label = pdf_annot_field_label(ctx, widget);
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, label);
}

static void ffi_PDFWidget_getName(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	char *name = NULL;

	fz_try(ctx)
		name = pdf_load_field_name(ctx, pdf_annot_obj(ctx, widget));
	fz_catch(ctx)
		rethrow(J);

	js_pushstring(J, name);

	fz_free(ctx, name);
}

static void ffi_PDFWidget_layoutTextWidget(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *widget = js_touserdata(J, 0, "pdf_widget");
	fz_layout_block *layout = NULL;
	fz_layout_line *line = NULL;
	fz_layout_char *chr = NULL;
	fz_rect bounds;
	fz_matrix mat;
	const char *s;
	int i, k;

	fz_try(ctx)
	{
		bounds = pdf_bound_widget(ctx, widget);
		layout = pdf_layout_text_widget(ctx, widget);
		mat = fz_concat(layout->inv_matrix, fz_translate(-bounds.x0, -bounds.y0));
	}
	fz_catch(ctx)
		rethrow(J);

	if (js_try(J)) {
		fz_drop_layout(ctx, layout);
		js_throw(J);
	}

	js_newobject(J);
	ffi_pushmatrix(J, layout->matrix);
	js_setproperty(J, -2, "matrix");
	ffi_pushmatrix(J, layout->inv_matrix);
	js_setproperty(J, -2, "invMatrix");

	s = layout->head->p;

	js_newarray(J);
	for (line = layout->head, i = 0; line; line = line->next, i++)
	{
		float y = line->y - line->font_size * 0.2f;
		float b = line->y + line->font_size;
		fz_rect lrect = fz_make_rect(line->x, y, line->x, b);
		lrect = fz_transform_rect(lrect, mat);

		js_newobject(J);
		js_pushnumber(J, line->x);
		js_setproperty(J, -2, "x");
		js_pushnumber(J, line->y);
		js_setproperty(J, -2, "y");
		js_pushnumber(J, line->font_size);
		js_setproperty(J, -2, "fontSize");
		js_pushnumber(J, fz_runeidx(s, line->p));
		js_setproperty(J, -2, "index");

		js_newarray(J);
		for (chr = line->text, k = 0; chr; chr = chr->next, k++)
		{
			fz_rect crect = fz_make_rect(chr->x, y, chr->x + chr->advance, b);
			crect = fz_transform_rect(crect, mat);
			lrect = fz_union_rect(lrect, crect);

			js_newobject(J);
			js_pushnumber(J, chr->x);
			js_setproperty(J, -2, "x");
			js_pushnumber(J, chr->advance);
			js_setproperty(J, -2, "advance");
			js_pushnumber(J, fz_runeidx(s, chr->p));
			js_setproperty(J, -2, "index");
			ffi_pushrect(J, crect);
			js_setproperty(J, -2, "rect");
			js_setindex(J, -2, k);
		}
		js_setproperty(J, -2, "chars");

		ffi_pushrect(J, lrect);
		js_setproperty(J, -2, "rect");

		js_setindex(J, -2, i);
	}
	js_setproperty(J, -2, "lines");

	js_endtry(J);
	fz_drop_layout(ctx, layout);
}

static void ffi_new_PDFPKCS7Signer(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_pkcs7_signer *signer = NULL;
	const char *filename = js_tostring(J, 1);
	const char *password = js_tostring(J, 2);
	fz_try(ctx)
		signer = pkcs7_openssl_read_pfx(ctx, filename, password);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushsigner(J, signer);
}

#endif /* FZ_ENABLE_PDF */

int murun_main(int argc, char **argv)
{
	fz_context *ctx;
	js_State *J;
	int i;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_register_document_handlers(ctx);

	J = js_newstate(alloc, ctx, JS_STRICT);
	if (!J)
	{
		fprintf(stderr, "cannot initialize mujs state\n");
		fz_drop_context(ctx);
		exit(1);
	}
	js_setcontext(J, ctx);

	if (js_try(J))
	{
		fprintf(stderr, "cannot initialize mujs functions\n");
		js_freestate(J);
		fz_drop_context(ctx);
		exit(1);
	}

	/* standard command line javascript functions */

	js_newcfunction(J, jsB_gc, "gc", 0);
	js_setglobal(J, "gc");

	js_newcfunction(J, jsB_load, "load", 1);
	js_setglobal(J, "load");

	js_newcfunction(J, jsB_print, "print", 1);
	js_setglobal(J, "print");

	js_newcfunction(J, jsB_write, "write", 0);
	js_setglobal(J, "write");

	js_newcfunction(J, jsB_read, "read", 1);
	js_setglobal(J, "read");

	js_newcfunction(J, jsB_readline, "readline", 0);
	js_setglobal(J, "readline");

	js_newcfunction(J, jsB_repr, "repr", 1);
	js_setglobal(J, "repr");

	js_newcfunction(J, jsB_quit, "quit", 1);
	js_setglobal(J, "quit");

	js_dostring(J, prefix_js);

	/* mupdf module */

	/* Create superclass for all userdata objects */
	js_dostring(J, "function Userdata() { throw new Error('Userdata is not callable'); }");
	js_getglobal(J, "Userdata");
	js_getproperty(J, -1, "prototype");
	js_setregistry(J, "Userdata");
	js_pop(J, 1);

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Archive.getFormat", ffi_Archive_getFormat, 0);
		jsB_propfun(J, "Archive.countEntries", ffi_Archive_countEntries, 0);
		jsB_propfun(J, "Archive.listEntry", ffi_Archive_listEntry, 1);
		jsB_propfun(J, "Archive.hasEntry", ffi_Archive_hasEntry, 1);
		jsB_propfun(J, "Archive.readEntry", ffi_Archive_readEntry, 1);
	}
	js_setregistry(J, "fz_archive");

	js_getregistry(J, "fz_archive");
	js_newobjectx(J);
	{
		jsB_propfun(J, "MultiArchive.mountArchive", ffi_MultiArchive_mountArchive, 2);
	}
	js_setregistry(J, "fz_multi_archive");

	js_getregistry(J, "fz_archive");
	js_newobjectx(J);
	{
		jsB_propfun(J, "TreeArchive.add", ffi_TreeArchive_add, 2);
	}
	js_setregistry(J, "fz_tree_archive");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Buffer.writeByte", ffi_Buffer_writeByte, 1);
		jsB_propfun(J, "Buffer.writeRune", ffi_Buffer_writeRune, 1);
		jsB_propfun(J, "Buffer.writeLine", ffi_Buffer_writeLine, 1);
		jsB_propfun(J, "Buffer.writeBuffer", ffi_Buffer_writeBuffer, 1);
		jsB_propfun(J, "Buffer.write", ffi_Buffer_write, 1);
		jsB_propfun(J, "Buffer.save", ffi_Buffer_save, 1);
		jsB_propfun(J, "Buffer.slice", ffi_Buffer_slice, 2);
	}
	js_setregistry(J, "fz_buffer");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Document.isPDF", ffi_Document_isPDF, 0);
		jsB_propfun(J, "Document.needsPassword", ffi_Document_needsPassword, 0);
		jsB_propfun(J, "Document.authenticatePassword", ffi_Document_authenticatePassword, 1);
		jsB_propfun(J, "Document.hasPermission", ffi_Document_hasPermission, 1);
		jsB_propfun(J, "Document.getMetaData", ffi_Document_getMetaData, 1);
		jsB_propfun(J, "Document.setMetaData", ffi_Document_setMetaData, 2);
		jsB_propfun(J, "Document.resolveLink", ffi_Document_resolveLink, 1);
		jsB_propfun(J, "Document.resolveLinkDestination", ffi_Document_resolveLinkDestination, 1);
		jsB_propfun(J, "Document.formatLinkURI", ffi_Document_formatLinkURI, 1);
		jsB_propfun(J, "Document.isReflowable", ffi_Document_isReflowable, 0);
		jsB_propfun(J, "Document.layout", ffi_Document_layout, 3);
		jsB_propfun(J, "Document.countPages", ffi_Document_countPages, 0);
		jsB_propfun(J, "Document.loadPage", ffi_Document_loadPage, 1);
		jsB_propfun(J, "Document.loadOutline", ffi_Document_loadOutline, 0);
		jsB_propfun(J, "Document.outlineIterator", ffi_Document_outlineIterator, 0);
	}
	js_setregistry(J, "fz_document");

	js_newobject(J);
	{
		jsB_propfun(J, "Document.openDocument", ffi_Document_openDocument, 2);
	}
	js_setglobal(J, "Document");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Story.place", ffi_Story_place, 1);
		jsB_propfun(J, "Story.draw", ffi_Story_draw, 2);
		jsB_propfun(J, "Story.document", ffi_Story_document, 0);
	}
	js_setregistry(J, "fz_story");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "DOM.body", ffi_DOM_body, 0);
		jsB_propfun(J, "DOM.documentElement", ffi_DOM_documentElement, 0);
		jsB_propfun(J, "DOM.createElement", ffi_DOM_createElement, 1);
		jsB_propfun(J, "DOM.createTextNode", ffi_DOM_createTextNode, 1);
		jsB_propfun(J, "DOM.find", ffi_DOM_find, 3);
		jsB_propfun(J, "DOM.findNext", ffi_DOM_findNext, 3);
		jsB_propfun(J, "DOM.appendChild", ffi_DOM_appendChild, 1);
		jsB_propfun(J, "DOM.insertBefore", ffi_DOM_insertBefore, 1);
		jsB_propfun(J, "DOM.insertAfter", ffi_DOM_insertAfter, 1);
		jsB_propfun(J, "DOM.remove", ffi_DOM_remove, 0);
		jsB_propfun(J, "DOM.clone", ffi_DOM_clone, 0);
		jsB_propfun(J, "DOM.firstChild", ffi_DOM_firstChild, 0);
		jsB_propfun(J, "DOM.parent", ffi_DOM_parent, 0);
		jsB_propfun(J, "DOM.next", ffi_DOM_next, 0);
		jsB_propfun(J, "DOM.previous", ffi_DOM_previous, 0);
		jsB_propfun(J, "DOM.addAttribute", ffi_DOM_addAttribute, 2);
		jsB_propfun(J, "DOM.removeAttribute", ffi_DOM_removeAttribute, 1);
		jsB_propfun(J, "DOM.attribute", ffi_DOM_attribute, 1);
		jsB_propfun(J, "DOM.getAttributes", ffi_DOM_getAttributes, 0);
	}
	js_setregistry(J, "fz_xml");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "OutlineIterator.item", ffi_OutlineIterator_item, 0);
		jsB_propfun(J, "OutlineIterator.next", ffi_OutlineIterator_next, 0);
		jsB_propfun(J, "OutlineIterator.prev", ffi_OutlineIterator_prev, 0);
		jsB_propfun(J, "OutlineIterator.up", ffi_OutlineIterator_up, 0);
		jsB_propfun(J, "OutlineIterator.down", ffi_OutlineIterator_down, 0);
		jsB_propfun(J, "OutlineIterator.insert", ffi_OutlineIterator_insert, 1);
		jsB_propfun(J, "OutlineIterator.delete", ffi_OutlineIterator_delete, 0);
		jsB_propfun(J, "OutlineIterator.update", ffi_OutlineIterator_update, 1);
	}
	js_setregistry(J, "fz_outline_iterator");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Page.isPDF", ffi_Page_isPDF, 0);
		jsB_propfun(J, "Page.getBounds", ffi_Page_getBounds, 0);
		jsB_propfun(J, "Page.run", ffi_Page_run, 2);
		jsB_propfun(J, "Page.run", ffi_Page_runPageContents, 2);
		jsB_propfun(J, "Page.run", ffi_Page_runPageAnnots, 2);
		jsB_propfun(J, "Page.run", ffi_Page_runPageWidgets, 2);

		jsB_propfun(J, "Page.toPixmap", ffi_Page_toPixmap, 4);
		jsB_propfun(J, "Page.toDisplayList", ffi_Page_toDisplayList, 1);
		jsB_propfun(J, "Page.toStructuredText", ffi_Page_toStructuredText, 1);
		jsB_propfun(J, "Page.search", ffi_Page_search, 0);
		jsB_propfun(J, "Page.getLinks", ffi_Page_getLinks, 0);
		jsB_propfun(J, "Page.createLink", ffi_Page_createLink, 2);
		jsB_propfun(J, "Page.deleteLink", ffi_Page_deleteLink, 1);
		jsB_propfun(J, "Page.getLabel", ffi_Page_getLabel, 0);
	}
	js_setregistry(J, "fz_page");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Link.getBounds", ffi_Link_getBounds, 0);
		jsB_propfun(J, "Link.setBounds", ffi_Link_setBounds, 1);
		jsB_propfun(J, "Link.getURI", ffi_Link_getURI, 0);
		jsB_propfun(J, "Link.setURI", ffi_Link_setURI, 1);
		jsB_propfun(J, "isExternal", ffi_Link_isExternal, 0);
	}
	js_setregistry(J, "fz_link");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Device.close", ffi_Device_close, 0);

		jsB_propfun(J, "Device.fillPath", ffi_Device_fillPath, 7);
		jsB_propfun(J, "Device.strokePath", ffi_Device_strokePath, 7);
		jsB_propfun(J, "Device.clipPath", ffi_Device_clipPath, 3);
		jsB_propfun(J, "Device.clipStrokePath", ffi_Device_clipStrokePath, 3);

		jsB_propfun(J, "Device.fillText", ffi_Device_fillText, 6);
		jsB_propfun(J, "Device.strokeText", ffi_Device_strokeText, 7);
		jsB_propfun(J, "Device.clipText", ffi_Device_clipText, 2);
		jsB_propfun(J, "Device.clipStrokeText", ffi_Device_clipStrokeText, 3);
		jsB_propfun(J, "Device.ignoreText", ffi_Device_ignoreText, 2);

		jsB_propfun(J, "Device.fillShade", ffi_Device_fillShade, 4);
		jsB_propfun(J, "Device.fillImage", ffi_Device_fillImage, 4);
		jsB_propfun(J, "Device.fillImageMask", ffi_Device_fillImageMask, 6);
		jsB_propfun(J, "Device.clipImageMask", ffi_Device_clipImageMask, 2);

		jsB_propfun(J, "Device.popClip", ffi_Device_popClip, 0);

		jsB_propfun(J, "Device.beginMask", ffi_Device_beginMask, 6);
		jsB_propfun(J, "Device.endMask", ffi_Device_endMask, 0);
		jsB_propfun(J, "Device.beginGroup", ffi_Device_beginGroup, 5);
		jsB_propfun(J, "Device.endGroup", ffi_Device_endGroup, 0);
		jsB_propfun(J, "Device.beginTile", ffi_Device_beginTile, 6);
		jsB_propfun(J, "Device.endTile", ffi_Device_endTile, 0);

		jsB_propfun(J, "Device.beginLayer", ffi_Device_beginLayer, 1);
		jsB_propfun(J, "Device.endLayer", ffi_Device_endLayer, 0);

		jsB_propfun(J, "Device.renderFlags", ffi_Device_renderFlags, 2);
		jsB_propfun(J, "Device.setDefaultColorSpaces", ffi_Device_setDefaultColorSpaces, 1);

		jsB_propfun(J, "Device.beginStructure", ffi_Device_beginStructure, 3);
		jsB_propfun(J, "Device.endStructure", ffi_Device_endStructure, 0);

		jsB_propfun(J, "Device.beginMetatext", ffi_Device_beginMetatext, 2);
		jsB_propfun(J, "Device.endMetatext", ffi_Device_endMetatext, 0);

	}
	js_setregistry(J, "fz_device");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "ColorSpace.getNumberOfComponents", ffi_ColorSpace_getNumberOfComponents, 0);
		jsB_propfun(J, "ColorSpace.getType", ffi_ColorSpace_getType, 0);
		jsB_propfun(J, "ColorSpace.toString", ffi_ColorSpace_toString, 0);
		jsB_propfun(J, "ColorSpace.isGray", ffi_ColorSpace_isGray, 0);
		jsB_propfun(J, "ColorSpace.isRGB", ffi_ColorSpace_isRGB, 0);
		jsB_propfun(J, "ColorSpace.isCMYK", ffi_ColorSpace_isCMYK, 0);
		jsB_propfun(J, "ColorSpace.isIndexed", ffi_ColorSpace_isIndexed, 0);
		jsB_propfun(J, "ColorSpace.isLab", ffi_ColorSpace_isLab, 0);
		jsB_propfun(J, "ColorSpace.isDeviceN", ffi_ColorSpace_isDeviceN, 0);
		jsB_propfun(J, "ColorSpace.isSubtractive", ffi_ColorSpace_isSubtractive, 0);
	}
	js_dup(J);
	js_setglobal(J, "ColorSpace");
	js_setregistry(J, "fz_colorspace");

	js_getglobal(J, "ColorSpace");
	{
		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_gray(ctx)), ffi_gc_fz_colorspace);
		js_dup(J);
		js_setregistry(J, "DeviceGray");
		js_setproperty(J, -2, "DeviceGray");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_rgb(ctx)), ffi_gc_fz_colorspace);
		js_dup(J);
		js_setregistry(J, "DeviceRGB");
		js_setproperty(J, -2, "DeviceRGB");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_bgr(ctx)), ffi_gc_fz_colorspace);
		js_dup(J);
		js_setregistry(J, "DeviceBGR");
		js_setproperty(J, -2, "DeviceBGR");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_cmyk(ctx)), ffi_gc_fz_colorspace);
		js_dup(J);
		js_setregistry(J, "DeviceCMYK");
		js_setproperty(J, -2, "DeviceCMYK");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_lab(ctx)), ffi_gc_fz_colorspace);
		js_dup(J);
		js_setregistry(J, "Lab");
		js_setproperty(J, -2, "Lab");
	}
	js_pop(J, 1);

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "DefaultColorSpaces.getDefaultGray", ffi_DefaultColorSpaces_getDefaultGray, 0);
		jsB_propfun(J, "DefaultColorSpaces.getDefaultRGB", ffi_DefaultColorSpaces_getDefaultRGB, 0);
		jsB_propfun(J, "DefaultColorSpaces.getDefaultCMYK", ffi_DefaultColorSpaces_getDefaultCMYK, 0);
		jsB_propfun(J, "DefaultColorSpaces.getOutputIntent", ffi_DefaultColorSpaces_getOutputIntent, 0);
		jsB_propfun(J, "DefaultColorSpaces.setDefaultGray", ffi_DefaultColorSpaces_setDefaultGray, 1);
		jsB_propfun(J, "DefaultColorSpaces.setDefaultRGB", ffi_DefaultColorSpaces_setDefaultRGB, 1);
		jsB_propfun(J, "DefaultColorSpaces.setDefaultCMYK", ffi_DefaultColorSpaces_setDefaultCMYK, 1);
		jsB_propfun(J, "DefaultColorSpaces.setOutputIntent", ffi_DefaultColorSpaces_setOutputIntent, 1);
	}
	js_dup(J);
	js_setglobal(J, "DefaultColorSpaces");
	js_setregistry(J, "fz_default_colorspaces");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Shade.getBounds", ffi_Shade_getBounds, 1);
	}
	js_setregistry(J, "fz_shade");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Image.getWidth", ffi_Image_getWidth, 0);
		jsB_propfun(J, "Image.getHeight", ffi_Image_getHeight, 0);
		jsB_propfun(J, "Image.getColorSpace", ffi_Image_getColorSpace, 0);
		jsB_propfun(J, "Image.getXResolution", ffi_Image_getXResolution, 0);
		jsB_propfun(J, "Image.getYResolution", ffi_Image_getYResolution, 0);
		jsB_propfun(J, "Image.getNumberOfComponents", ffi_Image_getNumberOfComponents, 0);
		jsB_propfun(J, "Image.getBitsPerComponent", ffi_Image_getBitsPerComponent, 0);
		jsB_propfun(J, "Image.getImageMask", ffi_Image_getImageMask, 0);
		jsB_propfun(J, "Image.getInterpolate", ffi_Image_getInterpolate, 0);
		jsB_propfun(J, "Image.getColorKey", ffi_Image_getColorKey, 0);
		jsB_propfun(J, "Image.getDecode", ffi_Image_getDecode, 0);
		jsB_propfun(J, "Image.getOrientation", ffi_Image_getOrientation, 0);
		jsB_propfun(J, "Image.getMask", ffi_Image_getMask, 0);
		jsB_propfun(J, "Image.toPixmap", ffi_Image_toPixmap, 2);
		jsB_propfun(J, "Image.setOrientation", ffi_Image_setOrientation, 1);
	}
	js_setregistry(J, "fz_image");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Font.getName", ffi_Font_getName, 0);
		jsB_propfun(J, "Font.encodeCharacter", ffi_Font_encodeCharacter, 1);
		jsB_propfun(J, "Font.advanceGlyph", ffi_Font_advanceGlyph, 2);
		jsB_propfun(J, "Font.isMono", ffi_Font_isMono, 0);
		jsB_propfun(J, "Font.isSerif", ffi_Font_isSerif, 0);
		jsB_propfun(J, "Font.isBold", ffi_Font_isBold, 0);
		jsB_propfun(J, "Font.isItalic", ffi_Font_isItalic, 0);
	}
	js_setregistry(J, "fz_font");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Text.walk", ffi_Text_walk, 1);
		jsB_propfun(J, "Text.showGlyph", ffi_Text_showGlyph, 5);
		jsB_propfun(J, "Text.showString", ffi_Text_showString, 4);
	}
	js_setregistry(J, "fz_text");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Path.walk", ffi_Path_walk, 1);
		jsB_propfun(J, "Path.moveTo", ffi_Path_moveTo, 2);
		jsB_propfun(J, "Path.lineTo", ffi_Path_lineTo, 2);
		jsB_propfun(J, "Path.curveTo", ffi_Path_curveTo, 6);
		jsB_propfun(J, "Path.curveToV", ffi_Path_curveToV, 4);
		jsB_propfun(J, "Path.curveToY", ffi_Path_curveToY, 4);
		jsB_propfun(J, "Path.closePath", ffi_Path_closePath, 0);
		jsB_propfun(J, "Path.rect", ffi_Path_rect, 4);
		jsB_propfun(J, "Path.getBounds", ffi_Path_getBounds, 2);
		jsB_propfun(J, "Path.transform", ffi_Path_transform, 1);
	}
	js_setregistry(J, "fz_path");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "DisplayList.run", ffi_DisplayList_run, 2);
		jsB_propfun(J, "DisplayList.getBounds", ffi_DisplayList_getBounds, 0);
		jsB_propfun(J, "DisplayList.toPixmap", ffi_DisplayList_toPixmap, 3);
		jsB_propfun(J, "DisplayList.toStructuredText", ffi_DisplayList_toStructuredText, 1);
		jsB_propfun(J, "DisplayList.search", ffi_DisplayList_search, 1);
	}
	js_setregistry(J, "fz_display_list");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "StructuredText.walk", ffi_StructuredText_walk, 1);
		jsB_propfun(J, "StructuredText.search", ffi_StructuredText_search, 1);
		jsB_propfun(J, "StructuredText.highlight", ffi_StructuredText_highlight, 2);
		jsB_propfun(J, "StructuredText.copy", ffi_StructuredText_copy, 2);
	}
	js_setregistry(J, "fz_stext_page");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Pixmap.getBounds", ffi_Pixmap_getBounds, 0);
		jsB_propfun(J, "Pixmap.clear", ffi_Pixmap_clear, 1);

		jsB_propfun(J, "Pixmap.getX", ffi_Pixmap_getX, 0);
		jsB_propfun(J, "Pixmap.getY", ffi_Pixmap_getY, 0);
		jsB_propfun(J, "Pixmap.getWidth", ffi_Pixmap_getWidth, 0);
		jsB_propfun(J, "Pixmap.getHeight", ffi_Pixmap_getHeight, 0);
		jsB_propfun(J, "Pixmap.getNumberOfComponents", ffi_Pixmap_getNumberOfComponents, 0);
		jsB_propfun(J, "Pixmap.getAlpha", ffi_Pixmap_getAlpha, 0);
		jsB_propfun(J, "Pixmap.getStride", ffi_Pixmap_getStride, 0);
		jsB_propfun(J, "Pixmap.getColorSpace", ffi_Pixmap_getColorSpace, 0);
		jsB_propfun(J, "Pixmap.getXResolution", ffi_Pixmap_getXResolution, 0);
		jsB_propfun(J, "Pixmap.getYResolution", ffi_Pixmap_getYResolution, 0);
		jsB_propfun(J, "Pixmap.getSample", ffi_Pixmap_getSample, 3);
		jsB_propfun(J, "Pixmap.setResolution", ffi_Pixmap_setResolution, 2);
		jsB_propfun(J, "Pixmap.invert", ffi_Pixmap_invert, 0);
		jsB_propfun(J, "Pixmap.invertLuminance", ffi_Pixmap_invertLuminance, 0);
		jsB_propfun(J, "Pixmap.gamma", ffi_Pixmap_gamma, 1);
		jsB_propfun(J, "Pixmap.tint", ffi_Pixmap_tint, 2);
		jsB_propfun(J, "Pixmap.warp", ffi_Pixmap_warp, 3);
		jsB_propfun(J, "Pixmap.convertToColorSpace", ffi_Pixmap_convertToColorSpace, 5);

		// Pixmap.getPixels() - Buffer
		// Pixmap.scale()

		jsB_propfun(J, "Pixmap.asPNG", ffi_Pixmap_asPNG, 0);

		jsB_propfun(J, "Pixmap.saveAsPNG", ffi_Pixmap_saveAsPNG, 1);
		jsB_propfun(J, "Pixmap.saveAsJPEG", ffi_Pixmap_saveAsJPEG, 2);
		jsB_propfun(J, "Pixmap.saveAsPAM", ffi_Pixmap_saveAsPAM, 1);
		jsB_propfun(J, "Pixmap.saveAsPNM", ffi_Pixmap_saveAsPNM, 1);
		jsB_propfun(J, "Pixmap.saveAsPBM", ffi_Pixmap_saveAsPBM, 1);
		jsB_propfun(J, "Pixmap.saveAsPKM", ffi_Pixmap_saveAsPKM, 1);
	}
	js_setregistry(J, "fz_pixmap");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "DocumentWriter.beginPage", ffi_DocumentWriter_beginPage, 1);
		jsB_propfun(J, "DocumentWriter.endPage", ffi_DocumentWriter_endPage, 0);
		jsB_propfun(J, "DocumentWriter.close", ffi_DocumentWriter_close, 0);
	}
	js_setregistry(J, "fz_document_writer");

#if FZ_ENABLE_PDF
	js_getregistry(J, "fz_document");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFDocument.getVersion", ffi_PDFDocument_getVersion, 0);
		jsB_propfun(J, "PDFDocument.getTrailer", ffi_PDFDocument_getTrailer, 0);
		jsB_propfun(J, "PDFDocument.countObjects", ffi_PDFDocument_countObjects, 0);
		jsB_propfun(J, "PDFDocument.createObject", ffi_PDFDocument_createObject, 0);
		jsB_propfun(J, "PDFDocument.deleteObject", ffi_PDFDocument_deleteObject, 1);
		jsB_propfun(J, "PDFDocument.addObject", ffi_PDFDocument_addObject, 1);
		jsB_propfun(J, "PDFDocument.addStream", ffi_PDFDocument_addStream, 2);
		jsB_propfun(J, "PDFDocument.addRawStream", ffi_PDFDocument_addRawStream, 2);
		jsB_propfun(J, "PDFDocument.addSimpleFont", ffi_PDFDocument_addSimpleFont, 2);
		jsB_propfun(J, "PDFDocument.addCJKFont", ffi_PDFDocument_addCJKFont, 4);
		jsB_propfun(J, "PDFDocument.addFont", ffi_PDFDocument_addFont, 1);
		jsB_propfun(J, "PDFDocument.addImage", ffi_PDFDocument_addImage, 1);
		jsB_propfun(J, "PDFDocument.loadImage", ffi_PDFDocument_loadImage, 1);

		jsB_propfun(J, "PDFDocument.addEmbeddedFile", ffi_PDFDocument_addEmbeddedFile, 6);
		jsB_propfun(J, "PDFDocument.getEmbeddedFileParams", ffi_PDFDocument_getEmbeddedFileParams, 1);
		jsB_propfun(J, "PDFDocument.getEmbeddedFileContents", ffi_PDFDocument_getEmbeddedFileContents, 1);
		jsB_propfun(J, "PDFDocument.verifyEmbeddedFileChecksum", ffi_PDFDocument_verifyEmbeddedFileChecksum, 1);

		jsB_propfun(J, "PDFDocument.addPage", ffi_PDFDocument_addPage, 4);
		jsB_propfun(J, "PDFDocument.insertPage", ffi_PDFDocument_insertPage, 2);
		jsB_propfun(J, "PDFDocument.deletePage", ffi_PDFDocument_deletePage, 1);
		jsB_propfun(J, "PDFDocument.countPages", ffi_PDFDocument_countPages, 0);
		jsB_propfun(J, "PDFDocument.findPage", ffi_PDFDocument_findPage, 1);
		jsB_propfun(J, "PDFDocument.findPageNumber", ffi_PDFDocument_findPageNumber, 1);
		jsB_propfun(J, "PDFDocument.lookupDest", ffi_PDFDocument_lookupDest, 1);
		jsB_propfun(J, "PDFDocument.save", ffi_PDFDocument_save, 2);

		jsB_propfun(J, "PDFDocument.newNull", ffi_PDFDocument_newNull, 0);
		jsB_propfun(J, "PDFDocument.newBoolean", ffi_PDFDocument_newBoolean, 1);
		jsB_propfun(J, "PDFDocument.newInteger", ffi_PDFDocument_newInteger, 1);
		jsB_propfun(J, "PDFDocument.newReal", ffi_PDFDocument_newReal, 1);
		jsB_propfun(J, "PDFDocument.newString", ffi_PDFDocument_newString, 1);
		jsB_propfun(J, "PDFDocument.newByteString", ffi_PDFDocument_newByteString, 1);
		jsB_propfun(J, "PDFDocument.newName", ffi_PDFDocument_newName, 1);
		jsB_propfun(J, "PDFDocument.newIndirect", ffi_PDFDocument_newIndirect, 2);
		jsB_propfun(J, "PDFDocument.newArray", ffi_PDFDocument_newArray, 1);
		jsB_propfun(J, "PDFDocument.newDictionary", ffi_PDFDocument_newDictionary, 1);

		jsB_propfun(J, "PDFDocument.newGraftMap", ffi_PDFDocument_newGraftMap, 0);
		jsB_propfun(J, "PDFDocument.graftObject", ffi_PDFDocument_graftObject, 1);
		jsB_propfun(J, "PDFDocument.graftPage", ffi_PDFDocument_graftPage, 3);

		jsB_propfun(J, "PDFDocument.enableJS", ffi_PDFDocument_enableJS, 0);
		jsB_propfun(J, "PDFDocument.disableJS", ffi_PDFDocument_disableJS, 0);
		jsB_propfun(J, "PDFDocument.isJSSupported", ffi_PDFDocument_isJSSupported, 0);
		jsB_propfun(J, "PDFDocument.setJSEventListener", ffi_PDFDocument_setJSEventListener, 1);

		jsB_propfun(J, "PDFDocument.countVersions", ffi_PDFDocument_countVersions, 0);
		jsB_propfun(J, "PDFDocument.countUnsavedVersions", ffi_PDFDocument_countUnsavedVersions, 0);
		jsB_propfun(J, "PDFDocument.validateChangeHistory", ffi_PDFDocument_validateChangeHistory, 0);
		jsB_propfun(J, "PDFDocument.wasPureXFA", ffi_PDFDocument_wasPureXFA, 0);

		jsB_propfun(J, "PDFDocument.hasUnsavedChanges", ffi_PDFDocument_hasUnsavedChanges, 0);
		jsB_propfun(J, "PDFDocument.wasRepaired", ffi_PDFDocument_wasRepaired, 0);
		jsB_propfun(J, "PDFDocument.canBeSavedIncrementally", ffi_PDFDocument_canBeSavedIncrementally, 0);

		jsB_propfun(J, "PDFDocument.enableJournal", ffi_PDFDocument_enableJournal, 0);
		jsB_propfun(J, "PDFDocument.getJournal", ffi_PDFDocument_getJournal, 0);
		jsB_propfun(J, "PDFDocument.beginOperation", ffi_PDFDocument_beginOperation, 1);
		jsB_propfun(J, "PDFDocument.beginImplicitOperation", ffi_PDFDocument_beginImplicitOperation, 0);
		jsB_propfun(J, "PDFDocument.endOperation", ffi_PDFDocument_endOperation, 0);
		jsB_propfun(J, "PDFDocument.abandonOperation", ffi_PDFDocument_abandonOperation, 0);
		jsB_propfun(J, "PDFDocument.canUndo", ffi_PDFDocument_canUndo, 0);
		jsB_propfun(J, "PDFDocument.canRedo", ffi_PDFDocument_canRedo, 0);
		jsB_propfun(J, "PDFDocument.undo", ffi_PDFDocument_undo, 0);
		jsB_propfun(J, "PDFDocument.redo", ffi_PDFDocument_redo, 0);

		jsB_propfun(J, "PDFDocument.setPageLabels", ffi_PDFDocument_setPageLabels, 4);
		jsB_propfun(J, "PDFDocument.deletePageLabels", ffi_PDFDocument_deletePageLabels, 1);
	}
	js_setregistry(J, "pdf_document");

	js_getregistry(J, "fz_page");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFPage.getObject", ffi_PDFPage_getObject, 0);
		jsB_propfun(J, "PDFPage.getWidgets", ffi_PDFPage_getWidgets, 0);
		jsB_propfun(J, "PDFPage.getAnnotations", ffi_PDFPage_getAnnotations, 0);
		jsB_propfun(J, "PDFPage.createAnnotation", ffi_PDFPage_createAnnotation, 1);
		jsB_propfun(J, "PDFPage.deleteAnnotation", ffi_PDFPage_deleteAnnotation, 1);
		jsB_propfun(J, "PDFPage.update", ffi_PDFPage_update, 0);
		jsB_propfun(J, "PDFPage.applyRedactions", ffi_PDFPage_applyRedactions, 2);
		jsB_propfun(J, "PDFPage.process", ffi_PDFPage_process, 1);
		jsB_propfun(J, "PDFPage.toPixmap", ffi_PDFPage_toPixmap, 6);
		jsB_propfun(J, "PDFPage.getTransform", ffi_PDFPage_getTransform, 0);
	}
	js_setregistry(J, "pdf_page");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFAnnotation.getBounds", ffi_PDFAnnotation_getBounds, 0);
		jsB_propfun(J, "PDFAnnotation.run", ffi_PDFAnnotation_run, 2);
		jsB_propfun(J, "PDFAnnotation.toPixmap", ffi_PDFAnnotation_toPixmap, 3);
		jsB_propfun(J, "PDFAnnotation.toDisplayList", ffi_PDFAnnotation_toDisplayList, 0);
		jsB_propfun(J, "PDFAnnotation.getObject", ffi_PDFAnnotation_getObject, 0);

		jsB_propfun(J, "PDFAnnotation.getType", ffi_PDFAnnotation_getType, 0);
		jsB_propfun(J, "PDFAnnotation.getFlags", ffi_PDFAnnotation_getFlags, 0);
		jsB_propfun(J, "PDFAnnotation.setFlags", ffi_PDFAnnotation_setFlags, 1);
		jsB_propfun(J, "PDFAnnotation.getContents", ffi_PDFAnnotation_getContents, 0);
		jsB_propfun(J, "PDFAnnotation.setContents", ffi_PDFAnnotation_setContents, 1);
		jsB_propfun(J, "PDFAnnotation.getRect", ffi_PDFAnnotation_getRect, 0);
		jsB_propfun(J, "PDFAnnotation.setRect", ffi_PDFAnnotation_setRect, 1);
		jsB_propfun(J, "PDFAnnotation.getBorder", ffi_PDFAnnotation_getBorder, 0);
		jsB_propfun(J, "PDFAnnotation.setBorder", ffi_PDFAnnotation_setBorder, 1);
		jsB_propfun(J, "PDFAnnotation.getColor", ffi_PDFAnnotation_getColor, 0);
		jsB_propfun(J, "PDFAnnotation.setColor", ffi_PDFAnnotation_setColor, 1);
		jsB_propfun(J, "PDFAnnotation.hasInteriorColor", ffi_PDFAnnotation_hasInteriorColor, 0);
		jsB_propfun(J, "PDFAnnotation.getInteriorColor", ffi_PDFAnnotation_getInteriorColor, 0);
		jsB_propfun(J, "PDFAnnotation.setInteriorColor", ffi_PDFAnnotation_setInteriorColor, 1);
		jsB_propfun(J, "PDFAnnotation.getOpacity", ffi_PDFAnnotation_getOpacity, 0);
		jsB_propfun(J, "PDFAnnotation.setOpacity", ffi_PDFAnnotation_setOpacity, 1);
		jsB_propfun(J, "PDFAnnotation.hasAuthor", ffi_PDFAnnotation_hasAuthor, 0);
		jsB_propfun(J, "PDFAnnotation.getAuthor", ffi_PDFAnnotation_getAuthor, 0);
		jsB_propfun(J, "PDFAnnotation.setAuthor", ffi_PDFAnnotation_setAuthor, 1);
		jsB_propfun(J, "PDFAnnotation.getCreationDate", ffi_PDFAnnotation_getCreationDate, 0);
		jsB_propfun(J, "PDFAnnotation.setCreationDate", ffi_PDFAnnotation_setCreationDate, 1);
		jsB_propfun(J, "PDFAnnotation.getModificationDate", ffi_PDFAnnotation_getModificationDate, 0);
		jsB_propfun(J, "PDFAnnotation.setModificationDate", ffi_PDFAnnotation_setModificationDate, 1);
		jsB_propfun(J, "PDFAnnotation.hasLineEndingStyles", ffi_PDFAnnotation_hasLineEndingStyles, 0);
		jsB_propfun(J, "PDFAnnotation.getLineEndingStyles", ffi_PDFAnnotation_getLineEndingStyles, 0);
		jsB_propfun(J, "PDFAnnotation.setLineEndingStyles", ffi_PDFAnnotation_setLineEndingStyles, 2);
		jsB_propfun(J, "PDFAnnotation.hasBorder", ffi_PDFAnnotation_hasBorder, 0);
		jsB_propfun(J, "PDFAnnotation.getBorderStyle", ffi_PDFAnnotation_getBorderStyle, 0);
		jsB_propfun(J, "PDFAnnotation.setBorderStyle", ffi_PDFAnnotation_setBorderStyle, 1);
		jsB_propfun(J, "PDFAnnotation.getBorderWidth", ffi_PDFAnnotation_getBorderWidth, 0);
		jsB_propfun(J, "PDFAnnotation.setBorderWidth", ffi_PDFAnnotation_setBorderWidth, 1);
		jsB_propfun(J, "PDFAnnotation.getBorderDashCount", ffi_PDFAnnotation_getBorderDashCount, 0);
		jsB_propfun(J, "PDFAnnotation.getBorderDashItem", ffi_PDFAnnotation_getBorderDashItem, 1);
		jsB_propfun(J, "PDFAnnotation.setBorderDashPattern", ffi_PDFAnnotation_setBorderDashPattern, 1);
		jsB_propfun(J, "PDFAnnotation.clearBorderDash", ffi_PDFAnnotation_clearBorderDash, 0);
		jsB_propfun(J, "PDFAnnotation.addBorderDashItem", ffi_PDFAnnotation_addBorderDashItem, 1);
		jsB_propfun(J, "PDFAnnotation.hasBorderEffect", ffi_PDFAnnotation_hasBorderEffect, 0);
		jsB_propfun(J, "PDFAnnotation.getBorderEffect", ffi_PDFAnnotation_getBorderEffect, 0);
		jsB_propfun(J, "PDFAnnotation.setBorderEffect", ffi_PDFAnnotation_setBorderEffect, 1);
		jsB_propfun(J, "PDFAnnotation.getBorderEffectIntensity", ffi_PDFAnnotation_getBorderEffectIntensity, 0);
		jsB_propfun(J, "PDFAnnotation.setBorderEffectIntensity", ffi_PDFAnnotation_setBorderEffectIntensity, 1);
		jsB_propfun(J, "PDFAnnotation.hasIcon", ffi_PDFAnnotation_hasIcon, 0);
		jsB_propfun(J, "PDFAnnotation.getIcon", ffi_PDFAnnotation_getIcon, 0);
		jsB_propfun(J, "PDFAnnotation.setIcon", ffi_PDFAnnotation_setIcon, 1);
		jsB_propfun(J, "PDFAnnotation.getQuadding", ffi_PDFAnnotation_getQuadding, 0);
		jsB_propfun(J, "PDFAnnotation.setQuadding", ffi_PDFAnnotation_setQuadding, 1);
		jsB_propfun(J, "PDFAnnotation.getLanguage", ffi_PDFAnnotation_getLanguage, 0);
		jsB_propfun(J, "PDFAnnotation.setLanguage", ffi_PDFAnnotation_setLanguage, 1);
		jsB_propfun(J, "PDFAnnotation.hasLine", ffi_PDFAnnotation_hasLine, 0);
		jsB_propfun(J, "PDFAnnotation.getLine", ffi_PDFAnnotation_getLine, 0);
		jsB_propfun(J, "PDFAnnotation.setLine", ffi_PDFAnnotation_setLine, 2);
		jsB_propfun(J, "PDFAnnotation.getDefaultAppearance", ffi_PDFAnnotation_getDefaultAppearance, 0);
		jsB_propfun(J, "PDFAnnotation.setDefaultAppearance", ffi_PDFAnnotation_setDefaultAppearance, 3);
		jsB_propfun(J, "PDFAnnotation.setAppearance", ffi_PDFAnnotation_setAppearance, 6);
		jsB_propfun(J, "PDFAnnotation.hasFilespec", ffi_PDFAnnotation_hasFilespec, 0);
		jsB_propfun(J, "PDFAnnotation.getFilespec", ffi_PDFAnnotation_getFilespec, 0);
		jsB_propfun(J, "PDFAnnotation.setFilespec", ffi_PDFAnnotation_setFilespec, 1);

		jsB_propfun(J, "PDFAnnotation.hasInkList", ffi_PDFAnnotation_hasInkList, 0);
		jsB_propfun(J, "PDFAnnotation.getInkList", ffi_PDFAnnotation_getInkList, 0);
		jsB_propfun(J, "PDFAnnotation.setInkList", ffi_PDFAnnotation_setInkList, 1);
		jsB_propfun(J, "PDFAnnotation.clearInkList", ffi_PDFAnnotation_clearInkList, 0);
		jsB_propfun(J, "PDFAnnotation.addInkList", ffi_PDFAnnotation_addInkList, 1);
		jsB_propfun(J, "PDFAnnotation.addInkListStroke", ffi_PDFAnnotation_addInkListStroke, 0);
		jsB_propfun(J, "PDFAnnotation.addInkListStrokeVertex", ffi_PDFAnnotation_addInkListStrokeVertex, 1);

		jsB_propfun(J, "PDFAnnotation.hasQuadPoints", ffi_PDFAnnotation_hasQuadPoints, 0);
		jsB_propfun(J, "PDFAnnotation.getQuadPoints", ffi_PDFAnnotation_getQuadPoints, 0);
		jsB_propfun(J, "PDFAnnotation.setQuadPoints", ffi_PDFAnnotation_setQuadPoints, 1);
		jsB_propfun(J, "PDFAnnotation.clearQuadPoints", ffi_PDFAnnotation_clearQuadPoints, 0);
		jsB_propfun(J, "PDFAnnotation.addQuadPoint", ffi_PDFAnnotation_addQuadPoint, 1);

		jsB_propfun(J, "PDFAnnotation.hasVertices", ffi_PDFAnnotation_hasVertices, 0);
		jsB_propfun(J, "PDFAnnotation.getVertices", ffi_PDFAnnotation_getVertices, 0);
		jsB_propfun(J, "PDFAnnotation.setVertices", ffi_PDFAnnotation_setVertices, 1);
		jsB_propfun(J, "PDFAnnotation.clearVertices", ffi_PDFAnnotation_clearVertices, 0);
		jsB_propfun(J, "PDFAnnotation.addVertex", ffi_PDFAnnotation_addVertex, 2);

		jsB_propfun(J, "PDFAnnotation.requestSynthesis", ffi_PDFAnnotation_requestSynthesis, 0);
		jsB_propfun(J, "PDFAnnotation.requestResynthesis", ffi_PDFAnnotation_requestResynthesis, 0);
		jsB_propfun(J, "PDFAnnotation.update", ffi_PDFAnnotation_update, 0);

		jsB_propfun(J, "PDFAnnotation.getHot", ffi_PDFAnnotation_getHot, 0);
		jsB_propfun(J, "PDFAnnotation.setHot", ffi_PDFAnnotation_setHot, 1);

		jsB_propfun(J, "PDFAnnotation.hasOpen", ffi_PDFAnnotation_hasOpen, 0);
		jsB_propfun(J, "PDFAnnotation.getIsOpen", ffi_PDFAnnotation_getIsOpen, 0);
		jsB_propfun(J, "PDFAnnotation.setIsOpen", ffi_PDFAnnotation_setIsOpen, 1);

		jsB_propfun(J, "PDFAnnotation.applyRedaction", ffi_PDFAnnotation_applyRedaction, 2);
		jsB_propfun(J, "PDFAnnotation.process", ffi_PDFAnnotation_process, 1);

		jsB_propfun(J, "PDFAnnotation.getHiddenForEditing", ffi_PDFAnnotation_getHiddenForEditing, 0);
		jsB_propfun(J, "PDFAnnotation.setHiddenForEditing", ffi_PDFAnnotation_setHiddenForEditing, 1);
	}
	js_setregistry(J, "pdf_annot");

	js_getregistry(J, "pdf_annot");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFWidget.getFieldType", ffi_PDFWidget_getFieldType, 0);
		jsB_propfun(J, "PDFWidget.getFieldFlags", ffi_PDFWidget_getFieldFlags, 0);
		jsB_propfun(J, "PDFWidget.getValue", ffi_PDFWidget_getValue, 0);
		jsB_propfun(J, "PDFWidget.setTextValue", ffi_PDFWidget_setTextValue, 1);
		jsB_propfun(J, "PDFWidget.setChoiceValue", ffi_PDFWidget_setChoiceValue, 1);
		jsB_propfun(J, "PDFWidget.toggle", ffi_PDFWidget_toggle, 0);
		jsB_propfun(J, "PDFWidget.getMaxLen", ffi_PDFWidget_getMaxLen, 0);
		jsB_propfun(J, "PDFWidget.getOptions", ffi_PDFWidget_getOptions, 1);
		jsB_propfun(J, "PDFWidget.layoutTextWidget", ffi_PDFWidget_layoutTextWidget, 0);

		jsB_propfun(J, "PDFWidget.eventEnter", ffi_PDFWidget_eventEnter, 0);
		jsB_propfun(J, "PDFWidget.eventExit", ffi_PDFWidget_eventExit, 0);
		jsB_propfun(J, "PDFWidget.eventDown", ffi_PDFWidget_eventDown, 0);
		jsB_propfun(J, "PDFWidget.eventUp", ffi_PDFWidget_eventUp, 0);
		jsB_propfun(J, "PDFWidget.eventFocus", ffi_PDFWidget_eventFocus, 0);
		jsB_propfun(J, "PDFWidget.eventBlur", ffi_PDFWidget_eventBlur, 0);

		jsB_propfun(J, "PDFWidget.isSigned", ffi_PDFWidget_isSigned, 0);
		jsB_propfun(J, "PDFWidget.isReadOnly", ffi_PDFWidget_isReadOnly, 0);
		jsB_propfun(J, "PDFWidget.validateSignature", ffi_PDFWidget_validateSignature, 0);
		jsB_propfun(J, "PDFWidget.checkCertificate", ffi_PDFWidget_checkCertificate, 0);
		jsB_propfun(J, "PDFWidget.checkDigest", ffi_PDFWidget_checkDigest, 0);
		jsB_propfun(J, "PDFWidget.getSignatory", ffi_PDFWidget_getSignatory, 0);
		jsB_propfun(J, "PDFWidget.clearSignature", ffi_PDFWidget_clearSignature, 0);
		jsB_propfun(J, "PDFWidget.sign", ffi_PDFWidget_sign, 5);
		jsB_propfun(J, "PDFWidget.previewSignature", ffi_PDFWidget_previewSignature, 5);
		jsB_propfun(J, "PDFWidget.getEditingState", ffi_PDFWidget_getEditingState, 0);
		jsB_propfun(J, "PDFWidget.setEditingState", ffi_PDFWidget_setEditingState, 1);
		jsB_propfun(J, "PDFWidget.getLabel", ffi_PDFWidget_getLabel, 0);
		jsB_propfun(J, "PDFWidget.getName", ffi_PDFWidget_getName, 0);
	}
	js_setregistry(J, "pdf_widget");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	js_setregistry(J, "pdf_pkcs7_signer");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFObject.getNumber", ffi_PDFObject_getNumber, 1);
		jsB_propfun(J, "PDFObject.getName", ffi_PDFObject_getName, 1);
		jsB_propfun(J, "PDFObject.getString", ffi_PDFObject_getString, 1);
		jsB_propfun(J, "PDFObject.getInheritable", ffi_PDFObject_getInheritable, 1);
		jsB_propfun(J, "PDFObject.get", ffi_PDFObject_get, 1);
		jsB_propfun(J, "PDFObject.put", ffi_PDFObject_put, 2);
		jsB_propfun(J, "PDFObject.push", ffi_PDFObject_push, 1);
		jsB_propfun(J, "PDFObject.delete", ffi_PDFObject_delete, 1);
		jsB_propfun(J, "PDFObject.resolve", ffi_PDFObject_resolve, 0);
		jsB_propfun(J, "PDFObject.toString", ffi_PDFObject_toString, 2);
		jsB_propfun(J, "PDFObject.valueOf", ffi_PDFObject_valueOf, 0);
		jsB_propfun(J, "PDFObject.isArray", ffi_PDFObject_isArray, 0);
		jsB_propfun(J, "PDFObject.isDictionary", ffi_PDFObject_isDictionary, 0);
		jsB_propfun(J, "PDFObject.isIndirect", ffi_PDFObject_isIndirect, 0);
		jsB_propfun(J, "PDFObject.asIndirect", ffi_PDFObject_asIndirect, 0);
		jsB_propfun(J, "PDFObject.isNull", ffi_PDFObject_isNull, 0);
		jsB_propfun(J, "PDFObject.isBoolean", ffi_PDFObject_isBoolean, 0);
		jsB_propfun(J, "PDFObject.asBoolean", ffi_PDFObject_asBoolean, 0);
		jsB_propfun(J, "PDFObject.isNumber", ffi_PDFObject_isNumber, 0);
		jsB_propfun(J, "PDFObject.asNumber", ffi_PDFObject_asNumber, 0);
		jsB_propfun(J, "PDFObject.isName", ffi_PDFObject_isName, 0);
		jsB_propfun(J, "PDFObject.asName", ffi_PDFObject_asName, 0);
		jsB_propfun(J, "PDFObject.isString", ffi_PDFObject_isString, 0);
		jsB_propfun(J, "PDFObject.asString", ffi_PDFObject_asString, 0);
		jsB_propfun(J, "PDFObject.asByteString", ffi_PDFObject_asByteString, 0);
		jsB_propfun(J, "PDFObject.isStream", ffi_PDFObject_isStream, 0);
		jsB_propfun(J, "PDFObject.readStream", ffi_PDFObject_readStream, 0);
		jsB_propfun(J, "PDFObject.readRawStream", ffi_PDFObject_readRawStream, 0);
		jsB_propfun(J, "PDFObject.writeObject", ffi_PDFObject_writeObject, 1);
		jsB_propfun(J, "PDFObject.writeStream", ffi_PDFObject_writeStream, 1);
		jsB_propfun(J, "PDFObject.writeRawStream", ffi_PDFObject_writeRawStream, 1);
		jsB_propfun(J, "PDFObject.forEach", ffi_PDFObject_forEach, 1);
		jsB_propfun(J, "PDFObject.compare", ffi_PDFObject_compare, 1);
	}
	js_setregistry(J, "pdf_obj");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFGraftMap.graftObject", ffi_PDFGraftMap_graftObject, 1);
		jsB_propfun(J, "PDFGraftMap.graftPage", ffi_PDFGraftMap_graftPage, 3);
	}
	js_setregistry(J, "pdf_graft_map");
#endif

	js_pushglobal(J);
	{
#if FZ_ENABLE_PDF
		jsB_propcon(J, "pdf_document", "PDFDocument", ffi_new_PDFDocument, 1);
		js_getglobal(J, "PDFDocument");
		{
			jsB_propfun(J, "formatURIWithPathAndDest", ffi_formatURIFromPathAndDest, 2);
			jsB_propfun(J, "appendDestToURI", ffi_appendDestToURI, 2);
		}
		js_pop(J, 1);
#endif

		jsB_propcon(J, "fz_archive", "Archive", ffi_new_Archive, 1);
		jsB_propcon(J, "fz_multi_archive", "MultiArchive", ffi_new_MultiArchive, 1);
		jsB_propcon(J, "fz_tree_archive", "TreeArchive", ffi_new_TreeArchive, 1);
		jsB_propcon(J, "fz_buffer", "Buffer", ffi_new_Buffer, 1);
		jsB_propcon(J, "fz_pixmap", "Pixmap", ffi_new_Pixmap, 3);
		jsB_propcon(J, "fz_image", "Image", ffi_new_Image, 2);
		jsB_propcon(J, "fz_font", "Font", ffi_new_Font, 2);
		jsB_propcon(J, "fz_text", "Text", ffi_new_Text, 0);
		jsB_propcon(J, "fz_path", "Path", ffi_new_Path, 0);
		jsB_propcon(J, "fz_display_list", "DisplayList", ffi_new_DisplayList, 1);
		jsB_propcon(J, "fz_device", "DrawDevice", ffi_new_DrawDevice, 2);
		jsB_propcon(J, "fz_device", "DisplayListDevice", ffi_new_DisplayListDevice, 1);
		jsB_propcon(J, "fz_document_writer", "DocumentWriter", ffi_new_DocumentWriter, 3);
		jsB_propcon(J, "fz_story", "Story", ffi_new_Story, 4);
#if FZ_ENABLE_PDF
		jsB_propcon(J, "pdf_pkcs7_signer", "PDFPKCS7Signer", ffi_new_PDFPKCS7Signer, 2);
#endif

		jsB_propfun(J, "readFile", ffi_readFile, 1);
		jsB_propfun(J, "enableICC", ffi_enableICC, 0);
		jsB_propfun(J, "disableICC", ffi_disableICC, 0);

		jsB_propfun(J, "setUserCSS", ffi_setUserCSS, 2);
	}

	js_pushglobal(J);
	js_setglobal(J, "mupdf");

	js_dostring(J, postfix_js);

	js_endtry(J);

	if (argc > 1) {
		if (js_try(J))
		{
			fprintf(stderr, "cannot initialize scriptArgs/scriptPath\n");
			js_freestate(J);
			fz_drop_context(ctx);
			exit(1);
		}
		js_pushstring(J, argv[1]);
		js_setglobal(J, "scriptPath");
		js_newarray(J);
		for (i = 2; i < argc; ++i) {
			js_pushstring(J, argv[i]);
			js_setindex(J, -2, i - 2);
		}
		js_setglobal(J, "scriptArgs");
		js_endtry(J);
		if (js_dofile(J, argv[1]))
		{
			js_freestate(J);
			fz_drop_context(ctx);
			return 1;
		}
	} else {
		char line[256];
		fputs(PS1, stdout);
		while (fgets(line, sizeof line, stdin)) {
			eval_print(J, line);
			fputs(PS1, stdout);
		}
		putchar('\n');
	}

	js_freestate(J);
	fz_drop_context(ctx);
	return 0;
}

#endif
