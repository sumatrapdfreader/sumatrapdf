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
#include <math.h>

static pdf_obj *
resolve_dest_rec(fz_context *ctx, pdf_document *doc, pdf_obj *dest, int depth)
{
	if (depth > 10) /* Arbitrary to avoid infinite recursion */
		return NULL;

	if (pdf_is_name(ctx, dest) || pdf_is_string(ctx, dest))
	{
		dest = pdf_lookup_dest(ctx, doc, dest);
		dest = resolve_dest_rec(ctx, doc, dest, depth+1);
		return dest;
	}

	else if (pdf_is_array(ctx, dest))
	{
		return dest;
	}

	else if (pdf_is_dict(ctx, dest))
	{
		dest = pdf_dict_get(ctx, dest, PDF_NAME(D));
		return resolve_dest_rec(ctx, doc, dest, depth+1);
	}

	else if (pdf_is_indirect(ctx, dest))
		return dest;

	return NULL;
}

static pdf_obj *
resolve_dest(fz_context *ctx, pdf_document *doc, pdf_obj *dest)
{
	return resolve_dest_rec(ctx, doc, dest, 0);
}

static void
populate_destination(fz_context *ctx, pdf_document *doc, pdf_obj *dest, int is_remote, fz_link_dest *destination)
{
	pdf_obj *arg1 = pdf_array_get(ctx, dest, 2);
	pdf_obj *arg2 = pdf_array_get(ctx, dest, 3);
	pdf_obj *arg3 = pdf_array_get(ctx, dest, 4);
	pdf_obj *arg4 = pdf_array_get(ctx, dest, 5);
	float arg1v = pdf_to_real(ctx, arg1);
	float arg2v = pdf_to_real(ctx, arg2);
	float arg3v = pdf_to_real(ctx, arg3);
	float arg4v = pdf_to_real(ctx, arg4);
	pdf_obj *type, *page = NULL;
	fz_matrix ctm = fz_identity;
	fz_rect rect;
	fz_point p;
	int pageno;

	if (is_remote)
		pageno = pdf_array_get_int(ctx, dest, 0);
	else
	{
		page = pdf_array_get(ctx, dest, 0);
		if (pdf_is_int(ctx, page))
		{
			pageno = pdf_to_int(ctx, page);
			page = pdf_lookup_page_obj(ctx, doc, pageno);
		}
		else
			pageno = pdf_lookup_page_number(ctx, doc, page);
		pageno = fz_clampi(pageno, 0, pdf_count_pages(ctx, doc) - 1);
		if (pdf_is_dict(ctx, page))
			pdf_page_obj_transform(ctx, page, NULL, &ctm);
	}

	destination->loc.page = pageno;

	type = pdf_array_get(ctx, dest, 1);
	if (type == PDF_NAME(XYZ))
		destination->type = FZ_LINK_DEST_XYZ;
	else if (type == PDF_NAME(Fit))
		destination->type = FZ_LINK_DEST_FIT;
	else if (type == PDF_NAME(FitH))
		destination->type = FZ_LINK_DEST_FIT_H;
	else if (type == PDF_NAME(FitV))
		destination->type = FZ_LINK_DEST_FIT_V;
	else if (type == PDF_NAME(FitR))
		destination->type = FZ_LINK_DEST_FIT_R;
	else if (type == PDF_NAME(FitB))
		destination->type = FZ_LINK_DEST_FIT_B;
	else if (type == PDF_NAME(FitBH))
		destination->type = FZ_LINK_DEST_FIT_BH;
	else if (type == PDF_NAME(FitBV))
		destination->type = FZ_LINK_DEST_FIT_BV;
	else
		destination->type = FZ_LINK_DEST_XYZ;

	switch (destination->type)
	{
	default:
	case FZ_LINK_DEST_FIT:
	case FZ_LINK_DEST_FIT_B:
		break;
	case FZ_LINK_DEST_FIT_H:
	case FZ_LINK_DEST_FIT_BH:
		p = fz_transform_point_xy(0, arg1v, ctm);
		destination->y = arg1 ? p.y : NAN;
		break;
	case FZ_LINK_DEST_FIT_V:
	case FZ_LINK_DEST_FIT_BV:
		p = fz_transform_point_xy(arg1v, 0, ctm);
		destination->x = arg1 ? p.x : NAN;
		break;
	case FZ_LINK_DEST_XYZ:
		p = fz_transform_point_xy(arg1v, arg2v, ctm);
		destination->x = arg1 ? p.x : NAN;
		destination->y = arg2 ? p.y : NAN;
		destination->zoom = arg3 ? (arg3v > 0 ? (arg3v * 100) : 100) : NAN;
		break;
	case FZ_LINK_DEST_FIT_R:
		rect.x0 = arg1v;
		rect.y0 = arg2v;
		rect.x1 = arg3v;
		rect.y1 = arg4v;
		fz_transform_rect(rect, ctm);
		destination->x = fz_min(rect.x0, rect.x1);
		destination->y = fz_min(rect.y0, rect.y1);
		destination->w = fz_abs(rect.x1 - rect.x0);
		destination->h = fz_abs(rect.y1 - rect.y0);
		break;
	}
}

static char *
pdf_parse_link_dest_to_file_with_uri(fz_context *ctx, pdf_document *doc, const char *uri, pdf_obj *dest)
{
	if (pdf_is_array(ctx, dest) && pdf_array_len(ctx, dest) >= 1)
	{
		fz_link_dest destination = fz_make_link_dest_none();
		populate_destination(ctx, doc, dest, 1, &destination);
		return pdf_append_explicit_dest_to_uri(ctx, uri, destination);
	}
	else if (pdf_is_name(ctx, dest))
	{
		const char *name = pdf_to_name(ctx, dest);
		return pdf_append_named_dest_to_uri(ctx, uri, name);
	}
	else if (pdf_is_string(ctx, dest))
	{
		const char *name = pdf_to_text_string(ctx, dest);
		return pdf_append_named_dest_to_uri(ctx, uri, name);
	}
	else
	{
		fz_warn(ctx, "invalid link destination");
		return NULL;
	}
}

static char *
pdf_parse_link_dest_to_file_with_path(fz_context *ctx, pdf_document *doc, const char *path, pdf_obj *dest, int is_remote)
{
	if (pdf_is_array(ctx, dest) && pdf_array_len(ctx, dest) >= 1)
	{
		fz_link_dest destination = fz_make_link_dest_none();
		if (!is_remote)
			dest = resolve_dest(ctx, doc, dest);
		populate_destination(ctx, doc, dest, is_remote, &destination);
		return pdf_new_uri_from_path_and_explicit_dest(ctx, path, destination);
	}
	else if (pdf_is_name(ctx, dest))
	{
		const char *name = pdf_to_name(ctx, dest);
		return pdf_new_uri_from_path_and_named_dest(ctx, path, name);
	}
	else if (pdf_is_string(ctx, dest))
	{
		const char *name = pdf_to_text_string(ctx, dest);
		return pdf_new_uri_from_path_and_named_dest(ctx, path, name);
	}
	else if (path)
	{
		fz_link_dest destination = fz_make_link_dest_none();
		return pdf_new_uri_from_path_and_explicit_dest(ctx, path, destination);
	}
	else
	{
		fz_warn(ctx, "invalid link destination");
		return NULL;
	}
}

static char *
pdf_parse_file_spec(fz_context *ctx, pdf_document *doc, pdf_obj *file_spec, pdf_obj *dest, int is_remote)
{
	pdf_obj *str = NULL;
	int is_url;

	if (pdf_is_string(ctx, file_spec))
		str = file_spec;
	else if (pdf_is_dict(ctx, file_spec)) {
		str = pdf_dict_get(ctx, file_spec, PDF_NAME(UF));
		if (!str)
			str = pdf_dict_get(ctx, file_spec, PDF_NAME(F));
		if (!str)
			str = pdf_dict_get(ctx, file_spec, PDF_NAME(Unix));
		if (!str)
			str = pdf_dict_get(ctx, file_spec, PDF_NAME(DOS));
		if (!str)
			str = pdf_dict_get(ctx, file_spec, PDF_NAME(Mac));
	}

	if (!pdf_is_string(ctx, str))
	{
		fz_warn(ctx, "cannot parse file specification");
		return NULL;
	}

	is_url = pdf_dict_get(ctx, file_spec, PDF_NAME(FS)) == PDF_NAME(URL);

	if (is_url)
		return pdf_parse_link_dest_to_file_with_uri(ctx, doc, pdf_to_text_string(ctx, str), dest);
	else
		return pdf_parse_link_dest_to_file_with_path(ctx, doc, pdf_to_text_string(ctx, str), dest, is_remote);
}

static pdf_obj *
pdf_embedded_file_stream(fz_context *ctx, pdf_obj *fs)
{
	pdf_obj *ef = pdf_dict_get(ctx, fs, PDF_NAME(EF));
	pdf_obj *file = pdf_dict_get(ctx, ef, PDF_NAME(UF));
	if (!file) file = pdf_dict_get(ctx, ef, PDF_NAME(F));
	if (!file) file = pdf_dict_get(ctx, ef, PDF_NAME(Unix));
	if (!file) file = pdf_dict_get(ctx, ef, PDF_NAME(DOS));
	if (!file) file = pdf_dict_get(ctx, ef, PDF_NAME(Mac));
	return file;
}

int
pdf_is_embedded_file(fz_context *ctx, pdf_obj *fs)
{
	return pdf_is_stream(ctx, pdf_embedded_file_stream(ctx, fs));
}

void
pdf_get_embedded_file_params(fz_context *ctx, pdf_obj *fs, pdf_embedded_file_params *out)
{
	pdf_obj *file, *params, *filename, *subtype;

	if (!pdf_is_embedded_file(ctx, fs) || !out)
		return;

	file = pdf_embedded_file_stream(ctx, fs);
	params = pdf_dict_get(ctx, file, PDF_NAME(Params));

	filename = pdf_dict_get(ctx, fs, PDF_NAME(UF));
	if (!filename) filename = pdf_dict_get(ctx, fs, PDF_NAME(F));
	if (!filename) filename = pdf_dict_get(ctx, fs, PDF_NAME(Unix));
	if (!filename) filename = pdf_dict_get(ctx, fs, PDF_NAME(DOS));
	if (!filename) filename = pdf_dict_get(ctx, fs, PDF_NAME(Mac));
	out->filename = pdf_to_text_string(ctx, filename);

	subtype = pdf_dict_get(ctx, file, PDF_NAME(Subtype));
	if (!subtype)
		out->mimetype = "application/octet-stream";
	else
		out->mimetype = pdf_to_name(ctx, subtype);
	out->size = pdf_dict_get_int(ctx, params, PDF_NAME(Size));
	out->created = pdf_dict_get_date(ctx, params, PDF_NAME(CreationDate));
	out->modified = pdf_dict_get_date(ctx, params, PDF_NAME(ModDate));
}

fz_buffer *
pdf_load_embedded_file_contents(fz_context *ctx, pdf_obj *fs)
{
	if (!pdf_is_embedded_file(ctx, fs))
		return NULL;
	return pdf_load_stream(ctx, pdf_embedded_file_stream(ctx, fs));
}

int
pdf_verify_embedded_file_checksum(fz_context *ctx, pdf_obj *fs)
{
	unsigned char digest[16];
	pdf_obj *file, *params;
	const char *checksum;
	fz_buffer *contents;
	int valid = 0;
	size_t len;

	if (!pdf_is_embedded_file(ctx, fs))
		return 1;

	file = pdf_embedded_file_stream(ctx, fs);
	params = pdf_dict_get(ctx, file, PDF_NAME(Params));
	checksum = pdf_dict_get_string(ctx, params, PDF_NAME(CheckSum), &len);
	if (!checksum || strlen(checksum) == 0)
		return 1;

	valid = 0;

	fz_try(ctx)
	{
		file = pdf_embedded_file_stream(ctx, fs);
		contents = pdf_load_stream(ctx, file);
		fz_md5_buffer(ctx, contents, digest);
		if (len == nelem(digest) && !memcmp(digest, checksum, nelem(digest)))
			valid = 1;
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return valid;
}

static const char *
pdf_guess_mime_type_from_file_name(fz_context *ctx, const char *filename)
{
	const char *ext = strrchr(filename, '.');
	if (ext)
	{
		if (!fz_strcasecmp(ext, ".pdf")) return "application/pdf";
		if (!fz_strcasecmp(ext, ".xml")) return "application/xml";
		if (!fz_strcasecmp(ext, ".zip")) return "application/zip";
		if (!fz_strcasecmp(ext, ".tar")) return "application/x-tar";

		/* Text */
		if (!fz_strcasecmp(ext, ".txt")) return "text/plain";
		if (!fz_strcasecmp(ext, ".rtf")) return "application/rtf";
		if (!fz_strcasecmp(ext, ".csv")) return "text/csv";
		if (!fz_strcasecmp(ext, ".html")) return "text/html";
		if (!fz_strcasecmp(ext, ".htm")) return "text/html";
		if (!fz_strcasecmp(ext, ".css")) return "text/css";

		/* Office */
		if (!fz_strcasecmp(ext, ".doc")) return "application/msword";
		if (!fz_strcasecmp(ext, ".ppt")) return "application/vnd.ms-powerpoint";
		if (!fz_strcasecmp(ext, ".xls")) return "application/vnd.ms-excel";
		if (!fz_strcasecmp(ext, ".docx")) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
		if (!fz_strcasecmp(ext, ".pptx")) return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
		if (!fz_strcasecmp(ext, ".xlsx")) return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
		if (!fz_strcasecmp(ext, ".odt")) return "application/vnd.oasis.opendocument.text";
		if (!fz_strcasecmp(ext, ".odp")) return "application/vnd.oasis.opendocument.presentation";
		if (!fz_strcasecmp(ext, ".ods")) return "application/vnd.oasis.opendocument.spreadsheet";

		/* Image */
		if (!fz_strcasecmp(ext, ".bmp")) return "image/bmp";
		if (!fz_strcasecmp(ext, ".gif")) return "image/gif";
		if (!fz_strcasecmp(ext, ".jpeg")) return "image/jpeg";
		if (!fz_strcasecmp(ext, ".jpg")) return "image/jpeg";
		if (!fz_strcasecmp(ext, ".png")) return "image/png";
		if (!fz_strcasecmp(ext, ".svg")) return "image/svg+xml";
		if (!fz_strcasecmp(ext, ".tif")) return "image/tiff";
		if (!fz_strcasecmp(ext, ".tiff")) return "image/tiff";

		/* Sound */
		if (!fz_strcasecmp(ext, ".flac")) return "audio/flac";
		if (!fz_strcasecmp(ext, ".mp3")) return "audio/mpeg";
		if (!fz_strcasecmp(ext, ".ogg")) return "audio/ogg";
		if (!fz_strcasecmp(ext, ".wav")) return "audio/wav";

		/* Movie */
		if (!fz_strcasecmp(ext, ".avi")) return "video/x-msvideo";
		if (!fz_strcasecmp(ext, ".mov")) return "video/quicktime";
		if (!fz_strcasecmp(ext, ".mp4")) return "video/mp4";
		if (!fz_strcasecmp(ext, ".webm")) return "video/webm";
	}
	return "application/octet-stream";
}

pdf_obj *
pdf_add_embedded_file(fz_context *ctx, pdf_document *doc,
	const char *filename, const char *mimetype, fz_buffer *contents,
	int64_t created, int64_t modified, int add_checksum)
{
	pdf_obj *file = NULL;
	pdf_obj *filespec = NULL;
	pdf_obj *params = NULL;

	fz_var(file);
	fz_var(filespec);

	if (!mimetype)
		mimetype = pdf_guess_mime_type_from_file_name(ctx, filename);

	pdf_begin_operation(ctx, doc, "Embed file");
	fz_try(ctx)
	{
		file = pdf_add_new_dict(ctx, doc, 3);
		pdf_dict_put(ctx, file, PDF_NAME(Type), PDF_NAME(EmbeddedFile));
		pdf_dict_put_name(ctx, file, PDF_NAME(Subtype), mimetype);
		pdf_update_stream(ctx, doc, file, contents, 0);

		params = pdf_dict_put_dict(ctx, file, PDF_NAME(Params), 4);
		pdf_dict_put_int(ctx, params, PDF_NAME(Size), fz_buffer_storage(ctx, contents, NULL));
		if (created >= 0)
				pdf_dict_put_date(ctx, params, PDF_NAME(CreationDate), created);
		if (modified >= 0)
				pdf_dict_put_date(ctx, params, PDF_NAME(ModDate), modified);
		if (add_checksum)
		{
			unsigned char digest[16];
			fz_md5_buffer(ctx, contents, digest);
				pdf_dict_put_string(ctx, params, PDF_NAME(CheckSum), (const char *) digest, nelem(digest));
		}

		filespec = pdf_add_filespec(ctx, doc, filename, file);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, file);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, filespec);
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}

	return filespec;
}

char *
pdf_parse_link_action(fz_context *ctx, pdf_document *doc, pdf_obj *action, int pagenum)
{
	pdf_obj *obj, *dest, *file_spec;

	if (!action)
		return NULL;

	obj = pdf_dict_get(ctx, action, PDF_NAME(S));
	if (pdf_name_eq(ctx, PDF_NAME(GoTo), obj))
	{
		dest = pdf_dict_get(ctx, action, PDF_NAME(D));
		return pdf_parse_link_dest(ctx, doc, dest);
	}
	else if (pdf_name_eq(ctx, PDF_NAME(URI), obj))
	{
		/* URI entries are ASCII strings */
		const char *uri = pdf_dict_get_text_string(ctx, action, PDF_NAME(URI));
		if (!fz_is_external_link(ctx, uri))
		{
			pdf_obj *uri_base_obj = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/URI/Base");
			const char *uri_base = uri_base_obj ? pdf_to_text_string(ctx, uri_base_obj) : "file://";
			char *new_uri = Memento_label(fz_malloc(ctx, strlen(uri_base) + strlen(uri) + 1), "link_action");
			strcpy(new_uri, uri_base);
			strcat(new_uri, uri);
			return new_uri;
		}
		return fz_strdup(ctx, uri);
	}
	else if (pdf_name_eq(ctx, PDF_NAME(Launch), obj))
	{
		file_spec = pdf_dict_get(ctx, action, PDF_NAME(F));
		return pdf_parse_file_spec(ctx, doc, file_spec, NULL, 0);
	}
	else if (pdf_name_eq(ctx, PDF_NAME(GoToR), obj))
	{
		dest = pdf_dict_get(ctx, action, PDF_NAME(D));
		file_spec = pdf_dict_get(ctx, action, PDF_NAME(F));
		return pdf_parse_file_spec(ctx, doc, file_spec, dest, 1);
	}
	else if (pdf_name_eq(ctx, PDF_NAME(Named), obj))
	{
		dest = pdf_dict_get(ctx, action, PDF_NAME(N));

		if (pdf_name_eq(ctx, PDF_NAME(FirstPage), dest))
			pagenum = 0;
		else if (pdf_name_eq(ctx, PDF_NAME(LastPage), dest))
			pagenum = pdf_count_pages(ctx, doc) - 1;
		else if (pdf_name_eq(ctx, PDF_NAME(PrevPage), dest) && pagenum >= 0)
		{
			if (pagenum > 0)
				pagenum--;
		}
		else if (pdf_name_eq(ctx, PDF_NAME(NextPage), dest) && pagenum >= 0)
		{
			if (pagenum < pdf_count_pages(ctx, doc) - 1)
				pagenum++;
		}
		else
			return NULL;

		return fz_asprintf(ctx, "#page=%d", pagenum + 1);
	}

	return NULL;
}

static void pdf_drop_link_imp(fz_context *ctx, fz_link *link)
{
	pdf_drop_obj(ctx, ((pdf_link *) link)->obj);
}

static void pdf_set_link_rect(fz_context *ctx, fz_link *link_, fz_rect rect)
{
	pdf_link *link = (pdf_link *) link_;
	if (link == NULL)
		return;

	pdf_begin_operation(ctx, link->page->doc, "Set link rectangle");

	fz_try(ctx)
	{
		pdf_dict_put_rect(ctx, link->obj, PDF_NAME(Rect), rect);
		link->super.rect = rect;
		pdf_end_operation(ctx, link->page->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, link->page->doc);
		fz_rethrow(ctx);
	}
}

static void pdf_set_link_uri(fz_context *ctx, fz_link *link_, const char *uri)
{
	pdf_link *link = (pdf_link *) link_;
	if (link == NULL)
		return;

	pdf_begin_operation(ctx, link->page->doc, "Set link uri");

	fz_try(ctx)
	{
		pdf_dict_put_drop(ctx, link->obj, PDF_NAME(A),
				pdf_new_action_from_link(ctx, link->page->doc, uri));
		fz_free(ctx, link->super.uri);
		link->super.uri = fz_strdup(ctx, uri);
		pdf_end_operation(ctx, link->page->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, link->page->doc);
		fz_rethrow(ctx);
	}
}

fz_link *pdf_new_link(fz_context *ctx, pdf_page *page, fz_rect rect, const char *uri, pdf_obj *obj)
{
	pdf_link *link = fz_new_derived_link(ctx, pdf_link, rect, uri);
	link->super.drop = (fz_link_drop_link_fn*) pdf_drop_link_imp;
	link->super.set_rect_fn = pdf_set_link_rect;
	link->super.set_uri_fn = pdf_set_link_uri;
	link->page = page; /* only borrowed, as the page owns the link */
	link->obj = pdf_keep_obj(ctx, obj);
	return &link->super;
}

static fz_link *
pdf_load_link(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_obj *dict, int pagenum, fz_matrix page_ctm)
{
	pdf_obj *action;
	pdf_obj *obj;
	fz_rect bbox;
	char *uri;
	fz_link *link = NULL;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Subtype));
	if (!pdf_name_eq(ctx, obj, PDF_NAME(Link)))
		return NULL;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Rect));
	if (!obj)
		return NULL;

	bbox = pdf_to_rect(ctx, obj);
	bbox = fz_transform_rect(bbox, page_ctm);

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Dest));
	if (obj)
		uri = pdf_parse_link_dest(ctx, doc, obj);
	else
	{
		action = pdf_dict_get(ctx, dict, PDF_NAME(A));
		/* fall back to additional action button's down/up action */
		if (!action)
			action = pdf_dict_geta(ctx, pdf_dict_get(ctx, dict, PDF_NAME(AA)), PDF_NAME(U), PDF_NAME(D));
		uri = pdf_parse_link_action(ctx, doc, action, pagenum);
	}

	if (!uri)
		return NULL;

	fz_try(ctx)
		link = (fz_link *) pdf_new_link(ctx, page, bbox, uri, dict);
	fz_always(ctx)
		fz_free(ctx, uri);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return link;
}

fz_link *
pdf_load_link_annots(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_obj *annots, int pagenum, fz_matrix page_ctm)
{
	fz_link *link, *head, *tail;
	pdf_obj *obj;
	int i, n;

	head = tail = NULL;
	link = NULL;

	n = pdf_array_len(ctx, annots);
	for (i = 0; i < n; i++)
	{
		/* FIXME: Move the try/catch out of the loop for performance? */
		fz_try(ctx)
		{
			obj = pdf_array_get(ctx, annots, i);
			link = pdf_load_link(ctx, doc, page, obj, pagenum, page_ctm);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			link = NULL;
		}

		if (link)
		{
			if (!head)
				head = tail = link;
			else
			{
				tail->next = link;
				tail = link;
			}
		}
	}

	return head;
}

static char*
format_explicit_dest_link_uri(fz_context *ctx, const char *schema, const char *uri, fz_link_dest dest)
{
	int pageno = dest.loc.page < 0 ? 1 : dest.loc.page + 1;
	int has_frag;

	if (!schema)
		schema = "";
	if (!uri)
		uri = "";

	has_frag = !!strchr(uri, '#');

	switch (dest.type)
	{
	default:
		return fz_asprintf(ctx, "%s%s%cpage=%d", schema, uri, "#&"[has_frag], pageno);
	case FZ_LINK_DEST_FIT:
		return fz_asprintf(ctx, "%s%s%cpage=%d&view=Fit", schema, uri, "#&"[has_frag], pageno);
	case FZ_LINK_DEST_FIT_B:
		return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitB", schema, uri, "#&"[has_frag], pageno);
	case FZ_LINK_DEST_FIT_H:
		if (isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitH", schema, uri, "#&"[has_frag], pageno);
		else
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitH,%g", schema, uri, "#&"[has_frag], pageno, dest.y);
	case FZ_LINK_DEST_FIT_BH:
		if (isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitBH", schema, uri, "#&"[has_frag], pageno);
		else
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitBH,%g", schema, uri, "#&"[has_frag], pageno, dest.y);
	case FZ_LINK_DEST_FIT_V:
		if (isnan(dest.x))
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitV", schema, uri, "#&"[has_frag], pageno);
		else
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitV,%g", schema, uri, "#&"[has_frag], pageno, dest.x);
	case FZ_LINK_DEST_FIT_BV:
		if (isnan(dest.x))
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitBV", schema, uri, "#&"[has_frag], pageno);
		else
			return fz_asprintf(ctx, "%s%s%cpage=%d&view=FitBV,%g", schema, uri, "#&"[has_frag], pageno, dest.x);
	case FZ_LINK_DEST_XYZ:
		if (!isnan(dest.zoom) && !isnan(dest.x) && !isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=%g,%g,%g", schema, uri, "#&"[has_frag], pageno, dest.zoom, dest.x, dest.y);
		else if (!isnan(dest.zoom) && !isnan(dest.x) && isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=%g,%g,nan", schema, uri, "#&"[has_frag], pageno, dest.zoom, dest.x);
		else if (!isnan(dest.zoom) && isnan(dest.x) && !isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=%g,nan,%g", schema, uri, "#&"[has_frag], pageno, dest.zoom, dest.y);
		else if (!isnan(dest.zoom) && isnan(dest.x) && isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=%g,nan,nan", schema, uri, "#&"[has_frag], pageno, dest.zoom);
		else if (isnan(dest.zoom) && !isnan(dest.x) && !isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=nan,%g,%g", schema, uri, "#&"[has_frag], pageno, dest.x, dest.y);
		else if (isnan(dest.zoom) && !isnan(dest.x) && isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=nan,%g,nan", schema, uri, "#&"[has_frag], pageno, dest.x);
		else if (isnan(dest.zoom) && isnan(dest.x) && !isnan(dest.y))
			return fz_asprintf(ctx, "%s%s%cpage=%d&zoom=nan,nan,%g", schema, uri, "#&"[has_frag], pageno, dest.y);
		else
			return fz_asprintf(ctx, "%s%s%cpage=%d", schema, uri, "#&"[has_frag], pageno);
	case FZ_LINK_DEST_FIT_R:
		return fz_asprintf(ctx, "%s%s%cpage=%d&viewrect=%g,%g,%g,%g", schema, uri, "#&"[has_frag], pageno,
			dest.x, dest.y, dest.w, dest.h);
	}
}

static char*
format_named_dest_link_uri(fz_context *ctx, const char *schema, const char *path, const char *name)
{
	if (!schema)
		schema = "";
	if (!path)
		path = "";
	return fz_asprintf(ctx, "%s%s#nameddest=%s", schema, path, name);
}

static char *
pdf_format_remote_link_uri_from_name(fz_context *ctx, const char *name)
{
	char *encoded_name = NULL;
	char *uri = NULL;

	encoded_name = fz_encode_uri_component(ctx, name);
	fz_try(ctx)
		uri = format_named_dest_link_uri(ctx, NULL, NULL, encoded_name);
	fz_always(ctx)
		fz_free(ctx, encoded_name);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return uri;
}

static int
is_file_uri(fz_context *ctx, const char *uri)
{
	return uri && !strncmp(uri, "file:", 5);
}

static int
has_explicit_dest(fz_context *ctx, const char *uri)
{
	const char *fragment;
	if (uri == NULL || (fragment = strchr(uri, '#')) == NULL)
		return 0;
	return strstr(fragment, "page=") != NULL;
}

static int
has_named_dest(fz_context *ctx, const char *uri)
{
	const char *fragment;
	if (uri == NULL || (fragment = strchr(uri, '#')) == NULL)
		return 0;
	return (strstr(fragment, "nameddest=") || !has_explicit_dest(ctx, uri));
}

static char *
parse_file_uri_path(fz_context *ctx, const char *uri)
{
	char *frag, *path, *temp;

	temp = fz_strdup(ctx, uri + 5);
	fz_try(ctx)
	{
		frag = strchr(temp, '#');
		if (frag)
			*frag = 0;
		path = fz_decode_uri_component(ctx, temp);
		fz_cleanname(path);
	}
	fz_always(ctx)
		fz_free(ctx, temp);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return path;
}

static char *
parse_uri_named_dest(fz_context *ctx, const char *uri)
{
	const char *nameddest_s = strstr(uri, "nameddest=");
	if (nameddest_s)
	{
		char *temp = fz_strdup(ctx, nameddest_s + 10);
		char *dest;
		fz_try(ctx)
		{
			char *ampersand = strchr(temp, '&');
			if (ampersand)
				*ampersand = 0;
			dest = fz_decode_uri_component(ctx, temp);
		}
		fz_always(ctx)
			fz_free(ctx, temp);
		fz_catch(ctx)
			fz_rethrow(ctx);
		return dest;
	}

	// We know there must be a # because of the check in has_named_dest
	return fz_decode_uri_component(ctx, strchr(uri, '#') + 1);
}

static float next_float(const char *str, int eatcomma, char **end)
{
	if (eatcomma && *str == ',')
		++str;
	return fz_strtof(str, end);
}

static fz_link_dest
pdf_new_explicit_dest_from_uri(fz_context *ctx, pdf_document *doc, const char *uri)
{
	char *page, *rect, *zoom, *view;
	fz_link_dest val = fz_make_link_dest_none();

	uri = uri ? strchr(uri, '#') : NULL;

	page = uri ? strstr(uri, "page=") : NULL;
	rect = uri ? strstr(uri, "viewrect=") : NULL;
	zoom = uri ? strstr(uri, "zoom=") : NULL;
	view = uri ? strstr(uri, "view=") : NULL;

	val.loc.chapter = 0;

	if (page)
	{
		val.loc.page = fz_atoi(page+5) - 1;
		val.loc.page = fz_maxi(val.loc.page, 0);
	}
	else
		val.loc.page = 0;

	if (rect)
	{
		rect += 9;
		val.type = FZ_LINK_DEST_FIT_R;
		val.x = next_float(rect, 0, &rect);
		val.y = next_float(rect, 1, &rect);
		val.w = next_float(rect, 1, &rect);
		val.h = next_float(rect, 1, &rect);
	}
	else if (zoom)
	{
		zoom += 5;
		val.type = FZ_LINK_DEST_XYZ;
		val.zoom = next_float(zoom, 0, &zoom);
		val.x = next_float(zoom, 1, &zoom);
		val.y = next_float(zoom, 1, &zoom);
		if (val.zoom <= 0 || isinf(val.zoom))
			val.zoom = 100;
	}
	else if (view)
	{
		view += 5;
		if (!fz_strncasecmp(view, "FitH", 4))
		{
			view += 4;
			val.type = FZ_LINK_DEST_FIT_H;
			val.y = strchr(view, ',') ? next_float(view, 1, &view) : NAN;
		}
		else if (!fz_strncasecmp(view, "FitBH", 5))
		{
			view += 5;
			val.type = FZ_LINK_DEST_FIT_BH;
			val.y = strchr(view, ',') ? next_float(view, 1, &view) : NAN;
		}
		else if (!fz_strncasecmp(view, "FitV", 4))
		{
			view += 4;
			val.type = FZ_LINK_DEST_FIT_V;
			val.x = strchr(view, ',') ? next_float(view, 1, &view) : NAN;
		}
		else if (!fz_strncasecmp(view, "FitBV", 5))
		{
			view += 5;
			val.type = FZ_LINK_DEST_FIT_BV;
			val.x = strchr(view, ',') ? next_float(view, 1, &view) : NAN;
		}
		else if (!fz_strncasecmp(view, "FitB", 4))
		{
			val.type = FZ_LINK_DEST_FIT_B;
		}
		else if (!fz_strncasecmp(view, "Fit", 3))
		{
			val.type = FZ_LINK_DEST_FIT;
		}
	}

	return val;
}

char *
pdf_append_named_dest_to_uri(fz_context *ctx, const char *uri, const char *name)
{
	char *encoded_name = NULL;
	char *new_uri = NULL;
	int has_frag;

	if (!uri)
		uri = "";

	has_frag = !!strchr(uri, '#');

	encoded_name = fz_encode_uri_component(ctx, name);
	fz_try(ctx)
		new_uri = fz_asprintf(ctx, "%s%cnameddest=%s", uri, "#&"[has_frag], encoded_name);
	fz_always(ctx)
		fz_free(ctx, encoded_name);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return new_uri;
}

char *
pdf_append_explicit_dest_to_uri(fz_context *ctx, const char *uri, fz_link_dest dest)
{
	return format_explicit_dest_link_uri(ctx, NULL, uri, dest);
}

char *
pdf_new_uri_from_path_and_named_dest(fz_context *ctx, const char *path, const char *name)
{
	const char *schema = NULL;
	char *encoded_name = NULL;
	char *encoded_path = NULL;
	char *uri = NULL;

	fz_var(encoded_name);
	fz_var(encoded_path);

	fz_try(ctx)
	{
		if (path && strlen(path) > 0)
		{
			if (path[0] == '/')
				schema = "file://";
			else
				schema = "file:";
			encoded_path = fz_encode_uri_pathname(ctx, path);
			fz_cleanname(encoded_path);
		}

		encoded_name = fz_encode_uri_component(ctx, name);
		uri = format_named_dest_link_uri(ctx, schema, encoded_path, encoded_name);
	}
	fz_always(ctx)
	{
		fz_free(ctx, encoded_name);
		fz_free(ctx, encoded_path);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return uri;
}

char *
pdf_new_uri_from_path_and_explicit_dest(fz_context *ctx, const char *path, fz_link_dest dest)
{
	const char *schema = NULL;
	char *encoded_path = NULL;
	char *uri = NULL;

	fz_var(encoded_path);

	fz_try(ctx)
	{
		if (path && strlen(path) > 0)
		{
			if (path[0] == '/')
				schema = "file://";
			else
				schema = "file:";
			encoded_path = fz_encode_uri_pathname(ctx, path);
			fz_cleanname(encoded_path);
		}

		uri = format_explicit_dest_link_uri(ctx, schema, encoded_path, dest);
	}
	fz_always(ctx)
		fz_free(ctx, encoded_path);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return uri;
}

char *
pdf_new_uri_from_explicit_dest(fz_context *ctx, fz_link_dest dest)
{
	return format_explicit_dest_link_uri(ctx, NULL, NULL, dest);
}

char *
pdf_parse_link_dest(fz_context *ctx, pdf_document *doc, pdf_obj *dest)
{
	if (pdf_is_array(ctx, dest) && pdf_array_len(ctx, dest) >= 1)
	{
		fz_link_dest destination = fz_make_link_dest_none();
		populate_destination(ctx, doc, dest, 0, &destination);
		return format_explicit_dest_link_uri(ctx, NULL, NULL, destination);
	}
	else if (pdf_is_name(ctx, dest))
	{
		const char *name = pdf_to_name(ctx, dest);
		return pdf_format_remote_link_uri_from_name(ctx, name);
	}
	else if (pdf_is_string(ctx, dest))
	{
		const char *name = pdf_to_text_string(ctx, dest);
		return pdf_format_remote_link_uri_from_name(ctx, name);
	}
	else
	{
		fz_warn(ctx, "invalid link destination");
		return NULL;
	}
}

static pdf_obj *
pdf_add_filespec_from_link(fz_context *ctx, pdf_document *doc, const char *uri)
{
	char *file = NULL;
	pdf_obj *filespec = NULL;
	fz_try(ctx)
	{
		if (is_file_uri(ctx, uri))
		{
			file = parse_file_uri_path(ctx, uri);
			filespec = pdf_add_filespec(ctx, doc, file, NULL);
		}
		else if (fz_is_external_link(ctx, uri))
			filespec = pdf_add_url_filespec(ctx, doc, uri);
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "can not add non-uri as file specification");
	}
	fz_always(ctx)
		fz_free(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return filespec;
}


pdf_obj *
pdf_new_action_from_link(fz_context *ctx, pdf_document *doc, const char *uri)
{
	pdf_obj *action = pdf_new_dict(ctx, doc, 2);
	char *file = NULL;

	fz_var(file);

	if (uri == NULL)
		return NULL;

	fz_try(ctx)
	{
		if (uri[0] == '#')
		{
			pdf_dict_put(ctx, action, PDF_NAME(S), PDF_NAME(GoTo));
			pdf_dict_put_drop(ctx, action, PDF_NAME(D),
				pdf_new_dest_from_link(ctx, doc, uri, 0));
		}
		else if (!strncmp(uri, "file:", 5))
		{
			pdf_dict_put(ctx, action, PDF_NAME(S), PDF_NAME(GoToR));
			pdf_dict_put_drop(ctx, action, PDF_NAME(D),
				pdf_new_dest_from_link(ctx, doc, uri, 1));
			pdf_dict_put_drop(ctx, action, PDF_NAME(F),
				pdf_add_filespec_from_link(ctx, doc, uri));
		}
		else if (fz_is_external_link(ctx, uri))
		{
			pdf_dict_put(ctx, action, PDF_NAME(S), PDF_NAME(URI));
			pdf_dict_put_text_string(ctx, action, PDF_NAME(URI), uri);
		}
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "unsupported link URI type");
	}
	fz_always(ctx)
		fz_free(ctx, file);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, action);
		fz_rethrow(ctx);
	}

	return action;
}


pdf_obj *pdf_add_filespec(fz_context *ctx, pdf_document *doc, const char *filename, pdf_obj *embedded_file)
{
	pdf_obj *filespec = NULL;
	char *asciiname = NULL;
	const char *s;
	size_t len, i;

	fz_var(asciiname);
	fz_var(filespec);

	fz_try(ctx)
	{
		len = strlen(filename) + 1;
		asciiname = fz_malloc(ctx, len);

		for (i = 0, s = filename; *s && i + 1 < len; ++i)
		{
			int c;
			s += fz_chartorune(&c, s);
			asciiname[i] = (c >= 32 && c <= 126) ? c : '_';
		}
		asciiname[i] = 0;

		filespec = pdf_add_new_dict(ctx, doc, 4);
		pdf_dict_put(ctx, filespec, PDF_NAME(Type), PDF_NAME(Filespec));
		pdf_dict_put_text_string(ctx, filespec, PDF_NAME(F), asciiname);
		pdf_dict_put_text_string(ctx, filespec, PDF_NAME(UF), filename);
		if (embedded_file)
		{
			pdf_obj *ef = pdf_dict_put_dict(ctx, filespec, PDF_NAME(EF), 1);
			pdf_dict_put(ctx, ef, PDF_NAME(F), embedded_file);
		}
	}
	fz_always(ctx)
		fz_free(ctx, asciiname);
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, filespec);
		fz_rethrow(ctx);
	}

	return filespec;
}

pdf_obj *pdf_add_url_filespec(fz_context *ctx, pdf_document *doc, const char *url)
{
	pdf_obj *filespec = pdf_add_new_dict(ctx, doc, 3);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, filespec, PDF_NAME(Type), PDF_NAME(Filespec));
		pdf_dict_put(ctx, filespec, PDF_NAME(FS), PDF_NAME(URL));
		pdf_dict_put_text_string(ctx, filespec, PDF_NAME(F), url);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, filespec);
		fz_rethrow(ctx);
	}
	return filespec;
}

pdf_obj *
pdf_new_dest_from_link(fz_context *ctx, pdf_document *doc, const char *uri, int is_remote)
{
	pdf_obj *dest = NULL;

	fz_var(dest);

	if (has_named_dest(ctx, uri))
	{
		char *name = parse_uri_named_dest(ctx, uri);

		fz_try(ctx)
			dest = pdf_new_text_string(ctx, name);
		fz_always(ctx)
			fz_free(ctx, name);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else
	{
		fz_matrix ctm, invctm;
		fz_link_dest val;
		pdf_obj *pageobj;
		fz_point p;
		fz_rect r;

		fz_try(ctx)
		{
			val = pdf_new_explicit_dest_from_uri(ctx, doc, uri);

			dest = pdf_new_array(ctx, doc, 6);

			if (is_remote)
			{
				pdf_array_push_int(ctx, dest, val.loc.page);
				invctm = fz_identity;
			}
			else
			{
				pageobj = pdf_lookup_page_obj(ctx, doc, val.loc.page);
				pdf_array_push(ctx, dest, pageobj);

				pdf_page_obj_transform(ctx, pageobj, NULL, &ctm);
				invctm = fz_invert_matrix(ctm);
			}

			switch (val.type)
			{
			default:
			case FZ_LINK_DEST_FIT:
				pdf_array_push(ctx, dest, PDF_NAME(Fit));
				break;
			case FZ_LINK_DEST_FIT_H:
				p = fz_transform_point_xy(0, val.y, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(FitH));
				if (isnan(p.y))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.y);
				break;
			case FZ_LINK_DEST_FIT_BH:
				p = fz_transform_point_xy(0, val.y, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(FitBH));
				if (isnan(p.y))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.y);
				break;
			case FZ_LINK_DEST_FIT_V:
				p = fz_transform_point_xy(val.x, 0, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(FitV));
				if (isnan(p.x))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.x);
				break;
			case FZ_LINK_DEST_FIT_BV:
				p = fz_transform_point_xy(val.x, 0, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(FitBV));
				if (isnan(p.x))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.x);
				break;
			case FZ_LINK_DEST_XYZ:
				p = fz_transform_point_xy(val.x, val.y, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(XYZ));
				if (isnan(p.x))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.x);
				if (isnan(p.y))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, p.y);
				if (isnan(val.zoom))
					pdf_array_push(ctx, dest, PDF_NULL);
				else
					pdf_array_push_real(ctx, dest, val.zoom / 100);
				break;
			case FZ_LINK_DEST_FIT_R:
				r.x0 = val.x;
				r.y0 = val.y;
				r.x1 = val.x + val.w;
				r.y1 = val.y + val.h;
				fz_transform_rect(r, invctm);
				pdf_array_push(ctx, dest, PDF_NAME(FitR));
				pdf_array_push_real(ctx, dest, r.x0);
				pdf_array_push_real(ctx, dest, r.y0);
				pdf_array_push_real(ctx, dest, r.x1);
				pdf_array_push_real(ctx, dest, r.y1);
				break;
			}
		}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, dest);
			fz_rethrow(ctx);
		}
	}

	return dest;
}

fz_link_dest
pdf_resolve_link_dest(fz_context *ctx, pdf_document *doc, const char *uri)
{
	fz_link_dest dest = fz_make_link_dest_none();
	pdf_obj *page_obj;
	fz_matrix page_ctm;
	fz_rect mediabox;
	pdf_obj *needle = NULL;
	char *name = NULL;
	char *desturi = NULL;
	pdf_obj *destobj = NULL;

	fz_var(needle);
	fz_var(name);

	fz_try(ctx)
	{
		if (has_explicit_dest(ctx, uri))
		{
			dest = pdf_new_explicit_dest_from_uri(ctx, doc, uri);
			if (!isnan(dest.x) || !isnan(dest.y) || !isnan(dest.w) || !isnan(dest.h))
			{
				page_obj = pdf_lookup_page_obj(ctx, doc, dest.loc.page);
				pdf_page_obj_transform(ctx, page_obj, &mediabox, &page_ctm);
				mediabox = fz_transform_rect(mediabox, page_ctm);

				/* clamp coordinates to remain on page */
				dest.x = fz_clamp(dest.x, 0, mediabox.x1 - mediabox.x0);
				dest.y = fz_clamp(dest.y, 0, mediabox.y1 - mediabox.y0);
				dest.w = fz_clamp(dest.w, 0, mediabox.x1 - dest.x);
				dest.h = fz_clamp(dest.h, 0, mediabox.y1 - dest.y);
			}
		}
		else if (has_named_dest(ctx, uri))
		{
			name = parse_uri_named_dest(ctx, uri);

			needle = pdf_new_text_string(ctx, name);
			destobj = resolve_dest(ctx, doc, needle);
			if (destobj)
			{
				fz_link_dest destdest;
				desturi = pdf_parse_link_dest(ctx, doc, destobj);
				destdest = pdf_resolve_link_dest(ctx, doc, desturi);
				if (dest.type == FZ_LINK_DEST_XYZ && isnan(dest.x) && isnan(dest.y) && isnan(dest.zoom))
					dest = destdest;
				else
					dest.loc = destdest.loc;
			}
		}
		else
			dest.loc.page = fz_atoi(uri) - 1;
	}
	fz_always(ctx)
	{
		fz_free(ctx, desturi);
		fz_free(ctx, name);
		pdf_drop_obj(ctx, needle);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return dest.loc.page >= 0 ? dest : fz_make_link_dest_none();
}

int
pdf_resolve_link(fz_context *ctx, pdf_document *doc, const char *uri, float *xp, float *yp)
{
	fz_link_dest dest = pdf_resolve_link_dest(ctx, doc, uri);
	if (xp) *xp = dest.x;
	if (yp) *yp = dest.y;
	return dest.loc.page;
}
