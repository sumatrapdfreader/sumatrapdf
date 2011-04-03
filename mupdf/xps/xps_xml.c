#include "fitz.h"
#include "muxps.h"

struct attribute
{
	char name[40];
	char *value;
	struct attribute *next;
};

struct element
{
	char name[40];
	struct attribute *atts;
	struct element *up, *down, *next;
};

struct parser
{
	struct element *head;
};

static inline void indent(int n)
{
	while (n--) putchar(' ');
}

void xml_print_element(struct element *item, int level)
{
	while (item) {
		struct attribute *att;
		indent(level);
		printf("<%s", item->name);
		for (att = item->atts; att; att = att->next)
			printf(" %s=\"%s\"", att->name, att->value);
		if (item->down) {
			printf(">\n");
			xml_print_element(item->down, level + 1);
			indent(level);
			printf("</%s>\n", item->name);
		}
		else {
			printf("/>\n");
		}
		item = item->next;
	}
}

struct element *xml_next(struct element *item)
{
	return item->next;
}

struct element *xml_down(struct element *item)
{
	return item->down;
}

char *xml_tag(struct element *item)
{
	return item->name;
}

char *xml_att(struct element *item, const char *name)
{
	struct attribute *att;
	for (att = item->atts; att; att = att->next)
		if (!strcmp(att->name, name))
			return att->value;
	return NULL;
}

static void xml_free_attribute(struct attribute *att)
{
	while (att) {
		struct attribute *next = att->next;
		if (att->value)
			fz_free(att->value);
		fz_free(att);
		att = next;
	}
}

void xml_free_element(struct element *item)
{
	while (item) {
		struct element *next = item->next;
		if (item->atts)
			xml_free_attribute(item->atts);
		if (item->down)
			xml_free_element(item->down);
		fz_free(item);
		item = next;
	}
}

static int xml_parse_entity(int *c, char *a)
{
	char *b;
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
	*c = *a++;
	return 1;
}

static void xml_emit_open_tag(struct parser *parser, char *a, char *b)
{
	struct element *head, *tail;

	head = fz_malloc(sizeof(struct element));
	if (b - a > sizeof(head->name))
		b = a + sizeof(head->name);
	memcpy(head->name, a, b - a);
	head->name[b - a] = 0;

	head->atts = NULL;
	head->up = parser->head;
	head->down = NULL;
	head->next = NULL;

	if (!parser->head->down) {
		parser->head->down = head;
	}
	else {
		tail = parser->head->down;
		while (tail->next)
			tail = tail->next;
		tail->next = head;
	}

	parser->head = head;
}

static void xml_emit_att_name(struct parser *parser, char *a, char *b)
{
	struct element *head = parser->head;
	struct attribute *att;

	att = fz_malloc(sizeof(struct attribute));
	if (b - a > sizeof(att->name))
		b = a + sizeof(att->name);
	memcpy(att->name, a, b - a);
	att->name[b - a] = 0;
	att->value = NULL;
	att->next = head->atts;
	head->atts = att;
}

static void xml_emit_att_value(struct parser *parser, char *a, char *b)
{
	struct element *head = parser->head;
	struct attribute *att = head->atts;
	char *s;
	int c;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = att->value = fz_malloc(b - a + 1);
	while (a < b) {
		if (*a == '&') {
			a += xml_parse_entity(&c, a);
			s += runetochar(s, &c);
		}
		else {
			*s++ = *a++;
		}
	}
	*s = 0;
}

static void xml_emit_close_tag(struct parser *parser)
{
	if (parser->head->up)
		parser->head = parser->head->up;
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

static char *xml_parse_document_imp(struct parser *x, char *p)
{
	char *mark;
	int quote;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	if (*p == '<') { ++p; goto parse_element; }
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
	if (*p == '[') goto parse_cdata;
	if (*p++ != '-') return "syntax error in comment (<! not followed by --)";
	if (*p++ != '-') return "syntax error in comment (<!- not followed by -)";
	mark = p;
	while (*p) {
		if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in comment";

parse_cdata:
	if (p[1] != 'C' || p[2] != 'D' || p[3] != 'A' || p[4] != 'T' || p[5] != 'A' || p[6] != '[')
		return "syntax error in CDATA section";
	p += 7;
	mark = p;
	while (*p) {
		if (p[0] == ']' && p[1] == ']' && p[2] == '>') {
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
	while (iswhite(*p)) ++p;
	if (*p != '>')
		return "syntax error in closing element";
	xml_emit_close_tag(x);
	++p;
	goto parse_text;

parse_element_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_open_tag(x, mark, p);
	if (*p == '>') { ++p; goto parse_text; }
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag(x);
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
	if (*p == '>') { ++p; goto parse_text; }
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag(x);
		p += 2;
		goto parse_text;
	}
	return "syntax error in attributes";

parse_attribute_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_att_name(x, mark, p);
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
		xml_emit_att_value(x, mark, p++);
		goto parse_attributes;
	}
	return "end of data in attribute value";
}

static char *convert_to_utf8(unsigned char *s, int n)
{
	unsigned char *e = s + n;
	char *dst, *d;
	int c;

	if (s[0] == 0xFE && s[1] == 0xFF) {
		dst = d = fz_malloc(n * 2);
		while (s + 1 < e) {
			c = s[0] << 8 | s[1];
			d += runetochar(d, &c);
			s += 2;
		}
		*d = 0;
		return dst;
	}

	if (s[0] == 0xFF && s[1] == 0xFE) {
		dst = d = fz_malloc(n * 2);
		while (s + 1 < e) {
			c = s[0] | s[1] << 8;
			d += runetochar(d, &c);
			s += 2;
		}
		*d = 0;
		return dst;
	}

	return (char*)s;
}

struct element *
xml_parse_document(unsigned char *s, int n)
{
	struct parser parser;
	struct element root;
	char *p, *error;

	/* s is already null-terminated (see xps_new_part) */

	memset(&root, 0, sizeof(root));
	parser.head = &root;

	p = convert_to_utf8(s, n);

	error = xml_parse_document_imp(&parser, p);
	if (error) {
		fz_throw(error);
		return NULL;
	}

	if (p != (char*)s)
		fz_free(p);

	return root.down;
}
