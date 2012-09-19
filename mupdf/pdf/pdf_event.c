#include "fitz.h"
#include "mupdf-internal.h"

typedef struct
{
	fz_doc_event base;
	fz_alert_event alert;
} fz_alert_event_internal;

fz_alert_event *fz_access_alert_event(fz_doc_event *event)
{
	fz_alert_event *alert = NULL;

	if (event->type == FZ_DOCUMENT_EVENT_ALERT)
		alert = &((fz_alert_event_internal *)event)->alert;

	return alert;
}

void pdf_event_issue_alert(pdf_document *doc, fz_alert_event *alert)
{
	fz_alert_event_internal ievent;
	ievent.base.type = FZ_DOCUMENT_EVENT_ALERT;
	ievent.alert = *alert;

	if (doc->event_cb)
		doc->event_cb((fz_doc_event *)&ievent, doc->event_cb_data);
}

void pdf_set_doc_event_callback(pdf_document *doc, fz_doc_event_cb *fn, void *data)
{
	doc->event_cb = fn;
	doc->event_cb_data = data;
}
