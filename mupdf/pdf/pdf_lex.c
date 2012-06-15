#include "fitz-internal.h"
#include "mupdf-internal.h"

#define IS_NUMBER \
	'+':case'-':case'.':case'0':case'1':case'2':case'3':\
	case'4':case'5':case'6':case'7':case'8':case'9'
#define IS_WHITE \
	'\000':case'\011':case'\012':case'\014':case'\015':case'\040'
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

static inline int unhex(int ch)
{
	if (ch >= '0' && ch <= '9') return ch - '0';
	if (ch >= 'A' && ch <= 'F') return ch - 'A' + 0xA;
	if (ch >= 'a' && ch <= 'f') return ch - 'a' + 0xA;
	return 0;
}

static void
lex_white(fz_stream *f)
{
	int c;
	do {
		c = fz_read_byte(f);
	} while ((c <= 32) && (iswhite(c)));
	if (c != EOF)
		fz_unread_byte(f);
}

static void
lex_comment(fz_stream *f)
{
	int c;
	do {
		c = fz_read_byte(f);
	} while ((c != '\012') && (c != '\015') && (c != EOF));
}

static int
lex_number(fz_stream *f, pdf_lexbuf *buf, int c)
{
	int neg = 0;
	int i = 0;
	int n;
	int d;
	float v;

	/* Initially we might have +, -, . or a digit */
	switch (c)
	{
	case '.':
		goto loop_after_dot;
	case '-':
		neg = 1;
		break;
	case '+':
		break;
	default: /* Must be a digit */
		i = c - '0';
		break;
	}

	while (1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case '.':
			goto loop_after_dot;
		case RANGE_0_9:
			i = 10*i + c - '0';
			/* FIXME: Need overflow check here; do we care? */
			break;
		default:
			fz_unread_byte(f);
			/* Fallthrough */
		case EOF:
			if (neg)
				i = -i;
			buf->i = i;
			return PDF_TOK_INT;
		}
	}

	/* In here, we've seen a dot, so can accept just digits */
loop_after_dot:
	n = 0;
	d = 1;
	while (1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case RANGE_0_9:
			if (d >= INT_MAX/10)
				goto underflow;
			n = n*10 + (c - '0');
			d *= 10;
			break;
		default:
			fz_unread_byte(f);
			/* Fallthrough */
		case EOF:
			v = (float)i + ((float)n / (float)d);
			if (neg)
				v = -v;
			buf->f = v;
			return PDF_TOK_REAL;
		}
	}

underflow:
	/* Ignore any digits after here, because they are too small */
	while (1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case RANGE_0_9:
			break;
		default:
			fz_unread_byte(f);
			/* Fallthrough */
		case EOF:
			v = (float)i + ((float)n / (float)d);
			if (neg)
				v = -v;
			buf->f = v;
			return PDF_TOK_REAL;
		}
	}
}

static void
lex_name(fz_stream *f, pdf_lexbuf *buf)
{
	char *s = buf->scratch;
	int n = buf->size;

	while (n > 1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case IS_WHITE:
		case IS_DELIM:
			fz_unread_byte(f);
			goto end;
		case EOF:
			goto end;
		case '#':
		{
			int d;
			c = fz_read_byte(f);
			switch (c)
			{
			case RANGE_0_9:
				d = (c - '0') << 4;
				break;
			case RANGE_a_f:
				d = (c - 'a' + 10) << 4;
				break;
			case RANGE_A_F:
				d = (c - 'A' + 10) << 4;
				break;
			default:
				fz_unread_byte(f);
				/* fallthrough */
			case EOF:
				goto end;
			}
			c = fz_read_byte(f);
			switch (c)
			{
			case RANGE_0_9:
				c -= '0';
				break;
			case RANGE_a_f:
				c -= 'a' - 10;
				break;
			case RANGE_A_F:
				c -= 'A' - 10;
				break;
			default:
				fz_unread_byte(f);
				/* fallthrough */
			case EOF:
				*s++ = d;
				n--;
				goto end;
			}
			*s++ = d + c;
			n--;
			break;
		}
		default:
			*s++ = c;
			n--;
			break;
		}
	}
end:
	*s = '\0';
	buf->len = s - buf->scratch;
}

static int
lex_string(fz_stream *f, char *buf, int n)
{
	char *s = buf;
	char *e = buf + n;
	int bal = 1;
	int oct;
	int c;

	while (s < e)
	{
		c = fz_read_byte(f);
		switch (c)
		{
		case EOF:
			goto end;
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
			c = fz_read_byte(f);
			switch (c)
			{
			case EOF:
				goto end;
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
			case RANGE_0_9:
				oct = c - '0';
				c = fz_read_byte(f);
				if (c >= '0' && c <= '9')
				{
					oct = oct * 8 + (c - '0');
					c = fz_read_byte(f);
					if (c >= '0' && c <= '9')
						oct = oct * 8 + (c - '0');
					else if (c != EOF)
						fz_unread_byte(f);
				}
				else if (c != EOF)
					fz_unread_byte(f);
				*s++ = oct;
				break;
			case '\n':
				break;
			case '\r':
				c = fz_read_byte(f);
				if ((c != '\n') && (c != EOF))
					fz_unread_byte(f);
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
	return s - buf;
}

static int
lex_hex_string(fz_stream *f, char *buf, int n)
{
	char *s = buf;
	char *e = buf + n;
	int a = 0, x = 0;
	int c;

	while (s < e)
	{
		c = fz_read_byte(f);
		switch (c)
		{
		case IS_WHITE:
			break;
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
		case EOF:
			goto end;
		default:
			fz_warn(f->ctx, "ignoring invalid character in hex string: '%c'", c);
		}
	}
end:
	return s - buf;
}

static int
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
	default:
		break;
	}

	return PDF_TOK_KEYWORD;
}

int
pdf_lex(fz_stream *f, pdf_lexbuf *buf)
{
	while (1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case EOF:
			return PDF_TOK_EOF;
		case IS_WHITE:
			lex_white(f);
			break;
		case '%':
			lex_comment(f);
			break;
		case '/':
			lex_name(f, buf);
			return PDF_TOK_NAME;
		case '(':
			buf->len = lex_string(f, buf->scratch, buf->size);
			return PDF_TOK_STRING;
		case ')':
			fz_warn(f->ctx, "lexical error (unexpected ')')");
			continue;
		case '<':
			c = fz_read_byte(f);
			if (c == '<')
			{
				return PDF_TOK_OPEN_DICT;
			}
			else
			{
				fz_unread_byte(f);
				buf->len = lex_hex_string(f, buf->scratch, buf->size);
				return PDF_TOK_STRING;
			}
		case '>':
			c = fz_read_byte(f);
			if (c == '>')
			{
				return PDF_TOK_CLOSE_DICT;
			}
			fz_warn(f->ctx, "lexical error (unexpected '>')");
			continue;
		case '[':
			return PDF_TOK_OPEN_ARRAY;
		case ']':
			return PDF_TOK_CLOSE_ARRAY;
		case '{':
			return PDF_TOK_OPEN_BRACE;
		case '}':
			return PDF_TOK_CLOSE_BRACE;
		case IS_NUMBER:
			return lex_number(f, buf, c);
		default: /* isregular: !isdelim && !iswhite && c != EOF */
			fz_unread_byte(f);
			lex_name(f, buf);
			return pdf_token_from_keyword(buf->scratch);
		}
	}
}

void pdf_print_token(fz_context *ctx, fz_buffer *fzbuf, int tok, pdf_lexbuf *buf)
{
	switch(tok)
	{
	case PDF_TOK_NAME:
		fz_buffer_printf(ctx, fzbuf, "/%s", buf->scratch);
		break;
	case PDF_TOK_STRING:
		{
			int i;
			fz_buffer_printf(ctx, fzbuf, "<");
			for (i = 0; i < buf->len; i++)
				fz_buffer_printf(ctx, fzbuf, "%02X", buf->scratch[i]);
			fz_buffer_printf(ctx, fzbuf, ">");
		}
		break;
	case PDF_TOK_OPEN_DICT:
		fz_buffer_printf(ctx, fzbuf, "<<");
		break;
	case PDF_TOK_CLOSE_DICT:
		fz_buffer_printf(ctx, fzbuf, ">>");
		break;
	case PDF_TOK_OPEN_ARRAY:
		fz_buffer_printf(ctx, fzbuf, "[");
		break;
	case PDF_TOK_CLOSE_ARRAY:
		fz_buffer_printf(ctx, fzbuf, "]");
		break;
	case PDF_TOK_OPEN_BRACE:
		fz_buffer_printf(ctx, fzbuf, "{");
		break;
	case PDF_TOK_CLOSE_BRACE:
		fz_buffer_printf(ctx, fzbuf, "}");
		break;
	case PDF_TOK_INT:
		fz_buffer_printf(ctx, fzbuf, "%d", buf->i);
		break;
	case PDF_TOK_REAL:
		{
			char sbuf[256];
			sprintf(sbuf, "%g", buf->f);
			if (strchr(sbuf, 'e')) /* bad news! */
				sprintf(sbuf, fabsf(buf->f) > 1 ? "%1.1f" : "%1.8f", buf->f);
			fz_buffer_printf(ctx, fzbuf, "%s", sbuf);
		}
		break;
	default:
		fz_buffer_printf(ctx, fzbuf, "%s", buf->scratch);
		break;
	}
}
