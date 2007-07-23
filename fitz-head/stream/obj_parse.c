#include "fitz-base.h"
#include "fitz-stream.h"

struct vap { va_list ap; };

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

static fz_error *parseobj(fz_obj **obj, char **sp, struct vap *v);

static inline int fromhex(char ch)
{
    if (ch >= '0' && ch <= '9')
        return  ch - '0';
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

static fz_error *parsename(fz_obj **obj, char **sp)
{
    fz_error *error;
    char buf[64];
    char *s = *sp;
    char *p = buf;

    s ++;		/* skip '/' */
    while (p < buf + sizeof buf - 1 && isregular(*s))
        *p++ = *s++;
    *p++ = 0;
    *sp = s;

    error = fz_newname(obj, buf);
    if (error)
        return fz_rethrow(error, "cannot create name");
    return fz_okay;
}

static fz_error *parsenumber(fz_obj **obj, char **sp)
{
    fz_error *error;
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
        error = fz_newreal(obj, atof(buf));
    else
        error = fz_newint(obj, atoi(buf));

    if (error)
        return fz_rethrow(error, "cannot parse number");
    return fz_okay;
}

static fz_error *parsedict(fz_obj **obj, char **sp, struct vap *v)
{
    fz_error *error = nil;
    fz_obj *dict = nil;
    fz_obj *key = nil;
    fz_obj *val = nil;
    char *s = *sp;

    error = fz_newdict(&dict, 8);
    if (error)
        return fz_rethrow(error, "cannot create dict");

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

        error = parsename(&key, &s);
        if (error)
        {
            error = fz_rethrow(error, "cannot parse key");
            goto cleanup;
        }

        skipwhite(&s);

        error = parseobj(&val, &s, v);
        if (error)
        {
            error = fz_rethrow(error, "cannot parse value");
            goto cleanup;
        }

        error = fz_dictput(dict, key, val);
        if (error)
        {
            error = fz_rethrow(error, "cannot insert dict entry");
            goto cleanup;
        }

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
    return error;
}

static fz_error *parsearray(fz_obj **obj, char **sp, struct vap *v)
{
    fz_error *error;
    fz_obj *a;
    fz_obj *o;
    char *s = *sp;

    error = fz_newarray(&a, 8);
    if (error)
        return fz_rethrow(error, "cannot create array");

    s ++;	/* skip '[' */

    while (*s)
    {
        skipwhite(&s);

        if (*s == ']')
        {
            s ++;
            break;
        }

        error = parseobj(&o, &s, v);
        if (error)
        {
            fz_dropobj(a);
            return fz_rethrow(error, "cannot parse item");
        }

        error = fz_arraypush(a, o);
        if (error)
        {
            fz_dropobj(o);
            fz_dropobj(a);
            return fz_rethrow(error, "cannot add item to array");
        }

        fz_dropobj(o);
    }

    *obj = a;
    *sp = s;
    return fz_okay;
}

static fz_error *parsestring(fz_obj **obj, char **sp)
{
    fz_error *error;
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

    error = fz_newstring(obj, buf, p - buf - 1);
    if (error)
        return fz_rethrow(error, "cannot create string");
    return fz_okay;
}

static fz_error *parsehexstring(fz_obj **obj, char **sp)
{
    fz_error *error;
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
    error = fz_newstring(obj, buf, p - buf);
    if (error)
        return fz_rethrow(error, "cannot create string");
    return fz_okay;
}

static fz_error *parseobj(fz_obj **obj, char **sp, struct vap *v)
{
    fz_error *error;
    char buf[32];
    int oid, gid, len;
    char *tmp;
    char *s = *sp;

    if (*s == '\0')
        return fz_throw("end of data");

    skipwhite(&s);

    error = nil;

    if (v != nil && *s == '%')
    {
        s ++;

        switch (*s)
        {
        case 'p': error = fz_newpointer(obj, va_arg(v->ap, void*)); break;
        case 'o': *obj = fz_keepobj(va_arg(v->ap, fz_obj*)); break;
        case 'b': error = fz_newbool(obj, va_arg(v->ap, int)); break;
        case 'i': error = fz_newint(obj, va_arg(v->ap, int)); break;
        case 'f': error = fz_newreal(obj, (float)va_arg(v->ap, double)); break;
        case 'n': error = fz_newname(obj, va_arg(v->ap, char*)); break;
        case 'r':
            oid = va_arg(v->ap, int);
            gid = va_arg(v->ap, int);
            error = fz_newindirect(obj, oid, gid);
            break;
        case 's':
            tmp = va_arg(v->ap, char*);
            error = fz_newstring(obj, tmp, strlen(tmp));
            break;
        case '#':
            tmp = va_arg(v->ap, char*);
            len = va_arg(v->ap, int);
            error = fz_newstring(obj, tmp, len);
            break;
        default:
            error = fz_throw("unknown format specifier in packobj: '%c'", *s);
            break;
        }

        if (error)
            error = fz_rethrow(error, "cannot create object for %% format");

        s ++;
    }

    else if (*s == '/')
    {
        error = parsename(obj, &s);
        if (error)
            error = fz_rethrow(error, "cannot parse name");
    }

    else if (*s == '(')
    {
        error = parsestring(obj, &s);
        if (error)
            error = fz_rethrow(error, "cannot parse string");
    }

    else if (*s == '<')
    {
        if (s[1] == '<')
        {
            error = parsedict(obj, &s, v);
            if (error)
                error = fz_rethrow(error, "cannot parse dict");
        }
        else
        {
            error = parsehexstring(obj, &s);
            if (error)
                error = fz_rethrow(error, "cannot parse hex string");
        }
    }

    else if (*s == '[')
    {
        error = parsearray(obj, &s, v);
        if (error)
            error = fz_rethrow(error, "cannot parse array");
    }

    else if (*s == '-' || *s == '.' || (*s >= '0' && *s <= '9'))
    {
        error = parsenumber(obj, &s);
        if (error)
            error = fz_rethrow(error, "cannot parse number");
    }

    else if (isregular(*s))
    {
        parsekeyword(&s, buf, buf + sizeof buf);

        if (strcmp("true", buf) == 0)
        {
            error = fz_newbool(obj, 1);
            if (error)
                error = fz_rethrow(error, "cannot create bool (true)");
        }
        else if (strcmp("false", buf) == 0)
        {
            error = fz_newbool(obj, 0);
            if (error)
                error = fz_rethrow(error, "cannot create bool (false)");
        }
        else if (strcmp("null", buf) == 0)
        {
            error = fz_newnull(obj);
            if (error)
                error = fz_rethrow(error, "cannot create null object");
        }
        else
            error = fz_throw("undefined keyword %s", buf);
    }

    else
        error = fz_throw("syntax error: unknown byte 0x%d", *s);

    *sp = s;
    return error;
}

fz_error *
fz_packobj(fz_obj **op, char *fmt, ...)
{
    fz_error *error;
    struct vap v;
    va_list ap;

    va_start(ap, fmt);
    va_copy(v.ap, ap);

    error = parseobj(op, &fmt, &v);
    if (error)
        error = fz_rethrow(error, "cannot parse object");

    va_end(ap);

    return error;
}

fz_error *
fz_parseobj(fz_obj **op, char *str)
{
    return parseobj(op, &str, nil);
}

