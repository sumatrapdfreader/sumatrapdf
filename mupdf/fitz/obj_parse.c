#include "fitz_base.h"
#include "fitz_stream.h"

struct vap { va_list ap; };

static fz_error parseobj(fz_obj **obj, pdf_xref *xref, char **sp, struct vap *v);

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

static inline int isdelim(int ch)
{
	return
		ch == '(' || ch == ')' ||
		ch == '<' || ch == '>' ||
		ch == '[' || ch == ']' ||
		ch == '{' || ch == '}' ||
		ch == '/' ||
		ch == '%';
}

static inline int isregular(int ch)
{
	return !isdelim(ch) && !iswhite(ch) && ch != EOF;
}

static inline int fromhex(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 0xA;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 0xA;
	return 0;
}

static inline void skipwhite(char **sp)
{
	char *s = *sp;
	while (iswhite(*s))
		s ++;
	*sp = s;
}

static void parsekeyword(char **sp, char *b, char *eb)
{
	char *s = *sp;
	while (b < eb && isregular(*s))
		*b++ = *s++;
	*b++ = 0;
	*sp = s;
}

static fz_obj * parsename(char **sp)
{
	char buf[64];
	char *s = *sp;
	char *p = buf;

	s ++;		/* skip '/' */
	while (p < buf + sizeof buf - 1 && isregular(*s))
		*p++ = *s++;
	*p++ = 0;
	*sp = s;

	return fz_newname(buf);
}

static fz_obj * parsenumber(char **sp)
{
	char buf[32];
	char *s = *sp;
	char *p = buf;

	while (p < buf + sizeof buf - 1)
	{
		if (s[0] == '-' || s[0] == '.' || (s[0] >= '0' && s[0] <= '9'))
			*p++ = *s++;
		else
			break;
	}
	*p++ = 0;
	*sp = s;

	if (strchr(buf, '.'))
		return fz_newreal(atof(buf));
	return fz_newint(atoi(buf));
}

static fz_error parsedict(fz_obj **obj, pdf_xref *xref, char **sp, struct vap *v)
{
	fz_error error;
	fz_obj *dict = nil;
	fz_obj *key = nil;
	fz_obj *val = nil;
	char *s = *sp;

	dict = fz_newdict(8);

	s += 2;	/* skip "<<" */

	while (*s)
	{
		skipwhite(&s);

		/* end-of-dict marker >> */
		if (*s == '>')
		{
			s ++;
			if (*s == '>')
			{
				s ++;
				break;
			}
			error = fz_throw("malformed >> marker");
			goto cleanup;
		}

		/* non-name as key, bail */
		if (*s != '/')
		{
			error = fz_throw("key is not a name");
			goto cleanup;
		}

		key = parsename(&s);

		skipwhite(&s);

		error = parseobj(&val, xref, &s, v);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse value");
			goto cleanup;
		}

		fz_dictput(dict, key, val);

		fz_dropobj(val); val = nil;
		fz_dropobj(key); key = nil;
	}

	*obj = dict;
	*sp = s;
	return fz_okay;

cleanup:
	if (val) fz_dropobj(val);
	if (key) fz_dropobj(key);
	if (dict) fz_dropobj(dict);
	*obj = nil;
	*sp = s;
	return error; /* already rethrown */
}

static fz_error parsearray(fz_obj **obj, pdf_xref *xref, char **sp, struct vap *v)
{
	fz_error error;
	fz_obj *a;
	fz_obj *o;
	char *s = *sp;

	a = fz_newarray(8);

	s ++;	/* skip '[' */

	while (*s)
	{
		skipwhite(&s);

		if (*s == ']')
		{
			s ++;
			break;
		}

		error = parseobj(&o, xref, &s, v);
		if (error)
		{
			fz_dropobj(a);
			return fz_rethrow(error, "cannot parse item");
		}

		fz_arraypush(a, o);

		fz_dropobj(o);
	}

	*obj = a;
	*sp = s;
	return fz_okay;
}

static fz_obj * parsestring(char **sp)
{
	char buf[512];
	char *s = *sp;
	char *p = buf;
	int balance = 1;
	int oct;

	s ++;	/* skip '(' */

	while (*s && p < buf + sizeof buf)
	{
		if (*s == '(')
		{
			balance ++;
			*p++ = *s++;
		}
		else if (*s == ')')
		{
			balance --;
			*p++ = *s++;
		}
		else if (*s == '\\')
		{
			s ++;
			if (*s >= '0' && *s <= '9')
			{
				oct = *s - '0';
				s ++;
				if (*s >= '0' && *s <= '9')
				{
					oct = oct * 8 + (*s - '0');
					s ++;
					if (*s >= '0' && *s <= '9')
					{
						oct = oct * 8 + (*s - '0');
						s ++;
					}
				}
				*p++ = oct;
			}
			else switch (*s)
			{
			case 'n': *p++ = '\n'; s++; break;
			case 'r': *p++ = '\r'; s++; break;
			case 't': *p++ = '\t'; s++; break;
			case 'b': *p++ = '\b'; s++; break;
			case 'f': *p++ = '\f'; s++; break;
			default: *p++ = *s++; break;
			}
		}
		else
			*p++ = *s++;

		if (balance == 0)
			break;
	}

	*sp = s;
	return fz_newstring(buf, p - buf - 1);
}

static fz_obj * parsehexstring(char **sp)
{
	char buf[512];
	char *s = *sp;
	char *p = buf;
	int a, b;

	s ++;		/* skip '<' */

	while (*s && p < buf + sizeof buf)
	{
		skipwhite(&s);
		if (*s == '>') {
			s ++;
			break;
		}
		a = *s++;

		if (*s == '\0')
			break;

		skipwhite(&s);
		if (*s == '>') {
			s ++;
			break;
		}
		b = *s++;

		*p++ = fromhex(a) * 16 + fromhex(b);
	}

	*sp = s;
	return fz_newstring(buf, p - buf);
}

static fz_error parseobj(fz_obj **obj, pdf_xref *xref, char **sp, struct vap *v)
{
	fz_error error;
	char buf[32];
	int num, gen, len;
	char *tmp;
	char *s = *sp;

	if (*s == '\0')
		return fz_throw("end of data");

	skipwhite(&s);

	if (v != nil && *s == '%')
	{
		s ++;

		switch (*s)
		{
		case 'o': *obj = fz_keepobj(va_arg(v->ap, fz_obj*)); break;
		case 'b': *obj = fz_newbool(va_arg(v->ap, int)); break;
		case 'i': *obj = fz_newint(va_arg(v->ap, int)); break;
		case 'f': *obj = fz_newreal((float)va_arg(v->ap, double)); break;
		case 'n': *obj = fz_newname(va_arg(v->ap, char*)); break;
		case 'r':
			num = va_arg(v->ap, int);
			gen = va_arg(v->ap, int);
			*obj = fz_newindirect(num, gen, xref);
			break;
		case 's':
			tmp = va_arg(v->ap, char*);
			*obj = fz_newstring(tmp, strlen(tmp));
			break;
		case '#':
			tmp = va_arg(v->ap, char*);
			len = va_arg(v->ap, int);
			*obj = fz_newstring(tmp, len);
			break;
		default:
			return fz_throw("unknown format specifier in packobj: '%c'", *s);
		}

		s ++;
	}

	else if (*s == '/')
	{
		*obj = parsename(&s);
	}

	else if (*s == '(')
	{
		*obj = parsestring(&s);
	}

	else if (*s == '<')
	{
		if (s[1] == '<')
		{
			error = parsedict(obj, xref, &s, v);
			if (error)
				return fz_rethrow(error, "cannot parse dict");
		}
		else
		{
			*obj = parsehexstring(&s);
		}
	}

	else if (*s == '[')
	{
		error = parsearray(obj, xref, &s, v);
		if (error)
			return fz_rethrow(error, "cannot parse array");
	}

	else if (*s == '-' || *s == '.' || (*s >= '0' && *s <= '9'))
	{
		*obj = parsenumber(&s);
	}

	else if (isregular(*s))
	{
		parsekeyword(&s, buf, buf + sizeof buf);

		if (strcmp("true", buf) == 0)
			*obj = fz_newbool(1);
		else if (strcmp("false", buf) == 0)
			*obj = fz_newbool(0);
		else if (strcmp("null", buf) == 0)
			*obj = fz_newnull();
		else
			return fz_throw("undefined keyword %s", buf);
	}

	else
		return fz_throw("syntax error: unknown byte 0x%d", *s);

	*sp = s;
	return fz_okay;
}

fz_error fz_packobj(fz_obj **op, pdf_xref *xref, char *fmt, ...)
{
	fz_error error;
	struct vap v;
	va_list ap;

	va_start(ap, fmt);
	va_copy(v.ap, ap);

	error = parseobj(op, xref, &fmt, &v);

	va_end(v.ap);
	va_end(ap);

	if (error)
		return fz_rethrow(error, "cannot parse object");
	return fz_okay;
}

fz_error fz_parseobj(fz_obj **op, pdf_xref *xref, char *str)
{
	return parseobj(op, xref, &str, nil);
}

