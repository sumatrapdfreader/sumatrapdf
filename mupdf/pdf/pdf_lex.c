#include "fitz.h"
#include "mupdf.h"

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
lex_number(fz_stream *f, char *s, int n, int *tok)
{
	char *buf = s;
	*tok = PDF_TOK_INT;

	/* Initially we might have +, -, . or a digit */
	if (n > 1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case '.':
			*tok = PDF_TOK_REAL;
			*s++ = c;
			n--;
			goto loop_after_dot;
		case '+':
		case '-':
		case RANGE_0_9:
			*s++ = c;
			n--;
			goto loop_after_sign;
		default:
			fz_unread_byte(f);
			goto end;
		case EOF:
			goto end;
		}
	}

	/* We can't accept a sign from here on in, just . or a digit */
loop_after_sign:
	while (n > 1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case '.':
			*tok = PDF_TOK_REAL;
			*s++ = c;
			n--;
			goto loop_after_dot;
		case RANGE_0_9:
			*s++ = c;
			break;
		default:
			fz_unread_byte(f);
			goto end;
		case EOF:
			goto end;
		}
		n--;
	}

	/* In here, we've seen a dot, so can accept just digits */
loop_after_dot:
	while (n > 1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case RANGE_0_9:
			*s++ = c;
			break;
		default:
			fz_unread_byte(f);
			goto end;
		case EOF:
			goto end;
		}
		n--;
	}

end:
	*s = '\0';
	return s-buf;
}

static void
lex_name(fz_stream *f, char *s, int n)
{
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
			fz_warn("ignoring invalid character in hex string: '%c'", c);
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

fz_error
pdf_lex(int *tok, fz_stream *f, char *buf, int n, int *sl)
{
	while (1)
	{
		int c = fz_read_byte(f);
		switch (c)
		{
		case EOF:
			*tok = PDF_TOK_EOF;
			return fz_okay;
		case IS_WHITE:
			lex_white(f);
			break;
		case '%':
			lex_comment(f);
			break;
		case '/':
			lex_name(f, buf, n);
			*sl = strlen(buf);
			*tok = PDF_TOK_NAME;
			return fz_okay;
		case '(':
			*sl = lex_string(f, buf, n);
			*tok = PDF_TOK_STRING;
			return fz_okay;
		case ')':
			*tok = PDF_TOK_ERROR;
			goto cleanuperror;
		case '<':
			c = fz_read_byte(f);
			if (c == '<')
			{
				*tok = PDF_TOK_OPEN_DICT;
			}
			else
			{
				fz_unread_byte(f);
				*sl = lex_hex_string(f, buf, n);
				*tok = PDF_TOK_STRING;
			}
			return fz_okay;
		case '>':
			c = fz_read_byte(f);
			if (c == '>')
			{
				*tok = PDF_TOK_CLOSE_DICT;
				return fz_okay;
			}
			*tok = PDF_TOK_ERROR;
			goto cleanuperror;
		case '[':
			*tok = PDF_TOK_OPEN_ARRAY;
			return fz_okay;
		case ']':
			*tok = PDF_TOK_CLOSE_ARRAY;
			return fz_okay;
		case '{':
			*tok = PDF_TOK_OPEN_BRACE;
			return fz_okay;
		case '}':
			*tok = PDF_TOK_CLOSE_BRACE;
			return fz_okay;
		case IS_NUMBER:
			fz_unread_byte(f);
			*sl = lex_number(f, buf, n, tok);
			return fz_okay;
		default: /* isregular: !isdelim && !iswhite && c != EOF */
			fz_unread_byte(f);
			lex_name(f, buf, n);
			*sl = strlen(buf);
			*tok = pdf_token_from_keyword(buf);
			return fz_okay;
		}
	}

cleanuperror:
	*tok = PDF_TOK_ERROR;
	return fz_throw("lexical error");
}
