#include "mupdf/pdf.h"

pdf_obj *pdf_get_inheritable(pdf_document *doc, pdf_obj *obj, char *key)
{
	pdf_obj *fobj = NULL;

	while (!fobj && obj)
	{
		fobj = pdf_dict_gets(obj, key);

		if (!fobj)
			obj = pdf_dict_gets(obj, "Parent");
	}

	return fobj ? fobj : pdf_dict_gets(pdf_dict_gets(pdf_dict_gets(pdf_trailer(doc), "Root"), "AcroForm"), key);
}

char *pdf_get_string_or_stream(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	int len = 0;
	char *buf = NULL;
	fz_buffer *strmbuf = NULL;
	char *text = NULL;

	fz_var(strmbuf);
	fz_var(text);
	fz_try(ctx)
	{
		if (pdf_is_string(obj))
		{
			len = pdf_to_str_len(obj);
			buf = pdf_to_str_buf(obj);
		}
		else if (pdf_is_stream(doc, pdf_to_num(obj), pdf_to_gen(obj)))
		{
			strmbuf = pdf_load_stream(doc, pdf_to_num(obj), pdf_to_gen(obj));
			len = fz_buffer_storage(ctx, strmbuf, (unsigned char **)&buf);
		}

		if (buf)
		{
			text = fz_malloc(ctx, len+1);
			memcpy(text, buf, len);
			text[len] = 0;
		}
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, strmbuf);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

char *pdf_field_value(pdf_document *doc, pdf_obj *field)
{
	return pdf_get_string_or_stream(doc, pdf_get_inheritable(doc, field, "V"));
}

int pdf_get_field_flags(pdf_document *doc, pdf_obj *obj)
{
	return pdf_to_int(pdf_get_inheritable(doc, obj, "Ff"));
}

static char *get_field_type_name(pdf_document *doc, pdf_obj *obj)
{
	return pdf_to_name(pdf_get_inheritable(doc, obj, "FT"));
}

int pdf_field_type(pdf_document *doc, pdf_obj *obj)
{
	char *type = get_field_type_name(doc, obj);
	int flags = pdf_get_field_flags(doc, obj);

	if (!strcmp(type, "Btn"))
	{
		if (flags & Ff_Pushbutton)
			return PDF_WIDGET_TYPE_PUSHBUTTON;
		else if (flags & Ff_Radio)
			return PDF_WIDGET_TYPE_RADIOBUTTON;
		else
			return PDF_WIDGET_TYPE_CHECKBOX;
	}
	else if (!strcmp(type, "Tx"))
		return PDF_WIDGET_TYPE_TEXT;
	else if (!strcmp(type, "Ch"))
	{
		if (flags & Ff_Combo)
			return PDF_WIDGET_TYPE_COMBOBOX;
		else
			return PDF_WIDGET_TYPE_LISTBOX;
	}
	else if (!strcmp(type, "Sig"))
		return PDF_WIDGET_TYPE_SIGNATURE;
	else
		return PDF_WIDGET_TYPE_NOT_WIDGET;
}

void pdf_set_field_type(pdf_document *doc, pdf_obj *obj, int type)
{
	int setbits = 0;
	int clearbits = 0;
	char *typename = NULL;

	switch(type)
	{
	case PDF_WIDGET_TYPE_PUSHBUTTON:
		typename = "Btn";
		setbits = Ff_Pushbutton;
		break;
	case PDF_WIDGET_TYPE_CHECKBOX:
		typename = "Btn";
		clearbits = Ff_Pushbutton;
		setbits = Ff_Radio;
		break;
	case PDF_WIDGET_TYPE_RADIOBUTTON:
		typename = "Btn";
		clearbits = (Ff_Pushbutton|Ff_Radio);
		break;
	case PDF_WIDGET_TYPE_TEXT:
		typename = "Tx";
		break;
	case PDF_WIDGET_TYPE_LISTBOX:
		typename = "Ch";
		clearbits = Ff_Combo;
		break;
	case PDF_WIDGET_TYPE_COMBOBOX:
		typename = "Ch";
		setbits = Ff_Combo;
		break;
	case PDF_WIDGET_TYPE_SIGNATURE:
		typename = "Sig";
		break;
	}

	if (typename)
		pdf_dict_puts_drop(obj, "FT", pdf_new_name(doc, typename));

	if (setbits != 0 || clearbits != 0)
	{
		int bits = pdf_to_int(pdf_dict_gets(obj, "Ff"));
		bits &= ~clearbits;
		bits |= setbits;
		pdf_dict_puts_drop(obj, "Ff", pdf_new_int(doc, bits));
	}
}
