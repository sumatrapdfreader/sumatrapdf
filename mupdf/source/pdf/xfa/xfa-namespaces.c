// Copyright (C) 2004-2026 Artifex Software, Inc.
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

#include "xfa-imp.h"

#include <string.h>

static const char *pdf_xfa_ns_uris[PDF_XFA_NS_COUNT] =
{
	"http://www.xfa.org/schema/xci/",
	"http://www.xfa.org/schema/xfa-connection-set/",
	"http://www.xfa.org/schema/xfa-data/",
	"http://www.xfa.org/schema/xfa-form/",
	"http://www.xfa.org/schema/xfa-locale-set/",
	"http://ns.adobe.com/xdp/pdf/",
	"http://www.w3.org/2000/09/xmldsig#",
	"http://www.xfa.org/schema/xfa-source-set/",
	"http://www.w3.org/1999/XSL/Transform",
	"http://www.xfa.org/schema/xfa-template/",
	"http://www.xfa.org/schema/xdc/",
	"http://ns.adobe.com/xdp/",
	"http://ns.adobe.com/xfdf/",
	"http://www.w3.org/1999/xhtml",
	"http://ns.adobe.com/xmpmeta/",
	NULL,
};

int
pdf_xfa_ns_check_uri(pdf_xfa_ns_id ns, const char *uri)
{
	const char *prefix;

	if (!uri || ns < 0 || ns >= PDF_XFA_NS_UNKNOWN)
		return 0;

	prefix = pdf_xfa_ns_uris[ns];
	if (!prefix)
		return 0;

	if (ns == PDF_XFA_NS_PDF || ns == PDF_XFA_NS_SIGNATURE ||
		ns == PDF_XFA_NS_STYLESHEET || ns == PDF_XFA_NS_XDP ||
		ns == PDF_XFA_NS_XFDF || ns == PDF_XFA_NS_XHTML ||
		ns == PDF_XFA_NS_XMPMETA)
		return strcmp(uri, prefix) == 0;

	return strncmp(uri, prefix, strlen(prefix)) == 0;
}

const char *
pdf_xfa_ns_uri(pdf_xfa_ns_id ns)
{
	if (ns < 0 || ns >= PDF_XFA_NS_UNKNOWN)
		return NULL;
	return pdf_xfa_ns_uris[ns];
}

pdf_xfa_ns_id
pdf_xfa_ns_from_uri(fz_context *ctx, pdf_xfa_builder *builder, const char *uri)
{
	pdf_xfa_ns_id ns;
	void *cached;

	if (!uri)
		return builder ? builder->current_ns : PDF_XFA_NS_UNKNOWN;

	if (builder && builder->uri_namespaces)
	{
		cached = fz_hash_find(ctx, builder->uri_namespaces, uri);
		if (cached)
			return (pdf_xfa_ns_id)(intptr_t)cached;
	}

	for (ns = 0; ns < PDF_XFA_NS_UNKNOWN; ns++)
	{
		if (pdf_xfa_ns_check_uri(ns, uri))
		{
			if (builder && builder->uri_namespaces)
				fz_hash_insert(ctx, builder->uri_namespaces, (void *)uri, (void *)(intptr_t)ns);
			return ns;
		}
	}

	if (builder)
	{
		pdf_xfa_ns_id unknown = (pdf_xfa_ns_id)builder->next_unknown_ns++;
		if (builder->uri_namespaces)
			fz_hash_insert(ctx, builder->uri_namespaces, (void *)uri, (void *)(intptr_t)unknown);
		return unknown;
	}

	return PDF_XFA_NS_UNKNOWN;
}