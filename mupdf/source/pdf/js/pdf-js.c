#include "mupdf/pdf.h"

struct pdf_js_s
{
	pdf_document *doc;
	pdf_obj *form;
	pdf_js_event event;
	pdf_jsimp *imp;
	pdf_jsimp_type *doctype;
	pdf_jsimp_type *eventtype;
	pdf_jsimp_type *fieldtype;
	pdf_jsimp_type *apptype;
};

static pdf_jsimp_obj *app_alert(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_jsimp_obj *cMsg_obj = NULL;
	pdf_jsimp_obj *nIcon_obj = NULL;
	pdf_jsimp_obj *nType_obj = NULL;
	pdf_jsimp_obj *cTitle_obj = NULL;
	pdf_jsimp_obj *nButton_obj = NULL;
	pdf_alert_event event;
	int arg_is_obj = 0;

	if (argc < 1 || argc > 6)
		return NULL;

	event.message = "";
	event.icon_type = PDF_ALERT_ICON_ERROR;
	event.button_group_type = PDF_ALERT_BUTTON_GROUP_OK;
	event.title = "MuPDF";
	event.check_box_message = NULL;
	event.button_pressed = 0;

	fz_var(cMsg_obj);
	fz_var(nIcon_obj);
	fz_var(nType_obj);
	fz_var(cTitle_obj);
	fz_try(ctx)
	{
		arg_is_obj = (argc == 1 && pdf_jsimp_to_type(js->imp, args[0]) != JS_TYPE_STRING);
		if (arg_is_obj)
		{
			cMsg_obj = pdf_jsimp_property(js->imp, args[0], "cMsg");
			nIcon_obj = pdf_jsimp_property(js->imp, args[0], "nIcon");
			nType_obj = pdf_jsimp_property(js->imp, args[0], "nType");
			cTitle_obj = pdf_jsimp_property(js->imp, args[0], "cTitle");
		}
		else
		{
			switch (argc)
			{
			case 6:
			case 5:
			case 4:
				cTitle_obj = args[3];
			case 3:
				nType_obj = args[2];
			case 2:
				nIcon_obj = args[1];
			case 1:
				cMsg_obj = args[0];
			}
		}

		if (cMsg_obj)
			event.message = pdf_jsimp_to_string(js->imp, cMsg_obj);

		if (nIcon_obj)
			event.icon_type = (int)pdf_jsimp_to_number(js->imp, nIcon_obj);

		if (nType_obj)
			event.button_group_type = (int)pdf_jsimp_to_number(js->imp, nType_obj);

		if (cTitle_obj)
			event.title = pdf_jsimp_to_string(js->imp, cTitle_obj);

		pdf_event_issue_alert(js->doc, &event);
		nButton_obj = pdf_jsimp_from_number(js->imp, (double)event.button_pressed);
	}
	fz_always(ctx)
	{
		if (arg_is_obj)
		{
			pdf_jsimp_drop_obj(js->imp, cMsg_obj);
			pdf_jsimp_drop_obj(js->imp, nIcon_obj);
			pdf_jsimp_drop_obj(js->imp, nType_obj);
			pdf_jsimp_drop_obj(js->imp, cTitle_obj);
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return nButton_obj;
}

static pdf_jsimp_obj *app_execDialog(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;

	pdf_event_issue_exec_dialog(js->doc);

	return NULL;
}

static pdf_jsimp_obj *app_execMenuItem(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;

	if (argc == 1)
		pdf_event_issue_exec_menu_item(js->doc, pdf_jsimp_to_string(js->imp, args[0]));

	return NULL;
}

static pdf_jsimp_obj *app_launchURL(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	char *cUrl;
	int bNewFrame = 0;

	switch (argc)
	{
	default:
		return NULL;
	case 2:
		bNewFrame = (int)pdf_jsimp_to_number(js->imp, args[1]);
	case 1:
		cUrl = pdf_jsimp_to_string(js->imp, args[0]);
	}

	pdf_event_issue_launch_url(js->doc, cUrl, bNewFrame);

	return NULL;
}

static pdf_obj *load_color(pdf_document *doc, pdf_jsimp *imp, pdf_jsimp_obj *val)
{
	pdf_obj *col = NULL;
	pdf_obj *comp = NULL;
	pdf_jsimp_obj *jscomp = NULL;
	int i;
	int n;
	fz_context *ctx = doc->ctx;

	n = pdf_jsimp_array_len(imp, val);

	/* The only legitimate color expressed as an array of length 1
	 * is [T], meaning transparent. Return a NULL object to represent
	 * transparent */
	if (n <= 1)
		return NULL;

	col = pdf_new_array(doc, n-1);

	fz_var(comp);
	fz_var(jscomp);
	fz_try(ctx)
	{
		for (i = 0; i < n-1; i++)
		{
			jscomp = pdf_jsimp_array_item(imp, val, i+1);
			comp = pdf_new_real(doc, pdf_jsimp_to_number(imp, jscomp));
			pdf_array_push(col, comp);
			pdf_jsimp_drop_obj(imp, jscomp);
			jscomp = NULL;
			pdf_drop_obj(comp);
			comp = NULL;
		}
	}
	fz_catch(ctx)
	{
		pdf_jsimp_drop_obj(imp, jscomp);
		pdf_drop_obj(comp);
		pdf_drop_obj(col);
		fz_rethrow(ctx);
	}

	return col;
}

static pdf_jsimp_obj *field_buttonSetCaption(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;
	char *name;

	if (argc != 1)
		return NULL;

	name = pdf_jsimp_to_string(js->imp, args[0]);
	pdf_field_set_button_caption(js->doc, field, name);

	return NULL;
}

static pdf_jsimp_obj *field_getName(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_obj *field = (pdf_obj *)obj;
	char *name;
	pdf_jsimp_obj *oname = NULL;

	if (field == NULL)
		return NULL;

	name = pdf_field_name(js->doc, field);
	fz_try(ctx)
	{
		oname = pdf_jsimp_from_string(js->imp, name);
	}
	fz_always(ctx)
	{
		fz_free(ctx, name);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return oname;
}

static void field_setName(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_warn(js->doc->ctx, "Unexpected call to field_setName");
}

static pdf_jsimp_obj *field_getDisplay(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;

	return field ? pdf_jsimp_from_number(js->imp, (double)pdf_field_display(js->doc, field)) : NULL;
}

static void field_setDisplay(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;
	if (field)
		pdf_field_set_display(js->doc, field, (int)pdf_jsimp_to_number(js->imp, val));
}

static pdf_jsimp_obj *field_getFillColor(void *jsctx, void *obj)
{
	return NULL;
}

static void field_setFillColor(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_obj *field = (pdf_obj *)obj;
	pdf_obj *col;

	if (!field)
		return;

	col = load_color(js->doc, js->imp, val);
	fz_try(ctx)
	{
		pdf_field_set_fill_color(js->doc, field, col);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(col);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static pdf_jsimp_obj *field_getTextColor(void *jsctx, void *obj)
{
	return NULL;
}

static void field_setTextColor(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_obj *field = (pdf_obj *)obj;
	pdf_obj *col;

	if (!field)
		return;

	col = load_color(js->doc, js->imp, val);
	fz_try(ctx)
	{
		pdf_field_set_text_color(js->doc, field, col);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(col);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static pdf_jsimp_obj *field_getBorderStyle(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;

	return field ? pdf_jsimp_from_string(js->imp, pdf_field_border_style(js->doc, field)) : NULL;
}

static void field_setBorderStyle(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;

	if (field)
		pdf_field_set_border_style(js->doc, field, pdf_jsimp_to_string(js->imp, val));
}

static pdf_jsimp_obj *field_getValue(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;
	char *fval;

	if (!field)
		return NULL;

	fval = pdf_field_value(js->doc, field);
	return pdf_jsimp_from_string(js->imp, fval?fval:"");
}

static void field_setValue(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	pdf_obj *field = (pdf_obj *)obj;

	if (field)
		(void)pdf_field_set_value(js->doc, field, pdf_jsimp_to_string(js->imp, val));
}

static pdf_jsimp_obj *event_getTarget(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;

	return pdf_jsimp_new_obj(js->imp, js->fieldtype, js->event.target);
}

static void event_setTarget(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_warn(js->doc->ctx, "Unexpected call to event_setTarget");
}

static pdf_jsimp_obj *event_getValue(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;
	char *v = js->event.value;

	return pdf_jsimp_from_string(js->imp, v?v:"");
}

static void event_setValue(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	fz_free(ctx, js->event.value);
	js->event.value = NULL;
	js->event.value = fz_strdup(ctx, pdf_jsimp_to_string(js->imp, val));
}

static pdf_jsimp_obj *event_getWillCommit(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;

	return pdf_jsimp_from_number(js->imp, 1.0);
}

static void event_setWillCommit(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_warn(js->doc->ctx, "Unexpected call to event_setWillCommit");
}

static pdf_jsimp_obj *event_getRC(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;

	return pdf_jsimp_from_number(js->imp, (double)js->event.rc);
}

static void event_setRC(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;

	js->event.rc = (int)pdf_jsimp_to_number(js->imp, val);
}

static pdf_jsimp_obj *doc_getEvent(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;

	return pdf_jsimp_new_obj(js->imp, js->eventtype, &js->event);
}

static void doc_setEvent(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_warn(js->doc->ctx, "Unexpected call to doc_setEvent");
}

static pdf_jsimp_obj *doc_getApp(void *jsctx, void *obj)
{
	pdf_js *js = (pdf_js *)jsctx;

	return pdf_jsimp_new_obj(js->imp, js->apptype, NULL);
}

static void doc_setApp(void *jsctx, void *obj, pdf_jsimp_obj *val)
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_warn(js->doc->ctx, "Unexpected call to doc_setApp");
}

static char *utf8_to_pdf(fz_context *ctx, char *utf8)
{
	char *pdf = fz_malloc(ctx, strlen(utf8)+1);
	int i = 0;
	unsigned char c;

	while ((c = *utf8) != 0)
	{
		if ((c & 0x80) == 0 && pdf_doc_encoding[c] == c)
		{
			pdf[i++] = c;
			utf8++ ;
		}
		else
		{
			int rune;
			int j;

			utf8 += fz_chartorune(&rune, utf8);

			for (j = 0; j < sizeof(pdf_doc_encoding) && pdf_doc_encoding[j] != rune; j++)
				;

			if (j < sizeof(pdf_doc_encoding))
				pdf[i++] = j;
		}
	}

	pdf[i] = 0;

	return pdf;
}

static pdf_jsimp_obj *doc_getField(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_obj *dict = NULL;
	char *utf8;
	char *name = NULL;

	if (argc != 1)
		return NULL;

	fz_var(dict);
	fz_var(name);
	fz_try(ctx)
	{
		utf8 = pdf_jsimp_to_string(js->imp, args[0]);

		if (utf8)
		{
			name = utf8_to_pdf(ctx, utf8);
			dict = pdf_lookup_field(js->form, name);
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, name);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "doc_getField failed: %s", fz_caught_message(ctx));
		dict = NULL;
	}

	return dict ? pdf_jsimp_new_obj(js->imp, js->fieldtype, dict) : NULL;
}

static void reset_field(pdf_js *js, pdf_jsimp_obj *item)
{
	fz_context *ctx = js->doc->ctx;
	char *name = NULL;
	char *utf8 = pdf_jsimp_to_string(js->imp, item);

	if (utf8)
	{
		pdf_obj *field;

		fz_var(name);
		fz_try(ctx)
		{
			name = utf8_to_pdf(ctx, utf8);
			field = pdf_lookup_field(js->form, name);
			if (field)
				pdf_field_reset(js->doc, field);
		}
		fz_always(ctx)
		{
			fz_free(ctx, name);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
}

static pdf_jsimp_obj *doc_resetForm(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_jsimp_obj *arr = NULL;
	pdf_jsimp_obj *elem = NULL;

	switch (argc)
	{
	case 0:
		break;
	case 1:
		switch (pdf_jsimp_to_type(js->imp, args[0]))
		{
		case JS_TYPE_NULL:
			break;
		case JS_TYPE_ARRAY:
			arr = args[0];
			break;
		case JS_TYPE_STRING:
			elem = args[0];
			break;
		default:
			return NULL;
		}
		break;
	default:
		return NULL;
	}

	fz_try(ctx)
	{
		if(arr)
		{
			/* An array of fields has been passed in. Call
			 * pdf_reset_field on each */
			int i, n = pdf_jsimp_array_len(js->imp, arr);

			for (i = 0; i < n; i++)
			{
				pdf_jsimp_obj *item = pdf_jsimp_array_item(js->imp, arr, i);

				if (item)
					reset_field(js, item);

			}
		}
		else if (elem)
		{
			reset_field(js, elem);
		}
		else
		{
			/* No argument or null passed in means reset all. */
			int i, n = pdf_array_len(js->form);

			for (i = 0; i < n; i++)
				pdf_field_reset(js->doc, pdf_array_get(js->form, i));
		}
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "doc_resetForm failed: %s", fz_caught_message(ctx));
	}

	return NULL;
}

static pdf_jsimp_obj *doc_print(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;

	pdf_event_issue_print(js->doc);

	return NULL;
}

static pdf_jsimp_obj *doc_mailDoc(void *jsctx, void *obj, int argc, pdf_jsimp_obj *args[])
{
	pdf_js *js = (pdf_js *)jsctx;
	fz_context *ctx = js->doc->ctx;
	pdf_jsimp_obj *bUI_obj = NULL;
	pdf_jsimp_obj *cTo_obj = NULL;
	pdf_jsimp_obj *cCc_obj = NULL;
	pdf_jsimp_obj *cBcc_obj = NULL;
	pdf_jsimp_obj *cSubject_obj = NULL;
	pdf_jsimp_obj *cMessage_obj = NULL;
	pdf_mail_doc_event event;
	int arg_is_obj = 0;

	if (argc < 1 || argc > 6)
		return NULL;

	event.ask_user = 1;
	event.to = "";
	event.cc = "";
	event.bcc = "";
	event.subject = "";
	event.message = "";

	fz_var(bUI_obj);
	fz_var(cTo_obj);
	fz_var(cCc_obj);
	fz_var(cBcc_obj);
	fz_var(cSubject_obj);
	fz_var(cMessage_obj);
	fz_try(ctx)
	{
		arg_is_obj = (argc == 1 && pdf_jsimp_to_type(js->imp, args[0]) != JS_TYPE_BOOLEAN);
		if (arg_is_obj)
		{
			bUI_obj = pdf_jsimp_property(js->imp, args[0], "bUI");
			cTo_obj = pdf_jsimp_property(js->imp, args[0], "cTo");
			cCc_obj = pdf_jsimp_property(js->imp, args[0], "cCc");
			cBcc_obj = pdf_jsimp_property(js->imp, args[0], "cBcc");
			cSubject_obj = pdf_jsimp_property(js->imp, args[0], "cSubject");
			cMessage_obj = pdf_jsimp_property(js->imp, args[0], "cMessage");
		}
		else
		{
			switch (argc)
			{
			case 6:
				cMessage_obj = args[5];
			case 5:
				cSubject_obj = args[4];
			case 4:
				cBcc_obj = args[3];
			case 3:
				cCc_obj = args[2];
			case 2:
				cTo_obj = args[1];
			case 1:
				bUI_obj = args[0];
			}
		}

		if (bUI_obj)
			event.ask_user = (int)pdf_jsimp_to_number(js->imp, bUI_obj);

		if (cTo_obj)
			event.to = pdf_jsimp_to_string(js->imp, cTo_obj);

		if (cCc_obj)
			event.cc = pdf_jsimp_to_string(js->imp, cCc_obj);

		if (cBcc_obj)
			event.bcc = pdf_jsimp_to_string(js->imp, cBcc_obj);

		if (cSubject_obj)
			event.subject = pdf_jsimp_to_string(js->imp, cSubject_obj);

		if (cMessage_obj)
			event.message = pdf_jsimp_to_string(js->imp, cMessage_obj);

		pdf_event_issue_mail_doc(js->doc, &event);
	}
	fz_always(ctx)
	{
		if (arg_is_obj)
		{
			pdf_jsimp_drop_obj(js->imp, bUI_obj);
			pdf_jsimp_drop_obj(js->imp, cTo_obj);
			pdf_jsimp_drop_obj(js->imp, cCc_obj);
			pdf_jsimp_drop_obj(js->imp, cBcc_obj);
			pdf_jsimp_drop_obj(js->imp, cSubject_obj);
			pdf_jsimp_drop_obj(js->imp, cMessage_obj);
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return NULL;
}

static void declare_dom(pdf_js *js)
{
	pdf_jsimp *imp = js->imp;

	/* Create the document type */
	js->doctype = pdf_jsimp_new_type(imp, NULL);
	pdf_jsimp_addmethod(imp, js->doctype, "getField", doc_getField);
	pdf_jsimp_addmethod(imp, js->doctype, "resetForm", doc_resetForm);
	pdf_jsimp_addmethod(imp, js->doctype, "print", doc_print);
	pdf_jsimp_addmethod(imp, js->doctype, "mailDoc", doc_mailDoc);
	pdf_jsimp_addproperty(imp, js->doctype, "event", doc_getEvent, doc_setEvent);
	pdf_jsimp_addproperty(imp, js->doctype, "app", doc_getApp, doc_setApp);

	/* Create the event type */
	js->eventtype = pdf_jsimp_new_type(imp, NULL);
	pdf_jsimp_addproperty(imp, js->eventtype, "target", event_getTarget, event_setTarget);
	pdf_jsimp_addproperty(imp, js->eventtype, "value", event_getValue, event_setValue);
	pdf_jsimp_addproperty(imp, js->eventtype, "willCommit", event_getWillCommit, event_setWillCommit);
	pdf_jsimp_addproperty(imp, js->eventtype, "rc", event_getRC, event_setRC);

	/* Create the field type */
	js->fieldtype = pdf_jsimp_new_type(imp, NULL);
	pdf_jsimp_addproperty(imp, js->fieldtype, "value", field_getValue, field_setValue);
	pdf_jsimp_addproperty(imp, js->fieldtype, "borderStyle", field_getBorderStyle, field_setBorderStyle);
	pdf_jsimp_addproperty(imp, js->fieldtype, "textColor", field_getTextColor, field_setTextColor);
	pdf_jsimp_addproperty(imp, js->fieldtype, "fillColor", field_getFillColor, field_setFillColor);
	pdf_jsimp_addproperty(imp, js->fieldtype, "display", field_getDisplay, field_setDisplay);
	pdf_jsimp_addproperty(imp, js->fieldtype, "name", field_getName, field_setName);
	pdf_jsimp_addmethod(imp, js->fieldtype, "buttonSetCaption", field_buttonSetCaption);

	/* Create the app type */
	js->apptype = pdf_jsimp_new_type(imp, NULL);
	pdf_jsimp_addmethod(imp, js->apptype, "alert", app_alert);
	pdf_jsimp_addmethod(imp, js->apptype, "execDialog", app_execDialog);
	pdf_jsimp_addmethod(imp, js->apptype, "execMenuItem", app_execMenuItem);
	pdf_jsimp_addmethod(imp, js->apptype, "launchURL", app_launchURL);

	/* Create the document object and tell the engine to use */
	pdf_jsimp_set_global_type(js->imp, js->doctype);
}

static void preload_helpers(pdf_js *js)
{
	/* When testing on the cluster, redefine the Date object
	 * to use a fixed date */
#ifdef CLUSTER
	pdf_jsimp_execute(js->imp,
"var MuPDFOldDate = Date\n"
"Date = function() { return new MuPDFOldDate(1979,5,15); }\n"
	);
#endif

	pdf_jsimp_execute(js->imp,
#include "gen_js_util.h"
	);
}

pdf_js *pdf_new_js(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_js *js = NULL;

	fz_var(js);
	fz_try(ctx)
	{
		pdf_obj *root, *acroform;

		js = fz_malloc_struct(ctx, pdf_js);
		js->doc = doc;

		/* Find the form array */
		root = pdf_dict_gets(pdf_trailer(doc), "Root");
		acroform = pdf_dict_gets(root, "AcroForm");
		js->form = pdf_dict_gets(acroform, "Fields");

		/* Initialise the javascript engine, passing the main context
		 * for use in memory allocation and exception handling. Also
		 * pass our js context, for it to pass back to us. */
		js->imp = pdf_new_jsimp(ctx, js);
		declare_dom(js);

		preload_helpers(js);
	}
	fz_catch(ctx)
	{
		pdf_drop_js(js);
		js = NULL;
	}

	return js;
}

void pdf_js_load_document_level(pdf_js *js)
{
	pdf_document *doc = js->doc;
	fz_context *ctx = doc->ctx;
	pdf_obj *javascript = NULL;
	char *codebuf = NULL;

	fz_var(javascript);
	fz_var(codebuf);
	fz_try(ctx)
	{
		int len, i;

		javascript = pdf_load_name_tree(doc, "JavaScript");
		len = pdf_dict_len(javascript);

		for (i = 0; i < len; i++)
		{
			pdf_obj *fragment = pdf_dict_get_val(javascript, i);
			pdf_obj *code = pdf_dict_gets(fragment, "JS");

			fz_var(codebuf);
			fz_try(ctx)
			{
				codebuf = pdf_to_utf8(doc, code);
				pdf_jsimp_execute(js->imp, codebuf);
			}
			fz_always(ctx)
			{
				fz_free(ctx, codebuf);
				codebuf = NULL;
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "Warning: %s", fz_caught_message(ctx));
			}
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(javascript);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_drop_js(pdf_js *js)
{
	if (js)
	{
		fz_context *ctx = js->doc->ctx;
		fz_free(ctx, js->event.value);
		pdf_jsimp_drop_type(js->imp, js->apptype);
		pdf_jsimp_drop_type(js->imp, js->fieldtype);
		pdf_jsimp_drop_type(js->imp, js->doctype);
		pdf_drop_jsimp(js->imp);
		fz_free(ctx, js);
	}
}

void pdf_js_setup_event(pdf_js *js, pdf_js_event *e)
{
	if (js)
	{
		fz_context *ctx = js->doc->ctx;
		char *ev = e->value ? e->value : "";
		char *v = fz_strdup(ctx, ev);

		fz_free(ctx, js->event.value);
		js->event.value = v;

		js->event.target = e->target;
		js->event.rc = 1;
	}
}

pdf_js_event *pdf_js_get_event(pdf_js *js)
{
	return js ? &js->event : NULL;
}

void pdf_js_execute(pdf_js *js, char *code)
{
	if (js)
	{
		fz_context *ctx = js->doc->ctx;
		fz_try(ctx)
		{
			pdf_jsimp_execute(js->imp, code);
		}
		fz_catch(ctx)
		{
		}
	}
}

void pdf_js_execute_count(pdf_js *js, char *code, int count)
{
	if (js)
	{
		fz_context *ctx = js->doc->ctx;
		fz_try(ctx)
		{
			pdf_jsimp_execute_count(js->imp, code, count);
		}
		fz_catch(ctx)
		{
		}
	}
}

int pdf_js_supported(void)
{
	return 1;
}
