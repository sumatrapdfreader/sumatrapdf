#include "mupdf/fitz.h"

static const struct { const char *ent; int ucs; } html_entities[] = {
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
	fz_xml *head;
	fz_context *ctx;
	int preserve_white;
	int depth;
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
	fz_xml *up, *down, *prev, *next;
};

static inline void indent(int n)
{
	while (n--) putchar(' ');
}

void fz_debug_xml(fz_xml *item, int level)
{
	if (item->text)
	{
		printf("%s\n", item->text);
	}
	else
	{
		fz_xml *child;
		struct attribute *att;

		indent(level);
		printf("<%s", item->name);
		for (att = item->atts; att; att = att->next)
			printf(" %s=\"%s\"", att->name, att->value);
		if (item->down)
		{
			printf(">\n");
			for (child = item->down; child; child = child->next)
				fz_debug_xml(child, level + 1);
			indent(level);
			printf("</%s>\n", item->name);
		}
		else
		{
			printf("/>\n");
		}
	}
}

fz_xml *fz_xml_prev(fz_xml *item)
{
	return item->prev;
}

fz_xml *fz_xml_next(fz_xml *item)
{
	return item->next;
}

fz_xml *fz_xml_up(fz_xml *item)
{
	return item->up;
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
	if (item->name[0])
		return item->name;
	return NULL;
}

int fz_xml_is_tag(fz_xml *item, const char *name)
{
	return item->name && !strcmp(item->name, name);
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
	int i;

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
		unsigned int n = strlen(html_entities[i].ent);
		if (!memcmp(a+1, html_entities[i].ent, n) && a[1+n] == ';') {
			*c = html_entities[i].ucs;
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

static void xml_emit_open_tag(struct parser *parser, char *a, char *b)
{
	fz_xml *head, *tail;
	char *ns;

	/* skip namespace prefix */
	for (ns = a; ns < b; ++ns)
		if (*ns == ':')
			a = ns + 1;

	head = fz_malloc_struct(parser->ctx, fz_xml);
	if (b - a > sizeof(head->name) - 1)
		b = a + sizeof(head->name) - 1;
	memcpy(head->name, a, b - a);
	head->name[b - a] = 0;

	head->atts = NULL;
	head->text = NULL;
	head->up = parser->head;
	head->down = NULL;
	head->prev = NULL;
	head->next = NULL;

	if (!parser->head->down) {
		parser->head->down = head;
	}
	else {
		tail = parser->head->down;
		while (tail->next)
			tail = tail->next;
		tail->next = head;
		head->prev = tail;
	}

	parser->head = head;
	parser->depth++;
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
	parser->depth--;
	if (parser->head->up)
		parser->head = parser->head->up;
}

static void xml_emit_text(struct parser *parser, char *a, char *b)
{
	static char *empty = "";
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

static void xml_emit_cdata(struct parser *parser, char *a, char *b)
{
	static char *empty = "";
	fz_xml *head;
	char *s;

	xml_emit_open_tag(parser, empty, empty);
	head = parser->head;

	s = head->text = fz_malloc(parser->ctx, b - a + 1);
	while (a < b)
		*s++ = *a++;
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
	if (*p == 'D' && !memcmp(p, "DOCTYPE", 7)) goto parse_declaration;
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
			xml_emit_cdata(x, mark, p);
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
fz_parse_xml(fz_context *ctx, unsigned char *s, int n, int preserve_white)
{
	struct parser parser;
	fz_xml root, *node;
	char *p, *error;
	int dofree;

	/* s is already null-terminated (see xps_new_part) */

	memset(&root, 0, sizeof(root));
	parser.head = &root;
	parser.ctx = ctx;
	parser.preserve_white = preserve_white;
	parser.depth = 0;

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

	for (node = root.down; node; node = node->next)
		node->up = NULL;
	return root.down;
}
