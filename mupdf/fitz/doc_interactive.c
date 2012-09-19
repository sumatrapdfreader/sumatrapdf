#include "fitz.h"
#include "mupdf-internal.h"

/*
	PDF is currently the only interactive format, so no need
	to indirect through function pointers.
*/

int fz_has_unsaved_changes(fz_interactive *idoc)
{
	return pdf_has_unsaved_changes((pdf_document*)idoc);
}

int fz_pass_event(fz_interactive *idoc, fz_page *page, fz_ui_event *ui_event)
{
	return pdf_pass_event((pdf_document*)idoc, (pdf_page*)page, ui_event);
}

fz_rect *fz_poll_screen_update(fz_interactive *idoc)
{
	return pdf_poll_screen_update((pdf_document*)idoc);
}

fz_widget *fz_focused_widget(fz_interactive *idoc)
{
	return pdf_focused_widget((pdf_document*)idoc);
}

fz_widget *fz_first_widget(fz_interactive *idoc, fz_page *page)
{
	return pdf_first_widget((pdf_document*)idoc, (pdf_page*)page);
}

fz_widget *fz_next_widget(fz_interactive *idoc, fz_widget *previous)
{
	return pdf_next_widget(previous);
}

char *fz_text_widget_text(fz_interactive *idoc, fz_widget *tw)
{
	return pdf_text_widget_text((pdf_document *)idoc, tw);
}

int fz_text_widget_max_len(fz_interactive *idoc, fz_widget *tw)
{
	return pdf_text_widget_max_len((pdf_document *)idoc, tw);
}

int fz_text_widget_content_type(fz_interactive *idoc, fz_widget *tw)
{
	return pdf_text_widget_content_type((pdf_document *)idoc, tw);
}

int fz_text_widget_set_text(fz_interactive *idoc, fz_widget *tw, char *text)
{
	return pdf_text_widget_set_text((pdf_document *)idoc, tw, text);
}

int fz_choice_widget_options(fz_interactive *idoc, fz_widget *tw, char *opts[])
{
	return pdf_choice_widget_options((pdf_document *)idoc, tw, opts);
}

int fz_choice_widget_is_multiselect(fz_interactive *idoc, fz_widget *tw)
{
	return pdf_choice_widget_is_multiselect((pdf_document *)idoc, tw);
}

int fz_choice_widget_value(fz_interactive *idoc, fz_widget *tw, char *opts[])
{
	return pdf_choice_widget_value((pdf_document *)idoc, tw, opts);
}

void fz_choice_widget_set_value(fz_interactive *idoc, fz_widget *tw, int n, char *opts[])
{
	pdf_choice_widget_set_value((pdf_document *)idoc, tw, n, opts);
}

void fz_set_doc_event_callback(fz_interactive *idoc, fz_doc_event_cb *event_cb, void *data)
{
	pdf_set_doc_event_callback((pdf_document *)idoc, event_cb, data);
}
