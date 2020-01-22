#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

#define IS_NUMBER \
	'+':case'-':case'.':case'0':case'1':case'2':case'3':\
	case'4':case'5':case'6':case'7':case'8':case'9'
#define IS_WHITE \
	'\x00':case'\x09':case'\x0a':case'\x0c':case'\x0d':case'\x20'
#define IS_HEX \
	'0':case'1':case'2':case'3':case'4':case'5':case'6':\
	case'7':case'8':case'9':case'A':case'B':case'C':\
	case'D':case'E':case'F':case'a':case'b':case'c':\
	case'd':case'e':case'f'
#define IS_DELIM \
	'(':case')':case'<':case'>':case'[':case']':case'{':\
	case'}':case'/':case'%'

#define RANGE_0_9 \
	'0':case'1':case'2':case'3':case'4':case'5':\
	case'6':case'7':case'8':case'9'
#define RANGE_a_f \
	'a':case'b':case'c':case'd':case'e':case'f'
#define RANGE_A_F \
	'A':case'B':case'C':case'D':case'E':case'F'
#define RANGE_0_7 \
	'0':case'1':case'2':case'3':case'4':case'5':case'6':case'7'

/* #define DUMP_LEXER_STREAM */
#ifdef DUMP_LEXER_STREAM
static inline int lex_byte(fz_context *ctx, fz_stream *stm)
{
	int c = fz_read_byte(ctx, stm);

	if (c == EOF)
		fz_write_printf(ctx, fz_stdout(ctx), "<EOF>");
	else if (c >= 32 && c < 128)
		fz_write_printf(ctx, fz_stdout(ctx), "%c", c);
	else
		fz_write_printf(ctx, fz_stdout(ctx), "<%02x>", c);
	return c;
}
#else
#define lex_byte(C,S) fz_read_byte(C,S)
#endif

static inline int iswhite(int ch)
{
	return
		ch == '\000' ||
		ch == '\011' ||
		ch == '\012' ||
		ch == '\014' ||
		ch == '\015' ||
		ch == '\040';
}

static inline int fz_isprint(int ch)
{
	return ch >= ' ' && ch <= '~';
}

static inline int unhex(int ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 0xA;
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 0xA;
	return 0;
}

static void
lex_white(fz_context *ctx, fz_stream *f)
{
	int c;
	do {
		c = lex_byte(ctx, f);
	} while ((c <= 32) && (iswhite(c)));
	if (c != EOF)
		fz_unread_byte(ctx, f);
}

static void
lex_comment(fz_context *ctx, fz_stream *f)
{
	int c;
	do {
		c = lex_byte(ctx, f);
	} while ((c != '\012') && (c != '\015') && (c != EOF));
}

/* Fast(ish) but inaccurate strtof, with Adobe overflow handling. */
static float acrobat_compatible_atof(char *s)
{
	int neg = 0;
	int i = 0;

	while (*s == '-')
	{
		neg = 1;
		++s;
	}
	while (*s == '+')
	{
		++s;
	}

	while (*s >= '0' && *s <= '9')
	{
		/* We deliberately ignore overflow here.
		 * Tests show that Acrobat handles * overflows in exactly the same way we do:
		 * 123450000000000000000678 is read as 678.
		 */
		i = i * 10 + (*s - '0');
		++s;
	}

	if (*s == '.')
	{
		float v = i;
		float n = 0;
		float d = 1;
		++s;
		while (*s >= '0' && *s <= '9')
		{
			n = 10 * n + (*s - '0');
			d = 10 * d;
			++s;
		}
		v += n / d;
		return neg ? -v : v;
	}
	else
	{
		return neg ? -i : i;
	}
}

/* Fast but inaccurate atoi. */
static int fast_atoi(char *s)
{
	int neg = 0;
	int i = 0;

	while (*s == '-')
	{
		neg = 1;
		++s;
	}
	while (*s == '+')
	{
		++s;
	}

	while (*s >= '0' && *s <= '9')
	{
		/* We deliberately ignore overflow here. */
		i = i * 10 + (*s - '0');
		++s;
	}

	return neg ? -i : i;
}

static int
lex_number(fz_context *ctx, fz_stream *f, pdf_lexbuf *buf, int c)
{
	char *s = buf->scratch;
	char *e = buf->scratch + buf->size - 1; /* leave space for zero terminator */
	char *isreal = (c == '.' ? s : NULL);
	int neg = (c == '-');
	int isbad = 0;

	*s++ = c;

	c = lex_byte(ctx, f);

	/* skip extra '-' signs at start of number */
	if (neg)
	{
		while (c == '-')
			c = lex_byte(ctx, f);
	}

	while (s < e)
	{
		switch (c)
		{
		case IS_WHITE:
		case IS_DELIM:
			fz_unread_byte(ctx, f);
			goto end;
		case EOF:
			goto end;
		case '.':
			if (isreal)
				isbad = 1;
			isreal = s;
			*s++ = c;
			break;
		case RANGE_0_9:
			*s++ = c;
			break;
		default:
			isbad = 1;
			*s++ = c;
			break;
		}
		c = lex_byte(ctx, f);
	}

end:
	*s = '\0';
	if (isbad)
		return PDF_TOK_ERROR;
	if (isreal)
	{
		/* We'd like to use the fastest possible atof
		 * routine, but we'd rather match acrobats
		 * handling of broken numbers. As such, we
		 * spot common broken cases and call an
		 * acrobat compatible routine where required. */
		if (neg > 1 || isreal - buf->scratch >= 10)
			buf->f = acrobat_compatible_atof(buf->scratch);
		else
			buf->f = fz_atof(buf->scratch);
		return PDF_TOK_REAL;
	}
	else
	{
		buf->i = fast_atoi(buf->scratch);
		return PDF_TOK_INT;
	}
}

static void
lex_name(fz_context *ctx, fz_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + fz_minz(127, lb->size);
	int c;

	while (1)
	{
		if (s == e)
		{
			if (e - lb->scratch < 127)
			{
				s += pdf_lexbuf_grow(ctx, lb);
				e = lb->scratch + fz_minz(127, lb->size);
			}
			else
			{
				/* truncate names that are too long */
				fz_warn(ctx, "name is too long");
				*s = 0;
				lb->len = s - lb->scratch;
				s = NULL;
			}
		}
		c = lex_byte(ctx, f);
		switch (c)
		{
		case IS_WHITE:
		case IS_DELIM:
			fz_unread_byte(ctx, f);
			goto end;
		case EOF:
			goto end;
		case '#':
		{
			int hex[2];
			int i;
			for (i = 0; i < 2; i++)
			{
				c = fz_peek_byte(ctx, f);
				switch (c)
				{
				case RANGE_0_9:
					if (i == 1 && c == '0' && hex[0] == 0)
						goto illegal;
					hex[i] = lex_byte(ctx, f) - '0';
					break;
				case RANGE_a_f:
					hex[i] = lex_byte(ctx, f) - 'a' + 10;
					break;
				case RANGE_A_F:
					hex[i] = lex_byte(ctx, f) - 'A' + 10;
					break;
				default:
				case EOF:
					goto illegal;
				}
			}
			if (s) *s++ = (hex[0] << 4) + hex[1];
			break;
illegal:
			if (i == 1)
				fz_unread_byte(ctx, f);
			if (s) *s++ = '#';
			continue;
		}
		default:
			if (s) *s++ = c;
			break;
		}
	}
end:
	if (s)
	{
		*s = '\0';
		lb->len = s - lb->scratch;
	}
}

static int
lex_string(fz_context *ctx, fz_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + lb->size;
	int bal = 1;
	int oct;
	int c;

	while (1)
	{
		if (s == e)
		{
			s += pdf_lexbuf_grow(ctx, lb);
			e = lb->scratch + lb->size;
		}
		c = lex_byte(ctx, f);
		switch (c)
		{
		case EOF:
			return PDF_TOK_ERROR;
		case '(':
			bal++;
			*s++ = c;
			break;
		case ')':
			bal --;
			if (bal == 0)
				goto end;
			*s++ = c;
			break;
		case '\\':
			c = lex_byte(ctx, f);
			switch (c)
			{
			case EOF:
				return PDF_TOK_ERROR;
			case 'n':
				*s++ = '\n';
				break;
			case 'r':
				*s++ = '\r';
				break;
			case 't':
				*s++ = '\t';
				break;
			case 'b':
				*s++ = '\b';
				break;
			case 'f':
				*s++ = '\f';
				break;
			case '(':
				*s++ = '(';
				break;
			case ')':
				*s++ = ')';
				break;
			case '\\':
				*s++ = '\\';
				break;
			case RANGE_0_7:
				oct = c - '0';
				c = lex_byte(ctx, f);
				if (c >= '0' && c <= '7')
				{
					oct = oct * 8 + (c - '0');
					c = lex_byte(ctx, f);
					if (c >= '0' && c <= '7')
						oct = oct * 8 + (c - '0');
					else if (c != EOF)
						fz_unread_byte(ctx, f);
				}
				else if (c != EOF)
					fz_unread_byte(ctx, f);
				*s++ = oct;
				break;
			case '\n':
				break;
			case '\r':
				c = lex_byte(ctx, f);
				if ((c != '\n') && (c != EOF))
					fz_unread_byte(ctx, f);
				break;
			default:
				*s++ = c;
			}
			break;
		default:
			*s++ = c;
			break;
		}
	}
end:
	lb->len = s - lb->scratch;
	return PDF_TOK_STRING;
}

static int
lex_hex_string(fz_context *ctx, fz_stream *f, pdf_lexbuf *lb)
{
	char *s = lb->scratch;
	char *e = s + lb->size;
	int a = 0, x = 0;
	int c;

	while (1)
	{
		if (s == e)
		{
			s += pdf_lexbuf_grow(ctx, lb);
			e = lb->scratch + lb->size;
		}
		c = lex_byte(ctx, f);
		switch (c)
		{
		case IS_WHITE:
			break;
		default:
			fz_warn(ctx, "invalid character in hex string");
			/* fall through */
		case IS_HEX:
			if (x)
			{
				*s++ = a * 16 + unhex(c);
				x = !x;
			}
			else
			{
				a = unhex(c);
				x = !x;
			}
			break;
		case '>':
			if (x)
			{
				*s++ = a * 16; /* pad truncated string with '0' */
			}
			goto end;
		case EOF:
			return PDF_TOK_ERROR;
		}
	}
end:
	lb->len = s - lb->scratch;
	return PDF_TOK_STRING;
}

static pdf_token
pdf_token_from_keyword(char *key)
{
	switch (*key)
	{
	case 'R':
		if (!strcmp(key, "R")) return PDF_TOK_R;
		break;
	case 't':
		if (!strcmp(key, "true")) return PDF_TOK_TRUE;
		if (!strcmp(key, "trailer")) return PDF_TOK_TRAILER;
		break;
	case 'f':
		if (!strcmp(key, "false")) return PDF_TOK_FALSE;
		break;
	case 'n':
		if (!strcmp(key, "null")) return PDF_TOK_NULL;
		break;
	case 'o':
		if (!strcmp(key, "obj")) return PDF_TOK_OBJ;
		break;
	case 'e':
		if (!strcmp(key, "endobj")) return PDF_TOK_ENDOBJ;
		if (!strcmp(key, "endstream")) return PDF_TOK_ENDSTREAM;
		break;
	case 's':
		if (!strcmp(key, "stream")) return PDF_TOK_STREAM;
		if (!strcmp(key, "startxref")) return PDF_TOK_STARTXREF;
		break;
	case 'x':
		if (!strcmp(key, "xref")) return PDF_TOK_XREF;
		break;
	}

	while (*key)
	{
		if (!fz_isprint(*key))
			return PDF_TOK_ERROR;
		++key;
	}

	return PDF_TOK_KEYWORD;
}

void pdf_lexbuf_init(fz_context *ctx, pdf_lexbuf *lb, int size)
{
	lb->size = lb->base_size = size;
	lb->len = 0;
	lb->scratch = &lb->buffer[0];
}

void pdf_lexbuf_fin(fz_context *ctx, pdf_lexbuf *lb)
{
	if (lb && lb->size != lb->base_size)
		fz_free(ctx, lb->scratch);
}

ptrdiff_t pdf_lexbuf_grow(fz_context *ctx, pdf_lexbuf *lb)
{
	char *old = lb->scratch;
	size_t newsize = lb->size * 2;
	if (lb->size == lb->base_size)
	{
		lb->scratch = Memento_label(fz_malloc(ctx, newsize), "pdf_lexbuf");
		memcpy(lb->scratch, lb->buffer, lb->size);
	}
	else
	{
		lb->scratch = fz_realloc(ctx, lb->scratch, newsize);
	}
	lb->size = newsize;
	return lb->scratch - old;
}

pdf_token
pdf_lex(fz_context *ctx, fz_stream *f, pdf_lexbuf *buf)
{
	while (1)
	{
		int c = lex_byte(ctx, f);
		switch (c)
		{
		case EOF:
			return PDF_TOK_EOF;
		case IS_WHITE:
			lex_white(ctx, f);
			break;
		case '%':
			lex_comment(ctx, f);
			break;
		case '/':
			lex_name(ctx, f, buf);
			return PDF_TOK_NAME;
		case '(':
			return lex_string(ctx, f, buf);
		case ')':
			return PDF_TOK_ERROR;
		case '<':
			c = lex_byte(ctx, f);
			if (c == '<')
				return PDF_TOK_OPEN_DICT;
			if (c != EOF)
				fz_unread_byte(ctx, f);
			return lex_hex_string(ctx, f, buf);
		case '>':
			c = lex_byte(ctx, f);
			if (c == '>')
				return PDF_TOK_CLOSE_DICT;
			if (c != EOF)
				fz_unread_byte(ctx, f);
			return PDF_TOK_ERROR;
		case '[':
			return PDF_TOK_OPEN_ARRAY;
		case ']':
			return PDF_TOK_CLOSE_ARRAY;
		case '{':
			return PDF_TOK_OPEN_BRACE;
		case '}':
			return PDF_TOK_CLOSE_BRACE;
		case IS_NUMBER:
			return lex_number(ctx, f, buf, c);
		default: /* isregular: !isdelim && !iswhite && c != EOF */
			fz_unread_byte(ctx, f);
			lex_name(ctx, f, buf);
			return pdf_token_from_keyword(buf->scratch);
		}
	}
}

pdf_token
pdf_lex_no_string(fz_context *ctx, fz_stream *f, pdf_lexbuf *buf)
{
	while (1)
	{
		int c = lex_byte(ctx, f);
		switch (c)
		{
		case EOF:
			return PDF_TOK_EOF;
		case IS_WHITE:
			lex_white(ctx, f);
			break;
		case '%':
			lex_comment(ctx, f);
			break;
		case '/':
			lex_name(ctx, f, buf);
			return PDF_TOK_NAME;
		case '(':
			return PDF_TOK_ERROR; /* no strings allowed */
		case ')':
			return PDF_TOK_ERROR; /* no strings allowed */
		case '<':
			c = lex_byte(ctx, f);
			if (c == '<')
				return PDF_TOK_OPEN_DICT;
			if (c != EOF)
				fz_unread_byte(ctx, f);
			return PDF_TOK_ERROR; /* no strings allowed */
		case '>':
			c = lex_byte(ctx, f);
			if (c == '>')
				return PDF_TOK_CLOSE_DICT;
			if (c != EOF)
				fz_unread_byte(ctx, f);
			return PDF_TOK_ERROR;
		case '[':
			return PDF_TOK_OPEN_ARRAY;
		case ']':
			return PDF_TOK_CLOSE_ARRAY;
		case '{':
			return PDF_TOK_OPEN_BRACE;
		case '}':
			return PDF_TOK_CLOSE_BRACE;
		case IS_NUMBER:
			return lex_number(ctx, f, buf, c);
		default: /* isregular: !isdelim && !iswhite && c != EOF */
			fz_unread_byte(ctx, f);
			lex_name(ctx, f, buf);
			return pdf_token_from_keyword(buf->scratch);
		}
	}
}

/*
	print a lexed token to a buffer, growing if necessary
*/
void pdf_append_token(fz_context *ctx, fz_buffer *fzbuf, int tok, pdf_lexbuf *buf)
{
	switch (tok)
	{
	case PDF_TOK_NAME:
		fz_append_printf(ctx, fzbuf, "/%s", buf->scratch);
		break;
	case PDF_TOK_STRING:
		if (buf->len >= buf->size)
			pdf_lexbuf_grow(ctx, buf);
		buf->scratch[buf->len] = 0;
		fz_append_pdf_string(ctx, fzbuf, buf->scratch);
		break;
	case PDF_TOK_OPEN_DICT:
		fz_append_string(ctx, fzbuf, "<<");
		break;
	case PDF_TOK_CLOSE_DICT:
		fz_append_string(ctx, fzbuf, ">>");
		break;
	case PDF_TOK_OPEN_ARRAY:
		fz_append_byte(ctx, fzbuf, '[');
		break;
	case PDF_TOK_CLOSE_ARRAY:
		fz_append_byte(ctx, fzbuf, ']');
		break;
	case PDF_TOK_OPEN_BRACE:
		fz_append_byte(ctx, fzbuf, '{');
		break;
	case PDF_TOK_CLOSE_BRACE:
		fz_append_byte(ctx, fzbuf, '}');
		break;
	case PDF_TOK_INT:
		fz_append_printf(ctx, fzbuf, "%ld", buf->i);
		break;
	case PDF_TOK_REAL:
		fz_append_printf(ctx, fzbuf, "%g", buf->f);
		break;
	default:
		fz_append_data(ctx, fzbuf, buf->scratch, buf->len);
		break;
	}
}
