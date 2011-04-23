/* SumatraPDF: support links and outlines */

#include "fitz.h"
#include "muxps.h"

static xps_outline *
xps_new_outline(char *title, char *target)
{
	xps_outline *outline = fz_malloc(sizeof(xps_outline));

	outline->title = fz_strdup(title);
	outline->target = fz_strdup(target);
	outline->next = NULL;
	outline->child = NULL;

	return outline;
}

static xps_named_dest *
xps_new_named_dest(char *target, int page)
{
	xps_named_dest *dest = fz_malloc(sizeof(xps_named_dest));

	dest->target = fz_strdup(target);
	dest->page = page;
	dest->rect = fz_empty_rect; // to be updated
	dest->next = NULL;

	return dest;
}

static xps_link *
xps_new_link(char *target, fz_rect rect, int is_dest)
{
	xps_link *link = fz_malloc(sizeof(xps_link));

	link->target = fz_strdup(target);
	link->rect = rect;
	link->is_dest = is_dest;
	link->next = NULL;

	return link;
}

void
xps_free_outline(xps_outline *outline)
{
	if (outline->child)
		xps_free_outline(outline->child);
	if (outline->next)
		xps_free_outline(outline->next);
	fz_free(outline->title);
	fz_free(outline->target);
	fz_free(outline);
}

void
xps_free_named_dest(xps_named_dest *dest)
{
	if (dest->next)
		xps_free_named_dest(dest->next);
	fz_free(dest->target);
	fz_free(dest);
}

void
xps_free_link(xps_link *link)
{
	if (link->next)
		xps_free_link(link->next);
	fz_free(link->target);
	fz_free(link);
}

static char *
xps_get_part_base_name(char *name)
{
	char *baseName = strrchr(name, '/');
	if (!baseName)
		baseName = name;
	return baseName + 1;
}

static void
xps_rels_for_part(char *buf, char *name, int bufSize)
{
	char *baseName = xps_get_part_base_name(name);
	fz_strlcpy(buf, name, MIN(baseName - name + 1, bufSize));
	fz_strlcat(buf, "_rels/", bufSize);
	fz_strlcat(buf, baseName, bufSize);
	fz_strlcat(buf, ".rels", bufSize);
}

static int
xps_open_and_parse(xps_context *ctx, char *path, xml_element **rootp)
{
	xps_part *part = xps_read_part(ctx, path);
	if (!part)
		return fz_rethrow(-1, "cannot read zip part '%s'", path);

	*rootp = xml_parse_document(part->data, part->size);
	xps_free_part(ctx, part);

	if (!*rootp)
		return fz_rethrow(-1, "cannot parse metadata for part '%s'", path);
	return fz_okay;
}

static xps_outline *
xps_get_insertion_point(xps_outline *root, int level)
{
	for (; root->next; root = root->next);
	if (level == 0 || !root->child)
		return root;
	return xps_get_insertion_point(root->child, level - 1);
}

// <DocumentStructure xmlns="http://schemas.microsoft.com/xps/2005/06/documentstructure">
//     <DocumentStructure.Outline>
//         <DocumentOutline xml:lang="en-US">
//             <OutlineEntry OutlineLevel="1"
//                           Description="XML Paper Specification"
//                           OutlineTarget="../FixedDoc.fdoc#PG_1_LNK_1812" />
//         </DocumentOutline>
//     </DocumentStructure.Outline>
// </DocumentStructure>

static void
xps_parse_outline_imp(xps_outline **outlinep, xml_element *item, char *base_uri)
{
	int lastLevel = 0;
	for (; item; item = xml_next(item))
	{
		char *description, *target;
		int level;
		char tgtbuf[1024];
		xps_outline *outline;

		xps_parse_outline_imp(outlinep, xml_down(item), base_uri);

		if (strcmp(xml_tag(item), "OutlineEntry") != 0 ||
			!(description = xml_att(item, "Description")) ||
			!(target = xml_att(item, "OutlineTarget")))
			continue;
		if (xml_att(item, "OutlineLevel"))
			level = atoi(xml_att(item, "OutlineLevel"));
		else
			level = lastLevel;

		xps_absolute_path(tgtbuf, base_uri, target, sizeof(tgtbuf));
		outline = xps_new_outline(description, tgtbuf);
		if (!*outlinep)
			*outlinep = outline;
		else if (level > lastLevel)
			xps_get_insertion_point(*outlinep, lastLevel)->child = outline;
		else
			xps_get_insertion_point(*outlinep, level)->next = outline;
		lastLevel = level;
	}
}

static int
xps_parse_outline_structure(xps_outline **outlinep, xps_context *ctx, char *name)
{
	char base_uri[1024];
	xml_element *root;

	int code = xps_open_and_parse(ctx, name, &root);
	if (code != fz_okay)
		return code;

	fz_strlcpy(base_uri, name, sizeof(base_uri));
	*(xps_get_part_base_name(base_uri) - 1) = '\0';

	xps_parse_outline_imp(outlinep, root, base_uri);

	xml_free_element(root);

	return fz_okay;
}

// <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
//     <Relationship Type="http://schemas.microsoft.com/xps/2005/06/documentstructure"
//                   Target="Structure/DocStructure.struct"/>
// </Relationships>

#define REL_DOC_STRUCTURE \
	"http://schemas.microsoft.com/xps/2005/06/documentstructure"

static int
xps_read_and_process_document_outline(xps_outline **outlinep, xps_context *ctx, xps_document *doc)
{
	char base_uri[1024];
	xml_element *root;
	xml_element *item;
	int code = fz_okay;

	xps_rels_for_part(base_uri, doc->name, sizeof(base_uri));

	code = xps_open_and_parse(ctx, base_uri, &root);
	if (code != fz_okay)
		return code;
	*strstr(base_uri, "/_rels/") = '\0';

	for (item = root; item; item = xml_next(item))
	{
		xml_element *relItem;
		if (strcmp(xml_tag(item), "Relationships") != 0)
			continue;
		for (relItem = xml_down(item); relItem; relItem = xml_next(relItem))
		{
			char *target, *type;
			if (!strcmp(xml_tag(relItem), "Relationship") &&
				(target = xml_att(relItem, "Target")) && (type = xml_att(relItem, "Type")) &&
				!strcmp(type, REL_DOC_STRUCTURE))
			{
				char tgtbuf[1024];
				xps_absolute_path(tgtbuf, base_uri, target, sizeof(tgtbuf));
				code = xps_parse_outline_structure(outlinep, ctx, tgtbuf);
			}
		}
	}

	xml_free_element(root);

	return code;
}

xps_outline *
xps_parse_outline(xps_context *ctx)
{
	xps_outline *root = NULL;
	xps_outline **next = &root;
	xps_document *doc;

	for (doc = ctx->first_fixdoc; doc; doc = doc->next)
	{
		int code = xps_read_and_process_document_outline(next, ctx, doc);
		if (code)
			fz_catch(code, "couldn't read the outline for part '%s'", doc->name);
		if (*next)
			next = &(*next)->next;
	}

	return root;
}

// <FixedDocument xmlns="http://schemas.microsoft.com/xps/2005/06">
//     <PageContent Source="Pages/1.fpage">
//         <PageContent.LinkTargets>
//             <LinkTarget Name="PG_1_LNK_1812"/>
//         </PageContent.LinkTargets>
//     </PageContent>
// </FixedDocument>

static void
xps_parse_names_imp(xps_named_dest **destsp, xps_context *ctx, xps_document *doc, xml_element *item, char *base_uri, int page)
{
	char tgtbuf[1024];

	for (; item; item = xml_next(item))
	{
		if (!strcmp(xml_tag(item), "PageContent") && xml_att(item, "Source") && page == 0)
		{
			int i;
			xps_page *page;
			xps_absolute_path(tgtbuf, base_uri, xml_att(item, "Source"), sizeof(tgtbuf));
			for (page = ctx->first_page, i = 0; page; page = page->next, i++)
			{
				if (strcmp(page->name, tgtbuf) != 0)
					continue;
				xps_parse_names_imp(destsp, ctx, doc, xml_down(item), base_uri, i + 1);
				break;
			}
		}
		else if (!strcmp(xml_tag(item), "LinkTarget") && xml_att(item, "Name") && page != 0)
		{
			xps_named_dest *dest;
			fz_strlcpy(tgtbuf, doc->name, sizeof(tgtbuf));
			fz_strlcat(tgtbuf, "#", sizeof(tgtbuf));
			fz_strlcat(tgtbuf, xml_att(item, "Name"), sizeof(tgtbuf));
			dest = xps_new_named_dest(tgtbuf, page);
			if (!*destsp)
				*destsp = dest;
			else
			{
				xps_named_dest *next;
				for (next = *destsp; next->next; next = next->next);
				next->next = dest;
			}
		}
		else
			xps_parse_names_imp(destsp, ctx, doc, xml_down(item), base_uri, page);
	}
}

static int
xps_read_and_process_dest_names(xps_named_dest **destsp, xps_context *ctx, xps_document *doc)
{
	char base_uri[1024];
	xml_element *root;

	int code = xps_open_and_parse(ctx, doc->name, &root);
	if (code != fz_okay)
		return code;

	fz_strlcpy(base_uri, doc->name, sizeof(base_uri));
	*(xps_get_part_base_name(base_uri) - 1) = '\0';

	xps_parse_names_imp(destsp, ctx, doc, root, base_uri, 0);

	xml_free_element(root);

	return fz_okay;
}

xps_named_dest *
xps_parse_named_dests(xps_context *ctx)
{
	xps_named_dest *root = NULL;
	xps_named_dest **next = &root;
	xps_document *doc;

	for (doc = ctx->first_fixdoc; doc; doc = doc->next)
	{
		int code = xps_read_and_process_dest_names(next, ctx, doc);
		if (code)
			fz_catch(code, "couldn't read the destination names for part '%s'", doc->name);
		if (*next)
			next = &(*next)->next;
	}

	return root;
}

void
xps_extract_link_info(xps_context *ctx, xml_element *node, fz_rect rect, char *base_uri)
{
	xps_link *link = NULL;
	char *value;

	if (!ctx->link_root)
		return;

	if ((value = xml_att(node, "FixedPage.NavigateUri")) && !strchr(value, ':'))
	{
		char tgtbuf[1024];
		xps_absolute_path(tgtbuf, base_uri, value, sizeof(tgtbuf));
		link = xps_new_link(tgtbuf, rect, 0);
	}
	else if (value) // link with a protocol (e.g. http://...)
		link = xps_new_link(value, rect, 0);
	else if ((value = xml_att(node, "Name")))
		link = xps_new_link(value, rect, 1);

	// insert the links in top-to-bottom order (first one is to be preferred)
	if (link)
	{
		link->next = ctx->link_root->next;
		ctx->link_root->next = link;
	}
}

/* SumatraPDF: extract document properties (hacky) */

static inline int iswhite(c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// <cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><dc:title>XML Paper Specification</dc:title><dc:subject>XPS Specification and Reference Guide</dc:subject><dc:creator>Jesse McGatha</dc:creator><dcterms:created xsi:type="dcterms:W3CDTF">2006-10-19T01:21:08Z</dcterms:created><dcterms:modified xsi:type="dcterms:W3CDTF">2006-10-19T01:21:08Z</dcterms:modified></cp:coreProperties>

static void
xps_hacky_get_prop(char *data, fz_obj *dict, char *name, char *tag_name)
{
	char *start, *end;
	fz_obj *value;

	start = strstr(data, tag_name);
	if (!start || start == data || start[-1] != '<')
		return;
	end = strstr(start + 1, tag_name);
	start = strchr(start, '>');
	if (!start || !end || start >= end || end[-2] != '<' || end[-1] != '/')
		return;

	for (start++; iswhite(*start); start++);
	for (end -= 3; iswhite(*end) && end > start; end--);

	value = fz_new_string(start, end - start + 1);
	fz_dict_puts(dict, name, value);
	fz_drop_obj(value);
}

#define CONTENT_TYPE_CORE_PROPS \
	"application/vnd.openxmlformats-package.core-properties+xml"

static int
xps_find_doc_props_path(xps_context *ctx, char path[1024])
{
	xml_element *root;

	int code = xps_open_and_parse(ctx, "/[Content_Types].xml", &root);
	if (code != fz_okay)
		return code;

	*path = '\0';
	if (root && !strcmp(xml_tag(root), "Types"))
	{
		xml_element *item;
		for (item = xml_down(root); item; item = xml_next(item))
		{
			if (!strcmp(xml_tag(item), "Override") && xml_att(item, "ContentType") &&
				!strcmp(xml_att(item, "ContentType"), CONTENT_TYPE_CORE_PROPS) &&
				xml_att(item, "PartName"))
			{
				fz_strlcpy(path, xml_att(item, "PartName"), 1024);
			}
		}
	}
	else
		code = fz_throw("couldn't parse part '[Content_Types].xml'");

	xml_free_element(root);

	return code;
}

fz_obj *xps_extract_doc_props(xps_context *ctx)
{
	char path[1024];
	xps_part *part;
	fz_obj *dict;

	if (xps_find_doc_props_path(ctx, path) != fz_okay)
	{
		fz_catch(-1, "couldn't find the exact part name for /docProps/core.xml");
		fz_strlcpy(path, "/docProps/core.xml", sizeof(path));
	}
	if (!*path)
		return NULL;

	part = xps_read_part(ctx, path);
	if (!part)
	{
		fz_catch(-1, "cannot read zip part '%s'", path);
		return NULL;
	}

	dict = fz_new_dict(8);
	xps_hacky_get_prop(part->data, dict, "Title", "dc:title");
	xps_hacky_get_prop(part->data, dict, "Subject", "dc:subject");
	xps_hacky_get_prop(part->data, dict, "Author", "dc:creator");
	xps_hacky_get_prop(part->data, dict, "CreationDate", "dcterms:created");
	xps_hacky_get_prop(part->data, dict, "ModDate", "dcterms:modified");

	xps_free_part(ctx, part);

	return dict;
}
