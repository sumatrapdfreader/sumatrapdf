// Copyright (C) 2004-2021 Artifex Software, Inc.
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

typedef struct
{
	pdf_doc_event base;
	pdf_alert_event alert;
} pdf_alert_event_internal;

pdf_alert_event *pdf_access_alert_event(fz_context *ctx, pdf_doc_event *evt)
{
	pdf_alert_event *alert = NULL;

	if (evt->type == PDF_DOCUMENT_EVENT_ALERT)
		alert = &((pdf_alert_event_internal *)evt)->alert;

	return alert;
}

void pdf_event_issue_alert(fz_context *ctx, pdf_document *doc, pdf_alert_event *alert)
{
	if (doc->event_cb)
	{
		pdf_alert_event_internal ievent;
		ievent.base.type = PDF_DOCUMENT_EVENT_ALERT;
		ievent.alert = *alert;

		doc->event_cb(ctx, doc, (pdf_doc_event *)&ievent, doc->event_cb_data);

		*alert = ievent.alert;
	}
}

void pdf_event_issue_print(fz_context *ctx, pdf_document *doc)
{
	pdf_doc_event e;

	e.type = PDF_DOCUMENT_EVENT_PRINT;

	if (doc->event_cb)
		doc->event_cb(ctx, doc, &e, doc->event_cb_data);
}

typedef struct
{
	pdf_doc_event base;
	const char *item;
} pdf_exec_menu_item_event_internal;

const char *pdf_access_exec_menu_item_event(fz_context *ctx, pdf_doc_event *evt)
{
	const char *item = NULL;

	if (evt->type == PDF_DOCUMENT_EVENT_EXEC_MENU_ITEM)
		item = ((pdf_exec_menu_item_event_internal *)evt)->item;

	return item;
}

void pdf_event_issue_exec_menu_item(fz_context *ctx, pdf_document *doc, const char *item)
{
	if (doc->event_cb)
	{
		pdf_exec_menu_item_event_internal ievent;
		ievent.base.type = PDF_DOCUMENT_EVENT_EXEC_MENU_ITEM;
		ievent.item = item;

		doc->event_cb(ctx, doc, (pdf_doc_event *)&ievent, doc->event_cb_data);
	}
}

typedef struct
{
	pdf_doc_event base;
	pdf_launch_url_event launch_url;
} pdf_launch_url_event_internal;

pdf_launch_url_event *pdf_access_launch_url_event(fz_context *ctx, pdf_doc_event *evt)
{
	pdf_launch_url_event *launch_url = NULL;

	if (evt->type == PDF_DOCUMENT_EVENT_LAUNCH_URL)
		launch_url = &((pdf_launch_url_event_internal *)evt)->launch_url;

	return launch_url;
}

void pdf_event_issue_launch_url(fz_context *ctx, pdf_document *doc, const char *url, int new_frame)
{
	if (doc->event_cb)
	{
		pdf_launch_url_event_internal e;

		e.base.type = PDF_DOCUMENT_EVENT_LAUNCH_URL;
		e.launch_url.url = url;
		e.launch_url.new_frame = new_frame;
		doc->event_cb(ctx, doc, (pdf_doc_event *)&e, doc->event_cb_data);
	}
}

typedef struct
{
	pdf_doc_event base;
	pdf_mail_doc_event mail_doc;
} pdf_mail_doc_event_internal;

pdf_mail_doc_event *pdf_access_mail_doc_event(fz_context *ctx, pdf_doc_event *evt)
{
	pdf_mail_doc_event *mail_doc = NULL;

	if (evt->type == PDF_DOCUMENT_EVENT_MAIL_DOC)
		mail_doc = &((pdf_mail_doc_event_internal *)evt)->mail_doc;

	return mail_doc;
}

void pdf_event_issue_mail_doc(fz_context *ctx, pdf_document *doc, pdf_mail_doc_event *evt)
{
	if (doc->event_cb)
	{
		pdf_mail_doc_event_internal e;

		e.base.type = PDF_DOCUMENT_EVENT_MAIL_DOC;
		e.mail_doc = *evt;

		doc->event_cb(ctx, doc, (pdf_doc_event *)&e, doc->event_cb_data);
	}
}

void pdf_set_doc_event_callback(fz_context *ctx, pdf_document *doc, pdf_doc_event_cb *event_cb, pdf_free_doc_event_data_cb *free_event_data_cb, void *data)
{
	if (doc->free_event_data_cb)
		doc->free_event_data_cb(ctx, doc->event_cb_data);
	doc->event_cb = event_cb;
	doc->free_event_data_cb = free_event_data_cb;
	doc->event_cb_data = data;
}

void *pdf_get_doc_event_callback_data(fz_context *ctx, pdf_document *doc)
{
	return doc->event_cb_data;
}
