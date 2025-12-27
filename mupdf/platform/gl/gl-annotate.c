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

#include "gl-app.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>

static int is_draw_mode = 0;
static int new_contents = 0;

static char save_filename[PATH_MAX];
static pdf_write_options save_opts;
static struct input opwinput;
static struct input upwinput;
static int do_high_security;
static int hs_resolution = 200;
static struct input ocr_language_input;
static int ocr_language_input_initialised = 0;
static pdf_document *pdf_has_redactions_doc = NULL;
static int pdf_has_redactions;
static int do_snapshot;

static int label_slider(const char *label, int *value, int min, int max)
{
	int changed;
	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	ui_layout(R, NONE, CENTER, 0, 0);
	changed = ui_slider(value, min, max, 100);
	ui_layout(L, X, CENTER, 0, 0);
	ui_label("%s: %d", label, *value);
	ui_panel_end();
	return changed;
}

static int label_select(const char *label, const char *id, const char *current, const char *options[], int n)
{
	int changed;
	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	ui_layout(L, NONE, CENTER, 0, 0);
	ui_label("%s: ", label);
	ui_layout(T, X, CENTER, 0, 0);
	changed = ui_select(id, current, options, n);
	ui_panel_end();
	return changed;
}

static int pdf_filter(const char *fn)
{
	const char *extension = strrchr(fn, '.');
	if (extension && !fz_strcasecmp(extension, ".pdf"))
		return 1;
	return 0;
}

static void init_save_pdf_options(void)
{
	save_opts = pdf_default_write_options;
	if (pdf->redacted)
		save_opts.do_garbage = 1;
	if (pdf_can_be_saved_incrementally(ctx, pdf))
		save_opts.do_incremental = 1;
	save_opts.do_compress = 1;
	save_opts.do_compress_images = 1;
	save_opts.do_compress_fonts = 1;
	do_high_security = 0;
	ui_input_init(&opwinput, "");
	ui_input_init(&upwinput, "");
	if (!ocr_language_input_initialised)
	{
		ui_input_init(&ocr_language_input, "eng");
		ocr_language_input_initialised = 1;
	}
}

static const char *cryptalgo_names[] = {
	"Keep",
	"None",
	"RC4, 40 bit",
	"RC4, 128 bit",
	"AES, 128 bit",
	"AES, 256 bit",
};

static void save_pdf_options(void)
{
	const char *cryptalgo = cryptalgo_names[save_opts.do_encrypt];
	int choice;
	int can_be_incremental;

	ui_layout(T, X, NW, ui.padsize, ui.padsize);
	ui_label("PDF write options:");
	ui_layout(T, X, NW, ui.padsize*2, ui.padsize);

	can_be_incremental = pdf_can_be_saved_incrementally(ctx, pdf);

	ui_checkbox("Snapshot", &do_snapshot);
	if (do_snapshot)
		return; /* ignore normal PDF options */

	ui_checkbox("High Security", &do_high_security);
	if (do_high_security)
	{
		int res200 = (hs_resolution == 200);
		int res300 = (hs_resolution == 300);
		int res600 = (hs_resolution == 600);
		int res1200 = (hs_resolution == 1200);
		ui_label("WARNING: High Security saving is a lossy procedure! Keep your original file safe.");
		ui_label("Resolution:");
		ui_checkbox("200", &res200);
		ui_checkbox("300", &res300);
		ui_checkbox("600", &res600);
		ui_checkbox("1200", &res1200);
		if (res200 && hs_resolution != 200)
			hs_resolution = 200;
		else if (res300 && hs_resolution != 300)
			hs_resolution = 300;
		else if (res600 && hs_resolution != 600)
			hs_resolution = 600;
		else if (res1200 && hs_resolution != 1200)
			hs_resolution = 1200;
		else if (!res200 && !res300 && !res600 && !res1200)
			hs_resolution = 200;
		ui_label("OCR Language:");
		ui_input(&ocr_language_input, 32, 1);

		return; /* ignore normal PDF options */
	}

	ui_checkbox_aux("Incremental", &save_opts.do_incremental, !can_be_incremental);

	fz_try(ctx)
	{
		if (pdf_count_signatures(ctx, pdf) && !save_opts.do_incremental)
		{
			if (can_be_incremental)
				ui_label("WARNING: Saving non-incrementally will break existing signatures");
			else
				ui_label("WARNING: Saving will break existing signatures");
		}
	}
	fz_catch(ctx)
	{
		/* Ignore the error. */
	}

	ui_spacer();
	ui_checkbox("Pretty-print", &save_opts.do_pretty);
	ui_checkbox("Ascii", &save_opts.do_ascii);
	ui_checkbox("Decompress", &save_opts.do_decompress);
	ui_checkbox("Compress", &save_opts.do_compress);
	ui_checkbox("Compress images", &save_opts.do_compress_images);
	ui_checkbox("Compress fonts", &save_opts.do_compress_fonts);
	ui_checkbox("Object streams", &save_opts.do_use_objstms);
	ui_checkbox("Preserve metadata", &save_opts.do_preserve_metadata);

	if (save_opts.do_incremental)
	{
		save_opts.do_garbage = 0;
		save_opts.do_linear = 0;
		save_opts.do_clean = 0;
		save_opts.do_sanitize = 0;
		save_opts.do_encrypt = PDF_ENCRYPT_KEEP;
	}
	else
	{
		ui_spacer();
		ui_checkbox("Linearize", &save_opts.do_linear);
		ui_checkbox_aux("Garbage collect", &save_opts.do_garbage, pdf->redacted);
		ui_checkbox("Clean syntax", &save_opts.do_clean);
		ui_checkbox("Sanitize syntax", &save_opts.do_sanitize);

		ui_spacer();
		ui_label("Encryption:");
		choice = ui_select("Encryption", cryptalgo, cryptalgo_names, nelem(cryptalgo_names));
		if (choice != -1)
			save_opts.do_encrypt = choice;
	}

	if (save_opts.do_encrypt >= PDF_ENCRYPT_RC4_40)
	{
		ui_spacer();
		ui_label("User password:");
		if (ui_input(&upwinput, 32, 1) >= UI_INPUT_EDIT)
			fz_strlcpy(save_opts.upwd_utf8, upwinput.text, nelem(save_opts.upwd_utf8));
		ui_label("Owner password:");
		if (ui_input(&opwinput, 32, 1) >= UI_INPUT_EDIT)
			fz_strlcpy(save_opts.opwd_utf8, opwinput.text, nelem(save_opts.opwd_utf8));
	}
}

struct {
	int n;
	int i;
	char *operation_text;
	char *progress_text;
	int (*step)(int cancel);
	int display;
} ui_slow_operation_state;

static int run_slow_operation_step(int cancel)
{
	fz_try(ctx)
	{
		int i = ui_slow_operation_state.step(cancel);
		if (ui_slow_operation_state.i == 0)
		{
			ui_slow_operation_state.i = 1;
			ui_slow_operation_state.n = i;
		}
		else
		{
			ui_slow_operation_state.i = i;
		}
	}
	fz_catch(ctx)
	{
		ui_slow_operation_state.i = -1;
		ui_show_warning_dialog("%s failed: %s",
			ui_slow_operation_state.operation_text,
			fz_caught_message(ctx));
		fz_report_error(ctx);

		/* Call to cancel. */
		fz_try(ctx)
			ui_slow_operation_state.step(1);
		fz_catch(ctx)
		{
			/* Ignore any error from cancelling */
		}
		return 1;
	}

	return 0;
}

static void slow_operation_dialog(void)
{
	int start_time;
	int errored = 0;

	ui_dialog_begin(16 * ui.gridsize, 4 * ui.gridsize);
	ui_layout(T, X, NW, ui.padsize, ui.padsize);

	ui_label("%s", ui_slow_operation_state.operation_text);

	ui_spacer();

	if (ui_slow_operation_state.i == 0)
		ui_label("Initializing.");
	else if (ui_slow_operation_state.i > ui_slow_operation_state.n)
		ui_label("Finalizing.");
	else
		ui_label("%s: %d / %d",
			ui_slow_operation_state.progress_text,
			ui_slow_operation_state.i,
			ui_slow_operation_state.n);

	ui_spacer();

	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	{
		ui_layout(R, NONE, S, 0, 0);
		if (ui_button("Cancel"))
		{
			ui.dialog = NULL;
			run_slow_operation_step(1);
			return;
		}
	}
	ui_panel_end();

	/* Only run the operations every other time. This ensures we
	 * actually see the update for page i before page i is
	 * processed. */
	ui_slow_operation_state.display = !ui_slow_operation_state.display;
	if (ui_slow_operation_state.display == 0)
	{
		/* Run steps for 200ms or until we're done. */
		start_time = glutGet(GLUT_ELAPSED_TIME);
		while (!errored && ui_slow_operation_state.i >= 0 &&
			glutGet(GLUT_ELAPSED_TIME) < start_time + 200)
		{
			errored = run_slow_operation_step(0);
		}
	}

	if (!errored && ui_slow_operation_state.i == -1)
		ui.dialog = NULL;

	/* ... and trigger a redisplay */
	glutPostRedisplay();
}

static void
ui_start_slow_operation(char *operation, char *progress, int (*step)(int))
{
	ui.dialog = slow_operation_dialog;
	ui_slow_operation_state.operation_text = operation;
	ui_slow_operation_state.progress_text = progress;
	ui_slow_operation_state.i = 0;
	ui_slow_operation_state.step = step;
}

struct {
	int i;
	int n;
	fz_document_writer *writer;
} hss_state;

static int step_high_security_save(int cancel)
{
	fz_page *page = NULL;
	fz_device *dev;

	/* Called with i=0 for init. i=1...n for pages 1 to n inclusive.
	 * i=n+1 for finalisation. */

	/* If cancelling, tidy up state. */
	if (cancel)
	{
		fz_drop_document_writer(ctx, hss_state.writer);
		hss_state.writer = NULL;
		return -1;
	}

	/* If initing, open the file, count the pages, return the number
	 * of pages ("number of steps in the operation"). */
	if (hss_state.i == 0)
	{
		char options[1024];

		fz_snprintf(options, sizeof(options),
			"compression=flate,resolution=%d,ocr-language=%s",
			hs_resolution, ocr_language_input.text);

		hss_state.writer = fz_new_pdfocr_writer(ctx, save_filename, options);
		hss_state.i = 1;
		hss_state.n = fz_count_pages(ctx, (fz_document *)pdf);
		return hss_state.n;
	}
	/* If we've done all the pages, finish the write. */
	if (hss_state.i > hss_state.n)
	{
		fz_close_document_writer(ctx, hss_state.writer);
		fz_drop_document_writer(ctx, hss_state.writer);
		hss_state.writer = NULL;
		fz_strlcpy(filename, save_filename, PATH_MAX);
		reload_document();
		return -1;
	}
	/* Otherwise, do the next page. */
	page = fz_load_page(ctx, (fz_document *)pdf, hss_state.i-1);

	fz_try(ctx)
	{
		dev = fz_begin_page(ctx, hss_state.writer, fz_bound_page(ctx, page));
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_drop_page(ctx, page);
		page = NULL;
		fz_end_page(ctx, hss_state.writer);
	}
	fz_catch(ctx)
	{
		fz_drop_page(ctx, page);
		fz_rethrow(ctx);
	}

	return ++hss_state.i;
}

static void save_high_security(void)
{
	/* FIXME */
	trace_action("//doc.hsredact(%q);\n", save_filename);
	memset(&hss_state, 0, sizeof(hss_state));
	ui_start_slow_operation("High Security Save", "Page", step_high_security_save);
}

static void do_save_pdf_dialog(int for_signing)
{
	if (ui_save_file(save_filename, save_pdf_options,
			do_snapshot ?
			"Select where to save the snapshot:" :
			do_high_security ?
			"Select where to save the redacted document:" :
			for_signing ?
			"Select where to save the signed document:" :
			"Select where to save the document:"))
	{
		ui.dialog = NULL;
		if (save_filename[0] != 0)
		{
			if (do_high_security)
			{
				save_high_security();
				return;
			}
			if (for_signing && !do_sign())
				return;
			if (save_opts.do_garbage)
				save_opts.do_garbage = 2;
			fz_try(ctx)
			{
				static char opts_string[4096];
				pdf_format_write_options(ctx, opts_string, sizeof(opts_string), &save_opts);
				trace_action("doc.save(%q,%q);\n", save_filename, opts_string);
				if (do_snapshot)
				{
					pdf_save_snapshot(ctx, pdf, save_filename);
					fz_strlcat(save_filename, ".journal", PATH_MAX);
					pdf_save_journal(ctx, pdf, save_filename);
				}
				else
				{
					pdf_save_document(ctx, pdf, save_filename, &save_opts);
					fz_strlcpy(filename, save_filename, PATH_MAX);
					fz_strlcat(save_filename, ".journal", PATH_MAX);
#ifdef _WIN32
					fz_remove_utf8(save_filename);
#else
					remove(save_filename);
#endif
					reload_document();
				}
			}
			fz_catch(ctx)
			{
				ui_show_warning_dialog("%s", fz_caught_message(ctx));
				fz_report_error(ctx);
			}
		}
	}
}

static void save_pdf_dialog(void)
{
	do_save_pdf_dialog(0);
}

static void save_signed_pdf_dialog(void)
{
	do_save_pdf_dialog(1);
}

void do_save_signed_pdf_file(void)
{
	init_save_pdf_options();
	ui_init_save_file(filename, pdf_filter);
	ui.dialog = save_signed_pdf_dialog;
}

void do_save_pdf_file(void)
{
	if (pdf)
	{
		init_save_pdf_options();
		ui_init_save_file(filename, pdf_filter);
		ui.dialog = save_pdf_dialog;
	}
}

static char attach_filename[PATH_MAX];

static void save_attachment_dialog(void)
{
	if (ui_save_file(attach_filename, NULL, "Save attachment as:"))
	{
		ui.dialog = NULL;
		if (attach_filename[0] != 0)
		{
			fz_try(ctx)
			{
				pdf_obj *fs = pdf_annot_filespec(ctx, ui.selected_annot);
				fz_buffer *buf = pdf_load_embedded_file_contents(ctx, fs);
				fz_save_buffer(ctx, buf, attach_filename);
				fz_drop_buffer(ctx, buf);
				trace_action("tmp = annot.getFilespec()\n");
				trace_action("doc.getEmbeddedFileContents(tmp).save(\"%s\");\n", attach_filename);
				trace_action("tmp = doc.verifyEmbeddedFileChecksum(tmp);\n");
				trace_action("if (tmp != true)\n");
				trace_action("  throw new RegressionError('Embedded file checksum:', tmp, 'expected:', true);\n");
			}
			fz_catch(ctx)
			{
				ui_show_warning_dialog("%s", fz_caught_message(ctx));
				fz_report_error(ctx);
			}
		}
	}
}

static void open_attachment_dialog(void)
{
	if (ui_open_file(attach_filename, "Select file to attach:"))
	{
		ui.dialog = NULL;
		if (attach_filename[0] != 0)
		{
			pdf_obj *fs = NULL;
			pdf_begin_operation(ctx, pdf, "Embed file attachment");
			fz_var(fs);
			fz_try(ctx)
			{
				int64_t created, modified;
				fz_buffer *contents;
				const char *filename;

				filename = fz_basename(attach_filename);
				contents = fz_read_file(ctx, attach_filename);
				created = fz_stat_ctime(attach_filename);
				modified = fz_stat_mtime(attach_filename);

				fs = pdf_add_embedded_file(ctx, pdf, filename, NULL, contents,
					created, modified, 0);
				pdf_set_annot_filespec(ctx, ui.selected_annot, fs);
				fz_drop_buffer(ctx, contents);
				trace_action("annot.setFilespec(doc.addEmbeddedFile(\"%s\", null, readFile(\"%s\"), new Date(%d).getTime(), new Date(%d).getTime(), false));\n", filename, attach_filename, created, modified);
			}
			fz_always(ctx)
			{
				pdf_drop_obj(ctx, fs);
				pdf_end_operation(ctx, pdf);
			}
			fz_catch(ctx)
			{
				ui_show_warning_dialog("%s", fz_caught_message(ctx));
				fz_report_error(ctx);
			}
		}
	}
}

static char stamp_image_filename[PATH_MAX];

static void open_stamp_image_dialog(void)
{
	if (ui_open_file(stamp_image_filename, "Select file for customized stamp:"))
	{
		ui.dialog = NULL;
		if (stamp_image_filename[0] != 0)
		{
			fz_image *img = NULL;
			fz_var(img);
			fz_try(ctx)
			{
				fz_rect rect = pdf_annot_rect(ctx, ui.selected_annot);
				trace_action("tmp = new Image(%q);\n", stamp_image_filename);
				img = fz_new_image_from_file(ctx, stamp_image_filename);
				trace_action("annot.setAppearance(tmp);\n");
				pdf_set_annot_stamp_image(ctx, ui.selected_annot, img);
				pdf_set_annot_rect(ctx, ui.selected_annot, fz_make_rect(
					rect.x0, rect.y0,
					rect.x0 + img->w * 72 / img->xres,
					rect.y0 + img->h * 72 / img->yres)
				);
				trace_action("annot.setIcon(%q);\n", fz_basename(stamp_image_filename));
				pdf_set_annot_icon_name(ctx, ui.selected_annot, fz_basename(stamp_image_filename));
			}
			fz_always(ctx)
			{
				fz_drop_image(ctx, img);
			}
			fz_catch(ctx)
			{
				ui_show_warning_dialog("%s", fz_caught_message(ctx));
				fz_report_error(ctx);
			}
		}
	}
}

static int rects_differ(fz_rect a, fz_rect b, float threshold)
{
	if (fz_abs(a.x0 - b.x0) > threshold) return 1;
	if (fz_abs(a.y0 - b.y0) > threshold) return 1;
	if (fz_abs(a.x1 - b.x1) > threshold) return 1;
	if (fz_abs(a.y1 - b.y1) > threshold) return 1;
	return 0;
}

static int points_differ(fz_point a, fz_point b, float threshold)
{
	if (fz_abs(a.x - b.x) > threshold) return 1;
	if (fz_abs(a.y - b.y) > threshold) return 1;
	return 0;
}

static const char *getuser(void)
{
	const char *u;
	u = getenv("USER");
	if (!u) u = getenv("USERNAME");
	if (!u) u = "user";
	return u;
}

static void new_annot(int type)
{
	char msg[100];

	trace_action("annot = page.createAnnotation(%q);\n", pdf_string_from_annot_type(ctx, type));

	fz_snprintf(msg, sizeof msg, "Create %s Annotation", pdf_string_from_annot_type(ctx, type));
	pdf_begin_operation(ctx, pdf, msg);

	ui_select_annot(pdf_create_annot(ctx, page, type));

	pdf_set_annot_modification_date(ctx, ui.selected_annot, time(NULL));
	if (pdf_annot_has_author(ctx, ui.selected_annot))
		pdf_set_annot_author(ctx, ui.selected_annot, getuser());

	pdf_end_operation(ctx, pdf);

	switch (type)
	{
	case PDF_ANNOT_REDACT:
		pdf_has_redactions_doc = pdf;
		pdf_has_redactions = 1;
		/* fallthrough */
	case PDF_ANNOT_INK:
	case PDF_ANNOT_POLYGON:
	case PDF_ANNOT_POLY_LINE:
	case PDF_ANNOT_HIGHLIGHT:
	case PDF_ANNOT_UNDERLINE:
	case PDF_ANNOT_STRIKE_OUT:
	case PDF_ANNOT_SQUIGGLY:
		is_draw_mode = 1;
		break;
	}
}

static const char *color_names[] = {
	"None",
	"Aqua",
	"Black",
	"Blue",
	"Fuchsia",
	"Gray",
	"Green",
	"Lime",
	"Maroon",
	"Navy",
	"Olive",
	"Orange",
	"Purple",
	"Red",
	"Silver",
	"Teal",
	"White",
	"Yellow",
};

static unsigned int color_values[] = {
	0x00000000, /* transparent */
	0xff00ffff, /* aqua */
	0xff000000, /* black */
	0xff0000ff, /* blue */
	0xffff00ff, /* fuchsia */
	0xff808080, /* gray */
	0xff008000, /* green */
	0xff00ff00, /* lime */
	0xff800000, /* maroon */
	0xff000080, /* navy */
	0xff808000, /* olive */
	0xffffa500, /* orange */
	0xff800080, /* purple */
	0xffff0000, /* red */
	0xffc0c0c0, /* silver */
	0xff008080, /* teal */
	0xffffffff, /* white */
	0xffffff00, /* yellow */
};

static unsigned int hex_from_color(int n, float color[4])
{
	float rgb[4];
	int r, g, b;
	switch (n)
	{
	default:
		return 0x00000000;
	case 1:
		r = color[0] * 255;
		return 0xff000000 | (r<<16) | (r<<8) | r;
	case 3:
		r = color[0] * 255;
		g = color[1] * 255;
		b = color[2] * 255;
		return 0xff000000 | (r<<16) | (g<<8) | b;
	case 4:
		fz_convert_color(ctx, fz_device_cmyk(ctx), color, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);
		r = rgb[0] * 255;
		g = rgb[1] * 255;
		b = rgb[2] * 255;
		return 0xff000000 | (r<<16) | (g<<8) | b;
	}
}

static const char *name_from_hex(unsigned int hex)
{
	static char buf[10];
	int i;
	for (i = 0; i < (int)nelem(color_names); ++i)
		if (color_values[i] == hex)
			return color_names[i];
	fz_snprintf(buf, sizeof buf, "#%06x", hex & 0xffffff);
	return buf;
}

static void do_annotate_color(char *label,
		void (*get_color)(fz_context *ctx, pdf_annot *annot, int *n, float color[4]),
		void (*set_color)(fz_context *ctx, pdf_annot *annot, int n, const float color[4]))
{
	float color[4];
	int hex, choice, n;
	get_color(ctx, ui.selected_annot, &n, color);
	choice = label_select(label, label, name_from_hex(hex_from_color(n, color)), color_names, nelem(color_names));
	if (choice != -1)
	{
		hex = color_values[choice];
		if (hex == 0)
		{
			trace_action("annot.set%s([]);\n", label);
			set_color(ctx, ui.selected_annot, 0, color);
		}
		else
		{
			color[0] = ((hex>>16)&0xff) / 255.0f;
			color[1] = ((hex>>8)&0xff) / 255.0f;
			color[2] = ((hex)&0xff) / 255.0f;
			trace_action("annot.set%s([%g, %g, %g]);\n", label, color[0], color[1], color[2]);
			set_color(ctx, ui.selected_annot, 3, color);
		}
	}
}

static void do_annotate_author(void)
{
	if (pdf_annot_has_author(ctx, ui.selected_annot))
	{
		const char *author = pdf_annot_author(ctx, ui.selected_annot);
		if (strlen(author) > 0)
			ui_label("Author: %s", author);
	}
}

static void do_annotate_date(void)
{
	const char *s = format_date(pdf_annot_modification_date(ctx, ui.selected_annot));
	if (s)
		ui_label("Date: %s", s);
}

static void do_annotate_flags(void)
{
	char str[100];
	int n = 0;
	str[0] = 0;
	int f = pdf_annot_flags(ctx, ui.selected_annot);
	if (f & PDF_ANNOT_IS_INVISIBLE) ++n, fz_strlcat(str, "Invisible ", sizeof str);
	if (f & PDF_ANNOT_IS_HIDDEN) ++n, fz_strlcat(str, "Hidden ", sizeof str);
	if (f & PDF_ANNOT_IS_PRINT) ++n, fz_strlcat(str, "Print ", sizeof str);
	if (f & PDF_ANNOT_IS_NO_ZOOM) ++n, fz_strlcat(str, "NoZoom ", sizeof str);
	if (f & PDF_ANNOT_IS_NO_ROTATE) ++n, fz_strlcat(str, "NoRotate ", sizeof str);
	if (f & PDF_ANNOT_IS_NO_VIEW) ++n, fz_strlcat(str, "NoView ", sizeof str);
	if (f & PDF_ANNOT_IS_READ_ONLY) ++n, fz_strlcat(str, "ReadOnly ", sizeof str);
	if (f & PDF_ANNOT_IS_LOCKED) ++n, fz_strlcat(str, "Locked ", sizeof str);
	if (f & PDF_ANNOT_IS_TOGGLE_NO_VIEW) ++n, fz_strlcat(str, "ToggleNoView ", sizeof str);
	if (f & PDF_ANNOT_IS_LOCKED_CONTENTS) ++n, fz_strlcat(str, "LockedContents ", sizeof str);
	if (str[0])
	{
		if (n < 3)
		{
			ui_label("Flags: %s", str);
		}
		else
		{
			ui_label("Flags:");
			ui.layout->padx = 10;
			ui_label(str);
			ui.layout->padx = 0;
		}
	}
}

static const char *intent_names[] = {
	"Default", // all
	"FreeTextCallout", // freetext
	"FreeTextTypewriter", // freetext
	"LineArrow", // line
	"LineDimension", // line
	"PolyLineDimension", // polyline
	"PolygonCloud", // polygon
	"PolygonDimension", // polygon
	"StampImage", // stamp
	"StampSnapshot", // stamp
};

static enum pdf_intent do_annotate_intent(void)
{
	enum pdf_intent intent;
	const char *intent_name;
	int choice;

	if (!pdf_annot_has_intent(ctx, ui.selected_annot))
		return PDF_ANNOT_IT_DEFAULT;

	intent = pdf_annot_intent(ctx, ui.selected_annot);

	if (intent == PDF_ANNOT_IT_UNKNOWN)
		intent_name = "Unknown";
	else
		intent_name = intent_names[intent];

	choice = label_select("Intent", "IT", intent_name, intent_names, nelem(intent_names));
	if (choice != -1)
	{
		trace_action("annot.setIntent(%d);\n", choice);
		pdf_set_annot_intent(ctx, ui.selected_annot, choice);
		intent = choice;

		// Changed intent!
		if (intent == PDF_ANNOT_IT_FREETEXT_CALLOUT)
		{
			pdf_set_annot_callout_point(ctx, ui.selected_annot, fz_make_point(0, 0));
			pdf_set_annot_callout_style(ctx, ui.selected_annot, PDF_ANNOT_LE_OPEN_ARROW);
		}
	}

	// Press 'c' to move Callout line to current cursor position.
	if (intent == PDF_ANNOT_IT_FREETEXT_CALLOUT)
	{
		if (!ui.focus && ui.key && ui.plain)
		{
			if (ui.key == 'c')
			{
				fz_point p = fz_transform_point(fz_make_point(ui.x, ui.y), view_page_inv_ctm);
				pdf_set_annot_callout_point(ctx, ui.selected_annot, p);
			}
		}
	}

	return intent;
}

static int do_annotate_contents(void)
{
	static int is_same_edit_operation = 1;
	static pdf_annot *last_annot = NULL;
	static struct input input;
	const char *contents;

	if (ui.focus != &input)
		is_same_edit_operation = 0;

	if (ui.selected_annot != last_annot || new_contents)
	{
		is_same_edit_operation = 0;
		last_annot = ui.selected_annot;
		contents = pdf_annot_contents(ctx, ui.selected_annot);
		ui_input_init(&input, contents);
		new_contents = 0;
	}

	ui_label("Contents:");
	if (ui_input(&input, 0, 5) >= UI_INPUT_EDIT)
	{
		trace_action("annot.setContents(%q);\n", input.text);
		if (is_same_edit_operation)
		{
			pdf_begin_implicit_operation(ctx, pdf);
			pdf_set_annot_contents(ctx, ui.selected_annot, input.text);
			pdf_end_operation(ctx, pdf);
		}
		else
		{
			pdf_set_annot_contents(ctx, ui.selected_annot, input.text);
			is_same_edit_operation = 1;
		}
	}

	return input.text[0] != 0;
}

static const char *file_attachment_icons[] = { "Graph", "Paperclip", "PushPin", "Tag" };
static const char *sound_icons[] = { "Speaker", "Mic" };
static const char *stamp_icons[] = {
	"Approved", "AsIs", "Confidential", "Departmental", "Draft",
	"Experimental", "Expired", "Final", "ForComment", "ForPublicRelease",
	"NotApproved", "NotForPublicRelease", "Sold", "TopSecret" };
static const char *text_icons[] = {
	"Comment", "Help", "Insert", "Key", "NewParagraph", "Note", "Paragraph" };
static const char *line_ending_styles[] = {
	"None", "Square", "Circle", "Diamond", "OpenArrow", "ClosedArrow", "Butt",
	"ROpenArrow", "RClosedArrow", "Slash" };
static const char *quadding_names[] = { "Left", "Center", "Right" };
static const char *font_names[] = { "Cour", "Helv", "TiRo" };
static const char *lang_names[] = { "", "ja", "ko", "zh-Hans", "zh-Hant" };
static const char *im_redact_names[] = { "Keep images", "Remove images", "Erase pixels" };
static const char *la_redact_names[] = { "Keep line art", "Remove covered line art", "Remove touched line art" };
static const char *tx_redact_names[] = { "Remove text", "Keep text", "Remove invisible text" };
static const char *border_styles[] = { "Solid", "Dashed", "Dotted" };
static const char *border_intensities[] = { "None", "Small clouds", "Large clouds", "Enormous clouds" };

static int should_edit_color(enum pdf_annot_type subtype)
{
	switch (subtype) {
	default:
		return 0;
	case PDF_ANNOT_STAMP:
	case PDF_ANNOT_TEXT:
	case PDF_ANNOT_FILE_ATTACHMENT:
	case PDF_ANNOT_SOUND:
	case PDF_ANNOT_CARET:
		return 1;
	case PDF_ANNOT_FREE_TEXT:
		return 1;
	case PDF_ANNOT_INK:
	case PDF_ANNOT_LINE:
	case PDF_ANNOT_SQUARE:
	case PDF_ANNOT_CIRCLE:
	case PDF_ANNOT_POLYGON:
	case PDF_ANNOT_POLY_LINE:
		return 1;
	case PDF_ANNOT_HIGHLIGHT:
	case PDF_ANNOT_UNDERLINE:
	case PDF_ANNOT_STRIKE_OUT:
	case PDF_ANNOT_SQUIGGLY:
		return 1;
	}
}

static int should_edit_icolor(enum pdf_annot_type subtype)
{
	switch (subtype) {
	default:
		return 0;
	case PDF_ANNOT_LINE:
	case PDF_ANNOT_POLYGON:
	case PDF_ANNOT_POLY_LINE:
	case PDF_ANNOT_SQUARE:
	case PDF_ANNOT_CIRCLE:
		return 1;
	}
}

struct {
	int n;
	int i;
	pdf_redact_options opts;
} rap_state;

static int
step_redact_all_pages(int cancel)
{
	pdf_page *pg;

	/* Called with i=0 for init. i=1...n for pages 1 to n inclusive.
	 * i=n+1 for finalisation. */

	if (cancel)
		return -1;

	if (rap_state.i == 0)
	{
		rap_state.i = 1;
		rap_state.n = pdf_count_pages(ctx, pdf);
		return rap_state.n;
	}
	else if (rap_state.i > rap_state.n)
	{
		trace_action("page = tmp;\n");
		trace_page_update();
		pdf_has_redactions = 0;
		load_page();
		return -1;
	}

	trace_action("page = doc.loadPage(%d);\n", rap_state.i-1);
	trace_action("page.applyRedactions(%s, %d);\n",
		rap_state.opts.black_boxes ? "true" : "false",
		rap_state.opts.image_method);
	pg = pdf_load_page(ctx, pdf, rap_state.i-1);
	fz_try(ctx)
		pdf_redact_page(ctx, pdf, pg, &rap_state.opts);
	fz_always(ctx)
		fz_drop_page(ctx, (fz_page *)pg);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ++rap_state.i;
}

static void redact_all_pages(pdf_redact_options *opts)
{
	trace_action("tmp = page;\n");
	memset(&rap_state, 0, sizeof(rap_state));
	rap_state.opts = *opts;
	ui_start_slow_operation("Redacting all pages in document.", "Page", step_redact_all_pages);
}

static int
document_has_redactions(void)
{
	int i, n;
	pdf_page *page = NULL;
	pdf_annot *annot;
	int has_redact = 0;

	fz_var(page);
	fz_var(has_redact);

	fz_try(ctx)
	{
		n = pdf_count_pages(ctx, pdf);
		for (i = 0; i < n && !has_redact; i++)
		{
			page = pdf_load_page(ctx, pdf, i);
			for (annot = pdf_first_annot(ctx, page);
				annot != NULL;
				annot = pdf_next_annot(ctx, annot))
			{
				if (pdf_annot_type(ctx, annot) == PDF_ANNOT_REDACT)
				{
					has_redact = 1;
					break;
				}
			}
			fz_drop_page(ctx, (fz_page *)page);
			page = NULL;
		}
	}
	fz_catch(ctx)
	{
		/* Ignore the error, and assume no redactions */
		fz_drop_page(ctx, (fz_page *)page);
	}
	return has_redact;
}

static int detect_border_style(enum pdf_border_style style, float width)
{
	if (style == PDF_BORDER_STYLE_DASHED)
	{
		int count = pdf_annot_border_dash_count(ctx, ui.selected_annot);
		float dashlen = pdf_annot_border_dash_item(ctx, ui.selected_annot, 0);
		if ((count == 1 || count == 2) && dashlen < 2 * width)
			return 2;
		return 1;
	}
	return 0;
}

static void do_border(void)
{
	static int width;
	static int choice;
	enum pdf_border_style style;

	ui_spacer();

	width = pdf_annot_border_width(ctx, ui.selected_annot);
	style = pdf_annot_border_style(ctx, ui.selected_annot);

	width = fz_clampi(width, 0, 12);
	if (label_slider("Border", &width, 0, 12))
	{
		pdf_set_annot_border_width(ctx, ui.selected_annot, width);
		trace_action("annot.setBorderWidth(%d);\n", width);
	}

	width = fz_max(width, 1);

	choice = detect_border_style(style, width);
	choice = label_select("Style", "BorderStyle", border_styles[choice], border_styles, nelem(border_styles));
	if (choice != -1)
	{
		pdf_clear_annot_border_dash(ctx, ui.selected_annot);
		trace_action("annot.clearBorderDash();\n");
		if (choice == 0)
		{
			pdf_set_annot_border_style(ctx, ui.selected_annot, PDF_BORDER_STYLE_SOLID);
			trace_action("annot.setBorderType('Solid');\n");
		}
		else if (choice == 1)
		{
			pdf_set_annot_border_style(ctx, ui.selected_annot, PDF_BORDER_STYLE_DASHED);
			pdf_add_annot_border_dash_item(ctx, ui.selected_annot, 3.0f * width);
			trace_action("annot.setBorderType('Dashed');\n");
			trace_action("annot.addBorderDashItem(%g);\n", 3.0f * width);
		}
		else if (choice == 2)
		{
			pdf_set_annot_border_style(ctx, ui.selected_annot, PDF_BORDER_STYLE_DASHED);
			pdf_add_annot_border_dash_item(ctx, ui.selected_annot, 1.0f * width);
			trace_action("annot.setBorderType('Dashed');\n");
			trace_action("annot.addBorderDashItem(%g);\n", 1.0f * width);
		}
	}

	if (pdf_annot_has_border_effect(ctx, ui.selected_annot))
	{
		static int intensity;
		intensity = fz_clampi(pdf_annot_border_effect_intensity(ctx, ui.selected_annot), 0, 3);
		if (pdf_annot_border_effect(ctx, ui.selected_annot) == PDF_BORDER_EFFECT_NONE)
			intensity = 0;

		intensity = label_select("Effect", "BorderEffect", border_intensities[intensity], border_intensities, nelem(border_intensities));
		if (intensity != -1)
		{
			enum pdf_border_effect effect = intensity ? PDF_BORDER_EFFECT_CLOUDY : PDF_BORDER_EFFECT_NONE;
			pdf_set_annot_border_effect(ctx, ui.selected_annot, effect);
			pdf_set_annot_border_effect_intensity(ctx, ui.selected_annot, intensity);
			trace_action("annot.setBorderEffect('%s');\n", effect ? "Cloudy" : "None");
			trace_action("annot.setBorderEffectIntensity(%d);\n", intensity);
		}
	}
}

static int image_file_filter(const char *fn)
{
	return !!fz_strstrcase(fn, ".jpg") || !!fz_strstrcase(fn, ".jpeg") || !!fz_strstrcase(fn, ".png");
}

static void resize_icon_annot(void)
{
	fz_rect rect = pdf_annot_rect(ctx, ui.selected_annot);
	rect = fz_make_rect(rect.x0, rect.y0, rect.x0 + 16, rect.y0 + 16);
	pdf_set_annot_rect(ctx, ui.selected_annot, rect);
}

void do_annotate_panel(void)
{
	static struct list annot_list;
	enum pdf_annot_type subtype;
	enum pdf_intent intent;
	pdf_annot *annot;
	int idx;
	int n;

	ui_layout(T, X, NW, ui.padsize, ui.padsize);

	if (ui_popup("CreateAnnotPopup", "Create...", 1, 16))
	{
		if (ui_popup_item("Text")) new_annot(PDF_ANNOT_TEXT);
		if (ui_popup_item("FreeText")) new_annot(PDF_ANNOT_FREE_TEXT);
		if (ui_popup_item("Stamp")) new_annot(PDF_ANNOT_STAMP);
		if (ui_popup_item("Caret")) new_annot(PDF_ANNOT_CARET);
		if (ui_popup_item("Ink")) new_annot(PDF_ANNOT_INK);
		if (ui_popup_item("Square")) new_annot(PDF_ANNOT_SQUARE);
		if (ui_popup_item("Circle")) new_annot(PDF_ANNOT_CIRCLE);
		if (ui_popup_item("Line")) new_annot(PDF_ANNOT_LINE);
		if (ui_popup_item("Polygon")) new_annot(PDF_ANNOT_POLYGON);
		if (ui_popup_item("PolyLine")) new_annot(PDF_ANNOT_POLY_LINE);
		if (ui_popup_item("Highlight")) new_annot(PDF_ANNOT_HIGHLIGHT);
		if (ui_popup_item("Underline")) new_annot(PDF_ANNOT_UNDERLINE);
		if (ui_popup_item("StrikeOut")) new_annot(PDF_ANNOT_STRIKE_OUT);
		if (ui_popup_item("Squiggly")) new_annot(PDF_ANNOT_SQUIGGLY);
		if (ui_popup_item("FileAttachment")) new_annot(PDF_ANNOT_FILE_ATTACHMENT);
		if (ui_popup_item("Redact")) new_annot(PDF_ANNOT_REDACT);
		ui_popup_end();
	}

	n = 0;
	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		++n;

	ui_list_begin(&annot_list, n, 0, ui.lineheight * 6 + 4);
	for (idx=0, annot = pdf_first_annot(ctx, page); annot; ++idx, annot = pdf_next_annot(ctx, annot))
	{
		char buf[256];
		int num = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
		subtype = pdf_annot_type(ctx, annot);
		fz_snprintf(buf, sizeof buf, "%d: %s", num, pdf_string_from_annot_type(ctx, subtype));
		if (ui_list_item(&annot_list, pdf_annot_obj(ctx, annot), buf, ui.selected_annot == annot))
		{
			trace_action("annot = page.getAnnotations()[%d];\n", idx);
			ui_select_annot(pdf_keep_annot(ctx, annot));
		}
	}
	ui_list_end(&annot_list);

	if (ui.selected_annot && (subtype = pdf_annot_type(ctx, ui.selected_annot)) != PDF_ANNOT_WIDGET)
	{
		int n, choice, has_content;
		pdf_obj *obj;

		/* common annotation properties */

		ui_spacer();

		do_annotate_author();
		do_annotate_date();
		do_annotate_flags();

		obj = pdf_dict_get(ctx, pdf_annot_obj(ctx, ui.selected_annot), PDF_NAME(Popup));
		if (obj)
			ui_label("Popup: %d 0 R", pdf_to_num(ctx, obj));

		has_content = do_annotate_contents();

		intent = do_annotate_intent();
		if (subtype == PDF_ANNOT_FREE_TEXT && intent == PDF_ANNOT_IT_FREETEXT_CALLOUT)
		{
			enum pdf_line_ending s;
			int s_choice;

			s = pdf_annot_callout_style(ctx, ui.selected_annot);

			s_choice = label_select("Callout", "CL", line_ending_styles[s], line_ending_styles, nelem(line_ending_styles));
			if (s_choice != -1)
			{
				s = s_choice;
				trace_action("annot.setCalloutStyle(%q);\n", line_ending_styles[s]);
				pdf_set_annot_callout_style(ctx, ui.selected_annot, s);
			}
		}

		if (subtype == PDF_ANNOT_FREE_TEXT)
		{
			int lang_choice, font_choice, color_choice, size_changed;
			int q;
			const char *text_lang;
			const char *text_font;
			char text_font_buf[20];
			char lang_buf[8];
			static float text_size_f, text_color[4];
			static int text_size;

			ui_spacer();

			text_lang = fz_string_from_text_language(lang_buf, pdf_annot_language(ctx, ui.selected_annot));
			lang_choice = label_select("Language", "DA/Lang", text_lang, lang_names, nelem(lang_names));
			if (lang_choice != -1)
			{
				text_lang = lang_names[lang_choice];
				trace_action("annot.setLanguage(%q);\n", text_lang);
				pdf_set_annot_language(ctx, ui.selected_annot, fz_text_language_from_string(text_lang));
			}

			q = pdf_annot_quadding(ctx, ui.selected_annot);
			choice = label_select("Text Align", "Q", quadding_names[q], quadding_names, nelem(quadding_names));
			if (choice != -1)
			{
				trace_action("annot.setQuadding(%d);\n", choice);
				pdf_set_annot_quadding(ctx, ui.selected_annot, choice);
			}

			pdf_annot_default_appearance_unmapped(ctx, ui.selected_annot, text_font_buf, sizeof text_font_buf, &text_size_f, &n, text_color);
			text_size = text_size_f;
			font_choice = label_select("Text Font", "DA/Font", text_font_buf, font_names, nelem(font_names));
			size_changed = label_slider("Text Size", &text_size, 8, 36);
			color_choice = label_select("Text Color", "DA/Color", name_from_hex(hex_from_color(n, text_color)), color_names+1, nelem(color_names)-1);
			if (font_choice != -1 || color_choice != -1 || size_changed)
			{
				if (font_choice != -1)
					text_font = font_names[font_choice];
				else
					text_font = text_font_buf;
				if (color_choice != -1)
				{
					n = 3;
					text_color[0] = ((color_values[color_choice+1]>>16) & 0xff) / 255.0f;
					text_color[1] = ((color_values[color_choice+1]>>8) & 0xff) / 255.0f;
					text_color[2] = ((color_values[color_choice+1]) & 0xff) / 255.0f;

					if (text_color[0] == text_color[1] && text_color[1] == text_color[2])
						n = 1;
				}
				if (n == 1)
					trace_action("annot.setDefaultAppearance(%q, %d, [%g]);\n",
						text_font, text_size, text_color[0]);
				else if (n == 3)
					trace_action("annot.setDefaultAppearance(%q, %d, [%g, %g, %g]);\n",
						text_font, text_size, text_color[0], text_color[1], text_color[2]);
				else if (n == 4)
					trace_action("annot.setDefaultAppearance(%q, %d, [%g, %g, %g, %g]);\n",
						text_font, text_size, text_color[0], text_color[1], text_color[2], text_color[3]);
				else
					trace_action("annot.setDefaultAppearance(%q, %d, []);\n",
						text_font, text_size);
				pdf_set_annot_default_appearance(ctx, ui.selected_annot, text_font, text_size, n, text_color);
			}
		}

		if (subtype == PDF_ANNOT_LINE || subtype == PDF_ANNOT_POLY_LINE)
		{
			enum pdf_line_ending s, e;
			int s_choice, e_choice;

			ui_spacer();

			pdf_annot_line_ending_styles(ctx, ui.selected_annot, &s, &e);

			s_choice = label_select("Line End 1", "LE0", line_ending_styles[s], line_ending_styles, nelem(line_ending_styles));
			e_choice = label_select("Line End 2", "LE1", line_ending_styles[e], line_ending_styles, nelem(line_ending_styles));

			if (s_choice != -1 || e_choice != -1)
			{
				if (s_choice != -1) s = s_choice;
				if (e_choice != -1) e = e_choice;
				trace_action("annot.setLineEndingStyles(%q, %q);\n", line_ending_styles[s], line_ending_styles[e]);
				pdf_set_annot_line_ending_styles(ctx, ui.selected_annot, s, e);
			}
		}

		if (subtype == PDF_ANNOT_LINE)
		{
			static int ll, lle, llo;
			static int cap;

			ll = pdf_annot_line_leader(ctx, ui.selected_annot);
			if (label_slider("Leader", &ll, -20, 20))
			{
				pdf_set_annot_line_leader(ctx, ui.selected_annot, ll);
				trace_action("annot.setLineLeader(%d);\n", ll);
			}

			if (ll)
			{
				lle = pdf_annot_line_leader_extension(ctx, ui.selected_annot);
				if (label_slider("  LLE", &lle, 0, 20))
				{
					pdf_set_annot_line_leader_extension(ctx, ui.selected_annot, lle);
					trace_action("annot.setLineLeaderExtension(%d);\n", ll);
				}

				llo = pdf_annot_line_leader_offset(ctx, ui.selected_annot);
				if (label_slider("  LLO", &llo, 0, 20))
				{
					pdf_set_annot_line_leader_offset(ctx, ui.selected_annot, llo);
					trace_action("annot.setLineLeaderOffset(%d);\n", ll);
				}
			}

			if (has_content)
			{
				cap = pdf_annot_line_caption(ctx, ui.selected_annot);
				if (ui_checkbox("Caption", &cap))
				{
					pdf_set_annot_line_caption(ctx, ui.selected_annot, cap);
					trace_action("annot.setLineCaption(%s);\n", cap ? "true" : "false");
				}
			}
		}

		if (pdf_annot_has_icon_name(ctx, ui.selected_annot))
		{
			const char *name = pdf_annot_icon_name(ctx, ui.selected_annot);

			switch (pdf_annot_type(ctx, ui.selected_annot))
			{
			default:
				break;
			case PDF_ANNOT_TEXT:
				ui_spacer();
				choice = label_select("Icon", "Icon", name, text_icons, nelem(text_icons));
				if (choice != -1)
				{
					trace_action("annot.setIcon(%q);\n", text_icons[choice]);
					pdf_set_annot_icon_name(ctx, ui.selected_annot, text_icons[choice]);
					resize_icon_annot();
				}
				break;
			case PDF_ANNOT_FILE_ATTACHMENT:
				ui_spacer();
				choice = label_select("Icon", "Icon", name, file_attachment_icons, nelem(file_attachment_icons));
				if (choice != -1)
				{
					trace_action("annot.setIcon(%q);\n", file_attachment_icons[choice]);
					pdf_set_annot_icon_name(ctx, ui.selected_annot, file_attachment_icons[choice]);
					resize_icon_annot();
				}
				break;
			case PDF_ANNOT_SOUND:
				ui_spacer();
				choice = label_select("Icon", "Icon", name, sound_icons, nelem(sound_icons));
				if (choice != -1)
				{
					trace_action("annot.setIcon(%q);\n", sound_icons[choice]);
					pdf_set_annot_icon_name(ctx, ui.selected_annot, sound_icons[choice]);
					resize_icon_annot();
				}
				break;
			case PDF_ANNOT_STAMP:
				ui_spacer();
				choice = label_select("Icon", "Icon", name, stamp_icons, nelem(stamp_icons));
				if (choice != -1)
				{
					trace_action("annot.setIcon(%q);\n", stamp_icons[choice]);
					pdf_set_annot_icon_name(ctx, ui.selected_annot, stamp_icons[choice]);
					resize_icon_annot();
				}
				break;
			}
		}

		if (pdf_annot_has_border(ctx, ui.selected_annot))
			do_border();

		ui_spacer();

		if (should_edit_color(subtype))
			do_annotate_color("Color", pdf_annot_color, pdf_set_annot_color);
		if (should_edit_icolor(subtype))
			do_annotate_color("InteriorColor", pdf_annot_interior_color, pdf_set_annot_interior_color);

		{
			static int opacity;
			opacity = pdf_annot_opacity(ctx, ui.selected_annot) * 100;
			if (label_slider("Opacity", &opacity, 0, 100))
			{
				trace_action("annot.setOpacity(%g);\n", opacity / 100.0f);
				pdf_set_annot_opacity(ctx, ui.selected_annot, opacity / 100.0f);
			}
		}

		if (pdf_annot_has_quad_points(ctx, ui.selected_annot))
		{
			ui_spacer();
			if (is_draw_mode)
			{
				n = pdf_annot_quad_point_count(ctx, ui.selected_annot);
				ui_label("QuadPoints: %d", n);
				if (ui_button("Clear"))
				{
					trace_action("annot.clearQuadPoints();\n");
					pdf_clear_annot_quad_points(ctx, ui.selected_annot);
				}
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (pdf_annot_has_vertices(ctx, ui.selected_annot))
		{
			ui_spacer();
			if (is_draw_mode)
			{
				n = pdf_annot_vertex_count(ctx, ui.selected_annot);
				ui_label("Vertices: %d", n);
				if (ui_button("Clear"))
				{
					trace_action("annot.clearVertices();\n");
					pdf_clear_annot_vertices(ctx, ui.selected_annot);
				}
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (pdf_annot_has_ink_list(ctx, ui.selected_annot))
		{
			ui_spacer();
			if (is_draw_mode)
			{
				n = pdf_annot_ink_list_count(ctx, ui.selected_annot);
				ui_label("InkList: %d strokes", n);
				if (ui_button("Clear"))
				{
					trace_action("annot.clearInkList();\n");
					pdf_clear_annot_ink_list(ctx, ui.selected_annot);
				}
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (pdf_annot_type(ctx, ui.selected_annot) == PDF_ANNOT_STAMP)
		{
			char attname[PATH_MAX];
			ui_spacer();
			if (ui_button("Image..."))
			{
				fz_dirname(attname, filename, sizeof attname);
				ui_init_open_file(attname, image_file_filter);
				ui.dialog = open_stamp_image_dialog;
			}
		}

		if (pdf_annot_type(ctx, ui.selected_annot) == PDF_ANNOT_FILE_ATTACHMENT)
		{
			pdf_filespec_params params;
			char attname[PATH_MAX];
			pdf_obj *fs = pdf_annot_filespec(ctx, ui.selected_annot);
			ui_spacer();
			if (pdf_is_embedded_file(ctx, fs))
			{
				if (ui_button("Save..."))
				{
					fz_dirname(attname, filename, sizeof attname);
					fz_strlcat(attname, "/", sizeof attname);
					pdf_get_filespec_params(ctx, fs, &params);
					fz_strlcat(attname, params.filename, sizeof attname);
					ui_init_save_file(attname, NULL);
					ui.dialog = save_attachment_dialog;
				}
			}
			if (ui_button("Embed..."))
			{
				fz_dirname(attname, filename, sizeof attname);
				ui_init_open_file(attname, NULL);
				ui.dialog = open_attachment_dialog;
			}
		}

		ui_spacer();
		if (ui_button("Delete"))
		{
			trace_action("page.deleteAnnotation(annot);\n");
			pdf_delete_annot(ctx, page, ui.selected_annot);
			page_annots_changed = 1;
			ui_select_annot(NULL);
			return;
		}
	}

	ui_layout(B, X, NW, ui.padsize, ui.padsize);

	if (ui_button("Save PDF..."))
		do_save_pdf_file();
}

static void new_redaction(pdf_page *page, fz_quad q)
{
	pdf_annot *annot;

	pdf_begin_operation(ctx, pdf, "Create Redaction");

	annot = pdf_create_annot(ctx, page, PDF_ANNOT_REDACT);

	fz_try(ctx)
	{
		pdf_set_annot_modification_date(ctx, annot, time(NULL));
		if (pdf_annot_has_author(ctx, annot))
			pdf_set_annot_author(ctx, annot, getuser());
		pdf_add_annot_quad_point(ctx, annot, q);
		pdf_set_annot_contents(ctx, annot, search_needle);

		trace_action("annot = page.createAnnotation(%q);\n", "Redact");
		trace_action("annot.addQuadPoint([%g, %g, %g, %g, %g, %g, %g, %g]);\n",
			q.ul.x, q.ul.y,
			q.ur.x, q.ur.y,
			q.ll.x, q.ll.y,
			q.lr.x, q.lr.y);
		trace_action("annot.setContents(%q);\n", search_needle);
	}
	fz_always(ctx)
		pdf_drop_annot(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	pdf_has_redactions_doc = pdf;
	pdf_has_redactions = 1;

	pdf_end_operation(ctx, pdf);
}

static struct { int i, n; } rds_state;

static int mark_search_step(int cancel)
{
	fz_quad quads[500];
	int i, count;

	if (rds_state.i == 0)
		return ++rds_state.i, rds_state.n;

	if (cancel || rds_state.i > rds_state.n)
	{
		trace_action("page = tmp;\n");
		load_page();
		return -1;
	}

	count = fz_search_page_number(ctx, (fz_document*)pdf, rds_state.i-1, search_needle, NULL, quads, nelem(quads));
	if (count > 0)
	{
		pdf_page *page = pdf_load_page(ctx, pdf, rds_state.i-1);
		trace_action("page = doc.loadPage(%d);\n", rds_state.i-1);
		for (i = 0; i < count; i++)
			new_redaction(page, quads[i]);
		fz_drop_page(ctx, (fz_page*)page);
	}

	return ++rds_state.i;
}

void mark_all_search_results(void)
{
	rds_state.i = 0;
	rds_state.n = pdf_count_pages(ctx, pdf);
	trace_action("tmp = page;\n");
	ui_start_slow_operation("Marking all search results for redaction.", "Page", mark_search_step);
}

void do_redact_panel(void)
{
	static struct list annot_list;
	enum pdf_annot_type subtype;
	pdf_annot *annot;
	int idx;
	int im_choice;
	int la_choice;
	int tx_choice;
	int i;

	int num_redact = 0;
	static pdf_redact_options redact_opts = { 1, PDF_REDACT_IMAGE_PIXELS, PDF_REDACT_LINE_ART_REMOVE_IF_TOUCHED };
	int search_valid;

	if (pdf_has_redactions_doc != pdf)
	{
		pdf_has_redactions_doc = pdf;
		pdf_has_redactions = document_has_redactions();
	}

	num_redact = 0;
	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		if (pdf_annot_type(ctx, annot) == PDF_ANNOT_REDACT)
			++num_redact;

	ui_layout(T, X, NW, ui.padsize, ui.padsize);

	if (ui_button("Add Redaction"))
		new_annot(PDF_ANNOT_REDACT);

	search_valid = search_has_results();
	if (ui_button_aux("Mark search in page", !search_valid))
	{
		for (i = 0; i < search_hit_count; i++)
			new_redaction(page, search_hit_quads[i]);
		search_hit_count = 0;
		ui_select_annot(NULL);
	}
	if (ui_button_aux("Mark search in document", search_needle == NULL))
	{
		mark_all_search_results();
		search_hit_count = 0;
		ui_select_annot(NULL);
	}

	ui_spacer();

	ui_label("When Redacting:");
	ui_checkbox("Draw black boxes", &redact_opts.black_boxes);
	im_choice = ui_select("Redact/IM", im_redact_names[redact_opts.image_method], im_redact_names, nelem(im_redact_names));
	if (im_choice != -1)
		redact_opts.image_method = im_choice;

	la_choice = ui_select("Redact/LA", la_redact_names[redact_opts.line_art], la_redact_names, nelem(la_redact_names));
	if (la_choice != -1)
		redact_opts.line_art = la_choice;

	tx_choice = ui_select("Redact/TX", tx_redact_names[redact_opts.text], tx_redact_names, nelem(tx_redact_names));
	if (tx_choice != -1)
		redact_opts.text = tx_choice;

	ui_spacer();

	if (ui_button_aux("Redact Page", num_redact == 0))
	{
		ui_select_annot(NULL);
		trace_action("page.applyRedactions(%s, %d, %d);\n",
			redact_opts.black_boxes ? "true" : "false",
			redact_opts.image_method,
			redact_opts.line_art);
		pdf_redact_page(ctx, pdf, page, &redact_opts);
		trace_page_update();
		load_page();
	}

	if (ui_button_aux("Redact Document", !pdf_has_redactions))
	{
		ui_select_annot(NULL);
		redact_all_pages(&redact_opts);
	}

	ui_spacer();

	ui_list_begin(&annot_list, num_redact, 0, ui.lineheight * 6 + 4);
	for (idx=0, annot = pdf_first_annot(ctx, page); annot; ++idx, annot = pdf_next_annot(ctx, annot))
	{
		char buf[50];
		int num = pdf_to_num(ctx, pdf_annot_obj(ctx, annot));
		subtype = pdf_annot_type(ctx, annot);
		if (subtype == PDF_ANNOT_REDACT)
		{
			const char *contents = pdf_annot_contents(ctx, annot);
			fz_snprintf(buf, sizeof buf, "%d: %s", num, contents[0] ? contents : "Redact");
			if (ui_list_item(&annot_list, pdf_annot_obj(ctx, annot), buf, ui.selected_annot == annot))
			{
				trace_action("annot = page.getAnnotations()[%d];\n", idx);
				ui_select_annot(pdf_keep_annot(ctx, annot));
			}
		}
	}
	ui_list_end(&annot_list);

	ui_spacer();

	if (ui.selected_annot && (subtype = pdf_annot_type(ctx, ui.selected_annot)) == PDF_ANNOT_REDACT)
	{
		int n;

		do_annotate_author();
		do_annotate_date();

		ui_spacer();

		if (is_draw_mode)
		{
			n = pdf_annot_quad_point_count(ctx, ui.selected_annot);
			ui_label("QuadPoints: %d", n);
			if (ui_button("Clear"))
			{
				trace_action("annot.clearQuadPoints();\n");
				pdf_clear_annot_quad_points(ctx, ui.selected_annot);
			}
			if (ui_button("Done"))
				is_draw_mode = 0;
		}
		else
		{
			if (ui_button("Edit"))
				is_draw_mode = 1;
		}

		ui_spacer();

		if (ui_button("Delete"))
		{
			trace_action("page.deleteAnnotation(annot);\n");
			pdf_delete_annot(ctx, page, ui.selected_annot);
			page_annots_changed = 1;
			ui_select_annot(NULL);
			return;
		}
	}

	ui_layout(B, X, NW, ui.padsize, ui.padsize);

	if (ui_button("Save PDF..."))
		do_save_pdf_file();
}

static void do_edit_icon(fz_irect canvas_area, fz_irect area, fz_rect *rect)
{
	static fz_point start_pt;
	static float w, h;
	static int moving = 0;

	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			start_pt.x = rect->x0;
			start_pt.y = rect->y0;
			w = rect->x1 - rect->x0;
			h = rect->y1 - rect->y0;
			moving = 1;
		}
	}

	if (ui.active == ui.selected_annot && moving)
	{
		rect->x0 = start_pt.x + (ui.x - ui.down_x);
		rect->y0 = start_pt.y + (ui.y - ui.down_y);

		/* Clamp to fit on page */
		rect->x0 = fz_clamp(rect->x0, view_page_area.x0, view_page_area.x1-w);
		rect->y0 = fz_clamp(rect->y0, view_page_area.y0, view_page_area.y1-h);
		rect->x1 = rect->x0 + w;
		rect->y1 = rect->y0 + h;

		/* cancel on right click */
		if (ui.right)
			moving = 0;

		/* Commit movement on mouse-up */
		if (!ui.down)
		{
			fz_point dp = { rect->x0 - start_pt.x, rect->y0 - start_pt.y };
			moving = 0;
			if (fz_abs(dp.x) > 0.1f || fz_abs(dp.y) > 0.1f)
			{
				fz_rect trect = pdf_annot_rect(ctx, ui.selected_annot);
				dp = fz_transform_vector(dp, view_page_inv_ctm);
				trect.x0 += dp.x; trect.x1 += dp.x;
				trect.y0 += dp.y; trect.y1 += dp.y;
				trace_action("annot.setRect([%g, %g, %g, %g]);\n", trect.x0, trect.y0, trect.x1, trect.y1);
				pdf_set_annot_rect(ctx, ui.selected_annot, trect);
			}
		}
	}
}

static void do_edit_rect(fz_irect canvas_area, fz_irect area, fz_rect *rect, int lock_aspect)
{
	enum {
		ER_N=1, ER_E=2, ER_S=4, ER_W=8,
		ER_NONE = 0,
		ER_NW = ER_N|ER_W,
		ER_NE = ER_N|ER_E,
		ER_SW = ER_S|ER_W,
		ER_SE = ER_S|ER_E,
		ER_MOVE = ER_N|ER_E|ER_S|ER_W,
	};
	static fz_rect start_rect;
	static int state = ER_NONE;

	area = fz_expand_irect(area, 5);
	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			start_rect = *rect;
			state = ER_NONE;
			if (ui.x <= area.x0 + 10) state |= ER_W;
			if (ui.x >= area.x1 - 10) state |= ER_E;
			if (ui.y <= area.y0 + 10) state |= ER_N;
			if (ui.y >= area.y1 - 10) state |= ER_S;
			if (!state) state = ER_MOVE;
		}
	}

	if (ui.active == ui.selected_annot && state != ER_NONE)
	{
		*rect = start_rect;
		if (state & ER_W) rect->x0 += (ui.x - ui.down_x);
		if (state & ER_E) rect->x1 += (ui.x - ui.down_x);
		if (state & ER_N) rect->y0 += (ui.y - ui.down_y);
		if (state & ER_S) rect->y1 += (ui.y - ui.down_y);
		if (rect->x1 < rect->x0) { float t = rect->x1; rect->x1 = rect->x0; rect->x0 = t; }
		if (rect->y1 < rect->y0) { float t = rect->y1; rect->y1 = rect->y0; rect->y0 = t; }
		if (rect->x1 < rect->x0 + 10) rect->x1 = rect->x0 + 10;
		if (rect->y1 < rect->y0 + 10) rect->y1 = rect->y0 + 10;

		if (lock_aspect)
		{
			float aspect = (start_rect.x1 - start_rect.x0) / (start_rect.y1 - start_rect.y0);
			switch (state)
			{
			case ER_SW:
			case ER_NW:
				rect->x0 = rect->x1 - (rect->y1 - rect->y0) * aspect;
				break;
			case ER_NE:
			case ER_SE:
			case ER_N:
			case ER_S:
				rect->x1 = rect->x0 + (rect->y1 - rect->y0) * aspect;
				break;
			case ER_E:
			case ER_W:
				rect->y1 = rect->y0 + (rect->x1 - rect->x0) / aspect;
				break;
			}
		}

		/* cancel on right click */
		if (ui.right)
			state = ER_NONE;

		/* commit on mouse-up */
		if (!ui.down)
		{
			state = ER_NONE;
			if (rects_differ(start_rect, *rect, 1))
			{
				fz_rect trect = fz_transform_rect(*rect, view_page_inv_ctm);
				trace_action("annot.setRect([%g, %g, %g, %g]);\n", trect.x0, trect.y0, trect.x1, trect.y1);
				pdf_set_annot_rect(ctx, ui.selected_annot, trect);
			}
		}
	}
}

static void do_edit_line(fz_irect canvas_area, fz_irect area, fz_rect *rect)
{
	enum { EL_NONE, EL_A=1, EL_B=2, EL_MOVE=EL_A|EL_B };
	static fz_point start_a, start_b;
	static int state = EL_NONE;
	fz_irect a_grab, b_grab;
	fz_point a, b;
	float lw;

	area = fz_expand_irect(area, 5);
	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			pdf_annot_line(ctx, ui.selected_annot, &start_a, &start_b);
			start_a = fz_transform_point(start_a, view_page_ctm);
			start_b = fz_transform_point(start_b, view_page_ctm);
			a_grab = fz_make_irect(start_a.x, start_a.y, start_a.x, start_a.y);
			b_grab = fz_make_irect(start_b.x, start_b.y, start_b.x, start_b.y);
			a_grab = fz_expand_irect(a_grab, 10);
			b_grab = fz_expand_irect(b_grab, 10);
			state = EL_NONE;
			if (ui_mouse_inside(a_grab)) state |= EL_A;
			if (ui_mouse_inside(b_grab)) state |= EL_B;
			if (!state) state = EL_MOVE;
		}
	}

	if (ui.active == ui.selected_annot && state != 0)
	{
		a = start_a;
		b = start_b;
		if (state & EL_A) { a.x += (ui.x - ui.down_x); a.y += (ui.y - ui.down_y); }
		if (state & EL_B) { b.x += (ui.x - ui.down_x); b.y += (ui.y - ui.down_y); }

		glBegin(GL_LINES);
		glColor4f(1, 0, 0, 1);
		glVertex2f(a.x, a.y);
		glVertex2f(b.x, b.y);
		glEnd();

		rect->x0 = fz_min(a.x, b.x);
		rect->y0 = fz_min(a.y, b.y);
		rect->x1 = fz_max(a.x, b.x);
		rect->y1 = fz_max(a.y, b.y);
		lw = pdf_annot_border_width(ctx, ui.selected_annot);
		*rect = fz_expand_rect(*rect, fz_matrix_expansion(view_page_ctm) * lw);

		/* cancel on right click */
		if (ui.right)
			state = EL_NONE;

		/* commit on mouse-up */
		if (!ui.down)
		{
			state = EL_NONE;
			if (points_differ(start_a, a, 1) || points_differ(start_b, b, 1))
			{
				a = fz_transform_point(a, view_page_inv_ctm);
				b = fz_transform_point(b, view_page_inv_ctm);
				trace_action("annot.setLine([%g, %g], [%g, %g]);\n", a.x, a.y, b.x, b.y);
				pdf_set_annot_line(ctx, ui.selected_annot, a, b);
			}
		}
	}
}

static void do_edit_polygon(fz_irect canvas_area, int close)
{
	static int drawing = 0;
	fz_point a, p;

	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(view_page_area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active || ui.active == ui.selected_annot)
			ui.cursor = GLUT_CURSOR_CROSSHAIR;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			drawing = 1;
		}
	}

	if (ui.active == ui.selected_annot && drawing)
	{
		int n = pdf_annot_vertex_count(ctx, ui.selected_annot);
		if (n > 0)
		{
			p = pdf_annot_vertex(ctx, ui.selected_annot, n-1);
			p = fz_transform_point(p, view_page_ctm);
			if (close)
			{
				a = pdf_annot_vertex(ctx, ui.selected_annot, 0);
				a = fz_transform_point(a, view_page_ctm);
			}
			glBegin(GL_LINE_STRIP);
			glColor4f(1, 0, 0, 1);
			glVertex2f(p.x, p.y);
			glVertex2f(ui.x, ui.y);
			if (close)
				glVertex2f(a.x, a.y);
			glEnd();
		}

		glColor4f(1, 0, 0, 1);
		glPointSize(4);
		glBegin(GL_POINTS);
		glVertex2f(ui.x, ui.y);
		glEnd();

		/* cancel on right click */
		if (ui.right)
			drawing = 0;

		/* commit point on mouse-up */
		if (!ui.down)
		{
			fz_point p = fz_transform_point_xy(ui.x, ui.y, view_page_inv_ctm);
			trace_action("annot.addVertex(%g, %g);\n", p.x, p.y);
			pdf_add_annot_vertex(ctx, ui.selected_annot, p);
			drawing = 0;
		}
	}
}

static void do_edit_ink(fz_irect canvas_area)
{
	static int drawing = 0;
	static fz_point p[1000];
	static int n, last_x, last_y;
	int i;

	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(view_page_area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active || ui.active == ui.selected_annot)
			ui.cursor = GLUT_CURSOR_CROSSHAIR;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			drawing = 1;
			n = 0;
			last_x = INT_MIN;
			last_y = INT_MIN;
		}
	}

	if (ui.active == ui.selected_annot && drawing)
	{
		if (n < (int)nelem(p) && (ui.x != last_x || ui.y != last_y))
		{
			p[n].x = fz_clamp(ui.x, view_page_area.x0, view_page_area.x1);
			p[n].y = fz_clamp(ui.y, view_page_area.y0, view_page_area.y1);
			++n;
		}
		last_x = ui.x;
		last_y = ui.y;

		if (n > 1)
		{
			glBegin(GL_LINE_STRIP);
			glColor4f(1, 0, 0, 1);
			for (i = 0; i < n; ++i)
				glVertex2f(p[i].x, p[i].y);
			glEnd();
		}

		/* cancel on right click */
		if (ui.right)
		{
			drawing = 0;
			n = 0;
		}

		/* commit stroke on mouse-up */
		if (!ui.down)
		{
			if (n > 1)
			{
				trace_action("annot.addInkList([");
				for (i = 0; i < n; ++i)
				{
					p[i] = fz_transform_point(p[i], view_page_inv_ctm);
					trace_action("%s[%g, %g]", (i > 0 ? ", " : ""), p[i].x, p[i].y);
				}
				trace_action("]);\n");
				pdf_add_annot_ink_list(ctx, ui.selected_annot, n, p);
			}
			drawing = 0;
			n = 0;
		}
	}
}

static void do_edit_quad_points(void)
{
	static fz_point pt = { 0, 0 };
	static int marking = 0;
	static fz_quad hits[1000];
	fz_rect rect;
	char *text;
	int i, n;

	if (ui_mouse_inside(view_page_area))
	{
		ui.hot = ui.selected_annot;
		if (!ui.active || ui.active == ui.selected_annot)
			ui.cursor = GLUT_CURSOR_TEXT;
		if (!ui.active && ui.down)
		{
			ui.active = ui.selected_annot;
			marking = 1;
			pt.x = ui.x;
			pt.y = ui.y;
		}
	}

	if (ui.active == ui.selected_annot && marking)
	{
		fz_point page_a = { pt.x, pt.y };
		fz_point page_b = { ui.x, ui.y };

		page_a = fz_transform_point(page_a, view_page_inv_ctm);
		page_b = fz_transform_point(page_b, view_page_inv_ctm);

		if (ui.mod == GLUT_ACTIVE_CTRL)
			fz_snap_selection(ctx, page_text, &page_a, &page_b, FZ_SELECT_WORDS);
		else if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
			fz_snap_selection(ctx, page_text, &page_a, &page_b, FZ_SELECT_LINES);

		if (ui.mod == GLUT_ACTIVE_SHIFT)
		{
			rect = fz_make_rect(
					fz_min(page_a.x, page_b.x),
					fz_min(page_a.y, page_b.y),
					fz_max(page_a.x, page_b.x),
					fz_max(page_a.y, page_b.y));
			n = 1;
			hits[0] = fz_quad_from_rect(rect);
		}
		else
		{
			n = fz_highlight_selection(ctx, page_text, page_a, page_b, hits, nelem(hits));
		}

		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); /* invert destination color */
		glEnable(GL_BLEND);

		glColor4f(1, 1, 1, 1);
		glBegin(GL_QUADS);
		for (i = 0; i < n; ++i)
		{
			fz_quad thit = fz_transform_quad(hits[i], view_page_ctm);
			glVertex2f(thit.ul.x, thit.ul.y);
			glVertex2f(thit.ur.x, thit.ur.y);
			glVertex2f(thit.lr.x, thit.lr.y);
			glVertex2f(thit.ll.x, thit.ll.y);
		}
		glEnd();

		glDisable(GL_BLEND);

		/* cancel on right click */
		if (ui.right)
			marking = 0;

		if (!ui.down)
		{
			if (n > 0)
			{
				pdf_begin_operation(ctx, pdf, "Edit quad points");

				trace_action("annot.clearQuadPoints();\n");
				pdf_clear_annot_quad_points(ctx, ui.selected_annot);
				for (i = 0; i < n; ++i)
				{
					trace_action("annot.addQuadPoint([%g, %g, %g, %g, %g, %g, %g, %g]);\n",
						hits[i].ul.x, hits[i].ul.y,
						hits[i].ur.x, hits[i].ur.y,
						hits[i].ll.x, hits[i].ll.y,
						hits[i].lr.x, hits[i].lr.y);
					pdf_add_annot_quad_point(ctx, ui.selected_annot, hits[i]);
				}

				if (ui.mod == GLUT_ACTIVE_SHIFT)
					text = fz_copy_rectangle(ctx, page_text, rect, 0);
				else
					text = fz_copy_selection(ctx, page_text, page_a, page_b, 0);

				trace_action("annot.setContents(%q);\n", text);
				pdf_set_annot_contents(ctx, ui.selected_annot, text);
				new_contents = 1;
				fz_free(ctx, text);

				pdf_end_operation(ctx, pdf);
			}
			marking = 0;
		}
	}
}

void do_annotate_canvas(fz_irect canvas_area)
{
	fz_rect bounds;
	fz_irect area;
	pdf_annot *annot;
	const void *nothing = ui.hot;
	int idx;

	for (idx=0, annot = pdf_first_annot(ctx, page); annot; ++idx, annot = pdf_next_annot(ctx, annot))
	{
		enum pdf_annot_type subtype = pdf_annot_type(ctx, annot);

		if (pdf_annot_has_rect(ctx, annot))
			bounds = pdf_annot_rect(ctx, annot);
		else
			bounds = pdf_bound_annot(ctx, annot);

		bounds = fz_transform_rect(bounds, view_page_ctm);
		area = fz_irect_from_rect(bounds);

		if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
		{
			pdf_set_annot_hot(ctx, annot, 1);

			ui.hot = annot;
			if (!ui.active && ui.down)
			{
				if (ui.selected_annot != annot)
				{
					trace_action("annot = page.getAnnotations()[%d];\n", idx);
					if (!ui.selected_annot && !showannotate)
						toggle_annotate(ANNOTATE_MODE_NORMAL);
					ui.active = annot;
					ui_select_annot(pdf_keep_annot(ctx, annot));
				}
			}
		}
		else
		{
			pdf_set_annot_hot(ctx, annot, 0);
		}

		if (annot == ui.selected_annot)
		{
			switch (subtype)
			{
			default:
				break;

			/* Popup window */
			case PDF_ANNOT_POPUP:
				do_edit_rect(canvas_area, area, &bounds, 0);
				break;

			/* Icons */
			case PDF_ANNOT_TEXT:
			case PDF_ANNOT_CARET:
			case PDF_ANNOT_FILE_ATTACHMENT:
			case PDF_ANNOT_SOUND:
				do_edit_icon(canvas_area, area, &bounds);
				break;

			case PDF_ANNOT_STAMP:
				do_edit_rect(canvas_area, area, &bounds, 1);
				break;

			case PDF_ANNOT_FREE_TEXT:
				do_edit_rect(canvas_area, area, &bounds, 0);
				break;

			/* Drawings */
			case PDF_ANNOT_LINE:
				do_edit_line(canvas_area, area, &bounds);
				break;
			case PDF_ANNOT_CIRCLE:
			case PDF_ANNOT_SQUARE:
				do_edit_rect(canvas_area, area, &bounds, 0);
				break;
			case PDF_ANNOT_POLYGON:
				if (is_draw_mode)
					do_edit_polygon(canvas_area, 1);
				break;
			case PDF_ANNOT_POLY_LINE:
				if (is_draw_mode)
					do_edit_polygon(canvas_area, 0);
				break;

			case PDF_ANNOT_INK:
				if (is_draw_mode)
					do_edit_ink(canvas_area);
				break;

			case PDF_ANNOT_HIGHLIGHT:
			case PDF_ANNOT_UNDERLINE:
			case PDF_ANNOT_STRIKE_OUT:
			case PDF_ANNOT_SQUIGGLY:
			case PDF_ANNOT_REDACT:
				if (is_draw_mode)
					do_edit_quad_points();
				break;
			}

			glLineStipple(1, 0xAAAA);
			glEnable(GL_LINE_STIPPLE);
			glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			glEnable(GL_BLEND);
			glColor4f(1, 1, 1, 1);
			glBegin(GL_LINE_LOOP);
			area = fz_irect_from_rect(bounds);
			glVertex2f(area.x0-0.5f, area.y0-0.5f);
			glVertex2f(area.x1+0.5f, area.y0-0.5f);
			glVertex2f(area.x1+0.5f, area.y1+0.5f);
			glVertex2f(area.x0-0.5f, area.y1+0.5f);
			glEnd();
			glDisable(GL_BLEND);
			glDisable(GL_LINE_STIPPLE);
		}
	}

	if (ui_mouse_inside(canvas_area) && ui.down)
	{
		if (!ui.active && ui.hot == nothing)
			ui_select_annot(NULL);
	}

	if (ui.right)
		is_draw_mode = 0;
}
