#include "muxps-internal.h"

static xml_element *
xps_find_resource(xps_document *doc, xps_resource *dict, char *name, char **urip)
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
xps_parse_resource_reference(xps_document *doc, xps_resource *dict, char *att, char **urip)
{
	char name[1024];
	char *s;

	if (strstr(att, "{StaticResource ") != att)
		return NULL;

	fz_strlcpy(name, att + 16, sizeof name);
	s = strrchr(name, '}');
	if (s)
		*s = 0;

	return xps_find_resource(doc, dict, name, urip);
}

void
xps_resolve_resource_reference(xps_document *doc, xps_resource *dict,
		char **attp, xml_element **tagp, char **urip)
{
	if (*attp)
	{
		xml_element *rsrc = xps_parse_resource_reference(doc, dict, *attp, urip);
		if (rsrc)
		{
			*attp = NULL;
			*tagp = rsrc;
		}
	}
}

static xps_resource *
xps_parse_remote_resource_dictionary(xps_document *doc, char *base_uri, char *source_att)
{
	char part_name[1024];
	char part_uri[1024];
	xps_resource *dict;
	xps_part *part;
	xml_element *xml;
	char *s;

	/* External resource dictionaries MUST NOT reference other resource dictionaries */
	xps_resolve_url(part_name, base_uri, source_att, sizeof part_name);
	part = xps_read_part(doc, part_name);
	xml = xml_parse_document(doc->ctx, part->data, part->size);
	xps_free_part(doc, part);

	if (!xml)
		return NULL;

	if (strcmp(xml_tag(xml), "ResourceDictionary"))
	{
		xml_free_element(doc->ctx, xml);
		fz_throw(doc->ctx, "expected ResourceDictionary element");
	}

	fz_strlcpy(part_uri, part_name, sizeof part_uri);
	s = strrchr(part_uri, '/');
	if (s)
		s[1] = 0;

	dict = xps_parse_resource_dictionary(doc, part_uri, xml);
	if (dict)
		dict->base_xml = xml; /* pass on ownership */

	return dict;
}

xps_resource *
xps_parse_resource_dictionary(xps_document *doc, char *base_uri, xml_element *root)
{
	xps_resource *head;
	xps_resource *entry;
	xml_element *node;
	char *source;
	char *key;

	source = xml_att(root, "Source");
	if (source)
		return xps_parse_remote_resource_dictionary(doc, base_uri, source);

	head = NULL;

	for (node = xml_down(root); node; node = xml_next(node))
	{
		key = xml_att(node, "x:Key");
		if (key)
		{
			entry = fz_malloc_struct(doc->ctx, xps_resource);
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
		head->base_uri = fz_strdup(doc->ctx, base_uri);

	return head;
}

void
xps_free_resource_dictionary(xps_document *doc, xps_resource *dict)
{
	xps_resource *next;
	while (dict)
	{
		next = dict->next;
		if (dict->base_xml)
			xml_free_element(doc->ctx, dict->base_xml);
		if (dict->base_uri)
			fz_free(doc->ctx, dict->base_uri);
		fz_free(doc->ctx, dict);
		dict = next;
	}
}

void
xps_print_resource_dictionary(xps_resource *dict)
{
	while (dict)
	{
		if (dict->base_uri)
			printf("URI = '%s'\n", dict->base_uri);
		printf("KEY = '%s' VAL = %p\n", dict->name, dict->data);
		if (dict->parent)
		{
			printf("PARENT = {\n");
			xps_print_resource_dictionary(dict->parent);
			printf("}\n");
		}
		dict = dict->next;
	}
}
