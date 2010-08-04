#include "fitz.h"
#include "mupdf.h"

#define ISNUMBER \
	'+':case'-':case'.':case'0':case'1':case'2':case'3':\
	case'4':case'5':case'6':case'7':case'8':case'9'
#define ISWHITE \
	'\000':case'\011':case'\012':case'\014':case'\015':case'\040'
#define ISHEX \
	'0':case'1':case'2':case'3':case'4':case'5':case'6':\
	case'7':case'8':case'9':case'A':case'B':case'C':\
	case'D':case'E':case'F':case'a':case'b':case'c':\
	case'd':case'e':case'f'
#define ISDELIM \
	'(':case')':case'<':case'>':case'[':case']':case'{':\
	case'}':case'/':case'%'

#define RANGE_0_9 \
	'0':case'1':case'2':case'3':case'4':case'5':\
	case'6':case'7':case'8':case'9'
#define RANGE_a_f \
	'a':case'b':case'c':case'd':case'e':case'f'
#define RANGE_A_F \
	'A':case'B':case'C':case'D':case'E':case'F'

static inline int
iswhite(int ch)
{
	return
		ch == '\000' ||
		ch == '\011' ||
		ch == '\012' ||
		ch == '\014' ||
		ch == '\015' ||
		ch == '\040';
}

static inline int
fromhex(int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 0xA;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 0xA;
	return 0;
}

static inline void
lexwhite(fz_stream *f)
{
	int c;
	do
	{
		c = fz_readbyte(f);
	}
	while ((c <= 32) && (iswhite(c)));
	if (c != EOF)
		fz_unreadbyte(f);
}

static inline void
lexcomment(fz_stream *f)
{
	int c;
	do
	{
		c = fz_readbyte(f);
	}
	while ((c != '\012') && (c != '\015') && (c != EOF));
}

static int
lexnumber(fz_stream *f, char *s, int n, pdf_token_e *tok)
{
	char *buf = s;
	*tok = PDF_TINT;

	/* Initially we might have +, -, . or a digit */
	if (n > 1)
	{
		int c = fz_readbyte(f);
		switch (c)
		{
		case '.':
			*tok = PDF_TREAL;
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
			fz_unreadbyte(f);
			goto end;
		case EOF:
			goto end;
		}
	}

	/* We can't accept a sign from here on in, just . or a digit */
loop_after_sign:
	while (n > 1)
	{
		int c = fz_readbyte(f);
		switch (c)
		{
		case '.':
			*tok = PDF_TREAL;
			*s++ = c;
			n--;
			goto loop_after_dot;
		case RANGE_0_9:
			*s++ = c;
			break;
		default:
			fz_unreadbyte(f);
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
		int c = fz_readbyte(f);
		switch (c)
		{
		case RANGE_0_9:
			*s++ = c;
			break;
		default:
			fz_unreadbyte(f);
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
lexname(fz_stream *f, char *s, int n)
{
	while (n > 1)
	{
		int c = fz_readbyte(f);
		switch (c)
		{
		case ISWHITE:
		case ISDELIM:
			fz_unreadbyte(f);
			goto end;
		case EOF:
			goto end;
		case '#':
		{
			int d;
			c = fz_readbyte(f);
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
				fz_unreadbyte(f);
				/* fallthrough */
			case EOF:
				goto end;
			}
			c = fz_readbyte(f);
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
				fz_unreadbyte(f);
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
lexstring(fz_stream *f, char *buf, int n)
{
	char *s = buf;
	char *e = buf + n;
	int bal = 1;
	int oct;
	int c;

	while (s < e)
	{
		c = fz_readbyte(f);
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
			c = fz_readbyte(f);
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
				c = fz_readbyte(f);
				if (c >= '0' && c <= '9')
				{
					oct = oct * 8 + (c - '0');
					c = fz_readbyte(f);
					if (c >= '0' && c <= '9')
						oct = oct * 8 + (c - '0');
					else if (c != EOF)
						fz_unreadbyte(f);
				}
				else if (c != EOF)
					fz_unreadbyte(f);
				*s++ = oct;
				break;
			case '\n':
				break;
			case '\r':
				c = fz_readbyte(f);
				if ((c != '\n') && (c != EOF))
					fz_unreadbyte(f);
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
lexhexstring(fz_stream *f, char *buf, int n)
{
	char *s = buf;
	char *e = buf + n;
	int a = 0, x = 0;
	int c;

	while (s < e)
	{
		c = fz_readbyte(f);
		switch (c)
		{
		case ISWHITE:
			break;
		case ISHEX:
			if (x)
			{
				*s++ = a * 16 + fromhex(c);
				x = !x;
			}
			else
			{
				a = fromhex(c);
				x = !x;
			}
			break;
		case '>':
		// cf. http://code.google.com/p/sumatrapdf/issues/detail?id=624
		case EOF:
			goto end;
		default:
			fz_warn("Ignoring invalid character in hexstring: %c", c);
		}
	}
end:
	return s - buf;
}

static pdf_token_e
pdf_tokenfromkeyword(char *key)
{
	switch (*key)
	{
	case 'R':
		if (!strcmp(key, "R")) return PDF_TR;
		break;
	case 't':
		if (!strcmp(key, "true")) return PDF_TTRUE;
		if (!strcmp(key, "trailer")) return PDF_TTRAILER;
		break;
	case 'f':
		if (!strcmp(key, "false")) return PDF_TFALSE;
		break;
	case 'n':
		if (!strcmp(key, "null")) return PDF_TNULL;
		break;
	case 'o':
		if (!strcmp(key, "obj")) return PDF_TOBJ;
		break;
	case 'e':
		if (!strcmp(key, "endobj")) return PDF_TENDOBJ;
		if (!strcmp(key, "endstream")) return PDF_TENDSTREAM;
		break;
	case 's':
		if (!strcmp(key, "stream")) return PDF_TSTREAM;
		if (!strcmp(key, "startxref")) return PDF_TSTARTXREF;
		break;
	case 'x':
		if (!strcmp(key, "xref")) return PDF_TXREF;
		break;
	default:
		break;
	}

	return PDF_TKEYWORD;
}

fz_error
pdf_lex(pdf_token_e *tok, fz_stream *f, char *buf, int n, int *sl)
{
	while (1)
	{
		int c = fz_readbyte(f);
		switch (c)
		{
		case EOF:
			*tok = PDF_TEOF;
			return fz_okay;
		case ISWHITE:
			lexwhite(f);
			break;
		case '%':
			lexcomment(f);
			break;
		case '/':
			lexname(f, buf, n);
			*sl = strlen(buf);
			*tok = PDF_TNAME;
			return fz_okay;
		case '(':
			*sl = lexstring(f, buf, n);
			*tok = PDF_TSTRING;
			return fz_okay;
		case ')':
			*tok = PDF_TERROR;
			goto cleanuperror;
		case '<':
			c = fz_readbyte(f);
			if (c == '<')
			{
				*tok = PDF_TODICT;
			}
			else
			{
				fz_unreadbyte(f);
				*sl = lexhexstring(f, buf, n);
				*tok = PDF_TSTRING;
			}
			return fz_okay;
		case '>':
			c = fz_readbyte(f);
			if (c == '>')
			{
				*tok = PDF_TCDICT;
				return fz_okay;
			}
			*tok = PDF_TERROR;
			goto cleanuperror;
		case '[':
			*tok = PDF_TOARRAY;
			return fz_okay;
		case ']':
			*tok = PDF_TCARRAY;
			return fz_okay;
		case '{':
			*tok = PDF_TOBRACE;
			return fz_okay;
		case '}':
			*tok = PDF_TCBRACE;
			return fz_okay;
		case ISNUMBER:
			fz_unreadbyte(f);
			*sl = lexnumber(f, buf, n, tok);
			return fz_okay;
		default: /* isregular: !isdelim && !iswhite && c != EOF */
			fz_unreadbyte(f);
			lexname(f, buf, n);
			*sl = strlen(buf);
			*tok = pdf_tokenfromkeyword(buf);
			return fz_okay;
		}
	}

cleanuperror:
	*tok = PDF_TERROR;
	return fz_throw("lexical error");
}
