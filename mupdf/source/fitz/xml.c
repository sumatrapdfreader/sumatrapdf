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

#include "xml-imp.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gumbo.h>

#define FZ_XML_MAX_DEPTH 4096

/* #define FZ_XML_SEQ */

static const struct { const char *name; int c; } html_entities[] = {
	{"nbsp",160}, {"iexcl",161}, {"cent",162}, {"pound",163},
	{"curren",164}, {"yen",165}, {"brvbar",166}, {"sect",167},
	{"uml",168}, {"copy",169}, {"ordf",170}, {"laquo",171},
	{"not",172}, {"shy",173}, {"reg",174}, {"macr",175}, {"deg",176},
	{"plusmn",177}, {"sup2",178}, {"sup3",179}, {"acute",180},
	{"micro",181}, {"para",182}, {"middot",183}, {"cedil",184},
	{"sup1",185}, {"ordm",186}, {"raquo",187}, {"frac14",188},
	{"frac12",189}, {"frac34",190}, {"iquest",191}, {"Agrave",192},
	{"Aacute",193}, {"Acirc",194}, {"Atilde",195}, {"Auml",196},
	{"Aring",197}, {"AElig",198}, {"Ccedil",199}, {"Egrave",200},
	{"Eacute",201}, {"Ecirc",202}, {"Euml",203}, {"Igrave",204},
	{"Iacute",205}, {"Icirc",206}, {"Iuml",207}, {"ETH",208},
	{"Ntilde",209}, {"Ograve",210}, {"Oacute",211}, {"Ocirc",212},
	{"Otilde",213}, {"Ouml",214}, {"times",215}, {"Oslash",216},
	{"Ugrave",217}, {"Uacute",218}, {"Ucirc",219}, {"Uuml",220},
	{"Yacute",221}, {"THORN",222}, {"szlig",223}, {"agrave",224},
	{"aacute",225}, {"acirc",226}, {"atilde",227}, {"auml",228},
	{"aring",229}, {"aelig",230}, {"ccedil",231}, {"egrave",232},
	{"eacute",233}, {"ecirc",234}, {"euml",235}, {"igrave",236},
	{"iacute",237}, {"icirc",238}, {"iuml",239}, {"eth",240},
	{"ntilde",241}, {"ograve",242}, {"oacute",243}, {"ocirc",244},
	{"otilde",245}, {"ouml",246}, {"divide",247}, {"oslash",248},
	{"ugrave",249}, {"uacute",250}, {"ucirc",251}, {"uuml",252},
	{"yacute",253}, {"thorn",254}, {"yuml",255}, {"lt",60}, {"gt",62},
	{"amp",38}, {"apos",39}, {"quot",34}, {"OElig",338}, {"oelig",339},
	{"Scaron",352}, {"scaron",353}, {"Yuml",376}, {"circ",710},
	{"tilde",732}, {"ensp",8194}, {"emsp",8195}, {"thinsp",8201},
	{"zwnj",8204}, {"zwj",8205}, {"lrm",8206}, {"rlm",8207},
	{"ndash",8211}, {"mdash",8212}, {"lsquo",8216}, {"rsquo",8217},
	{"sbquo",8218}, {"ldquo",8220}, {"rdquo",8221}, {"bdquo",8222},
	{"dagger",8224}, {"Dagger",8225}, {"permil",8240}, {"lsaquo",8249},
	{"rsaquo",8250}, {"euro",8364}, {"fnof",402}, {"Alpha",913},
	{"Beta",914}, {"Gamma",915}, {"Delta",916}, {"Epsilon",917},
	{"Zeta",918}, {"Eta",919}, {"Theta",920}, {"Iota",921}, {"Kappa",922},
	{"Lambda",923}, {"Mu",924}, {"Nu",925}, {"Xi",926}, {"Omicron",927},
	{"Pi",928}, {"Rho",929}, {"Sigma",931}, {"Tau",932}, {"Upsilon",933},
	{"Phi",934}, {"Chi",935}, {"Psi",936}, {"Omega",937}, {"alpha",945},
	{"beta",946}, {"gamma",947}, {"delta",948}, {"epsilon",949},
	{"zeta",950}, {"eta",951}, {"theta",952}, {"iota",953}, {"kappa",954},
	{"lambda",955}, {"mu",956}, {"nu",957}, {"xi",958}, {"omicron",959},
	{"pi",960}, {"rho",961}, {"sigmaf",962}, {"sigma",963}, {"tau",964},
	{"upsilon",965}, {"phi",966}, {"chi",967}, {"psi",968}, {"omega",969},
	{"thetasym",977}, {"upsih",978}, {"piv",982}, {"bull",8226},
	{"hellip",8230}, {"prime",8242}, {"Prime",8243}, {"oline",8254},
	{"frasl",8260}, {"weierp",8472}, {"image",8465}, {"real",8476},
	{"trade",8482}, {"alefsym",8501}, {"larr",8592}, {"uarr",8593},
	{"rarr",8594}, {"darr",8595}, {"harr",8596}, {"crarr",8629},
	{"lArr",8656}, {"uArr",8657}, {"rArr",8658}, {"dArr",8659},
	{"hArr",8660}, {"forall",8704}, {"part",8706}, {"exist",8707},
	{"empty",8709}, {"nabla",8711}, {"isin",8712}, {"notin",8713},
	{"ni",8715}, {"prod",8719}, {"sum",8721}, {"minus",8722},
	{"lowast",8727}, {"radic",8730}, {"prop",8733}, {"infin",8734},
	{"ang",8736}, {"and",8743}, {"or",8744}, {"cap",8745}, {"cup",8746},
	{"int",8747}, {"there4",8756}, {"sim",8764}, {"cong",8773},
	{"asymp",8776}, {"ne",8800}, {"equiv",8801}, {"le",8804}, {"ge",8805},
	{"sub",8834}, {"sup",8835}, {"nsub",8836}, {"sube",8838},
	{"supe",8839}, {"oplus",8853}, {"otimes",8855}, {"perp",8869},
	{"sdot",8901}, {"lceil",8968}, {"rceil",8969}, {"lfloor",8970},
	{"rfloor",8971}, {"lang",9001}, {"rang",9002}, {"loz",9674},
	{"spades",9824}, {"clubs",9827}, {"hearts",9829}, {"diams",9830},
};

struct parser
{
	fz_pool *pool;
	fz_xml *head;
	int preserve_white;
	int depth;
#ifdef FZ_XML_SEQ
	int seq;
#endif
};

static void xml_indent(fz_context *ctx, fz_output *out, int n)
{
	while (n--) {
		fz_write_byte(ctx, out, ' ');
		fz_write_byte(ctx, out, ' ');
	}
}

void fz_debug_xml(fz_xml *item, int level)
{
	/* This is a bit nasty as it relies on implementation
	 * details of both fz_stdout, and fz_write_printf coping
	 * with NULL ctx. */
	fz_output_xml(NULL, fz_stdout(NULL), item, level);
}

void fz_output_xml(fz_context *ctx, fz_output *out, fz_xml *item, int level)
{
	char *s;

	if (item == NULL)
		return;

	/* Skip over the DOC object at the top. */
	if (item->up == NULL)
	{
		fz_output_xml(ctx, out, item->down, level);
		return;
	}

	s = fz_xml_text(item);
	xml_indent(ctx, out, level);
	if (s)
	{
		int c;
		fz_write_byte(ctx, out, '"');
		while (*s) {
			s += fz_chartorune(&c, s);
			switch (c) {
			default:
				if (c > 0xFFFF)
					fz_write_printf(ctx, out, "\\u{%X}", c);
				else if (c < 32 || c > 127)
					fz_write_printf(ctx, out, "\\u%04X", c);
				else
					fz_write_byte(ctx, out, c);
				break;
			case '\\': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, '\\'); break;
			case '\b': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, 'b'); break;
			case '\f': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, 'f'); break;
			case '\n': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, 'n'); break;
			case '\r': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, 'r'); break;
			case '\t': fz_write_byte(ctx, out, '\\'); fz_write_byte(ctx, out, 't'); break;
			}
		}
		fz_write_byte(ctx, out, '"');
#ifdef FZ_XML_SEQ
		fz_write_printf(ctx, out, " <%d>", item->seq);
#endif
		fz_write_byte(ctx, out, '\n');
	}
	else
	{
		fz_xml *child;
		struct attribute *att;

#ifdef FZ_XML_SEQ
		fz_write_printf(ctx, out, "(%s <%d>\n", item->u.node.u.d.name, item->u.node.seq);
#else
		fz_write_printf(ctx, out, "(%s\n", item->u.node.u.d.name);
#endif
		for (att = item->u.node.u.d.atts; att; att = att->next)
		{
			xml_indent(ctx, out, level);
			fz_write_printf(ctx, out, "=%s %s\n", att->name, att->value);
		}
		for (child = fz_xml_down(item); child; child = child->u.node.next)
			fz_output_xml(ctx, out, child, level + 1);
		xml_indent(ctx, out, level);
#ifdef FZ_XML_SEQ
		fz_write_printf(ctx, out, ")%s <%d>\n", item->u.node.u.d.name, item->u.node.seq);
#else
		fz_write_printf(ctx, out, ")%s\n", item->u.node.u.d.name);
#endif
	}
}

fz_xml *fz_xml_prev(fz_xml *item)
{
	return item && item->up ? item->u.node.prev : NULL;
}

fz_xml *fz_xml_next(fz_xml *item)
{
	return item && item->up ? item->u.node.next : NULL;
}

fz_xml *fz_xml_up(fz_xml *item)
{
	/* Never step up to the DOC. */
	return item && item->up && item->up->up ? item->up : NULL;
}

fz_xml *fz_xml_down(fz_xml *item)
{
	/* DOC items can never have MAGIC_TEXT as their down value,
	 * so this is safe. */
	return item && !FZ_TEXT_ITEM(item) ? item->down : NULL;
}

char *fz_xml_text(fz_xml *item)
{
	/* DOC items can never have MAGIC_TEXT as their down value,
	 * so this is safe. */
	return (item && FZ_TEXT_ITEM(item)) ? item->u.node.u.text : NULL;
}

char *fz_xml_tag(fz_xml *item)
{
	/* DOC items can never have MAGIC_TEXT as their down value,
	 * so this is safe. */
	return item && !FZ_TEXT_ITEM(item) ? item->u.node.u.d.name : NULL;
}

int fz_xml_is_tag(fz_xml *item, const char *name)
{
	if (!item || FZ_DOCUMENT_ITEM(item) || FZ_TEXT_ITEM(item))
		return 0;
	return !strcmp(item->u.node.u.d.name, name);
}

char *fz_xml_att(fz_xml *item, const char *name)
{
	struct attribute *att;
	if (!item || FZ_DOCUMENT_ITEM(item) || FZ_TEXT_ITEM(item))
		return NULL;
	for (att = item->u.node.u.d.atts; att; att = att->next)
		if (!strcmp(att->name, name))
			return att->value;
	return NULL;
}

char *fz_xml_att_alt(fz_xml *item, const char *one, const char *two)
{
	char *val = fz_xml_att(item, one);
	if (!val)
		val = fz_xml_att(item, two);
	return val;
}

fz_xml *fz_xml_find(fz_xml *item, const char *tag)
{
	/* Skip over any DOC item. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	while (item)
	{
		if (!FZ_TEXT_ITEM(item) && !strcmp(item->u.node.u.d.name, tag))
			return item;
		item = item->u.node.next;
	}
	return NULL;
}

fz_xml *fz_xml_find_next(fz_xml *item, const char *tag)
{
	/* Skip over any DOC item. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	if (item)
		item = item->u.node.next;
	return fz_xml_find(item, tag);
}

fz_xml *fz_xml_find_down(fz_xml *item, const char *tag)
{
	if (item)
		item = fz_xml_down(item);
	return fz_xml_find(item, tag);
}

int fz_xml_att_eq(fz_xml *item, const char *name, const char *match)
{
	const char *val = fz_xml_att(item, name);

	return val ? !strcmp(val, match) : 0;
}

fz_xml *fz_xml_find_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	/* Skip over any document item. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	while (1)
	{
		item = tag ? fz_xml_find(item, tag) : item;
		if (item == NULL || fz_xml_att_eq(item, att, match))
			break;
		item = item->u.node.next;
	}

	return item;
}

fz_xml *fz_xml_find_next_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	/* Skip over any document item. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	do
	{
		item = tag ? fz_xml_find_next(item, tag) : item->u.node.next;
	}
	while (item != NULL && !fz_xml_att_eq(item, att, match));

	return item;
}

fz_xml *fz_xml_find_down_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	return fz_xml_find_match(fz_xml_down(item), tag, att, match);
}

fz_xml *fz_xml_root(fz_xml *xml)
{
	if (xml == NULL)
		return NULL;

	/* If we've been given a node mid-tree, run up to the root to find
	 * the doc node. */
	while (xml->up)
		xml = xml->up;

	/* And the root is the child of the doc.*/
	return xml->down;
}

void fz_drop_xml(fz_context *ctx, fz_xml *xml)
{
	if (!xml)
		return;

	/* Whereever we are in the tree, we want the doc node at the root. */
	while (xml->up)
		xml = xml->up;

	/* Drop a reference to the tree as a whole. */
	if (fz_drop_imp(ctx, xml, &xml->u.doc.refs) == 0)
		return;

	fz_drop_pool(ctx, xml->u.doc.pool);
}

void fz_detach_xml(fz_context *ctx, fz_xml *node)
{
	fz_xml *doc = node;

	/* If we're already a document node, then this is a NOP. */
	if (doc->up == NULL)
		return;

	/* Move doc to be the doc pointer at the top of the tree. */
	while (doc->up)
	{
		doc = doc->up;
	}

	/* Relocate node to be the child of doc. */
	node->up->down = NULL;
	doc->down = node;

	/* NOTE: Suppose that X = doc->down on entry. On exit doc->down == node, but
	 * X->up = doc. We need to be careful throughout this code to not assume that
	 * Y is always a child of Y->up. */
}

size_t xml_parse_entity(int *c, const char *a)
{
	char *b;
	size_t i;

	if (a[1] == '#') {
		if (a[2] == 'x')
			*c = strtol(a + 3, &b, 16);
		else
			*c = strtol(a + 2, &b, 10);
		if (*b == ';')
			return b - a + 1;
	}
	else if (a[1] == 'l' && a[2] == 't' && a[3] == ';') {
		*c = '<';
		return 4;
	}
	else if (a[1] == 'g' && a[2] == 't' && a[3] == ';') {
		*c = '>';
		return 4;
	}
	else if (a[1] == 'a' && a[2] == 'm' && a[3] == 'p' && a[4] == ';') {
		*c = '&';
		return 5;
	}
	else if (a[1] == 'a' && a[2] == 'p' && a[3] == 'o' && a[4] == 's' && a[5] == ';') {
		*c = '\'';
		return 6;
	}
	else if (a[1] == 'q' && a[2] == 'u' && a[3] == 'o' && a[4] == 't' && a[5] == ';') {
		*c = '"';
		return 6;
	}

	/* We should only be doing this for XHTML, but it shouldn't be a problem. */
	for (i = 0; i < nelem(html_entities); ++i) {
		size_t n = strlen(html_entities[i].name);
		if (!strncmp(a+1, html_entities[i].name, n) && a[n+1] == ';') {
			*c = html_entities[i].c;
			return n + 2;
		}
	}

	*c = *a;
	return 1;
}

static inline int isname(int c)
{
	return c == '.' || c == '-' || c == '_' || c == ':' ||
		(c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z');
}

static inline int iswhite(int c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static void xml_emit_open_tag(fz_context *ctx, struct parser *parser, const char *a, const char *b, int is_text)
{
	fz_xml *head, *tail;
	const char *ns;
	size_t size;

	if (is_text)
		size = offsetof(fz_xml, u.node.u.text) + b-a+1;
	else
	{
		/* skip namespace prefix */
		for (ns = a; ns < b - 1; ++ns)
			if (*ns == ':')
				a = ns + 1;

		size = offsetof(fz_xml, u.node.u.d.name) + b-a+1;
	}
	head = fz_pool_alloc(ctx, parser->pool, size);

	if (is_text)
		head->down = MAGIC_TEXT;
	else
	{
		memcpy(head->u.node.u.d.name, a, b - a);
		head->u.node.u.d.name[b - a] = 0;
		head->u.node.u.d.atts = NULL;
		head->down = NULL;
	}

	head->up = parser->head;
	head->u.node.next = NULL;
#ifdef FZ_XML_SEQ
	head->u.node.seq = parser->seq++;
#endif

	/* During construction, we use head->next to mean "the
	 * tail of the children. When we close the tag, we
	 * rewrite it to be NULL. */
	if (!parser->head->down) {
		parser->head->down = head;
		parser->head->u.node.next = head;
		head->u.node.prev = NULL;
	}
	else {
		tail = parser->head->u.node.next;
		tail->u.node.next = head;
		head->u.node.prev = tail;
		parser->head->u.node.next = head;
	}

	parser->head = head;
	parser->depth++;
	if (parser->depth >= FZ_XML_MAX_DEPTH)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "too deep xml element nesting");
}

static void xml_emit_att_name(fz_context *ctx, struct parser *parser, const char *a, const char *b)
{
	fz_xml *head = parser->head;
	struct attribute *att;
	size_t size;

	size = offsetof(struct attribute, name) + b-a+1;
	att = fz_pool_alloc(ctx, parser->pool, size);
	memcpy(att->name, a, b - a);
	att->name[b - a] = 0;
	att->value = NULL;
	att->next = head->u.node.u.d.atts;
	head->u.node.u.d.atts = att;
}

void fz_xml_add_att(fz_context *ctx, fz_pool *pool, fz_xml *node, const char *key, const char *val)
{
	size_t size = offsetof(struct attribute, name) + strlen(key) + 1;
	struct attribute *att = fz_pool_alloc(ctx, pool, size);
	memcpy(att->name, key, strlen(key)+1);
	att->value = fz_pool_alloc(ctx, pool, strlen(val) + 1);
	memcpy(att->value, val, strlen(val)+1);
	att->next = node->u.node.u.d.atts;
	node->u.node.u.d.atts = att;
}

static void xml_emit_att_value(fz_context *ctx, struct parser *parser, const char *a, const char *b)
{
	fz_xml *head = parser->head;
	struct attribute *att = head->u.node.u.d.atts;
	char *s;
	int c;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = att->value = fz_pool_alloc(ctx, parser->pool, b - a + 1);
	while (a < b) {
		if (*a == '&') {
			a += xml_parse_entity(&c, a);
			s += fz_runetochar(s, c);
		}
		else {
			*s++ = *a++;
		}
	}
	*s = 0;
}

static void xml_emit_close_tag(fz_context *ctx, struct parser *parser)
{
	parser->depth--;
	parser->head->u.node.next = NULL;
	if (parser->head->up)
		parser->head = parser->head->up;
}

static void xml_emit_text(fz_context *ctx, struct parser *parser, const char *a, const char *b)
{
	fz_xml *head;
	const char *p;
	char *s;
	int c;

	/* Skip text outside the root tag */
	if (parser->depth == 0)
		return;

	/* Skip all-whitespace text nodes */
	if (!parser->preserve_white)
	{
		for (p = a; p < b; p++)
			if (!iswhite(*p))
				break;
		if (p == b)
			return;
	}

	xml_emit_open_tag(ctx, parser, a, b, 1);
	head = parser->head;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = fz_xml_text(head);
	while (a < b) {
		if (*a == '&') {
			a += xml_parse_entity(&c, a);
			s += fz_runetochar(s, c);
		}
		else {
			*s++ = *a++;
		}
	}
	*s = 0;

	xml_emit_close_tag(ctx, parser);
}

static void xml_emit_cdata(fz_context *ctx, struct parser *parser, const char *a, const char *b)
{
	fz_xml *head;
	char *s;

	xml_emit_open_tag(ctx, parser, a, b, 1);
	head = parser->head;

	s = head->u.node.u.text;
	while (a < b)
		*s++ = *a++;
	*s = 0;

	xml_emit_close_tag(ctx, parser);
}

static int close_tag(fz_context *ctx, struct parser *parser, const char *mark, const char *p)
{
	const char *ns, *tag;

	/* skip namespace prefix */
	for (ns = mark; ns < p - 1; ++ns)
		if (*ns == ':')
			mark = ns + 1;

	tag = fz_xml_tag(parser->head);
	if (tag && strncmp(tag, mark, p-mark) == 0 && tag[p-mark] == 0)
	{
		xml_emit_close_tag(ctx, parser);
		return 0;
	}
	return 1;
}

static char *xml_parse_document_imp(fz_context *ctx, struct parser *parser, const char *p) /* lgtm [cpp/use-of-goto] */
{
	const char *mark;
	int quote;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	if (*p == '<') {
		if (mark < p)
			xml_emit_text(ctx, parser, mark, p);
		++p;
		goto parse_element;
	} else if (mark < p)
		xml_emit_text(ctx, parser, mark, p);
	return NULL;

parse_element:
	if (*p == '/') { ++p; goto parse_closing_element; }
	if (*p == '!') { ++p; goto parse_comment; }
	if (*p == '?') { ++p; goto parse_processing_instruction; }
	while (iswhite(*p)) ++p;
	if (isname(*p))
		goto parse_element_name;
	return "syntax error in element";

parse_comment:
	if (p[0]=='D' && p[1]=='O' && p[2]=='C' && p[3]=='T' && p[4]=='Y' && p[5]=='P' && p[6]=='E')
		goto parse_declaration;
	if (p[0]=='E' && p[1]=='N' && p[2]=='T' && p[3]=='I' && p[4]=='T' && p[5]=='Y')
		goto parse_declaration;
	if (*p == '[') goto parse_cdata;
	if (*p++ != '-') return "syntax error in comment (<! not followed by --)";
	if (*p++ != '-') return "syntax error in comment (<!- not followed by -)";
	while (*p) {
		if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in comment";

parse_declaration:
	while (*p) if (*p++ == '>') goto parse_text;
	return "end of data in declaration";

parse_cdata:
	if (p[1] != 'C' || p[2] != 'D' || p[3] != 'A' || p[4] != 'T' || p[5] != 'A' || p[6] != '[')
		return "syntax error in CDATA section";
	p += 7;
	mark = p;
	while (*p) {
		if (p[0] == ']' && p[1] == ']' && p[2] == '>') {
			xml_emit_cdata(ctx, parser, mark, p);
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in CDATA section";

parse_processing_instruction:
	while (*p) {
		if (p[0] == '?' && p[1] == '>') {
			p += 2;
			goto parse_text;
		}
		++p;
	}
	return "end of data in processing instruction";

parse_closing_element:
	while (iswhite(*p)) ++p;
	mark = p;
	while (isname(*p)) ++p;
	if (!isname(*mark))
		return "syntax error in closing element";
	if (close_tag(ctx, parser, mark, p))
		return "opening and closing tag mismatch";
	while (iswhite(*p)) ++p;
	if (*p != '>')
		return "syntax error in closing element";
	++p;
	goto parse_text;

parse_element_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_open_tag(ctx, parser, mark, p, 0);
	if (*p == '>') {
		++p;
		goto parse_text;
	}
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag(ctx, parser);
		p += 2;
		goto parse_text;
	}
	if (iswhite(*p))
		goto parse_attributes;
	return "syntax error after element name";

parse_attributes:
	while (iswhite(*p)) ++p;
	if (isname(*p))
		goto parse_attribute_name;
	if (*p == '>') {
		++p;
		goto parse_text;
	}
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag(ctx, parser);
		p += 2;
		goto parse_text;
	}
	return "syntax error in attributes";

parse_attribute_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_att_name(ctx, parser, mark, p);
	while (iswhite(*p)) ++p;
	if (*p == '=') { ++p; goto parse_attribute_value; }
	return "syntax error after attribute name";

parse_attribute_value:
	while (iswhite(*p)) ++p;
	quote = *p++;
	mark = p;

	/* special case for handling MOBI filepos=00000 syntax */
	if (quote >= '0' && quote <= '9') {
		while (*p >= '0' && *p <= '9') ++p;
		xml_emit_att_value(ctx, parser, mark, p);
		goto parse_attributes;
	}

	if (quote != '"' && quote != '\'')
		return "missing quote character";
	while (*p && *p != quote) ++p;
	if (*p == quote) {
		xml_emit_att_value(ctx, parser, mark, p++);
		goto parse_attributes;
	}
	return "end of data in attribute value";
}

static int fast_tolower(int c)
{
	if ((unsigned)c - 'A' < 26)
		return c | 32;
	return c;
}

static int fast_strncasecmp(const char *a, const char *b, size_t n)
{
	if (!n--)
		return 0;
	for (; *a && *b && n && fast_tolower(*a) == fast_tolower(*b); a++, b++, n--)
		;
	return fast_tolower(*a) - fast_tolower(*b);
}

static char *fast_strcasestr(char *h, char *n)
{
	int n0 = fast_tolower(*n++);
	size_t nn = strlen(n);
	while (*h != 0)
	{
		if (fast_tolower(*h) == n0 && fast_strncasecmp(h+1, n, nn) == 0)
			return h;
		++h;
	}
	return NULL;
}

static int startswith(const char *a, const char *b)
{
	return !fast_strncasecmp(a, b, strlen(b));
}

// Look for encoding in <meta http-equiv="content-type" content="text/html; charset=XXX"> tags
static const unsigned short *find_meta_encoding(char *s)
{
	const unsigned short *table = NULL;
	char *end, *meta;

	meta = fast_strcasestr(s, "<meta");
	while (meta && !table)
	{
		end = strchr(meta, '>');
		if (end)
		{
			*end = 0;
			if (fast_strcasestr(meta, "http-equiv") && fast_strcasestr(meta, "content-type"))
			{
				char *charset = fast_strcasestr(meta, "charset=");
				if (charset)
				{
					char *enc = charset + 8;
					if (startswith(enc, "iso-8859-1") || startswith(enc, "latin1"))
						table = fz_unicode_from_iso8859_1;
					else if (startswith(enc, "iso-8859-7") || startswith(enc, "greek"))
						table = fz_unicode_from_iso8859_7;
					else if (startswith(enc, "koi8"))
						table = fz_unicode_from_koi8u;
					else if (startswith(enc, "windows-1250"))
						table = fz_unicode_from_windows_1250;
					else if (startswith(enc, "windows-1251"))
						table = fz_unicode_from_windows_1251;
					else if (startswith(enc, "windows-1252"))
						table = fz_unicode_from_windows_1252;
				}
			}
			*end = '>';
		}
		meta = fast_strcasestr(meta + 5, "<meta");
	}

	return table;
}

static const unsigned short *find_xml_encoding(char *s)
{
	const unsigned short *table = NULL;
	char *end, *xml, *enc;

	end = strchr(s, '>');
	if (end)
	{
		*end = 0;
		xml = strstr(s, "<?xml");
		if (xml)
		{
			enc = strstr(xml, "encoding=");
			if (enc)
			{
				enc += 10;
				if (startswith(enc, "iso-8859-1") || startswith(enc, "latin1"))
					table = fz_unicode_from_iso8859_1;
				else if (startswith(enc, "iso-8859-7") || startswith(enc, "greek"))
					table = fz_unicode_from_iso8859_7;
				else if (startswith(enc, "koi8"))
					table = fz_unicode_from_koi8u;
				else if (startswith(enc, "windows-1250"))
					table = fz_unicode_from_windows_1250;
				else if (startswith(enc, "windows-1251"))
					table = fz_unicode_from_windows_1251;
				else if (startswith(enc, "windows-1252"))
					table = fz_unicode_from_windows_1252;
			}
		}
		*end = '>';
	}

	if (!table)
		table = find_meta_encoding(s);

	return table;
}

static char *convert_to_utf8(fz_context *ctx, unsigned char *s, size_t n, int *dofree)
{
	const unsigned short *table;
	const unsigned char *e = s + n;
	char *dst, *d;
	int c;

	if (s[0] == 0xFE && s[1] == 0xFF) {
		s += 2;
		dst = d = Memento_label(fz_malloc(ctx, n * FZ_UTFMAX), "utf8_from_be");
		while (s + 1 < e) {
			c = s[0] << 8 | s[1];
			d += fz_runetochar(d, c);
			s += 2;
		}
		*d = 0;
		*dofree = 1;
		return dst;
	}

	if (s[0] == 0xFF && s[1] == 0xFE) {
		s += 2;
		dst = d = Memento_label(fz_malloc(ctx, n * FZ_UTFMAX), "utf8_from_le");
		while (s + 1 < e) {
			c = s[0] | s[1] << 8;
			d += fz_runetochar(d, c);
			s += 2;
		}
		*d = 0;
		*dofree = 1;
		return dst;
	}

	table = find_xml_encoding((char*)s);
	if (table) {
		dst = d = Memento_label(fz_malloc(ctx, n * FZ_UTFMAX), "utf8");
		while (*s) {
			c = table[*s++];
			d += fz_runetochar(d, c);
		}
		*d = 0;
		*dofree = 1;
		return dst;
	}

	*dofree = 0;

	if (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF)
		return (char*)s+3;

	return (char*)s;
}

fz_xml *
fz_parse_xml_stream(fz_context *ctx, fz_stream *stm, int preserve_white)
{
	fz_buffer *buf = fz_read_all(ctx, stm, 128);
	fz_xml *xml = NULL;

	fz_var(xml);

	fz_try(ctx)
		xml = fz_parse_xml(ctx, buf, preserve_white);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return xml;
}

static fz_xml *
parse_and_drop_buffer(fz_context *ctx, fz_buffer *buf, int preserve_white)
{
	fz_xml *xml = NULL;

	fz_var(xml);

	fz_try(ctx)
		xml = fz_parse_xml(ctx, buf, preserve_white);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return xml;
}

fz_xml *
fz_parse_xml_archive_entry(fz_context *ctx, fz_archive *arch, const char *filename, int preserve_white)
{
	fz_buffer *buf = fz_read_archive_entry(ctx, arch, filename);

	return parse_and_drop_buffer(ctx, buf, preserve_white);
}

fz_xml *
fz_try_parse_xml_archive_entry(fz_context *ctx, fz_archive *arch, const char *filename, int preserve_white)
{
	fz_buffer *buf = fz_try_read_archive_entry(ctx, arch, filename);

	if (buf == NULL)
		return NULL;

	return parse_and_drop_buffer(ctx, buf, preserve_white);
}

fz_xml *
fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white)
{
	struct parser parser;
	fz_xml *xml = NULL;
	fz_xml root, *node;
	char *p = NULL;
	char *error;
	int dofree = 0;
	unsigned char *s;
	size_t n;
	static unsigned char empty_string[] = "";

	fz_var(dofree);
	fz_var(p);

	if (buf == NULL)
	{
		n = 0;
		s = empty_string;
	}
	else
	{
		/* ensure we are zero-terminated */
		fz_terminate_buffer(ctx, buf);
		n = fz_buffer_storage(ctx, buf, &s);
	}

	memset(&root, 0, sizeof(root));
	parser.pool = fz_new_pool(ctx);
	parser.head = &root;
	parser.preserve_white = preserve_white;
	parser.depth = 0;
#ifdef FZ_XML_SEQ
	parser.seq = 0;
#endif

	fz_try(ctx)
	{
		p = convert_to_utf8(ctx, s, n, &dofree);

		error = xml_parse_document_imp(ctx, &parser, p);
		if (error)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "%s", error);

		for (node = parser.head; node; node = node->up)
			node->u.node.next = NULL;

		xml = fz_pool_alloc(ctx, parser.pool, sizeof *xml);
		xml->up = NULL;
		xml->down = root.down;
		xml->u.doc.refs = 1;
		xml->u.doc.pool = parser.pool;

		for (node = root.down; node; node = node->u.node.next)
			node->up = xml;
	}
	fz_always(ctx)
	{
		if (dofree)
			fz_free(ctx, p);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, parser.pool);
		fz_rethrow(ctx);
	}

	return xml;
}

/*
	Parse the contents of buffer into a tree of XML nodes, using the HTML5 syntax.

	Gumbo doesn't check for malloc errors. Use our pool allocator and let it longjmp
	out of Gumbo on allocation errors. At the end (success or fail) we release the
	pool used for Gumbo's parse tree all at once.
*/

struct mem_gumbo {
	fz_context *ctx;
	fz_pool *pool;
};

static void *alloc_gumbo(void *ctx, size_t size)
{
	struct mem_gumbo *mem = ctx;
	return fz_pool_alloc(mem->ctx, mem->pool, size);
}

static void dealloc_gumbo(void *ctx, void *ptr)
{
	/* nothing */
}

static void xml_from_gumbo(fz_context *ctx, struct parser *parser, GumboNode *node)
{
	unsigned int i;
	const char *tag, *end, *sentinel;

	switch (node->type)
	{
	case GUMBO_NODE_ELEMENT:
		if (node->v.element.tag != GUMBO_TAG_UNKNOWN)
		{
			tag = gumbo_normalized_tagname(node->v.element.tag);
			end = tag + strlen(tag);
		}
		else
		{
			tag = node->v.element.original_tag.data;
			sentinel = tag + node->v.element.original_tag.length;
			if (tag[0] == '<')
				++tag;
			for (end = tag; end < sentinel; ++end)
				if (end[0] == '>' || end[0] == '/' || iswhite(end[0]))
					break;
		}
		xml_emit_open_tag(ctx, parser, tag, end, 0);
		for (i = 0; i < node->v.element.attributes.length; ++i)
		{
			GumboAttribute *att = node->v.element.attributes.data[i];
			xml_emit_att_name(ctx, parser, att->name, att->name+strlen(att->name));
			xml_emit_att_value(ctx, parser, att->value, att->value+strlen(att->value));
		}
		for (i = 0; i < node->v.element.children.length; ++i)
		{
			GumboNode *child = node->v.element.children.data[i];
			xml_from_gumbo(ctx, parser, child);
		}
		xml_emit_close_tag(ctx, parser);
		break;

	case GUMBO_NODE_TEXT:
	case GUMBO_NODE_CDATA:
	case GUMBO_NODE_WHITESPACE:
		xml_emit_text(ctx, parser, node->v.text.text, node->v.text.text+strlen(node->v.text.text));
		break;

	case GUMBO_NODE_DOCUMENT:
	case GUMBO_NODE_COMMENT:
	case GUMBO_NODE_TEMPLATE:
		break;
	}
}

fz_xml *
fz_parse_xml_from_html5(fz_context *ctx, fz_buffer *buf)
{
	struct parser parser;
	fz_xml *xml = NULL;
	fz_xml root, *node;
	char *p = NULL;
	int dofree = 0;
	unsigned char *s;
	size_t n;
	GumboOutput *soup = NULL;
	GumboOptions opts;
	struct mem_gumbo mem;
	static unsigned char empty_string[] = "";

	fz_var(mem.pool);
	fz_var(soup);
	fz_var(dofree);
	fz_var(p);

	if (buf == NULL)
	{
		n = 0;
		s = empty_string;
	}
	else
	{
		/* ensure we are zero-terminated */
		fz_terminate_buffer(ctx, buf);
		n = fz_buffer_storage(ctx, buf, &s);
	}

	mem.ctx = ctx;
	mem.pool = NULL;

	memset(&root, 0, sizeof(root));
	parser.pool = fz_new_pool(ctx);
	parser.head = &root;
	parser.preserve_white = 1;
	parser.depth = 0;
#ifdef FZ_XML_SEQ
	parser.seq = 0;
#endif

	fz_try(ctx)
	{
		p = convert_to_utf8(ctx, s, n, &dofree);

		mem.pool = fz_new_pool(ctx);
		memset(&opts, 0, sizeof opts);
		opts.allocator = alloc_gumbo;
		opts.deallocator = dealloc_gumbo;
		opts.userdata = &mem;
		opts.tab_stop = 8;
		opts.stop_on_first_error = 0;
		opts.max_errors = -1;
		opts.fragment_context = GUMBO_TAG_LAST;
		opts.fragment_namespace = GUMBO_NAMESPACE_HTML;

		soup = gumbo_parse_with_options(&opts, (const char *)p, strlen(p));

		xml_from_gumbo(ctx, &parser, soup->root);

		for (node = parser.head; node; node = node->up)
			node->u.node.next = NULL;

		xml = fz_pool_alloc(ctx, parser.pool, sizeof *xml);
		xml->up = NULL;
		xml->down = root.down;
		xml->u.doc.pool = parser.pool;
		xml->u.doc.refs = 1;

		for (node = root.down; node; node = node->u.node.next)
			node->up = xml;
	}
	fz_always(ctx)
	{
		if (soup)
			gumbo_destroy_output(&opts, soup);
		fz_drop_pool(ctx, mem.pool);
		if (dofree)
			fz_free(ctx, p);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, parser.pool);
		fz_rethrow(ctx);
	}

	return xml;
}

fz_xml *fz_xml_find_dfs(fz_xml *item, const char *tag, const char *att, const char *match)
{
	return fz_xml_find_dfs_top(item, tag, att, match, NULL);
}

fz_xml *fz_xml_find_dfs_top(fz_xml *item, const char *tag, const char *att, const char *match, fz_xml *top)
{
	/* Skip over any DOC object. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	while (item)
	{
		if (!FZ_TEXT_ITEM(item) && (tag == NULL || !strcmp(item->u.node.u.d.name, tag)))
		{
			if (att == NULL || (match == NULL ? fz_xml_att(item, att) != NULL : fz_xml_att_eq(item, att, match)))
				return item;
		}

		if (!FZ_TEXT_ITEM(item) && item->down)
			item = item->down;
		else if (item->u.node.next)
			item = item->u.node.next;
		else
			while (1) {
				item = item->up;
				/* Stop searching if we hit our declared 'top' item. */
				if (item == top)
					return NULL;
				/* We should never reach item == NULL, but just in case. */
				if (item == NULL)
					return NULL;
				/* If we reach the DOC object at the top, we're done. */
				if (item->up == NULL)
					return NULL;
				if (item->u.node.next)
				{
					item = item->u.node.next;
					break;
				}
			}
	}

	return NULL;
}

fz_xml *fz_xml_find_next_dfs(fz_xml *item, const char *tag, const char *att, const char *match)
{
	return fz_xml_find_next_dfs_top(item, tag, att, match, NULL);
}

fz_xml *fz_xml_find_next_dfs_top(fz_xml *item, const char *tag, const char *att, const char *match, fz_xml *top)
{
	/* Skip over any DOC object. */
	if (item && FZ_DOCUMENT_ITEM(item))
		item = item->down;

	if (item == NULL)
		return NULL;

	if (item->down)
		item = item->down;
	else if (item->u.node.next)
		item = item->u.node.next;
	else
		while (1) {
			item = item->up;
			/* Stop searching if we hit our declared 'top' item. */
			if (item == top)
				return NULL;
			/* We should never reach item == NULL, but just in case. */
			if (item == NULL)
				return NULL;
			/* If we reach the DOC object at the top, we're done. */
			if (item->up == NULL)
				return NULL;
			if (item->u.node.next)
			{
				item = item->u.node.next;
				break;
			}
		}

	return fz_xml_find_dfs_top(item, tag, att, match, top);
}

fz_xml *fz_keep_xml(fz_context *ctx, fz_xml *xml)
{
	fz_xml *dom = xml;
	if (xml == NULL)
		return xml;

	while (dom->up)
		dom = dom->up;

	fz_keep_imp(ctx, dom, &dom->u.doc.refs);

	/* Return the original node pointer, not the dom pointer! */
	return xml;
}
