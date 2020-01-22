#include "gl-app.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

static int is_draw_mode = 0;

static char save_filename[PATH_MAX];
static pdf_write_options save_opts;
static struct input opwinput;
static struct input upwinput;

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
	save_opts.do_compress = 1;
	save_opts.do_compress_images = 1;
	save_opts.do_compress_fonts = 1;
	save_opts.do_incremental = 1;
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

	ui_layout(T, X, NW, 2, 2);
	ui_label("PDF write options:");
	ui_layout(T, X, NW, 4, 2);

	ui_checkbox("Incremental", &save_opts.do_incremental);
	fz_try(ctx)
	{
		if (pdf_count_signatures(ctx, pdf))
		{
			ui_label("WARNING: Saving non-incrementally will break existing signatures");
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
		ui_checkbox("Garbage collect", &save_opts.do_garbage);
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

static void do_save_pdf_dialog(int for_signing)
{
	ui_input_init(&opwinput, "");
	ui_input_init(&upwinput, "");

	if (ui_save_file(save_filename, save_pdf_options, for_signing ? "Select where to save the signed document:" : "Select where to save the document:"))
	{
		ui.dialog = NULL;
		if (save_filename[0] != 0)
		{
			if (for_signing && !do_sign())
				return;
			if (save_opts.do_garbage)
				save_opts.do_garbage = 2;
			fz_try(ctx)
			{
				pdf_save_document(ctx, pdf, save_filename, &save_opts);
				fz_strlcpy(filename, save_filename, PATH_MAX);
				reload();
			}
			fz_catch(ctx)
			{
				ui_show_warning_dialog("%s", fz_caught_message(ctx));
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
	selected_annot = pdf_create_annot(ctx, page, type);

	pdf_set_annot_modification_date(ctx, selected_annot, time(NULL));
	if (pdf_annot_has_author(ctx, selected_annot))
		pdf_set_annot_author(ctx, selected_annot, getuser());

	pdf_update_appearance(ctx, selected_annot);

	switch (type)
	{
	case PDF_ANNOT_INK:
	case PDF_ANNOT_POLYGON:
	case PDF_ANNOT_POLY_LINE:
	case PDF_ANNOT_HIGHLIGHT:
	case PDF_ANNOT_UNDERLINE:
	case PDF_ANNOT_STRIKE_OUT:
	case PDF_ANNOT_SQUIGGLY:
	case PDF_ANNOT_REDACT:
		is_draw_mode = 1;
		break;
	}

	render_page();
}

static void do_annotate_flags(void)
{
	char buf[4096];
	int f = pdf_annot_flags(ctx, selected_annot);
	fz_strlcpy(buf, "Flags:", sizeof buf);
	if (f & PDF_ANNOT_IS_INVISIBLE) fz_strlcat(buf, " inv", sizeof buf);
	if (f & PDF_ANNOT_IS_HIDDEN) fz_strlcat(buf, " hidden", sizeof buf);
	if (f & PDF_ANNOT_IS_PRINT) fz_strlcat(buf, " print", sizeof buf);
	if (f & PDF_ANNOT_IS_NO_ZOOM) fz_strlcat(buf, " nz", sizeof buf);
	if (f & PDF_ANNOT_IS_NO_ROTATE) fz_strlcat(buf, " nr", sizeof buf);
	if (f & PDF_ANNOT_IS_NO_VIEW) fz_strlcat(buf, " nv", sizeof buf);
	if (f & PDF_ANNOT_IS_READ_ONLY) fz_strlcat(buf, " ro", sizeof buf);
	if (f & PDF_ANNOT_IS_LOCKED) fz_strlcat(buf, " lock", sizeof buf);
	if (f & PDF_ANNOT_IS_TOGGLE_NO_VIEW) fz_strlcat(buf, " tnv", sizeof buf);
	if (f & PDF_ANNOT_IS_LOCKED_CONTENTS) fz_strlcat(buf, " lc", sizeof buf);
	if (!f) fz_strlcat(buf, " none", sizeof buf);
	ui_label("%s", buf);
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
		return 0;
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
	get_color(ctx, selected_annot, &n, color);
	ui_label("%s:", label);
	choice = ui_select(label, name_from_hex(hex_from_color(n, color)), color_names, nelem(color_names));
	if (choice != -1)
	{
		hex = color_values[choice];
		if (hex == 0)
			set_color(ctx, selected_annot, 0, color);
		else
		{
			color[0] = ((hex>>16)&0xff) / 255.0f;
			color[1] = ((hex>>8)&0xff) / 255.0f;
			color[2] = ((hex)&0xff) / 255.0f;
			set_color(ctx, selected_annot, 3, color);
		}
	}
}

static void do_annotate_author(void)
{
	if (pdf_annot_has_author(ctx, selected_annot))
	{
		const char *author = pdf_annot_author(ctx, selected_annot);
		if (strlen(author) > 0)
			ui_label("Author: %s", author);
	}
}

static void do_annotate_date(void)
{
	time_t secs = pdf_annot_modification_date(ctx, selected_annot);
	if (secs > 0)
	{
#ifdef _POSIX_SOURCE
		struct tm tmbuf, *tm = gmtime_r(&secs, &tmbuf);
#else
		struct tm *tm = gmtime(&secs);
#endif
		char buf[100];
		if (tm)
		{
			strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", tm);
			ui_label("Date: %s", buf);
		}
	}
}

static void do_annotate_contents(void)
{
	static pdf_annot *last_annot = NULL;
	static struct input input;
	const char *contents;

	if (selected_annot != last_annot)
	{
		last_annot = selected_annot;
		contents = pdf_annot_contents(ctx, selected_annot);
		ui_input_init(&input, contents);
	}

	ui_label("Contents:");
	if (ui_input(&input, 0, 5) >= UI_INPUT_EDIT)
		pdf_set_annot_contents(ctx, selected_annot, input.text);
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
static const char *font_names[] = { "Cour", "Helv", "TiRo", "Symb", "ZaDb", };

static int should_edit_border(enum pdf_annot_type subtype)
{
	switch (subtype) {
	default:
		return 0;
	case PDF_ANNOT_FREE_TEXT:
		return 1;
	case PDF_ANNOT_INK:
	case PDF_ANNOT_LINE:
	case PDF_ANNOT_SQUARE:
	case PDF_ANNOT_CIRCLE:
	case PDF_ANNOT_POLYGON:
	case PDF_ANNOT_POLY_LINE:
		return 1;
	}
}

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
	case PDF_ANNOT_SQUARE:
	case PDF_ANNOT_CIRCLE:
		return 1;
	}
}

void do_annotate_panel(void)
{
	static struct list annot_list;
	enum pdf_annot_type subtype;
	pdf_annot *annot;
	int n;

	int has_redact = 0;
	int was_dirty = pdf->dirty;

	ui_layout(T, X, NW, 2, 2);

	if (ui_popup("CreateAnnotPopup", "Create...", 1, 15))
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
		if (ui_popup_item("Redact")) new_annot(PDF_ANNOT_REDACT);
		ui_popup_end();
	}

	n = 0;
	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		++n;

	ui_list_begin(&annot_list, n, 0, ui.lineheight * 10 + 4);
	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		char buf[256];
		int num = pdf_to_num(ctx, annot->obj);
		subtype = pdf_annot_type(ctx, annot);
		fz_snprintf(buf, sizeof buf, "%d: %s", num, pdf_string_from_annot_type(ctx, subtype));
		if (ui_list_item(&annot_list, annot->obj, buf, selected_annot == annot))
			selected_annot = annot;
		if (subtype == PDF_ANNOT_REDACT)
			has_redact = 1;
	}
	ui_list_end(&annot_list);

	if (selected_annot && (subtype = pdf_annot_type(ctx, selected_annot)) != PDF_ANNOT_WIDGET)
	{
		fz_rect rect;
		fz_irect irect;
		int n, choice;
		pdf_obj *obj;

		if (ui_button("Delete"))
		{
			pdf_delete_annot(ctx, page, selected_annot);
			selected_annot = NULL;
			render_page();
			return;
		}

		ui_spacer();

		/* common annotation properties */

		rect = pdf_annot_rect(ctx, selected_annot);
		irect = fz_irect_from_rect(rect);
		ui_label("Rect: %d %d %d %d", irect.x0, irect.y0, irect.x1, irect.y1);

		do_annotate_flags();
		do_annotate_author();
		do_annotate_date();

		obj = pdf_dict_get(ctx, selected_annot->obj, PDF_NAME(Popup));
		if (obj)
			ui_label("Popup: %d 0 R", pdf_to_num(ctx, obj));

		ui_spacer();

		do_annotate_contents();

		ui_spacer();

		if (subtype == PDF_ANNOT_FREE_TEXT)
		{
			int font_choice, color_choice, size_changed;
			int q = pdf_annot_quadding(ctx, selected_annot);
			const char *text_font;
			static float text_size_f, text_color[3];
			static int text_size;
			ui_label("Text Alignment:");
			choice = ui_select("Q", quadding_names[q], quadding_names, nelem(quadding_names));
			if (choice != -1)
				pdf_set_annot_quadding(ctx, selected_annot, choice);

			pdf_annot_default_appearance(ctx, selected_annot, &text_font, &text_size_f, text_color);
			text_size = text_size_f;

			ui_label("Text Font:");
			font_choice = ui_select("DA/Font", text_font, font_names, nelem(font_names));
			ui_label("Text Size: %d", text_size);
			size_changed = ui_slider(&text_size, 8, 36, 256);
			ui_label("Text Color:");
			color_choice = ui_select("DA/Color", name_from_hex(hex_from_color(3, text_color)), color_names+1, nelem(color_names)-1);
			if (font_choice != -1 || color_choice != -1 || size_changed)
			{
				if (font_choice != -1)
					text_font = font_names[font_choice];
				if (color_choice != -1)
				{
					text_color[0] = ((color_values[color_choice+1]>>16) & 0xff) / 255.0f;
					text_color[1] = ((color_values[color_choice+1]>>8) & 0xff) / 255.0f;
					text_color[2] = ((color_values[color_choice+1]) & 0xff) / 255.0f;
				}
				pdf_set_annot_default_appearance(ctx, selected_annot, text_font, text_size, text_color);
			}
			ui_spacer();
		}

		if (subtype == PDF_ANNOT_LINE)
		{
			enum pdf_line_ending s, e;
			int s_choice, e_choice;

			pdf_annot_line_ending_styles(ctx, selected_annot, &s, &e);

			ui_label("Line Start:");
			s_choice = ui_select("LE0", line_ending_styles[s], line_ending_styles, nelem(line_ending_styles));

			ui_label("Line End:");
			e_choice = ui_select("LE1", line_ending_styles[e], line_ending_styles, nelem(line_ending_styles));

			if (s_choice != -1 || e_choice != -1)
			{
				if (s_choice != -1) s = s_choice;
				if (e_choice != -1) e = e_choice;
				pdf_set_annot_line_ending_styles(ctx, selected_annot, s, e);
			}
		}

		if (pdf_annot_has_icon_name(ctx, selected_annot))
		{
			const char *name = pdf_annot_icon_name(ctx, selected_annot);
			ui_label("Icon:");
			switch (pdf_annot_type(ctx, selected_annot))
			{
			default:
				break;
			case PDF_ANNOT_TEXT:
				choice = ui_select("Icon", name, text_icons, nelem(text_icons));
				if (choice != -1)
					pdf_set_annot_icon_name(ctx, selected_annot, text_icons[choice]);
				break;
			case PDF_ANNOT_FILE_ATTACHMENT:
				choice = ui_select("Icon", name, file_attachment_icons, nelem(file_attachment_icons));
				if (choice != -1)
					pdf_set_annot_icon_name(ctx, selected_annot, file_attachment_icons[choice]);
				break;
			case PDF_ANNOT_SOUND:
				choice = ui_select("Icon", name, sound_icons, nelem(sound_icons));
				if (choice != -1)
					pdf_set_annot_icon_name(ctx, selected_annot, sound_icons[choice]);
				break;
			case PDF_ANNOT_STAMP:
				choice = ui_select("Icon", name, stamp_icons, nelem(stamp_icons));
				if (choice != -1)
					pdf_set_annot_icon_name(ctx, selected_annot, stamp_icons[choice]);
				break;
			}
		}

		if (should_edit_border(subtype))
		{
			static int border;
			border = pdf_annot_border(ctx, selected_annot);
			ui_label("Border: %d", border);
			if (ui_slider(&border, 0, 12, 100))
				pdf_set_annot_border(ctx, selected_annot, border);
		}

		if (should_edit_color(subtype))
			do_annotate_color("Color", pdf_annot_color, pdf_set_annot_color);
		if (should_edit_icolor(subtype))
			do_annotate_color("Interior Color", pdf_annot_interior_color, pdf_set_annot_interior_color);

		if (subtype == PDF_ANNOT_HIGHLIGHT)
		{
			static int opacity;
			opacity = pdf_annot_opacity(ctx, selected_annot) * 255;
			ui_label("Opacity:");
			if (ui_slider(&opacity, 0, 255, 256))
				pdf_set_annot_opacity(ctx, selected_annot, opacity / 255.0f);
		}

		if (pdf_annot_has_open(ctx, selected_annot))
		{
			int is_open = pdf_annot_is_open(ctx, selected_annot);
			int start_is_open = is_open;
			ui_checkbox("Open", &is_open);
			if (start_is_open != is_open)
				pdf_set_annot_is_open(ctx, selected_annot, is_open);
		}

		ui_spacer();

		if (pdf_annot_has_quad_points(ctx, selected_annot))
		{
			if (is_draw_mode)
			{
				n = pdf_annot_quad_point_count(ctx, selected_annot);
				ui_label("QuadPoints: %d", n);
				if (ui_button("Clear"))
					pdf_clear_annot_quad_points(ctx, selected_annot);
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (pdf_annot_has_vertices(ctx, selected_annot))
		{
			if (is_draw_mode)
			{
				n = pdf_annot_vertex_count(ctx, selected_annot);
				ui_label("Vertices: %d", n);
				if (ui_button("Clear"))
					pdf_clear_annot_vertices(ctx, selected_annot);
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (pdf_annot_has_ink_list(ctx, selected_annot))
		{
			if (is_draw_mode)
			{
				n = pdf_annot_ink_list_count(ctx, selected_annot);
				ui_label("InkList: %d strokes", n);
				if (ui_button("Clear"))
					pdf_clear_annot_ink_list(ctx, selected_annot);
				if (ui_button("Done"))
					is_draw_mode = 0;
			}
			else
			{
				if (ui_button("Edit"))
					is_draw_mode = 1;
			}
		}

		if (selected_annot && selected_annot->needs_new_ap)
		{
			pdf_update_appearance(ctx, selected_annot);
			render_page();
		}
	}

	ui_layout(B, X, NW, 2, 2);

	if (ui_button("Save PDF..."))
		do_save_pdf_file();

	if (has_redact)
	{
		if (ui_button("Redact"))
		{
			selected_annot = NULL;
			pdf_redact_page(ctx, pdf, page, NULL);
			load_page();
			render_page();
		}
	}

	if (was_dirty != pdf->dirty)
		update_title();
}

static void do_edit_icon(fz_irect canvas_area, fz_irect area, fz_rect *rect)
{
	static fz_point start_pt;
	static float w, h;
	static int moving = 0;

	if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
	{
		ui.hot = selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			start_pt.x = rect->x0;
			start_pt.y = rect->y0;
			w = rect->x1 - rect->x0;
			h = rect->y1 - rect->y0;
			moving = 1;
		}
	}

	if (ui.active == selected_annot && moving)
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
				fz_rect trect = pdf_annot_rect(ctx, selected_annot);
				dp = fz_transform_vector(dp, view_page_inv_ctm);
				trect.x0 += dp.x; trect.x1 += dp.x;
				trect.y0 += dp.y; trect.y1 += dp.y;
				pdf_set_annot_rect(ctx, selected_annot, trect);
			}
		}
	}
}

static void do_edit_rect(fz_irect canvas_area, fz_irect area, fz_rect *rect)
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
		ui.hot = selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			start_rect = *rect;
			state = ER_NONE;
			if (ui.x <= area.x0 + 10) state |= ER_W;
			if (ui.x >= area.x1 - 10) state |= ER_E;
			if (ui.y <= area.y0 + 10) state |= ER_N;
			if (ui.y >= area.y1 - 10) state |= ER_S;
			if (!state) state = ER_MOVE;
		}
	}

	if (ui.active == selected_annot && state != ER_NONE)
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
				pdf_set_annot_rect(ctx, selected_annot, trect);
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
		ui.hot = selected_annot;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			pdf_annot_line(ctx, selected_annot, &start_a, &start_b);
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

	if (ui.active == selected_annot && state != 0)
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
		lw = pdf_annot_border(ctx, selected_annot);
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
				pdf_set_annot_line(ctx, selected_annot, a, b);
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
		ui.hot = selected_annot;
		if (!ui.active || ui.active == selected_annot)
			ui.cursor = GLUT_CURSOR_CROSSHAIR;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			drawing = 1;
		}
	}

	if (ui.active == selected_annot && drawing)
	{
		int n = pdf_annot_vertex_count(ctx, selected_annot);
		if (n > 0)
		{
			p = pdf_annot_vertex(ctx, selected_annot, n-1);
			p = fz_transform_point(p, view_page_ctm);
			if (close)
			{
				a = pdf_annot_vertex(ctx, selected_annot, 0);
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
			pdf_add_annot_vertex(ctx, selected_annot, p);
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
		ui.hot = selected_annot;
		if (!ui.active || ui.active == selected_annot)
			ui.cursor = GLUT_CURSOR_CROSSHAIR;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			drawing = 1;
			n = 0;
			last_x = INT_MIN;
			last_y = INT_MIN;
		}
	}

	if (ui.active == selected_annot && drawing)
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
				for (i = 0; i < n; ++i)
					p[i] = fz_transform_point(p[i], view_page_inv_ctm);
				pdf_add_annot_ink_list(ctx, selected_annot, n, p);
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
	int i, n;

	if (ui_mouse_inside(view_page_area))
	{
		ui.hot = selected_annot;
		if (!ui.active || ui.active == selected_annot)
			ui.cursor = GLUT_CURSOR_TEXT;
		if (!ui.active && ui.down)
		{
			ui.active = selected_annot;
			marking = 1;
			pt.x = ui.x;
			pt.y = ui.y;
		}
	}

	if (ui.active == selected_annot && marking)
	{
		fz_point page_a = { pt.x, pt.y };
		fz_point page_b = { ui.x, ui.y };

		page_a = fz_transform_point(page_a, view_page_inv_ctm);
		page_b = fz_transform_point(page_b, view_page_inv_ctm);

		n = fz_highlight_selection(ctx, page_text, page_a, page_b, hits, nelem(hits));

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
				pdf_clear_annot_quad_points(ctx, selected_annot);
				for (i = 0; i < n; ++i)
					pdf_add_annot_quad_point(ctx, selected_annot, hits[i]);
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

	int was_dirty = pdf->dirty;

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		enum pdf_annot_type subtype = pdf_annot_type(ctx, annot);

		bounds = pdf_bound_annot(ctx, annot);
		bounds = fz_transform_rect(bounds, view_page_ctm);
		area = fz_irect_from_rect(bounds);

		if (ui_mouse_inside(canvas_area) && ui_mouse_inside(area))
		{
			ui.hot = annot;
			if (!ui.active && ui.down)
			{
				if (selected_annot != annot)
				{
					if (!selected_annot && !showannotate)
						toggle_annotate();
					ui.active = annot;
					selected_annot = annot;
				}
			}
		}

		if (annot == selected_annot)
		{
			switch (subtype)
			{
			default:
				break;

			/* Popup window */
			case PDF_ANNOT_POPUP:
				do_edit_rect(canvas_area, area, &bounds);
				break;

			/* Icons */
			case PDF_ANNOT_TEXT:
			case PDF_ANNOT_CARET:
			case PDF_ANNOT_FILE_ATTACHMENT:
			case PDF_ANNOT_SOUND:
				do_edit_icon(canvas_area, area, &bounds);
				break;

			case PDF_ANNOT_STAMP:
				do_edit_rect(canvas_area, area, &bounds);
				break;

			case PDF_ANNOT_FREE_TEXT:
				do_edit_rect(canvas_area, area, &bounds);
				break;

			/* Drawings */
			case PDF_ANNOT_LINE:
				do_edit_line(canvas_area, area, &bounds);
				break;
			case PDF_ANNOT_CIRCLE:
			case PDF_ANNOT_SQUARE:
				do_edit_rect(canvas_area, area, &bounds);
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

			if (annot->needs_new_ap)
			{
				pdf_update_appearance(ctx, annot);
				render_page();
			}
		}
	}

	if (ui_mouse_inside(canvas_area) && ui.down)
	{
		if (!ui.active && ui.hot == nothing)
			selected_annot = NULL;
	}

	if (ui.right)
		is_draw_mode = 0;

	if (was_dirty != pdf->dirty)
		update_title();
}
