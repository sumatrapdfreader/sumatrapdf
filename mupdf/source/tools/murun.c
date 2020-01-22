#include "mupdf/fitz.h"

#if FZ_ENABLE_PDF
#include "mupdf/pdf.h"
#endif

#if FZ_ENABLE_JS

#include "mujs.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
	js_pushundefined(J);
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
	js_pushundefined(J);
}

static void jsB_write(js_State *J)
{
	unsigned int i, top = js_gettop(J);
	for (i = 1; i < top; ++i) {
		const char *s = js_tostring(J, i);
		if (i > 1) putchar(' ');
		fputs(s, stdout);
	}
	js_pushundefined(J);
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

static void jsB_quit(js_State *J)
{
	exit(js_tonumber(J, 1));
}

static const char *require_js =
	"function require(name) {\n"
	"var cache = require.cache;\n"
	"if (name in cache) return cache[name];\n"
	"var exports = {};\n"
	"cache[name] = exports;\n"
	"Function('exports', read(name+'.js'))(exports);\n"
	"return exports;\n"
	"}\n"
	"require.cache = Object.create(null);\n"
;

static const char *stacktrace_js =
	"Error.prototype.toString = function() {\n"
	"if (this.stackTrace) return this.name + ': ' + this.message + this.stackTrace;\n"
	"return this.name + ': ' + this.message;\n"
	"};\n"
;

/* destructors */

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

#if FZ_ENABLE_PDF

static void ffi_gc_pdf_widget(js_State *J, void *widget)
{
	fz_context *ctx = js_getcontext(J);
	pdf_drop_widget(ctx, widget);
}

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
		js_newuserdata(J, "pdf_document", document, ffi_gc_fz_document);
	} else {
		js_getregistry(J, "fz_document");
		js_newuserdata(J, "fz_document", document, ffi_gc_fz_document);
	}
}

static fz_page *ffi_topage(js_State *J, int idx)
{
	if (js_isuserdata(J, idx, "pdf_page"))
		return js_touserdata(J, idx, "pdf_page");
	return js_touserdata(J, idx, "fz_page");
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
	js_newuserdata(J, "fz_document", doc, ffi_gc_fz_document);
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
	if (colorspace == fz_device_rgb(ctx))
		js_getregistry(J, "DeviceRGB");
	else if (colorspace == fz_device_bgr(ctx))
		js_getregistry(J, "DeviceBGR");
	else if (colorspace == fz_device_gray(ctx))
		js_getregistry(J, "DeviceGray");
	else if (colorspace == fz_device_cmyk(ctx))
		js_getregistry(J, "DeviceCMYK");
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

static fz_color_params ffi_tocolorparams(js_State *J, int idx)
{
	/* TODO */
	return fz_default_color_params;
}

static void ffi_pushcolorparams(js_State *J, fz_color_params color_params)
{
	/* TODO */
	js_pushnull(J);
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

#if FZ_ENABLE_PDF

static fz_buffer *ffi_tobuffer(js_State *J, int idx)
{
	fz_context *ctx = js_getcontext(J);
	fz_buffer *buf = NULL;

	if (js_isuserdata(J, idx, "fz_buffer"))
		buf = fz_keep_buffer(ctx, js_touserdata(J, idx, "fz_buffer"));
	else {
		const char *str = js_tostring(J, idx);
		fz_try(ctx)
			buf = fz_new_buffer_from_copied_data(ctx, (const unsigned char *)str, strlen(str));
		fz_catch(ctx)
			rethrow(J);
	}

	return buf;
}

#endif /* FZ_ENABLE_PDF */

/* device calling into js from c */

typedef struct js_device_s
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

	dev->super.begin_layer = js_dev_begin_layer;
	dev->super.end_layer = js_dev_end_layer;

	dev->J = J;
	return (fz_device*)dev;
}

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

/* mupdf module */

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

static void ffi_new_Document(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	const char *filename = js_tostring(J, 1);
	fz_document *doc = NULL;

	fz_try(ctx)
		doc = fz_open_document(ctx, filename);
	fz_catch(ctx)
		rethrow(J);

	ffi_pushdocument(J, doc);
}

static void ffi_Document_isPDF(js_State *J)
{
	js_pushboolean(J, js_isuserdata(J, 0, "pdf_document"));
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
	else
		js_pushundefined(J);
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

		if (outline->title)
			js_pushstring(J, outline->title);
		else
			js_pushundefined(J);
		js_setproperty(J, -2, "title");

		if (outline->uri)
			js_pushstring(J, outline->uri);
		else
			js_pushundefined(J);
		js_setproperty(J, -2, "uri");

		if (outline->page >= 0)
			js_pushnumber(J, outline->page);
		else
			js_pushundefined(J);
		js_setproperty(J, -2, "page");

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

	to_outline(J, outline);

	fz_drop_outline(ctx, outline);
}

static void ffi_Page_isPDF(js_State *J)
{
	js_pushboolean(J, js_isuserdata(J, 0, "pdf_page"));
}

static void ffi_Page_bound(js_State *J)
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

static void ffi_Page_run(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_device *device = NULL;
	fz_matrix ctm = ffi_tomatrix(J, 2);
	int no_annots = js_isdefined(J, 3) ? js_toboolean(J, 3) : 0;

	if (js_isuserdata(J, 1, "fz_device")) {
		device = js_touserdata(J, 1, "fz_device");
		fz_try(ctx)
			if (no_annots)
				fz_run_page_contents(ctx, page, device, ctm, NULL);
			else
				fz_run_page(ctx, page, device, ctm, NULL);
		fz_catch(ctx)
			rethrow(J);
	} else {
		device = new_js_device(ctx, J);
		js_copy(J, 1); /* put the js device on the top so the callbacks know where to get it */
		fz_try(ctx) {
			if (no_annots)
				fz_run_page_contents(ctx, page, device, ctm, NULL);
			else
				fz_run_page(ctx, page, device, ctm, NULL);
			fz_close_device(ctx, device);
		}
		fz_always(ctx)
			fz_drop_device(ctx, device);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_Page_toDisplayList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	int no_annots = js_isdefined(J, 1) ? js_toboolean(J, 1) : 0;
	fz_display_list *list = NULL;

	fz_try(ctx)
		if (no_annots)
			list = fz_new_display_list_from_page_contents(ctx, page);
		else
			list = fz_new_display_list_from_page(ctx, page);
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
	int no_annots = js_isdefined(J, 4) ? js_toboolean(J, 4) : 0;
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		if (no_annots)
			pixmap = fz_new_pixmap_from_page_contents(ctx, page, ctm, colorspace, alpha);
		else
			pixmap = fz_new_pixmap_from_page(ctx, page, ctm, colorspace, alpha);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
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

static void ffi_Page_search(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	const char *needle = js_tostring(J, 1);
	fz_quad hits[256];
	int i, n = 0;

	fz_try(ctx)
		n = fz_search_page(ctx, page, needle, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		ffi_pushquad(J, hits[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_Page_getLinks(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_page *page = ffi_topage(J, 0);
	fz_link *link, *links = NULL;
	int i = 0;

	js_newarray(J);

	fz_try(ctx)
		links = fz_load_links(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (link = links; link; link = link->next) {
		js_newobject(J);

		ffi_pushrect(J, link->rect);
		js_setproperty(J, -2, "bounds");

		js_pushstring(J, link->uri);
		js_setproperty(J, -2, "uri");

		js_setindex(J, -2, i++);
	}

	fz_drop_link(ctx, links);
}

static void ffi_ColorSpace_getNumberOfComponents(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushnumber(J, fz_colorspace_n(ctx, colorspace));
}

static void ffi_ColorSpace_toString(js_State *J)
{
	fz_colorspace *colorspace = js_touserdata(J, 0, "fz_colorspace");
	fz_context *ctx = js_getcontext(J);
	js_pushstring(J, fz_colorspace_name(ctx, colorspace));
}

static void ffi_new_Pixmap(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_colorspace *colorspace = js_touserdata(J, 1, "fz_colorspace");
	fz_irect bounds = ffi_toirect(J, 2);
	int alpha = js_toboolean(J, 3);
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		pixmap = fz_new_pixmap_with_bbox(ctx, colorspace, bounds, 0, alpha);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
}

static void ffi_Pixmap_saveAsPNG(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	const char *filename = js_tostring(J, 1);

	fz_try(ctx)
		fz_save_pixmap_as_png(ctx, pixmap, filename);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_Pixmap_bound(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
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
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
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
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->x);
}

static void ffi_Pixmap_getY(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->y);
}

static void ffi_Pixmap_getWidth(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->w);
}

static void ffi_Pixmap_getHeight(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->h);
}

static void ffi_Pixmap_getNumberOfComponents(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->n);
}

static void ffi_Pixmap_getAlpha(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->alpha);
}

static void ffi_Pixmap_getStride(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->stride);
}

static void ffi_Pixmap_getSample(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
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
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->xres);
}

static void ffi_Pixmap_getYResolution(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	js_pushnumber(J, pixmap->yres);
}

static void ffi_Pixmap_getColorSpace(js_State *J)
{
	fz_pixmap *pixmap = js_touserdata(J, 0, "fz_pixmap");
	ffi_pushcolorspace(J, pixmap->colorspace);
}

static void ffi_new_Image(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_image *image = NULL;

	if (js_isuserdata(J, 1, "fz_pixmap")) {
		fz_pixmap *pixmap = js_touserdata(J, 1, "fz_pixmap");
		fz_try(ctx)
			image = fz_new_image_from_pixmap(ctx, pixmap, NULL);
		fz_catch(ctx)
			rethrow(J);
	} else {
		const char *name = js_tostring(J, 1);
		fz_try(ctx)
			image = fz_new_image_from_file(ctx, name);
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

	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
}

static void ffi_Shade_bound(js_State *J)
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
	fz_text_span *span;
	fz_matrix trm;
	int i;

	js_getproperty(J, 1, "showGlyph");
	for (span = text->head; span; span = span->next) {
		ffi_pushfont(J, span->font);
		trm = span->trm;
		for (i = 0; i < span->len; ++i) {
			trm.e = span->items[i].x;
			trm.f = span->items[i].y;
			js_copy(J, -2); /* showGlyph function */
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
		js_pop(J, 1); /* pop font object */
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

static void ffi_Path_bound(js_State *J)
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
		fz_try(ctx) {
			fz_run_display_list(ctx, list, device, ctm, fz_infinite_rect, NULL);
			fz_close_device(ctx, device);
		}
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

	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
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
	fz_quad hits[256];
	int i, n = 0;

	fz_try(ctx)
		n = fz_search_display_list(ctx, list, needle, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		ffi_pushquad(J, hits[i]);
		js_setindex(J, -2, i);
	}
}

static void ffi_StructuredText_search(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	fz_stext_page *text = js_touserdata(J, 0, "fz_stext_page");
	const char *needle = js_tostring(J, 1);
	fz_quad hits[256];
	int i, n = 0;

	fz_try(ctx)
		n = fz_search_stext_page(ctx, text, needle, hits, nelem(hits));
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);
	for (i = 0; i < n; ++i) {
		ffi_pushquad(J, hits[i]);
		js_setindex(J, -2, i);
	}
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
	fz_display_list *list = js_touserdata(J, 0, "fz_display_list");
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
	fz_pixmap *pixmap = js_touserdata(J, 2, "fz_pixmap");
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

static void ffi_pushobj(js_State *J, pdf_obj *obj);

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
	fz_buffer *buf = ffi_tobuffer(J, 1); /* FIXME: leak if ffi_toobj throws */
	pdf_obj *obj = js_iscoercible(J, 2) ? ffi_toobj(J, pdf, 2) : NULL;
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
	pdf_obj *resources = ffi_toobj(J, pdf, 3); /* FIXME: leak if ffi_tobuffer throws */
	fz_buffer *contents = ffi_tobuffer(J, 4);
	pdf_obj *ind = NULL;

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

static void ffi_PDFObject_get(js_State *J)
{
	pdf_obj *obj = js_touserdata(J, 0, "pdf_obj");
	const char *key = js_tostring(J, 1);
	if (!ffi_pdf_obj_has(J, obj, key))
		js_pushundefined(J);
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
			js_pushnumber(J, i);
			ffi_pushobj(J, pdf_keep_obj(ctx, val));
			js_call(J, 2);
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
			js_pushstring(J, key);
			ffi_pushobj(J, pdf_keep_obj(ctx, val));
			js_call(J, 2);
			js_pop(J, 1);
		}
		return;
	}
}

static void ffi_PDFPage_getWidgets(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_page *page = js_touserdata(J, 0, "pdf_page");
	pdf_widget *widget = NULL;
	int i = 0;

	fz_try(ctx)
		widget = pdf_first_widget(ctx, page);
	fz_catch(ctx)
		rethrow(J);

	js_newarray(J);

	while (widget) {
		js_getregistry(J, "pdf_widget");
		js_newuserdata(J, "pdf_widget", pdf_keep_widget(ctx, widget), ffi_gc_pdf_widget);
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
	int subtype;

	fz_try(ctx)
	{
		subtype = pdf_annot_type_from_string(ctx, name);
		annot = pdf_create_annot(ctx, page, subtype);
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
	pdf_annot *annot = js_touserdata(J, 1, "pdf_annot");
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

static void ffi_PDFAnnotation_bound(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
		fz_try(ctx) {
			pdf_run_annot(ctx, annot, device, ctm, NULL);
			fz_close_device(ctx, device);
		}
		fz_always(ctx)
			fz_drop_device(ctx, device);
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_toDisplayList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_matrix ctm = ffi_tomatrix(J, 1);
	fz_colorspace *colorspace = js_touserdata(J, 2, "fz_colorspace");
	int alpha = js_toboolean(J, 3);
	fz_pixmap *pixmap = NULL;

	fz_try(ctx)
		pixmap = pdf_new_pixmap_from_annot(ctx, annot, ctm, colorspace, NULL, alpha);
	fz_catch(ctx)
		rethrow(J);

	js_getregistry(J, "fz_pixmap");
	js_newuserdata(J, "fz_pixmap", pixmap, ffi_gc_fz_pixmap);
}

static void ffi_PDFAnnotation_getType(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int type;
	const char *subtype = NULL;
	fz_try(ctx)
	{
		type = pdf_annot_type(ctx, annot);
		subtype = pdf_string_from_annot_type(ctx, type);
	}
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, subtype);
}

static void ffi_PDFAnnotation_getFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int flags = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getContents(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	const char *contents = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, contents);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getRect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_rect rect = ffi_torect(J, 1);
	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getBorder(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	float border = js_tonumber(J, 1);
	fz_try(ctx)
		pdf_set_annot_border(ctx, annot, border);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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

static void ffi_PDFAnnotation_getInteriorColor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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

static void ffi_PDFAnnotation_getQuadPoints(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");

	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addQuadPoint(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_quad q = ffi_toquad(J, 1);

	fz_try(ctx)
		pdf_add_annot_quad_point(ctx, annot, q);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getVertices(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");

	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addVertex(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_point p = ffi_topoint(J, 1);

	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, p);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
			js_pushnumber(J, pt.x);
			js_setindex(J, -2, k * 2 + 0);
			js_pushnumber(J, pt.y);
			js_setindex(J, -2, k * 2 + 1);
		}
		js_setindex(J, -2, i);
	}
}

static void ffi_PDFAnnotation_setInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_point *points = NULL;
	int *counts = NULL;
	int n, nv, k, i, v;

	fz_var(counts);
	fz_var(points);

	n = js_getlength(J, 1);
	nv = 0;
	for (i = 0; i < n; ++i) {
		js_getindex(J, 1, i);
		nv += js_getlength(J, -1) / 2;
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
		counts[i] = js_getlength(J, -1) / 2;
		for (k = 0; k < counts[i]; ++k) {
			js_getindex(J, -1, k*2);
			points[v].x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_getindex(J, -1, k*2+1);
			points[v].y = js_tonumber(J, -1);
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_try(ctx)
		pdf_clear_annot_ink_list(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addInkList(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int i, n;
	float x, y;

	n = js_getlength(J, 1);

	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		rethrow(J);

	for (i = 0; i < n; i += 2) {
		js_getindex(J, 1, i);
		x = js_tonumber(J, -1);
		js_pop(J, 1);

		js_getindex(J, 1, i+1);
		y = js_tonumber(J, -1);
		js_pop(J, 1);

		fz_try(ctx)
			pdf_add_annot_ink_list_stroke_vertex(ctx, annot, fz_make_point(x, y));
		fz_catch(ctx)
			rethrow(J);
	}
}

static void ffi_PDFAnnotation_addInkListStroke(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_try(ctx)
		pdf_add_annot_ink_list_stroke(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_addInkListStrokeVertex(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	float x = js_tonumber(J, 1);
	float y = js_tonumber(J, 2);
	fz_try(ctx)
		pdf_add_annot_ink_list_stroke_vertex(ctx, annot, fz_make_point(x, y));
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getAuthor(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	const char *author = js_tostring(J, 1);

	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_getModificationDate(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
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
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	double time = js_tonumber(J, 1);

	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, time / 1000);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_updateAppearance(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	fz_try(ctx)
		pdf_update_appearance(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFAnnotation_update(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_annot *annot = js_touserdata(J, 0, "pdf_annot");
	int changed = 0;
	fz_try(ctx)
		changed = pdf_update_annot(ctx, annot);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, changed);
}


static void ffi_PDFWidget_getFieldType(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	int type;
	fz_try(ctx)
		type = pdf_field_type(ctx, widget->obj);
	fz_catch(ctx)
		rethrow(J);
	switch (type)
	{
	default:
	case PDF_WIDGET_TYPE_BUTTON: js_pushstring(J, "button"); break;
	case PDF_WIDGET_TYPE_CHECKBOX: js_pushstring(J, "checkbox"); break;
	case PDF_WIDGET_TYPE_COMBOBOX: js_pushstring(J, "combobox"); break;
	case PDF_WIDGET_TYPE_LISTBOX: js_pushstring(J, "listbox"); break;
	case PDF_WIDGET_TYPE_RADIOBUTTON: js_pushstring(J, "radiobutton"); break;
	case PDF_WIDGET_TYPE_SIGNATURE: js_pushstring(J, "signature"); break;
	case PDF_WIDGET_TYPE_TEXT: js_pushstring(J, "text"); break;
	}
}

static void ffi_PDFWidget_getFieldFlags(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	int flags;
	fz_try(ctx)
		flags = pdf_field_flags(ctx, widget->obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushnumber(J, flags);
}

static void ffi_PDFWidget_getRect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_rect rect;
	fz_try(ctx)
		rect = pdf_annot_rect(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	ffi_pushrect(J, rect);
}

static void ffi_PDFWidget_setRect(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_rect rect = ffi_torect(J, 1);
	fz_try(ctx)
		pdf_set_annot_rect(ctx, widget, rect);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_getValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value;
	fz_try(ctx)
		value = pdf_field_value(ctx, widget->obj);
	fz_catch(ctx)
		rethrow(J);
	js_pushstring(J, value);
}

static void ffi_PDFWidget_setTextValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_text_field_value(ctx, widget, value);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_setChoiceValue(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	const char *value = js_tostring(J, 1);
	fz_try(ctx)
		pdf_set_choice_field_value(ctx, widget, value);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_toggle(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
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
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
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
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	int export = js_toboolean(J, 1);
	const char *opt;
	int i, n;
	fz_try(ctx)
		n = pdf_choice_field_option_count(ctx, widget->obj);
	fz_catch(ctx)
		rethrow(J);
	js_newarray(J);
	for (i = 0; i < n; ++i) {
		fz_try(ctx)
			opt = pdf_choice_field_option(ctx, widget->obj, export, i);
		fz_catch(ctx)
			rethrow(J);
		js_pushstring(J, opt);
	}
	js_endtry(J);
}

static void ffi_PDFWidget_update(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	int changed = 0;
	fz_try(ctx)
		changed = pdf_update_widget(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
	js_pushboolean(J, changed);
}

static void ffi_PDFWidget_eventEnter(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_enter(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventExit(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_exit(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventDown(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_down(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventUp(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_up(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventFocus(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_focus(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

static void ffi_PDFWidget_eventBlur(js_State *J)
{
	fz_context *ctx = js_getcontext(J);
	pdf_widget *widget = js_touserdata(J, 0, "pdf_widget");
	fz_try(ctx)
		pdf_annot_event_blur(ctx, widget);
	fz_catch(ctx)
		rethrow(J);
}

#endif /* FZ_ENABLE_PDF */

int murun_main(int argc, char **argv)
{
	fz_context *ctx;
	js_State *J;
	int i;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	fz_register_document_handlers(ctx);

	J = js_newstate(alloc, ctx, JS_STRICT);
	js_setcontext(J, ctx);

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

	js_dostring(J, require_js);
	js_dostring(J, stacktrace_js);

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
		jsB_propfun(J, "Buffer.writeByte", ffi_Buffer_writeByte, 1);
		jsB_propfun(J, "Buffer.writeRune", ffi_Buffer_writeRune, 1);
		jsB_propfun(J, "Buffer.writeLine", ffi_Buffer_writeLine, 1);
		jsB_propfun(J, "Buffer.writeBuffer", ffi_Buffer_writeBuffer, 1);
		jsB_propfun(J, "Buffer.write", ffi_Buffer_write, 1);
		jsB_propfun(J, "Buffer.save", ffi_Buffer_save, 1);
	}
	js_setregistry(J, "fz_buffer");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Document.isPDF", ffi_Document_isPDF, 0);
		jsB_propfun(J, "Document.needsPassword", ffi_Document_needsPassword, 0);
		jsB_propfun(J, "Document.authenticatePassword", ffi_Document_authenticatePassword, 1);
		//jsB_propfun(J, "Document.hasPermission", ffi_Document_hasPermission, 1);
		jsB_propfun(J, "Document.getMetaData", ffi_Document_getMetaData, 1);
		jsB_propfun(J, "Document.isReflowable", ffi_Document_isReflowable, 0);
		jsB_propfun(J, "Document.layout", ffi_Document_layout, 3);
		jsB_propfun(J, "Document.countPages", ffi_Document_countPages, 0);
		jsB_propfun(J, "Document.loadPage", ffi_Document_loadPage, 1);
		jsB_propfun(J, "Document.loadOutline", ffi_Document_loadOutline, 0);
	}
	js_setregistry(J, "fz_document");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Page.isPDF", ffi_Page_isPDF, 0);
		jsB_propfun(J, "Page.bound", ffi_Page_bound, 0);
		jsB_propfun(J, "Page.run", ffi_Page_run, 3);
		jsB_propfun(J, "Page.toPixmap", ffi_Page_toPixmap, 4);
		jsB_propfun(J, "Page.toDisplayList", ffi_Page_toDisplayList, 1);
		jsB_propfun(J, "Page.toStructuredText", ffi_Page_toStructuredText, 1);
		jsB_propfun(J, "Page.search", ffi_Page_search, 0);
		jsB_propfun(J, "Page.getLinks", ffi_Page_getLinks, 0);
	}
	js_setregistry(J, "fz_page");

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
	}
	js_setregistry(J, "fz_device");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "ColorSpace.getNumberOfComponents", ffi_ColorSpace_getNumberOfComponents, 0);
		jsB_propfun(J, "ColorSpace.toString", ffi_ColorSpace_toString, 0);
	}
	js_setregistry(J, "fz_colorspace");
	{
		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_gray(ctx)), ffi_gc_fz_colorspace);
		js_setregistry(J, "DeviceGray");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_rgb(ctx)), ffi_gc_fz_colorspace);
		js_setregistry(J, "DeviceRGB");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_bgr(ctx)), ffi_gc_fz_colorspace);
		js_setregistry(J, "DeviceBGR");

		js_getregistry(J, "fz_colorspace");
		js_newuserdata(J, "fz_colorspace", fz_keep_colorspace(ctx, fz_device_cmyk(ctx)), ffi_gc_fz_colorspace);
		js_setregistry(J, "DeviceCMYK");
	}

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Shade.bound", ffi_Shade_bound, 1);
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
		jsB_propfun(J, "Image.getInterpolate", ffi_Image_getInterpolate, 0);
		jsB_propfun(J, "Image.getImageMask", ffi_Image_getImageMask, 0);
		jsB_propfun(J, "Image.getMask", ffi_Image_getMask, 0);
		jsB_propfun(J, "Image.toPixmap", ffi_Image_toPixmap, 2);
	}
	js_setregistry(J, "fz_image");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Font.getName", ffi_Font_getName, 0);
		jsB_propfun(J, "Font.encodeCharacter", ffi_Font_encodeCharacter, 1);
		jsB_propfun(J, "Font.advanceGlyph", ffi_Font_advanceGlyph, 2);
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
		jsB_propfun(J, "Path.bound", ffi_Path_bound, 2);
		jsB_propfun(J, "Path.transform", ffi_Path_transform, 1);
	}
	js_setregistry(J, "fz_path");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "DisplayList.run", ffi_DisplayList_run, 2);
		jsB_propfun(J, "DisplayList.toPixmap", ffi_DisplayList_toPixmap, 3);
		jsB_propfun(J, "DisplayList.toStructuredText", ffi_DisplayList_toStructuredText, 1);
		jsB_propfun(J, "DisplayList.search", ffi_DisplayList_search, 1);
	}
	js_setregistry(J, "fz_display_list");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "StructuredText.search", ffi_StructuredText_search, 1);
		jsB_propfun(J, "StructuredText.highlight", ffi_StructuredText_highlight, 2);
		jsB_propfun(J, "StructuredText.copy", ffi_StructuredText_copy, 2);
	}
	js_setregistry(J, "fz_stext_page");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "Pixmap.bound", ffi_Pixmap_bound, 0);
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

		// Pixmap.samples()
		// Pixmap.invert
		// Pixmap.tint
		// Pixmap.gamma
		// Pixmap.scale()

		jsB_propfun(J, "Pixmap.saveAsPNG", ffi_Pixmap_saveAsPNG, 1);
		// Pixmap.saveAsPNM, PAM, PWG, PCL

		// Pixmap.halftone() -> Bitmap
		// Pixmap.md5()
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
		jsB_propfun(J, "PDFDocument.addPage", ffi_PDFDocument_addPage, 4);
		jsB_propfun(J, "PDFDocument.insertPage", ffi_PDFDocument_insertPage, 2);
		jsB_propfun(J, "PDFDocument.deletePage", ffi_PDFDocument_deletePage, 1);
		jsB_propfun(J, "PDFDocument.countPages", ffi_PDFDocument_countPages, 0);
		jsB_propfun(J, "PDFDocument.findPage", ffi_PDFDocument_findPage, 1);
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

		jsB_propfun(J, "PDFDocument.enableJS", ffi_PDFDocument_enableJS, 0);
	}
	js_setregistry(J, "pdf_document");

	js_getregistry(J, "fz_page");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFPage.getWidgets", ffi_PDFPage_getWidgets, 0);
		jsB_propfun(J, "PDFPage.getAnnotations", ffi_PDFPage_getAnnotations, 0);
		jsB_propfun(J, "PDFPage.createAnnotation", ffi_PDFPage_createAnnotation, 1);
		jsB_propfun(J, "PDFPage.deleteAnnotation", ffi_PDFPage_deleteAnnotation, 1);
		jsB_propfun(J, "PDFPage.update", ffi_PDFPage_update, 0);
	}
	js_setregistry(J, "pdf_page");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFAnnotation.bound", ffi_PDFAnnotation_bound, 0);
		jsB_propfun(J, "PDFAnnotation.run", ffi_PDFAnnotation_run, 2);
		jsB_propfun(J, "PDFAnnotation.toPixmap", ffi_PDFAnnotation_toPixmap, 3);
		jsB_propfun(J, "PDFAnnotation.toDisplayList", ffi_PDFAnnotation_toDisplayList, 0);

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
		jsB_propfun(J, "PDFAnnotation.getInteriorColor", ffi_PDFAnnotation_getInteriorColor, 0);
		jsB_propfun(J, "PDFAnnotation.setInteriorColor", ffi_PDFAnnotation_setInteriorColor, 1);
		jsB_propfun(J, "PDFAnnotation.getAuthor", ffi_PDFAnnotation_getAuthor, 0);
		jsB_propfun(J, "PDFAnnotation.setAuthor", ffi_PDFAnnotation_setAuthor, 1);
		jsB_propfun(J, "PDFAnnotation.getModificationDate", ffi_PDFAnnotation_getModificationDate, 0);
		jsB_propfun(J, "PDFAnnotation.setModificationDate", ffi_PDFAnnotation_setModificationDate, 0);

		jsB_propfun(J, "PDFAnnotation.getInkList", ffi_PDFAnnotation_getInkList, 0);
		jsB_propfun(J, "PDFAnnotation.setInkList", ffi_PDFAnnotation_setInkList, 1);
		jsB_propfun(J, "PDFAnnotation.clearInkList", ffi_PDFAnnotation_clearInkList, 0);
		jsB_propfun(J, "PDFAnnotation.addInkList", ffi_PDFAnnotation_addInkList, 1);
		jsB_propfun(J, "PDFAnnotation.addInkListStroke", ffi_PDFAnnotation_addInkListStroke, 0);
		jsB_propfun(J, "PDFAnnotation.addInkListStrokeVertex", ffi_PDFAnnotation_addInkListStrokeVertex, 2);

		jsB_propfun(J, "PDFAnnotation.getQuadPoints", ffi_PDFAnnotation_getQuadPoints, 0);
		jsB_propfun(J, "PDFAnnotation.setQuadPoints", ffi_PDFAnnotation_setQuadPoints, 1);
		jsB_propfun(J, "PDFAnnotation.clearQuadPoints", ffi_PDFAnnotation_clearQuadPoints, 0);
		jsB_propfun(J, "PDFAnnotation.addQuadPoint", ffi_PDFAnnotation_addQuadPoint, 1);

		jsB_propfun(J, "PDFAnnotation.getVertices", ffi_PDFAnnotation_getVertices, 0);
		jsB_propfun(J, "PDFAnnotation.setVertices", ffi_PDFAnnotation_setVertices, 1);
		jsB_propfun(J, "PDFAnnotation.clearVertices", ffi_PDFAnnotation_clearVertices, 0);
		jsB_propfun(J, "PDFAnnotation.addVertex", ffi_PDFAnnotation_addVertex, 2);

		jsB_propfun(J, "PDFAnnotation.updateAppearance", ffi_PDFAnnotation_updateAppearance, 0);
		jsB_propfun(J, "PDFAnnotation.update", ffi_PDFAnnotation_update, 0);
	}
	js_dup(J);
	js_setglobal(J, "PDFAnnot");
	js_setregistry(J, "pdf_annot");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		// jsB_propfun(J, "PDFWidget.bound", ffi_PDFWidget_bound, 0);
		// jsB_propfun(J, "PDFWidget.run", ffi_PDFWidget_run, 2);
		// jsB_propfun(J, "PDFWidget.toPixmap", ffi_PDFWidget_toPixmap, 3);
		// jsB_propfun(J, "PDFWidget.toDisplayList", ffi_PDFWidget_toDisplayList, 0);

		jsB_propfun(J, "PDFWidget.getFieldType", ffi_PDFWidget_getFieldType, 0);
		jsB_propfun(J, "PDFWidget.getFieldFlags", ffi_PDFWidget_getFieldFlags, 0);
		jsB_propfun(J, "PDFWidget.getRect", ffi_PDFWidget_getRect, 0);
		jsB_propfun(J, "PDFWidget.setRect", ffi_PDFWidget_setRect, 1);
		jsB_propfun(J, "PDFWidget.getValue", ffi_PDFWidget_getValue, 0);
		jsB_propfun(J, "PDFWidget.setTextValue", ffi_PDFWidget_setTextValue, 1);
		jsB_propfun(J, "PDFWidget.setChoiceValue", ffi_PDFWidget_setChoiceValue, 1);
		jsB_propfun(J, "PDFWidget.toggle", ffi_PDFWidget_toggle, 0);
		jsB_propfun(J, "PDFWidget.getMaxLen", ffi_PDFWidget_getMaxLen, 0);
		jsB_propfun(J, "PDFWidget.getOptions", ffi_PDFWidget_getOptions, 1);

		jsB_propfun(J, "PDFWidget.update", ffi_PDFWidget_update, 0);

		jsB_propfun(J, "PDFWidget.eventEnter", ffi_PDFWidget_eventEnter, 0);
		jsB_propfun(J, "PDFWidget.eventExit", ffi_PDFWidget_eventExit, 0);
		jsB_propfun(J, "PDFWidget.eventDown", ffi_PDFWidget_eventDown, 0);
		jsB_propfun(J, "PDFWidget.eventUp", ffi_PDFWidget_eventUp, 0);
		jsB_propfun(J, "PDFWidget.eventFocus", ffi_PDFWidget_eventFocus, 0);
		jsB_propfun(J, "PDFWidget.eventBlur", ffi_PDFWidget_eventBlur, 0);
	}
	js_dup(J);
	js_setglobal(J, "PDFWidget");
	js_setregistry(J, "pdf_widget");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
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
	}
	js_setregistry(J, "pdf_obj");

	js_getregistry(J, "Userdata");
	js_newobjectx(J);
	{
		jsB_propfun(J, "PDFGraftMap.graftObject", ffi_PDFGraftMap_graftObject, 1);
	}
	js_setregistry(J, "pdf_graft_map");
#endif

	js_pushglobal(J);
	{
#if FZ_ENABLE_PDF
		jsB_propcon(J, "pdf_document", "PDFDocument", ffi_new_PDFDocument, 1);
#endif

		jsB_propcon(J, "fz_buffer", "Buffer", ffi_new_Buffer, 1);
		jsB_propcon(J, "fz_document", "Document", ffi_new_Document, 1);
		jsB_propcon(J, "fz_pixmap", "Pixmap", ffi_new_Pixmap, 3);
		jsB_propcon(J, "fz_image", "Image", ffi_new_Image, 1);
		jsB_propcon(J, "fz_font", "Font", ffi_new_Font, 2);
		jsB_propcon(J, "fz_text", "Text", ffi_new_Text, 0);
		jsB_propcon(J, "fz_path", "Path", ffi_new_Path, 0);
		jsB_propcon(J, "fz_display_list", "DisplayList", ffi_new_DisplayList, 1);
		jsB_propcon(J, "fz_device", "DrawDevice", ffi_new_DrawDevice, 2);
		jsB_propcon(J, "fz_device", "DisplayListDevice", ffi_new_DisplayListDevice, 1);
		jsB_propcon(J, "fz_document_writer", "DocumentWriter", ffi_new_DocumentWriter, 3);

		jsB_propfun(J, "readFile", ffi_readFile, 1);

		js_getregistry(J, "DeviceGray");
		js_defproperty(J, -2, "DeviceGray", JS_DONTENUM | JS_READONLY | JS_DONTCONF);

		js_getregistry(J, "DeviceRGB");
		js_defproperty(J, -2, "DeviceRGB", JS_DONTENUM | JS_READONLY | JS_DONTCONF);

		js_getregistry(J, "DeviceBGR");
		js_defproperty(J, -2, "DeviceBGR", JS_DONTENUM | JS_READONLY | JS_DONTCONF);

		js_getregistry(J, "DeviceCMYK");
		js_defproperty(J, -2, "DeviceCMYK", JS_DONTENUM | JS_READONLY | JS_DONTCONF);

		jsB_propfun(J, "setUserCSS", ffi_setUserCSS, 2);
	}

	/* re-implement matrix math in javascript */
	js_dostring(J, "var Identity = Object.freeze([1,0,0,1,0,0]);");
	js_dostring(J, "function Scale(sx,sy) { return [sx,0,0,sy,0,0]; }");
	js_dostring(J, "function Translate(tx,ty) { return [1,0,0,1,tx,ty]; }");
	js_dostring(J, "function Concat(a,b) { return ["
			"a[0] * b[0] + a[1] * b[2],"
			"a[0] * b[1] + a[1] * b[3],"
			"a[2] * b[0] + a[3] * b[2],"
			"a[2] * b[1] + a[3] * b[3],"
			"a[4] * b[0] + a[5] * b[2] + b[4],"
			"a[4] * b[1] + a[5] * b[3] + b[5]];}");

	if (argc > 1) {
		js_pushstring(J, argv[1]);
		js_setglobal(J, "scriptPath");
		js_newarray(J);
		for (i = 2; i < argc; ++i) {
			js_pushstring(J, argv[i]);
			js_setindex(J, -2, i - 2);
		}
		js_setglobal(J, "scriptArgs");
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
