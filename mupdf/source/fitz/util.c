#include "mupdf/fitz.h"

#include <float.h>

fz_display_list *
fz_new_display_list_from_page(fz_context *ctx, fz_page *page)
{
	fz_display_list *list;
	fz_device *dev = NULL;

	fz_var(dev);

	list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_list_device(ctx, list);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_rethrow(ctx);
	}

	return list;
}

fz_display_list *
fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number)
{
	fz_page *page;
	fz_display_list *list = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		list = fz_new_display_list_from_page(ctx, page);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return list;
}

fz_display_list *
fz_new_display_list_from_page_contents(fz_context *ctx, fz_page *page)
{
	fz_display_list *list;
	fz_device *dev = NULL;

	fz_var(dev);

	list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_list_device(ctx, list);
		fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_rethrow(ctx);
	}

	return list;
}

/*
	Render the page to a pixmap using the transform and colorspace.
*/
fz_pixmap *
fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_display_list_with_separations(ctx, list, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_display_list_with_separations(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;

	fz_var(dev);

	rect = fz_bound_display_list(ctx, list);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);
	if (alpha)
		fz_clear_pixmap(ctx, pix);
	else
		fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

/*
	Render the page contents without annotations.
*/
fz_pixmap *
fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_contents_with_separations(ctx, page, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_contents_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;

	fz_var(dev);

	rect = fz_bound_page(ctx, page);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);
	if (alpha)
		fz_clear_pixmap(ctx, pix);
	else
		fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_with_separations(ctx, page, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;

	fz_var(dev);

	rect = fz_bound_page(ctx, page);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);

	fz_try(ctx)
	{
		if (alpha)
			fz_clear_pixmap(ctx, pix);
		else
			fz_clear_pixmap_with_value(ctx, pix, 0xFF);

		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_number_with_separations(ctx, doc, number, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_number_with_separations(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_page *page;
	fz_pixmap *pix = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_with_separations(ctx, page, ctm, cs, seps, alpha);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return pix;
}

fz_stext_page *
fz_new_stext_page_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options)
{
	fz_stext_page *text;
	fz_device *dev = NULL;

	fz_var(dev);

	if (list == NULL)
		return NULL;

	text = fz_new_stext_page(ctx, fz_bound_display_list(ctx, list));
	fz_try(ctx)
	{
		dev = fz_new_stext_device(ctx, text, options);
		fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_stext_page *
fz_new_stext_page_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options)
{
	fz_stext_page *text;
	fz_device *dev = NULL;

	fz_var(dev);

	if (page == NULL)
		return NULL;

	text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_stext_device(ctx, text, options);
		fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_stext_page *
fz_new_stext_page_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options)
{
	fz_page *page;
	fz_stext_page *text = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		text = fz_new_stext_page_from_page(ctx, page, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return text;
}

int
fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, fz_quad *hit_bbox, int hit_max)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_display_list(ctx, list, NULL);
	fz_try(ctx)
		count = fz_search_stext_page(ctx, text, needle, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

/*
	Search for the 'needle' text on the page.
	Record the hits in the hit_bbox array and return the number of hits.
	Will stop looking once it has filled hit_max rectangles.
*/
int
fz_search_page(fz_context *ctx, fz_page *page, const char *needle, fz_quad *hit_bbox, int hit_max)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_page(ctx, page, NULL);
	fz_try(ctx)
		count = fz_search_stext_page(ctx, text, needle, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_quad *hit_bbox, int hit_max)
{
	fz_page *page;
	int count = 0;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_search_page(ctx, page, needle, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int number, const char *needle, fz_quad *hit_bbox, int hit_max)
{
	fz_page *page;
	int count = 0;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		count = fz_search_page(ctx, page, needle, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

/*
	Convert structured text into plain text.
*/
fz_buffer *
fz_new_buffer_from_stext_page(fz_context *ctx, fz_stext_page *page)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	fz_buffer *buf;

	buf = fz_new_buffer(ctx, 256);
	fz_try(ctx)
	{
		for (block = page->first_block; block; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_TEXT)
			{
				for (line = block->u.t.first_line; line; line = line->next)
				{
					for (ch = line->first_char; ch; ch = ch->next)
						fz_append_rune(ctx, buf, ch->c);
					fz_append_byte(ctx, buf, '\n');
				}
				fz_append_byte(ctx, buf, '\n');
			}
		}
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}

	return buf;
}

fz_buffer *
fz_new_buffer_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options)
{
	fz_stext_page *text;
	fz_buffer *buf = NULL;

	text = fz_new_stext_page_from_display_list(ctx, list, options);
	fz_try(ctx)
		buf = fz_new_buffer_from_stext_page(ctx, text);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options)
{
	fz_stext_page *text;
	fz_buffer *buf = NULL;

	text = fz_new_stext_page_from_page(ctx, page, options);
	fz_try(ctx)
		buf = fz_new_buffer_from_stext_page(ctx, text);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options)
{
	fz_page *page;
	fz_buffer *buf = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		buf = fz_new_buffer_from_page(ctx, page, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

/*
	Write image as a data URI (for HTML and SVG output).
*/
void
fz_write_image_as_data_uri(fz_context *ctx, fz_output *out, fz_image *image)
{
	fz_compressed_buffer *cbuf;
	fz_buffer *buf;

	cbuf = fz_compressed_image_buffer(ctx, image);

	if (cbuf && cbuf->params.type == FZ_IMAGE_JPEG)
	{
		int type = fz_colorspace_type(ctx, image->colorspace);
		if (type == FZ_COLORSPACE_GRAY || type == FZ_COLORSPACE_RGB)
		{
			fz_write_string(ctx, out, "data:image/jpeg;base64,");
			fz_write_base64_buffer(ctx, out, cbuf->buffer, 1);
			return;
		}
	}
	if (cbuf && cbuf->params.type == FZ_IMAGE_PNG)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, cbuf->buffer, 1);
		return;
	}

	buf = fz_new_buffer_from_image_as_png(ctx, image, fz_default_color_params);
	fz_try(ctx)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_write_pixmap_as_data_uri(fz_context *ctx, fz_output *out, fz_pixmap *pixmap)
{
	fz_buffer *buf = fz_new_buffer_from_pixmap_as_png(ctx, pixmap, fz_default_color_params);
	fz_try(ctx)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
 * Use text extraction to convert the input document into XHTML, then
 * open the result as a new document that can be reflowed.
 */
fz_document *
fz_new_xhtml_document_from_document(fz_context *ctx, fz_document *old_doc)
{
	fz_stext_options opts = { FZ_STEXT_PRESERVE_IMAGES };
	fz_document *new_doc;
	fz_buffer *buf = NULL;
	fz_output *out = NULL;
	fz_stream *stm = NULL;
	fz_stext_page *text = NULL;
	int i;

	fz_var(buf);
	fz_var(out);
	fz_var(stm);
	fz_var(text);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 8192);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_header_as_xhtml(ctx, out);

		for (i = 0; i < fz_count_pages(ctx, old_doc); ++i)
		{
			text = fz_new_stext_page_from_page_number(ctx, old_doc, i, &opts);
			fz_print_stext_page_as_xhtml(ctx, out, text, i+1);
			fz_drop_stext_page(ctx, text);
			text = NULL;
		}

		fz_print_stext_trailer_as_xhtml(ctx, out);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);

		stm = fz_open_buffer(ctx, buf);
		new_doc = fz_open_document_with_stream(ctx, "application/html+xml", stm);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_drop_buffer(ctx, buf);
		fz_drop_output(ctx, out);
		fz_drop_stext_page(ctx, text);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return new_doc;
}
