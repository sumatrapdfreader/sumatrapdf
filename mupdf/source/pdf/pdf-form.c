// Copyright (C) 2004-2022 Artifex Software, Inc.
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
#include "pdf-annot-imp.h"

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
	return pdf_dict_get_inheritable_int(ctx, obj, PDF_NAME(Ff));
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

const char *pdf_field_type_string(fz_context *ctx, pdf_obj *obj)
{
	switch (pdf_field_type(ctx, obj))
	{
	default:
	case PDF_WIDGET_TYPE_BUTTON: return "button";
	case PDF_WIDGET_TYPE_CHECKBOX: return "checkbox";
	case PDF_WIDGET_TYPE_COMBOBOX: return "combobox";
	case PDF_WIDGET_TYPE_LISTBOX: return "listbox";
	case PDF_WIDGET_TYPE_RADIOBUTTON: return "radiobutton";
	case PDF_WIDGET_TYPE_SIGNATURE: return "signature";
	case PDF_WIDGET_TYPE_TEXT: return "text";
	}
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
	pdf_document *doc = pdf_get_bound_document(ctx, field);
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);
		for (i = 0; i < n; i++)
			pdf_field_mark_dirty(ctx, pdf_array_get(ctx, kids, i));
	}
	pdf_dirty_obj(ctx, field);
	if (doc)
		doc->resynth_required = 1;
}

static void update_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *obj, const char *text)
{
	const char *old_text;
	pdf_obj *grp;

	if (!text)
		text = "";

	/* All fields of the same name should be updated, so
	 * set the value at the head of the group */
	grp = find_head_of_field_group(ctx, obj);
	if (grp)
		obj = grp;

	/* Only update if we change the actual value. */
	old_text = pdf_dict_get_text_string(ctx, obj, PDF_NAME(V));
	if (old_text && !strcmp(old_text, text))
		return;

	pdf_dict_put_text_string(ctx, obj, PDF_NAME(V), text);

	pdf_field_mark_dirty(ctx, obj);
}

static pdf_obj *
pdf_lookup_field_imp(fz_context *ctx, pdf_obj *arr, const char *str, pdf_cycle_list *cycle_up);

static pdf_obj *
lookup_field_sub(fz_context *ctx, pdf_obj *dict, const char *str, pdf_cycle_list *cycle_up)
{
	pdf_obj *kids;
	pdf_obj *name;

	name = pdf_dict_get(ctx, dict, PDF_NAME(T));

	/* If we have a name, check it matches. If it matches, consume that
	 * portion of str. If not, exit. */
	if (name)
	{
		const char *match = pdf_to_text_string(ctx, name);
		const char *e = str;
		size_t len;
		while (*e && *e != '.')
			e++;
		len = e-str;
		if (strncmp(str, match, len) != 0 || (match[len] != 0 && match[len] != '.'))
			/* name doesn't match. */
			return NULL;
		str = e;
		if (*str == '.')
			str++;
	}

	/* If there is a kids array, but the search string is not empty, we have
	encountered an internal field which represents a set of terminal fields. */

	/* If there is a kids array and the search string is not empty,
	walk those looking for the appropriate one. */
	kids = pdf_dict_get(ctx, dict, PDF_NAME(Kids));
	if (kids && *str != 0)
		return pdf_lookup_field_imp(ctx, kids, str, cycle_up);

	/* The field may be a terminal or an internal field at this point.
	Accept it as the match if the match string is exhausted. */
	if (*str == 0)
		return dict;

	return NULL;
}

static pdf_obj *
pdf_lookup_field_imp(fz_context *ctx, pdf_obj *arr, const char *str, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	int len = pdf_array_len(ctx, arr);
	int i;

	for (i = 0; i < len; i++)
	{
		pdf_obj *k = pdf_array_get(ctx, arr, i);
		pdf_obj *found;
		if (pdf_cycle(ctx, &cycle, cycle_up, k))
			fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in fields");
		found = lookup_field_sub(ctx, k, str, &cycle);
		if (found)
			return found;
	}

	return NULL;
}

pdf_obj *
pdf_lookup_field(fz_context *ctx, pdf_obj *arr, const char *str)
{
	return pdf_lookup_field_imp(ctx, arr, str, NULL);
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
				pdf_obj *ap = pdf_dict_get(ctx, field, PDF_NAME(AP));
				pdf_obj *n = pdf_dict_get(ctx, ap, PDF_NAME(N));

				/* Value does not refer to any appearance state in the
				normal appearance stream dictionary, default to Off instead. */
				if (pdf_is_dict(ctx, n) && !pdf_dict_get(ctx, n, leafv))
					leafv = NULL;

				if (!pdf_is_name(ctx, leafv))
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

static void add_field_hierarchy_to_array(fz_context *ctx, pdf_obj *array, pdf_obj *field, pdf_obj *fields, int exclude)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	char *needle = pdf_load_field_name(ctx, field);
	int i, n;

	fz_try(ctx)
	{
		n = pdf_array_len(ctx, fields);
		for (i = 0; i < n; i++)
		{
			char *name = pdf_load_field_name(ctx, pdf_array_get(ctx, fields, i));
			int found = !strcmp(needle, name);
			fz_free(ctx, name);
			if (found)
				break;
		}
	}
	fz_always(ctx)
		fz_free(ctx, needle);
	fz_catch(ctx)
		fz_rethrow(ctx);

	if ((exclude && i < n) || (!exclude && i == n))
		return;

	pdf_array_push(ctx, array, field);

	if (kids)
	{
		int i, n = pdf_array_len(ctx, kids);

		for (i = 0; i < n; i++)
			add_field_hierarchy_to_array(ctx, array, pdf_array_get(ctx, kids, i), fields, exclude);
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
		n = pdf_array_len(ctx, fields);

		for (i = 0; i < n; i++)
		{
			pdf_obj *field = pdf_array_get(ctx, fields, i);

			if (pdf_is_string(ctx, field))
				field = pdf_lookup_field(ctx, form, pdf_to_str_buf(ctx, field));

			if (field)
				add_field_hierarchy_to_array(ctx, result, field, fields, exclude);
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

typedef struct
{
	pdf_obj *pageobj;
	pdf_obj *chk;
} lookup_state;

static void *find_widget_on_page(fz_context *ctx, fz_page *page_, void *state_)
{
	lookup_state *state = (lookup_state *) state_;
	pdf_page *page = (pdf_page *) page_;
	pdf_annot *widget;

	if (state->pageobj && pdf_objcmp_resolve(ctx, state->pageobj, page->obj))
		return NULL;

	for (widget = pdf_first_widget(ctx, page); widget != NULL; widget = pdf_next_widget(ctx, widget))
	{
		if (!pdf_objcmp_resolve(ctx, state->chk, widget->obj))
			return widget;
	}

	return NULL;
}

static pdf_annot *find_widget(fz_context *ctx, pdf_document *doc, pdf_obj *chk)
{
	lookup_state state;

	state.pageobj = pdf_dict_get(ctx, chk, PDF_NAME(P));
	state.chk = chk;

	return fz_process_opened_pages(ctx, (fz_document *) doc, find_widget_on_page, &state);
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

	if (pdf_name_eq(ctx, pdf_dict_get(ctx, chk, PDF_NAME(AS)), val))
		return;

	pdf_dict_put(ctx, chk, PDF_NAME(AS), val);
	pdf_set_annot_has_changed(ctx, find_widget(ctx, doc, chk));
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

static void
begin_annot_op(fz_context *ctx, pdf_annot *annot, const char *op)
{
	pdf_begin_operation(ctx, annot->page->doc, op);
}

static void
end_annot_op(fz_context *ctx, pdf_annot *annot)
{
	pdf_end_operation(ctx, annot->page->doc);
}

static void
abandon_annot_op(fz_context *ctx, pdf_annot *annot)
{
	pdf_abandon_operation(ctx, annot->page->doc);
}

static void toggle_check_box(fz_context *ctx, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;

	begin_annot_op(ctx, annot, "Toggle checkbox");

	fz_try(ctx)
	{
		pdf_obj *field = annot->obj;
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
			{
				end_annot_op(ctx, annot);
				break;
			}
			val = PDF_NAME(Off);
		}
		else
		{
			val = pdf_button_field_on_state(ctx, field);
		}

		pdf_dict_put(ctx, grp, PDF_NAME(V), val);
		set_check_grp(ctx, doc, grp, val);
		doc->recalculate = 1;
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_set_annot_has_changed(ctx, annot);
}

int pdf_has_unsaved_changes(fz_context *ctx, pdf_document *doc)
{
	int i;

	if (doc->num_incremental_sections == 0)
		return 0;

	for (i = 0; i < doc->xref_sections->num_objects; i++)
		if (doc->xref_sections->subsec->table[i].type != 0)
			break;
	return i != doc->xref_sections->num_objects;
}

int pdf_was_repaired(fz_context *ctx, pdf_document *doc)
{
	return doc->repair_attempted;
}

int pdf_toggle_widget(fz_context *ctx, pdf_annot *widget)
{
	switch (pdf_widget_type(ctx, widget))
	{
	default:
		return 0;
	case PDF_WIDGET_TYPE_CHECKBOX:
	case PDF_WIDGET_TYPE_RADIOBUTTON:
		toggle_check_box(ctx, widget);
		return 1;
	}
	return 0;
}

int
pdf_update_page(fz_context *ctx, pdf_page *page)
{
	pdf_annot *annot;
	pdf_annot *widget;
	int changed = 0;

	fz_try(ctx)
	{
		pdf_begin_implicit_operation(ctx, page->doc);
		if (page->doc->recalculate)
			pdf_calculate_form(ctx, page->doc);

		for (annot = page->annots; annot; annot = annot->next)
			if (pdf_update_annot(ctx, annot))
				changed = 1;
		for (widget = page->widgets; widget; widget = widget->next)
			if (pdf_update_annot(ctx, widget))
				changed = 1;
		pdf_end_operation(ctx, page->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, page->doc);
		fz_rethrow(ctx);
	}

	return changed;
}

pdf_annot *pdf_first_widget(fz_context *ctx, pdf_page *page)
{
	return page->widgets;
}

pdf_annot *pdf_next_widget(fz_context *ctx, pdf_annot *widget)
{
	return widget->next;
}

enum pdf_widget_type pdf_widget_type(fz_context *ctx, pdf_annot *widget)
{
	enum pdf_widget_type ret = PDF_WIDGET_TYPE_BUTTON;

	pdf_annot_push_local_xref(ctx, widget);

	fz_try(ctx)
	{
		pdf_obj *subtype = pdf_dict_get(ctx, widget->obj, PDF_NAME(Subtype));
		if (pdf_name_eq(ctx, subtype, PDF_NAME(Widget)))
			ret = pdf_field_type(ctx, widget->obj);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, widget);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

static int set_validated_field_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *text, int ignore_trigger_events)
{
	char *newtext = NULL;

	if (!ignore_trigger_events)
	{
		if (!pdf_field_event_validate(ctx, doc, field, text, &newtext))
			return 0;
	}

	update_field_value(ctx, doc, field, newtext ? newtext : text);

	fz_free(ctx, newtext);

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
static char *load_field_name(fz_context *ctx, pdf_obj *field, int spare, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	char *res = NULL;
	pdf_obj *parent;
	const char *lname;
	int llen;

	if (pdf_cycle(ctx, &cycle, cycle_up, field))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cycle in field parents");

	parent = pdf_dict_get(ctx, field, PDF_NAME(Parent));
	lname = pdf_dict_get_text_string(ctx, field, PDF_NAME(T));
	llen = (int)strlen(lname);

	// Limit fields to 16K
	if (llen > (16 << 10) || llen + spare > (16 << 10))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Field name too long");

	/*
	 * If we found a name at this point in the field hierarchy
	 * then we'll need extra space for it and a dot
	 */
	if (llen)
		spare += llen+1;

	if (parent)
	{
		res = load_field_name(ctx, parent, spare, &cycle);
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

char *pdf_load_field_name(fz_context *ctx, pdf_obj *field)
{
	return load_field_name(ctx, field, 0, NULL);
}

void pdf_create_field_name(fz_context *ctx, pdf_document *doc, const char *prefix, char *buf, size_t len)
{
	pdf_obj *form = pdf_dict_getl(ctx, pdf_trailer(ctx, doc),
		PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(Fields), NULL);
	int i;
	for (i = 0; i < 65536; ++i) {
		fz_snprintf(buf, len, "%s%d", prefix, i);
		if (!pdf_lookup_field(ctx, form, buf))
			return;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "Could not create unique field name.");
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

		pdf_dict_put_int(ctx, field, PDF_NAME(F), f);
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
	float size, color[4];
	/* TODO? */
	const char *da = pdf_to_str_buf(ctx, pdf_dict_get_inheritable(ctx, field, PDF_NAME(DA)));
	int n;

	pdf_parse_default_appearance(ctx, da, &font, &size, &n, color);

	switch (pdf_array_len(ctx, col))
	{
	default:
		n = 0;
		color[0] = color[1] = color[2] = color[3] = 0;
		break;
	case 1:
		n = 1;
		color[0] = pdf_array_get_real(ctx, col, 0);
		break;
	case 3:
		n = 3;
		color[0] = pdf_array_get_real(ctx, col, 0);
		color[1] = pdf_array_get_real(ctx, col, 1);
		color[2] = pdf_array_get_real(ctx, col, 2);
		break;
	case 4:
		n = 4;
		color[0] = pdf_array_get_real(ctx, col, 0);
		color[1] = pdf_array_get_real(ctx, col, 1);
		color[2] = pdf_array_get_real(ctx, col, 2);
		color[3] = pdf_array_get_real(ctx, col, 3);
		break;
	}

	pdf_print_default_appearance(ctx, buf, sizeof buf, font, size, n, color);
	pdf_dict_put_string(ctx, field, PDF_NAME(DA), buf, strlen(buf));
	pdf_field_mark_dirty(ctx, field);
}

pdf_annot *
pdf_keep_widget(fz_context *ctx, pdf_annot *widget)
{
	return pdf_keep_annot(ctx, widget);
}

void
pdf_drop_widget(fz_context *ctx, pdf_annot *widget)
{
	pdf_drop_annot(ctx, widget);
}

void
pdf_drop_widgets(fz_context *ctx, pdf_annot *widget)
{
	while (widget)
	{
		pdf_annot *next = widget->next;
		pdf_drop_widget(ctx, widget);
		widget = next;
	}
}

pdf_annot *
pdf_create_signature_widget(fz_context *ctx, pdf_page *page, char *name)
{
	fz_rect rect = { 12, 12, 12+100, 12+50 };
	pdf_annot *annot = pdf_create_annot_raw(ctx, page, PDF_ANNOT_WIDGET);
	fz_try(ctx)
	{
		pdf_obj *obj = annot->obj;
		pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, page->doc), PDF_NAME(Root));
		pdf_obj *acroform = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
		pdf_obj *fields, *lock;
		if (!acroform)
		{
			acroform = pdf_new_dict(ctx, page->doc, 1);
			pdf_dict_put_drop(ctx, root, PDF_NAME(AcroForm), acroform);
		}
		fields = pdf_dict_get(ctx, acroform, PDF_NAME(Fields));
		if (!fields)
		{
			fields = pdf_new_array(ctx, page->doc, 1);
			pdf_dict_put_drop(ctx, acroform, PDF_NAME(Fields), fields);
		}
		pdf_set_annot_rect(ctx, annot, rect);
		pdf_dict_put(ctx, obj, PDF_NAME(FT), PDF_NAME(Sig));
		pdf_dict_put_int(ctx, obj, PDF_NAME(F), PDF_ANNOT_IS_PRINT);
		pdf_dict_put_text_string(ctx, obj, PDF_NAME(DA), "/Helv 0 Tf 0 g");
		pdf_dict_put_text_string(ctx, obj, PDF_NAME(T), name);
		pdf_array_push(ctx, fields, obj);
		lock = pdf_dict_put_dict(ctx, obj, PDF_NAME(Lock), 1);
		pdf_dict_put(ctx, lock, PDF_NAME(Action), PDF_NAME(All));
	}
	fz_catch(ctx)
	{
		pdf_delete_annot(ctx, page, annot);
	}
	return (pdf_annot *)annot;
}

fz_rect
pdf_bound_widget(fz_context *ctx, pdf_annot *widget)
{
	return pdf_bound_annot(ctx, widget);
}

int
pdf_update_widget(fz_context *ctx, pdf_annot *widget)
{
	return pdf_update_annot(ctx, widget);
}

int pdf_text_widget_max_len(fz_context *ctx, pdf_annot *tw)
{
	pdf_annot *annot = (pdf_annot *)tw;
	return pdf_dict_get_inheritable_int(ctx, annot->obj, PDF_NAME(MaxLen));
}

int pdf_text_widget_format(fz_context *ctx, pdf_annot *tw)
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

static char *
merge_changes(fz_context *ctx, const char *value, int start, int end, const char *change)
{
	int changelen = change ? (int)strlen(change) : 0;
	int valuelen = value ? (int)strlen(value) : 0;
	int prelen = (start >= 0 ? (start < valuelen ? start : valuelen) : 0);
	int postlen = (end >= 0 && end <= valuelen ? valuelen - end : 0);
	int newlen =  prelen + changelen + postlen + 1;
	char *merged = fz_malloc(ctx, newlen);
	char *m = merged;

	if (prelen)
	{
		memcpy(m, value, prelen);
		m += prelen;
	}
	if (changelen)
	{
		memcpy(m, change, changelen);
		m += changelen;
	}
	if (postlen)
	{
		memcpy(m, &value[end], postlen);
		m += postlen;
	}
	*m = 0;

	return merged;
}

int pdf_set_text_field_value(fz_context *ctx, pdf_annot *widget, const char *update)
{
	pdf_document *doc = widget->page->doc;
	pdf_keystroke_event evt = { 0 };
	char *new_change = NULL;
	char *new_value = NULL;
	char *merged_value = NULL;
	int rc = 1;

	pdf_begin_operation(ctx, doc, "Edit text field");

	fz_var(new_value);
	fz_var(new_change);
	fz_var(merged_value);
	fz_try(ctx)
	{
		if (!widget->ignore_trigger_events)
		{
			evt.value = pdf_annot_field_value(ctx, widget);
			evt.change = update;
			evt.selStart = 0;
			evt.selEnd = (int)strlen(evt.value);
			evt.willCommit = 0;
			rc = pdf_annot_field_event_keystroke(ctx, doc, widget, &evt);
			new_change = evt.newChange;
			new_value = evt.newValue;
			evt.newValue = NULL;
			evt.newChange = NULL;
			if (rc)
			{
				merged_value = merge_changes(ctx, new_value, evt.selStart, evt.selEnd, new_change);
				evt.value = merged_value;
				evt.change = "";
				evt.selStart = -1;
				evt.selEnd = -1;
				evt.willCommit = 1;
				rc = pdf_annot_field_event_keystroke(ctx, doc, widget, &evt);
				if (rc)
					rc = pdf_set_annot_field_value(ctx, doc, widget, evt.newValue, 0);
			}
		}
		else
		{
			rc = pdf_set_annot_field_value(ctx, doc, widget, update, 1);
		}
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
		fz_free(ctx, new_value);
		fz_free(ctx, evt.newValue);
		fz_free(ctx, new_change);
		fz_free(ctx, evt.newChange);
		fz_free(ctx, merged_value);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_warn(ctx, "could not set widget text");
		rc = 0;
	}
	return rc;
}

int pdf_edit_text_field_value(fz_context *ctx, pdf_annot *widget, const char *value, const char *change, int *selStart, int *selEnd, char **result)
{
	pdf_document *doc = widget->page->doc;
	pdf_keystroke_event evt = {0};
	int rc = 1;

	pdf_begin_operation(ctx, doc, "Text field keystroke");

	fz_try(ctx)
	{
		if (!widget->ignore_trigger_events)
		{
			evt.value = value;
			evt.change = change;
			evt.selStart = *selStart;
			evt.selEnd = *selEnd;
			evt.willCommit = 0;
			rc = pdf_annot_field_event_keystroke(ctx, doc, widget, &evt);
			if (rc)
			{
				*result = merge_changes(ctx, evt.newValue, evt.selStart, evt.selEnd, evt.newChange);
				*selStart = evt.selStart + (int)strlen(evt.newChange);
				*selEnd = *selStart;
			}
		}
		else
		{
			*result = merge_changes(ctx, value, *selStart, *selEnd, change);
			*selStart = evt.selStart + (int)strlen(change);
			*selEnd = *selStart;
		}
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
		fz_free(ctx, evt.newValue);
		fz_free(ctx, evt.newChange);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_warn(ctx, "could not process text widget keystroke");
		rc = 0;
	}
	return rc;
}

int pdf_set_choice_field_value(fz_context *ctx, pdf_annot *widget, const char *new_value)
{
	/* Choice widgets use almost the same keystroke processing as text fields. */
	return pdf_set_text_field_value(ctx, widget, new_value);
}

int pdf_choice_widget_options(fz_context *ctx, pdf_annot *tw, int exportval, const char *opts[])
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

int pdf_choice_widget_is_multiselect(fz_context *ctx, pdf_annot *tw)
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

int pdf_choice_widget_value(fz_context *ctx, pdf_annot *tw, const char *opts[])
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

void pdf_choice_widget_set_value(fz_context *ctx, pdf_annot *tw, int n, const char *opts[])
{
	pdf_annot *annot = (pdf_annot *)tw;
	pdf_obj *optarr = NULL;
	int i;

	if (!annot)
		return;

	begin_annot_op(ctx, annot, "Set choice");

	fz_var(optarr);
	fz_try(ctx)
	{
		if (n != 1)
		{
			optarr = pdf_new_array(ctx, annot->page->doc, n);

			for (i = 0; i < n; i++)
				pdf_array_push_text_string(ctx, optarr, opts[i]);

			pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(V), optarr);
		}
		else
			pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(V), opts[0]);

		/* FIXME: when n > 1, we should be regenerating the indexes */
		pdf_dict_del(ctx, annot->obj, PDF_NAME(I));

		pdf_field_mark_dirty(ctx, annot->obj);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
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
	pdf_obj* vtype;

	if (pdf_dict_get_inheritable(ctx, field, PDF_NAME(FT)) != PDF_NAME(Sig))
		return 0;
	/* Signatures can only be signed if the value is a dictionary,
	 * and if the value has a Type, it should be Sig. */
	v = pdf_dict_get_inheritable(ctx, field, PDF_NAME(V));
	vtype = pdf_dict_get(ctx, v, PDF_NAME(Type));
	return pdf_is_dict(ctx, v) && (vtype ? pdf_name_eq(ctx, vtype, PDF_NAME(Sig)) : 1);
}

int pdf_widget_is_signed(fz_context *ctx, pdf_annot *widget)
{
	if (widget == NULL)
		return 0;
	return pdf_signature_is_signed(ctx, widget->page->doc, widget->obj);
}

int pdf_widget_is_readonly(fz_context *ctx, pdf_annot *widget)
{
	int fflags;
	if (widget == NULL)
		return 0;
	fflags = pdf_field_flags(ctx, ((pdf_annot *) widget)->obj);
	return fflags & PDF_FIELD_IS_READ_ONLY;
}

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

static fz_xml_doc *load_xfa(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *xfa;
	fz_buffer *buf = NULL;
	fz_buffer *packet = NULL;
	int i;

	if (doc->xfa)
		return doc->xfa; /* Already loaded, and present. */

	xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");
	if (!pdf_is_array(ctx, xfa) && !pdf_is_stream(ctx, xfa))
		return NULL; /* No XFA */

	fz_var(buf);
	fz_var(packet);

	fz_try(ctx)
	{
		if (pdf_is_stream(ctx, xfa))
		{
			/* Load entire XFA resource */
			buf = pdf_load_stream(ctx, xfa);
		}
		else
		{
			/* Concatenate packets to create entire XFA resource */
			buf = fz_new_buffer(ctx, 1024);
			for(i = 0; i < pdf_array_len(ctx, xfa); ++i)
			{
				pdf_obj *ref = pdf_array_get(ctx, xfa, i);
				if (pdf_is_stream(ctx, ref))
				{
					packet = pdf_load_stream(ctx, ref);
					fz_append_buffer(ctx, buf, packet);
					fz_drop_buffer(ctx, packet);
					packet = NULL;
				}
			}
		}

		/* Parse and stow away XFA resource in document */
		doc->xfa = fz_parse_xml(ctx, buf, 0);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, packet);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return doc->xfa;
}

static fz_xml *
get_xfa_resource(fz_context *ctx, pdf_document *doc, const char *str)
{
	fz_xml_doc *xfa;

	xfa = load_xfa(ctx, doc);
	if (!xfa)
		return NULL;

	return fz_xml_find_down(fz_xml_root(xfa), str);
}

static int
find_name_component(char **np, char **sp, char **ep)
{
	char *n = *np;
	char *s, *e;
	int idx = 0;

	if (*n == '.')
		n++;

	/* Find the next name we are looking for. */
	s = e = n;
	while (*e && *e != '[' && *e != '.')
		e++;

	/* So the next name is s..e */
	n = e;
	if (*n == '[')
	{
		n++;
		while (*n >= '0' && *n <= '9')
			idx = idx*10 + *n++ - '0';
		while (*n && *n != ']')
			n++;
		if (*n == ']')
			n++;
	}
	*np = n;
	*sp = s;
	*ep = e;

	return idx;
}

static pdf_obj *
annot_from_name(fz_context *ctx, pdf_document *doc, const char *str)
{
	pdf_obj *fields = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");

	if (strncmp(str, "xfa[0].", 7) == 0)
		str += 7;
	if (strncmp(str, "template[0].", 12) == 0)
		str += 12;

	return pdf_lookup_field(ctx, fields, str);
}

static pdf_obj *
get_locked_fields_from_xfa(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	char *name = pdf_load_field_name(ctx, field);
	char *n = name;
	const char *use;
	fz_xml *node;

	if (name == NULL)
		return NULL;

	fz_try(ctx)
	{
		node = get_xfa_resource(ctx, doc, "template");

		do
		{
			char c, *s, *e;
			int idx = 0;
			char *key;

			idx = find_name_component(&n, &s, &e);
			/* We want the idx'th occurrence of s..e */

			/* Hacky */
			c = *e;
			*e = 0;
			key = *n ? "subform" : "field";
			node = fz_xml_find_down_match(node, key, "name", s);
			while (node && idx > 0)
			{
				node = fz_xml_find_next_match(node, key, "name", s);
				idx--;
			}
			*e = c;
		}
		while (node && *n == '.');
	}
	fz_always(ctx)
		fz_free(ctx, name);
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (node == NULL)
		return NULL;

	node = fz_xml_find_down(node, "ui");
	node = fz_xml_find_down(node, "signature");
	node = fz_xml_find_down(node, "manifest");

	use = fz_xml_att(node, "use");
	if (use == NULL)
		return NULL;
	if (*use == '#')
		use++;

	/* Now look for a variables entry in a subform that defines this. */
	while (node)
	{
		fz_xml *variables, *manifest, *ref;
		pdf_obj *arr;

		/* Find the enclosing subform */
		do {
			node = fz_xml_up(node);
		} while (node && strcmp(fz_xml_tag(node), "subform"));

		/* Look for a variables within that. */
		variables = fz_xml_find_down(node, "variables");
		if (variables == NULL)
			continue;

		manifest = fz_xml_find_down_match(variables, "manifest", "id", use);
		if (manifest == NULL)
			continue;

		arr = pdf_new_array(ctx, doc, 16);
		fz_try(ctx)
		{
			ref = fz_xml_find_down(manifest, "ref");
			while (ref)
			{
				const char *s = fz_xml_text(fz_xml_down(ref));
				pdf_array_push(ctx, arr, annot_from_name(ctx, doc, s));
				ref = fz_xml_find_next(ref, "ref");
			}
		}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, arr);
			fz_rethrow(ctx);
		}
		return arr;
	}

	return NULL;
}

static void
lock_field(fz_context *ctx, pdf_obj *f)
{
	int ff = pdf_dict_get_inheritable_int(ctx, f, PDF_NAME(Ff));

	if ((ff & PDF_FIELD_IS_READ_ONLY) ||
		!pdf_name_eq(ctx, pdf_dict_get(ctx, f, PDF_NAME(Type)), PDF_NAME(Annot)) ||
		!pdf_name_eq(ctx, pdf_dict_get(ctx, f, PDF_NAME(Subtype)), PDF_NAME(Widget)))
		return;

	pdf_dict_put_int(ctx, f, PDF_NAME(Ff), ff | PDF_FIELD_IS_READ_ONLY);
}

static void
lock_xfa_locked_fields(fz_context *ctx, pdf_obj *a)
{
	int i;
	int len = pdf_array_len(ctx, a);

	for (i = 0; i < len; i++)
	{
		lock_field(ctx, pdf_array_get(ctx, a, i));
	}
}


void pdf_signature_set_value(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_pkcs7_signer *signer, int64_t stime)
{
	pdf_obj *v = NULL;
	pdf_obj *o = NULL;
	pdf_obj *r = NULL;
	pdf_obj *t = NULL;
	pdf_obj *a = NULL;
	pdf_obj *b = NULL;
	pdf_obj *l = NULL;
	pdf_obj *indv;
	int vnum;
	size_t max_digest_size;
	char *buf = NULL;

	vnum = pdf_create_object(ctx, doc);
	indv = pdf_new_indirect(ctx, doc, vnum, 0);
	pdf_dict_put_drop(ctx, field, PDF_NAME(V), indv);

	max_digest_size = signer->max_digest_size(ctx, signer);

	fz_var(v);
	fz_var(o);
	fz_var(r);
	fz_var(t);
	fz_var(a);
	fz_var(b);
	fz_var(l);
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
		pdf_dict_put_date(ctx, v, PDF_NAME(M), stime);

		o = pdf_dict_put_array(ctx, v, PDF_NAME(Reference), 1);
		r = pdf_array_put_dict(ctx, o, 0, 4);
		pdf_dict_put(ctx, r, PDF_NAME(Data), pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)));
		pdf_dict_put(ctx, r, PDF_NAME(TransformMethod), PDF_NAME(FieldMDP));
		pdf_dict_put(ctx, r, PDF_NAME(Type), PDF_NAME(SigRef));
		t = pdf_dict_put_dict(ctx, r, PDF_NAME(TransformParams), 5);

		l = pdf_dict_getp(ctx, field, "Lock/Action");
		if (l)
		{
			a = pdf_dict_getp(ctx, field, "Lock/Fields");
		}
		else
		{
			/* Lock action wasn't specified so we need to encode an Include.
			 * Before we just use an empty array, check in the XFA for locking
			 * details. */
			a = get_locked_fields_from_xfa(ctx, doc, field);
			if (a)
				lock_xfa_locked_fields(ctx, a);

			/* If we don't get a result from the XFA, just encode an empty array
			 * (leave a == NULL), even if Lock/Fields exists because we don't really
			 * know what to do with the information if the action isn't defined. */
			l = PDF_NAME(Include);
		}

		pdf_dict_put(ctx, t, PDF_NAME(Action), l);

		if (pdf_name_eq(ctx, l, PDF_NAME(Include)) || pdf_name_eq(ctx, l, PDF_NAME(Exclude)))
		{
			/* For action Include and Exclude, we need to encode a Fields array */
			if (!a)
			{
				/* If one wasn't defined or we chose to ignore it because no action
				 * was defined then use an empty one. */
				b = pdf_new_array(ctx, doc, 0);
				a = b;
			}

			pdf_dict_put_drop(ctx, t, PDF_NAME(Fields), pdf_copy_array(ctx, a));
		}

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
		pdf_drop_obj(ctx, b);
		fz_free(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_set_widget_editing_state(fz_context *ctx, pdf_annot *widget, int editing)
{
	widget->ignore_trigger_events = editing;
}

int pdf_get_widget_editing_state(fz_context *ctx, pdf_annot *widget)
{
	return widget->ignore_trigger_events;
}

static void pdf_execute_js_action(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path, pdf_obj *js)
{
	if (js)
	{
		char *code = pdf_load_stream_or_string_as_utf8(ctx, js);
		int in_op = 0;

		fz_var(in_op);
		fz_try(ctx)
		{
			char buf[100];
			fz_snprintf(buf, sizeof buf, "%d/%s", pdf_to_num(ctx, target), path);
			pdf_begin_operation(ctx, doc, "Javascript Event");
			in_op = 1;
			pdf_js_execute(doc->js, buf, code, NULL);
			pdf_end_operation(ctx, doc);
		}
		fz_always(ctx)
		{
			fz_free(ctx, code);
		}
		fz_catch(ctx)
		{
			if (in_op)
				pdf_abandon_operation(ctx, doc);
			fz_rethrow(ctx);
		}
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

static void pdf_execute_action_chain(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path, pdf_obj *action, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	pdf_obj *next;

	if (pdf_cycle(ctx, &cycle, cycle_up, action))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in action chain");

	if (pdf_is_array(ctx, action))
	{
		int i, n = pdf_array_len(ctx, action);
		for (i = 0; i < n; ++i)
			pdf_execute_action_chain(ctx, doc, target, path, pdf_array_get(ctx, action, i), &cycle);
	}
	else
	{
		pdf_execute_action_imp(ctx, doc, target, path, action);
		next = pdf_dict_get(ctx, action, PDF_NAME(Next));
		if (next)
			pdf_execute_action_chain(ctx, doc, target, path, next, &cycle);
	}
}

static void pdf_execute_action(fz_context *ctx, pdf_document *doc, pdf_obj *target, const char *path)
{
	pdf_obj *action = pdf_dict_getp_inheritable(ctx, target, path);
	if (action)
		pdf_execute_action_chain(ctx, doc, target, path, action, NULL);
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

static void
annot_execute_action(fz_context *ctx, pdf_annot *annot, const char *act)
{
	begin_annot_op(ctx, annot, "JavaScript action");

	fz_try(ctx)
	{
		pdf_execute_action(ctx, annot->page->doc, annot->obj, act);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

void pdf_annot_event_enter(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/E");
}

void pdf_annot_event_exit(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/X");
}

void pdf_annot_event_down(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/D");
}

void pdf_annot_event_up(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *action;

	begin_annot_op(ctx, annot, "JavaScript action");

	fz_try(ctx)
	{
		action = pdf_dict_get(ctx, annot->obj, PDF_NAME(A));
		if (action)
			pdf_execute_action_chain(ctx, annot->page->doc, annot->obj, "A", action, NULL);
		else
			pdf_execute_action(ctx, annot->page->doc, annot->obj, "AA/U");
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

void pdf_annot_event_focus(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/Fo");
}

void pdf_annot_event_blur(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/Bl");
}

void pdf_annot_event_page_open(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/PO");
}

void pdf_annot_event_page_close(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/PC");
}

void pdf_annot_event_page_visible(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/PV");
}

void pdf_annot_event_page_invisible(fz_context *ctx, pdf_annot *annot)
{
	annot_execute_action(ctx, annot, "AA/PI");
}

int pdf_field_event_keystroke(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_keystroke_event *evt)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp_inheritable(ctx, field, "AA/K/JS");
		if (action)
		{
			pdf_js_event_init_keystroke(js, field, evt);
			pdf_execute_js_action(ctx, doc, field, "AA/K/JS", action);
			return pdf_js_event_result_keystroke(js, evt);
		}
	}
	evt->newChange = fz_strdup(ctx, evt->change);
	evt->newValue = fz_strdup(ctx, evt->value);
	return 1;
}

int pdf_annot_field_event_keystroke(fz_context *ctx, pdf_document *doc, pdf_annot *annot, pdf_keystroke_event *evt)
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = pdf_field_event_keystroke(ctx, doc, annot->obj, evt);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

char *pdf_field_event_format(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp_inheritable(ctx, field, "AA/F/JS");
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

int pdf_field_event_validate(fz_context *ctx, pdf_document *doc, pdf_obj *field, const char *value, char **newvalue)
{
	pdf_js *js = doc->js;

	*newvalue = NULL;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp_inheritable(ctx, field, "AA/V/JS");
		if (action)
		{
			pdf_js_event_init(js, field, value, 1);
			pdf_execute_js_action(ctx, doc, field, "AA/V/JS", action);
			return pdf_js_event_result_validate(js, newvalue);
		}
	}
	return 1;
}

void pdf_field_event_calculate(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	pdf_js *js = doc->js;
	if (js)
	{
		pdf_obj *action = pdf_dict_getp_inheritable(ctx, field, "AA/C/JS");
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
					new_value = pdf_js_event_value(js);
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
