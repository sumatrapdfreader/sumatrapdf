#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

/* Must be kept in sync with definitions in pdf_util.js */
enum
{
	Display_Visible,
	Display_Hidden,
	Display_NoPrint,
	Display_NoView
};

enum
{
	SigFlag_SignaturesExist = 1,
	SigFlag_AppendOnly = 2
};

const char *pdf_field_value(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *v = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
	if (pdf_is_name(ctx, v))
		return pdf_to_name(ctx, v);
	if (pdf_is_stream(ctx, v))
	{
		// FIXME: pdf_dict_put_inheritable...
		char *str = pdf_new_utf8_from_pdf_stream_obj(ctx, v);
		fz_try(ctx)
			pdf_dict_put_text_string(ctx, field, PDF_NAME(V), str);
		fz_always(ctx)
			fz_free(ctx, str);
		fz_catch(ctx)
			fz_rethrow(ctx);
		v = pdf_dict_get(ctx, field, PDF_NAME(V));
	}
	return pdf_to_text_string(ctx, v);
}

int pdf_field_flags(fz_context *ctx, pdf_obj *obj)
{
	return pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, obj, PDF_NAME(Ff)));
}

int pdf_field_type(fz_context *ctx, pdf_obj *obj)
{
	pdf_obj *type = pdf_dict_get_inheritable(ctx, obj, PDF_NAME(FT));
	int flags = pdf_field_flags(ctx, obj);
	if (pdf_name_eq(ctx, type, PDF_NAME(Btn)))
	{
		if (flags & PDF_BTN_FIELD_IS_PUSHBUTTON)
			return PDF_WIDGET_TYPE_BUTTON;
		else if (flags & PDF_BTN_FIELD_IS_RADIO)
			return PDF_WIDGET_TYPE_RADIOBUTTON;
		else
			return PDF_WIDGET_TYPE_CHECKBOX;
	}
	else if (pdf_name_eq(ctx, type, PDF_NAME(Tx)))
		return PDF_WIDGET_TYPE_TEXT;
	else if (pdf_name_eq(ctx, type, PDF_NAME(Ch)))
	{
		if (flags & PDF_CH_FIELD_IS_COMBO)
			return PDF_WIDGET_TYPE_COMBOBOX;
		else
			return PDF_WIDGET_TYPE_LISTBOX;
	}
	else if (pdf_name_eq(ctx, type, PDF_NAME(Sig)))
		return PDF_WIDGET_TYPE_SIGNATURE;
	else
		return PDF_WIDGET_TYPE_BUTTON;
}

static int pdf_field_dirties_document(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	int ff = pdf_field_flags(ctx, field);
	if (ff & PDF_FIELD_IS_NO_EXPORT) return 0;
	if (ff & PDF_FIELD_IS_READ_ONLY) return 0;
	return 1;
}

/* Find the point in a field hierarchy where all descendants
 * share the same name */
static pdf_obj *find_head_of_field_group(fz_context *ctx, pdf_obj *obj)
{
	if (obj == NULL || pdf_dict_get(ctx, obj, PDF_NAME(T)))
		return obj;
	else
		return find_head_of_field_group(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Parent)));
}

static void pdf_field_mark_dirty(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);
		for (i = 0; i < n; i++)
			pdf_field_mark_dirty(ctx, pdf_array_get(ctx, kids, i));
	}
	pdf_dirty_obj(ctx, field);
}

static void update_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *obj, const char *text)
{
	pdf_obj *grp;

	if (!text)
		text = "";

	/* All fields of the same name should be updated, so
	 * set the value at the head of the group */
	grp = find_head_of_field_group(ctx, obj);
	if (grp)
		obj = grp;

	pdf_dict_put_text_string(ctx, obj, PDF_NAME(V), text);

	pdf_field_mark_dirty(ctx, obj);
}

static pdf_obj *find_field(fz_context *ctx, pdf_obj *dict, const char *name, int len)
{
	int i, n = pdf_array_len(ctx, dict);
	for (i = 0; i < n; i++)
	{
		pdf_obj *field = pdf_array_get(ctx, dict, i);
		const char *part = pdf_dict_get_text_string(ctx, field, PDF_NAME(T));
		if (strlen(part) == (size_t)len && !memcmp(part, name, len))
			return field;
	}
	return NULL;
}

pdf_obj *pdf_lookup_field(fz_context *ctx, pdf_obj *form, const char *name)
{
	const char *dot;
	const char *namep;
	pdf_obj *dict = NULL;
	int len;

	/* Process the fully qualified field name which has
	* the partial names delimited by '.'. Pretend there
	* was a preceding '.' to simplify the loop */
	dot = name - 1;

	while (dot && form)
	{
		namep = dot + 1;
		dot = strchr(namep, '.');
		len = dot ? dot - namep : (int)strlen(namep);
		dict = find_field(ctx, form, namep, len);
		if (dot)
			form = pdf_dict_get(ctx, dict, PDF_NAME(Kids));
	}

	return dict;
}

static void reset_form_field(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	/* Set V to DV wherever DV is present, and delete V where DV is not.
	 * FIXME: we assume for now that V has not been set unequal
	 * to DV higher in the hierarchy than "field".
	 *
	 * At the bottom of the hierarchy we may find widget annotations
	 * that aren't also fields, but DV and V will not be present in their
	 * dictionaries, and attempts to remove V will be harmless. */
	pdf_obj *dv = pdf_dict_get(ctx, field, PDF_NAME(DV));
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));

	if (dv)
		pdf_dict_put(ctx, field, PDF_NAME(V), dv);
	else
		pdf_dict_del(ctx, field, PDF_NAME(V));

	if (kids == NULL)
	{
		/* The leaves of the tree are widget annotations
		 * In some cases we need to update the appearance state;
		 * in others we need to mark the field as dirty so that
		 * the appearance stream will be regenerated. */
		switch (pdf_field_type(ctx, field))
		{
		case PDF_WIDGET_TYPE_CHECKBOX:
		case PDF_WIDGET_TYPE_RADIOBUTTON:
			{
				pdf_obj *leafv = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
				if (!leafv)
					leafv = PDF_NAME(Off);
				pdf_dict_put(ctx, field, PDF_NAME(AS), leafv);
			}
			pdf_field_mark_dirty(ctx, field);
			break;

		case PDF_WIDGET_TYPE_BUTTON:
		case PDF_WIDGET_TYPE_SIGNATURE:
			/* Pushbuttons and signatures have no value to reset. */
			break;

		default:
			pdf_field_mark_dirty(ctx, field);
			break;
		}
	}

	if (pdf_field_dirties_document(ctx, doc, field))
		doc->dirty = 1;
}

void pdf_field_reset(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));

	reset_form_field(ctx, doc, field);

	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			pdf_field_reset(ctx, doc, pdf_array_get(ctx, kids, i));
	}
}

static void add_field_hierarchy_to_array(fz_context *ctx, pdf_obj *array, pdf_obj *field)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	pdf_obj *exclude = pdf_dict_get(ctx, field, PDF_NAME(Exclude));

	if (exclude)
		return;

	pdf_array_push(ctx, array, field);

	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			add_field_hierarchy_to_array(ctx, array, pdf_array_get(ctx, kids, i));
	}
}

/*
	When resetting or submitting a form, the fields to act upon are defined
	by an array of either field references or field names, plus a flag determining
	whether to act upon the fields in the array, or all fields other than those in
	the array. specified_fields interprets this information and produces the array
	of fields to be acted upon.
*/
static pdf_obj *specified_fields(fz_context *ctx, pdf_document *doc, pdf_obj *fields, int exclude)
{
	pdf_obj *form = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(Fields), NULL);
	int i, n;
	pdf_obj *result = pdf_new_array(ctx, doc, 0);

	fz_try(ctx)
	{
		/* The 'fields' array not being present signals that all fields
		* should be acted upon, so handle it using the exclude case - excluding none */
		if (exclude || !fields)
		{
			/* mark the fields we don't want to act upon */
			n = pdf_array_len(ctx, fields);
			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(ctx, fields, i);

				if (pdf_is_string(ctx, field))
					field = pdf_lookup_field(ctx, form, pdf_to_str_buf(ctx, field));

				if (field)
					pdf_dict_put(ctx, field, PDF_NAME(Exclude), PDF_NULL);
			}

			/* Act upon all unmarked fields */
			n = pdf_array_len(ctx, form);

			for (i = 0; i < n; i++)
				add_field_hierarchy_to_array(ctx, result, pdf_array_get(ctx, form, i));

			/* Unmark the marked fields */
			n = pdf_array_len(ctx, fields);

			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(ctx, fields, i);

				if (pdf_is_string(ctx, field))
					field = pdf_lookup_field(ctx, form, pdf_to_str_buf(ctx, field));

				if (field)
					pdf_dict_del(ctx, field, PDF_NAME(Exclude));
			}
		}
		else
		{
			n = pdf_array_len(ctx, fields);

			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(ctx, fields, i);

				if (pdf_is_string(ctx, field))
					field = pdf_lookup_field(ctx, form, pdf_to_str_buf(ctx, field));

				if (field)
					add_field_hierarchy_to_array(ctx, result, field);
			}
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, result);
		fz_rethrow(ctx);
	}

	return result;
}

void pdf_reset_form(fz_context *ctx, pdf_document *doc, pdf_obj *fields, int exclude)
{
	pdf_obj *sfields = specified_fields(ctx, doc, fields, exclude);
	fz_try(ctx)
	{
		int i, n = pdf_array_len(ctx, sfields);
		for (i = 0; i < n; i++)
			reset_form_field(ctx, doc, pdf_array_get(ctx, sfields, i));
		doc->recalculate = 1;
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, sfields);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void set_check(fz_context *ctx, pdf_document *doc, pdf_obj *chk, pdf_obj *name)
{
	pdf_obj *n = pdf_dict_getp(ctx, chk, "AP/N");
	pdf_obj *val;

	/* If name is a possible value of this check
	* box then use it, otherwise use "Off" */
	if (pdf_dict_get(ctx, n, name))
		val = name;
	else
		val = PDF_NAME(Off);

	pdf_dict_put(ctx, chk, PDF_NAME(AS), val);
}

/* Set the values of all fields in a group defined by a node
 * in the hierarchy */
static void set_check_grp(fz_context *ctx, pdf_document *doc, pdf_obj *grp, pdf_obj *val)
{
	pdf_obj *kids = pdf_dict_get(ctx, grp, PDF_NAME(Kids));

	if (kids == NULL)
	{
		set_check(ctx, doc, grp, val);
	}
	else
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			set_check_grp(ctx, doc, pdf_array_get(ctx, kids, i), val);
	}
}

void pdf_calculate_form(fz_context *ctx, pdf_document *doc)
{
	if (doc->js)
	{
		fz_try(ctx)
		{
			pdf_obj *co = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/CO");
			int i, n = pdf_array_len(ctx, co);
			for (i = 0; i < n; i++)
			{
				pdf_obj *field = pdf_array_get(ctx, co, i);
				pdf_field_event_calculate(ctx, doc, field);
			}
		}
		fz_always(ctx)
			doc->recalculate = 0;
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static pdf_obj *find_on_state(fz_context *ctx, pdf_obj *dict)
{
	int i, n = pdf_dict_len(ctx, dict);
	for (i = 0; i < n; ++i)
	{
		pdf_obj *key = pdf_dict_get_key(ctx, dict, i);
		if (key != PDF_NAME(Off))
			return key;
	}
	return NULL;
}

pdf_obj *pdf_button_field_on_state(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *ap = pdf_dict_get(ctx, field, PDF_NAME(AP));
	pdf_obj *on = find_on_state(ctx, pdf_dict_get(ctx, ap, PDF_NAME(N)));
	if (!on) on = find_on_state(ctx, pdf_dict_get(ctx, ap, PDF_NAME(D)));
	if (!on) on = PDF_NAME(Yes);
	return on;
}

static void toggle_check_box(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	int ff = pdf_field_flags(ctx, field);
	int is_radio = (ff & PDF_BTN_FIELD_IS_RADIO);
	int is_no_toggle_to_off = (ff & PDF_BTN_FIELD_IS_NO_TOGGLE_TO_OFF);
	pdf_obj *grp, *as, *val;

	grp = find_head_of_field_group(ctx, field);
	if (!grp)
		grp = field;

	/* TODO: check V value as well as or instead of AS? */
	as = pdf_dict_get(ctx, field, PDF_NAME(AS));
	if (as && as != PDF_NAME(Off))
	{
		if (is_radio && is_no_toggle_to_off)
			return;
		val = PDF_NAME(Off);
	}
	else
	{
		val = pdf_button_field_on_state(ctx, field);
	}

	pdf_dict_put(ctx, grp, PDF_NAME(V), val);
	set_check_grp(ctx, doc, grp, val);
	doc->recalculate = 1;
}

/*
	Determine whether changes have been made since the
	document was opened or last saved.
*/
int pdf_has_unsaved_changes(fz_context *ctx, pdf_document *doc)
{
	return doc->dirty;
}

/*
	Toggle the state of a specified annotation. Applies only to check-box
	and radio-button widgets.
*/
int pdf_toggle_widget(fz_context *ctx, pdf_widget *widget)
{
	switch (pdf_widget_type(ctx, widget))
	{
	default:
		return 0;
	case PDF_WIDGET_TYPE_CHECKBOX:
	case PDF_WIDGET_TYPE_RADIOBUTTON:
		toggle_check_box(ctx, widget->page->doc, widget->obj);
		return 1;
	}
	return 0;
}

/*
	Recalculate form fields if necessary.

	Loop through all annotations on the page and update them. Return true
	if any of them were changed (by either event or javascript actions, or
	by annotation editing) and need re-rendering.

	If you need more granularity, loop through the annotations and call
	pdf_update_annot for each one to detect changes on a per-annotation
	basis.
*/
int
pdf_update_page(fz_context *ctx, pdf_page *page)
{
	pdf_annot *annot;
	pdf_widget *widget;
	int changed = 0;

	if (page->doc->recalculate)
		pdf_calculate_form(ctx, page->doc);

	for (annot = page->annots; annot; annot = annot->next)
		if (pdf_update_annot(ctx, annot))
			changed = 1;
	for (widget = page->widgets; widget; widget = widget->next)
		if (pdf_update_annot(ctx, widget))
			changed = 1;

	return changed;
}

pdf_widget *pdf_first_widget(fz_context *ctx, pdf_page *page)
{
	return page->widgets;
}

pdf_widget *pdf_next_widget(fz_context *ctx, pdf_widget *widget)
{
	return widget->next;
}

enum pdf_widget_type pdf_widget_type(fz_context *ctx, pdf_widget *widget)
{
	pdf_obj *subtype = pdf_dict_get(ctx, widget->obj, PDF_NAME(Subtype));
	if (pdf_name_eq(ctx, subtype, PDF_NAME(Widget)))
		return pdf_field_type(ctx, widget->obj);
	return PDF_WIDGET_TYPE_BUTTON;
}

static int set_validated_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *text, int ignore_trigger_events)
{
	if (!ignore_trigger_events)
	{
		if (!pdf_field_event_validate(ctx, doc, field, text))
			return 0;
	}

	if (pdf_field_dirties_document(ctx, doc, field))
		doc->dirty = 1;
	update_field_value(ctx, doc, field, text);

	return 1;
}

static void update_checkbox_selector(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *val)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));

	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			update_checkbox_selector(ctx, doc, pdf_array_get(ctx, kids, i), val);
	}
	else
	{
		pdf_obj *n = pdf_dict_getp(ctx, field, "AP/N");
		pdf_obj *oval;

		if (pdf_dict_gets(ctx, n, val))
			oval = pdf_new_name(ctx, val);
		else
			oval = PDF_NAME(Off);
		pdf_dict_put_drop(ctx, field, PDF_NAME(AS), oval);
	}
}

static int set_checkbox_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *val)
{
	update_checkbox_selector(ctx, doc, field, val);
	update_field_value(ctx, doc, field, val);
	return 1;
}

int pdf_set_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *text, int ignore_trigger_events)
{
	int accepted = 0;

	switch (pdf_field_type(ctx, field))
	{
	case PDF_WIDGET_TYPE_TEXT:
	case PDF_WIDGET_TYPE_COMBOBOX:
	case PDF_WIDGET_TYPE_LISTBOX:
		accepted = set_validated_field_value(ctx, doc, field, text, ignore_trigger_events);
		break;

	case PDF_WIDGET_TYPE_CHECKBOX:
	case PDF_WIDGET_TYPE_RADIOBUTTON:
		accepted = set_checkbox_value(ctx, doc, field, text);
		break;

	default:
		update_field_value(ctx, doc, field, text);
		accepted = 1;
		break;
	}

	if (!ignore_trigger_events)
		doc->recalculate = 1;

	return accepted;
}

char *pdf_field_border_style(fz_context *ctx, pdf_obj *field)
{
	const char *bs = pdf_to_name(ctx, pdf_dict_getl(ctx, field, PDF_NAME(BS), PDF_NAME(S), NULL));
	switch (*bs)
	{
	case 'S': return "Solid";
	case 'D': return "Dashed";
	case 'B': return "Beveled";
	case 'I': return "Inset";
	case 'U': return "Underline";
	}
	return "Solid";
}

void pdf_field_set_border_style(fz_context *ctx, pdf_obj *field, const char *text)
{
	pdf_obj *val;

	if (!strcmp(text, "Solid"))
		val = PDF_NAME(S);
	else if (!strcmp(text, "Dashed"))
		val = PDF_NAME(D);
	else if (!strcmp(text, "Beveled"))
		val = PDF_NAME(B);
	else if (!strcmp(text, "Inset"))
		val = PDF_NAME(I);
	else if (!strcmp(text, "Underline"))
		val = PDF_NAME(U);
	else
		return;

	pdf_dict_putl_drop(ctx, field, val, PDF_NAME(BS), PDF_NAME(S), NULL);
	pdf_field_mark_dirty(ctx, field);
}

void pdf_field_set_button_caption(fz_context *ctx, pdf_obj *field, const char *text)
{
	if (pdf_field_type(ctx, field) == PDF_WIDGET_TYPE_BUTTON)
	{
		pdf_obj *val = pdf_new_text_string(ctx, text);
		pdf_dict_putl_drop(ctx, field, val, PDF_NAME(MK), PDF_NAME(CA), NULL);
		pdf_field_mark_dirty(ctx, field);
	}
}

int pdf_field_display(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *kids;
	int f, res = Display_Visible;

	/* Base response on first of children. Not ideal,
	 * but not clear how to handle children with
	 * differing values */
	while ((kids = pdf_dict_get(ctx, field, PDF_NAME(Kids))) != NULL)
		field = pdf_array_get(ctx, kids, 0);

	f = pdf_dict_get_int(ctx, field, PDF_NAME(F));

	if (f & PDF_ANNOT_IS_HIDDEN)
	{
		res = Display_Hidden;
	}
	else if (f & PDF_ANNOT_IS_PRINT)
	{
		if (f & PDF_ANNOT_IS_NO_VIEW)
			res = Display_NoView;
	}
	else
	{
		if (f & PDF_ANNOT_IS_NO_VIEW)
			res = Display_Hidden;
		else
			res = Display_NoPrint;
	}

	return res;
}

/*
 * get the field name in a char buffer that has spare room to
 * add more characters at the end.
 */
static char *get_field_name(fz_context *ctx, pdf_obj *field, int spare)
{
	char *res = NULL;
	pdf_obj *parent = pdf_dict_get(ctx, field, PDF_NAME(Parent));
	const char *lname = pdf_dict_get_text_string(ctx, field, PDF_NAME(T));
	int llen = (int)strlen(lname);

	/*
	 * If we found a name at this point in the field hierarchy
	 * then we'll need extra space for it and a dot
	 */
	if (llen)
		spare += llen+1;

	if (parent)
	{
		res = get_field_name(ctx, parent, spare);
	}
	else
	{
		res = Memento_label(fz_malloc(ctx, spare+1), "form_field_name");
		res[0] = 0;
	}

	if (llen)
	{
		if (res[0])
			strcat(res, ".");

		strcat(res, lname);
	}

	return res;
}

/* Note: This function allocates a string for the return value that you must free manually. */
char *pdf_field_name(fz_context *ctx, pdf_obj *field)
{
	return get_field_name(ctx, field, 0);
}

const char *pdf_field_label(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *label = pdf_dict_get_inheritable(ctx, field, PDF_NAME(TU));
	if (!label)
		label = pdf_dict_get_inheritable(ctx, field, PDF_NAME(T));
	if (label)
		return pdf_to_text_string(ctx, label);
	return "Unnamed";
}

void pdf_field_set_display(fz_context *ctx, pdf_obj *field, int d)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));

	if (!kids)
	{
		int mask = (PDF_ANNOT_IS_HIDDEN|PDF_ANNOT_IS_PRINT|PDF_ANNOT_IS_NO_VIEW);
		int f = pdf_dict_get_int(ctx, field, PDF_NAME(F)) & ~mask;
		pdf_obj *fo;

		switch (d)
		{
		case Display_Visible:
			f |= PDF_ANNOT_IS_PRINT;
			break;
		case Display_Hidden:
			f |= PDF_ANNOT_IS_HIDDEN;
			break;
		case Display_NoView:
			f |= (PDF_ANNOT_IS_PRINT|PDF_ANNOT_IS_NO_VIEW);
			break;
		case Display_NoPrint:
			break;
		}

		fo = pdf_new_int(ctx, f);
		pdf_dict_put_drop(ctx, field, PDF_NAME(F), fo);
	}
	else
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			pdf_field_set_display(ctx, pdf_array_get(ctx, kids, i), d);
	}
}

void pdf_field_set_fill_color(fz_context *ctx, pdf_obj *field, pdf_obj *col)
{
	/* col == NULL mean transparent, but we can simply pass it on as with
	 * non-NULL values because pdf_dict_putp interprets a NULL value as
	 * delete */
	pdf_dict_putl(ctx, field, col, PDF_NAME(MK), PDF_NAME(BG), NULL);
	pdf_field_mark_dirty(ctx, field);
}

void pdf_field_set_text_color(fz_context *ctx, pdf_obj *field, pdf_obj *col)
{
	char buf[100];
	const char *font;
	float size, color[3], black;
	const char *da = pdf_to_str_buf(ctx, pdf_dict_get_inheritable(ctx, field, PDF_NAME(DA)));

	pdf_parse_default_appearance(ctx, da, &font, &size, color);

	switch (pdf_array_len(ctx, col))
	{
	default:
		color[0] = color[1] = color[2] = 0;
		break;
	case 1:
		color[0] = color[1] = color[2] = pdf_array_get_real(ctx, col, 0);
		break;
	case 3:
		color[0] = pdf_array_get_real(ctx, col, 0);
		color[1] = pdf_array_get_real(ctx, col, 1);
		color[2] = pdf_array_get_real(ctx, col, 2);
		break;
	case 4:
		black = pdf_array_get_real(ctx, col, 3);
		color[0] = 1 - fz_min(1, pdf_array_get_real(ctx, col, 0) + black);
		color[1] = 1 - fz_min(1, pdf_array_get_real(ctx, col, 1) + black);
		color[2] = 1 - fz_min(1, pdf_array_get_real(ctx, col, 2) + black);
		break;
	}

	pdf_print_default_appearance(ctx, buf, sizeof buf, font, size, color);
	pdf_dict_put_string(ctx, field, PDF_NAME(DA), buf, strlen(buf));
	pdf_field_mark_dirty(ctx, field);
}

pdf_widget *
pdf_keep_widget(fz_context *ctx, pdf_widget *widget)
{
	return pdf_keep_annot(ctx, widget);
}

void
pdf_drop_widget(fz_context *ctx, pdf_widget *widget)
{
	pdf_drop_annot(ctx, widget);
}

void
pdf_drop_widgets(fz_context *ctx, pdf_widget *widget)
{
	while (widget)
	{
		pdf_widget *next = widget->next;
		pdf_drop_widget(ctx, widget);
		widget = next;
	}
}

fz_rect
pdf_bound_widget(fz_context *ctx, pdf_widget *widget)
{
	return pdf_bound_annot(ctx, widget);
}

int
pdf_update_widget(fz_context *ctx, pdf_widget *widget)
{
	return pdf_update_annot(ctx, widget);
}

/*
	get the maximum number of
	characters permitted in a text widget
*/
int pdf_text_widget_max_len(fz_context *ctx, pdf_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;
	return pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(MaxLen)));
}

/*
	get the type of content
	required by a text widget
*/
int pdf_text_widget_format(fz_context *ctx, pdf_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;
	int type = PDF_WIDGET_TX_FORMAT_NONE;
	pdf_obj *js = pdf_dict_getl(ctx, annot->obj, PDF_NAME(AA), PDF_NAME(F), PDF_NAME(JS), NULL);
	if (js)
	{
		char *code = pdf_load_stream_or_string_as_utf8(ctx, js);
		if (strstr(code, "AFNumber_Format"))
			type = PDF_WIDGET_TX_FORMAT_NUMBER;
		else if (strstr(code, "AFSpecial_Format"))
			type = PDF_WIDGET_TX_FORMAT_SPECIAL;
		else if (strstr(code, "AFDate_FormatEx"))
			type = PDF_WIDGET_TX_FORMAT_DATE;
		else if (strstr(code, "AFTime_FormatEx"))
			type = PDF_WIDGET_TX_FORMAT_TIME;
		fz_free(ctx, code);
	}

	return type;
}

/*
	Update the text of a text widget.
	The text is first validated by the Field/Keystroke event processing and accepted only if it passes.
	The function returns whether validation passed.
*/
int pdf_set_text_field_value(fz_context *ctx, pdf_widget *widget, const char *new_value)
{
	pdf_document *doc = widget->page->doc;
	pdf_keystroke_event event;
	char *newChange = NULL;
	int rc = 1;

	event.newChange = NULL;

	fz_var(newChange);
	fz_var(event.newChange);
	fz_try(ctx)
	{
		if (!widget->ignore_trigger_events)
		{
			event.value = pdf_field_value(ctx, widget->obj);
			event.change = new_value;
			event.selStart = 0;
			event.selEnd = strlen(event.value);
			event.willCommit = 0;
			rc = pdf_field_event_keystroke(ctx, doc, widget->obj, &event);
			if (rc)
			{
				if (event.newChange)
					event.value = newChange = event.newChange;
				else
					event.value = new_value;
				event.change = "";
				event.selStart = -1;
				event.selEnd = -1;
				event.willCommit = 1;
				event.newChange = NULL;
				rc = pdf_field_event_keystroke(ctx, doc, widget->obj, &event);
				if (rc)
					rc = pdf_set_field_value(ctx, doc, widget->obj, event.value, 0);
			}
		}
		else
		{
			rc = pdf_set_field_value(ctx, doc, widget->obj, new_value, 1);
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, newChange);
		fz_free(ctx, event.newChange);
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "could not set widget text");
		rc = 0;
	}
	return rc;
}

int pdf_set_choice_field_value(fz_context *ctx, pdf_widget *widget, const char *new_value)
{
	/* Choice widgets use almost the same keystroke processing as text fields. */
	return pdf_set_text_field_value(ctx, widget, new_value);
}

/*
	get the list of options for a list
	box or combo box. Returns the number of options and fills in their
	names within the supplied array. Should first be called with a
	NULL array to find out how big the array should be.  If exportval
	is true, then the export values will be returned and not the list
	values if there are export values present.
*/
int pdf_choice_widget_options(fz_context *ctx, pdf_widget *tw, int exportval, const char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr;
	int i, n, m;

	optarr = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(Opt));
	n = pdf_array_len(ctx, optarr);

	if (opts)
	{
		for (i = 0; i < n; i++)
		{
			m = pdf_array_len(ctx, pdf_array_get(ctx, optarr, i));
			/* If it is a two element array, the second item is the one that we want if we want the listing value. */
			if (m == 2)
				if (exportval)
					opts[i] = pdf_array_get_text_string(ctx, pdf_array_get(ctx, optarr, i), 0);
				else
					opts[i] = pdf_array_get_text_string(ctx, pdf_array_get(ctx, optarr, i), 1);
			else
				opts[i] = pdf_array_get_text_string(ctx, optarr, i);
		}
	}
	return n;
}

int pdf_choice_field_option_count(fz_context *ctx, pdf_obj *field)
{
	pdf_obj *opt = pdf_dict_get_inheritable(ctx, field, PDF_NAME(Opt));
	return pdf_array_len(ctx, opt);
}

const char *pdf_choice_field_option(fz_context *ctx, pdf_obj *field, int export, int i)
{
	pdf_obj *opt = pdf_dict_get_inheritable(ctx, field, PDF_NAME(Opt));
	pdf_obj *ent = pdf_array_get(ctx, opt, i);
	if (pdf_array_len(ctx, ent) == 2)
		return pdf_array_get_text_string(ctx, ent, export ? 0 : 1);
	else
		return pdf_to_text_string(ctx, ent);
}

int pdf_choice_widget_is_multiselect(fz_context *ctx, pdf_widget *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;

	if (!annot) return 0;

	switch (pdf_field_type(ctx, annot->obj))
	{
	case PDF_WIDGET_TYPE_LISTBOX:
		return (pdf_field_flags(ctx, annot->obj) & PDF_CH_FIELD_IS_MULTI_SELECT) != 0;
	default:
		return 0;
	}
}

/*
	get the value of a choice widget.
	Returns the number of options currently selected and fills in
	the supplied array with their strings. Should first be called
	with NULL as the array to find out how big the array need to
	be. The filled in elements should not be freed by the caller.
*/
int pdf_choice_widget_value(fz_context *ctx, pdf_widget *tw, const char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr;
	int i, n;

	if (!annot)
		return 0;

	optarr = pdf_dict_get(ctx, annot->obj, PDF_NAME(V));

	if (pdf_is_string(ctx, optarr))
	{
		if (opts)
			opts[0] = pdf_to_text_string(ctx, optarr);
		return 1;
	}
	else
	{
		n = pdf_array_len(ctx, optarr);
		if (opts)
		{
			for (i = 0; i < n; i++)
			{
				pdf_obj *elem = pdf_array_get(ctx, optarr, i);
				if (pdf_is_array(ctx, elem))
					elem = pdf_array_get(ctx, elem, 1);
				opts[i] = pdf_to_text_string(ctx, elem);
			}
		}
		return n;
	}
}

/*
	set the value of a choice widget. The
	caller should pass the number of options selected and an
	array of their names
*/
void pdf_choice_widget_set_value(fz_context *ctx, pdf_widget *tw, int n, const char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr = NULL, *opt;
	int i;

	if (!annot)
		return;

	fz_var(optarr);
	fz_try(ctx)
	{
		if (n != 1)
		{
			optarr = pdf_new_array(ctx, annot->page->doc, n);

			for (i = 0; i < n; i++)
			{
				opt = pdf_new_text_string(ctx, opts[i]);
				pdf_array_push_drop(ctx, optarr, opt);
			}

			pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(V), optarr);
		}
		else
		{
			opt = pdf_new_text_string(ctx, opts[0]);
			pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(V), opt);
		}

		/* FIXME: when n > 1, we should be regenerating the indexes */
		pdf_dict_del(ctx, annot->obj, PDF_NAME(I));

		pdf_field_mark_dirty(ctx, annot->obj);
		if (pdf_field_dirties_document(ctx, annot->page->doc, annot->obj))
			annot->page->doc->dirty = 1;
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, optarr);
		fz_rethrow(ctx);
	}
}

int pdf_signature_byte_range(fz_context *ctx, pdf_document *doc, pdf_obj *signature, fz_range *byte_range)
{
	pdf_obj *br = pdf_dict_getl(ctx, signature, PDF_NAME(V), PDF_NAME(ByteRange), NULL);
	int i, n = pdf_array_len(ctx, br)/2;

	if (byte_range)
	{
		for (i = 0; i < n; i++)
		{
			int64_t offset = pdf_array_get_int(ctx, br, 2*i);
			int length = pdf_array_get_int(ctx, br, 2*i+1);

			if (offset < 0 || offset > doc->file_size)
				fz_throw(ctx, FZ_ERROR_GENERIC, "offset of signature byte range outside of file");
			else if (length < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "length of signature byte range negative");
			else if (offset + length > doc->file_size)
				fz_throw(ctx, FZ_ERROR_GENERIC, "signature byte range extends past end of file");

			byte_range[i].offset = offset;
			byte_range[i].length = length;
		}
	}

	return n;
}

static int is_white(int c)
{
	return c == '\x00' || c == '\x09' || c == '\x0a' || c == '\x0c' || c == '\x0d' || c == '\x20';
}

static int is_hex_or_white(int c)
{
	return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9') || is_white(c);
}

static void validate_certificate_data(fz_context *ctx, pdf_document *doc, fz_range *hole)
{
	fz_stream *stm;
	int c;

	stm = fz_open_range_filter(ctx, doc->file, hole, 1);
	fz_try(ctx)
	{
		while (is_white((c = fz_read_byte(ctx, stm))))
			;

		if (c == '<')
			c = fz_read_byte(ctx, stm);

		while (is_hex_or_white((c = fz_read_byte(ctx, stm))))
			;

		if (c == '>')
			c = fz_read_byte(ctx, stm);

		while (is_white((c = fz_read_byte(ctx, stm))))
			;

		if (c != EOF)
			fz_throw(ctx, FZ_ERROR_GENERIC, "signature certificate data contains invalid character");
		if ((size_t)fz_tell(ctx, stm) != hole->length)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of signature certificate data");
	}
	fz_always(ctx)
		fz_drop_stream(ctx, stm);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int rangecmp(const void *a_, const void *b_)
{
	const fz_range *a = (const fz_range *) a_;
	const fz_range *b = (const fz_range *) b_;
	return (int) (a->offset - b->offset);
}

static void validate_byte_ranges(fz_context *ctx, pdf_document *doc, fz_range *unsorted, int nranges)
{
	int64_t offset = 0;
	fz_range *sorted;
	int i;

	sorted = fz_calloc(ctx, nranges, sizeof(*sorted));
	memcpy(sorted, unsorted, nranges * sizeof(*sorted));
	qsort(sorted, nranges, sizeof(*sorted), rangecmp);

	fz_try(ctx)
	{
		offset = 0;

		for (i = 0; i < nranges; i++)
		{
			if (sorted[i].offset > offset)
			{
				fz_range hole;

				hole.offset = offset;
				hole.length = sorted[i].offset - offset;

				validate_certificate_data(ctx, doc, &hole);
			}

			offset = fz_maxi64(offset, sorted[i].offset + sorted[i].length);
		}
	}
	fz_always(ctx)
		fz_free(ctx, sorted);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	retrieve an fz_stream to read the bytes hashed for the signature
*/
fz_stream *pdf_signature_hash_bytes(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	fz_range *byte_range = NULL;
	int byte_range_len;
	fz_stream *bytes = NULL;

	fz_var(byte_range);
	fz_try(ctx)
	{
		byte_range_len = pdf_signature_byte_range(ctx, doc, signature, NULL);
		if (byte_range_len)
		{
			byte_range = fz_calloc(ctx, byte_range_len, sizeof(*byte_range));
			pdf_signature_byte_range(ctx, doc, signature, byte_range);
		}

		validate_byte_ranges(ctx, doc, byte_range, byte_range_len);
		bytes = fz_open_range_filter(ctx, doc->file, byte_range, byte_range_len);
	}
	fz_always(ctx)
	{
		fz_free(ctx, byte_range);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return bytes;
}

int pdf_signature_incremental_change_since_signing(fz_context *ctx, pdf_document *doc, pdf_obj *signature)
{
	fz_range *byte_range = NULL;
	int byte_range_len;
	int changed = 0;

	fz_var(byte_range);
	fz_try(ctx)
	{
		byte_range_len = pdf_signature_byte_range(ctx, doc, signature, NULL);
		if (byte_range_len)
		{
			fz_range *last_range;
			int64_t end_of_range;

			byte_range = fz_calloc(ctx, byte_range_len, sizeof(*byte_range));
			pdf_signature_byte_range(ctx, doc, signature, byte_range);

			last_range = &byte_range[byte_range_len -1];
			end_of_range = last_range->offset + last_range->length;

			/* We can see how long the document was when signed by inspecting the byte
			 * ranges of the signature.  The document, when read in, may have already
			 * had changes tagged on to it, past its extent when signed, or we may have
			 * made changes since reading it, which will be held in a new incremental
			 * xref section. */
			if (doc->file_size > end_of_range || doc->num_incremental_sections > 0)
				changed = 1;
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, byte_range);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return changed;
}

int pdf_signature_is_signed(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_obj *v;

	if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) != PDF_NAME(Sig))
		return 0;
	/* Signatures can only be signed if the value is a /Sig field. */
	v = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
	return pdf_name_eq(ctx, pdf_dict_get(ctx, v, PDF_NAME(Type)), PDF_NAME(Sig));
}

/* NOTE: contents is allocated and must be freed by the caller! */
size_t pdf_signature_contents(fz_context *ctx, pdf_document *doc, pdf_obj *signature, char **contents)
{
	pdf_obj *v_ref = pdf_dict_get_inheritable(ctx, signature, PDF_NAME(V));
	pdf_obj *v_obj = pdf_load_unencrypted_object(ctx, doc, pdf_to_num(ctx, v_ref));
	char *copy = NULL;
	size_t len;

	fz_var(copy);
	fz_try(ctx)
	{
		pdf_obj *c = pdf_dict_get(ctx, v_obj, PDF_NAME(Contents));
		char *s;

		s = pdf_to_str_buf(ctx, c);
		len = pdf_to_str_len(ctx, c);

		if (contents)
		{
			copy = Memento_label(fz_malloc(ctx, len), "sig_contents");
			memcpy(copy, s, len);
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, v_obj);
	fz_catch(ctx)
	{
		fz_free(ctx, copy);
		fz_rethrow(ctx);
	}

	if (contents)
		*contents = copy;
	return len;
}

void pdf_signature_set_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_pkcs7_signer *signer, int64_t stime)
{
	pdf_obj *v = NULL;
	pdf_obj *o = NULL;
	pdf_obj *r = NULL;
	pdf_obj *t = NULL;
	pdf_obj *a = NULL;
	pdf_obj *indv;
	int vnum;
	size_t max_digest_size;
	char *buf = NULL;
	char date_string[40];

	vnum = pdf_create_object(ctx, doc);
	indv = pdf_new_indirect(ctx, doc, vnum, 0);
	pdf_dict_put_drop(ctx, field, PDF_NAME(V), indv);

	max_digest_size = signer->max_digest_size(signer);

	fz_var(v);
	fz_var(o);
	fz_var(r);
	fz_var(t);
	fz_var(a);
	fz_var(buf);
	fz_try(ctx)
	{
		v = pdf_new_dict(ctx, doc, 4);
		pdf_update_object(ctx, doc, vnum, v);

		buf = fz_calloc(ctx, max_digest_size, 1);

		/* Ensure that the /Filter entry is the first entry in the
		   dictionary after the digest contents since we look for
		   this tag when completing signatures in pdf-write.c in order
		   to generate the correct byte range. */
		pdf_dict_put_array(ctx, v, PDF_NAME(ByteRange), 4);
		pdf_dict_put_string(ctx, v, PDF_NAME(Contents), buf, max_digest_size);
		pdf_dict_put(ctx, v, PDF_NAME(Filter), PDF_NAME(Adobe_PPKLite));
		pdf_dict_put(ctx, v, PDF_NAME(SubFilter), PDF_NAME(adbe_pkcs7_detached));
		pdf_dict_put(ctx, v, PDF_NAME(Type), PDF_NAME(Sig));
		pdf_format_date(ctx, date_string, sizeof date_string, stime);
		pdf_dict_put_text_string(ctx, v, PDF_NAME(M), date_string);

		o = pdf_new_array(ctx, doc, 1);
		pdf_dict_put(ctx, v, PDF_NAME(Reference), o);
		r = pdf_new_dict(ctx, doc, 4);
		pdf_array_put(ctx, o, 0, r);
		pdf_dict_put(ctx, r, PDF_NAME(Data), pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)));
		pdf_dict_put(ctx, r, PDF_NAME(TransformMethod), PDF_NAME(FieldMDP));
		pdf_dict_put(ctx, r, PDF_NAME(Type), PDF_NAME(SigRef));
		t = pdf_new_dict(ctx, doc, 5);
		pdf_dict_put(ctx, r, PDF_NAME(TransformParams), t);
		pdf_dict_put(ctx, t, PDF_NAME(Action), pdf_dict_getp(ctx, field, "Lock/Action"));
		a = pdf_dict_getp(ctx, field, "Lock/Fields");
		if (a)
			pdf_dict_put_drop(ctx, t, PDF_NAME(Fields), pdf_copy_array(ctx, a));
		pdf_dict_put(ctx, t, PDF_NAME(Type), PDF_NAME(TransformParams));
		pdf_dict_put(ctx, t, PDF_NAME(V), PDF_NAME(1_2));

		/* Record details within the document structure so that contents
		* and byte_range can be updated with their correct values at
		* saving time */
		pdf_xref_store_unsaved_signature(ctx, doc, field, signer);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, v);
		pdf_drop_obj(ctx, o);
		pdf_drop_obj(ctx, r);
		pdf_drop_obj(ctx, t);
		fz_free(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
	Update internal state appropriate for editing this field. When editing
	is true, updating the text of the text widget will not have any
	side-effects such as changing other widgets or running javascript.
	This state is intended for the period when a text widget is having
	characters typed into it. The state should be reverted at the end of
	the edit sequence and the text newly updated.
*/
void pdf_set_widget_editing_state(fz_context *ctx, pdf_widget *widget, int editing)
{
	widget->ignore_trigger_events = editing;
}

int pdf_get_widget_editing_state(fz_context *ctx, pdf_widget *widget)
{
	return widget->ignore_trigger_events;
}

static void pdf_execute_js_action(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path, pdf_obj *js)
{
	if (js)
	{
		char *code = pdf_load_stream_or_string_as_utf8(ctx, js);
		fz_try(ctx)
		{
			char buf[100];
			fz_snprintf(buf, sizeof buf, "%d/%s", pdf_to_num(ctx, target), path);
			pdf_js_execute(doc->js, buf, code);
		}
		fz_always(ctx)
			fz_free(ctx, code);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static void pdf_execute_action_imp(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path, pdf_obj *action)
{
	pdf_obj *S = pdf_dict_get(ctx, action, PDF_NAME(S));
	if (pdf_name_eq(ctx, S, PDF_NAME(JavaScript)))
	{
		if (doc->js)
			pdf_execute_js_action(ctx, doc, target, path, pdf_dict_get(ctx, action, PDF_NAME(JS)));
	}
	if (pdf_name_eq(ctx, S, PDF_NAME(ResetForm)))
	{
		pdf_obj *fields = pdf_dict_get(ctx, action, PDF_NAME(Fields));
		int flags = pdf_dict_get_int(ctx, action, PDF_NAME(Flags));
		pdf_reset_form(ctx, doc, fields, flags & 1);
	}
}

static void pdf_execute_action_chain(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path, pdf_obj *action)
{
	pdf_obj *next;

	if (pdf_mark_obj(ctx, action))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in action chain");
	fz_try(ctx)
	{
		if (pdf_is_array(ctx, action))
		{
			int i, n = pdf_array_len(ctx, action);
			for (i = 0; i < n; ++i)
				pdf_execute_action_chain(ctx, doc, target, path, pdf_array_get(ctx, action, i));
		}
		else
		{
			pdf_execute_action_imp(ctx, doc, target, path, action);
			next = pdf_dict_get(ctx, action, PDF_NAME(Next));
			if (next)
				pdf_execute_action_chain(ctx, doc, target, path, next);
		}
	}
	fz_always(ctx)
		pdf_unmark_obj(ctx, action);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void pdf_execute_action(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path)
{
	pdf_obj *action = pdf_dict_getp(ctx, target, path);
	if (action)
		pdf_execute_action_chain(ctx, doc, target, path, action);
}

void pdf_document_event_will_close(fz_context *ctx, pdf_document *doc)
{
	pdf_execute_action(ctx, doc, pdf_trailer(ctx, doc), "Root/AA/WC");
}

void pdf_document_event_will_save(fz_context *ctx, pdf_document *doc)
{
	pdf_execute_action(ctx, doc, pdf_trailer(ctx, doc), "Root/AA/WS");
}

void pdf_document_event_did_save(fz_context *ctx, pdf_document *doc)
{
	pdf_execute_action(ctx, doc, pdf_trailer(ctx, doc), "Root/AA/DS");
}

void pdf_document_event_will_print(fz_context *ctx, pdf_document *doc)
{
	pdf_execute_action(ctx, doc, pdf_trailer(ctx, doc), "Root/AA/WP");
}

void pdf_document_event_did_print(fz_context *ctx, pdf_document *doc)
{
	pdf_execute_action(ctx, doc, pdf_trailer(ctx, doc), "Root/AA/DP");
}

void pdf_page_event_open(fz_context *ctx, pdf_page *page)
{
	pdf_execute_action(ctx, page->doc, page->obj, "AA/O");
}

void pdf_page_event_close(fz_context *ctx, pdf_page *page)
{
	pdf_execute_action(ctx, page->doc, page->obj, "AA/C");
}

void pdf_annot_event_enter(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/E");
}

void pdf_annot_event_exit(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/X");
}

void pdf_annot_event_down(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/D");
}

void pdf_annot_event_up(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *action = pdf_dict_get(ctx, annot->obj, PDF_NAME(A));
	if (action)
		pdf_execute_action_chain(ctx, annot->page->doc, annot->obj, "A", action);
	else
		pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/U");
}

void pdf_annot_event_focus(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/Fo");
}

void pdf_annot_event_blur(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/Bl");
}

void pdf_annot_event_page_open(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/PO");
}

void pdf_annot_event_page_close(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/PC");
}

void pdf_annot_event_page_visible(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/PV");
}

void pdf_annot_event_page_invisible(fz_context *ctx, pdf_annot *annot)
{
	pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/PI");
}

int pdf_field_event_keystroke(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_keystroke_event *evt)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp(ctx, field, "AA/K/JS");
		if (action)
		{
			pdf_js_event_init_keystroke(js, field, evt);
			pdf_execute_js_action(ctx, doc, field, "AA/K/JS", action);
			return pdf_js_event_result_keystroke(js, evt);
		}
	}
	return 1;
}

char *pdf_field_event_format(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp(ctx, field, "AA/F/JS");
		if (action)
		{
			const char *value = pdf_field_value(ctx, field);
			pdf_js_event_init(js, field, value, 1);
			pdf_execute_js_action(ctx, doc, field, "AA/F/JS", action);
			return pdf_js_event_value(js);
		}
	}
	return NULL;
}

int pdf_field_event_validate(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *value)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp(ctx, field, "AA/V/JS");
		if (action)
		{
			pdf_js_event_init(js, field, value, 1);
			pdf_execute_js_action(ctx, doc, field, "AA/V/JS", action);
			return pdf_js_event_result(js);
		}
	}
	return 1;
}

void pdf_field_event_calculate(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp(ctx, field, "AA/C/JS");
		if (action)
		{
			char *old_value = fz_strdup(ctx, pdf_field_value(ctx, field));
			char *new_value = NULL;
			fz_var(new_value);
			fz_try(ctx)
			{
				pdf_js_event_init(js, field, old_value, 1);
				pdf_execute_js_action(ctx, doc, field, "AA/C/JS", action);
				if (pdf_js_event_result(js))
				{
					char *new_value = pdf_js_event_value(js);
					if (strcmp(old_value, new_value))
						pdf_set_field_value(ctx, doc, field, new_value, 0);
				}
			}
			fz_always(ctx)
			{
				fz_free(ctx, old_value);
				fz_free(ctx, new_value);
			}
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}
}

static void
count_sigs(fz_context *ctx, pdf_obj *field, void *arg, pdf_obj **ft)
{
	int *n = (int *)arg;

	if (!pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Type)), PDF_NAME(Annot)) ||
		!pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Subtype)), PDF_NAME(Widget)) ||
		!pdf_name_eq(ctx, *ft, PDF_NAME(Sig)))
		return;

	(*n)++;
}

static pdf_obj *ft_name[2] = { PDF_NAME(FT), NULL };

int pdf_count_signatures(fz_context *ctx, pdf_document *doc)
{
	int n = 0;
	pdf_obj *ft = NULL;
	pdf_obj *form_fields = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");
	pdf_walk_tree(ctx, form_fields, PDF_NAME(Kids), count_sigs, NULL, &n, ft_name, &ft);
	return n;
}
