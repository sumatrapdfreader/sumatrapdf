// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#include <assert.h>
#include <limits.h>
#include <string.h>

#undef DEBUG_PROGESSIVE_ADVANCE

#ifdef DEBUG_PROGESSIVE_ADVANCE
#define DEBUGMESS(A) do { fz_warn A; } while (0)
#else
#define DEBUGMESS(A) do { } while (0)
#endif

#define isdigit(c) (c >= '0' && c <= '9')

static inline int iswhite(int ch)
{
	return
		ch == '\000' || ch == '\011' || ch == '\012' ||
		ch == '\014' || ch == '\015' || ch == '\040';
}

/*
 * xref tables
 */

static void
pdf_drop_xref_subsec(fz_context *ctx, pdf_xref *xref)
{
	pdf_xref_subsec *sub = xref->subsec;
	pdf_unsaved_sig *usig;
	int e;

	while (sub != NULL)
	{
		pdf_xref_subsec *next_sub = sub->next;
		for (e = 0; e < sub->len; e++)
		{
			pdf_xref_entry *entry = &sub->table[e];
			pdf_drop_obj(ctx, entry->obj);
			fz_drop_buffer(ctx, entry->stm_buf);
		}
		fz_free(ctx, sub->table);
		fz_free(ctx, sub);
		sub = next_sub;
	}

	pdf_drop_obj(ctx, xref->pre_repair_trailer);
	pdf_drop_obj(ctx, xref->trailer);

	while ((usig = xref->unsaved_sigs) != NULL)
	{
		xref->unsaved_sigs = usig->next;
		pdf_drop_obj(ctx, usig->field);
		pdf_drop_signer(ctx, usig->signer);
		fz_free(ctx, usig);
	}
}

static void pdf_drop_xref_sections_imp(fz_context *ctx, pdf_document *doc, pdf_xref *xref_sections, int num_xref_sections)
{
	int x;

	for (x = 0; x < num_xref_sections; x++)
		pdf_drop_xref_subsec(ctx, &xref_sections[x]);

	fz_free(ctx, xref_sections);
}

static void pdf_drop_xref_sections(fz_context *ctx, pdf_document *doc)
{
	pdf_drop_xref_sections_imp(ctx, doc, doc->saved_xref_sections, doc->saved_num_xref_sections);
	pdf_drop_xref_sections_imp(ctx, doc, doc->xref_sections, doc->num_xref_sections);

	doc->saved_xref_sections = NULL;
	doc->saved_num_xref_sections = 0;
	doc->xref_sections = NULL;
	doc->num_xref_sections = 0;
	doc->num_incremental_sections = 0;
}

static void
extend_xref_index(fz_context *ctx, pdf_document *doc, int newlen)
{
	int i;

	doc->xref_index = fz_realloc_array(ctx, doc->xref_index, newlen, int);
	for (i = doc->max_xref_len; i < newlen; i++)
	{
		doc->xref_index[i] = 0;
	}
	doc->max_xref_len = newlen;
}

static void
resize_xref_sub(fz_context *ctx, pdf_xref *xref, int base, int newlen)
{
	pdf_xref_subsec *sub;
	int i;

	assert(xref != NULL);
	sub = xref->subsec;
	assert(sub->next == NULL && sub->start == base && sub->len+base == xref->num_objects);
	assert(newlen+base > xref->num_objects);

	sub->table = fz_realloc_array(ctx, sub->table, newlen, pdf_xref_entry);
	for (i = sub->len; i < newlen; i++)
	{
		sub->table[i].type = 0;
		sub->table[i].ofs = 0;
		sub->table[i].gen = 0;
		sub->table[i].num = 0;
		sub->table[i].stm_ofs = 0;
		sub->table[i].stm_buf = NULL;
		sub->table[i].obj = NULL;
	}
	sub->len = newlen;
	if (newlen+base > xref->num_objects)
		xref->num_objects = newlen+base;
}

/* This is only ever called when we already have an incremental
 * xref. This means there will only be 1 subsec, and it will be
 * a complete subsec. */
static void pdf_resize_xref(fz_context *ctx, pdf_document *doc, int newlen)
{
	pdf_xref *xref = &doc->xref_sections[doc->xref_base];

	resize_xref_sub(ctx, xref, 0, newlen);
	if (doc->max_xref_len < newlen)
		extend_xref_index(ctx, doc, newlen);
}

static void pdf_populate_next_xref_level(fz_context *ctx, pdf_document *doc)
{
	pdf_xref *xref;
	doc->xref_sections = fz_realloc_array(ctx, doc->xref_sections, doc->num_xref_sections + 1, pdf_xref);
	doc->num_xref_sections++;

	xref = &doc->xref_sections[doc->num_xref_sections - 1];
	xref->subsec = NULL;
	xref->num_objects = 0;
	xref->trailer = NULL;
	xref->pre_repair_trailer = NULL;
	xref->unsaved_sigs = NULL;
	xref->unsaved_sigs_end = NULL;
}

pdf_obj *pdf_trailer(fz_context *ctx, pdf_document *doc)
{
	/* Return the document's trailer (of the appropriate vintage) */
	pdf_xref *xrefs = doc->xref_sections;

	return xrefs ? xrefs[doc->xref_base].trailer : NULL;
}

void pdf_set_populating_xref_trailer(fz_context *ctx, pdf_document *doc, pdf_obj *trailer)
{
	/* Update the trailer of the xref section being populated */
	pdf_xref *xref = &doc->xref_sections[doc->num_xref_sections - 1];
	if (xref->trailer)
	{
		pdf_drop_obj(ctx, xref->pre_repair_trailer);
		xref->pre_repair_trailer = xref->trailer;
	}
	xref->trailer = pdf_keep_obj(ctx, trailer);
}

int pdf_xref_len(fz_context *ctx, pdf_document *doc)
{
	int i = doc->xref_base;
	int xref_len = 0;

	if (doc->local_xref && doc->local_xref_nesting > 0)
		xref_len = doc->local_xref->num_objects;

	while (i < doc->num_xref_sections)
		xref_len = fz_maxi(xref_len, doc->xref_sections[i++].num_objects);

	return xref_len;
}

/* Ensure that the given xref has a single subsection
 * that covers the entire range. */
static void
ensure_solid_xref(fz_context *ctx, pdf_document *doc, int num, int which)
{
	pdf_xref *xref = &doc->xref_sections[which];
	pdf_xref_subsec *sub = xref->subsec;
	pdf_xref_subsec *new_sub;

	if (num < xref->num_objects)
		num = xref->num_objects;

	if (sub != NULL && sub->next == NULL && sub->start == 0 && sub->len >= num)
		return;

	new_sub = fz_malloc_struct(ctx, pdf_xref_subsec);
	fz_try(ctx)
	{
		new_sub->table = fz_malloc_struct_array(ctx, num, pdf_xref_entry);
		new_sub->start = 0;
		new_sub->len = num;
		new_sub->next = NULL;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, new_sub);
		fz_rethrow(ctx);
	}

	/* Move objects over to the new subsection and destroy the old
	 * ones */
	sub = xref->subsec;
	while (sub != NULL)
	{
		pdf_xref_subsec *next = sub->next;
		int i;

		for (i = 0; i < sub->len; i++)
		{
			new_sub->table[i+sub->start] = sub->table[i];
		}
		fz_free(ctx, sub->table);
		fz_free(ctx, sub);
		sub = next;
	}
	xref->num_objects = num;
	xref->subsec = new_sub;
	if (doc->max_xref_len < num)
		extend_xref_index(ctx, doc, num);
}

static pdf_xref_entry *
pdf_get_local_xref_entry(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref *xref = doc->local_xref;
	pdf_xref_subsec *sub;

	if (xref == NULL || doc->local_xref_nesting == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Local xref not present!");

	/* Local xrefs only ever have 1 section, and it should be solid. */
	sub = xref->subsec;
	assert(sub && !sub->next);
	if (num >= sub->start && num < sub->start + sub->len)
		return &sub->table[num - sub->start];

	/* Expand the xref so we can return a pointer. */
	resize_xref_sub(ctx, xref, 0, num+1);
	sub = xref->subsec;
	return &sub->table[num - sub->start];
}

pdf_xref_entry *pdf_get_populating_xref_entry(fz_context *ctx, pdf_document *doc, int num)
{
	/* Return an entry within the xref currently being populated */
	pdf_xref *xref;
	pdf_xref_subsec *sub;

	if (doc->num_xref_sections == 0)
	{
		doc->xref_sections = fz_malloc_struct(ctx, pdf_xref);
		doc->num_xref_sections = 1;
	}

	if (doc->local_xref && doc->local_xref_nesting > 0)
		return pdf_get_local_xref_entry(ctx, doc, num);

	/* Prevent accidental heap underflow */
	if (num < 0 || num > PDF_MAX_OBJECT_NUMBER)
		fz_throw(ctx, FZ_ERROR_GENERIC, "object number out of range (%d)", num);

	/* Return the pointer to the entry in the last section. */
	xref = &doc->xref_sections[doc->num_xref_sections-1];

	for (sub = xref->subsec; sub != NULL; sub = sub->next)
	{
		if (num >= sub->start && num < sub->start + sub->len)
			return &sub->table[num-sub->start];
	}

	/* We've been asked for an object that's not in a subsec. */
	ensure_solid_xref(ctx, doc, num+1, doc->num_xref_sections-1);
	xref = &doc->xref_sections[doc->num_xref_sections-1];
	sub = xref->subsec;

	return &sub->table[num-sub->start];
}

/* It is vital that pdf_get_xref_entry_aux called with !solidify_if_needed
 * and a value object number, does NOT try/catch or throw. */
static
pdf_xref_entry *pdf_get_xref_entry_aux(fz_context *ctx, pdf_document *doc, int i, int solidify_if_needed)
{
	pdf_xref *xref = NULL;
	pdf_xref_subsec *sub;
	int j;

	if (i < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Negative object number requested");

	if (i < doc->max_xref_len)
		j = doc->xref_index[i];
	else
		j = 0;

	/* If we have an active local xref, check there first. */
	if (doc->local_xref && doc->local_xref_nesting > 0)
	{
		xref = doc->local_xref;

		if (i < xref->num_objects)
		{
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				pdf_xref_entry *entry;

				if (i < sub->start || i >= sub->start + sub->len)
					continue;

				entry = &sub->table[i - sub->start];
				if (entry->type)
					return entry;
			}
		}
	}

	/* We may be accessing an earlier version of the document using xref_base
	 * and j may be an index into a later xref section */
	if (doc->xref_base > j)
		j = doc->xref_base;
	else
		j = 0;


	/* Find the first xref section where the entry is defined. */
	for (; j < doc->num_xref_sections; j++)
	{
		xref = &doc->xref_sections[j];

		if (i < xref->num_objects)
		{
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				pdf_xref_entry *entry;

				if (i < sub->start || i >= sub->start + sub->len)
					continue;

				entry = &sub->table[i - sub->start];
				if (entry->type)
				{
					/* Don't update xref_index if xref_base may have
					 * influenced the value of j */
					if (doc->xref_base == 0)
						doc->xref_index[i] = j;
					return entry;
				}
			}
		}
	}

	/* Didn't find the entry in any section. Return the entry from
	 * the local_xref (if there is one active), or the final section. */
	if (doc->local_xref && doc->local_xref_nesting > 0)
	{
		if (xref == NULL || i < xref->num_objects)
		{
			xref = doc->local_xref;
			sub = xref->subsec;
			assert(sub != NULL && sub->next == NULL);
			if (i >= sub->start && i < sub->start + sub->len)
				return &sub->table[i - sub->start];
		}

		/* Expand the xref so we can return a pointer. */
		resize_xref_sub(ctx, xref, 0, i+1);
		sub = xref->subsec;
		return &sub->table[i - sub->start];
	}

	doc->xref_index[i] = 0;
	if (xref == NULL || i < xref->num_objects)
	{
		xref = &doc->xref_sections[doc->xref_base];
		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			if (i >= sub->start && i < sub->start + sub->len)
				return &sub->table[i - sub->start];
		}
	}

	/* Some really hairy code here. When we are reading the file in
	 * initially, we read from 'newest' to 'oldest' (i.e. from 0 to
	 * doc->num_xref_sections-1). Each section is created initially
	 * with num_objects == 0 in it, and remains like that while we
	 * are parsing the stream from the file. This is the only time
	 * we'll ever have xref_sections with 0 objects in them. */
	if (doc->xref_sections[doc->num_xref_sections-1].num_objects == 0)
	{
		/* The oldest xref section has 0 objects in it. So we are
		 * parsing an xref stream while loading. We don't want to
		 * solidify the xref we are currently parsing for (as it'll
		 * get very confused, and end up a different 'shape' in
		 * memory to that which is in the file, and would hence
		 * render 'fingerprinting' for snapshotting invalid) so
		 * just give up at this point. */
		return NULL;
	}

	if (!solidify_if_needed)
		return NULL;

	/* At this point, we solidify the xref. This ensures that we
	 * can return a pointer. This is the only case where this function
	 * might throw an exception, and it will never happen when we are
	 * working within a 'solid' xref. */
	ensure_solid_xref(ctx, doc, i+1, 0);
	xref = &doc->xref_sections[0];
	sub = xref->subsec;
	return &sub->table[i - sub->start];
}

pdf_xref_entry *pdf_get_xref_entry(fz_context *ctx, pdf_document *doc, int i)
{
	return pdf_get_xref_entry_aux(ctx, doc, i, 1);
}

pdf_xref_entry *pdf_get_xref_entry_no_change(fz_context *ctx, pdf_document *doc, int i)
{
	return pdf_get_xref_entry_aux(ctx, doc, i, 0);
}

pdf_xref_entry *pdf_get_xref_entry_no_null(fz_context *ctx, pdf_document *doc, int i)
{
	pdf_xref_entry *entry = pdf_get_xref_entry(ctx, doc, i);
	if (entry != NULL)
		return entry;
	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find object in xref (%d 0 R), but not allowed to return NULL", i);
}

void pdf_xref_entry_map(fz_context *ctx, pdf_document *doc, void (*fn)(fz_context *, pdf_xref_entry *, int, pdf_document *, void *), void *arg)
{
	int i, j;
	pdf_xref_subsec *sub;
	int xref_base = doc->xref_base;

	fz_try(ctx)
	{
		/* Map over any active local xref first. */
		if (doc->local_xref && doc->local_xref_nesting > 0)
		{
			pdf_xref *xref = doc->local_xref;

			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				for (i = sub->start; i < sub->start + sub->len; i++)
				{
					pdf_xref_entry *entry = &sub->table[i - sub->start];
					if (entry->type)
						fn(ctx, entry, i, doc, arg);
				}
			}
		}

		for (j = 0; j < doc->num_xref_sections; j++)
		{
			pdf_xref *xref = &doc->xref_sections[j];
			doc->xref_base = j;

			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				for (i = sub->start; i < sub->start + sub->len; i++)
				{
					pdf_xref_entry *entry = &sub->table[i - sub->start];
					if (entry->type)
						fn(ctx, entry, i, doc, arg);
				}
			}
		}
	}
	fz_always(ctx)
	{
		doc->xref_base = xref_base;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
	Ensure we have an incremental xref section where we can store
	updated versions of indirect objects. This is a new xref section
	consisting of a single xref subsection.
*/
static void ensure_incremental_xref(fz_context *ctx, pdf_document *doc)
{
	/* If there are as yet no incremental sections, or if the most recent
	 * one has been used to sign a signature field, then we need a new one.
	 * After a signing, any further document changes require a new increment */
	if ((doc->num_incremental_sections == 0 || doc->xref_sections[0].unsaved_sigs != NULL)
		&& !doc->disallow_new_increments)
	{
		pdf_xref *xref = &doc->xref_sections[0];
		pdf_xref *pxref;
		pdf_xref_entry *new_table = fz_malloc_struct_array(ctx, xref->num_objects, pdf_xref_entry);
		pdf_xref_subsec *sub = NULL;
		pdf_obj *trailer = NULL;
		int i;

		fz_var(trailer);
		fz_var(sub);
		fz_try(ctx)
		{
			sub = fz_malloc_struct(ctx, pdf_xref_subsec);
			trailer = xref->trailer ? pdf_copy_dict(ctx, xref->trailer) : NULL;
			doc->xref_sections = fz_realloc_array(ctx, doc->xref_sections, doc->num_xref_sections + 1, pdf_xref);
			xref = &doc->xref_sections[0];
			pxref = &doc->xref_sections[1];
			memmove(pxref, xref, doc->num_xref_sections * sizeof(pdf_xref));
			/* xref->num_objects is already correct */
			xref->subsec = sub;
			sub = NULL;
			xref->trailer = trailer;
			xref->pre_repair_trailer = NULL;
			xref->unsaved_sigs = NULL;
			xref->unsaved_sigs_end = NULL;
			xref->subsec->next = NULL;
			xref->subsec->len = xref->num_objects;
			xref->subsec->start = 0;
			xref->subsec->table = new_table;
			doc->num_xref_sections++;
			doc->num_incremental_sections++;
		}
		fz_catch(ctx)
		{
			fz_free(ctx, sub);
			fz_free(ctx, new_table);
			pdf_drop_obj(ctx, trailer);
			fz_rethrow(ctx);
		}

		/* Update the xref_index */
		for (i = 0; i < doc->max_xref_len; i++)
		{
			doc->xref_index[i]++;
		}
	}
}

/* Used when altering a document */
pdf_xref_entry *pdf_get_incremental_xref_entry(fz_context *ctx, pdf_document *doc, int i)
{
	pdf_xref *xref;
	pdf_xref_subsec *sub;

	/* Make a new final xref section if we haven't already */
	ensure_incremental_xref(ctx, doc);

	xref = &doc->xref_sections[doc->xref_base];
	if (i >= xref->num_objects)
		pdf_resize_xref(ctx, doc, i + 1);

	sub = xref->subsec;
	assert(sub != NULL && sub->next == NULL);
	assert(i >= sub->start && i < sub->start + sub->len);
	doc->xref_index[i] = 0;
	return &sub->table[i - sub->start];
}

int pdf_xref_is_incremental(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref *xref = &doc->xref_sections[doc->xref_base];
	pdf_xref_subsec *sub = xref->subsec;

	assert(sub != NULL && sub->next == NULL && sub->len == xref->num_objects && sub->start == 0);

	return num < xref->num_objects && sub->table[num].type;
}

/* Used when clearing signatures. Removes the signature
from the list of unsaved signed signatures. */
void pdf_xref_remove_unsaved_signature(fz_context *ctx, pdf_document *doc, pdf_obj *field)
{
	int num = pdf_to_num(ctx, field);
	int idx = doc->xref_index[num];
	pdf_xref *xref = &doc->xref_sections[idx];
	pdf_unsaved_sig **usigptr = &xref->unsaved_sigs;
	pdf_unsaved_sig *usig = xref->unsaved_sigs;

	while (usig)
	{
		pdf_unsaved_sig **nextptr = &usig->next;
		pdf_unsaved_sig *next = usig->next;

		if (usig->field == field)
		{
			if (xref->unsaved_sigs_end == &usig->next)
			{
				if (usig->next)
					xref->unsaved_sigs_end = &usig->next->next;
				else
					xref->unsaved_sigs_end = NULL;
			}
			if (usigptr)
				*usigptr = usig->next;

			usig->next = NULL;
			pdf_drop_obj(ctx, usig->field);
			pdf_drop_signer(ctx, usig->signer);
			fz_free(ctx, usig);

			break;
		}

		usig = next;
		usigptr = nextptr;
	}
}

void pdf_xref_store_unsaved_signature(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_pkcs7_signer *signer)
{
	pdf_xref *xref = &doc->xref_sections[0];
	pdf_unsaved_sig *unsaved_sig;

	/* Record details within the document structure so that contents
	 * and byte_range can be updated with their correct values at
	 * saving time */
	unsaved_sig = fz_malloc_struct(ctx, pdf_unsaved_sig);
	unsaved_sig->field = pdf_keep_obj(ctx, field);
	unsaved_sig->signer = signer->keep(ctx, signer);
	unsaved_sig->next = NULL;
	if (xref->unsaved_sigs_end == NULL)
		xref->unsaved_sigs_end = &xref->unsaved_sigs;

	*xref->unsaved_sigs_end = unsaved_sig;
	xref->unsaved_sigs_end = &unsaved_sig->next;
}

int pdf_xref_obj_is_unsaved_signature(pdf_document *doc, pdf_obj *obj)
{
	int i;
	for (i = 0; i < doc->num_incremental_sections; i++)
	{
		pdf_xref *xref = &doc->xref_sections[i];
		pdf_unsaved_sig *usig;

		for (usig = xref->unsaved_sigs; usig; usig = usig->next)
		{
			if (usig->field == obj)
				return 1;
		}
	}

	return 0;
}

void pdf_ensure_solid_xref(fz_context *ctx, pdf_document *doc, int num)
{
	if (doc->num_xref_sections == 0)
		pdf_populate_next_xref_level(ctx, doc);

	ensure_solid_xref(ctx, doc, num, 0);
}

int pdf_xref_ensure_incremental_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *new_entry, *old_entry;
	pdf_xref_subsec *sub = NULL;
	int i;
	pdf_obj *copy;

	/* Make sure we have created an xref section for incremental updates */
	ensure_incremental_xref(ctx, doc);

	/* Search for the section that contains this object */
	for (i = doc->xref_index[num]; i < doc->num_xref_sections; i++)
	{
		pdf_xref *xref = &doc->xref_sections[i];

		if (num < 0 && num >= xref->num_objects)
			break;
		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			if (sub->start <= num && num < sub->start + sub->len && sub->table[num - sub->start].type)
				break;
		}
		if (sub != NULL)
			break;
	}
	/* sub == NULL implies we did not find it */

	/* If we don't find it, or it's already in the incremental section, return */
	if (i == 0 || sub == NULL)
		return 0;

	copy = pdf_deep_copy_obj(ctx, sub->table[num - sub->start].obj);

	/* Move the object to the incremental section */
	i = doc->xref_index[num];
	doc->xref_index[num] = 0;
	old_entry = &sub->table[num - sub->start];
	fz_try(ctx)
		new_entry = pdf_get_incremental_xref_entry(ctx, doc, num);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, copy);
		doc->xref_index[num] = i;
		fz_rethrow(ctx);
	}
	*new_entry = *old_entry;
	if (new_entry->type == 'o')
	{
		new_entry->type = 'n';
		new_entry->gen = 0;
	}
	/* Better keep a copy. We must override the old entry with
	 * the copy because the caller may be holding a reference to
	 * the original and expect it to end up in the new entry */
	old_entry->obj = copy;
	old_entry->stm_buf = NULL;

	return 1;
}

void pdf_xref_ensure_local_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *new_entry, *old_entry;
	pdf_xref_subsec *sub = NULL;
	int i;
	pdf_xref *xref;
	pdf_obj *copy;

	/* Is it in the local section already? */
	xref = doc->local_xref;
	for (sub = xref->subsec; sub != NULL; sub = sub->next)
	{
		if (sub->start <= num && num < sub->start + sub->len && sub->table[num - sub->start].type)
			break;
	}
	/* If we found it, it's in the local section already. */
	if (sub != NULL)
		return;

	/* Search for the section that contains this object */
	for (i = doc->xref_index[num]; i < doc->num_xref_sections; i++)
	{
		xref = &doc->xref_sections[i];

		if (num < 0 && num >= xref->num_objects)
			break;
		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			if (sub->start <= num && num < sub->start + sub->len && sub->table[num - sub->start].type)
				break;
		}
		if (sub != NULL)
			break;
	}
	/* sub == NULL implies we did not find it */
	if (sub == NULL)
		return; /* No object to find */

	copy = pdf_deep_copy_obj(ctx, sub->table[num - sub->start].obj);

	/* Copy the object to the local section */
	i = doc->xref_index[num];
	doc->xref_index[num] = 0;
	old_entry = &sub->table[num - sub->start];
	fz_try(ctx)
		new_entry = pdf_get_local_xref_entry(ctx, doc, num);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, copy);
		doc->xref_index[num] = i;
		fz_rethrow(ctx);
	}
	*new_entry = *old_entry;
	if (new_entry->type == 'o')
	{
		new_entry->type = 'n';
		new_entry->gen = 0;
	}
	new_entry->stm_buf = NULL;
	new_entry->obj = NULL;
	/* old entry is incremental and may have changes.
	 * Better keep a copy. We must override the old entry with
	 * the copy because the caller may be holding a reference to
	 * the original and expect it to end up in the new entry */
	new_entry->obj = old_entry->obj;
	old_entry->obj = copy;
	new_entry->stm_buf = NULL; /* FIXME */
}

void pdf_replace_xref(fz_context *ctx, pdf_document *doc, pdf_xref_entry *entries, int n)
{
	int *xref_index = NULL;
	pdf_xref *xref = NULL;
	pdf_xref_subsec *sub;

	fz_var(xref_index);
	fz_var(xref);

	fz_try(ctx)
	{
		xref_index = fz_calloc(ctx, n, sizeof(int));
		xref = fz_malloc_struct(ctx, pdf_xref);
		sub = fz_malloc_struct(ctx, pdf_xref_subsec);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, xref);
		fz_free(ctx, xref_index);
		fz_rethrow(ctx);
	}

	sub->table = entries;
	sub->start = 0;
	sub->len = n;

	xref->subsec = sub;
	xref->num_objects = n;
	xref->trailer = pdf_keep_obj(ctx, pdf_trailer(ctx, doc));

	/* The new table completely replaces the previous separate sections */
	pdf_drop_xref_sections(ctx, doc);

	doc->xref_sections = xref;
	doc->num_xref_sections = 1;
	doc->num_incremental_sections = 0;
	doc->xref_base = 0;
	doc->disallow_new_increments = 0;
	doc->max_xref_len = n;

	fz_free(ctx, doc->xref_index);
	doc->xref_index = xref_index;
}

void pdf_forget_xref(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *trailer = pdf_keep_obj(ctx, pdf_trailer(ctx, doc));

	pdf_drop_local_xref_and_resources(ctx, doc);

	if (doc->saved_xref_sections)
		pdf_drop_xref_sections_imp(ctx, doc, doc->saved_xref_sections, doc->saved_num_xref_sections);

	doc->saved_xref_sections = doc->xref_sections;
	doc->saved_num_xref_sections = doc->num_xref_sections;

	doc->xref_sections = NULL;
	doc->startxref = 0;
	doc->num_xref_sections = 0;
	doc->num_incremental_sections = 0;
	doc->xref_base = 0;
	doc->disallow_new_increments = 0;

	fz_try(ctx)
	{
		pdf_get_populating_xref_entry(ctx, doc, 0);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, trailer);
		fz_rethrow(ctx);
	}

	/* Set the trailer of the final xref section. */
	doc->xref_sections[0].trailer = trailer;
}

/*
 * magic version tag and startxref
 */

int
pdf_version(fz_context *ctx, pdf_document *doc)
{
	int version = doc->version;
	fz_try(ctx)
	{
		pdf_obj *obj = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(Version), NULL);
		const char *str = pdf_to_name(ctx, obj);
		if (*str)
			version = 10 * (fz_atof(str) + 0.05f);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "Ignoring broken Root/Version number.");
	}
	return version;
}

static void
pdf_load_version(fz_context *ctx, pdf_document *doc)
{
	char buf[20];

	fz_seek(ctx, doc->file, 0, SEEK_SET);
	fz_read_line(ctx, doc->file, buf, sizeof buf);
	if (strlen(buf) < 5 || (memcmp(buf, "%PDF-", 5) != 0 && memcmp(buf, "%FDF-", 5) != 0))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize version marker");

	if (buf[1] == 'F')
		doc->is_fdf = 1;

	doc->version = 10 * (fz_atof(buf+5) + 0.05f);
	if (doc->version < 10 || doc->version > 17)
		if (doc->version != 20)
			fz_warn(ctx, "unknown PDF version: %d.%d", doc->version / 10, doc->version % 10);
}

static void
pdf_read_start_xref(fz_context *ctx, pdf_document *doc)
{
	unsigned char buf[1024];
	size_t i, n;
	int64_t t;

	fz_seek(ctx, doc->file, 0, SEEK_END);

	doc->file_size = fz_tell(ctx, doc->file);

	t = fz_maxi64(0, doc->file_size - (int64_t)sizeof buf);
	fz_seek(ctx, doc->file, t, SEEK_SET);

	n = fz_read(ctx, doc->file, buf, sizeof buf);
	if (n < 9)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find startxref");

	i = n - 9;
	do
	{
		if (memcmp(buf + i, "startxref", 9) == 0)
		{
			i += 9;
			while (i < n && iswhite(buf[i]))
				i ++;
			doc->startxref = 0;
			while (i < n && isdigit(buf[i]))
			{
				if (doc->startxref >= INT64_MAX/10)
					fz_throw(ctx, FZ_ERROR_GENERIC, "startxref too large");
				doc->startxref = doc->startxref * 10 + (buf[i++] - '0');
			}
			if (doc->startxref != 0)
				return;
			break;
		}
	} while (i-- > 0);

	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find startxref");
}

void fz_skip_space(fz_context *ctx, fz_stream *stm)
{
	do
	{
		int c = fz_peek_byte(ctx, stm);
		if (c == EOF || c > 32)
			return;
		(void)fz_read_byte(ctx, stm);
	}
	while (1);
}

int fz_skip_string(fz_context *ctx, fz_stream *stm, const char *str)
{
	while (*str)
	{
		int c = fz_peek_byte(ctx, stm);
		if (c == EOF || c != *str++)
			return 1;
		(void)fz_read_byte(ctx, stm);
	}
	return 0;
}

/*
 * trailer dictionary
 */

static int
pdf_xref_size_from_old_trailer(fz_context *ctx, pdf_document *doc)
{
	int len;
	char *s;
	int64_t t;
	pdf_token tok;
	int c;
	int size = 0;
	int64_t ofs;
	pdf_obj *trailer = NULL;
	size_t n;
	pdf_lexbuf *buf = &doc->lexbuf.base;
	pdf_obj *obj = NULL;

	fz_var(trailer);

	/* Record the current file read offset so that we can reinstate it */
	ofs = fz_tell(ctx, doc->file);

	fz_skip_space(ctx, doc->file);
	if (fz_skip_string(ctx, doc->file, "xref"))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find xref marker");
	fz_skip_space(ctx, doc->file);

	while (1)
	{
		c = fz_peek_byte(ctx, doc->file);
		if (!isdigit(c))
			break;

		fz_read_line(ctx, doc->file, buf->scratch, buf->size);
		s = buf->scratch;
		fz_strsep(&s, " "); /* ignore start */
		if (!s)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref subsection length missing");
		len = fz_atoi(fz_strsep(&s, " "));
		if (len < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref subsection length must be positive");

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
			fz_seek(ctx, doc->file, -(2 + (int)strlen(s)), SEEK_CUR);

		t = fz_tell(ctx, doc->file);
		if (t < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot tell in file");

		/* Spec says xref entries should be 20 bytes, but it's not infrequent
		 * to see 19, in particular for some PCLm drivers. Cope. */
		if (len > 0)
		{
			n = fz_read(ctx, doc->file, (unsigned char *)buf->scratch, 20);
			if (n < 19)
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed xref table");
			if (n == 20 && buf->scratch[19] > 32)
				n = 19;
		}
		else
			n = 20;

		if (len > (int64_t)((INT64_MAX - t) / n))
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref has too many entries");

		fz_seek(ctx, doc->file, t + n * (int64_t)len, SEEK_SET);
	}

	fz_try(ctx)
	{
		tok = pdf_lex(ctx, doc->file, buf);
		if (tok != PDF_TOK_TRAILER)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected trailer marker");

		tok = pdf_lex(ctx, doc->file, buf);
		if (tok != PDF_TOK_OPEN_DICT)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected trailer dictionary");

		trailer = pdf_parse_dict(ctx, doc, doc->file, buf);

		obj = pdf_dict_get(ctx, trailer, PDF_NAME(Size));
		if (pdf_is_indirect(ctx, obj))
			fz_throw(ctx, FZ_ERROR_GENERIC, "trailer Size entry is indirect");

		size = pdf_dict_get_int(ctx, trailer, PDF_NAME(Size));
		if (size < 0 || size > PDF_MAX_OBJECT_NUMBER + 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "trailer Size entry out of range");
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, trailer);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	fz_seek(ctx, doc->file, ofs, SEEK_SET);

	return size;
}

static pdf_xref_entry *
pdf_xref_find_subsection(fz_context *ctx, pdf_document *doc, int start, int len)
{
	pdf_xref *xref = &doc->xref_sections[doc->num_xref_sections-1];
	pdf_xref_subsec *sub, *extend = NULL;
	int num_objects;
	int solidify = 0;

	if (len == 0)
		return NULL;

	/* Different cases here.
	 * Case 1) We might be asking for a subsection (or a subset of a
	 *         subsection) that we already have - Just return it.
	 * Case 2) We might be asking for a subsection that overlaps (or
	 *         extends) a subsection we already have - extend the existing one.
	 * Case 3) We might be asking for a subsection that overlaps multiple
	 *         existing subsections - solidify the whole set.
	 * Case 4) We might be asking for a completely new subsection - just
	 *         allocate it.
	 */

	/* Sanity check */
	for (sub = xref->subsec; sub != NULL; sub = sub->next)
	{
		if (start >= sub->start && start <= sub->start + sub->len)
		{
			/* 'start' is in (or immediately after) 'sub' */
			if (start + len <= sub->start + sub->len)
			{
				/* And so is start+len-1 - just return this! Case 1. */
				return &sub->table[start-sub->start];
			}
			/* So we overlap with sub. */
			if (extend == NULL)
			{
				/* Maybe we can extend sub? */
				extend = sub;
			}
			else
			{
				/* OK, so we've already found an overlapping one. We'll need to solidify. Case 3. */
				solidify = 1;
				break;
			}
		}
		else if (start + len > sub->start && start + len < sub->start + sub->len)
		{
			/* The end of the start+len range is in 'sub'. */
			/* For now, we won't support extending sub backwards. Just take this as
			 * needing to solidify. Case 3. */
			solidify = 1;
			break;
		}
		else if (start < sub->start && start + len >= sub->start + sub->len)
		{
			/* The end of the start+len range is beyond 'sub'. */
			/* For now, we won't support extending sub backwards. Just take this as
			 * needing to solidify. Another variant of case 3. */
			solidify = 1;
			break;
		}
	}

	num_objects = xref->num_objects;
	if (num_objects < start + len)
		num_objects = start + len;

	if (solidify)
	{
		/* Case 3: Solidify the xref */
		ensure_solid_xref(ctx, doc, num_objects, doc->num_xref_sections-1);
		xref = &doc->xref_sections[doc->num_xref_sections-1];
		sub = xref->subsec;
	}
	else if (extend)
	{
		/* Case 2: Extend the subsection */
		int newlen = start + len - extend->start;
		sub = extend;
		sub->table = fz_realloc_array(ctx, sub->table, newlen, pdf_xref_entry);
		memset(&sub->table[sub->len], 0, sizeof(pdf_xref_entry) * (newlen - sub->len));
		sub->len = newlen;
		if (xref->num_objects < sub->start + sub->len)
			xref->num_objects = sub->start + sub->len;
		if (doc->max_xref_len < sub->start + sub->len)
			extend_xref_index(ctx, doc, sub->start + sub->len);
	}
	else
	{
		/* Case 4 */
		sub = fz_malloc_struct(ctx, pdf_xref_subsec);
		fz_try(ctx)
		{
			sub->table = fz_malloc_struct_array(ctx, len, pdf_xref_entry);
			sub->start = start;
			sub->len = len;
			sub->next = xref->subsec;
			xref->subsec = sub;
		}
		fz_catch(ctx)
		{
			fz_free(ctx, sub);
			fz_rethrow(ctx);
		}
		if (xref->num_objects < num_objects)
			xref->num_objects = num_objects;
		if (doc->max_xref_len < num_objects)
			extend_xref_index(ctx, doc, num_objects);
	}
	return &sub->table[start-sub->start];
}

static inline void
validate_object_number_range(fz_context *ctx, int first, int len, const char *what)
{
	if (first < 0 || first > PDF_MAX_OBJECT_NUMBER)
		fz_throw(ctx, FZ_ERROR_GENERIC, "first object number in %s out of range", what);
	if (len < 0 || len > PDF_MAX_OBJECT_NUMBER)
		fz_throw(ctx, FZ_ERROR_GENERIC, "number of objects in %s out of range", what);
	if (len > 0 && len - 1 > PDF_MAX_OBJECT_NUMBER - first)
		fz_throw(ctx, FZ_ERROR_GENERIC, "last object number in %s out of range", what);
}

static pdf_obj *
pdf_read_old_xref(fz_context *ctx, pdf_document *doc)
{
	int start, len, c, i, xref_len, carried;
	fz_stream *file = doc->file;
	pdf_xref_entry *table;
	pdf_token tok;
	size_t n;
	char *s, *e;
	pdf_lexbuf *buf = &doc->lexbuf.base;

	xref_len = pdf_xref_size_from_old_trailer(ctx, doc);

	fz_skip_space(ctx, doc->file);
	if (fz_skip_string(ctx, doc->file, "xref"))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find xref marker");
	fz_skip_space(ctx, doc->file);

	while (1)
	{
		c = fz_peek_byte(ctx, file);
		if (!isdigit(c))
			break;

		fz_read_line(ctx, file, buf->scratch, buf->size);
		s = buf->scratch;
		start = fz_atoi(fz_strsep(&s, " "));
		len = fz_atoi(fz_strsep(&s, " "));

		/* broken pdfs where the section is not on a separate line */
		if (s && *s != '\0')
		{
			fz_warn(ctx, "broken xref subsection. proceeding anyway.");
			fz_seek(ctx, file, -(2 + (int)strlen(s)), SEEK_CUR);
		}

		validate_object_number_range(ctx, start, len, "xref subsection");

		/* broken pdfs where size in trailer undershoots entries in xref sections */
		if (start + len > xref_len)
		{
			fz_warn(ctx, "broken xref subsection, proceeding anyway.");
		}

		table = pdf_xref_find_subsection(ctx, doc, start, len);

		/* Xref entries SHOULD be 20 bytes long, but we see 19 byte
		 * ones more frequently than we'd like (e.g. PCLm drivers).
		 * Cope with this by 'carrying' data forward. */
		carried = 0;
		for (i = 0; i < len; i++)
		{
			pdf_xref_entry *entry = &table[i];
			n = fz_read(ctx, file, (unsigned char *) buf->scratch + carried, 20-carried);
			if (n != (size_t)(20-carried))
				fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected EOF in xref table");
			n += carried;
			buf->scratch[n] = '\0';
			if (!entry->type)
			{
				s = buf->scratch;
				e = s + n;

				entry->num = start + i;

				/* broken pdfs where line start with white space */
				while (s < e && iswhite(*s))
					s++;

				if (s == e || !isdigit(*s))
					fz_throw(ctx, FZ_ERROR_GENERIC, "xref offset missing");
				while (s < e && isdigit(*s))
					entry->ofs = entry->ofs * 10 + *s++ - '0';

				while (s < e && iswhite(*s))
					s++;
				if (s == e || !isdigit(*s))
					fz_throw(ctx, FZ_ERROR_GENERIC, "xref generation number missing");
				while (s < e && isdigit(*s))
					entry->gen = entry->gen * 10 + *s++ - '0';

				while (s < e && iswhite(*s))
					s++;
				if (s == e || (*s != 'f' && *s != 'n' && *s != 'o'))
					fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected xref type: 0x%x (%d %d R)", s == e ? 0 : *s, entry->num, entry->gen);
				entry->type = *s++;

				/* If the last byte of our buffer isn't an EOL (or space), carry one byte forward */
				carried = buf->scratch[19] > 32;
				if (carried)
					buf->scratch[0] = buf->scratch[19];
			}
		}
		if (carried)
			fz_unread_byte(ctx, file);
	}

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_TRAILER)
		fz_throw(ctx, FZ_ERROR_GENERIC, "expected trailer marker");

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_OPEN_DICT)
		fz_throw(ctx, FZ_ERROR_GENERIC, "expected trailer dictionary");

	doc->last_xref_was_old_style = 1;

	return pdf_parse_dict(ctx, doc, file, buf);
}

static void
pdf_read_new_xref_section(fz_context *ctx, pdf_document *doc, fz_stream *stm, int i0, int i1, int w0, int w1, int w2)
{
	pdf_xref_entry *table;
	int i, n;

	validate_object_number_range(ctx, i0, i1, "xref subsection");

	table = pdf_xref_find_subsection(ctx, doc, i0, i1);
	for (i = i0; i < i0 + i1; i++)
	{
		pdf_xref_entry *entry = &table[i-i0];
		int a = 0;
		int64_t b = 0;
		int c = 0;

		if (fz_is_eof(ctx, stm))
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated xref stream");

		for (n = 0; n < w0; n++)
			a = (a << 8) + fz_read_byte(ctx, stm);
		for (n = 0; n < w1; n++)
			b = (b << 8) + fz_read_byte(ctx, stm);
		for (n = 0; n < w2; n++)
			c = (c << 8) + fz_read_byte(ctx, stm);

		if (!entry->type)
		{
			int t = w0 ? a : 1;
			entry->type = t == 0 ? 'f' : t == 1 ? 'n' : t == 2 ? 'o' : 0;
			entry->ofs = w1 ? b : 0;
			entry->gen = w2 ? c : 0;
			entry->num = i;
		}
	}

	doc->last_xref_was_old_style = 0;
}

/* Entered with file locked, remains locked throughout. */
static pdf_obj *
pdf_read_new_xref(fz_context *ctx, pdf_document *doc)
{
	fz_stream *stm = NULL;
	pdf_obj *trailer = NULL;
	pdf_obj *index = NULL;
	pdf_obj *obj = NULL;
	int gen, num = 0;
	int64_t ofs, stm_ofs;
	int size, w0, w1, w2;
	int t;

	fz_var(trailer);
	fz_var(stm);

	fz_try(ctx)
	{
		ofs = fz_tell(ctx, doc->file);
		trailer = pdf_parse_ind_obj(ctx, doc, doc->file, &num, &gen, &stm_ofs, NULL);
		if (num == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Trailer object number cannot be 0\n");
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, trailer);
		fz_rethrow(ctx);
	}

	fz_try(ctx)
	{
		pdf_xref_entry *entry;

		obj = pdf_dict_get(ctx, trailer, PDF_NAME(Size));
		if (!obj)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream missing Size entry (%d 0 R)", num);

		size = pdf_to_int(ctx, obj);

		obj = pdf_dict_get(ctx, trailer, PDF_NAME(W));
		if (!obj)
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream missing W entry (%d  R)", num);

		if (pdf_is_indirect(ctx, pdf_array_get(ctx, obj, 0)))
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream object type field width an indirect object");
		if (pdf_is_indirect(ctx, pdf_array_get(ctx, obj, 1)))
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream object field 2 width an indirect object");
		if (pdf_is_indirect(ctx, pdf_array_get(ctx, obj, 2)))
			fz_throw(ctx, FZ_ERROR_GENERIC, "xref stream object field 3 width an indirect object");

		if (doc->file_reading_linearly && pdf_dict_get(ctx, trailer, PDF_NAME(Encrypt)))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot read linearly with encryption");

		w0 = pdf_array_get_int(ctx, obj, 0);
		w1 = pdf_array_get_int(ctx, obj, 1);
		w2 = pdf_array_get_int(ctx, obj, 2);

		if (w0 < 0)
			fz_warn(ctx, "xref stream objects have corrupt type");
		if (w1 < 0)
			fz_warn(ctx, "xref stream objects have corrupt offset");
		if (w2 < 0)
			fz_warn(ctx, "xref stream objects have corrupt generation");

		w0 = w0 < 0 ? 0 : w0;
		w1 = w1 < 0 ? 0 : w1;
		w2 = w2 < 0 ? 0 : w2;

		index = pdf_dict_get(ctx, trailer, PDF_NAME(Index));

		stm = pdf_open_stream_with_offset(ctx, doc, num, trailer, stm_ofs);

		if (!index)
		{
			pdf_read_new_xref_section(ctx, doc, stm, 0, size, w0, w1, w2);
		}
		else
		{
			int n = pdf_array_len(ctx, index);
			for (t = 0; t < n; t += 2)
			{
				int i0 = pdf_array_get_int(ctx, index, t + 0);
				int i1 = pdf_array_get_int(ctx, index, t + 1);
				pdf_read_new_xref_section(ctx, doc, stm, i0, i1, w0, w1, w2);
			}
		}
		entry = pdf_get_populating_xref_entry(ctx, doc, num);
		entry->ofs = ofs;
		entry->gen = gen;
		entry->num = num;
		entry->stm_ofs = stm_ofs;
		pdf_drop_obj(ctx, entry->obj);
		entry->obj = pdf_keep_obj(ctx, trailer);
		entry->type = 'n';
		pdf_set_obj_parent(ctx, trailer, num);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, trailer);
		fz_rethrow(ctx);
	}

	return trailer;
}

static pdf_obj *
pdf_read_xref(fz_context *ctx, pdf_document *doc, int64_t ofs)
{
	pdf_obj *trailer;
	int c;

	fz_seek(ctx, doc->file, ofs, SEEK_SET);

	while (iswhite(fz_peek_byte(ctx, doc->file)))
		fz_read_byte(ctx, doc->file);

	c = fz_peek_byte(ctx, doc->file);
	if (c == 'x')
		trailer = pdf_read_old_xref(ctx, doc);
	else if (isdigit(c))
		trailer = pdf_read_new_xref(ctx, doc);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot recognize xref format");

	return trailer;
}

static int64_t
read_xref_section(fz_context *ctx, pdf_document *doc, int64_t ofs)
{
	pdf_obj *trailer = NULL;
	pdf_obj *prevobj;
	int64_t xrefstmofs = 0;
	int64_t prevofs = 0;

	trailer = pdf_read_xref(ctx, doc, ofs);
	fz_try(ctx)
	{
		pdf_set_populating_xref_trailer(ctx, doc, trailer);

		/* FIXME: do we overwrite free entries properly? */
		/* FIXME: Does this work properly with progression? */
		xrefstmofs = pdf_to_int64(ctx, pdf_dict_get(ctx, trailer, PDF_NAME(XRefStm)));
		if (xrefstmofs)
		{
			if (xrefstmofs < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "negative xref stream offset");

			/*
				Read the XRefStm stream, but throw away the resulting trailer. We do not
				follow any Prev tag therein, as specified on Page 108 of the PDF reference
				1.7
			*/
			pdf_drop_obj(ctx, pdf_read_xref(ctx, doc, xrefstmofs));
		}

		prevobj = pdf_dict_get(ctx, trailer, PDF_NAME(Prev));
		if (pdf_is_int(ctx, prevobj))
		{
			prevofs = pdf_to_int64(ctx, prevobj);
			if (prevofs <= 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid offset for previous xref section");
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, trailer);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return prevofs;
}

static void
pdf_read_xref_sections(fz_context *ctx, pdf_document *doc, int64_t ofs, int read_previous)
{
	int i, len, cap;
	int64_t *offsets;
	int populated = 0;
	int size, xref_len;

	len = 0;
	cap = 10;
	offsets = fz_malloc_array(ctx, cap, int64_t);

	fz_var(populated);

	fz_try(ctx)
	{
		while(ofs)
		{
			for (i = 0; i < len; i ++)
			{
				if (offsets[i] == ofs)
					break;
			}
			if (i < len)
			{
				fz_warn(ctx, "ignoring xref section recursion at offset %d", (int)ofs);
				break;
			}
			if (len == cap)
			{
				cap *= 2;
				offsets = fz_realloc_array(ctx, offsets, cap, int64_t);
			}
			offsets[len++] = ofs;

			pdf_populate_next_xref_level(ctx, doc);
			populated = 1;
			ofs = read_xref_section(ctx, doc, ofs);
			if (!read_previous)
				break;
		}

		/* For pathological files, such as chinese-example.pdf, where the original
		 * xref in the file is highly fragmented, we can safely solidify it here
		 * with no ill effects. */
		ensure_solid_xref(ctx, doc, 0, doc->num_xref_sections-1);

		size = pdf_dict_get_int(ctx, pdf_trailer(ctx, doc), PDF_NAME(Size));
		xref_len = pdf_xref_len(ctx, doc);
		if (xref_len > size)
			fz_throw(ctx, FZ_ERROR_GENERIC, "incorrect number of xref entries in trailer, repairing");
	}
	fz_always(ctx)
	{
		fz_free(ctx, offsets);
	}
	fz_catch(ctx)
	{
		/* Undo pdf_populate_next_xref_level if we've done that already. */
		if (populated)
		{
			pdf_drop_xref_subsec(ctx, &doc->xref_sections[doc->num_xref_sections - 1]);
			doc->num_xref_sections--;
		}
		fz_rethrow(ctx);
	}
}

static void
pdf_prime_xref_index(fz_context *ctx, pdf_document *doc)
{
	int i, j;
	int *idx = doc->xref_index;

	for (i = doc->num_xref_sections-1; i >= 0; i--)
	{
		pdf_xref *xref = &doc->xref_sections[i];
		pdf_xref_subsec *subsec = xref->subsec;
		while (subsec != NULL)
		{
			int start = subsec->start;
			int end = subsec->start + subsec->len;
			for (j = start; j < end; j++)
			{
				char t = subsec->table[j-start].type;
				if (t != 0 && t != 'f')
					idx[j] = i;
			}

			subsec = subsec->next;
		}
	}
}

static void
check_xref_entry_offsets(fz_context *ctx, pdf_xref_entry *entry, int i, pdf_document *doc, void *arg)
{
	int xref_len = (int)(intptr_t)arg;

	if (entry->type == 'n')
	{
		/* Special case code: "0000000000 * n" means free,
		 * according to some producers (inc Quartz) */
		if (entry->ofs == 0)
			entry->type = 'f';
		else if (entry->ofs <= 0 || entry->ofs >= doc->file_size)
			fz_throw(ctx, FZ_ERROR_GENERIC, "object offset out of range: %d (%d 0 R)", (int)entry->ofs, i);
	}
	else if (entry->type == 'o')
	{
		/* Read this into a local variable here, because pdf_get_xref_entry
		 * may solidify the xref, hence invalidating "entry", meaning we
		 * need a stashed value for the throw. */
		int64_t ofs = entry->ofs;
		if (ofs <= 0 || ofs >= xref_len || pdf_get_xref_entry_no_null(ctx, doc, ofs)->type != 'n')
			fz_throw(ctx, FZ_ERROR_GENERIC, "invalid reference to an objstm that does not exist: %d (%d 0 R)", (int)ofs, i);
	}
}

/*
 * load xref tables from pdf
 *
 * File locked on entry, throughout and on exit.
 */

static void
pdf_load_xref(fz_context *ctx, pdf_document *doc)
{
	int xref_len;
	pdf_xref_entry *entry;

	pdf_read_start_xref(ctx, doc);

	pdf_read_xref_sections(ctx, doc, doc->startxref, 1);

	if (pdf_xref_len(ctx, doc) == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "found xref was empty");

	pdf_prime_xref_index(ctx, doc);

	entry = pdf_get_xref_entry_no_null(ctx, doc, 0);
	/* broken pdfs where first object is missing */
	if (!entry->type)
	{
		entry->type = 'f';
		entry->gen = 65535;
		entry->num = 0;
	}
	/* broken pdfs where first object is not free */
	else if (entry->type != 'f')
		fz_warn(ctx, "first object in xref is not free");

	/* broken pdfs where object offsets are out of range */
	xref_len = pdf_xref_len(ctx, doc);
	pdf_xref_entry_map(ctx, doc, check_xref_entry_offsets, (void *)(intptr_t)xref_len);
}

static void
pdf_check_linear(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *dict = NULL;
	pdf_obj *o;
	int num, gen;
	int64_t stmofs;

	fz_var(dict);

	fz_try(ctx)
	{
		dict = pdf_parse_ind_obj(ctx, doc, doc->file, &num, &gen, &stmofs, NULL);
		if (!pdf_is_dict(ctx, dict))
			break;
		o = pdf_dict_get(ctx, dict, PDF_NAME(Linearized));
		if (o == NULL)
			break;
		if (pdf_to_int(ctx, o) != 1)
			break;
		doc->has_linearization_object = 1;
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, dict);
	fz_catch(ctx)
	{
		/* Silently swallow this error. */
	}
}

static void
pdf_load_linear(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *dict = NULL;
	pdf_obj *hint = NULL;
	pdf_obj *o;
	int num, gen, lin, len;
	int64_t stmofs;

	fz_var(dict);
	fz_var(hint);

	fz_try(ctx)
	{
		pdf_xref_entry *entry;

		dict = pdf_parse_ind_obj(ctx, doc, doc->file, &num, &gen, &stmofs, NULL);
		if (!pdf_is_dict(ctx, dict))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to read linearized dictionary");
		o = pdf_dict_get(ctx, dict, PDF_NAME(Linearized));
		if (o == NULL)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to read linearized dictionary");
		lin = pdf_to_int(ctx, o);
		if (lin != 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected version of Linearized tag (%d)", lin);
		doc->has_linearization_object = 1;
		len = pdf_dict_get_int(ctx, dict, PDF_NAME(L));
		if (len != doc->file_length)
			fz_throw(ctx, FZ_ERROR_GENERIC, "File has been updated since linearization");

		pdf_read_xref_sections(ctx, doc, fz_tell(ctx, doc->file), 0);

		doc->linear_page_count = pdf_dict_get_int(ctx, dict, PDF_NAME(N));
		doc->linear_page_refs = fz_realloc_array(ctx, doc->linear_page_refs, doc->linear_page_count, pdf_obj *);
		memset(doc->linear_page_refs, 0, doc->linear_page_count * sizeof(pdf_obj*));
		doc->linear_obj = dict;
		doc->linear_pos = fz_tell(ctx, doc->file);
		doc->linear_page1_obj_num = pdf_dict_get_int(ctx, dict, PDF_NAME(O));
		doc->linear_page_refs[0] = pdf_new_indirect(ctx, doc, doc->linear_page1_obj_num, 0);
		doc->linear_page_num = 0;
		hint = pdf_dict_get(ctx, dict, PDF_NAME(H));
		doc->hint_object_offset = pdf_array_get_int(ctx, hint, 0);
		doc->hint_object_length = pdf_array_get_int(ctx, hint, 1);

		entry = pdf_get_populating_xref_entry(ctx, doc, 0);
		entry->type = 'f';
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, dict);
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Drop back to non linearized reading mode */
		doc->file_reading_linearly = 0;
	}
}

/*
 * Initialize and load xref tables.
 * If password is not null, try to decrypt.
 */

static void
pdf_init_document(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *encrypt, *id;
	int repaired = 0;

	fz_try(ctx)
	{
		/* Check to see if we should work in progressive mode */
		if (doc->file->progressive)
		{
			doc->file_reading_linearly = 1;
			fz_seek(ctx, doc->file, 0, SEEK_END);
			doc->file_length = fz_tell(ctx, doc->file);
			if (doc->file_length < 0)
				doc->file_length = 0;
			fz_seek(ctx, doc->file, 0, SEEK_SET);
		}

		pdf_load_version(ctx, doc);

		if (doc->is_fdf)
		{
			doc->file_reading_linearly = 0;
			repaired = 1;
			break; /* skip to end of try/catch */
		}

		/* Try to load the linearized file if we are in progressive
		 * mode. */
		if (doc->file_reading_linearly)
			pdf_load_linear(ctx, doc);
		else
			/* Even if we're not in progressive mode, check to see
			 * if the file claims to be linearized. This is important
			 * for checking signatures later on. */
			pdf_check_linear(ctx, doc);

		/* If we aren't in progressive mode (or the linear load failed
		 * and has set us back to non-progressive mode), load normally.
		 */
		if (!doc->file_reading_linearly)
			pdf_load_xref(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_drop_xref_sections(ctx, doc);
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		doc->file_reading_linearly = 0;
		fz_warn(ctx, "trying to repair broken xref");
		repaired = 1;
	}

	fz_try(ctx)
	{
		if (repaired)
		{
			/* pdf_repair_xref may access xref_index, so reset it properly */
			if (doc->xref_index)
				memset(doc->xref_index, 0, sizeof(int) * doc->max_xref_len);
			pdf_repair_xref(ctx, doc);
			pdf_prime_xref_index(ctx, doc);
		}

		encrypt = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt));
		id = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(ID));
		if (pdf_is_dict(ctx, encrypt))
			doc->crypt = pdf_new_crypt(ctx, encrypt, id);

		/* Allow lazy clients to read encrypted files with a blank password */
		(void)pdf_authenticate_password(ctx, doc, "");

		if (repaired)
		{
			pdf_repair_trailer(ctx, doc);
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_repair_trailer(fz_context *ctx, pdf_document *doc)
{
	int hasroot, hasinfo;
	pdf_obj *obj, *nobj;
	pdf_obj *dict = NULL;
	int i;

	int xref_len = pdf_xref_len(ctx, doc);
	pdf_repair_obj_stms(ctx, doc);

	hasroot = (pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)) != NULL);
	hasinfo = (pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info)) != NULL);

	fz_var(dict);

	fz_try(ctx)
	{
		/* Scan from the end so we have a better chance of finding
		 * newer objects if there are multiple instances of Info and
		 * Root objects.
		 */
		for (i = xref_len - 1; i > 0 && (!hasinfo || !hasroot); --i)
		{
			pdf_xref_entry *entry = pdf_get_xref_entry_no_null(ctx, doc, i);
			if (entry->type == 0 || entry->type == 'f')
				continue;

			fz_try(ctx)
			{
				dict = pdf_load_object(ctx, doc, i);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "ignoring broken object (%d 0 R)", i);
				continue;
			}

			if (!hasroot)
			{
				obj = pdf_dict_get(ctx, dict, PDF_NAME(Type));
				if (obj == PDF_NAME(Catalog))
				{
					nobj = pdf_new_indirect(ctx, doc, i, 0);
					pdf_dict_put_drop(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), nobj);
					hasroot = 1;
				}
			}

			if (!hasinfo)
			{
				if (pdf_dict_get(ctx, dict, PDF_NAME(Creator)) || pdf_dict_get(ctx, dict, PDF_NAME(Producer)))
				{
					nobj = pdf_new_indirect(ctx, doc, i, 0);
					pdf_dict_put_drop(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info), nobj);
					hasinfo = 1;
				}
			}

			pdf_drop_obj(ctx, dict);
			dict = NULL;
		}
	}
	fz_always(ctx)
	{
		/* ensure that strings are not used in their repaired, non-decrypted form */
		if (doc->crypt)
		{
			pdf_crypt *tmp;
			pdf_clear_xref(ctx, doc);

			/* ensure that Encryption dictionary and ID are cached without decryption,
			   otherwise a decrypted Encryption dictionary and ID may be used when saving
			   the PDF causing it to be inconsistent (since strings/streams are encrypted
			   with the actual encryption key, not the decrypted encryption key). */
			tmp = doc->crypt;
			doc->crypt = NULL;
			fz_try(ctx)
			{
				(void) pdf_resolve_indirect(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt)));
				(void) pdf_resolve_indirect(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(ID)));
			}
			fz_always(ctx)
				doc->crypt = tmp;
			fz_catch(ctx)
			{
				fz_rethrow(ctx);
			}
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, dict);
		fz_rethrow(ctx);
	}
}

void
pdf_invalidate_xfa(fz_context *ctx, pdf_document *doc)
{
	if (doc == NULL)
		return;
	fz_drop_xml(ctx, doc->xfa);
	doc->xfa = NULL;
}

static void
pdf_drop_document_imp(fz_context *ctx, pdf_document *doc)
{
	int i;

	fz_defer_reap_start(ctx);

	/* Type3 glyphs in the glyph cache can contain pdf_obj pointers
	 * that we are about to destroy. Simplest solution is to bin the
	 * glyph cache at this point. */
	fz_try(ctx)
		fz_purge_glyph_cache(ctx);
	fz_catch(ctx)
	{
		/* Swallow error, but continue dropping */
	}

	pdf_set_doc_event_callback(ctx, doc, NULL, NULL, NULL);
	pdf_drop_js(ctx, doc->js);

	pdf_drop_journal(ctx, doc->journal);

	pdf_drop_resource_tables(ctx, doc);

	pdf_drop_local_xref(ctx, doc->local_xref);

	pdf_drop_xref_sections(ctx, doc);
	fz_free(ctx, doc->xref_index);

	fz_drop_stream(ctx, doc->file);
	pdf_drop_crypt(ctx, doc->crypt);

	pdf_drop_obj(ctx, doc->linear_obj);
	if (doc->linear_page_refs)
	{
		for (i=0; i < doc->linear_page_count; i++)
			pdf_drop_obj(ctx, doc->linear_page_refs[i]);

		fz_free(ctx, doc->linear_page_refs);
	}

	fz_free(ctx, doc->hint_page);
	fz_free(ctx, doc->hint_shared_ref);
	fz_free(ctx, doc->hint_shared);
	fz_free(ctx, doc->hint_obj_offsets);

	for (i=0; i < doc->num_type3_fonts; i++)
	{
		fz_try(ctx)
			fz_decouple_type3_font(ctx, doc->type3_fonts[i], (void *)doc);
		fz_always(ctx)
			fz_drop_font(ctx, doc->type3_fonts[i]);
		fz_catch(ctx)
		{
			/* Swallow error, but continue dropping */
		}
	}

	fz_free(ctx, doc->type3_fonts);

	pdf_drop_ocg(ctx, doc);

	pdf_empty_store(ctx, doc);

	pdf_lexbuf_fin(ctx, &doc->lexbuf.base);

	fz_drop_colorspace(ctx, doc->oi);

	for (i = 0; i < doc->orphans_count; i++)
		pdf_drop_obj(ctx, doc->orphans[i]);

	fz_free(ctx, doc->orphans);

	pdf_drop_page_tree_internal(ctx, doc);

	fz_defer_reap_end(ctx);

	pdf_invalidate_xfa(ctx, doc);
}

void
pdf_drop_document(fz_context *ctx, pdf_document *doc)
{
	fz_drop_document(ctx, &doc->super);
}

pdf_document *
pdf_keep_document(fz_context *ctx, pdf_document *doc)
{
	return (pdf_document *)fz_keep_document(ctx, &doc->super);
}

/*
 * compressed object streams
 */

/*
	Do not hold pdf_xref_entry's over call to this function as they
	may be invalidated!
*/
static pdf_xref_entry *
pdf_load_obj_stm(fz_context *ctx, pdf_document *doc, int num, pdf_lexbuf *buf, int target)
{
	fz_stream *stm = NULL;
	pdf_obj *objstm = NULL;
	int *numbuf = NULL;
	int64_t *ofsbuf = NULL;

	pdf_obj *obj;
	int64_t first;
	int count;
	int i;
	pdf_token tok;
	pdf_xref_entry *ret_entry = NULL;
	int ret_idx;
	int xref_len;
	int found;
	fz_stream *sub = NULL;

	fz_var(numbuf);
	fz_var(ofsbuf);
	fz_var(objstm);
	fz_var(stm);
	fz_var(sub);

	fz_try(ctx)
	{
		objstm = pdf_load_object(ctx, doc, num);

		if (pdf_obj_marked(ctx, objstm))
			fz_throw(ctx, FZ_ERROR_GENERIC, "recursive object stream lookup");
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, objstm);
		fz_rethrow(ctx);
	}

	fz_try(ctx)
	{
		(void)pdf_mark_obj(ctx, objstm);

		count = pdf_dict_get_int(ctx, objstm, PDF_NAME(N));
		first = pdf_dict_get_int(ctx, objstm, PDF_NAME(First));

		validate_object_number_range(ctx, first, count, "object stream");

		numbuf = fz_calloc(ctx, count, sizeof(*numbuf));
		ofsbuf = fz_calloc(ctx, count, sizeof(*ofsbuf));

		xref_len = pdf_xref_len(ctx, doc);

		found = 0;

		stm = pdf_open_stream_number(ctx, doc, num);
		for (i = 0; i < count; i++)
		{
			tok = pdf_lex(ctx, stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d 0 R)", num);
			numbuf[found] = buf->i;

			tok = pdf_lex(ctx, stm, buf);
			if (tok != PDF_TOK_INT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "corrupt object stream (%d 0 R)", num);
			ofsbuf[found] = buf->i;

			if (numbuf[found] <= 0 || numbuf[found] >= xref_len)
				fz_warn(ctx, "object stream object out of range, skipping");
			else
				found++;
		}

		ret_idx = -1;
		for (i = 0; i < found; i++)
		{
			pdf_xref_entry *entry;
			uint64_t length;
			int64_t offset;

			offset = first + ofsbuf[i];
			if (i+1 < found)
				length = ofsbuf[i+1] - ofsbuf[i];
			else
				length = UINT64_MAX;

			sub = fz_open_null_filter(ctx, stm, length, offset);

			obj = pdf_parse_stm_obj(ctx, doc, sub, buf);
			fz_drop_stream(ctx, sub);
			sub = NULL;

			entry = pdf_get_xref_entry_no_null(ctx, doc, numbuf[i]);

			pdf_set_obj_parent(ctx, obj, numbuf[i]);

			/* We may have set entry->type to be 'O' from being 'o' to avoid nasty
			 * recursions in pdf_cache_object. Accept the type being 'O' here. */
			if ((entry->type == 'o' || entry->type == 'O') && entry->ofs == num)
			{
				/* If we already have an entry for this object,
				 * we'd like to drop it and use the new one -
				 * but this means that anyone currently holding
				 * a pointer to the old one will be left with a
				 * stale pointer. Instead, we drop the new one
				 * and trust that the old one is correct. */
				if (entry->obj)
				{
					if (pdf_objcmp(ctx, entry->obj, obj))
						fz_warn(ctx, "Encountered new definition for object %d - keeping the original one", numbuf[i]);
					pdf_drop_obj(ctx, obj);
				}
				else
				{
					entry->obj = obj;
					fz_drop_buffer(ctx, entry->stm_buf);
					entry->stm_buf = NULL;
				}
				if (numbuf[i] == target)
					ret_idx = i;
			}
			else
			{
				pdf_drop_obj(ctx, obj);
			}
		}
		/* Parsing our way through the stream can cause the xref to be
		 * solidified, which will move an entry. We therefore can't
		 * read the entry for returning until no more parsing is to be
		 * done. Thus we end up reading this entry twice. */
		if (ret_idx >= 0)
			ret_entry = pdf_get_xref_entry_no_null(ctx, doc, numbuf[ret_idx]);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_drop_stream(ctx, sub);
		fz_free(ctx, ofsbuf);
		fz_free(ctx, numbuf);
		pdf_unmark_obj(ctx, objstm);
		pdf_drop_obj(ctx, objstm);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	return ret_entry;
}

/*
 * object loading
 */
static int
pdf_obj_read(fz_context *ctx, pdf_document *doc, int64_t *offset, int *nump, pdf_obj **page)
{
	pdf_lexbuf *buf = &doc->lexbuf.base;
	int num, gen, tok;
	int64_t numofs, genofs, stmofs, tmpofs, newtmpofs;
	int xref_len;
	pdf_xref_entry *entry;

	numofs = *offset;
	fz_seek(ctx, doc->file, numofs, SEEK_SET);

	/* We expect to read 'num' here */
	tok = pdf_lex(ctx, doc->file, buf);
	genofs = fz_tell(ctx, doc->file);
	if (tok != PDF_TOK_INT)
	{
		/* Failed! */
		DEBUGMESS((ctx, "skipping unexpected data (tok=%d) at %d", tok, *offset));
		*offset = genofs;
		return tok == PDF_TOK_EOF;
	}
	*nump = num = buf->i;

	/* We expect to read 'gen' here */
	tok = pdf_lex(ctx, doc->file, buf);
	tmpofs = fz_tell(ctx, doc->file);
	if (tok != PDF_TOK_INT)
	{
		/* Failed! */
		DEBUGMESS((ctx, "skipping unexpected data after \"%d\" (tok=%d) at %d", num, tok, *offset));
		*offset = tmpofs;
		return tok == PDF_TOK_EOF;
	}
	gen = buf->i;

	/* We expect to read 'obj' here */
	do
	{
		tmpofs = fz_tell(ctx, doc->file);
		tok = pdf_lex(ctx, doc->file, buf);
		if (tok == PDF_TOK_OBJ)
			break;
		if (tok != PDF_TOK_INT)
		{
			DEBUGMESS((ctx, "skipping unexpected data (tok=%d) at %d", tok, tmpofs));
			*offset = fz_tell(ctx, doc->file);
			return tok == PDF_TOK_EOF;
		}
		DEBUGMESS((ctx, "skipping unexpected int %d at %d", num, numofs));
		*nump = num = gen;
		numofs = genofs;
		gen = buf->i;
		genofs = tmpofs;
	}
	while (1);

	/* Now we read the actual object */
	xref_len = pdf_xref_len(ctx, doc);

	/* When we are reading a progressive file, we typically see:
	 *    File Header
	 *    obj m (Linearization params)
	 *    xref #1 (refers to objects m-n)
	 *    obj m+1
	 *    ...
	 *    obj n
	 *    obj 1
	 *    ...
	 *    obj n-1
	 *    xref #2
	 *
	 * The linearisation params are read elsewhere, hence
	 * whenever we read an object it should just go into the
	 * previous xref.
	 */
	tok = pdf_repair_obj(ctx, doc, buf, &stmofs, NULL, NULL, NULL, page, &newtmpofs, NULL);

	do /* So we can break out of it */
	{
		if (num <= 0 || num >= xref_len)
		{
			fz_warn(ctx, "Not a valid object number (%d %d obj)", num, gen);
			break;
		}
		if (gen != 0)
		{
			fz_warn(ctx, "Unexpected non zero generation number in linearized file");
		}
		entry = pdf_get_populating_xref_entry(ctx, doc, num);
		if (entry->type != 0)
		{
			DEBUGMESS((ctx, "Duplicate object found (%d %d obj)", num, gen));
			break;
		}
		if (page && *page)
		{
			DEBUGMESS((ctx, "Successfully read object %d @ %d - and found page %d!", num, numofs, doc->linear_page_num));
			if (!entry->obj)
				entry->obj = pdf_keep_obj(ctx, *page);

			if (doc->linear_page_refs[doc->linear_page_num] == NULL)
				doc->linear_page_refs[doc->linear_page_num] = pdf_new_indirect(ctx, doc, num, gen);
		}
		else
		{
			DEBUGMESS((ctx, "Successfully read object %d @ %d", num, numofs));
		}
		entry->type = 'n';
		entry->gen = gen; // XXX: was 0
		entry->num = num;
		entry->ofs = numofs;
		entry->stm_ofs = stmofs;
	}
	while (0);
	if (page && *page)
		doc->linear_page_num++;

	if (tok == PDF_TOK_ENDOBJ)
	{
		*offset = fz_tell(ctx, doc->file);
	}
	else
	{
		*offset = newtmpofs;
	}
	return 0;
}

static void
pdf_load_hinted_page(fz_context *ctx, pdf_document *doc, int pagenum)
{
	pdf_obj *page = NULL;

	if (!doc->hints_loaded || !doc->linear_page_refs)
		return;

	if (doc->linear_page_refs[pagenum])
		return;

	fz_var(page);

	fz_try(ctx)
	{
		int num = doc->hint_page[pagenum].number;
		page = pdf_load_object(ctx, doc, num);
		if (pdf_name_eq(ctx, PDF_NAME(Page), pdf_dict_get(ctx, page, PDF_NAME(Type))))
		{
			/* We have found the page object! */
			DEBUGMESS((ctx, "LoadHintedPage pagenum=%d num=%d", pagenum, num));
			doc->linear_page_refs[pagenum] = pdf_new_indirect(ctx, doc, num, 0);
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, page);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Silently swallow the error and proceed as normal */
	}
}

static int
read_hinted_object(fz_context *ctx, pdf_document *doc, int num)
{
	/* Try to find the object using our hint table. Find the closest
	 * object <= the one we want that has a hint and read forward from
	 * there. */
	int expected = num;
	int curr_pos;
	int64_t start, offset;

	while (doc->hint_obj_offsets[expected] == 0 && expected > 0)
		expected--;
	if (expected != num)
		DEBUGMESS((ctx, "object %d is unhinted, will search forward from %d", expected, num));
	if (expected == 0)	/* No hints found, just bail */
		return 0;

	curr_pos = fz_tell(ctx, doc->file);
	offset = doc->hint_obj_offsets[expected];

	fz_var(expected);

	fz_try(ctx)
	{
		int found;

		/* Try to read forward from there */
		do
		{
			start = offset;
			DEBUGMESS((ctx, "Searching for object %d @ %d", expected, offset));
			pdf_obj_read(ctx, doc, &offset, &found, 0);
			DEBUGMESS((ctx, "Found object %d - next will be @ %d", found, offset));
			if (found <= expected)
			{
				/* We found the right one (or one earlier than
				 * we expected). Update the hints. */
				doc->hint_obj_offsets[expected] = offset;
				doc->hint_obj_offsets[found] = start;
				doc->hint_obj_offsets[found+1] = offset;
				/* Retry with the next one */
				expected = found+1;
			}
			else
			{
				/* We found one later than we expected. */
				doc->hint_obj_offsets[expected] = 0;
				doc->hint_obj_offsets[found] = start;
				doc->hint_obj_offsets[found+1] = offset;
				while (doc->hint_obj_offsets[expected] == 0 && expected > 0)
					expected--;
				if (expected == 0)	/* No hints found, we give up */
					break;
			}
		}
		while (found != num);
	}
	fz_always(ctx)
	{
		fz_seek(ctx, doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* FIXME: Currently we ignore the hint. Perhaps we should
		 * drop back to non-hinted operation here. */
		doc->hint_obj_offsets[expected] = 0;
		fz_rethrow(ctx);
	}
	return expected != 0;
}

pdf_obj *
pdf_load_unencrypted_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *x;

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		fz_throw(ctx, FZ_ERROR_GENERIC, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(ctx, doc));

	x = pdf_get_xref_entry_no_null(ctx, doc, num);
	if (x->type == 'n')
	{
		fz_seek(ctx, doc->file, x->ofs, SEEK_SET);
		return pdf_parse_ind_obj(ctx, doc, doc->file, NULL, NULL, NULL, NULL);
	}
	return NULL;
}

pdf_xref_entry *
pdf_cache_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *x;
	int rnum, rgen, try_repair;

	fz_var(try_repair);

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		fz_throw(ctx, FZ_ERROR_GENERIC, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(ctx, doc));

object_updated:
	try_repair = 0;
	rnum = num;

	x = pdf_get_xref_entry(ctx, doc, num);
	if (x == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find object in xref (%d 0 R)", num);

	if (x->obj != NULL)
		return x;

	if (x->type == 'f')
	{
		x->obj = PDF_NULL;
	}
	else if (x->type == 'n')
	{
		fz_seek(ctx, doc->file, x->ofs, SEEK_SET);

		fz_try(ctx)
		{
			x->obj = pdf_parse_ind_obj(ctx, doc, doc->file,
					&rnum, &rgen, &x->stm_ofs, &try_repair);
		}
		fz_catch(ctx)
		{
			if (!try_repair || fz_caught(ctx) == FZ_ERROR_TRYLATER)
				fz_rethrow(ctx);
		}

		if (!try_repair && rnum != num)
		{
			pdf_drop_obj(ctx, x->obj);
			x->type = 'f';
			x->ofs = -1;
			x->gen = 0;
			x->num = 0;
			x->stm_ofs = 0;
			x->obj = NULL;
			try_repair = (doc->repair_attempted == 0);
		}

		if (try_repair)
		{
perform_repair:
			fz_try(ctx)
			{
				pdf_repair_xref(ctx, doc);
				pdf_prime_xref_index(ctx, doc);
				pdf_repair_obj_stms(ctx, doc);
				pdf_repair_trailer(ctx, doc);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_rethrow_if(ctx, FZ_ERROR_REPAIRED);
				if (rnum == num)
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot parse object (%d 0 R)", num);
				else
					fz_throw(ctx, FZ_ERROR_GENERIC, "found object (%d 0 R) instead of (%d 0 R)", rnum, num);
			}
			goto object_updated;
		}

		if (doc->crypt)
			pdf_crypt_obj(ctx, doc->crypt, x->obj, x->num, x->gen);
	}
	else if (x->type == 'o')
	{
		if (!x->obj)
		{
			pdf_xref_entry *orig_x = x;
			pdf_xref_entry *ox = x; /* This init is unused, but it shuts warnings up. */
			orig_x->type = 'O'; /* Mark this node so we know we're recursing. */
			fz_try(ctx)
				x = pdf_load_obj_stm(ctx, doc, x->ofs, &doc->lexbuf.base, num);
			fz_always(ctx)
			{
				/* Most of the time ox == orig_x, but if pdf_load_obj_stm performed a
				 * repair, it may not be. It is safe to call pdf_get_xref_entry_no_change
				 * here, as it does not try/catch. */
				ox = pdf_get_xref_entry_no_change(ctx, doc, num);
				/* Bug 706762: ox can be NULL if the object went away during a repair. */
				if (ox && ox->type == 'O')
					ox->type = 'o'; /* Not recursing any more. */
			}
			fz_catch(ctx)
				fz_rethrow(ctx);
			if (x == NULL)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot load object stream containing object (%d 0 R)", num);
			if (!x->obj)
			{
				x->type = 'f';
				if (ox)
					ox->type = 'f';
				if (doc->repair_attempted)
					fz_throw(ctx, FZ_ERROR_GENERIC, "object (%d 0 R) was not found in its object stream", num);
				goto perform_repair;
			}
		}
	}
	else if (doc->hint_obj_offsets && read_hinted_object(ctx, doc, num))
	{
		goto object_updated;
	}
	else if (doc->file_length && doc->linear_pos < doc->file_length)
	{
		fz_throw(ctx, FZ_ERROR_TRYLATER, "cannot find object in xref (%d 0 R) - not loaded yet?", num);
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find object in xref (%d 0 R)", num);
	}

	pdf_set_obj_parent(ctx, x->obj, num);
	return x;
}

pdf_obj *
pdf_load_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *entry = pdf_cache_object(ctx, doc, num);
	return pdf_keep_obj(ctx, entry->obj);
}

pdf_obj *
pdf_resolve_indirect(fz_context *ctx, pdf_obj *ref)
{
	if (pdf_is_indirect(ctx, ref))
	{
		pdf_document *doc = pdf_get_indirect_document(ctx, ref);
		int num = pdf_to_num(ctx, ref);
		pdf_xref_entry *entry;

		if (!doc)
			return NULL;
		if (num <= 0)
		{
			fz_warn(ctx, "invalid indirect reference (%d 0 R)", num);
			return NULL;
		}

		fz_try(ctx)
			entry = pdf_cache_object(ctx, doc, num);
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_REPAIRED);
			fz_warn(ctx, "cannot load object (%d 0 R) into cache", num);
			return NULL;
		}

		ref = entry->obj;
	}
	return ref;
}

pdf_obj *
pdf_resolve_indirect_chain(fz_context *ctx, pdf_obj *ref)
{
	int sanity = 10;

	while (pdf_is_indirect(ctx, ref))
	{
		if (--sanity == 0)
		{
			fz_warn(ctx, "too many indirections (possible indirection cycle involving %d 0 R)", pdf_to_num(ctx, ref));
			return NULL;
		}

		ref = pdf_resolve_indirect(ctx, ref);
	}

	return ref;
}

int
pdf_count_objects(fz_context *ctx, pdf_document *doc)
{
	return pdf_xref_len(ctx, doc);
}

int
pdf_is_local_object(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	pdf_xref *xref = doc->local_xref;
	pdf_xref_subsec *sub;
	int num;

	if (!pdf_is_indirect(ctx, obj))
		return 0;

	if (xref == NULL)
		return 0; /* no local xref present */

	num = pdf_to_num(ctx, obj);

	/* Local xrefs only ever have 1 section, and it should be solid. */
	sub = xref->subsec;
	if (num >= sub->start && num < sub->start + sub->len)
		return sub->table[num - sub->start].type != 0;

	return 0;
}

static int
pdf_create_local_object(fz_context *ctx, pdf_document *doc)
{
	/* TODO: reuse free object slots by properly linking free object chains in the ofs field */
	pdf_xref_entry *entry;
	int num;

	num = doc->local_xref->num_objects;

	entry = pdf_get_local_xref_entry(ctx, doc, num);
	entry->type = 'f';
	entry->ofs = -1;
	entry->gen = 0;
	entry->num = num;
	entry->stm_ofs = 0;
	entry->stm_buf = NULL;
	entry->obj = NULL;
	return num;
}

int
pdf_create_object(fz_context *ctx, pdf_document *doc)
{
	/* TODO: reuse free object slots by properly linking free object chains in the ofs field */
	pdf_xref_entry *entry;
	int num;

	if (doc->local_xref && doc->local_xref_nesting > 0)
		return pdf_create_local_object(ctx, doc);

	num = pdf_xref_len(ctx, doc);

	if (num > PDF_MAX_OBJECT_NUMBER)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too many objects stored in pdf");

	entry = pdf_get_incremental_xref_entry(ctx, doc, num);
	entry->type = 'f';
	entry->ofs = -1;
	entry->gen = 0;
	entry->num = num;
	entry->stm_ofs = 0;
	entry->stm_buf = NULL;
	entry->obj = NULL;

	pdf_add_journal_fragment(ctx, doc, num, NULL, NULL, 1);

	return num;
}

static void
pdf_delete_local_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *x;

	if (doc->local_xref == NULL || doc->local_xref_nesting == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "No local xref to delete from!");

	if (num <= 0 || num >= doc->local_xref->num_objects)
	{
		fz_warn(ctx, "local object out of range (%d 0 R); xref size %d", num, doc->local_xref->num_objects);
		return;
	}

	x = pdf_get_local_xref_entry(ctx, doc, num);

	fz_drop_buffer(ctx, x->stm_buf);
	pdf_drop_obj(ctx, x->obj);

	x->type = 'f';
	x->ofs = 0;
	x->gen += 1;
	x->num = 0;
	x->stm_ofs = 0;
	x->stm_buf = NULL;
	x->obj = NULL;
}

void
pdf_delete_object(fz_context *ctx, pdf_document *doc, int num)
{
	pdf_xref_entry *x;
	pdf_xref *xref;
	int j;

	if (doc->local_xref && doc->local_xref_nesting > 0)
	{
		pdf_delete_local_object(ctx, doc, num);
		return;
	}

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
	{
		fz_warn(ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(ctx, doc));
		return;
	}

	x = pdf_get_incremental_xref_entry(ctx, doc, num);

	fz_drop_buffer(ctx, x->stm_buf);
	pdf_drop_obj(ctx, x->obj);

	x->type = 'f';
	x->ofs = 0;
	x->gen += 1;
	x->num = 0;
	x->stm_ofs = 0;
	x->stm_buf = NULL;
	x->obj = NULL;

	/* Currently we've left a 'free' object in the incremental
	 * section. This is enough to cause us to think that the
	 * document has changes. Check back in the non-incremental
	 * sections to see if the last instance of the object there
	 * was free (or if this object never appeared). If so, we
	 * can mark this object as non-existent in the incremental
	 * xref. This is important so we can 'undo' back to emptiness
	 * after we save/when we reload a snapshot. */
	for (j = 1; j < doc->num_xref_sections; j++)
	{
		xref = &doc->xref_sections[j];

		if (num < xref->num_objects)
		{
			pdf_xref_subsec *sub;
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				pdf_xref_entry *entry;

				if (num < sub->start || num >= sub->start + sub->len)
					continue;

				entry = &sub->table[num - sub->start];
				if (entry->type)
				{
					if (entry->type == 'f')
					{
						/* It was free already! */
						x->type = 0;
						x->gen = 0;
					}
					/* It was a real object. */
					return;
				}
			}
		}
	}
	/* It never appeared before. */
	x->type = 0;
	x->gen = 0;
}

static void
pdf_update_local_object(fz_context *ctx, pdf_document *doc, int num, pdf_obj *newobj)
{
	pdf_xref_entry *x;

	if (doc->local_xref == NULL || doc->local_xref_nesting == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't update local object without a local xref");

	if (!newobj)
	{
		pdf_delete_local_object(ctx, doc, num);
		return;
	}

	x = pdf_get_local_xref_entry(ctx, doc, num);

	pdf_drop_obj(ctx, x->obj);

	x->type = 'n';
	x->ofs = 0;
	x->obj = pdf_keep_obj(ctx, newobj);

	pdf_set_obj_parent(ctx, newobj, num);
}

void
pdf_update_object(fz_context *ctx, pdf_document *doc, int num, pdf_obj *newobj)
{
	pdf_xref_entry *x;

	if (doc->local_xref && doc->local_xref_nesting > 0)
	{
		pdf_update_local_object(ctx, doc, num, newobj);
		return;
	}

	if (num <= 0 || num >= pdf_xref_len(ctx, doc))
	{
		fz_warn(ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(ctx, doc));
		return;
	}

	if (!newobj)
	{
		pdf_delete_object(ctx, doc, num);
		return;
	}

	x = pdf_get_incremental_xref_entry(ctx, doc, num);

	pdf_drop_obj(ctx, x->obj);

	x->type = 'n';
	x->ofs = 0;
	x->obj = pdf_keep_obj(ctx, newobj);

	pdf_set_obj_parent(ctx, newobj, num);
}

void
pdf_update_stream(fz_context *ctx, pdf_document *doc, pdf_obj *obj, fz_buffer *newbuf, int compressed)
{
	int num;
	pdf_xref_entry *x;

	if (pdf_is_indirect(ctx, obj))
		num = pdf_to_num(ctx, obj);
	else
		num = pdf_obj_parent_num(ctx, obj);

	/* Write the Length first, as this has the effect of moving the
	 * old object into the journal for undo. This also moves the
	 * stream buffer with it, keeping it consistent. */
	pdf_dict_put_int(ctx, obj, PDF_NAME(Length), fz_buffer_storage(ctx, newbuf, NULL));

	if (doc->local_xref && doc->local_xref_nesting > 0)
	{
		x = pdf_get_local_xref_entry(ctx, doc, num);
	}
	else
	{
		if (num <= 0 || num >= pdf_xref_len(ctx, doc))
		{
			fz_warn(ctx, "object out of range (%d 0 R); xref size %d", num, pdf_xref_len(ctx, doc));
			return;
		}

		x = pdf_get_xref_entry_no_null(ctx, doc, num);
	}

	fz_drop_buffer(ctx, x->stm_buf);
	x->stm_buf = fz_keep_buffer(ctx, newbuf);

	if (!compressed)
	{
		pdf_dict_del(ctx, obj, PDF_NAME(Filter));
		pdf_dict_del(ctx, obj, PDF_NAME(DecodeParms));
	}
}

int
pdf_lookup_metadata(fz_context *ctx, pdf_document *doc, const char *key, char *buf, int size)
{
	if (!strcmp(key, FZ_META_FORMAT))
	{
		int version = pdf_version(ctx, doc);
		return 1 + (int)fz_snprintf(buf, size, "PDF %d.%d", version/10, version % 10);
	}

	if (!strcmp(key, FZ_META_ENCRYPTION))
	{
		if (doc->crypt)
		{
			const char *stream_method = pdf_crypt_stream_method(ctx, doc->crypt);
			const char *string_method = pdf_crypt_string_method(ctx, doc->crypt);
			if (stream_method == string_method)
				return 1 + (int)fz_snprintf(buf, size, "Standard V%d R%d %d-bit %s",
						pdf_crypt_version(ctx, doc->crypt),
						pdf_crypt_revision(ctx, doc->crypt),
						pdf_crypt_length(ctx, doc->crypt),
						pdf_crypt_string_method(ctx, doc->crypt));
			else
				return 1 + (int)fz_snprintf(buf, size, "Standard V%d R%d %d-bit streams: %s strings: %s",
						pdf_crypt_version(ctx, doc->crypt),
						pdf_crypt_revision(ctx, doc->crypt),
						pdf_crypt_length(ctx, doc->crypt),
						pdf_crypt_stream_method(ctx, doc->crypt),
						pdf_crypt_string_method(ctx, doc->crypt));
		}
		else
			return 1 + (int)fz_strlcpy(buf, "None", size);
	}

	if (strstr(key, "info:") == key)
	{
		pdf_obj *info;
		const char *s;
		int n;

		info = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info));
		if (!info)
			return -1;

		info = pdf_dict_gets(ctx, info, key + 5);
		if (!info)
			return -1;

		s = pdf_to_text_string(ctx, info);
		if (strlen(s) <= 0)
			return -1;

		n = 1 + (int)fz_strlcpy(buf, s, size);
		return n;
	}

	return -1;
}

void
pdf_set_metadata(fz_context *ctx, pdf_document *doc, const char *key, const char *value)
{

	pdf_obj *info = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info));

	pdf_begin_operation(ctx, doc, "Set Metadata");

	fz_try(ctx)
	{
		/* Ensure we have an Info dictionary. */
		if (!pdf_is_dict(ctx, info))
		{
			info = pdf_add_new_dict(ctx, doc, 8);
			pdf_dict_put_drop(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info), info);
		}

		if (!strcmp(key, FZ_META_INFO_TITLE))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Title), value);
		else if (!strcmp(key, FZ_META_INFO_AUTHOR))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Author), value);
		else if (!strcmp(key, FZ_META_INFO_SUBJECT))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Subject), value);
		else if (!strcmp(key, FZ_META_INFO_KEYWORDS))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Keywords), value);
		else if (!strcmp(key, FZ_META_INFO_CREATOR))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Creator), value);
		else if (!strcmp(key, FZ_META_INFO_PRODUCER))
			pdf_dict_put_text_string(ctx, info, PDF_NAME(Producer), value);
		else if (!strcmp(key, FZ_META_INFO_CREATIONDATE))
		{
			int64_t time = pdf_parse_date(ctx, value);
			if (time >= 0)
				pdf_dict_put_date(ctx, info, PDF_NAME(CreationDate), time);
		}
		else if (!strcmp(key, FZ_META_INFO_MODIFICATIONDATE))
		{
			int64_t time = pdf_parse_date(ctx, value);
			if (time >= 0)
				pdf_dict_put_date(ctx, info, PDF_NAME(ModDate), time);
		}

		if (!strncmp(key, FZ_META_INFO, strlen(FZ_META_INFO)))
			key += strlen(FZ_META_INFO);
		pdf_dict_put_text_string(ctx, info, pdf_new_name(ctx, key), value);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

static fz_link_dest
pdf_resolve_link_imp(fz_context *ctx, fz_document *doc_, const char *uri)
{
	pdf_document *doc = (pdf_document*)doc_;
	return pdf_resolve_link_dest(ctx, doc, uri);
}

char *pdf_format_link_uri(fz_context *ctx, fz_document *doc, fz_link_dest dest)
{
	return pdf_new_uri_from_explicit_dest(ctx, dest);
}

/*
	Initializers for the fz_document interface.

	The functions are split across two files to allow calls to a
	version of the constructor that does not link in the interpreter.
	The interpreter references the built-in font and cmap resources
	which are quite big. Not linking those into the mutool binary
	saves roughly 6MB of space.
*/

static pdf_document *
pdf_new_document(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = fz_new_derived_document(ctx, pdf_document);

#ifndef NDEBUG
	{
		void pdf_verify_name_table_sanity(void);
		pdf_verify_name_table_sanity();
	}
#endif

	doc->super.drop_document = (fz_document_drop_fn*)pdf_drop_document_imp;
	doc->super.get_output_intent = (fz_document_output_intent_fn*)pdf_document_output_intent;
	doc->super.needs_password = (fz_document_needs_password_fn*)pdf_needs_password;
	doc->super.authenticate_password = (fz_document_authenticate_password_fn*)pdf_authenticate_password;
	doc->super.has_permission = (fz_document_has_permission_fn*)pdf_has_permission;
	doc->super.outline_iterator = (fz_document_outline_iterator_fn*)pdf_new_outline_iterator;
	doc->super.resolve_link_dest = pdf_resolve_link_imp;
	doc->super.format_link_uri = pdf_format_link_uri;
	doc->super.count_pages = pdf_count_pages_imp;
	doc->super.load_page = pdf_load_page_imp;
	doc->super.page_label = pdf_page_label_imp;
	doc->super.lookup_metadata = (fz_document_lookup_metadata_fn*)pdf_lookup_metadata;
	doc->super.set_metadata = (fz_document_set_metadata_fn*)pdf_set_metadata;

	pdf_lexbuf_init(ctx, &doc->lexbuf.base, PDF_LEXBUF_LARGE);
	doc->file = fz_keep_stream(ctx, file);

	/* Default to PDF-1.7 if the version header is missing and for new documents */
	doc->version = 17;

	return doc;
}

pdf_document *
pdf_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	pdf_document *doc = pdf_new_document(ctx, file);
	fz_try(ctx)
	{
		pdf_init_document(ctx, doc);
	}
	fz_catch(ctx)
	{
		/* fz_drop_document may clobber our error code/message so we have to stash them temporarily. */
		char message[256];
		int caught = fz_caught(ctx);
		fz_strlcpy(message, fz_caught_message(ctx), sizeof message);
		fz_drop_document(ctx, &doc->super);
		fz_throw(ctx, caught, "%s", message);
	}
	return doc;
}

/* Uncomment the following to test progressive loading. */
/* #define TEST_PROGRESSIVE_HACK */

pdf_document *
pdf_open_document(fz_context *ctx, const char *filename)
{
	fz_stream *file = NULL;
	pdf_document *doc = NULL;

	fz_var(file);
	fz_var(doc);

	fz_try(ctx)
	{
		file = fz_open_file(ctx, filename);
#ifdef TEST_PROGRESSIVE_HACK
		file->progressive = 1;
#endif
		doc = pdf_new_document(ctx, file);
		pdf_init_document(ctx, doc);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, file);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

#ifdef TEST_PROGRESSIVE_HACK
	if (doc->file_reading_linearly)
	{
		fz_try(ctx)
			pdf_progressive_advance(ctx, doc, doc->linear_page_count-1);
		fz_catch(ctx)
		{
			doc->file_reading_linearly = 0;
			/* swallow the error */
		}
	}
#endif

	return doc;
}

static void
pdf_load_hints(fz_context *ctx, pdf_document *doc, int objnum)
{
	fz_stream *stream = NULL;
	pdf_obj *dict;

	fz_var(stream);
	fz_var(dict);

	fz_try(ctx)
	{
		int i, j, least_num_page_objs, page_obj_num_bits;
		int least_page_len, page_len_num_bits, shared_hint_offset;
		/* int least_page_offset, page_offset_num_bits; */
		/* int least_content_stream_len, content_stream_len_num_bits; */
		int num_shared_obj_num_bits, shared_obj_num_bits;
		/* int numerator_bits, denominator_bits; */
		int shared;
		int shared_obj_num, shared_obj_offset, shared_obj_count_page1;
		int shared_obj_count_total;
		int least_shared_group_len, shared_group_len_num_bits;
		int max_object_num = pdf_xref_len(ctx, doc);

		stream = pdf_open_stream_number(ctx, doc, objnum);
		dict = pdf_get_xref_entry_no_null(ctx, doc, objnum)->obj;
		if (dict == NULL || !pdf_is_dict(ctx, dict))
			fz_throw(ctx, FZ_ERROR_GENERIC, "malformed hint object");

		shared_hint_offset = pdf_dict_get_int(ctx, dict, PDF_NAME(S));

		/* Malloc the structures (use realloc to cope with the fact we
		 * may try this several times before enough data is loaded) */
		doc->hint_page = fz_realloc_array(ctx, doc->hint_page, doc->linear_page_count+1, pdf_hint_page);
		memset(doc->hint_page, 0, sizeof(*doc->hint_page) * (doc->linear_page_count+1));
		doc->hint_obj_offsets = fz_realloc_array(ctx, doc->hint_obj_offsets, max_object_num, int64_t);
		memset(doc->hint_obj_offsets, 0, sizeof(*doc->hint_obj_offsets) * max_object_num);
		doc->hint_obj_offsets_max = max_object_num;

		/* Read the page object hints table: Header first */
		least_num_page_objs = fz_read_bits(ctx, stream, 32);
		/* The following is sometimes a lie, but we read this version,
		 * as other table values are built from it. In
		 * pdf_reference17.pdf, this points to 2 objects before the
		 * first pages page object. */
		doc->hint_page[0].offset = fz_read_bits(ctx, stream, 32);
		if (doc->hint_page[0].offset > doc->hint_object_offset)
			doc->hint_page[0].offset += doc->hint_object_length;
		page_obj_num_bits = fz_read_bits(ctx, stream, 16);
		least_page_len = fz_read_bits(ctx, stream, 32);
		page_len_num_bits = fz_read_bits(ctx, stream, 16);
		/* least_page_offset = */ (void) fz_read_bits(ctx, stream, 32);
		/* page_offset_num_bits = */ (void) fz_read_bits(ctx, stream, 16);
		/* least_content_stream_len = */ (void) fz_read_bits(ctx, stream, 32);
		/* content_stream_len_num_bits = */ (void) fz_read_bits(ctx, stream, 16);
		num_shared_obj_num_bits = fz_read_bits(ctx, stream, 16);
		shared_obj_num_bits = fz_read_bits(ctx, stream, 16);
		/* numerator_bits = */ (void) fz_read_bits(ctx, stream, 16);
		/* denominator_bits = */ (void) fz_read_bits(ctx, stream, 16);

		/* Item 1: Page object numbers */
		doc->hint_page[0].number = doc->linear_page1_obj_num;
		/* We don't care about the number of objects in the first page */
		(void)fz_read_bits(ctx, stream, page_obj_num_bits);
		j = 1;
		for (i = 1; i < doc->linear_page_count; i++)
		{
			int delta_page_objs = fz_read_bits(ctx, stream, page_obj_num_bits);

			doc->hint_page[i].number = j;
			j += least_num_page_objs + delta_page_objs;
		}
		doc->hint_page[i].number = j; /* Not a real page object */
		fz_sync_bits(ctx, stream);
		/* Item 2: Page lengths */
		j = doc->hint_page[0].offset;
		for (i = 0; i < doc->linear_page_count; i++)
		{
			int delta_page_len = fz_read_bits(ctx, stream, page_len_num_bits);
			int old = j;

			doc->hint_page[i].offset = j;
			j += least_page_len + delta_page_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		doc->hint_page[i].offset = j;
		fz_sync_bits(ctx, stream);
		/* Item 3: Shared references */
		shared = 0;
		for (i = 0; i < doc->linear_page_count; i++)
		{
			int num_shared_objs = fz_read_bits(ctx, stream, num_shared_obj_num_bits);
			doc->hint_page[i].index = shared;
			shared += num_shared_objs;
		}
		doc->hint_page[i].index = shared;
		doc->hint_shared_ref = fz_realloc_array(ctx, doc->hint_shared_ref, shared, int);
		memset(doc->hint_shared_ref, 0, sizeof(*doc->hint_shared_ref) * shared);
		fz_sync_bits(ctx, stream);
		/* Item 4: Shared references */
		for (i = 0; i < shared; i++)
		{
			int ref = fz_read_bits(ctx, stream, shared_obj_num_bits);
			doc->hint_shared_ref[i] = ref;
		}
		/* Skip items 5,6,7 as we don't use them */

		fz_seek(ctx, stream, shared_hint_offset, SEEK_SET);

		/* Read the shared object hints table: Header first */
		shared_obj_num = fz_read_bits(ctx, stream, 32);
		shared_obj_offset = fz_read_bits(ctx, stream, 32);
		if (shared_obj_offset > doc->hint_object_offset)
			shared_obj_offset += doc->hint_object_length;
		shared_obj_count_page1 = fz_read_bits(ctx, stream, 32);
		shared_obj_count_total = fz_read_bits(ctx, stream, 32);
		shared_obj_num_bits = fz_read_bits(ctx, stream, 16);
		least_shared_group_len = fz_read_bits(ctx, stream, 32);
		shared_group_len_num_bits = fz_read_bits(ctx, stream, 16);

		/* Sanity check the references in Item 4 above to ensure we
		 * don't access out of range with malicious files. */
		for (i = 0; i < shared; i++)
		{
			if (doc->hint_shared_ref[i] >= shared_obj_count_total)
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed hint stream (shared refs)");
			}
		}

		doc->hint_shared = fz_realloc_array(ctx, doc->hint_shared, shared_obj_count_total+1, pdf_hint_shared);
		memset(doc->hint_shared, 0, sizeof(*doc->hint_shared) * (shared_obj_count_total+1));

		/* Item 1: Shared references */
		j = doc->hint_page[0].offset;
		for (i = 0; i < shared_obj_count_page1; i++)
		{
			int off = fz_read_bits(ctx, stream, shared_group_len_num_bits);
			int old = j;
			doc->hint_shared[i].offset = j;
			j += off + least_shared_group_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		/* FIXME: We would have problems recreating the length of the
		 * last page 1 shared reference group. But we'll never need
		 * to, so ignore it. */
		j = shared_obj_offset;
		for (; i < shared_obj_count_total; i++)
		{
			int off = fz_read_bits(ctx, stream, shared_group_len_num_bits);
			int old = j;
			doc->hint_shared[i].offset = j;
			j += off + least_shared_group_len;
			if (old <= doc->hint_object_offset && j > doc->hint_object_offset)
				j += doc->hint_object_length;
		}
		doc->hint_shared[i].offset = j;
		fz_sync_bits(ctx, stream);
		/* Item 2: Signature flags: read these just so we can skip */
		for (i = 0; i < shared_obj_count_total; i++)
		{
			doc->hint_shared[i].number = fz_read_bits(ctx, stream, 1);
		}
		fz_sync_bits(ctx, stream);
		/* Item 3: Signatures: just skip */
		for (i = 0; i < shared_obj_count_total; i++)
		{
			if (doc->hint_shared[i].number)
			{
				(void) fz_read_bits(ctx, stream, 128);
			}
		}
		fz_sync_bits(ctx, stream);
		/* Item 4: Shared object object numbers */
		j = doc->linear_page1_obj_num; /* FIXME: This is a lie! */
		for (i = 0; i < shared_obj_count_page1; i++)
		{
			doc->hint_shared[i].number = j;
			j += fz_read_bits(ctx, stream, shared_obj_num_bits) + 1;
		}
		j = shared_obj_num;
		for (; i < shared_obj_count_total; i++)
		{
			doc->hint_shared[i].number = j;
			j += fz_read_bits(ctx, stream, shared_obj_num_bits) + 1;
		}
		doc->hint_shared[i].number = j;

		/* Now, actually use the data we have gathered. */
		for (i = 0 /*shared_obj_count_page1*/; i < shared_obj_count_total; i++)
		{
			if (doc->hint_shared[i].number >= 0 && doc->hint_shared[i].number < max_object_num)
				doc->hint_obj_offsets[doc->hint_shared[i].number] = doc->hint_shared[i].offset;
		}
		for (i = 0; i < doc->linear_page_count; i++)
		{
			if (doc->hint_page[i].number >= 0 && doc->hint_page[i].number < max_object_num)
				doc->hint_obj_offsets[doc->hint_page[i].number] = doc->hint_page[i].offset;
		}
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Don't try to load hints again */
		doc->hints_loaded = 1;
		/* We won't use the linearized object anymore. */
		doc->file_reading_linearly = 0;
		/* Any other error becomes a TRYLATER */
		fz_throw(ctx, FZ_ERROR_TRYLATER, "malformed hints object");
	}
	doc->hints_loaded = 1;
}

static void
pdf_load_hint_object(fz_context *ctx, pdf_document *doc)
{
	pdf_lexbuf *buf = &doc->lexbuf.base;
	int64_t curr_pos;

	curr_pos = fz_tell(ctx, doc->file);
	fz_seek(ctx, doc->file, doc->hint_object_offset, SEEK_SET);
	fz_try(ctx)
	{
		while (1)
		{
			pdf_obj *page = NULL;
			int num, tok;

			tok = pdf_lex(ctx, doc->file, buf);
			if (tok != PDF_TOK_INT)
				break;
			num = buf->i;
			tok = pdf_lex(ctx, doc->file, buf);
			if (tok != PDF_TOK_INT)
				break;
			/* Ignore gen = buf->i */
			tok = pdf_lex(ctx, doc->file, buf);
			if (tok != PDF_TOK_OBJ)
				break;
			(void)pdf_repair_obj(ctx, doc, buf, NULL, NULL, NULL, NULL, &page, NULL, NULL);
			pdf_load_hints(ctx, doc, num);
		}
	}
	fz_always(ctx)
	{
		fz_seek(ctx, doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

pdf_obj *pdf_progressive_advance(fz_context *ctx, pdf_document *doc, int pagenum)
{
	int curr_pos;
	pdf_obj *page = NULL;

	pdf_load_hinted_page(ctx, doc, pagenum);

	if (pagenum < 0 || pagenum >= doc->linear_page_count)
		fz_throw(ctx, FZ_ERROR_GENERIC, "page load out of range (%d of %d)", pagenum, doc->linear_page_count);

	if (doc->linear_pos == doc->file_length)
		return doc->linear_page_refs[pagenum];

	/* Only load hints once, and then only after we have got page 0 */
	if (pagenum > 0 && !doc->hints_loaded && doc->hint_object_offset > 0 && doc->linear_pos >= doc->hint_object_offset)
	{
		/* Found hint object */
		pdf_load_hint_object(ctx, doc);
	}

	DEBUGMESS((ctx, "continuing to try to advance from %d", doc->linear_pos));
	curr_pos = fz_tell(ctx, doc->file);

	fz_var(page);

	fz_try(ctx)
	{
		int eof;
		do
		{
			int num;
			eof = pdf_obj_read(ctx, doc, &doc->linear_pos, &num, &page);
			pdf_drop_obj(ctx, page);
			page = NULL;
		}
		while (!eof);

		{
			pdf_obj *catalog;
			pdf_obj *pages;
			doc->linear_pos = doc->file_length;
			pdf_load_xref(ctx, doc);
			catalog = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			pages = pdf_dict_get(ctx, catalog, PDF_NAME(Pages));

			if (!pdf_is_dict(ctx, pages))
				fz_throw(ctx, FZ_ERROR_GENERIC, "missing page tree");
			break;
		}
	}
	fz_always(ctx)
	{
		fz_seek(ctx, doc->file, curr_pos, SEEK_SET);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, page);
		if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		{
			if (doc->linear_page_refs[pagenum] == NULL)
			{
				/* Still not got a page */
				fz_rethrow(ctx);
			}
		}
		else
			fz_rethrow(ctx);
	}

	return doc->linear_page_refs[pagenum];
}

pdf_document *pdf_document_from_fz_document(fz_context *ctx, fz_document *ptr)
{
	return (pdf_document *)((ptr && ptr->count_pages == pdf_count_pages_imp) ? ptr : NULL);
}

pdf_page *pdf_page_from_fz_page(fz_context *ctx, fz_page *ptr)
{
	return (pdf_page *)((ptr && ptr->bound_page == (fz_page_bound_page_fn*)pdf_bound_page) ? ptr : NULL);
}

pdf_document *pdf_specifics(fz_context *ctx, fz_document *doc)
{
	return pdf_document_from_fz_document(ctx, doc);
}

pdf_obj *
pdf_add_object(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	pdf_document *orig_doc;
	int num;

	orig_doc = pdf_get_bound_document(ctx, obj);
	if (orig_doc && orig_doc != doc)
		fz_throw(ctx, FZ_ERROR_GENERIC, "tried to add an object belonging to a different document");
	if (pdf_is_indirect(ctx, obj))
		return pdf_keep_obj(ctx, obj);
	num = pdf_create_object(ctx, doc);
	pdf_update_object(ctx, doc, num, obj);
	return pdf_new_indirect(ctx, doc, num, 0);
}

pdf_obj *
pdf_add_object_drop(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	pdf_obj *ind = NULL;
	fz_try(ctx)
		ind = pdf_add_object(ctx, doc, obj);
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return ind;
}

pdf_obj *
pdf_add_new_dict(fz_context *ctx, pdf_document *doc, int initial)
{
	return pdf_add_object_drop(ctx, doc, pdf_new_dict(ctx, doc, initial));
}

pdf_obj *
pdf_add_new_array(fz_context *ctx, pdf_document *doc, int initial)
{
	return pdf_add_object_drop(ctx, doc, pdf_new_array(ctx, doc, initial));
}

pdf_obj *
pdf_add_stream(fz_context *ctx, pdf_document *doc, fz_buffer *buf, pdf_obj *obj, int compressed)
{
	pdf_obj *ind;
	if (!obj)
		ind = pdf_add_new_dict(ctx, doc, 4);
	else
		ind = pdf_add_object(ctx, doc, obj);
	fz_try(ctx)
		pdf_update_stream(ctx, doc, ind, buf, compressed);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, ind);
		fz_rethrow(ctx);
	}
	return ind;
}

pdf_document *pdf_create_document(fz_context *ctx)
{
	pdf_document *doc;
	pdf_obj *root;
	pdf_obj *pages;
	pdf_obj *trailer = NULL;

	fz_var(trailer);

	doc = pdf_new_document(ctx, NULL);
	fz_try(ctx)
	{
		doc->file_size = 0;
		doc->startxref = 0;
		doc->num_xref_sections = 0;
		doc->num_incremental_sections = 0;
		doc->xref_base = 0;
		doc->disallow_new_increments = 0;
		pdf_get_populating_xref_entry(ctx, doc, 0);

		trailer = pdf_new_dict(ctx, doc, 2);
		pdf_dict_put_int(ctx, trailer, PDF_NAME(Size), 3);
		pdf_dict_put_drop(ctx, trailer, PDF_NAME(Root), root = pdf_add_new_dict(ctx, doc, 2));
		pdf_dict_put(ctx, root, PDF_NAME(Type), PDF_NAME(Catalog));
		pdf_dict_put_drop(ctx, root, PDF_NAME(Pages), pages = pdf_add_new_dict(ctx, doc, 3));
		pdf_dict_put(ctx, pages, PDF_NAME(Type), PDF_NAME(Pages));
		pdf_dict_put_int(ctx, pages, PDF_NAME(Count), 0);
		pdf_dict_put_array(ctx, pages, PDF_NAME(Kids), 1);

		/* Set the trailer of the final xref section. */
		doc->xref_sections[0].trailer = trailer;
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, trailer);
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}
	return doc;
}

static const char *pdf_extensions[] =
{
	"pdf",
	"pclm",
	"ai",
	NULL
};

static const char *pdf_mimetypes[] =
{
	"application/pdf",
	"application/PCLm",
	NULL
};

static int
pdf_recognize_doc_content(fz_context *ctx, fz_stream *stream)
{
	const char *match = "%PDF-";
	int pos = 0;
	int n = 4096+5;
	int c;

	do
	{
		c = fz_read_byte(ctx, stream);
		if (c == EOF)
			return 0;
		if (c == match[pos])
		{
			pos++;
			if (pos == 5)
				return 100;
		}
		else
		{
			/* Restart matching, but recheck c against the start. */
			pos = (c == match[0]);
		}
	}
	while (--n > 0);

	return 0;
}

fz_document_handler pdf_document_handler =
{
	NULL,
	(fz_document_open_fn*)pdf_open_document,
	(fz_document_open_with_stream_fn*)pdf_open_document_with_stream,
	pdf_extensions,
	pdf_mimetypes,
	NULL,
	NULL,
	pdf_recognize_doc_content
};

void pdf_mark_xref(fz_context *ctx, pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];
		pdf_xref_subsec *sub;

		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			for (e = 0; e < sub->len; e++)
			{
				pdf_xref_entry *entry = &sub->table[e];
				if (entry->obj)
				{
					entry->marked = 1;
				}
			}
		}
	}
}

void pdf_clear_xref(fz_context *ctx, pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];
		pdf_xref_subsec *sub;

		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			for (e = 0; e < sub->len; e++)
			{
				pdf_xref_entry *entry = &sub->table[e];
				/* We cannot drop objects if the stream
				 * buffer has been updated */
				if (entry->obj != NULL && entry->stm_buf == NULL)
				{
					if (pdf_obj_refs(ctx, entry->obj) == 1)
					{
						pdf_drop_obj(ctx, entry->obj);
						entry->obj = NULL;
					}
				}
			}
		}
	}
}

void pdf_clear_xref_to_mark(fz_context *ctx, pdf_document *doc)
{
	int x, e;

	for (x = 0; x < doc->num_xref_sections; x++)
	{
		pdf_xref *xref = &doc->xref_sections[x];
		pdf_xref_subsec *sub;

		for (sub = xref->subsec; sub != NULL; sub = sub->next)
		{
			for (e = 0; e < sub->len; e++)
			{
				pdf_xref_entry *entry = &sub->table[e];

				/* We cannot drop objects if the stream buffer has
				 * been updated */
				if (entry->obj != NULL && entry->stm_buf == NULL)
				{
					if (!entry->marked && pdf_obj_refs(ctx, entry->obj) == 1)
					{
						pdf_drop_obj(ctx, entry->obj);
						entry->obj = NULL;
					}
				}
			}
		}
	}
}

int
pdf_count_versions(fz_context *ctx, pdf_document *doc)
{
	return doc->num_xref_sections-doc->num_incremental_sections-doc->has_linearization_object;
}

int
pdf_count_unsaved_versions(fz_context *ctx, pdf_document *doc)
{
	return doc->num_incremental_sections;
}

int
pdf_doc_was_linearized(fz_context *ctx, pdf_document *doc)
{
	return doc->has_linearization_object;
}

static int pdf_obj_exists(fz_context *ctx, pdf_document *doc, int i)
{
	pdf_xref_subsec *sub;
	int j;

	if (i < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Negative object number requested");

	if (i <= doc->max_xref_len)
		j = doc->xref_index[i];
	else
		j = 0;

	/* We may be accessing an earlier version of the document using xref_base
	 * and j may be an index into a later xref section */
	if (doc->xref_base > j)
		j = doc->xref_base;

	/* Find the first xref section where the entry is defined. */
	for (; j < doc->num_xref_sections; j++)
	{
		pdf_xref *xref = &doc->xref_sections[j];

		if (i < xref->num_objects)
		{
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				if (i < sub->start || i >= sub->start + sub->len)
					continue;

				if (sub->table[i - sub->start].type)
					return 1;
			}
		}
	}

	return 0;
}

enum {
	FIELD_CHANGED = 1,
	FIELD_CHANGE_VALID = 2,
	FIELD_CHANGE_INVALID = 4
};

typedef struct
{
	int num_obj;
	int obj_changes[1];
} pdf_changes;

static int
check_unchanged_between(fz_context *ctx, pdf_document *doc, pdf_changes *changes, pdf_obj *nobj, pdf_obj *oobj)
{
	int marked = 0;
	int changed = 0;

	/* Trivially identical => trivially unchanged. */
	if (nobj == oobj)
		return 0;

	/* Strictly speaking we shouldn't need to call fz_var,
	 * but I suspect static analysis tools are not smart
	 * enough to figure that out. */
	fz_var(marked);

	if (pdf_is_indirect(ctx, nobj))
	{
		int o_xref_base = doc->xref_base;

		/* Both must be indirect if one is. */
		if (!pdf_is_indirect(ctx, oobj))
		{
			changes->obj_changes[pdf_to_num(ctx, nobj)] |= FIELD_CHANGE_INVALID;
			return 1;
		}

		/* Handle recursing back into ourselves. */
		if (pdf_obj_marked(ctx, nobj))
		{
			if (pdf_obj_marked(ctx, oobj))
				return 0;
			changes->obj_changes[pdf_to_num(ctx, nobj)] |= FIELD_CHANGE_INVALID;
			return 1;
		}
		else if (pdf_obj_marked(ctx, oobj))
		{
			changes->obj_changes[pdf_to_num(ctx, nobj)] |= FIELD_CHANGE_INVALID;
			return 1;
		}

		nobj = pdf_resolve_indirect_chain(ctx, nobj);
		doc->xref_base = o_xref_base+1;
		fz_try(ctx)
		{
			oobj = pdf_resolve_indirect_chain(ctx, oobj);
			if (oobj != nobj)
			{
				/* Different objects, so lock them */
				if (!pdf_obj_marked(ctx, nobj) && !pdf_obj_marked(ctx, oobj))
				{
					(void)pdf_mark_obj(ctx, nobj);
					(void)pdf_mark_obj(ctx, oobj);
					marked = 1;
				}
			}
		}
		fz_always(ctx)
			doc->xref_base = o_xref_base;
		fz_catch(ctx)
			fz_rethrow(ctx);

		if (nobj == oobj)
			return 0; /* Trivially identical */
	}

	fz_var(changed);

	fz_try(ctx)
	{
		if (pdf_is_dict(ctx, nobj))
		{
			int i, n = pdf_dict_len(ctx, nobj);

			if (!pdf_is_dict(ctx, oobj) || n != pdf_dict_len(ctx, oobj))
			{
change_found:
				changes->obj_changes[pdf_to_num(ctx, nobj)] |= FIELD_CHANGE_INVALID;
				changed = 1;
				break;
			}

			for (i = 0; i < n; i++)
			{
				pdf_obj *key = pdf_dict_get_key(ctx, nobj, i);
				pdf_obj *nval = pdf_dict_get(ctx, nobj, key);
				pdf_obj *oval = pdf_dict_get(ctx, oobj, key);

				changed |= check_unchanged_between(ctx, doc, changes, nval, oval);
			}
		}
		else if (pdf_is_array(ctx, nobj))
		{
			int i, n = pdf_array_len(ctx, nobj);

			if (!pdf_is_array(ctx, oobj) || n != pdf_array_len(ctx, oobj))
				goto change_found;

			for (i = 0; i < n; i++)
			{
				pdf_obj *nval = pdf_array_get(ctx, nobj, i);
				pdf_obj *oval = pdf_array_get(ctx, oobj, i);

				changed |= check_unchanged_between(ctx, doc, changes, nval, oval);
			}
		}
		else if (pdf_objcmp(ctx, nobj, oobj))
			goto change_found;
	}
	fz_always(ctx)
	{
		if (marked)
		{
			pdf_unmark_obj(ctx, nobj);
			pdf_unmark_obj(ctx, oobj);
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return changed;
}

typedef struct
{
	int max;
	int len;
	char **list;
} char_list;

/* This structure is used to hold the definition of which fields
 * are locked. */
struct pdf_locked_fields
{
	int p;
	int all;
	char_list includes;
	char_list excludes;
};

static void
free_char_list(fz_context *ctx, char_list *c)
{
	int i;

	if (c == NULL)
		return;

	for (i = c->len-1; i >= 0; i--)
		fz_free(ctx, c->list[i]);
	fz_free(ctx, c->list);
	c->len = 0;
	c->max = 0;
}

void
pdf_drop_locked_fields(fz_context *ctx, pdf_locked_fields *fl)
{
	if (fl == NULL)
		return;

	free_char_list(ctx, &fl->includes);
	free_char_list(ctx, &fl->excludes);
	fz_free(ctx, fl);
}

static void
char_list_append(fz_context *ctx, char_list *list, const char *s)
{
	if (list->len == list->max)
	{
		int n = list->max * 2;
		if (n == 0) n = 4;

		list->list = fz_realloc_array(ctx, list->list, n, char *);
		list->max = n;
	}
	list->list[list->len] = fz_strdup(ctx, s);
	list->len++;
}

int
pdf_is_field_locked(fz_context *ctx, pdf_locked_fields *locked, const char *name)
{
	int i;

	if (locked->p == 1)
	{
		/* Permissions were set, and say that field changes are not to be allowed. */
		return 1; /* Locked */
	}

	if(locked->all)
	{
		/* The only way we might not be unlocked is if
		 * we are listed in the excludes. */
		for (i = 0; i < locked->excludes.len; i++)
			if (!strcmp(locked->excludes.list[i], name))
				return 0;
		return 1;
	}

	/* The only way we can be locked is for us to be in the includes. */
	for (i = 0; i < locked->includes.len; i++)
		if (strcmp(locked->includes.list[i], name) == 0)
			return 1;

	/* Anything else is unlocked */
	return 0;
}

/* Unfortunately, in C, there is no legal way to define a function
 * type that returns itself. We therefore have to use a struct
 * wrapper. */
typedef struct filter_wrap
{
	struct filter_wrap (*func)(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
} filter_wrap;

typedef struct filter_wrap (*filter_fn)(fz_context *ctx, pdf_obj *dict, pdf_obj *key);

#define RETURN_FILTER(f) { filter_wrap rf; rf.func = (f); return rf; }

static filter_wrap filter_simple(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	RETURN_FILTER(NULL);
}

static filter_wrap filter_transformparams(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(Type)) ||
		pdf_name_eq(ctx, key, PDF_NAME(P)) ||
		pdf_name_eq(ctx, key, PDF_NAME(V)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Document)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Msg)) ||
		pdf_name_eq(ctx, key, PDF_NAME(V)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Annots)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Form)) ||
		pdf_name_eq(ctx, key, PDF_NAME(FormEx)) ||
		pdf_name_eq(ctx, key, PDF_NAME(EF)) ||
		pdf_name_eq(ctx, key, PDF_NAME(P)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Action)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Fields)))
		RETURN_FILTER(&filter_simple);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_reference(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(Type)) ||
		pdf_name_eq(ctx, key, PDF_NAME(TransformMethod)) ||
		pdf_name_eq(ctx, key, PDF_NAME(DigestMethod)) ||
		pdf_name_eq(ctx, key, PDF_NAME(DigestValue)) ||
		pdf_name_eq(ctx, key, PDF_NAME(DigestLocation)))
		RETURN_FILTER(&filter_simple);
	if (pdf_name_eq(ctx, key, PDF_NAME(TransformParams)))
		RETURN_FILTER(&filter_transformparams);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_prop_build_sub(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(Name)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Date)) ||
		pdf_name_eq(ctx, key, PDF_NAME(R)) ||
		pdf_name_eq(ctx, key, PDF_NAME(PreRelease)) ||
		pdf_name_eq(ctx, key, PDF_NAME(OS)) ||
		pdf_name_eq(ctx, key, PDF_NAME(NonEFontNoWarn)) ||
		pdf_name_eq(ctx, key, PDF_NAME(TrustedMode)) ||
		pdf_name_eq(ctx, key, PDF_NAME(V)) ||
		pdf_name_eq(ctx, key, PDF_NAME(REx)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Preview)))
		RETURN_FILTER(&filter_simple);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_prop_build(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(Filter)) ||
		pdf_name_eq(ctx, key, PDF_NAME(PubSec)) ||
		pdf_name_eq(ctx, key, PDF_NAME(App)) ||
		pdf_name_eq(ctx, key, PDF_NAME(SigQ)))
		RETURN_FILTER(&filter_prop_build_sub);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_v(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	/* Text can point to a stream object */
	if (pdf_name_eq(ctx, key, PDF_NAME(Length)) && pdf_is_stream(ctx, dict))
		RETURN_FILTER(&filter_simple);
	/* Sigs point to a dict. */
	if (pdf_name_eq(ctx, key, PDF_NAME(Type)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Filter)) ||
		pdf_name_eq(ctx, key, PDF_NAME(SubFilter)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Contents)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Cert)) ||
		pdf_name_eq(ctx, key, PDF_NAME(ByteRange)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Changes)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Name)) ||
		pdf_name_eq(ctx, key, PDF_NAME(M)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Location)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Reason)) ||
		pdf_name_eq(ctx, key, PDF_NAME(ContactInfo)) ||
		pdf_name_eq(ctx, key, PDF_NAME(R)) ||
		pdf_name_eq(ctx, key, PDF_NAME(V)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Prop_AuthTime)) ||
		pdf_name_eq(ctx, key, PDF_NAME(Prop_AuthType)))
	RETURN_FILTER(&filter_simple);
	if (pdf_name_eq(ctx, key, PDF_NAME(Reference)))
		RETURN_FILTER(filter_reference);
	if (pdf_name_eq(ctx, key, PDF_NAME(Prop_Build)))
		RETURN_FILTER(filter_prop_build);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_appearance(fz_context *ctx, pdf_obj *dict, pdf_obj *key);

static filter_wrap filter_xobject_list(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	/* FIXME: Infinite recursion possible here? */
	RETURN_FILTER(&filter_appearance);
}

static filter_wrap filter_font(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	/* In the example I've seen the /Name field was dropped, so we'll allow
	 * local changes, but none that follow an indirection. */
	RETURN_FILTER(NULL);
}

/* FIXME: One idea here is to make filter_font_list and filter_xobject_list
 * only accept NEW objects as changes. Will think about this. */
static filter_wrap filter_font_list(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	RETURN_FILTER(&filter_font);
}

static filter_wrap filter_resources(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(XObject)))
		RETURN_FILTER(&filter_xobject_list);
	if (pdf_name_eq(ctx, key, PDF_NAME(Font)))
		RETURN_FILTER(&filter_font_list);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_appearance(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	if (pdf_name_eq(ctx, key, PDF_NAME(Resources)))
		RETURN_FILTER(&filter_resources);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_ap(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	/* Just the /N entry for now. May need to add more later. */
	if (pdf_name_eq(ctx, key, PDF_NAME(N)) && pdf_is_stream(ctx, pdf_dict_get(ctx, dict, key)))
		RETURN_FILTER(&filter_appearance);
	RETURN_FILTER(NULL);
}

static filter_wrap filter_xfa(fz_context *ctx, pdf_obj *dict, pdf_obj *key)
{
	/* Text can point to a stream object */
	if (pdf_is_stream(ctx, dict))
		RETURN_FILTER(&filter_simple);
	RETURN_FILTER(NULL);
}

static void
filter_changes_accepted(fz_context *ctx, pdf_changes *changes, pdf_obj *obj, filter_fn filter)
{
	int obj_num;

	if (obj == NULL || pdf_obj_marked(ctx, obj))
		return;

	obj_num = pdf_to_num(ctx, obj);

	fz_try(ctx)
	{
		if (obj_num != 0)
		{
			(void)pdf_mark_obj(ctx, obj);
			changes->obj_changes[obj_num] |= FIELD_CHANGE_VALID;
		}
		if (filter == NULL)
			break;
		if (pdf_is_dict(ctx, obj))
		{
			int i, n = pdf_dict_len(ctx, obj);

			for (i = 0; i < n; i++)
			{
				pdf_obj *key = pdf_dict_get_key(ctx, obj, i);
				pdf_obj *val = pdf_dict_get_val(ctx, obj, i);
				filter_fn f = (filter(ctx, obj, key)).func;
				if (f != NULL)
					filter_changes_accepted(ctx, changes, val, f);
			}
		}
		else if (pdf_is_array(ctx, obj))
		{
			int i, n = pdf_array_len(ctx, obj);

			for (i = 0; i < n; i++)
			{
				pdf_obj *val = pdf_array_get(ctx, obj, i);
				filter_changes_accepted(ctx, changes, val, filter);
			}
		}
	}
	fz_always(ctx)
		if (obj_num != 0)
			pdf_unmark_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
check_field(fz_context *ctx, pdf_document *doc, pdf_changes *changes, pdf_obj *obj, pdf_locked_fields *locked, const char *name_prefix, pdf_obj *new_v, pdf_obj *old_v)
{
	pdf_obj *old_obj, *new_obj, *n_v, *o_v;
	int o_xref_base;
	int obj_num;
	char *field_name = NULL;

	/* All fields MUST be indirections, either in the Fields array
	 * or AcroForms, or in the Kids array of other Fields. */
	if (!pdf_is_indirect(ctx, obj))
		return;

	obj_num = pdf_to_num(ctx, obj);
	o_xref_base = doc->xref_base;
	new_obj = pdf_resolve_indirect_chain(ctx, obj);

	/* Similarly, all fields must be dicts */
	if (!pdf_is_dict(ctx, new_obj))
		return;

	if (pdf_obj_marked(ctx, obj))
		return;

	fz_var(field_name);

	fz_try(ctx)
	{
		int i, len;
		const char *name;
		size_t n;
		pdf_obj *t;
		int is_locked;

		(void)pdf_mark_obj(ctx, obj);

		/* Do this within the try, so we can catch any problems */
		doc->xref_base = o_xref_base+1;
		old_obj = pdf_resolve_indirect_chain(ctx, obj);

		t = pdf_dict_get(ctx, old_obj, PDF_NAME(T));
		if (t != NULL)
		{
			name = pdf_dict_get_text_string(ctx, old_obj, PDF_NAME(T));
			n = strlen(name)+1;
			if (*name_prefix)
				n += 1 + strlen(name_prefix);
			field_name = fz_malloc(ctx, n);
			if (*name_prefix)
			{
				strcpy(field_name, name_prefix);
				strcat(field_name, ".");
			}
			else
				*field_name = 0;
			strcat(field_name, name);
			name_prefix = field_name;
		}

		doc->xref_base = o_xref_base;

		if (!pdf_is_dict(ctx, old_obj))
			break;

		/* Check V explicitly, allowing for it being inherited. */
		n_v = pdf_dict_get(ctx, new_obj, PDF_NAME(V));
		if (n_v == NULL)
			n_v = new_v;
		o_v = pdf_dict_get(ctx, old_obj, PDF_NAME(V));
		if (o_v == NULL)
			o_v = old_v;

		is_locked = pdf_is_field_locked(ctx, locked, name_prefix);
		if (pdf_name_eq(ctx, pdf_dict_get(ctx, new_obj, PDF_NAME(Type)), PDF_NAME(Annot)) &&
			pdf_name_eq(ctx, pdf_dict_get(ctx, new_obj, PDF_NAME(Subtype)), PDF_NAME(Widget)))
		{
			if (is_locked)
			{
				/* If locked, V must not change! */
				if (check_unchanged_between(ctx, doc, changes, n_v, o_v))
					changes->obj_changes[obj_num] |= FIELD_CHANGE_INVALID;
			}
			else
			{
				/* If not locked, V can change to be filled in! */
				filter_changes_accepted(ctx, changes, n_v, &filter_v);
				changes->obj_changes[obj_num] |= FIELD_CHANGE_VALID;
			}
		}

		/* Check all the fields in the new object are
		 * either the same as the old object, or are
		 * expected changes. */
		len = pdf_dict_len(ctx, new_obj);
		for (i = 0; i < len; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, new_obj, i);
			pdf_obj *nval = pdf_dict_get(ctx, new_obj, key);
			pdf_obj *oval = pdf_dict_get(ctx, old_obj, key);

			/* Kids arrays shouldn't change. */
			if (pdf_name_eq(ctx, key, PDF_NAME(Kids)))
			{
				int j, m;

				/* Kids must be an array. If it's not, count it as a difference. */
				if (!pdf_is_array(ctx, nval) || !pdf_is_array(ctx, oval))
				{
change_found:
					changes->obj_changes[obj_num] |= FIELD_CHANGE_INVALID;
					break;
				}
				m = pdf_array_len(ctx, nval);
				/* Any change in length counts as a difference */
				if (m != pdf_array_len(ctx, oval))
					goto change_found;
				for (j = 0; j < m; j++)
				{
					pdf_obj *nkid = pdf_array_get(ctx, nval, j);
					pdf_obj *okid = pdf_array_get(ctx, oval, j);
					/* Kids arrays are supposed to all be indirect. If they aren't,
					 * count it as a difference. */
					if (!pdf_is_indirect(ctx, nkid) || !pdf_is_indirect(ctx, okid))
						goto change_found;
					/* For now at least, we'll count any change in number as a difference. */
					if (pdf_to_num(ctx, nkid) != pdf_to_num(ctx, okid))
						goto change_found;
					check_field(ctx, doc, changes, nkid, locked, name_prefix, n_v, o_v);
				}
			}
			else if (pdf_name_eq(ctx, key, PDF_NAME(V)))
			{
				/* V is checked above */
			}
			else if (pdf_name_eq(ctx, key, PDF_NAME(AP)))
			{
				/* If we're locked, then nothing can change. If not,
				 * we can change to be filled in. */
				if (is_locked)
					check_unchanged_between(ctx, doc, changes, nval, oval);
				else
					filter_changes_accepted(ctx, changes, nval, &filter_ap);
			}
			/* All other fields can't change */
			else
				check_unchanged_between(ctx, doc, changes, nval, oval);
		}

		/* Now check all the fields in the old object to
		 * make sure none were dropped. */
		len = pdf_dict_len(ctx, old_obj);
		for (i = 0; i < len; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, old_obj, i);
			pdf_obj *nval, *oval;

			/* V is checked above */
			if (pdf_name_eq(ctx, key, PDF_NAME(V)))
				continue;

			nval = pdf_dict_get(ctx, new_obj, key);
			oval = pdf_dict_get(ctx, old_obj, key);

			if (nval == NULL && oval != NULL)
				changes->obj_changes[pdf_to_num(ctx, nval)] |= FIELD_CHANGE_INVALID;
		}
		changes->obj_changes[obj_num] |= FIELD_CHANGE_VALID;

	}
	fz_always(ctx)
	{
		pdf_unmark_obj(ctx, obj);
		fz_free(ctx, field_name);
		doc->xref_base = o_xref_base;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int
pdf_obj_changed_in_version(fz_context *ctx, pdf_document *doc, int num, int version)
{
	if (num < 0 || num > doc->max_xref_len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Invalid object number requested");

	return version == doc->xref_index[num];
}

static void
merge_lock_specification(fz_context *ctx, pdf_locked_fields *fields, pdf_obj *lock)
{
	pdf_obj *action;
	int i, r, w;

	if (lock == NULL)
		return;

	action = pdf_dict_get(ctx, lock, PDF_NAME(Action));

	if (pdf_name_eq(ctx, action, PDF_NAME(All)))
	{
		/* All fields locked means we don't need any stored
		 * includes/excludes. */
		fields->all = 1;
		free_char_list(ctx, &fields->includes);
		free_char_list(ctx, &fields->excludes);
	}
	else
	{
		pdf_obj *f = pdf_dict_get(ctx, lock, PDF_NAME(Fields));
		int len = pdf_array_len(ctx, f);

		if (pdf_name_eq(ctx, action, PDF_NAME(Include)))
		{
			if (fields->all)
			{
				/* Current state = "All except <excludes> are locked".
				 * We need to remove <Fields> from <excludes>. */
				for (i = 0; i < len; i++)
				{
					const char *s = pdf_array_get_text_string(ctx, f, i);
					int r, w;

					for (r = w = 0; r < fields->excludes.len; r++)
					{
						if (strcmp(s, fields->excludes.list[r]))
							fields->excludes.list[w++] = fields->excludes.list[r];
					}
					fields->excludes.len = w;
				}
			}
			else
			{
				/* Current state = <includes> are locked.
				 * We need to add <Fields> to <include> (avoiding repetition). */
				for (i = 0; i < len; i++)
				{
					const char *s = pdf_array_get_text_string(ctx, f, i);

					for (r = 0; r < fields->includes.len; r++)
					{
						if (!strcmp(s, fields->includes.list[r]))
							break;
					}
					if (r == fields->includes.len)
						char_list_append(ctx, &fields->includes, s);
				}
			}
		}
		else if (pdf_name_eq(ctx, action, PDF_NAME(Exclude)))
		{
			if (fields->all)
			{
				/* Current state = "All except <excludes> are locked.
				 * We need to remove anything from <excludes> that isn't in <Fields>. */
				for (r = w = 0; r < fields->excludes.len; r++)
				{
					for (i = 0; i < len; i++)
					{
						const char *s = pdf_array_get_text_string(ctx, f, i);
						if (!strcmp(s, fields->excludes.list[r]))
							break;
					}
					if (i != len) /* we found a match */
						fields->excludes.list[w++] = fields->excludes.list[r];
				}
				fields->excludes.len = w;
			}
			else
			{
				/* Current state = <includes> are locked.
				 * Set all. <excludes> becomes <Fields> less <includes>. Remove <includes>. */
				fields->all = 1;
				for (i = 0; i < len; i++)
				{
					const char *s = pdf_array_get_text_string(ctx, f, i);
					for (r = 0; r < fields->includes.len; r++)
					{
						if (!strcmp(s, fields->includes.list[r]))
							break;
					}
					if (r == fields->includes.len)
						char_list_append(ctx, &fields->excludes, s);
				}
				free_char_list(ctx, &fields->includes);
			}
		}
	}
}

static void
find_locked_fields_value(fz_context *ctx, pdf_locked_fields *fields, pdf_obj *v)
{
	pdf_obj *ref = pdf_dict_get(ctx, v, PDF_NAME(Reference));
	int i, n;

	if (!ref)
		return;

	n = pdf_array_len(ctx, ref);
	for (i = 0; i < n; i++)
	{
		pdf_obj *sr = pdf_array_get(ctx, ref, i);
		pdf_obj *tm, *tp, *type;

		/* Type is optional, but if it exists, it'd better be SigRef. */
		type = pdf_dict_get(ctx, sr, PDF_NAME(Type));
		if (type != NULL && !pdf_name_eq(ctx, type, PDF_NAME(SigRef)))
			continue;
		tm = pdf_dict_get(ctx, sr, PDF_NAME(TransformMethod));
		tp = pdf_dict_get(ctx, sr, PDF_NAME(TransformParams));
		if (pdf_name_eq(ctx, tm, PDF_NAME(DocMDP)))
		{
			int p = pdf_dict_get_int(ctx, tp, PDF_NAME(P));

			if (p == 0)
				p = 2;
			if (fields->p == 0)
				fields->p = p;
			else
				fields->p = fz_mini(fields->p, p);
		}
		else if (pdf_name_eq(ctx, tm, PDF_NAME(FieldMDP)))
			merge_lock_specification(ctx, fields, tp);
	}
}

static void
find_locked_fields_aux(fz_context *ctx, pdf_obj *field, pdf_locked_fields *fields, pdf_obj *inherit_v, pdf_obj *inherit_ft)
{
	int i, n;

	if (!pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Type)), PDF_NAME(Annot)))
		return;

	if (pdf_obj_marked(ctx, field))
		return;

	fz_try(ctx)
	{
		pdf_obj *kids, *v, *ft;

		(void)pdf_mark_obj(ctx, field);

		v = pdf_dict_get(ctx, field, PDF_NAME(V));
		if (v == NULL)
			v = inherit_v;
		ft = pdf_dict_get(ctx, field, PDF_NAME(FT));
		if (ft == NULL)
			ft = inherit_ft;

		/* We are looking for Widget annotations of type Sig that are
		 * signed (i.e. have a 'V' field). */
		if (pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Subtype)), PDF_NAME(Widget)) &&
			pdf_name_eq(ctx, ft, PDF_NAME(Sig)) &&
			pdf_name_eq(ctx, pdf_dict_get(ctx, v, PDF_NAME(Type)), PDF_NAME(Sig)))
		{
			/* Signed Sig Widgets (i.e. ones with a 'V' field) need
			 * to have their lock field respected. */
			merge_lock_specification(ctx, fields, pdf_dict_get(ctx, field, PDF_NAME(Lock)));

			/* Look for DocMDP and FieldMDP entries to see what
			 * flavours of alterations are allowed. */
			find_locked_fields_value(ctx, fields, v);
		}

		/* Recurse as required */
		kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
		if (kids)
		{
			n = pdf_array_len(ctx, kids);
			for (i = 0; i < n; i++)
				find_locked_fields_aux(ctx, pdf_array_get(ctx, kids, i), fields, v, ft);
		}
	}
	fz_always(ctx)
		pdf_unmark_obj(ctx, field);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

pdf_locked_fields *
pdf_find_locked_fields(fz_context *ctx, pdf_document *doc, int version)
{
	pdf_locked_fields *fields = fz_malloc_struct(ctx, pdf_locked_fields);
	int o_xref_base = doc->xref_base;
	doc->xref_base = version;

	fz_var(fields);

	fz_try(ctx)
	{
		pdf_obj *fobj = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");
		int i, len = pdf_array_len(ctx, fobj);

		if (len == 0)
			break;

		for (i = 0; i < len; i++)
			find_locked_fields_aux(ctx, pdf_array_get(ctx, fobj, i), fields, NULL, NULL);

		/* Add in any DocMDP referenced directly from the Perms dict. */
		find_locked_fields_value(ctx, fields, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Perms/DocMDP"));
	}
	fz_always(ctx)
		doc->xref_base = o_xref_base;
	fz_catch(ctx)
	{
		pdf_drop_locked_fields(ctx, fields);
		fz_rethrow(ctx);
	}

	return fields;
}

pdf_locked_fields *
pdf_find_locked_fields_for_sig(fz_context *ctx, pdf_document *doc, pdf_obj *sig)
{
	pdf_locked_fields *fields = fz_malloc_struct(ctx, pdf_locked_fields);

	fz_var(fields);

	fz_try(ctx)
	{
		pdf_obj *ref;
		int i, len;

		/* Ensure it really is a sig */
		if (!pdf_name_eq(ctx, pdf_dict_get(ctx, sig, PDF_NAME(Subtype)), PDF_NAME(Widget)) ||
			!pdf_name_eq(ctx, pdf_dict_get_inheritable(ctx, sig, PDF_NAME(FT)), PDF_NAME(Sig)))
			break;

		/* Check the locking details given in the V (i.e. what the signature value
		 * claims to lock). */
		ref = pdf_dict_getp(ctx, sig, "V/Reference");
		len = pdf_array_len(ctx, ref);
		for (i = 0; i < len; i++)
		{
			pdf_obj *tp = pdf_dict_get(ctx, pdf_array_get(ctx, ref, i), PDF_NAME(TransformParams));
			merge_lock_specification(ctx, fields, tp);
		}

		/* Also, check the locking details given in the Signature definition. This may
		 * not strictly be necessary as it's supposed to be "what the form author told
		 * the signature that it should lock". A well-formed signature should lock
		 * at least that much (possibly with extra fields locked from the XFA). If the
		 * signature doesn't lock as much as it was told to, we should be suspicious
		 * of the signing application. It is not clear that this test is actually
		 * necessary, or in keeping with what Acrobat does. */
		merge_lock_specification(ctx, fields, pdf_dict_get(ctx, sig, PDF_NAME(Lock)));
	}
	fz_catch(ctx)
	{
		pdf_drop_locked_fields(ctx, fields);
		fz_rethrow(ctx);
	}

	return fields;
}

static int
validate_locked_fields(fz_context *ctx, pdf_document *doc, int version, pdf_locked_fields *locked)
{
	int o_xref_base = doc->xref_base;
	pdf_changes *changes;
	int num_objs;
	int i, n;
	int all_indirects = 1;

	num_objs = doc->max_xref_len;
	changes = Memento_label(fz_calloc(ctx, 1, sizeof(*changes) + sizeof(int)*(num_objs-1)), "pdf_changes");
	changes->num_obj = num_objs;

	fz_try(ctx)
	{
		pdf_obj *acroform, *new_acroform, *old_acroform;
		int len, acroform_num;

		doc->xref_base = version;

		/* Detect every object that has changed */
		for (i = 1; i < num_objs; i++)
		{
			if (pdf_obj_changed_in_version(ctx, doc, i, version))
				changes->obj_changes[i] = FIELD_CHANGED;
		}

		/* FIXME: Compare PageTrees and NumberTrees (just to allow for them being regenerated
		 * and having produced stuff that represents the same stuff). */

		/* The metadata of a document may be regenerated. Allow for that. */
		filter_changes_accepted(ctx, changes, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Metadata"), &filter_simple);

		/* The ModDate of document info may be regenerated. Allow for that. */
		/* FIXME: We accept all changes in document info, when maybe we ought to just
		 * accept ModDate? */
		filter_changes_accepted(ctx, changes, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Info"), &filter_simple);

		/* The Encryption dict may be rewritten for the new Xref. */
		filter_changes_accepted(ctx, changes, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Encrypt"), &filter_simple);

		/* We have to accept certain changes in the top level AcroForms dict,
		 * so get the 2 versions... */
		acroform = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm");
		acroform_num = pdf_to_num(ctx, acroform);
		new_acroform = pdf_resolve_indirect_chain(ctx, acroform);
		doc->xref_base = version+1;
		old_acroform = pdf_resolve_indirect_chain(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm"));
		doc->xref_base = version;
		n = pdf_dict_len(ctx, new_acroform);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, new_acroform, i);
			pdf_obj *nval = pdf_dict_get(ctx, new_acroform, key);
			pdf_obj *oval = pdf_dict_get(ctx, old_acroform, key);

			if (pdf_name_eq(ctx, key, PDF_NAME(Fields)))
			{
				int j;

				len = pdf_array_len(ctx, nval);
				for (j = 0; j < len; j++)
				{
					pdf_obj *field = pdf_array_get(ctx, nval, j);
					if (!pdf_is_indirect(ctx, field))
						all_indirects = 0;
					check_field(ctx, doc, changes, field, locked, "", NULL, NULL);
				}
			}
			else if (pdf_name_eq(ctx, key, PDF_NAME(SigFlags)))
			{
				/* Accept this */
				changes->obj_changes[acroform_num] |= FIELD_CHANGE_VALID;
			}
			else if (pdf_name_eq(ctx, key, PDF_NAME(DR)))
			{
				/* Accept any changes from within the Document Resources */
				filter_changes_accepted(ctx, changes, nval, &filter_resources);
			}
			else if (pdf_name_eq(ctx, key, PDF_NAME(XFA)))
			{
				/* Allow any changes within the XFA streams. */
				filter_changes_accepted(ctx, changes, nval, &filter_xfa);
			}
			else if (pdf_objcmp(ctx, nval, oval))
			{
				changes->obj_changes[acroform_num] |= FIELD_CHANGE_INVALID;
			}
		}

		/* Allow for any object streams/XRefs to be changed. */
		doc->xref_base = version+1;
		for (i = 1; i < num_objs; i++)
		{
			pdf_obj *oobj, *otype;
			if (changes->obj_changes[i] != FIELD_CHANGED)
				continue;
			if (!pdf_obj_exists(ctx, doc, i))
			{
				/* Not present this version - must be newly created, can't be a change. */
				changes->obj_changes[i] |= FIELD_CHANGE_VALID;
				continue;
			}
			oobj = pdf_load_object(ctx, doc, i);
			otype = pdf_dict_get(ctx, oobj, PDF_NAME(Type));
			if (pdf_name_eq(ctx, otype, PDF_NAME(ObjStm)) ||
				pdf_name_eq(ctx, otype, PDF_NAME(XRef)))
			{
				changes->obj_changes[i] |= FIELD_CHANGE_VALID;
			}
			pdf_drop_obj(ctx, oobj);
		}
	}
	fz_always(ctx)
		doc->xref_base = o_xref_base;
	fz_catch(ctx)
	{
		fz_free(ctx, changes);
		fz_rethrow(ctx);
	}

	for (i = 1; i < num_objs; i++)
	{
		if (changes->obj_changes[i] == FIELD_CHANGED)
			/* Change with no reason */
			break;
		if (changes->obj_changes[i] & FIELD_CHANGE_INVALID)
			/* Illegal Change */
			break;
	}

	fz_free(ctx, changes);

	return (i == num_objs) && all_indirects;
}

int
pdf_validate_changes(fz_context *ctx, pdf_document *doc, int version)
{
	int unsaved_versions = pdf_count_unsaved_versions(ctx, doc);
	int n = pdf_count_versions(ctx, doc);
	pdf_locked_fields *locked = NULL;
	int result;

	if (version < 0 || version >= n)
		fz_throw(ctx, FZ_ERROR_GENERIC, "There aren't that many changes to find in this document!");

	/* We are wanting to compare version+1 with version to make sure
	 * that the only changes made in going to version are conformant
	 * with what was allowed in version+1. The production of version
	 * might have involved signing a signature field and locking down
	 * more fields - this means that taking the list of locked things
	 * from version rather than version+1 will give us bad results! */
	locked = pdf_find_locked_fields(ctx, doc, unsaved_versions+version+1);

	fz_try(ctx)
	{
		if (!locked->all && locked->includes.len == 0 && locked->p == 0)
		{
			/* If nothing is locked at all, then all changes are permissible. */
			result = 1;
		}
		else
			result = validate_locked_fields(ctx, doc, unsaved_versions+version, locked);
	}
	fz_always(ctx)
		pdf_drop_locked_fields(ctx, locked);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return result;
}

int
pdf_validate_change_history(fz_context *ctx, pdf_document *doc)
{
	int num_versions = pdf_count_versions(ctx, doc);
	int v;

	if (num_versions < 2)
		return 0; /* Unless there are at least 2 versions, there have been no updates. */

	for(v = num_versions - 2; v >= 0; v--)
	{
		if (!pdf_validate_changes(ctx, doc, v))
			return v+1;
	}
	return 0;
}

/* Return the version that obj appears in, or -1 for not found. */
static int
pdf_find_incremental_update_num_for_obj(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	pdf_xref *xref = NULL;
	pdf_xref_subsec *sub;
	int i, j;

	if (obj == NULL)
		return -1;

	/* obj needs to be indirect for us to get a num out of it. */
	i = pdf_to_num(ctx, obj);
	if (i <= 0)
		return -1;

	/* obj can't be indirect below, so resolve it here. */
	obj = pdf_resolve_indirect_chain(ctx, obj);

	/* Find the first xref section where the entry is defined. */
	for (j = 0; j < doc->num_xref_sections; j++)
	{
		xref = &doc->xref_sections[j];

		if (i < xref->num_objects)
		{
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				pdf_xref_entry *entry;

				if (i < sub->start || i >= sub->start + sub->len)
					continue;

				entry = &sub->table[i - sub->start];
				if (entry->obj == obj)
					return j;
			}
		}
	}
	return -1;
}

int pdf_find_version_for_obj(fz_context *ctx, pdf_document *doc, pdf_obj *obj)
{
	int v = pdf_find_incremental_update_num_for_obj(ctx, doc, obj);
	int n;

	if (v == -1)
		return -1;

	n = pdf_count_versions(ctx, doc) + pdf_count_unsaved_versions(ctx, doc);
	if (v > n)
		return n;

	return v;
}

int pdf_validate_signature(fz_context *ctx, pdf_annot *widget)
{
	pdf_document *doc = widget->page->doc;
	int unsaved_versions = pdf_count_unsaved_versions(ctx, doc);
	int num_versions = pdf_count_versions(ctx, doc) + unsaved_versions;
	int version = pdf_find_version_for_obj(ctx, doc, widget->obj);
	int i;
	pdf_locked_fields *locked = NULL;
	int o_xref_base;

	if (version > num_versions-1)
		version = num_versions-1;

	/* Get the locked definition from the object when it was signed. */
	o_xref_base = doc->xref_base;
	doc->xref_base = version;

	fz_var(locked); /* Not really needed, but it stops warnings */

	fz_try(ctx)
	{
		locked = pdf_find_locked_fields_for_sig(ctx, doc, widget->obj);
		for (i = version-1; i >= unsaved_versions; i--)
		{
			doc->xref_base = i;
			if (!validate_locked_fields(ctx, doc, i, locked))
				break;
		}
	}
	fz_always(ctx)
	{
		doc->xref_base = o_xref_base;
		pdf_drop_locked_fields(ctx, locked);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return i+1-unsaved_versions;
}

int pdf_was_pure_xfa(fz_context *ctx, pdf_document *doc)
{
	int num_unsaved_versions = pdf_count_unsaved_versions(ctx, doc);
	int num_versions = pdf_count_versions(ctx, doc);
	int v;
	int o_xref_base = doc->xref_base;
	int pure_xfa = 0;

	fz_var(pure_xfa);

	fz_try(ctx)
	{
		for(v = num_versions + num_unsaved_versions; !pure_xfa && v >= num_unsaved_versions; v--)
		{
			pdf_obj *o;
			doc->xref_base = v;
			o = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm");
			/* If we find a version that had an empty Root/AcroForm/Fields, but had a
			 * Root/AcroForm/XFA entry, then we deduce that this was at one time a
			 * pure XFA form. */
			if (pdf_array_len(ctx, pdf_dict_get(ctx, o, PDF_NAME(Fields))) == 0 &&
				pdf_dict_get(ctx, o, PDF_NAME(XFA)) != NULL)
				pure_xfa = 1;
		}
	}
	fz_always(ctx)
		doc->xref_base = o_xref_base;
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pure_xfa;
}

pdf_xref *pdf_new_local_xref(fz_context *ctx, pdf_document *doc)
{
	int n = pdf_xref_len(ctx, doc);
	pdf_xref *xref = fz_malloc_struct(ctx, pdf_xref);

	xref->subsec = NULL;
	xref->num_objects = n;
	xref->trailer = NULL;
	xref->pre_repair_trailer = NULL;
	xref->unsaved_sigs = NULL;
	xref->unsaved_sigs_end = NULL;

	fz_try(ctx)
	{
		xref->subsec = fz_malloc_struct(ctx, pdf_xref_subsec);
		xref->subsec->len = n;
		xref->subsec->start = 0;
		xref->subsec->table = fz_malloc_struct_array(ctx, n, pdf_xref_entry);
		xref->subsec->next = NULL;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, xref->subsec);
		fz_free(ctx, xref);
		fz_rethrow(ctx);
	}

	return xref;
}

void pdf_drop_local_xref(fz_context *ctx, pdf_xref *xref)
{
	if (xref == NULL)
		return;

	pdf_drop_xref_subsec(ctx, xref);

	fz_free(ctx, xref);
}

void pdf_drop_local_xref_and_resources(fz_context *ctx, pdf_document *doc)
{
	pdf_purge_local_font_resources(ctx, doc);
	pdf_purge_locals_from_store(ctx, doc);
	pdf_drop_local_xref(ctx, doc->local_xref);
	doc->local_xref = NULL;
	doc->resynth_required = 1;
}

void
pdf_debug_doc_changes(fz_context *ctx, pdf_document *doc)
{
	int i, j;

	if (doc->num_incremental_sections == 0)
		fz_write_printf(ctx, fz_stddbg(ctx), "No incremental xrefs");
	else
	{
		for (i = 0; i < doc->num_incremental_sections; i++)
		{
			pdf_xref *xref = &doc->xref_sections[i];
			pdf_xref_subsec *sub;

			fz_write_printf(ctx, fz_stddbg(ctx), "Incremental xref:\n");
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				fz_write_printf(ctx, fz_stddbg(ctx), "  Objects %d->%d\n", sub->start, sub->start + sub->len - 1);
				for (j = 0; j < sub->len; j++)
				{
					pdf_xref_entry *e = &sub->table[j];
					if (e->type == 0)
						continue;
					fz_write_printf(ctx, fz_stddbg(ctx), "%d %d obj (%c)\n", j + sub->start, e->gen, e->type);
					pdf_debug_obj(ctx, e->obj);
					fz_write_printf(ctx, fz_stddbg(ctx), "\nendobj\n");
				}
			}
		}
	}

	if (doc->local_xref == NULL)
		fz_write_printf(ctx, fz_stddbg(ctx), "No local xref");
	else
	{
		for (i = 0; i < doc->num_incremental_sections; i++)
		{
			pdf_xref *xref = doc->local_xref;
			pdf_xref_subsec *sub;

			fz_write_printf(ctx, fz_stddbg(ctx), "Local xref (%sin force):\n", doc->local_xref_nesting == 0 ? "not " : "");
			for (sub = xref->subsec; sub != NULL; sub = sub->next)
			{
				fz_write_printf(ctx, fz_stddbg(ctx), "  Objects %d->%d\n", sub->start, sub->start + sub->len - 1);
				for (j = 0; j < sub->len; j++)
				{
					pdf_xref_entry *e = &sub->table[j];
					if (e->type == 0)
						continue;
					fz_write_printf(ctx, fz_stddbg(ctx), "%d %d obj (%c)\n", j + sub->start, e->gen, e->type);
					pdf_debug_obj(ctx, e->obj);
					fz_write_printf(ctx, fz_stddbg(ctx), "\nendobj\n");
				}
			}
		}
	}

}

pdf_obj *
pdf_metadata(fz_context *ctx, pdf_document *doc)
{
	int initial = doc->xref_base;
	pdf_obj *obj = NULL;

	fz_var(obj);

	fz_try(ctx)
	{
		do
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			obj = pdf_dict_get(ctx, root, PDF_NAME(Metadata));
			if (obj)
				break;
			doc->xref_base++;
		}
		while (doc->xref_base < doc->num_xref_sections);
	}
	fz_always(ctx)
		doc->xref_base = initial;
	fz_catch(ctx)
		fz_rethrow(ctx);

	return obj;
}

int pdf_obj_is_incremental(fz_context *ctx, pdf_obj *obj)
{
	pdf_document *doc = pdf_get_bound_document(ctx, obj);
	int v;

	if (doc == NULL || doc->num_incremental_sections == 0)
		return 0;

	v = pdf_find_incremental_update_num_for_obj(ctx, doc, obj);

	return (v == 0);
}

void pdf_minimize_document(fz_context *ctx, pdf_document *doc)
{
	int i;

	/* Don't throw anything away if we've done a repair! */
	if (doc == NULL || doc->repair_attempted)
		return;

	/* Don't throw anything away in the incremental section, as that's where
	 * all our changes will be. */
	for (i = doc->num_incremental_sections; i < doc->num_xref_sections; i++)
	{
		pdf_xref *xref = &doc->xref_sections[i];
		pdf_xref_subsec *sub;

		for (sub = xref->subsec; sub; sub = sub->next)
		{
			int len = sub->len;
			int j;
			for (j = 0; j < len; j++)
			{
				pdf_xref_entry *e = &sub->table[j];
				if (e->obj == NULL)
					continue;
				e->obj = pdf_drop_singleton_obj(ctx, e->obj);
			}
		}
	}
}
