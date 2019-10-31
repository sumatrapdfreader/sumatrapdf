#include "mupdf/fitz.h"
#include "html-imp.h"

#include <string.h>

struct lexbuf
{
	fz_context *ctx;
	fz_pool *pool;
	const unsigned char *s;
	const char *file;
	int line;
	int lookahead;
	int c;
	int string_len;
	char string[1024];
};

static fz_css_value *parse_expr(struct lexbuf *buf);
static fz_css_selector *parse_selector(struct lexbuf *buf);

FZ_NORETURN static void fz_css_error(struct lexbuf *buf, const char *msg)
{
	fz_throw(buf->ctx, FZ_ERROR_SYNTAX, "css syntax error: %s (%s:%d)", msg, buf->file, buf->line);
}

fz_css *fz_new_css(fz_context *ctx)
{
	fz_pool *pool = fz_new_pool(ctx);
	fz_css *css = NULL;

	fz_try(ctx)
	{
		css = fz_pool_alloc(ctx, pool, sizeof *css);
		css->pool = pool;
		css->rule = NULL;
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}

	return css;
}

void fz_drop_css(fz_context *ctx, fz_css *css)
{
	if (css)
		fz_drop_pool(ctx, css->pool);
}

static fz_css_rule *fz_new_css_rule(fz_context *ctx, fz_pool *pool, fz_css_selector *selector, fz_css_property *declaration)
{
	fz_css_rule *rule = fz_pool_alloc(ctx, pool, sizeof *rule);
	rule->selector = selector;
	rule->declaration = declaration;
	rule->next = NULL;
	return rule;
}

static fz_css_selector *fz_new_css_selector(fz_context *ctx, fz_pool *pool, const char *name)
{
	fz_css_selector *sel = fz_pool_alloc(ctx, pool, sizeof *sel);
	sel->name = name ? fz_pool_strdup(ctx, pool, name) : NULL;
	sel->combine = 0;
	sel->cond = NULL;
	sel->left = NULL;
	sel->right = NULL;
	sel->next = NULL;
	return sel;
}

static fz_css_condition *fz_new_css_condition(fz_context *ctx, fz_pool *pool, int type, const char *key, const char *val)
{
	fz_css_condition *cond = fz_pool_alloc(ctx, pool, sizeof *cond);
	cond->type = type;
	cond->key = key ? fz_pool_strdup(ctx, pool, key) : NULL;
	cond->val = val ? fz_pool_strdup(ctx, pool, val) : NULL;
	cond->next = NULL;
	return cond;
}

static fz_css_property *fz_new_css_property(fz_context *ctx, fz_pool *pool, const char *name, fz_css_value *value, int spec)
{
	fz_css_property *prop = fz_pool_alloc(ctx, pool, sizeof *prop);
	prop->name = fz_pool_strdup(ctx, pool, name);
	prop->value = value;
	prop->spec = spec;
	prop->important = 0;
	prop->next = NULL;
	return prop;
}

static fz_css_value *fz_new_css_value_x(fz_context *ctx, fz_pool *pool, int type)
{
	fz_css_value *val = fz_pool_alloc(ctx, pool, sizeof *val);
	val->type = type;
	val->data = NULL;
	val->args = NULL;
	val->next = NULL;
	return val;
}

static fz_css_value *fz_new_css_value(fz_context *ctx, fz_pool *pool, int type, const char *data)
{
	fz_css_value *val = fz_pool_alloc(ctx, pool, sizeof *val);
	val->type = type;
	val->data = fz_pool_strdup(ctx, pool, data);
	val->args = NULL;
	val->next = NULL;
	return val;
}

static void css_lex_next(struct lexbuf *buf)
{
	buf->c = *(buf->s++);
	if (buf->c == '\n')
		++buf->line;
}

static void css_lex_init(fz_context *ctx, struct lexbuf *buf, fz_pool *pool, const char *s, const char *file)
{
	buf->ctx = ctx;
	buf->pool = pool;
	buf->s = (const unsigned char *)s;
	buf->c = 0;
	buf->file = file;
	buf->line = 1;
	css_lex_next(buf);

	buf->string_len = 0;
}

static inline int iswhite(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int isnmstart(int c)
{
	return c == '\\' || c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= 128 && c <= 255);
}

static int isnmchar(int c)
{
	return c == '\\' || c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') || c == '-' || (c >= 128 && c <= 255);
}

static void css_push_char(struct lexbuf *buf, int c)
{
	if (buf->string_len + 1 >= (int)nelem(buf->string))
		fz_css_error(buf, "token too long");
	buf->string[buf->string_len++] = c;
}

static int css_lex_accept(struct lexbuf *buf, int t)
{
	if (buf->c == t)
	{
		css_lex_next(buf);
		return 1;
	}
	return 0;
}

static void css_lex_expect(struct lexbuf *buf, int t)
{
	if (!css_lex_accept(buf, t))
		fz_css_error(buf, "unexpected character");
}

static int css_lex_number(struct lexbuf *buf)
{
	while (buf->c >= '0' && buf->c <= '9')
	{
		css_push_char(buf, buf->c);
		css_lex_next(buf);
	}

	if (css_lex_accept(buf, '.'))
	{
		css_push_char(buf, '.');
		while (buf->c >= '0' && buf->c <= '9')
		{
			css_push_char(buf, buf->c);
			css_lex_next(buf);
		}
	}

	if (css_lex_accept(buf, '%'))
	{
		css_push_char(buf, '%');
		css_push_char(buf, 0);
		return CSS_PERCENT;
	}

	if (isnmstart(buf->c))
	{
		css_push_char(buf, buf->c);
		css_lex_next(buf);
		while (isnmchar(buf->c))
		{
			css_push_char(buf, buf->c);
			css_lex_next(buf);
		}
		css_push_char(buf, 0);
		return CSS_LENGTH;
	}

	css_push_char(buf, 0);
	return CSS_NUMBER;
}

static int css_lex_keyword(struct lexbuf *buf)
{
	while (isnmchar(buf->c))
	{
		css_push_char(buf, buf->c);
		css_lex_next(buf);
	}
	css_push_char(buf, 0);
	return CSS_KEYWORD;
}

static int css_lex_hash(struct lexbuf *buf)
{
	while (isnmchar(buf->c))
	{
		css_push_char(buf, buf->c);
		css_lex_next(buf);
	}
	css_push_char(buf, 0);
	return CSS_HASH;
}

static int css_lex_string(struct lexbuf *buf, int q)
{
	while (buf->c && buf->c != q)
	{
		if (css_lex_accept(buf, '\\'))
		{
			if (css_lex_accept(buf, 'n'))
				css_push_char(buf, '\n');
			else if (css_lex_accept(buf, 'r'))
				css_push_char(buf, '\r');
			else if (css_lex_accept(buf, 'f'))
				css_push_char(buf, '\f');
			else if (css_lex_accept(buf, '\f'))
				/* line continuation */ ;
			else if (css_lex_accept(buf, '\n'))
				/* line continuation */ ;
			else if (css_lex_accept(buf, '\r'))
				css_lex_accept(buf, '\n');
			else
			{
				css_push_char(buf, buf->c);
				css_lex_next(buf);
			}
		}
		else
		{
			css_push_char(buf, buf->c);
			css_lex_next(buf);
		}
	}
	css_lex_expect(buf, q);
	css_push_char(buf, 0);
	return CSS_STRING;
}

static void css_lex_uri(struct lexbuf *buf)
{
	while (buf->c && buf->c != ')' && !iswhite(buf->c))
	{
		if (css_lex_accept(buf, '\\'))
		{
			if (css_lex_accept(buf, 'n'))
				css_push_char(buf, '\n');
			else if (css_lex_accept(buf, 'r'))
				css_push_char(buf, '\r');
			else if (css_lex_accept(buf, 'f'))
				css_push_char(buf, '\f');
			else
			{
				css_push_char(buf, buf->c);
				css_lex_next(buf);
			}
		}
		else if (buf->c == '!' || buf->c == '#' || buf->c == '$' || buf->c == '%' || buf->c == '&' ||
				(buf->c >= '*' && buf->c <= '[') ||
				(buf->c >= ']' && buf->c <= '~') ||
				buf->c > 159)
		{
			css_push_char(buf, buf->c);
			css_lex_next(buf);
		}
		else
			fz_css_error(buf, "unexpected character in url");
	}
	css_push_char(buf, 0);
}

static int css_lex(struct lexbuf *buf)
{
	int t;

	// TODO: keyword escape sequences

	buf->string_len = 0;

restart:
	if (buf->c == 0)
		return EOF;

	if (iswhite(buf->c))
	{
		while (iswhite(buf->c))
			css_lex_next(buf);
		return ' ';
	}

	if (css_lex_accept(buf, '/'))
	{
		if (css_lex_accept(buf, '*'))
		{
			while (buf->c)
			{
				if (css_lex_accept(buf, '*'))
				{
					while (buf->c == '*')
						css_lex_next(buf);
					if (css_lex_accept(buf, '/'))
						goto restart;
				}
				css_lex_next(buf);
			}
			fz_css_error(buf, "unterminated comment");
		}
		return '/';
	}

	if (css_lex_accept(buf, '<'))
	{
		if (css_lex_accept(buf, '!'))
		{
			css_lex_expect(buf, '-');
			css_lex_expect(buf, '-');
			goto restart; /* ignore CDO */
		}
		return '<';
	}

	if (css_lex_accept(buf, '-'))
	{
		if (css_lex_accept(buf, '-'))
		{
			css_lex_expect(buf, '>');
			goto restart; /* ignore CDC */
		}
		if (isnmstart(buf->c))
		{
			css_push_char(buf, '-');
			return css_lex_keyword(buf);
		}
		return '-';
	}

	if (css_lex_accept(buf, '.'))
	{
		if (buf->c >= '0' && buf->c <= '9')
		{
			css_push_char(buf, '.');
			return css_lex_number(buf);
		}
		return '.';
	}

	if (css_lex_accept(buf, '#'))
	{
		if (isnmchar(buf->c))
			return css_lex_hash(buf);
		return '#';
	}

	if (css_lex_accept(buf, '"'))
		return css_lex_string(buf, '"');
	if (css_lex_accept(buf, '\''))
		return css_lex_string(buf, '\'');

	if (buf->c >= '0' && buf->c <= '9')
		return css_lex_number(buf);

	if (css_lex_accept(buf, 'u'))
	{
		if (css_lex_accept(buf, 'r'))
		{
			if (css_lex_accept(buf, 'l'))
			{
				if (css_lex_accept(buf, '('))
				{
					while (iswhite(buf->c))
						css_lex_next(buf);
					if (css_lex_accept(buf, '"'))
						css_lex_string(buf, '"');
					else if (css_lex_accept(buf, '\''))
						css_lex_string(buf, '\'');
					else
						css_lex_uri(buf);
					while (iswhite(buf->c))
						css_lex_next(buf);
					css_lex_expect(buf, ')');
					return CSS_URI;
				}
				css_push_char(buf, 'u');
				css_push_char(buf, 'r');
				css_push_char(buf, 'l');
				return css_lex_keyword(buf);
			}
			css_push_char(buf, 'u');
			css_push_char(buf, 'r');
			return css_lex_keyword(buf);
		}
		css_push_char(buf, 'u');
		return css_lex_keyword(buf);
	}

	if (isnmstart(buf->c))
	{
		css_push_char(buf, buf->c);
		css_lex_next(buf);
		return css_lex_keyword(buf);
	}

	t = buf->c;
	css_lex_next(buf);
	return t;
}

static void next(struct lexbuf *buf)
{
	buf->lookahead = css_lex(buf);
}

static int accept(struct lexbuf *buf, int t)
{
	if (buf->lookahead == t)
	{
		next(buf);
		return 1;
	}
	return 0;
}

static void expect(struct lexbuf *buf, int t)
{
	if (accept(buf, t))
		return;
	fz_css_error(buf, "unexpected token");
}

static void white(struct lexbuf *buf)
{
	while (buf->lookahead == ' ')
		next(buf);
}

static int iscond(int t)
{
	return t == ':' || t == '.' || t == '[' || t == CSS_HASH;
}

static fz_css_value *parse_term(struct lexbuf *buf)
{
	fz_css_value *v;

	if (buf->lookahead == '+' || buf->lookahead == '-')
	{
		float sign = buf->lookahead == '-' ? -1 : 1;
		next(buf);
		if (buf->lookahead != CSS_NUMBER && buf->lookahead != CSS_LENGTH && buf->lookahead != CSS_PERCENT)
			fz_css_error(buf, "expected number");
		if (sign < 0)
		{
			v = fz_new_css_value_x(buf->ctx, buf->pool, buf->lookahead);
			v->data = fz_pool_alloc(buf->ctx, buf->pool, strlen(buf->string) + 2);
			v->data[0] = '-';
			strcpy(v->data + 1, buf->string);
		}
		else
		{
			v = fz_new_css_value(buf->ctx, buf->pool, buf->lookahead, buf->string);
		}
		next(buf);
		white(buf);
		return v;
	}

	if (buf->lookahead == CSS_KEYWORD)
	{
		v = fz_new_css_value(buf->ctx, buf->pool, CSS_KEYWORD, buf->string);
		next(buf);
		if (accept(buf, '('))
		{
			white(buf);
			v->type = '(';
			v->args = parse_expr(buf);
			expect(buf, ')');
		}
		white(buf);
		return v;
	}

	switch (buf->lookahead)
	{
	case CSS_HASH:
	case CSS_STRING:
	case CSS_URI:
	case CSS_NUMBER:
	case CSS_LENGTH:
	case CSS_PERCENT:
		v = fz_new_css_value(buf->ctx, buf->pool, buf->lookahead, buf->string);
		next(buf);
		white(buf);
		return v;
	}

	fz_css_error(buf, "expected value");
}

static fz_css_value *parse_expr(struct lexbuf *buf)
{
	fz_css_value *head, *tail;

	head = tail = parse_term(buf);

	while (buf->lookahead != '}' && buf->lookahead != ';' && buf->lookahead != '!' &&
			buf->lookahead != ')' && buf->lookahead != EOF)
	{
		if (accept(buf, ','))
		{
			white(buf);
			tail = tail->next = fz_new_css_value(buf->ctx, buf->pool, ',', ",");
			tail = tail->next = parse_term(buf);
		}
		else if (accept(buf, '/'))
		{
			white(buf);
			tail = tail->next = fz_new_css_value(buf->ctx, buf->pool, '/', "/");
			tail = tail->next = parse_term(buf);
		}
		else
		{
			tail = tail->next = parse_term(buf);
		}
	}

	return head;
}

static fz_css_property *parse_declaration(struct lexbuf *buf)
{
	fz_css_property *p;

	if (buf->lookahead != CSS_KEYWORD)
		fz_css_error(buf, "expected keyword in property");
	p = fz_new_css_property(buf->ctx, buf->pool, buf->string, NULL, 0);
	next(buf);

	white(buf);
	expect(buf, ':');
	white(buf);

	p->value = parse_expr(buf);

	/* !important */
	if (accept(buf, '!'))
	{
		white(buf);
		if (buf->lookahead != CSS_KEYWORD || strcmp(buf->string, "important"))
			fz_css_error(buf, "expected keyword 'important' after '!'");
		p->important = 1;
		next(buf);
		white(buf);
	}

	return p;
}

static fz_css_property *parse_declaration_list(struct lexbuf *buf)
{
	fz_css_property *head, *tail;

	white(buf);

	if (buf->lookahead == '}' || buf->lookahead == EOF)
		return NULL;

	head = tail = parse_declaration(buf);

	while (accept(buf, ';'))
	{
		white(buf);

		if (buf->lookahead != '}' && buf->lookahead != ';' && buf->lookahead != EOF)
		{
			tail = tail->next = parse_declaration(buf);
		}
	}

	return head;
}

static char *parse_attrib_value(struct lexbuf *buf)
{
	char *s;

	if (buf->lookahead == CSS_KEYWORD || buf->lookahead == CSS_STRING)
	{
		s = fz_pool_strdup(buf->ctx, buf->pool, buf->string);
		next(buf);
		white(buf);
		return s;
	}

	fz_css_error(buf, "expected attribute value");
}

static fz_css_condition *parse_condition(struct lexbuf *buf)
{
	fz_css_condition *c;

	if (accept(buf, ':'))
	{
		accept(buf, ':'); /* swallow css3 :: syntax and pretend it's a normal pseudo-class */
		if (buf->lookahead != CSS_KEYWORD)
			fz_css_error(buf, "expected keyword after ':'");
		c = fz_new_css_condition(buf->ctx, buf->pool, ':', "pseudo", buf->string);
		next(buf);
		if (accept(buf, '('))
		{
			white(buf);
			if (accept(buf, CSS_KEYWORD))
				white(buf);
			expect(buf, ')');
		}
		return c;
	}

	if (accept(buf, '.'))
	{
		if (buf->lookahead != CSS_KEYWORD)
			fz_css_error(buf, "expected keyword after '.'");
		c = fz_new_css_condition(buf->ctx, buf->pool, '.', "class", buf->string);
		next(buf);
		return c;
	}

	if (accept(buf, '['))
	{
		white(buf);

		if (buf->lookahead != CSS_KEYWORD)
			fz_css_error(buf, "expected keyword after '['");
		c = fz_new_css_condition(buf->ctx, buf->pool, '[', buf->string, NULL);
		next(buf);

		white(buf);

		if (accept(buf, '='))
		{
			c->type = '=';
			c->val = parse_attrib_value(buf);
		}
		else if (accept(buf, '|'))
		{
			expect(buf, '=');
			c->type = '|';
			c->val = parse_attrib_value(buf);
		}
		else if (accept(buf, '~'))
		{
			expect(buf, '=');
			c->type = '~';
			c->val = parse_attrib_value(buf);
		}

		expect(buf, ']');

		return c;
	}

	if (buf->lookahead == CSS_HASH)
	{
		c = fz_new_css_condition(buf->ctx, buf->pool, '#', "id", buf->string);
		next(buf);
		return c;
	}

	fz_css_error(buf, "expected condition");
}

static fz_css_condition *parse_condition_list(struct lexbuf *buf)
{
	fz_css_condition *head, *tail;

	head = tail = parse_condition(buf);
	while (iscond(buf->lookahead))
	{
		tail = tail->next = parse_condition(buf);
	}
	return head;
}

static fz_css_selector *parse_simple_selector(struct lexbuf *buf)
{
	fz_css_selector *s;

	if (accept(buf, '*'))
	{
		s = fz_new_css_selector(buf->ctx, buf->pool, NULL);
		if (iscond(buf->lookahead))
			s->cond = parse_condition_list(buf);
		return s;
	}
	else if (buf->lookahead == CSS_KEYWORD)
	{
		s = fz_new_css_selector(buf->ctx, buf->pool, buf->string);
		next(buf);
		if (iscond(buf->lookahead))
			s->cond = parse_condition_list(buf);
		return s;
	}
	else if (iscond(buf->lookahead))
	{
		s = fz_new_css_selector(buf->ctx, buf->pool, NULL);
		s->cond = parse_condition_list(buf);
		return s;
	}

	fz_css_error(buf, "expected selector");
}

static fz_css_selector *parse_combinator(struct lexbuf *buf, int c, fz_css_selector *a)
{
	fz_css_selector *sel, *b;
	white(buf);
	b = parse_simple_selector(buf);
	sel = fz_new_css_selector(buf->ctx, buf->pool, NULL);
	sel->combine = c;
	sel->left = a;
	sel->right = b;
	return sel;
}

static fz_css_selector *parse_selector(struct lexbuf *buf)
{
	fz_css_selector *sel = parse_simple_selector(buf);
	for (;;)
	{
		if (accept(buf, ' '))
		{
			if (accept(buf, '+'))
				sel = parse_combinator(buf, '+', sel);
			else if (accept(buf, '>'))
				sel = parse_combinator(buf, '>', sel);
			else if (buf->lookahead != ',' && buf->lookahead != '{' && buf->lookahead != EOF)
				sel = parse_combinator(buf, ' ', sel);
			else
				break;
		}
		else if (accept(buf, '+'))
			sel = parse_combinator(buf, '+', sel);
		else if (accept(buf, '>'))
			sel = parse_combinator(buf, '>', sel);
		else
			break;
	}
	return sel;
}

static fz_css_selector *parse_selector_list(struct lexbuf *buf)
{
	fz_css_selector *head, *tail;

	head = tail = parse_selector(buf);
	while (accept(buf, ','))
	{
		white(buf);
		tail = tail->next = parse_selector(buf);
	}
	return head;
}

static fz_css_rule *parse_ruleset(struct lexbuf *buf)
{
	fz_css_selector *s = NULL;
	fz_css_property *p = NULL;

	fz_try(buf->ctx)
	{
		s = parse_selector_list(buf);
		expect(buf, '{');
		p = parse_declaration_list(buf);
		expect(buf, '}');
		white(buf);
	}
	fz_catch(buf->ctx)
	{
		if (fz_caught(buf->ctx) != FZ_ERROR_SYNTAX)
			fz_rethrow(buf->ctx);
		while (buf->lookahead != EOF)
		{
			if (accept(buf, '}'))
			{
				white(buf);
				break;
			}
			next(buf);
		}
		return NULL;
	}

	return fz_new_css_rule(buf->ctx, buf->pool, s, p);
}

static fz_css_rule *parse_at_page(struct lexbuf *buf)
{
	fz_css_selector *s = NULL;
	fz_css_property *p = NULL;

	white(buf);
	if (accept(buf, ':'))
	{
		expect(buf, CSS_KEYWORD);
		white(buf);
	}
	expect(buf, '{');
	p = parse_declaration_list(buf);
	expect(buf, '}');
	white(buf);

	s = fz_new_css_selector(buf->ctx, buf->pool, "@page");
	return fz_new_css_rule(buf->ctx, buf->pool, s, p);
}

static fz_css_rule *parse_at_font_face(struct lexbuf *buf)
{
	fz_css_selector *s = NULL;
	fz_css_property *p = NULL;

	white(buf);
	expect(buf, '{');
	p = parse_declaration_list(buf);
	expect(buf, '}');
	white(buf);

	s = fz_new_css_selector(buf->ctx, buf->pool, "@font-face");
	return fz_new_css_rule(buf->ctx, buf->pool, s, p);
}

static void parse_at_rule(struct lexbuf *buf)
{
	expect(buf, CSS_KEYWORD);

	/* skip until '{' or ';' */
	while (buf->lookahead != EOF)
	{
		if (accept(buf, ';'))
		{
			white(buf);
			return;
		}
		if (accept(buf, '{'))
		{
			int depth = 1;
			while (buf->lookahead != EOF && depth > 0)
			{
				if (accept(buf, '{'))
					++depth;
				else if (accept(buf, '}'))
					--depth;
				else
					next(buf);
			}
			white(buf);
			return;
		}
		next(buf);
	}
}

static fz_css_rule *parse_stylesheet(struct lexbuf *buf, fz_css_rule *chain)
{
	fz_css_rule *rule, **nextp, *tail;

	tail = chain;
	if (tail)
	{
		while (tail->next)
			tail = tail->next;
		nextp = &tail->next;
	}
	else
	{
		nextp = &tail;
	}

	white(buf);

	while (buf->lookahead != EOF)
	{
		if (accept(buf, '@'))
		{
			if (buf->lookahead == CSS_KEYWORD && !strcmp(buf->string, "page"))
			{
				next(buf);
				rule = *nextp = parse_at_page(buf);
				nextp = &rule->next;
			}
			else if (buf->lookahead == CSS_KEYWORD && !strcmp(buf->string, "font-face"))
			{
				next(buf);
				rule = *nextp = parse_at_font_face(buf);
				nextp = &rule->next;
			}
			else
			{
				parse_at_rule(buf);
			}
		}
		else
		{
			fz_css_rule *x = parse_ruleset(buf);
			if (x)
			{
				rule = *nextp = x;
				nextp = &rule->next;
			}
		}
		white(buf);
	}

	return chain ? chain : tail;
}

fz_css_property *fz_parse_css_properties(fz_context *ctx, fz_pool *pool, const char *source)
{
	struct lexbuf buf;
	css_lex_init(ctx, &buf, pool, source, "<inline>");
	next(&buf);
	return parse_declaration_list(&buf);
}

void fz_parse_css(fz_context *ctx, fz_css *css, const char *source, const char *file)
{
	struct lexbuf buf;
	css_lex_init(ctx, &buf, css->pool, source, file);
	next(&buf);
	css->rule = parse_stylesheet(&buf, css->rule);
}
