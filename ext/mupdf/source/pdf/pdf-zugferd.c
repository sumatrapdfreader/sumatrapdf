// Copyright (C) 2024-2025 Artifex Software, Inc.
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

static const char *
tag_or_text(fz_xml *x, const char *find)
{
	const char *text;
	const char *f = strchr(find, ':');

	/* If we find a : we have a namespace. Search for both with and
	 * without the namespace. */
	if (f)
		f++;

	text = fz_xml_att(x, find);
	if (text == NULL && f)
		text = fz_xml_att(x, f);
	if (text == NULL)
		text = fz_xml_text(fz_xml_down(fz_xml_find_down(x, find)));
	if (text == NULL && f)
		text = fz_xml_text(fz_xml_down(fz_xml_find_down(x, f)));

	return text;
}

static enum pdf_zugferd_profile
do_zugferd_profile(fz_context *ctx, pdf_document *doc, float *version, char **fname)
{
	pdf_obj *metadata = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(Metadata), NULL);
	fz_buffer *buf;
	fz_xml *xml = NULL;
	fz_xml *x;
	enum pdf_zugferd_profile ret = PDF_NOT_ZUGFERD;

	if (version)
		*version = 0;
	if (fname)
		*fname = NULL;

	if (metadata == NULL)
		return PDF_NOT_ZUGFERD;

	buf = pdf_load_stream(ctx, metadata);

	fz_var(xml);

	fz_try(ctx)
	{
		xml = fz_parse_xml(ctx, buf, 0);

		/* Version 1. */
		x = fz_xml_find_dfs(xml, "Description", "xmlns:zf", "urn:ferd:pdfa:CrossIndustryDocument:invoice:1p0#");
		if (x)
		{
			while (x)
			{
				/* The Version tag in the document appears to always be 1.0 */
				const char *v = tag_or_text(x, "zf:Version");
				const char *cl = tag_or_text(x, "zf:ConformanceLevel");
				const char *df = tag_or_text(x, "zf:DocumentFileName");
				const char *dt = tag_or_text(x, "zf:DocumentType");
				if (v && dt && !strcmp(dt, "INVOICE"))
				{
					if (!cl)
						fz_warn(ctx, "No conformance level specified");
					else if (!strcmp(cl, "COMFORT"))
						ret = PDF_ZUGFERD_COMFORT;
					else if (!strcmp(cl, "BASIC"))
						ret = PDF_ZUGFERD_BASIC;
					else if (!strcmp(cl, "EXTENDED"))
						ret = PDF_ZUGFERD_EXTENDED;

					if (version)
						*version = fz_atof(v);

					if (!df)
						fz_warn(ctx, "ZUGFeRD doc is missing filename");
					else if (strcmp(df, "ZUGFeRD-invoice.xml"))
						fz_warn(ctx, "ZUGFeRD doc has non-standard filename");
					if (fname && df)
						*fname = fz_strdup(ctx, df); /* Nothing can throw after this */
					break;
				}

				x = fz_xml_find_next_dfs(x, "Description", "xmlns:zf", "urn:ferd:pdfa:CrossIndustryDocument:invoice:1p0#");
			}
			break;
		}

		/* Version 2. */
		x = fz_xml_find_dfs(xml, "Description", "xmlns:fx", "urn:zugferd:pdfa:CrossIndustryDocument:invoice:2p0#");
		if (x)
		{
			while (x)
			{
				const char *v = tag_or_text(x, "fx:Version");
				const char *cl = tag_or_text(x, "fx:ConformanceLevel");
				const char *df = tag_or_text(x, "fx:DocumentFileName");
				const char *dt = tag_or_text(x, "fx:DocumentType");
				if (v && dt && !strcmp(dt, "INVOICE"))
				{
					if (!cl)
						fz_warn(ctx, "No conformance level specified");
					else if (!strcmp(cl, "EN 16931"))
						ret = PDF_ZUGFERD_COMFORT;
					else if (!strcmp(cl, "BASIC"))
						ret = PDF_ZUGFERD_BASIC;
					else if (!strcmp(cl, "EXTENDED"))
						ret = PDF_ZUGFERD_EXTENDED;
					else if (!strcmp(cl, "BASIC WL"))
						ret = PDF_ZUGFERD_BASIC_WL;
					else if (!strcmp(cl, "MINIMUM"))
						ret = PDF_ZUGFERD_MINIMUM;

					if (version)
						*version = fz_atof(v);

					if (!df)
						fz_warn(ctx, "ZUGFeRD doc is missing filename");
					else if (strcmp(df, "zugferd-invoice.xml"))
						fz_warn(ctx, "ZUGFeRD doc has non-standard filename");
					if (fname && df)
						*fname = fz_strdup(ctx, df); /* Nothing can throw after this */
					break;
				}

				x = fz_xml_find_next_dfs(x, "Description", "xmlns:fx", "urn:zugferd:pdfa:CrossIndustryDocument:invoice:2p0#");
			}
			break;
		}

		/* Version 2.1 + 2.11 */
		x = fz_xml_find_dfs(xml, "Description", "xmlns:fx", "urn:factur-x:pdfa:CrossIndustryDocument:invoice:1p0#");
		if (x)
		{
			while (x)
			{
				const char *v = tag_or_text(x, "fx:Version");
				const char *cl = tag_or_text(x, "fx:ConformanceLevel");
				const char *df = tag_or_text(x, "fx:DocumentFileName");
				const char *dt = tag_or_text(x, "fx:DocumentType");
				if (v && dt && !strcmp(dt, "INVOICE"))
				{
					if (!cl)
						fz_warn(ctx, "No conformance level specified");
					else if (!strcmp(cl, "EN 16931"))
						ret = PDF_ZUGFERD_COMFORT;
					else if (!strcmp(cl, "BASIC"))
						ret = PDF_ZUGFERD_BASIC;
					else if (!strcmp(cl, "EXTENDED"))
						ret = PDF_ZUGFERD_EXTENDED;
					else if (!strcmp(cl, "BASIC WL"))
						ret = PDF_ZUGFERD_BASIC_WL;
					else if (!strcmp(cl, "MINIMUM"))
						ret = PDF_ZUGFERD_MINIMUM;
					else if (!strcmp(cl, "XRECHNUNG"))
						ret = PDF_ZUGFERD_XRECHNUNG;

					if (version)
						*version = fz_atof(v);

					if (!df)
						fz_warn(ctx, "ZUGFeRD doc is missing filename");
					else if (ret == PDF_ZUGFERD_XRECHNUNG && strcmp(df, "xrechnung.xml"))
						fz_warn(ctx, "ZUGFeRD doc has non-standard filename");
					else if (ret != PDF_ZUGFERD_XRECHNUNG && strcmp(df, "factur-x.xml"))
						fz_warn(ctx, "ZUGFeRD doc has non-standard filename");
					if (fname && df)
						*fname = fz_strdup(ctx, df); /* Nothing can throw after this */
					break;
				}

				x = fz_xml_find_next_dfs(x, "Description", "xmlns:fx", "urn:factur-x:pdfa:CrossIndustryDocument:invoice:1p0#");
			}
			break;
		}
	}
	fz_always(ctx)
	{
		fz_drop_xml(ctx, xml);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

enum pdf_zugferd_profile pdf_zugferd_profile(fz_context *ctx, pdf_document *doc, float *version)
{
	return do_zugferd_profile(ctx, doc, version, NULL);
}

fz_buffer *pdf_zugferd_xml(fz_context *ctx, pdf_document *doc)
{
	char *fname;
	float version;
	enum pdf_zugferd_profile p = do_zugferd_profile(ctx, doc, &version, &fname);
	int count, i;
	fz_buffer *buf = NULL;

	if (p == PDF_NOT_ZUGFERD)
	{
		fz_free(ctx, fname);
		return NULL;
	}

	fz_try(ctx)
	{
		count = pdf_count_document_associated_files(ctx, doc);
		for (i = 0; i < count; i++)
		{
			pdf_obj *fs = pdf_document_associated_file(ctx, doc, i);
			pdf_filespec_params params;

			pdf_get_filespec_params(ctx, fs, &params);

			if (!strcmp(fname, params.filename))
			{
				if (!pdf_is_embedded_file(ctx, fs))
					fz_throw(ctx, FZ_ERROR_FORMAT, "ZUGFeRD XML was not embedded");

				buf = pdf_load_embedded_file_contents(ctx, fs);
				break;
			}
		}
	}
	fz_always(ctx)
		fz_free(ctx, fname);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return buf;
}

const char *
pdf_zugferd_profile_to_string(fz_context *ctx, enum pdf_zugferd_profile profile)
{
	static const char *strings[] =
	{
		"NOT ZUGFERD",
		"COMFORT",
		"BASIC",
		"EXTENDED",
		"BASIC WL",
		"MINIMUM",
		"XRECHNUNG",
		"UNKNOWN"
	};

	if (profile < PDF_NOT_ZUGFERD || profile > PDF_ZUGFERD_UNKNOWN)
		profile = PDF_ZUGFERD_UNKNOWN;

	return strings[profile];
}
