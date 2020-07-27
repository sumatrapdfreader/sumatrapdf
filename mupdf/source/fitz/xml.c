#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gumbo.h>

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

struct attribute
{
	char *value;
	struct attribute *next;
	char name[1];
};

struct fz_xml_doc
{
	fz_pool *pool;
	fz_xml *root;
};

/* Text nodes never use the down pointer. Therefore
 * if the down pointer is the MAGIC_TEXT value, we
 * know there is text. */
struct fz_xml
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

void fz_debug_xml(fz_xml *item, int level)
{
	char *s = fz_xml_text(item);
	if (s)
	{
		int c;
		xml_indent(level);
		putchar('"');
		while (*s) {
			s += fz_chartorune(&c, s);
			switch (c) {
			default:
				if (c > 0xFFFF)
					printf("\\u{%X}", c);
				else if (c < 32 || c > 127)
					printf("\\u%04X", c);
				else
					putchar(c);
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

fz_xml *fz_xml_prev(fz_xml *item)
{
	return item ? item->prev : NULL;
}

fz_xml *fz_xml_next(fz_xml *item)
{
	return item ? item->next : NULL;
}

fz_xml *fz_xml_up(fz_xml *item)
{
	return item ? item->up : NULL;
}

fz_xml *fz_xml_down(fz_xml *item)
{
	return item && !FZ_TEXT_ITEM(item) ? item->down : NULL;
}

char *fz_xml_text(fz_xml *item)
{
	return (item && FZ_TEXT_ITEM(item)) ? item->u.text : NULL;
}

char *fz_xml_tag(fz_xml *item)
{
	return item && !FZ_TEXT_ITEM(item) && item->u.d.name[0] ? item->u.d.name : NULL;
}

int fz_xml_is_tag(fz_xml *item, const char *name)
{
	if (!item || FZ_TEXT_ITEM(item))
		return 0;
	return !strcmp(item->u.d.name, name);
}

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

char *fz_xml_att_alt(fz_xml *item, const char *one, const char *two)
{
	char *val = fz_xml_att(item, one);
	if (!val)
		val = fz_xml_att(item, two);
	return val;
}

fz_xml *fz_xml_find(fz_xml *item, const char *tag)
{
	while (item)
	{
		if (!FZ_TEXT_ITEM(item) && !strcmp(item->u.d.name, tag))
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

int fz_xml_att_eq(fz_xml *item, const char *name, const char *match)
{
	const char *val = fz_xml_att(item, name);

	return val ? !strcmp(val, match) : 0;
}

fz_xml *fz_xml_find_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	while (1)
	{
		item = fz_xml_find(item, tag);
		if (item == NULL || fz_xml_att_eq(item, att, match))
			break;
		item = item->next;
	}

	return item;
}

fz_xml *fz_xml_find_next_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	do
	{
		item = fz_xml_find_next(item, tag);
	}
	while (item != NULL && !fz_xml_att_eq(item, att, match));

	return item;
}

fz_xml *fz_xml_find_down_match(fz_xml *item, const char *tag, const char *att, const char *match)
{
	return fz_xml_find_match(fz_xml_down(item), tag, att, match);
}

fz_xml *fz_xml_root(fz_xml_doc *xml)
{
	return xml ? xml->root : NULL;
}

void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml)
{
	if (xml)
		fz_drop_pool(ctx, xml->pool);
}

void fz_detach_xml(fz_context *ctx, fz_xml_doc *xml, fz_xml *node)
{
	if (node->up)
		node->up->down = NULL;
	xml->root = node;
}

static size_t xml_parse_entity(int *c, const char *a)
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
		size = offsetof(fz_xml, u.text) + b-a+1;
	else
	{
		/* skip namespace prefix */
		for (ns = a; ns < b - 1; ++ns)
			if (*ns == ':')
				a = ns + 1;

		size = offsetof(fz_xml, u.d.name) + b-a+1;
	}
	head = fz_pool_alloc(ctx, parser->pool, size);

	if (is_text)
		head->down = MAGIC_TEXT;
	else
	{
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
	att->next = head->u.d.atts;
	head->u.d.atts = att;
}

static void xml_emit_att_value(fz_context *ctx, struct parser *parser, const char *a, const char *b)
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

	s = head->u.text;
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

static char *xml_parse_document_imp(fz_context *ctx, struct parser *parser, const char *p)
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

fz_xml_doc *
fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white)
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

fz_xml_doc *
fz_parse_xml_from_html5(fz_context *ctx, fz_buffer *buf)
{
	struct parser parser;
	fz_xml_doc *xml = NULL;
	fz_xml root, *node;
	char *p = NULL;
	int dofree = 0;
	unsigned char *s;
	size_t n;
	GumboOutput *soup = NULL;
	GumboOptions opts;
	struct mem_gumbo mem;

	fz_var(mem.pool);
	fz_var(soup);
	fz_var(dofree);
	fz_var(p);

	/* ensure we are zero-terminated */
	fz_terminate_buffer(ctx, buf);
	n = fz_buffer_storage(ctx, buf, &s);

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
			node->next = NULL;
		for (node = root.down; node; node = node->next)
			node->up = NULL;

		xml = fz_pool_alloc(ctx, parser.pool, sizeof *xml);
		xml->pool = parser.pool;
		xml->root = root.down;
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
