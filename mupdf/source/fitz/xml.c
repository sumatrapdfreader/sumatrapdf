#include "mupdf/fitz.h"

struct parser
{
	fz_xml *head;
	fz_context *ctx;
};

struct attribute
{
	char name[40];
	char *value;
	struct attribute *next;
};

struct fz_xml_s
{
	char name[40];
	char *text;
	struct attribute *atts;
	fz_xml *up, *down, *next;
};

static inline void indent(int n)
{
	while (n--) putchar(' ');
}

void fz_debug_xml(fz_xml *item, int level)
{
	while (item) {
		if (item->text) {
			printf("%s\n", item->text);
		} else {
			struct attribute *att;
			indent(level);
			printf("<%s", item->name);
			for (att = item->atts; att; att = att->next)
				printf(" %s=\"%s\"", att->name, att->value);
			if (item->down) {
				printf(">\n");
				fz_debug_xml(item->down, level + 1);
				indent(level);
				printf("</%s>\n", item->name);
			}
			else {
				printf("/>\n");
			}
		}
		item = item->next;
	}
}

fz_xml *fz_xml_next(fz_xml *item)
{
	return item->next;
}

fz_xml *fz_xml_down(fz_xml *item)
{
	return item->down;
}

char *fz_xml_text(fz_xml *item)
{
	return item->text;
}

char *fz_xml_tag(fz_xml *item)
{
	return item->name;
}

char *fz_xml_att(fz_xml *item, const char *name)
{
	struct attribute *att;
	for (att = item->atts; att; att = att->next)
		if (!strcmp(att->name, name))
			return att->value;
	return NULL;
}

static void xml_free_attribute(fz_context *ctx, struct attribute *att)
{
	while (att) {
		struct attribute *next = att->next;
		if (att->value)
			fz_free(ctx, att->value);
		fz_free(ctx, att);
		att = next;
	}
}

void fz_free_xml(fz_context *ctx, fz_xml *item)
{
	while (item)
	{
		fz_xml *next = item->next;
		if (item->text)
			fz_free(ctx, item->text);
		if (item->atts)
			xml_free_attribute(ctx, item->atts);
		if (item->down)
			fz_free_xml(ctx, item->down);
		fz_free(ctx, item);
		item = next;
	}
}

void fz_detach_xml(fz_xml *node)
{
	if (node->up)
		node->up->down = NULL;
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

static void xml_emit_open_tag(struct parser *parser, char *a, char *b)
{
	fz_xml *head, *tail;

	head = fz_malloc_struct(parser->ctx, fz_xml);
	if (b - a > sizeof(head->name) - 1)
		b = a + sizeof(head->name) - 1;
	memcpy(head->name, a, b - a);
	head->name[b - a] = 0;

	head->atts = NULL;
	head->text = NULL;
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
	fz_xml *head = parser->head;
	struct attribute *att;

	att = fz_malloc_struct(parser->ctx, struct attribute);
	if (b - a > sizeof(att->name) - 1)
		b = a + sizeof(att->name) - 1;
	memcpy(att->name, a, b - a);
	att->name[b - a] = 0;
	att->value = NULL;
	att->next = head->atts;
	head->atts = att;
}

static void xml_emit_att_value(struct parser *parser, char *a, char *b)
{
	fz_xml *head = parser->head;
	struct attribute *att = head->atts;
	char *s;
	int c;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = att->value = fz_malloc(parser->ctx, b - a + 1);
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

static void xml_emit_close_tag(struct parser *parser)
{
	if (parser->head->up)
		parser->head = parser->head->up;
}

static void xml_emit_text(struct parser *parser, char *a, char *b)
{
	static char *empty = "";
	fz_xml *head;
	char *s;
	int c;

	/* Skip all-whitespace text nodes */
	for (s = a; s < b; s++)
		if (!iswhite(*s))
			break;
	if (s == b)
		return;

	xml_emit_open_tag(parser, empty, empty);
	head = parser->head;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = head->text = fz_malloc(parser->ctx, b - a + 1);
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

	xml_emit_close_tag(parser);
}

static char *xml_parse_document_imp(struct parser *x, char *p)
{
	char *mark;
	int quote;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	xml_emit_text(x, mark, p);
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

static char *convert_to_utf8(fz_context *doc, unsigned char *s, int n, int *dofree)
{
	unsigned char *e = s + n;
	char *dst, *d;
	int c;

	if (s[0] == 0xFE && s[1] == 0xFF) {
		s += 2;
		dst = d = fz_malloc(doc, n * 2);
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
		dst = d = fz_malloc(doc, n * 2);
		while (s + 1 < e) {
			c = s[0] | s[1] << 8;
			d += fz_runetochar(d, c);
			s += 2;
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
fz_parse_xml(fz_context *ctx, unsigned char *s, int n)
{
	struct parser parser;
	fz_xml root;
	char *p, *error;
	int dofree;

	/* s is already null-terminated (see xps_new_part) */

	memset(&root, 0, sizeof(root));
	parser.head = &root;
	parser.ctx = ctx;

	p = convert_to_utf8(ctx, s, n, &dofree);

	fz_try(ctx)
	{
		error = xml_parse_document_imp(&parser, p);
		if (error)
			fz_throw(ctx, FZ_ERROR_GENERIC, "%s", error);
	}
	fz_always(ctx)
	{
		if (dofree)
			fz_free(ctx, p);
	}
	fz_catch(ctx)
	{
		fz_free_xml(ctx, root.down);
		fz_rethrow(ctx);
	}

	return root.down;
}
