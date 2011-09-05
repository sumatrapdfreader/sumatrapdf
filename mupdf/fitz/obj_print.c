#include "fitz.h"

struct fmt
{
	char *buf;
	int cap;
	int len;
	int indent;
	int tight;
	int col;
	int sep;
	int last;
};

static void fmt_obj(struct fmt *fmt, fz_obj *obj);

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
	return	ch == '(' || ch == ')' ||
		ch == '<' || ch == '>' ||
		ch == '[' || ch == ']' ||
		ch == '{' || ch == '}' ||
		ch == '/' ||
		ch == '%';
}

static inline void fmt_putc(struct fmt *fmt, int c)
{
	if (fmt->sep && !isdelim(fmt->last) && !isdelim(c)) {
		fmt->sep = 0;
		fmt_putc(fmt, ' ');
	}
	fmt->sep = 0;

	if (fmt->buf && fmt->len < fmt->cap)
		fmt->buf[fmt->len] = c;

	if (c == '\n')
		fmt->col = 0;
	else
		fmt->col ++;

	fmt->len ++;

	fmt->last = c;
}

static inline void fmt_indent(struct fmt *fmt)
{
	int i = fmt->indent;
	while (i--) {
		fmt_putc(fmt, ' ');
		fmt_putc(fmt, ' ');
	}
}

static inline void fmt_puts(struct fmt *fmt, char *s)
{
	while (*s)
		fmt_putc(fmt, *s++);
}

static inline void fmt_sep(struct fmt *fmt)
{
	fmt->sep = 1;
}

static void fmt_str(struct fmt *fmt, fz_obj *obj)
{
	char *s = fz_to_str_buf(obj);
	int n = fz_to_str_len(obj);
	int i, c;

	fmt_putc(fmt, '(');
	for (i = 0; i < n; i++)
	{
		c = (unsigned char)s[i];
		if (c == '\n')
			fmt_puts(fmt, "\\n");
		else if (c == '\r')
			fmt_puts(fmt, "\\r");
		else if (c == '\t')
			fmt_puts(fmt, "\\t");
		else if (c == '\b')
			fmt_puts(fmt, "\\b");
		else if (c == '\f')
			fmt_puts(fmt, "\\f");
		else if (c == '(')
			fmt_puts(fmt, "\\(");
		else if (c == ')')
			fmt_puts(fmt, "\\)");
		else if (c < 32 || c >= 127) {
			char buf[16];
			fmt_putc(fmt, '\\');
			sprintf(buf, "%03o", c);
			fmt_puts(fmt, buf);
		}
		else
			fmt_putc(fmt, c);
	}
	fmt_putc(fmt, ')');
}

static void fmt_hex(struct fmt *fmt, fz_obj *obj)
{
	char *s = fz_to_str_buf(obj);
	int n = fz_to_str_len(obj);
	int i, b, c;

	fmt_putc(fmt, '<');
	for (i = 0; i < n; i++) {
		b = (unsigned char) s[i];
		c = (b >> 4) & 0x0f;
		fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		c = (b) & 0x0f;
		fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
	}
	fmt_putc(fmt, '>');
}

static void fmt_name(struct fmt *fmt, fz_obj *obj)
{
	unsigned char *s = (unsigned char *) fz_to_name(obj);
	int i, c;

	fmt_putc(fmt, '/');

	for (i = 0; s[i]; i++)
	{
		if (isdelim(s[i]) || iswhite(s[i]) ||
			s[i] == '#' || s[i] < 32 || s[i] >= 127)
		{
			fmt_putc(fmt, '#');
			c = (s[i] >> 4) & 0xf;
			fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
			c = s[i] & 0xf;
			fmt_putc(fmt, c < 0xA ? c + '0' : c + 'A' - 0xA);
		}
		else
		{
			fmt_putc(fmt, s[i]);
		}
	}
}

static void fmt_array(struct fmt *fmt, fz_obj *obj)
{
	int i;

	if (fmt->tight) {
		fmt_putc(fmt, '[');
		for (i = 0; i < fz_array_len(obj); i++) {
			fmt_obj(fmt, fz_array_get(obj, i));
			fmt_sep(fmt);
		}
		fmt_putc(fmt, ']');
	}
	else {
		fmt_puts(fmt, "[ ");
		for (i = 0; i < fz_array_len(obj); i++) {
			if (fmt->col > 60) {
				fmt_putc(fmt, '\n');
				fmt_indent(fmt);
			}
			fmt_obj(fmt, fz_array_get(obj, i));
			fmt_putc(fmt, ' ');
		}
		fmt_putc(fmt, ']');
		fmt_sep(fmt);
	}
}

static void fmt_dict(struct fmt *fmt, fz_obj *obj)
{
	int i;
	fz_obj *key, *val;

	if (fmt->tight) {
		fmt_puts(fmt, "<<");
		for (i = 0; i < fz_dict_len(obj); i++) {
			fmt_obj(fmt, fz_dict_get_key(obj, i));
			fmt_sep(fmt);
			fmt_obj(fmt, fz_dict_get_val(obj, i));
			fmt_sep(fmt);
		}
		fmt_puts(fmt, ">>");
	}
	else {
		fmt_puts(fmt, "<<\n");
		fmt->indent ++;
		for (i = 0; i < fz_dict_len(obj); i++) {
			key = fz_dict_get_key(obj, i);
			val = fz_dict_get_val(obj, i);
			fmt_indent(fmt);
			fmt_obj(fmt, key);
			fmt_putc(fmt, ' ');
			if (!fz_is_indirect(val) && fz_is_array(val))
				fmt->indent ++;
			fmt_obj(fmt, val);
			fmt_putc(fmt, '\n');
			if (!fz_is_indirect(val) && fz_is_array(val))
				fmt->indent --;
		}
		fmt->indent --;
		fmt_indent(fmt);
		fmt_puts(fmt, ">>");
	}
}

static void fmt_obj(struct fmt *fmt, fz_obj *obj)
{
	char buf[256];

	if (!obj)
		fmt_puts(fmt, "<NULL>");
	else if (fz_is_indirect(obj))
	{
		sprintf(buf, "%d %d R", fz_to_num(obj), fz_to_gen(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_null(obj))
		fmt_puts(fmt, "null");
	else if (fz_is_bool(obj))
		fmt_puts(fmt, fz_to_bool(obj) ? "true" : "false");
	else if (fz_is_int(obj))
	{
		sprintf(buf, "%d", fz_to_int(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_real(obj))
	{
		sprintf(buf, "%g", fz_to_real(obj));
		if (strchr(buf, 'e')) /* bad news! */
			sprintf(buf, fabsf(fz_to_real(obj)) > 1 ? "%1.1f" : "%1.8f", fz_to_real(obj));
		fmt_puts(fmt, buf);
	}
	else if (fz_is_string(obj))
	{
		char *str = fz_to_str_buf(obj);
		int len = fz_to_str_len(obj);
		int added = 0;
		int i, c;
		for (i = 0; i < len; i++) {
			c = (unsigned char)str[i];
			if (strchr("()\\\n\r\t\b\f", c))
				added ++;
			else if (c < 32 || c >= 127)
				added += 3;
		}
		if (added < len)
			fmt_str(fmt, obj);
		else
			fmt_hex(fmt, obj);
	}
	else if (fz_is_name(obj))
		fmt_name(fmt, obj);
	else if (fz_is_array(obj))
		fmt_array(fmt, obj);
	else if (fz_is_dict(obj))
		fmt_dict(fmt, obj);
	else
		fmt_puts(fmt, "<unknown object>");
}

static int
fz_sprint_obj(char *s, int n, fz_obj *obj, int tight)
{
	struct fmt fmt;

	fmt.indent = 0;
	fmt.col = 0;
	fmt.sep = 0;
	fmt.last = 0;

	fmt.tight = tight;
	fmt.buf = s;
	fmt.cap = n;
	fmt.len = 0;
	fmt_obj(&fmt, obj);

	if (fmt.buf && fmt.len < fmt.cap)
		fmt.buf[fmt.len] = '\0';

	return fmt.len;
}

int
fz_fprint_obj(FILE *fp, fz_obj *obj, int tight)
{
	char buf[1024];
	char *ptr;
	int n;

	n = fz_sprint_obj(NULL, 0, obj, tight);
	if ((n + 1) < sizeof buf)
	{
		fz_sprint_obj(buf, sizeof buf, obj, tight);
		fputs(buf, fp);
		fputc('\n', fp);
	}
	else
	{
		ptr = fz_malloc(n + 1);
		fz_sprint_obj(ptr, n + 1, obj, tight);
		fputs(ptr, fp);
		fputc('\n', fp);
		fz_free(ptr);
	}
	return n;
}

void
fz_debug_obj(fz_obj *obj)
{
	fz_fprint_obj(stdout, obj, 0);
}

void
fz_debug_ref(fz_obj *ref)
{
	fz_obj *obj;
	obj = fz_resolve_indirect(ref);
	fz_debug_obj(obj);
}
