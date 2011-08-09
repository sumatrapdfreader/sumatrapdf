#include "fitz.h"
#include "muxps.h"

static xml_element *
xps_find_resource(xps_context *ctx, xps_resource *dict, char *name, char **urip)
{
	xps_resource *head, *node;
	for (head = dict; head; head = head->parent)
	{
		for (node = head; node; node = node->next)
		{
			if (!strcmp(node->name, name))
			{
				if (urip && head->base_uri)
					*urip = head->base_uri;
				return node->data;
			}
		}
	}
	return NULL;
}

static xml_element *
xps_parse_resource_reference(xps_context *ctx, xps_resource *dict, char *att, char **urip)
{
	char name[1024];
	char *s;

	if (strstr(att, "{StaticResource ") != att)
		return NULL;

	fz_strlcpy(name, att + 16, sizeof name);
	s = strrchr(name, '}');
	if (s)
		*s = 0;

	return xps_find_resource(ctx, dict, name, urip);
}

void
xps_resolve_resource_reference(xps_context *ctx, xps_resource *dict,
		char **attp, xml_element **tagp, char **urip)
{
	if (*attp)
	{
		xml_element *rsrc = xps_parse_resource_reference(ctx, dict, *attp, urip);
		if (rsrc)
		{
			*attp = NULL;
			*tagp = rsrc;
		}
	}
}

static int
xps_parse_remote_resource_dictionary(xps_context *ctx, xps_resource **dictp, char *base_uri, char *source_att)
{
	char part_name[1024];
	char part_uri[1024];
	xps_resource *dict;
	xps_part *part;
	xml_element *xml;
	char *s;
	int code;

	/* External resource dictionaries MUST NOT reference other resource dictionaries */
	xps_absolute_path(part_name, base_uri, source_att, sizeof part_name);
	part = xps_read_part(ctx, part_name);
	if (!part)
	{
		return fz_throw("cannot find remote resource part '%s'", part_name);
	}

	xml = xml_parse_document(part->data, part->size);
	if (!xml)
	{
		xps_free_part(ctx, part);
		return fz_rethrow(-1, "cannot parse xml");
	}

	if (strcmp(xml_tag(xml), "ResourceDictionary"))
	{
		xml_free_element(xml);
		xps_free_part(ctx, part);
		return fz_throw("expected ResourceDictionary element (found %s)", xml_tag(xml));
	}

	fz_strlcpy(part_uri, part_name, sizeof part_uri);
	s = strrchr(part_uri, '/');
	if (s)
		s[1] = 0;

	code = xps_parse_resource_dictionary(ctx, &dict, part_uri, xml);
	if (code)
	{
		xml_free_element(xml);
		xps_free_part(ctx, part);
		return fz_rethrow(code, "cannot parse remote resource dictionary: %s", part_uri);
	}

	dict->base_xml = xml; /* pass on ownership */

	xps_free_part(ctx, part);

	*dictp = dict;
	return fz_okay;
}

int
xps_parse_resource_dictionary(xps_context *ctx, xps_resource **dictp, char *base_uri, xml_element *root)
{
	xps_resource *head;
	xps_resource *entry;
	xml_element *node;
	char *source;
	char *key;
	int code;

	source = xml_att(root, "Source");
	if (source)
	{
		code = xps_parse_remote_resource_dictionary(ctx, dictp, base_uri, source);
		if (code)
			return fz_rethrow(code, "cannot parse remote resource dictionary");
		return fz_okay;
	}

	head = NULL;

	for (node = xml_down(root); node; node = xml_next(node))
	{
		key = xml_att(node, "x:Key");
		if (key)
		{
			entry = fz_malloc(sizeof(xps_resource));
			entry->name = key;
			entry->base_uri = NULL;
			entry->base_xml = NULL;
			entry->data = node;
			entry->next = head;
			entry->parent = NULL;
			head = entry;
		}
	}

	if (head)
		head->base_uri = fz_strdup(base_uri);
	else
		return fz_throw("empty resource dictionary");

	*dictp = head;
	return fz_okay;
}

void
xps_free_resource_dictionary(xps_context *ctx, xps_resource *dict)
{
	xps_resource *next;
	while (dict)
	{
		next = dict->next;
		if (dict->base_xml)
			xml_free_element(dict->base_xml);
		if (dict->base_uri)
			fz_free(dict->base_uri);
		fz_free(dict);
		dict = next;
	}
}

void
xps_debug_resource_dictionary(xps_resource *dict)
{
	while (dict)
	{
		if (dict->base_uri)
			printf("URI = '%s'\n", dict->base_uri);
		printf("KEY = '%s' VAL = %p\n", dict->name, dict->data);
		if (dict->parent)
		{
			printf("PARENT = {\n");
			xps_debug_resource_dictionary(dict->parent);
			printf("}\n");
		}
		dict = dict->next;
	}
}
