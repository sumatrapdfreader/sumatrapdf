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

/*
 * pdfshow -- the ultimate pdf debugging tool
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static pdf_document *doc = NULL;
static fz_output *out = NULL;
static pdf_object_labels *labels = NULL;
static int showbinary = 0;
static int showdecode = 1;
static int do_tight = 0;
static int do_repair = 0;
static int do_label = 0;
static int showcolumn;

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool show [options] file.pdf ( trailer | xref | pages | grep | outline | js | form | <path> ) *\n"
		"\t-p -\tpassword\n"
		"\t-o -\toutput file\n"
		"\t-e\tleave stream contents in their original form\n"
		"\t-b\tprint only stream contents, as raw binary data\n"
		"\t-g\tprint only object, one line per object, suitable for grep\n"
		"\t-r\tforce repair before showing any objects\n"
		"\t-L\tshow object labels\n"
		"\tpath: path to an object, starting with either an object number,\n"
		"\t\t'pages', 'trailer', or a property in the trailer;\n"
		"\t\tpath elements separated by '.' or '/'. Path elements must be\n"
		"\t\tarray index numbers, dictionary property names, or '*'.\n"
	);
	return 1;
}

static void showtrailer(fz_context *ctx)
{
	if (do_tight)
		fz_write_printf(ctx, out, "trailer ");
	else
		fz_write_printf(ctx, out, "trailer\n");
	pdf_print_obj(ctx, out, pdf_trailer(ctx, doc), do_tight, 1);
	fz_write_printf(ctx, out, "\n");
}

static void showxref(fz_context *ctx)
{
	int i;
	int xref_len = pdf_xref_len(ctx, doc);
	fz_write_printf(ctx, out, "xref\n0 %d\n", xref_len);
	for (i = 0; i < xref_len; i++)
	{
		pdf_xref_entry *entry = pdf_get_xref_entry_no_null(ctx, doc, i);
		fz_write_printf(ctx, out, "%05d: %010d %05d %c \n",
				i,
				(int)entry->ofs,
				entry->gen,
				entry->type ? entry->type : '-');
	}
}

static void showpages(fz_context *ctx)
{
	pdf_obj *ref;
	int i, n = pdf_count_pages(ctx, doc);
	for (i = 0; i < n; ++i)
	{
		ref = pdf_lookup_page_obj(ctx, doc, i);
		fz_write_printf(ctx, out, "page %d = %d 0 R\n", i + 1, pdf_to_num(ctx, ref));
	}
}

static void showsafe(unsigned char *buf, size_t n)
{
	size_t i;
	for (i = 0; i < n; i++) {
		if (buf[i] == '\r' || buf[i] == '\n') {
			putchar('\n');
			showcolumn = 0;
		}
		else if (buf[i] < 32 || buf[i] > 126) {
			putchar('.');
			showcolumn ++;
		}
		else {
			putchar(buf[i]);
			showcolumn ++;
		}
		if (showcolumn == 79) {
			putchar('\n');
			showcolumn = 0;
		}
	}
}

static void showstream(fz_context *ctx, int num)
{
	fz_stream *stm;
	unsigned char buf[2048];
	size_t n;

	showcolumn = 0;

	if (showdecode)
		stm = pdf_open_stream_number(ctx, doc, num);
	else
		stm = pdf_open_raw_stream_number(ctx, doc, num);

	while (1)
	{
		n = fz_read(ctx, stm, buf, sizeof buf);
		if (n == 0)
			break;
		if (showbinary)
			fz_write_data(ctx, out, buf, n);
		else
			showsafe(buf, n);
	}

	fz_drop_stream(ctx, stm);
}

static void showlabel(fz_context *ctx, void *arg, const char *label)
{
	fz_write_printf(ctx, arg, "%% %s\n", label);
}

static void showobject(fz_context *ctx, pdf_obj *ref)
{
	pdf_obj *obj = pdf_resolve_indirect(ctx, ref);
	int num = pdf_to_num(ctx, ref);
	if (pdf_is_stream(ctx, ref))
	{
		if (showbinary)
		{
			showstream(ctx, num);
		}
		else
		{
			if (do_label)
				pdf_label_object(ctx, labels, num, showlabel, out);
			if (do_tight)
			{
				fz_write_printf(ctx, out, "%d 0 obj ", num);
				pdf_print_obj(ctx, out, obj, 1, 1);
				fz_write_printf(ctx, out, " stream\n");
			}
			else
			{
				fz_write_printf(ctx, out, "%d 0 obj\n", num);
				pdf_print_obj(ctx, out, obj, 0, 1);
				fz_write_printf(ctx, out, "\nstream\n");
				showstream(ctx, num);
				fz_write_printf(ctx, out, "endstream\n");
				fz_write_printf(ctx, out, "endobj\n");
			}
		}
	}
	else
	{
		if (do_label)
			pdf_label_object(ctx, labels, num, showlabel, out);
		if (do_tight)
		{
			fz_write_printf(ctx, out, "%d 0 obj ", num);
			pdf_print_obj(ctx, out, obj, 1, 1);
			fz_write_printf(ctx, out, "\n");
		}
		else
		{
			fz_write_printf(ctx, out, "%d 0 obj\n", num);
			pdf_print_obj(ctx, out, obj, 0, 1);
			fz_write_printf(ctx, out, "\nendobj\n");
		}
	}
}

static void showgrep(fz_context *ctx)
{
	pdf_obj *ref, *obj;
	int i, len;

	len = pdf_count_objects(ctx, doc);
	for (i = 0; i < len; i++)
	{
		pdf_xref_entry *entry = pdf_get_xref_entry_no_null(ctx, doc, i);
		if (entry->type == 'n' || entry->type == 'o')
		{
			ref = NULL;

			fz_var(ref);
			fz_try(ctx)
			{
				ref = pdf_new_indirect(ctx, doc, i, 0);
				obj = pdf_resolve_indirect(ctx, ref);
			}
			fz_catch(ctx)
			{
				pdf_drop_obj(ctx, ref);
				fz_warn(ctx, "skipping object (%d 0 R)", i);
				continue;
			}

			pdf_sort_dict(ctx, obj);

			fz_write_printf(ctx, out, "%d 0 obj ", i);
			pdf_print_obj(ctx, out, obj, 1, 1);
			if (pdf_is_stream(ctx, ref))
				fz_write_printf(ctx, out, " stream");
			fz_write_printf(ctx, out, "\n");

			pdf_drop_obj(ctx, ref);
		}
	}

	fz_write_printf(ctx, out, "trailer ");
	pdf_print_obj(ctx, out, pdf_trailer(ctx, doc), 1, 1);
	fz_write_printf(ctx, out, "\n");
}

static void
print_outline(fz_context *ctx, fz_outline *outline, int level)
{
	int i;
	while (outline)
	{
		if (outline->down)
			fz_write_byte(ctx, out, outline->is_open ? '-' : '+');
		else
			fz_write_byte(ctx, out, '|');

		for (i = 0; i < level; i++)
			fz_write_byte(ctx, out, '\t');
		fz_write_printf(ctx, out, "%Q\t%s\n", outline->title, outline->uri);
		if (outline->down)
			print_outline(ctx, outline->down, level + 1);
		outline = outline->next;
	}
}

static void showoutline(fz_context *ctx)
{
	fz_outline *outline = fz_load_outline(ctx, (fz_document*)doc);
	fz_try(ctx)
		print_outline(ctx, outline, 1);
	fz_always(ctx)
		fz_drop_outline(ctx, outline);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void showtext(fz_context *ctx, char *buf, int indent)
{
	int bol = 1;
	int c = *buf;
	while (*buf)
	{
		c = *buf++;
		if (c == '\r')
		{
			if (*buf == '\n')
				++buf;
			c = '\n';
		}
		if (indent && bol)
			fz_write_byte(ctx, out, '\t');
		fz_write_byte(ctx, out, c);
		bol = (c == '\n');
	}
	if (!bol)
		fz_write_byte(ctx, out, '\n');
}

static void showjs(fz_context *ctx)
{
	pdf_obj *tree;
	int i;

	tree = pdf_load_name_tree(ctx, doc, PDF_NAME(JavaScript));
	for (i = 0; i < pdf_dict_len(ctx, tree); ++i)
	{
		pdf_obj *name = pdf_dict_get_key(ctx, tree, i);
		pdf_obj *action = pdf_dict_get_val(ctx, tree, i);
		pdf_obj *js = pdf_dict_get(ctx, action, PDF_NAME(JS));
		char *src = pdf_load_stream_or_string_as_utf8(ctx, js);
		fz_write_printf(ctx, out, "// %s\n", pdf_to_name(ctx, name));
		showtext(ctx, src, 0);
		fz_free(ctx, src);
	}
}

static void showaction(fz_context *ctx, pdf_obj *action, const char *name)
{
	if (action)
	{
		pdf_obj *js = pdf_dict_get(ctx, action, PDF_NAME(JS));
		if (js)
		{
			char *src = pdf_load_stream_or_string_as_utf8(ctx, js);
			fz_write_printf(ctx, out, "    %s: {\n", name);
			showtext(ctx, src, 1);
			fz_write_printf(ctx, out, "    }\n", name);
			fz_free(ctx, src);
		}
		else
		{
			fz_write_printf(ctx, out, "    %s: ", name);
			if (pdf_is_indirect(ctx, action))
				action = pdf_resolve_indirect(ctx, action);
			pdf_print_obj(ctx, out, action, 1, 1);
			fz_write_printf(ctx, out, "\n");
		}
	}
}

static void showfield(fz_context *ctx, pdf_obj *field, int recurse)
{
	pdf_obj *kids, *ft, *parent;
	const char *tu, *value;
	char *t;
	int ff;
	int i, n;

	t = pdf_load_field_name(ctx, field);
	tu = pdf_dict_get_text_string(ctx, field, PDF_NAME(TU));
	ft = pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT));
	ff = pdf_field_flags(ctx, field);
	parent = pdf_dict_get(ctx, field, PDF_NAME(Parent));

	fz_write_printf(ctx, out, "field %d\n", pdf_to_num(ctx, field));
	fz_write_printf(ctx, out, "    Type: %s\n", pdf_to_name(ctx, ft));
	if (ff)
	{
		fz_write_printf(ctx, out, "    Flags:");
		if (ff & PDF_FIELD_IS_READ_ONLY) fz_write_string(ctx, out, " readonly");
		if (ff & PDF_FIELD_IS_REQUIRED) fz_write_string(ctx, out, " required");
		if (ff & PDF_FIELD_IS_NO_EXPORT) fz_write_string(ctx, out, " noExport");
		if (ft == PDF_NAME(Btn))
		{
			if (ff & PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF) fz_write_string(ctx, out, " noToggleToOff");
			if (ff & PDF_BTN_FIELD_IS_RADIO) fz_write_string(ctx, out, " radio");
			if (ff & PDF_BTN_FIELD_IS_PUSHBUTTON) fz_write_string(ctx, out, " pushButton");
			if (ff & PDF_BTN_FIELD_IS_RADIOS_IN_UNISON) fz_write_string(ctx, out, " radiosInUnison");
		}
		if (ft == PDF_NAME(Tx))
		{
			if (ff & PDF_TX_FIELD_IS_MULTILINE) fz_write_string(ctx, out, " multiline");
			if (ff & PDF_TX_FIELD_IS_PASSWORD) fz_write_string(ctx, out, " password");
			if (ff & PDF_TX_FIELD_IS_FILE_SELECT) fz_write_string(ctx, out, " fileSelect");
			if (ff & PDF_TX_FIELD_IS_DO_NOT_SPELL_CHECK) fz_write_string(ctx, out, " dontSpellCheck");
			if (ff & PDF_TX_FIELD_IS_DO_NOT_SCROLL) fz_write_string(ctx, out, " dontScroll");
			if (ff & PDF_TX_FIELD_IS_COMB) fz_write_string(ctx, out, " comb");
			if (ff & PDF_TX_FIELD_IS_RICH_TEXT) fz_write_string(ctx, out, " richText");
		}
		if (ft == PDF_NAME(Ch))
		{
			if (ff & PDF_CH_FIELD_IS_COMBO) fz_write_string(ctx, out, " combo");
			if (ff & PDF_CH_FIELD_IS_EDIT) fz_write_string(ctx, out, " edit");
			if (ff & PDF_CH_FIELD_IS_SORT) fz_write_string(ctx, out, " sort");
			if (ff & PDF_CH_FIELD_IS_MULTI_SELECT) fz_write_string(ctx, out, " multiSelect");
			if (ff & PDF_CH_FIELD_IS_DO_NOT_SPELL_CHECK) fz_write_string(ctx, out, " dontSpellCheck");
			if (ff & PDF_CH_FIELD_IS_COMMIT_ON_SEL_CHANGE) fz_write_string(ctx, out, " commitOnSelChange");
		}
		fz_write_string(ctx, out, "\n");
	}
	fz_write_printf(ctx, out, "    Name: %(\n", t);
	fz_free(ctx, t);
	if (*tu)
		fz_write_printf(ctx, out, "    Label: %q\n", tu);
	if (parent)
		fz_write_printf(ctx, out, "    Parent: %d\n", pdf_to_num(ctx, parent));

	showaction(ctx, pdf_dict_getp(ctx, field, "A"), "Action");

	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/K"), "Keystroke");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/V"), "Validate");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/F"), "Format");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/C"), "Calculate");

	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/E"), "Enter");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/X"), "Exit");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/D"), "Down");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/U"), "Up");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/Fo"), "Focus");
	showaction(ctx, pdf_dict_getp_inheritable(ctx, field, "AA/Bl"), "Blur");

	value = pdf_field_value(ctx, field);
	if (value && value[0])
		fz_write_printf(ctx, out, "    Value: %q\n", value);

	fz_write_string(ctx, out, "\n");

	if (recurse)
	{
		kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
		n = pdf_array_len(ctx, kids);
		for (i = 0; i < n; ++i)
			showfield(ctx, pdf_array_get(ctx, kids, i), 1);
	}
}

static void showform(fz_context *ctx)
{
	pdf_obj *fields, *page, *obj;
	int i, k, n, m;

	fields = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");
	if (fields)
	{
		n = pdf_array_len(ctx, fields);
		for (i = 0; i < n; ++i)
			showfield(ctx, pdf_array_get(ctx, fields, i), 1);
	}
	else
	{
		fz_write_string(ctx, out, "% Root/AcroForm/Fields is missing!\n\n");
		m = pdf_count_pages(ctx, doc);
		for (k = 0; k < m; ++k)
		{
			page = pdf_lookup_page_obj(ctx, doc, k);
			fields = pdf_dict_get(ctx, page, PDF_NAME(Annots));
			n = pdf_array_len(ctx, fields);
			for (i = 0; i < n; ++i)
			{
				obj = pdf_array_get(ctx, fields, i);
				if (pdf_dict_get(ctx, obj, PDF_NAME(Subtype)) == PDF_NAME(Widget))
					showfield(ctx, obj, 0);
			}
		}
	}
}

#define SEP ".[]/"

static int isnumber(char *s)
{
	if (*s == '-')
		s++;
	while (*s)
	{
		if (*s < '0' || *s > '9')
			return 0;
		++s;
	}
	return 1;
}

static void showpath(fz_context *ctx, char *path, pdf_obj *obj)
{
	if (path && path[0])
	{
		char *part = fz_strsep(&path, SEP);
		if (part && part[0])
		{
			if (!strcmp(part, "*"))
			{
				int i, n;
				char buf[1000];
				if (pdf_is_array(ctx, obj))
				{
					n = pdf_array_len(ctx, obj);
					for (i = 0; i < n; ++i)
					{
						if (path)
						{
							fz_strlcpy(buf, path, sizeof buf);
							showpath(ctx, buf, pdf_array_get(ctx, obj, i));
						}
						else
							showpath(ctx, NULL, pdf_array_get(ctx, obj, i));
					}
				}
				else if (pdf_is_dict(ctx, obj))
				{
					n = pdf_dict_len(ctx, obj);
					for (i = 0; i < n; ++i)
					{
						if (path)
						{
							fz_strlcpy(buf, path, sizeof buf);
							showpath(ctx, buf, pdf_dict_get_val(ctx, obj, i));
						}
						else
							showpath(ctx, NULL, pdf_dict_get_val(ctx, obj, i));
					}
				}
				else
				{
					fz_write_string(ctx, out, "null\n");
				}
			}
			else if (isnumber(part) && pdf_is_array(ctx, obj))
			{
				int num = atoi(part);
				num = num < 0 ? pdf_array_len(ctx, obj) + num : num - 1;
				showpath(ctx, path, pdf_array_get(ctx, obj, num));
			}
			else
				showpath(ctx, path, pdf_dict_gets(ctx, obj, part));
		}
		else
			fz_write_string(ctx, out, "null\n");
	}
	else
	{
		if (pdf_is_indirect(ctx, obj))
			showobject(ctx, obj);
		else
		{
			pdf_print_obj(ctx, out, obj, do_tight, 0);
			fz_write_string(ctx, out, "\n");
		}
	}
}

static void showpathpage(fz_context *ctx, char *path)
{
	if (path)
	{
		char *part = fz_strsep(&path, SEP);
		if (part && part[0])
		{
			if (!strcmp(part, "*"))
			{
				int i, n;
				char buf[1000];
				n = pdf_count_pages(ctx, doc);
				for (i = 0; i < n; ++i)
				{
					if (path)
					{
						fz_strlcpy(buf, path, sizeof buf);
						showpath(ctx, buf, pdf_lookup_page_obj(ctx, doc, i));
					}
					else
						showpath(ctx, NULL, pdf_lookup_page_obj(ctx, doc, i));
				}
			}
			else if (isnumber(part))
			{
				int num = atoi(part);
				num = num < 0 ? pdf_count_pages(ctx, doc) + num : num - 1;
				showpath(ctx, path, pdf_lookup_page_obj(ctx, doc, num));
			}
			else
				fz_write_string(ctx, out, "null\n");
		}
		else
			fz_write_string(ctx, out, "null\n");
	}
	else
	{
		showpages(ctx);
	}
}

static void showpathroot(fz_context *ctx, char *path)
{
	char buf[2000], *list = buf, *part;
	fz_strlcpy(buf, path, sizeof buf);
	part = fz_strsep(&list, SEP);
	if (part && part[0])
	{
		if (!strcmp(part, "trailer"))
			showpath(ctx, list, pdf_trailer(ctx, doc));
		else if (!strcmp(part, "pages"))
			showpathpage(ctx, list);
		else if (isnumber(part))
		{
			pdf_obj *obj;
			int num = atoi(part);
			num = num < 0 ? pdf_xref_len(ctx, doc) + num : num;
			obj = pdf_new_indirect(ctx, doc, num, 0);
			fz_try(ctx)
				showpath(ctx, list, obj);
			fz_always(ctx)
				pdf_drop_obj(ctx, obj);
			fz_catch(ctx)
				;
		}
		else
			showpath(ctx, list, pdf_dict_gets(ctx, pdf_trailer(ctx, doc), part));
	}
	else
		fz_write_string(ctx, out, "null\n");
}

static void show(fz_context *ctx, char *sel)
{
	if (!strcmp(sel, "trailer"))
		showtrailer(ctx);
	else if (!strcmp(sel, "xref"))
		showxref(ctx);
	else if (!strcmp(sel, "pages"))
		showpages(ctx);
	else if (!strcmp(sel, "grep"))
		showgrep(ctx);
	else if (!strcmp(sel, "outline"))
		showoutline(ctx);
	else if (!strcmp(sel, "js"))
		showjs(ctx);
	else if (!strcmp(sel, "form"))
		showform(ctx);
	else
		showpathroot(ctx, sel);
}

int pdfshow_main(int argc, char **argv)
{
	char *password = NULL; /* don't throw errors if encrypted */
	char *filename = NULL;
	char *output = NULL;
	int c;
	int errored = 0;

	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	while ((c = fz_getopt(argc, argv, "p:o:begrL")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'o': output = fz_optpath(fz_optarg); break;
		case 'b': showbinary = 1; break;
		case 'e': showdecode = 0; break;
		case 'g': do_tight = 1; break;
		case 'r': do_repair = 1; break;
		case 'L': do_label = 1; break;
		default: return usage();
		}
	}

	if (fz_optind == argc)
		return usage();

	filename = argv[fz_optind++];

	if (output)
		out = fz_new_output_with_path(ctx, output, 0);
	else
		out = fz_stdout(ctx);

	fz_var(doc);
	fz_var(labels);
	fz_try(ctx)
	{
		doc = pdf_open_document(ctx, filename);
		if (pdf_needs_password(ctx, doc))
			if (!pdf_authenticate_password(ctx, doc, password))
				fz_warn(ctx, "cannot authenticate password: %s", filename);

		if (do_repair)
		{
			fz_try(ctx)
			{
				pdf_repair_xref(ctx, doc);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
				fz_rethrow_if(ctx, FZ_ERROR_REPAIRED);
				fz_report_error(ctx);
			}
		}

		if (do_label)
			labels = pdf_load_object_labels(ctx, doc);

		if (fz_optind == argc)
			showtrailer(ctx);

		while (fz_optind < argc)
			show(ctx, argv[fz_optind++]);
	}
	fz_always(ctx)
	{
		fz_close_output(ctx, out);
		fz_drop_output(ctx, out);
		pdf_drop_object_labels(ctx, labels);
		pdf_drop_document(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		errored = 1;
	}

	fz_drop_context(ctx);
	return errored;
}
