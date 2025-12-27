// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#if FZ_ENABLE_JS

// set limits to how much memory and cpu malicious/broken scripts can use up
#define PDF_JS_LIMIT_RUNTIME (10 << 20) // ten million instructions
#define PDF_JS_LIMIT_MEMORY (100 << 20) // one hundred megabytes

#include "mujs.h"

#include <stdarg.h>
#include <string.h>

struct pdf_js
{
	fz_context *ctx;
	pdf_document *doc;
	js_State *imp;
	pdf_js_console *console;
	void *console_user;
};

FZ_NORETURN static void rethrow(pdf_js *js)
{
	js_newerror(js->imp, fz_convert_error(js->ctx, NULL));
	js_throw(js->imp);
}

/* Unpack argument object with named arguments into actual parameters. */
static pdf_js *unpack_arguments(js_State *J, ...)
{
	if (js_isobject(J, 1))
	{
		int i = 1;
		va_list args;

		js_copy(J, 1);

		va_start(args, J);
		for (;;)
		{
			const char *s = va_arg(args, const char *);
			if (!s)
				break;
			js_getproperty(J, -1, s);
			js_replace(J, i++);
		}
		va_end(args);

		js_pop(J, 1);
	}
	return js_getcontext(J);
}

static void app_alert(js_State *J)
{
	pdf_js *js = unpack_arguments(J, "cMsg", "nIcon", "nType", "cTitle", "oDoc", "oCheckbox", NULL);
	pdf_alert_event evt;

	/* TODO: Currently we do not support app.openDoc() in javascript actions, hence
	oDoc can only point to the current document (or not be passed). When mupdf
	supports opening other documents oDoc must be converted to a pdf_document * that
	can be passed to the callback. In the mean time, we just pas the current document.
	*/
	evt.doc = js->doc;

	evt.message = js_tostring(J, 1);
	evt.icon_type = js_tointeger(J, 2);
	evt.button_group_type = js_tointeger(J, 3);
	evt.title = js_isdefined(J, 4) ? js_tostring(J, 4) : "PDF alert";

	evt.has_check_box = 0;
	evt.check_box_message = NULL;
	evt.initially_checked = 0;
	evt.finally_checked = 0;

	if (js_isobject(J, 6))
	{
		evt.has_check_box = 1;
		evt.check_box_message = "Do not show this message again";
		if (js_hasproperty(J, 6, "cMsg"))
		{
			if (js_iscoercible(J, -1))
				evt.check_box_message = js_tostring(J, -1);
			js_pop(J, 1);
		}
		if (js_hasproperty(J, 6, "bInitialValue"))
		{
			evt.initially_checked = js_tointeger(J, -1);
			js_pop(J, 1);
		}
		if (js_hasproperty(J, 6, "bAfterValue"))
		{
			evt.finally_checked = js_tointeger(J, -1);
			js_pop(J, 1);
		}
	}

	/* These are the default buttons automagically "pressed"
	when the dialog box window is closed in Acrobat. */
	switch (evt.button_group_type)
	{
	default:
	case PDF_ALERT_BUTTON_GROUP_OK:
		evt.button_pressed = PDF_ALERT_BUTTON_OK;
		break;
	case PDF_ALERT_BUTTON_GROUP_OK_CANCEL:
		evt.button_pressed = PDF_ALERT_BUTTON_CANCEL;
		break;
	case PDF_ALERT_BUTTON_GROUP_YES_NO:
		evt.button_pressed = PDF_ALERT_BUTTON_YES;
		break;
	case PDF_ALERT_BUTTON_GROUP_YES_NO_CANCEL:
		evt.button_pressed = PDF_ALERT_BUTTON_CANCEL;
		break;
	}

	fz_try(js->ctx)
		pdf_event_issue_alert(js->ctx, js->doc, &evt);
	fz_catch(js->ctx)
		rethrow(js);

	if (js_isobject(J, 6))
	{
		js_pushboolean(js->imp, evt.finally_checked);
		js_setproperty(js->imp, 6, "bAfterValue");
	}

	js_pushnumber(J, evt.button_pressed);
}

static void app_execMenuItem(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	const char *cMenuItem = js_tostring(J, 1);
	fz_try(js->ctx)
		pdf_event_issue_exec_menu_item(js->ctx, js->doc, cMenuItem);
	fz_catch(js->ctx)
		rethrow(js);
}

static void app_launchURL(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	const char *cUrl = js_tostring(J, 1);
	int bNewFrame = js_toboolean(J, 1);
	fz_try(js->ctx)
		pdf_event_issue_launch_url(js->ctx, js->doc, cUrl, bNewFrame);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_finalize(js_State *J, void *p)
{
	pdf_js *js = js_getcontext(J);
	pdf_drop_obj(js->ctx, p);
}

static void field_buttonSetCaption(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *cCaption = js_tostring(J, 1);
	fz_try(js->ctx)
		pdf_field_set_button_caption(js->ctx, field, cCaption);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_getName(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	char *name = NULL;
	fz_try(js->ctx)
		name = pdf_load_field_name(js->ctx, field);
	fz_catch(js->ctx)
		rethrow(js);
	if (js_try(J)) {
		fz_free(js->ctx, name);
		js_throw(J);
	} else {
		js_pushstring(J, name);
		js_endtry(J);
		fz_free(js->ctx, name);
	}
}

static void field_setName(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_warn(js->ctx, "Unexpected call to field_setName");
}

static void field_getDisplay(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	int display = 0;
	fz_try(js->ctx)
		display = pdf_field_display(js->ctx, field);
	fz_catch(js->ctx)
		rethrow(js);
	js_pushnumber(J, display);
}

static void field_setDisplay(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	int display = js_tonumber(J, 1);
	fz_try(js->ctx)
		pdf_field_set_display(js->ctx, field, display);
	fz_catch(js->ctx)
		rethrow(js);
}

static pdf_obj *load_color(pdf_js *js, int idx)
{
	fz_context *ctx = js->ctx;
	pdf_document *doc = js->doc;
	js_State *J = js->imp;

	pdf_obj *color = NULL;
	int i, n;
	float c;

	n = js_getlength(J, idx);

	/* The only legitimate color expressed as an array of length 1
	 * is [T], meaning transparent. Return a NULL object to represent
	 * transparent */
	if (n <= 1)
		return NULL;

	fz_var(color);

	fz_try(ctx)
	{
		color = pdf_new_array(ctx, doc, n-1);
		for (i = 0; i < n-1; i++)
		{
			js_getindex(J, idx, i+1);
			c = js_tonumber(J, -1);
			js_pop(J, 1);

			pdf_array_push_real(ctx, color, c);
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, color);
		rethrow(js);
	}

	return color;
}

static void field_getFillColor(js_State *J)
{
	js_pushundefined(J);
}

static void field_setFillColor(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	pdf_obj *color = load_color(js, 1);
	fz_try(js->ctx)
		pdf_field_set_fill_color(js->ctx, field, color);
	fz_always(js->ctx)
		pdf_drop_obj(js->ctx, color);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_getTextColor(js_State *J)
{
	js_pushundefined(J);
}

static void field_setTextColor(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	pdf_obj *color = load_color(js, 1);
	fz_try(js->ctx)
		pdf_field_set_text_color(js->ctx, field, color);
	fz_always(js->ctx)
		pdf_drop_obj(js->ctx, color);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_getBorderStyle(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *border_style = NULL;
	fz_try(js->ctx)
		border_style = pdf_field_border_style(js->ctx, field);
	fz_catch(js->ctx)
		rethrow(js);
	js_pushstring(J, border_style);
}

static void field_setBorderStyle(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *border_style = js_tostring(J, 1);
	fz_try(js->ctx)
		pdf_field_set_border_style(js->ctx, field, border_style);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_getValue(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *str = NULL;
	char *end;
	double num;

	fz_try(js->ctx)
		str = pdf_field_value(js->ctx, field);
	fz_catch(js->ctx)
		rethrow(js);

	num = strtod(str, &end);
	if (*str && *end == 0)
		js_pushnumber(J, num);
	else
		js_pushstring(J, str);
}

static void field_setValue(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *value = js_tostring(J, 1);

	fz_try(js->ctx)
		(void)pdf_set_field_value(js->ctx, js->doc, field, value, 0);
	fz_catch(js->ctx)
		rethrow(js);
}

static void field_getType(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field = js_touserdata(J, 0, "Field");
	const char *type;

	fz_try(js->ctx)
		type = pdf_field_type_string(js->ctx, field);
	fz_catch(js->ctx)
		rethrow(js);

	js_pushstring(J, type);
}

static void field_setType(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_warn(js->ctx, "Unexpected call to field_setType");
}

static void doc_getField(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_context *ctx = js->ctx;
	const char *cName = js_tostring(J, 1);
	pdf_obj *dict = NULL;
	pdf_obj *form = NULL;

	fz_try(ctx)
	{
		form = pdf_dict_getl(ctx, pdf_trailer(ctx, js->doc), PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(Fields));
		dict = pdf_lookup_field(ctx, form, cName);
	}
	fz_catch(ctx)
		rethrow(js);

	if (dict)
	{
		js_getregistry(J, "Field");
		js_newuserdata(J, "Field", pdf_keep_obj(js->ctx, dict), field_finalize);
	}
	else
	{
		js_pushnull(J);
	}
}

static void doc_getNumPages(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	int pages = pdf_count_pages(js->ctx, js->doc);
	js_pushnumber(J, pages);
}

static void doc_setNumPages(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_warn(js->ctx, "Unexpected call to doc_setNumPages");
}

static void doc_getMetaString(js_State *J, const char *key)
{
	pdf_js *js = js_getcontext(J);
	char buf[256];
	int ret;

	fz_try(js->ctx)
		ret = fz_lookup_metadata(js->ctx, &js->doc->super, key, buf, nelem(buf)) > 0;
	fz_catch(js->ctx)
		rethrow(js);

	if (ret > 0)
		js_pushstring(J, buf);
	else
		js_pushundefined(J);
}

static void doc_setMetaString(js_State *J, const char *key)
{
	pdf_js *js = js_getcontext(J);
	const char *value = js_tostring(J, 1);
	fz_set_metadata(js->ctx, &js->doc->super, key, value);
}

static void doc_getMetaDate(js_State *J, const char *key)
{
	pdf_js *js = js_getcontext(J);
	char buf[256];
	int ret;
	double time;

	fz_try(js->ctx)
	{
		ret = fz_lookup_metadata(js->ctx, &js->doc->super, key, buf, nelem(buf)) > 0;
		if (ret > 0)
			time = pdf_parse_date(js->ctx, buf);
	}
	fz_catch(js->ctx)
		rethrow(js);

	if (ret > 0)
	{
		js_getglobal(J, "Date");
		js_pushnumber(J, time * 1000);
		js_construct(J, 1);
	}
	else
		js_pushundefined(J);
}

static void doc_setMetaDate(js_State *J, const char *key)
{
	pdf_js *js = js_getcontext(J);
	int64_t time;
	char value[40];

	// Coerce the argument into a date object and extract the time value.
	js_getglobal(J, "Date");
	js_copy(J, 1);
	js_construct(J, 1);
	time = js_tonumber(J, -1) / 1000;
	js_pop(J, 1);

	fz_try(js->ctx)
		if (pdf_format_date(js->ctx, time, value, nelem(value)))
			fz_set_metadata(js->ctx, &js->doc->super, key, value);
	fz_catch(js->ctx)
		rethrow(js);
}

static void doc_getAuthor(js_State *J) { doc_getMetaString(J, FZ_META_INFO_AUTHOR); }
static void doc_setAuthor(js_State *J) { doc_setMetaString(J, FZ_META_INFO_AUTHOR); }
static void doc_getTitle(js_State *J) { doc_getMetaString(J, FZ_META_INFO_TITLE); }
static void doc_setTitle(js_State *J) { doc_setMetaString(J, FZ_META_INFO_TITLE); }
static void doc_getSubject(js_State *J) { doc_getMetaString(J, FZ_META_INFO_SUBJECT); }
static void doc_setSubject(js_State *J) { doc_setMetaString(J, FZ_META_INFO_SUBJECT); }
static void doc_getKeywords(js_State *J) { doc_getMetaString(J, FZ_META_INFO_KEYWORDS); }
static void doc_setKeywords(js_State *J) { doc_setMetaString(J, FZ_META_INFO_KEYWORDS); }
static void doc_getCreator(js_State *J) { doc_getMetaString(J, FZ_META_INFO_CREATOR); }
static void doc_setCreator(js_State *J) { doc_setMetaString(J, FZ_META_INFO_CREATOR); }
static void doc_getProducer(js_State *J) { doc_getMetaString(J, FZ_META_INFO_PRODUCER); }
static void doc_setProducer(js_State *J) { doc_setMetaString(J, FZ_META_INFO_PRODUCER); }
static void doc_getCreationDate(js_State *J) { doc_getMetaDate(J, FZ_META_INFO_CREATIONDATE); }
static void doc_setCreationDate(js_State *J) { doc_setMetaDate(J, FZ_META_INFO_CREATIONDATE); }
static void doc_getModDate(js_State *J) { doc_getMetaDate(J, FZ_META_INFO_MODIFICATIONDATE); }
static void doc_setModDate(js_State *J) { doc_setMetaDate(J, FZ_META_INFO_MODIFICATIONDATE); }

static void doc_resetForm(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	pdf_obj *field, *form;
	fz_context *ctx = js->ctx;
	int i, n;

	fz_try(ctx)
		form = pdf_dict_getl(ctx, pdf_trailer(ctx, js->doc), PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(Fields));
	fz_catch(ctx)
		rethrow(js);

	/* An array of fields has been passed in. Call pdf_reset_field on each item. */
	if (js_isarray(J, 1))
	{
		n = js_getlength(J, 1);
		for (i = 0; i < n; ++i)
		{
			js_getindex(J, 1, i);
			fz_try(ctx)
				field = pdf_lookup_field(ctx, form, js_tostring(J, -1));
			fz_catch(ctx)
				rethrow(js);
			if (field)
				pdf_field_reset(ctx, js->doc, field);
			js_pop(J, 1);
		}
	}

	/* No argument or null passed in means reset all. */
	else
	{
		fz_try(ctx)
		{
			n = pdf_array_len(ctx, form);
		for (i = 0; i < n; i++)
				pdf_field_reset(ctx, js->doc, pdf_array_get(ctx, form, i));
		}
			fz_catch(ctx)
				rethrow(js);
		}
	}

static void doc_print(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_try(js->ctx)
		pdf_event_issue_print(js->ctx, js->doc);
	fz_catch(js->ctx)
		rethrow(js);
}

static void doc_mailDoc(js_State *J)
{
	pdf_js *js = unpack_arguments(J, "bUI", "cTo", "cCc", "cBcc", "cSubject", "cMessage", NULL);
	pdf_mail_doc_event evt;

	evt.ask_user = js_isdefined(J, 1) ? js_toboolean(J, 1) : 1;
	evt.to = js_tostring(J, 2);
	evt.cc = js_tostring(J, 3);
	evt.bcc = js_tostring(J, 4);
	evt.subject = js_tostring(J, 5);
	evt.message = js_tostring(J, 6);

	fz_try(js->ctx)
		pdf_event_issue_mail_doc(js->ctx, js->doc, &evt);
	fz_catch(js->ctx)
		rethrow(js);
}

static void doc_calculateNow(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_try(js->ctx)
		pdf_calculate_form(js->ctx, js->doc);
	fz_catch(js->ctx)
		rethrow(js);
}

static void console_println(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	if (js->console && js->console->write)
	{
		int i, top = js_gettop(J);
		js->console->write(js->console_user, "\n");
		for (i = 1; i < top; ++i) {
			const char *s = js_tostring(J, i);
			if (i > 1)
				js->console->write(js->console_user, " ");
			js->console->write(js->console_user, s);
		}
	}
	js_pushboolean(J, 1);
}

static void console_clear(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	if (js->console && js->console->clear)
		js->console->clear(js->console_user);
	js_pushundefined(J);
}

static void console_show(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	if (js->console && js->console->show)
		js->console->show(js->console_user);
	js_pushundefined(J);
}

static void console_hide(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	if (js->console && js->console->hide)
		js->console->hide(js->console_user);
	js_pushundefined(J);
}

static void util_printf_d(fz_context *ctx, fz_buffer *out, int ds, int sign, int pad, unsigned int w, int base, int value)
{
	static const char *digits = "0123456789abcdef";
	char buf[50];
	unsigned int a, i;
	int m = 0;

	if (w > sizeof buf)
		w = sizeof buf;

	if (value < 0)
	{
		sign = '-';
		a = -value;
	}
	else
	{
		a = value;
	}

	i = 0;
	do
	{
		buf[i++] = digits[a % base];
		a /= base;
		if (a > 0 && ++m == 3)
		{
			if (ds == 0) buf[i++] = ',';
			if (ds == 2) buf[i++] = '.';
			m = 0;
		}
	} while (a);

	if (sign)
	{
		if (pad == '0')
			while (i < w - 1)
				buf[i++] = pad;
		buf[i++] = sign;
	}
	while (i < w)
		buf[i++] = pad;

	while (i > 0)
		fz_append_byte(ctx, out, buf[--i]);
}

static void util_printf_f(fz_context *ctx, fz_buffer *out, int ds, int sign, int pad, int special, unsigned int w, int p, double value)
{
	char buf[40], *point, *digits = buf;
	size_t n = 0;
	int m = 0;

	fz_snprintf(buf, sizeof buf, "%.*f", p, value);

	if (*digits == '-')
	{
		sign = '-';
		++digits;
	}

	if (*digits != '.' && (*digits < '0' || *digits > '9'))
	{
		fz_append_string(ctx, out, "nan");
		return;
	}

	n = strlen(digits);
	if (sign)
		++n;
	point = strchr(digits, '.');
	if (point)
		m = 3 - (point - digits) % 3;
	else
	{
		m = 3 - n % 3;
		if (special)
			++n;
	}
	if (m == 3)
		m = 0;

	if (pad == '0' && sign)
		fz_append_byte(ctx, out, sign);
	for (; n < w; ++n)
		fz_append_byte(ctx, out, pad);
	if (pad == ' ' && sign)
		fz_append_byte(ctx, out, sign);

	while (*digits && *digits != '.')
	{
		fz_append_byte(ctx, out, *digits++);
		if (++m == 3 && *digits && *digits != '.')
		{
			if (ds == 0) fz_append_byte(ctx, out, ',');
			if (ds == 2) fz_append_byte(ctx, out, '.');
			m = 0;
		}
	}

	if (*digits == '.' || special)
	{
		if (ds == 0 || ds == 1)
			fz_append_byte(ctx, out, '.');
		else
			fz_append_byte(ctx, out, ',');
	}

	if (*digits == '.')
	{
		++digits;
		while (*digits)
			fz_append_byte(ctx, out, *digits++);
	}
}

static void util_printf(js_State *J)
{
	pdf_js *js = js_getcontext(J);
	fz_context *ctx = js->ctx;
	const char *fmt = js_tostring(J, 1);
	fz_buffer *out = NULL;
	int ds, p, sign, pad, special;
	unsigned int w;
	int c, i = 1;
	int failed = 0;
	const char *str;

	fz_var(out);
	fz_try(ctx)
	{
		out = fz_new_buffer(ctx, 256);

		while ((c = *fmt++) != 0)
		{
			if (c == '%')
			{
				c = *fmt++;

				ds = 1;
				if (c == ',')
				{
					c = *fmt++;
					if (!c)
						break;
					ds = c - '0';
				}

				special = 0;
				sign = 0;
				pad = ' ';
				while (c == ' ' || c == '+' || c == '0' || c == '#')
				{
					if (c == '+') sign = '+';
					else if (c == ' ') sign = ' ';
					else if (c == '0') pad = '0';
					else if (c == '#') special = 1;
					c = *fmt++;
				}
				if (!pad)
					pad = ' ';
				if (!c)
					break;

				w = 0;
				while (c >= '0' && c <= '9')
				{
					w = w * 10 + (c - '0');
					c = *fmt++;
				}
				if (!c)
					break;

				p = 0;
				if (c == '.')
				{
					c = *fmt++;
					while (c >= '0' && c <= '9')
					{
						p = p * 10 + (c - '0');
						c = *fmt++;
					}
				}
				else
				{
					special = 1;
				}
				if (!c)
					break;

				switch (c)
				{
				case '%':
					fz_append_byte(ctx, out, '%');
					break;
				case 'x':
					util_printf_d(ctx, out, ds, sign, pad, w, 16, js_tryinteger(J, ++i, 0));
					break;
				case 'd':
					util_printf_d(ctx, out, ds, sign, pad, w, 10, js_tryinteger(J, ++i, 0));
					break;
				case 'f':
					util_printf_f(ctx, out, ds, sign, pad, special, w, p, js_trynumber(J, ++i, 0));
					break;
				case 's':
				default:
					fz_append_string(ctx, out, js_trystring(J, ++i, ""));
				}
			}
			else
			{
				fz_append_byte(ctx, out, c);
			}
		}

		str = fz_string_from_buffer(ctx, out);
		if (js_try(J))
		{
			failed = 1;
		}
		else
		{
			js_pushstring(J, str);
			js_endtry(J);
		}
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, out);
	fz_catch(ctx)
		rethrow(js);

	if (failed)
		js_throw(J);
}

static void addmethod(js_State *J, const char *name, js_CFunction fun, int n)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_newcfunction(J, fun, name, n);
	js_defproperty(J, -2, realname, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

static void addproperty(js_State *J, const char *name, js_CFunction getfun, js_CFunction setfun)
{
	const char *realname = strchr(name, '.');
	realname = realname ? realname + 1 : name;
	js_newcfunction(J, getfun, name, 0);
	js_newcfunction(J, setfun, name, 1);
	js_defaccessor(J, -3, realname, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

static int declare_dom(pdf_js *js)
{
	js_State *J = js->imp;

	if (js_try(J))
	{
		return -1;
	}

	/* Allow access to the global environment via the 'global' name */
	js_pushglobal(J);
	js_defglobal(J, "global", JS_READONLY | JS_DONTCONF | JS_DONTENUM);

	/* Create the 'event' object */
	js_newobject(J);
	js_defglobal(J, "event", JS_READONLY | JS_DONTCONF | JS_DONTENUM);

	/* Create the 'util' object */
	js_newobject(J);
	{
		// TODO: util.printd
		// TODO: util.printx
		addmethod(J, "util.printf", util_printf, 1);
	}
	js_defglobal(J, "util", JS_READONLY | JS_DONTCONF | JS_DONTENUM);

	/* Create the 'app' object */
	js_newobject(J);
	{
#ifdef _WIN32
		js_pushstring(J, "WIN");
#elif defined(__APPLE__)
		js_pushstring(J, "MAC");
#else
		js_pushstring(J, "UNIX");
#endif
		js_defproperty(J, -2, "app.platform", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

		addmethod(J, "app.alert", app_alert, 6);
		addmethod(J, "app.execMenuItem", app_execMenuItem, 1);
		addmethod(J, "app.launchURL", app_launchURL, 2);
	}
	js_defglobal(J, "app", JS_READONLY | JS_DONTCONF | JS_DONTENUM);

	/* Create the Field prototype object */
	js_newobject(J);
	{
		addproperty(J, "Field.value", field_getValue, field_setValue);
		addproperty(J, "Field.type", field_getType, field_setType);
		addproperty(J, "Field.borderStyle", field_getBorderStyle, field_setBorderStyle);
		addproperty(J, "Field.textColor", field_getTextColor, field_setTextColor);
		addproperty(J, "Field.fillColor", field_getFillColor, field_setFillColor);
		addproperty(J, "Field.display", field_getDisplay, field_setDisplay);
		addproperty(J, "Field.name", field_getName, field_setName);
		addmethod(J, "Field.buttonSetCaption", field_buttonSetCaption, 1);
	}
	js_setregistry(J, "Field");

	/* Create the console object */
	js_newobject(J);
	{
		addmethod(J, "console.println", console_println, 1);
		addmethod(J, "console.clear", console_clear, 0);
		addmethod(J, "console.show", console_show, 0);
		addmethod(J, "console.hide", console_hide, 0);
	}
	js_defglobal(J, "console", JS_READONLY | JS_DONTCONF | JS_DONTENUM);

	/* Put all of the Doc methods in the global object, which is used as
	 * the 'this' binding for regular non-strict function calls. */
	js_pushglobal(J);
	{
		addproperty(J, "Doc.numPages", doc_getNumPages, doc_setNumPages);
		addproperty(J, "Doc.author", doc_getAuthor, doc_setAuthor);
		addproperty(J, "Doc.title", doc_getTitle, doc_setTitle);
		addproperty(J, "Doc.subject", doc_getSubject, doc_setSubject);
		addproperty(J, "Doc.keywords", doc_getKeywords, doc_setKeywords);
		addproperty(J, "Doc.creator", doc_getCreator, doc_setCreator);
		addproperty(J, "Doc.producer", doc_getProducer, doc_setProducer);
		addproperty(J, "Doc.creationDate", doc_getCreationDate, doc_setCreationDate);
		addproperty(J, "Doc.modDate", doc_getModDate, doc_setModDate);
		addmethod(J, "Doc.getField", doc_getField, 1);
		addmethod(J, "Doc.resetForm", doc_resetForm, 0);
		addmethod(J, "Doc.calculateNow", doc_calculateNow, 0);
		addmethod(J, "Doc.print", doc_print, 0);
		addmethod(J, "Doc.mailDoc", doc_mailDoc, 6);
	}
	js_pop(J, 1);

	js_endtry(J);

	return 0;
}

static int preload_helpers(pdf_js *js)
{
	if (js_try(js->imp))
		return -1;

	/* When testing on the cluster:
	 * Use a fixed date for "new Date" and Date.now().
	 * Sadly, this breaks uses of the Date function without the new keyword.
	 * Return a fixed random sequence from Math.random().
	 */
#ifdef CLUSTER
	js_dostring(js->imp,
"var MuPDFOldDate = Date\n"
"Date = function() { return new MuPDFOldDate(298252800000); }\n"
"Date.now = function() { return 298252800000; }\n"
"Date.UTC = function() { return 298252800000; }\n"
"Date.parse = MuPDFOldDate.parse;\n"
"Math.random = function() { return (Math.random.seed = Math.random.seed * 48271 % 2147483647) / 2147483647; }\n"
"Math.random.seed = 217;\n"
	);
#endif

	js_dostring(js->imp,
#include "js/util.js.h"
	);

	js_endtry(js->imp);
	return 0;
}

void pdf_drop_js(fz_context *ctx, pdf_js *js)
{
	if (js)
	{
		if (js->console && js->console->drop)
			js->console->drop(js->console, js->console_user);
		js_freestate(js->imp);
		fz_free(ctx, js);
	}
}

static void *pdf_js_alloc(void *actx, void *ptr, int n)
{
	return fz_realloc_no_throw(actx, ptr, n);
}

static void default_js_console_clear(void *user)
{
	fz_context *ctx = user;
	fz_write_string(ctx, fz_stddbg(ctx), "--- clear console ---\n");
}

static void default_js_console_write(void *user, const char *message)
{
	fz_context *ctx = user;
	fz_write_string(ctx, fz_stddbg(ctx), message);
}

static pdf_js_console default_js_console = {
	NULL,
	NULL,
	NULL,
	default_js_console_clear,
	default_js_console_write,
};

static pdf_js *pdf_new_js(fz_context *ctx, pdf_document *doc)
{
	pdf_js *js = fz_malloc_struct(ctx, pdf_js);

	js->ctx = ctx;
	js->doc = doc;

	fz_try(ctx)
	{
		/* Initialise the javascript engine, passing the fz_context for use in memory allocation. */
		js->imp = js_newstate(pdf_js_alloc, ctx, 0);
		if (!js->imp)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot initialize javascript engine");

		/* Also set our pdf_js context, so we can retrieve it in callbacks. */
		js_setcontext(js->imp, js);

		js->console = &default_js_console;
		js->console_user = js->ctx;

		if (declare_dom(js))
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot initialize dom interface");
		if (preload_helpers(js))
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot initialize helper functions");
	}
	fz_catch(ctx)
	{
		pdf_drop_js(ctx, js);
		fz_rethrow(ctx);
	}

	return js;
}

static void pdf_js_load_document_level(pdf_js *js)
{
	fz_context *ctx = js->ctx;
	pdf_document *doc = js->doc;
	pdf_obj *javascript;
	int len, i;
	int in_op = 0;

	javascript = pdf_load_name_tree(ctx, doc, PDF_NAME(JavaScript));
	len = pdf_dict_len(ctx, javascript);

	fz_var(in_op);

	fz_try(ctx)
	{
		pdf_begin_operation(ctx, doc, "Document level Javascript");
		in_op = 1;
		for (i = 0; i < len; i++)
		{
			pdf_obj *fragment = pdf_dict_get_val(ctx, javascript, i);
			pdf_obj *code = pdf_dict_get(ctx, fragment, PDF_NAME(JS));
			char *codebuf = pdf_load_stream_or_string_as_utf8(ctx, code);
			char buf[100];
			if (pdf_is_indirect(ctx, code))
				fz_snprintf(buf, sizeof buf, "%d", pdf_to_num(ctx, code));
			else
				fz_snprintf(buf, sizeof buf, "Root/Names/JavaScript/Names/%d/JS", (i+1)*2);
			pdf_js_execute(js, buf, codebuf, NULL);
			fz_free(ctx, codebuf);
		}
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, javascript);
	fz_catch(ctx)
	{
		if (in_op)
			pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

void pdf_js_event_init(pdf_js *js, pdf_obj *target, const char *value, int willCommit)
{
	if (js)
	{
		js_getglobal(js->imp, "event");
		{
			js_pushboolean(js->imp, 1);
			js_setproperty(js->imp, -2, "rc");

			js_pushboolean(js->imp, willCommit);
			js_setproperty(js->imp, -2, "willCommit");

			js_getregistry(js->imp, "Field");
			js_newuserdata(js->imp, "Field", pdf_keep_obj(js->ctx, target), field_finalize);
			js_setproperty(js->imp, -2, "target");

			js_pushstring(js->imp, value);
			js_setproperty(js->imp, -2, "value");
		}
		js_pop(js->imp, 1);
	}
}

int pdf_js_event_result(pdf_js *js)
{
	int rc = 1;
	if (js)
	{
		js_getglobal(js->imp, "event");
		js_getproperty(js->imp, -1, "rc");
		rc = js_tryboolean(js->imp, -1, 1);
		js_pop(js->imp, 2);
	}
	return rc;
}

int pdf_js_event_result_validate(pdf_js *js, char **newtext)
{
	int rc = 1;
	*newtext = NULL;
	if (js)
	{
		js_getglobal(js->imp, "event");
		js_getproperty(js->imp, -1, "rc");
		rc = js_tryboolean(js->imp, -1, 1);
		js_pop(js->imp, 1);
		if (rc)
		{
			js_getproperty(js->imp, -1, "value");
			*newtext = fz_strdup(js->ctx, js_trystring(js->imp, -1, ""));
			js_pop(js->imp, 1);
		}
		js_pop(js->imp, 1);
	}
	return rc;
}

void pdf_js_event_init_keystroke(pdf_js *js, pdf_obj *target, pdf_keystroke_event *evt)
{
	if (js)
	{
		pdf_js_event_init(js, target, evt->value, evt->willCommit);
		js_getglobal(js->imp, "event");
		{
			js_pushstring(js->imp, evt->change);
			js_setproperty(js->imp, -2, "change");
			js_pushnumber(js->imp, evt->selStart);
			js_setproperty(js->imp, -2, "selStart");
			js_pushnumber(js->imp, evt->selEnd);
			js_setproperty(js->imp, -2, "selEnd");
		}
		js_pop(js->imp, 1);
	}
}

int pdf_js_event_result_keystroke(pdf_js *js, pdf_keystroke_event *evt)
{
	int rc = 1;
	if (js)
	{
		js_getglobal(js->imp, "event");
		{
			js_getproperty(js->imp, -1, "rc");
			rc = js_tryboolean(js->imp, -1, 1);
			js_pop(js->imp, 1);
			if (rc)
			{
				js_getproperty(js->imp, -1, "change");
				evt->newChange = fz_strdup(js->ctx, js_trystring(js->imp, -1, ""));
				js_pop(js->imp, 1);
				js_getproperty(js->imp, -1, "value");
				evt->newValue = fz_strdup(js->ctx, js_trystring(js->imp, -1, ""));
				js_pop(js->imp, 1);
				js_getproperty(js->imp, -1, "selStart");
				evt->selStart = js_tryinteger(js->imp, -1, 0);
				js_pop(js->imp, 1);
				js_getproperty(js->imp, -1, "selEnd");
				evt->selEnd = js_tryinteger(js->imp, -1, 0);
				js_pop(js->imp, 1);
			}
		}
		js_pop(js->imp, 1);
	}
	return rc;
}

char *pdf_js_event_value(pdf_js *js)
{
	char *value = NULL;
	if (js)
	{
		js_getglobal(js->imp, "event");
		js_getproperty(js->imp, -1, "value");
		value = fz_strdup(js->ctx, js_trystring(js->imp, -1, "undefined"));
		js_pop(js->imp, 2);
	}
	return value;
}

void pdf_js_execute(pdf_js *js, const char *name, const char *source, char **result)
{
	fz_context *ctx;
	js_State *J;

	if (!js)
		return;

	ctx = js->ctx;
	J = js->imp;

	pdf_begin_implicit_operation(ctx, js->doc);
	fz_try(ctx)
	{
		if (js_ploadstring(J, name, source)) {
			if (result)
				*result = fz_strdup(ctx, js_trystring(J, -1, "Error"));
			js_pop(J, 1);
		} else {
			js_pushundefined(J);
			js_setlimit(J, PDF_JS_LIMIT_RUNTIME, PDF_JS_LIMIT_MEMORY);
			if (js_pcall(J, 0)) {
				if (result)
					*result = fz_strdup(ctx, js_trystring(J, -1, "Error"));
				else
					fz_write_printf(ctx, fz_stddbg(ctx), "js: %s\n", js_trystring(J, -1, "Error"));
				js_pop(J, 1);
			} else {
				if (result)
					*result = fz_strdup(ctx, js_tryrepr(J, -1, "can't convert to string"));
				js_pop(J, 1);
			}
		}
		pdf_end_operation(ctx, js->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, js->doc);
		fz_rethrow(ctx);
	}
}

pdf_js_console *pdf_js_get_console(fz_context *ctx, pdf_document *doc)
{
	return (doc && doc->js) ? doc->js->console : NULL;
}

void pdf_js_set_console(fz_context *ctx, pdf_document *doc, pdf_js_console *console, void *user)
{
	if (doc->js)
	{
		if (doc->js->console && doc->js->console->drop)
			doc->js->console->drop(doc->js->console, doc->js->console_user);

		doc->js->console = console;
		doc->js->console_user = user;
	}
}

void pdf_enable_js(fz_context *ctx, pdf_document *doc)
{
	if (!doc->js)
	{
		doc->js = pdf_new_js(ctx, doc);
		pdf_js_load_document_level(doc->js);
	}
}

void pdf_disable_js(fz_context *ctx, pdf_document *doc)
{
	pdf_drop_js(ctx, doc->js);
	doc->js = NULL;
}

int pdf_js_supported(fz_context *ctx, pdf_document *doc)
{
	return doc->js != NULL;
}

#else /* FZ_ENABLE_JS */

void pdf_drop_js(fz_context *ctx, pdf_js *js) { }
void pdf_enable_js(fz_context *ctx, pdf_document *doc) { }
void pdf_disable_js(fz_context *ctx, pdf_document *doc) { }
int pdf_js_supported(fz_context *ctx, pdf_document *doc) { return 0; }
void pdf_js_event_init(pdf_js *js, pdf_obj *target, const char *value, int willCommit) { }
void pdf_js_event_init_keystroke(pdf_js *js, pdf_obj *target, pdf_keystroke_event *evt) { }
int pdf_js_event_result_keystroke(pdf_js *js, pdf_keystroke_event *evt) { return 1; }
int pdf_js_event_result(pdf_js *js) { return 1; }
char *pdf_js_event_value(pdf_js *js) { return ""; }
void pdf_js_execute(pdf_js *js, const char *name, const char *source, char **result) { }
int pdf_js_event_result_validate(pdf_js *js, char **newvalue) { *newvalue=NULL; return 1; }
pdf_js_console *pdf_js_get_console(fz_context *ctx, pdf_document *doc) { return NULL; }
void pdf_js_set_console(fz_context *ctx, pdf_document *doc, pdf_js_console *console, void *user) { }


#endif /* FZ_ENABLE_JS */
