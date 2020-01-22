#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* #define FZ_XML_SEQ */

/* We bend the XML parser slightly when it's reading for HTML.
 * To do this, we use extra knowledge about HTML tags, expressed
 * in the table in a header. */

#define START_OPTIONAL 1
#define END_OPTIONAL 2
#define END_FORBIDDEN 4
#define DEPRECATED 8
#define DTD_LOOSE 16
#define DTD_FRAMESET 32
/* The following values are added by us. */
/* If a tag can contain nested instances of itself, we
 * mark it as being a container. */
#define CONTAINER 64
/* Table tags autoclose each other in complex ways. */
#define TABLE_SHIFT 7
#define TABLE_MASK 7
/* Which level of a table are we? */
/* 128 * 1 = TABLE
 * 128 * 2 = COLGROUP
 * 128 * 3 = COL
 * 128 * 4 = THEAD/TBODY/TFOOT
 * 128 * 5 = TR
 * 128 * 6 = TD/TH
 * Any table tag, will be autoclosed by the opening of another table tag
 * (within the same container) of a smaller level than it. */
#define IMPLIES_SHIFT 10
#define IMPLIES_SHIFT2 17
#define IMPLIES_SHIFT3 24
#define IMPLIES_MASK 127
/* If a tag should always be contained within another one, we
 * indicate this with an 'implies'. TABLE tags never imply
 * out of the current table. */

typedef struct { char tag[16]; int flags; } fz_xml_html_tag_t;

#define HTML_TAG(A,B,C,D,E) fz_xml_html_tag_ ## A

enum
{
	fz_xml_html_tag__NONE,
#include "html-tags.h"
	, fz_xml_html_tag__NUMTAGS
};

#define HTML_TAG(A,B,C,D,E) { # A, E | (fz_xml_html_tag_ ## B << IMPLIES_SHIFT) | (fz_xml_html_tag_ ## C << IMPLIES_SHIFT2)  | (fz_xml_html_tag_ ## D << IMPLIES_SHIFT3) }

fz_xml_html_tag_t html_tags[] =
{
	{ "", 0 },
#include "html-tags.h"
};

/*
When parsing XML, we assume that all tags are properly terminated.
i.e. <foo> has </foo>  or <foo />.
We currently don't check this at all. In fact, we generate an incorrect
tree if this isn't the case.

For example:

<a><b><c></b><d></d></a>

Will produce:
  a
  b
  cd

Rather than:
  a
  bd
  c

Over the course of a large HTML file, this can lead to HUGE "south easterly" skew
in the tree.

This happens because (when parsing pure xml) when we hit </d>, we don't check that the tag we are closing is actually a <d>.

So, some heuristics to use when parsing HTML:
  * When we open a tag 'foo', if we are immediately in another 'foo', then close the first 'foo' first.
  * When we close a tag 'foo', run up the stack looking for an enclosing 'foo'. If we find one, close
	everything up to and including that. If we don't find one, don't close anything.

With these heuristics, we get the following for free:
  * A TD closes any other TD.
  * A /TR closes and open TD.
  * A TR closes any open TR.

This leaves problems with:
  * Nested tables.
  * Nested divs.
  * Nested spans.

Tables, divs and spans (let alone nested ones) are problematic anyway. Ignore
this for now.

We could special case TABLE, DIV and SPAN so appropriate tags don't pop up past them.
*/

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
	int for_html;
	int depth;
#ifdef FZ_XML_SEQ
	int seq;
#endif
};

struct attribute
{
	char *value;
	struct attribute *next;
	char name[1];
};

struct fz_xml_doc_s
{
	fz_pool *pool;
	fz_xml *root;
};

/* Text nodes never use the down pointer. Therefore
 * if the down pointer is the MAGIC_TEXT value, we
 * know there is text. */
struct fz_xml_s
{
	fz_xml *up, *down, *prev, *next;
#ifdef FZ_XML_SEQ
	int seq;
#endif
	union
	{
		char text[1];
		struct
		{
			struct attribute *atts;
			char name[1];
		} d;
	} u;
};

#define MAGIC_TEXT ((fz_xml *)1)
#define FZ_TEXT_ITEM(item) (item && item->down == MAGIC_TEXT)

static void xml_indent(int n)
{
	while (n--) {
		putchar(' ');
		putchar(' ');
	}
}

/*
	Pretty-print an XML tree to stdout.
*/
void fz_debug_xml(fz_xml *item, int level)
{
	char *s = fz_xml_text(item);
	if (s)
	{
		int c;
		xml_indent(level);
		putchar('"');
		while ((c = *s++)) {
			switch (c) {
			default:
				if (c < 32 || c > 127) {
					putchar('\\');
					putchar('x');
					putchar("0123456789ABCDEF"[(c>>4) & 15]);
					putchar("0123456789ABCDEF"[(c) & 15]);
				} else {
					putchar(c);
				}
				break;
			case '\\': putchar('\\'); putchar('\\'); break;
			case '\b': putchar('\\'); putchar('b'); break;
			case '\f': putchar('\\'); putchar('f'); break;
			case '\n': putchar('\\'); putchar('n'); break;
			case '\r': putchar('\\'); putchar('r'); break;
			case '\t': putchar('\\'); putchar('t'); break;
			}
		}
		putchar('"');
#ifdef FZ_XML_SEQ
		printf(" <%d>", item->seq);
#endif
		putchar('\n');
	}
	else
	{
		fz_xml *child;
		struct attribute *att;

		xml_indent(level);
#ifdef FZ_XML_SEQ
		printf("(%s <%d>\n", item->u.d.name, item->seq);
#else
		printf("(%s\n", item->u.d.name);
#endif
		for (att = item->u.d.atts; att; att = att->next)
		{
			xml_indent(level);
			printf("=%s %s\n", att->name, att->value);
		}
		for (child = fz_xml_down(item); child; child = child->next)
			fz_debug_xml(child, level + 1);
		xml_indent(level);
#ifdef FZ_XML_SEQ
		printf(")%s <%d>\n", item->u.d.name, item->seq);
#else
		printf(")%s\n", item->u.d.name);
#endif
	}
}

/*
	Return previous sibling of XML node.
*/
fz_xml *fz_xml_prev(fz_xml *item)
{
	return item ? item->prev : NULL;
}

/*
	Return next sibling of XML node.
*/
fz_xml *fz_xml_next(fz_xml *item)
{
	return item ? item->next : NULL;
}

/*
	Return parent of XML node.
*/
fz_xml *fz_xml_up(fz_xml *item)
{
	return item ? item->up : NULL;
}

/*
	Return first child of XML node.
*/
fz_xml *fz_xml_down(fz_xml *item)
{
	return item && !FZ_TEXT_ITEM(item) ? item->down : NULL;
}

/*
	Return the text content of an XML node.
	Return NULL if the node is a tag.
*/
char *fz_xml_text(fz_xml *item)
{
	return (item && FZ_TEXT_ITEM(item)) ? item->u.text : NULL;
}

/*
	Return tag of XML node. Return NULL for text nodes.
*/
char *fz_xml_tag(fz_xml *item)
{
	return item && !FZ_TEXT_ITEM(item) && item->u.d.name[0] ? item->u.d.name : NULL;
}

/*
	Return true if the tag name matches.
*/
int fz_xml_is_tag(fz_xml *item, const char *name)
{
	if (!item || FZ_TEXT_ITEM(item))
		return 0;
	return !strcmp(item->u.d.name, name);
}

/*
	Return the value of an attribute of an XML node.
	NULL if the attribute doesn't exist.
*/
char *fz_xml_att(fz_xml *item, const char *name)
{
	struct attribute *att;
	if (!item || FZ_TEXT_ITEM(item))
		return NULL;
	for (att = item->u.d.atts; att; att = att->next)
		if (!strcmp(att->name, name))
			return att->value;
	return NULL;
}

fz_xml *fz_xml_find(fz_xml *item, const char *tag)
{
	while (item)
	{
		if (!strcmp(item->u.d.name, tag))
			return item;
		item = item->next;
	}
	return NULL;
}

fz_xml *fz_xml_find_next(fz_xml *item, const char *tag)
{
	if (item)
		item = item->next;
	return fz_xml_find(item, tag);
}

fz_xml *fz_xml_find_down(fz_xml *item, const char *tag)
{
	if (item)
		item = fz_xml_down(item);
	return fz_xml_find(item, tag);
}

fz_xml *fz_xml_root(fz_xml_doc *xml)
{
	return xml ? xml->root : NULL;
}

/*
	Free the XML node and all its children and siblings.
*/
void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml)
{
	if (xml)
		fz_drop_pool(ctx, xml->pool);
}

/*
	Detach a node from the tree, unlinking it from its parent,
	and setting the document root to the node.
*/
void fz_detach_xml(fz_context *ctx, fz_xml_doc *xml, fz_xml *node)
{
	if (node->up)
		node->up->down = NULL;
	xml->root = node;
}

static size_t xml_parse_entity(int *c, char *a)
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

static int
find_html_tag(const char *tag, size_t len)
{
	int low = 0;
	int high = nelem(html_tags);
	int mid;

	while (low != high)
	{
		int cmp;
		mid = (low + high)>>1;
		cmp = strncmp(html_tags[mid].tag, tag, len);
		if (cmp == 0)
			cmp = html_tags[mid].tag[len];
		if (cmp == 0)
			return mid;
		if (cmp < 0)
			low = mid+1;
		else
			high = mid;
	}

	return fz_xml_html_tag__NONE;
}

static int xml_emit_open_tag(fz_context *ctx, struct parser *parser, char *a, char *b, int is_text)
{
	fz_xml *head, *tail;
	size_t size;
	int autoclose = 0;

	if (is_text)
		size = offsetof(fz_xml, u.text) + b-a+1;
	else
		size = offsetof(fz_xml, u.d.name) + b-a+1;
	head = fz_pool_alloc(ctx, parser->pool, size);

	if (is_text)
		head->down = MAGIC_TEXT;
	else
	{
		if (parser->for_html)
		{
			int tag_num;

			/* Lowercase the tag */
			char *s = head->u.d.name;
			char *t = a;
			while (t != b)
			{
				char c = *t++;
				if (c >= 'A' && c <= 'Z')
					c += 'a' - 'A';
				*s++ = c;
			}

			tag_num = find_html_tag(head->u.d.name, b-a);
			if (tag_num != fz_xml_html_tag__NONE && (html_tags[tag_num].flags & END_FORBIDDEN))
				autoclose = 1;
			/* If parsing as HTML, and we see a FictionBook tag, turn off HTML fixups. */
			if (tag_num == fz_xml_html_tag_fictionbook)
			{
				/* Restore original case. */
				memcpy(head->u.d.name, "FictionBook", 11);
				parser->for_html = 0;
			}
		}
		else
			memcpy(head->u.d.name, a, b - a);
		head->u.d.name[b - a] = 0;
		head->u.d.atts = NULL;
		head->down = NULL;
	}

	head->up = parser->head;
	head->next = NULL;
#ifdef FZ_XML_SEQ
	head->seq = parser->seq++;
#endif

	/* During construction, we use head->next to mean "the
	 * tail of the children. When we close the tag, we
	 * rewrite it to be NULL. */
	if (!parser->head->down) {
		parser->head->down = head;
		parser->head->next = head;
		head->prev = NULL;
	}
	else {
		tail = parser->head->next;
		tail->next = head;
		head->prev = tail;
		parser->head->next = head;
	}

	parser->head = head;
	parser->depth++;

	return autoclose;
}

static void xml_emit_att_name(fz_context *ctx, struct parser *parser, char *a, char *b)
{
	fz_xml *head = parser->head;
	struct attribute *att;
	size_t size;

	size = offsetof(struct attribute, name) + b-a+1;
	att = fz_pool_alloc(ctx, parser->pool, size);
	memcpy(att->name, a, b - a);
	att->name[b - a] = 0;
	att->value = NULL;
	att->next = head->u.d.atts;
	head->u.d.atts = att;
}

static void xml_emit_att_value(fz_context *ctx, struct parser *parser, char *a, char *b)
{
	fz_xml *head = parser->head;
	struct attribute *att = head->u.d.atts;
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
	parser->head->next = NULL;
	if (parser->head->up)
		parser->head = parser->head->up;
}

static void xml_emit_text(fz_context *ctx, struct parser *parser, char *a, char *b)
{
	fz_xml *head;
	char *s;
	int c;

	/* Skip text outside the root tag */
	if (parser->depth == 0)
		return;

	/* Skip all-whitespace text nodes */
	if (!parser->preserve_white)
	{
		for (s = a; s < b; s++)
			if (!iswhite(*s))
				break;
		if (s == b)
			return;
	}

	(void)xml_emit_open_tag(ctx, parser, a, b, 1);
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

static void xml_emit_cdata(fz_context *ctx, struct parser *parser, char *a, char *b)
{
	fz_xml *head;
	char *s;

	(void)xml_emit_open_tag(ctx, parser, a, b, 1);
	head = parser->head;

	s = head->u.text;
	while (a < b)
		*s++ = *a++;
	*s = 0;

	xml_emit_close_tag(ctx, parser);
}

static int
pop_to_tag(fz_context *ctx, struct parser *parser, char *mark, char *p)
{
	fz_xml *to, *head;

	/* Run up from the tag */
	if (parser->for_html)
	{
		for (to = parser->head; to; to = to->up)
		{
			char *tag = fz_xml_tag(to);
			if (tag && fz_strncasecmp(tag, mark, p-mark) == 0 && tag[p-mark] == 0)
				break; /* Found a matching tag */
		}
	}
	else
	{
		for (to = parser->head; to; to = to->up)
		{
			char *tag = fz_xml_tag(to);
			if (tag && strncmp(tag, mark, p-mark) == 0 && tag[p-mark] == 0)
				break; /* Found a matching tag */
		}
	}

	if (to == NULL)
	{
		/* We didn't find a matching enclosing tag. Don't close anything. */
		return 0;
	}

	/* Pop everything up to and including this tag. */
	for (head = parser->head; head != to; head = head->up)
		xml_emit_close_tag(ctx, parser);
	return 1;
}

static void
open_implied(fz_context *ctx, struct parser *parser, int tag)
{
	fz_xml *head;
	int implied, implied2, implied3, tag_num;
	int table_level;

	if (tag == fz_xml_html_tag__NONE)
		return;

	implied  = (html_tags[tag].flags >> IMPLIES_SHIFT) & IMPLIES_MASK;
	implied2 = (html_tags[tag].flags >> IMPLIES_SHIFT2) & IMPLIES_MASK;
	implied3 = (html_tags[tag].flags >> IMPLIES_SHIFT3) & IMPLIES_MASK;
	if (implied == fz_xml_html_tag__NONE)
		return;
	if (implied2 == fz_xml_html_tag__NONE)
		implied2 = implied;
	if (implied3 == fz_xml_html_tag__NONE)
		implied3 = implied;

	/* So, check to see whether implied{,2,3} is present. */
	table_level = (html_tags[tag].flags>>TABLE_SHIFT) & TABLE_MASK;
	if (table_level != 0)
	{
		/* Table tag. Autoclose anything within the current TABLE
		 * with >= table_level. */
		fz_xml *close_to = NULL;
		int implied_found = 0;
		for (head = parser->head; head; head = head->up)
		{
			char *tag = fz_xml_tag(head);
			int level;

			if (tag == NULL)
				continue;
			tag_num = find_html_tag(tag, strlen(tag));
			level = (html_tags[tag_num].flags>>TABLE_SHIFT) & TABLE_MASK;
			if (level >= table_level)
				close_to = head;
			if (tag_num == implied || tag_num == implied2 || tag_num == implied3)
				implied_found = 1;
			if (tag_num == fz_xml_html_tag_table)
				break;
		}
		if (close_to)
		{
			for (head = parser->head; head; head = head->up)
			{
				xml_emit_close_tag(ctx, parser);
				if (head == close_to)
					break;
			}
		}
		if (!implied_found)
		{
			char *tag = html_tags[implied].tag;
			open_implied(ctx, parser, implied);
			xml_emit_open_tag(ctx, parser, tag, tag + strlen(tag), 0);
		}
	}
	else
	{
		/* Non table tag. Open by implication. */
		for (head = parser->head; head; head = head->up)
		{
			char *tag = fz_xml_tag(head);

			if (tag == NULL)
				continue;
			tag_num = find_html_tag(tag, strlen(tag));
			if (tag_num == implied || tag_num == implied2 || tag_num == implied3)
				break;
		}
		if (head == NULL)
		{
			char *s = html_tags[implied].tag;
			open_implied(ctx, parser, implied);
			(void)xml_emit_open_tag(ctx, parser, s, s+strlen(s), 0);
		}
	}
}

/* When we meet a new tag, before we open it, there may be
 * things we should do first... */
static void
pre_open_tag(fz_context *ctx, struct parser *parser, char *mark, char *p)
{
	fz_xml *head = parser->head;
	int tag_num;

	if (!parser->for_html)
		return;

	tag_num = find_html_tag(mark, p-mark);

	if (tag_num == fz_xml_html_tag__NONE)
		return;

	if ((html_tags[tag_num].flags & CONTAINER) == 0)
	{
		/* We aren't a container flag. This means that we should autoclose up to
		 * any matching tags in the same container. */
		fz_xml *which;
		for (which = head; which; which = which->up)
		{
			char *tag = fz_xml_tag(which);
			int tag_num2 = tag ? find_html_tag(tag, strlen(tag)) : fz_xml_html_tag__NONE;
			if (tag_num == tag_num2)
			{
				/* Autoclose everything from head to which inclusive */
				while (1)
				{
					int done = (head == which);
					xml_emit_close_tag(ctx, parser);
					head = head->up;
					if (done)
						break;
				}
				break;
			}
			if (html_tags[tag_num2].flags & CONTAINER)
			{
				/* Stop searching */
				break;
			}
		}
	}

	/* Now, autoopen any tags implied by this one. */
	open_implied(ctx, parser, tag_num);
}

static char *
skip_namespace_prefix(char *mark, char *p)
{
	char *ns;

	for (ns = mark; ns < p - 1; ++ns)
		if (*ns == ':')
			mark = ns + 1;

	return mark;
}

static char *xml_parse_document_imp(fz_context *ctx, struct parser *parser, char *p)
{
	char *mark;
	int quote;
	int autoclose;
	char *q;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	if (*p == '<') {
		/* skip trailing newline before closing tag */
		if (p[1] == '/' && mark < p - 1 && p[-1] == '\n')
			xml_emit_text(ctx, parser, mark, p - 1);
		else if (mark < p)
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
	q = p;
	while (iswhite(*p)) ++p;
	if (*p != '>')
		return "syntax error in closing element";
	mark = skip_namespace_prefix(mark, q);
	if (pop_to_tag(ctx, parser, mark, q))
		xml_emit_close_tag(ctx, parser);
	++p;
	goto parse_text;

parse_element_name:
	mark = p;
	while (isname(*p)) ++p;
	mark = skip_namespace_prefix(mark, p);
	pre_open_tag(ctx, parser, mark, p);
	autoclose = xml_emit_open_tag(ctx, parser, mark, p, 0);
	if (*p == '>') {
		if (autoclose)
			xml_emit_close_tag(ctx, parser);
		++p;
		if (*p == '\n') ++p; /* must skip linebreak immediately after an opening tag */
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
		if (autoclose)
			xml_emit_close_tag(ctx, parser);
		++p;
		if (*p == '\n') ++p; /* must skip linebreak immediately after an opening tag */
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
	if (quote != '"' && quote != '\'')
		return "missing quote character";
	mark = p;
	while (*p && *p != quote) ++p;
	if (*p == quote) {
		xml_emit_att_value(ctx, parser, mark, p++);
		goto parse_attributes;
	}
	return "end of data in attribute value";
}

static int startswith(const char *a, const char *b)
{
	return !fz_strncasecmp(a, b, strlen(b));
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

/*
	Parse the contents of buffer into a tree of xml nodes.

	preserve_white: whether to keep or delete all-whitespace nodes.
*/
fz_xml_doc *
fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white, int for_html)
{
	struct parser parser;
	fz_xml_doc *xml = NULL;
	fz_xml root, *node;
	char *p = NULL;
	char *error;
	int dofree = 0;
	unsigned char *s;
	size_t n;

	fz_var(dofree);
	fz_var(p);

	/* ensure we are zero-terminated */
	fz_terminate_buffer(ctx, buf);
	n = fz_buffer_storage(ctx, buf, &s);

	memset(&root, 0, sizeof(root));
	parser.pool = fz_new_pool(ctx);
	parser.head = &root;
	parser.preserve_white = preserve_white;
	parser.for_html = for_html;
	parser.depth = 0;
#ifdef FZ_XML_SEQ
	parser.seq = 0;
#endif

	fz_try(ctx)
	{
		p = convert_to_utf8(ctx, s, n, &dofree);

		error = xml_parse_document_imp(ctx, &parser, p);
		if (error)
			fz_throw(ctx, FZ_ERROR_GENERIC, "%s", error);

		for (node = parser.head; node; node = node->up)
			node->next = NULL;

		for (node = root.down; node; node = node->next)
			node->up = NULL;

		xml = fz_pool_alloc(ctx, parser.pool, sizeof *xml);
		xml->pool = parser.pool;
		xml->root = root.down;
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
