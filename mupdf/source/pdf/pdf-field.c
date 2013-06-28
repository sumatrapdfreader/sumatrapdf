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
