#include "mupdf/fitz.h"
#include "html-imp.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static const char *inherit_list[] = {
	"color",
	"direction",
	"font-family",
	"font-style",
	"font-variant",
	"font-weight",
	"letter-spacing",
	"line-height",
	"list-style-image",
	"list-style-position",
	"list-style-type",
	"orphans",
	"quotes",
	"text-align",
	"text-indent",
	"text-transform",
	"visibility",
	"white-space",
	"widows",
	"word-spacing",
};

static const char *border_width_kw[] = {
	"medium",
	"thick",
	"thin",
};

static const char *border_style_kw[] = {
	"dashed",
	"dotted",
	"double",
	"groove",
	"hidden",
	"inset",
	"none",
	"outset",
	"ridge",
	"solid",
};

static const char *color_kw[] = {
	"aqua",
	"black",
	"blue",
	"fuchsia",
	"gray",
	"green",
	"lime",
	"maroon",
	"navy",
	"olive",
	"orange",
	"purple",
	"red",
	"silver",
	"teal",
	"transparent",
	"white",
	"yellow",
};

static const char *list_style_type_kw[] = {
	"armenian",
	"circle",
	"decimal",
	"decimal-leading-zero",
	"disc",
	"georgian",
	"lower-alpha",
	"lower-greek",
	"lower-latin",
	"lower-roman",
	"none",
	"square",
	"upper-alpha",
	"upper-greek",
	"upper-latin",
	"upper-roman",
};

static const char *list_style_position_kw[] = {
	"inside",
	"outside",
};

static int
keyword_in_list(const char *name, const char **list, int n)
{
	int l = 0;
	int r = n - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(name, list[m]);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return 1;
	}
	return 0;
}

static int
is_bold_from_font_weight(const char *weight)
{
	return !strcmp(weight, "bold") || !strcmp(weight, "bolder") || atoi(weight) > 400;
}

static int
is_italic_from_font_style(const char *style)
{
	return !strcmp(style, "italic") || !strcmp(style, "oblique");
}

/*
 * Compute specificity
 */

static int
count_condition_ids(fz_css_condition *cond)
{
	int n = 0;
	while (cond)
	{
		if (cond->type == '#')
			n ++;
		cond = cond->next;
	}
	return n;
}

static int
count_selector_ids(fz_css_selector *sel)
{
	int n = count_condition_ids(sel->cond);
	if (sel->left && sel->right)
	{
		n += count_selector_ids(sel->left);
		n += count_selector_ids(sel->right);
	}
	return n;
}

static int
count_condition_atts(fz_css_condition *cond)
{
	int n = 0;
	while (cond)
	{
		if (cond->type != '#' && cond->type != ':')
			n ++;
		cond = cond->next;
	}
	return n;
}

static int
count_selector_atts(fz_css_selector *sel)
{
	int n = count_condition_atts(sel->cond);
	if (sel->left && sel->right)
	{
		n += count_selector_atts(sel->left);
		n += count_selector_atts(sel->right);
	}
	return n;
}

static int
count_condition_names(fz_css_condition *cond)
{
	int n = 0;
	while (cond)
	{
		if (cond->type == ':')
			n ++;
		cond = cond->next;
	}
	return n;
}

static int
count_selector_names(fz_css_selector *sel)
{
	int n = count_condition_names(sel->cond);
	if (sel->left && sel->right)
	{
		n += count_selector_names(sel->left);
		n += count_selector_names(sel->right);
	}
	else if (sel->name)
	{
		n ++;
	}
	return n;
}

#define INLINE_SPECIFICITY 10000

static int
selector_specificity(fz_css_selector *sel, int important)
{
	int b = count_selector_ids(sel);
	int c = count_selector_atts(sel);
	int d = count_selector_names(sel);
	return important * 1000 + b * 100 + c * 10 + d;
}

/*
 * Selector matching
 */

static int
match_att_exists_condition(fz_xml *node, const char *key)
{
	const char *s = fz_xml_att(node, key);
	return s != NULL;
}

static int
match_att_is_condition(fz_xml *node, const char *key, const char *val)
{
	const char *att = fz_xml_att(node, key);
	return att && !strcmp(val, att);
}

static int
match_att_has_condition(fz_xml *node, const char *att, const char *needle)
{
	const char *haystack = fz_xml_att(node, att);
	const char *ss;
	size_t n;
	if (haystack) {
		/* Try matching whole property first. */
		if (!strcmp(haystack, needle))
			return 1;

		/* Look for matching words. */
		n = strlen(needle);
		ss = strstr(haystack, needle);
		if (ss && (ss[n] == ' ' || ss[n] == 0) && (ss == haystack || ss[-1] == ' '))
			return 1;
	}
	return 0;
}

static int
match_condition(fz_css_condition *cond, fz_xml *node)
{
	if (!cond)
		return 1;

	switch (cond->type) {
	default: return 0;
	case ':': return 0; /* don't support pseudo-classes */
	case '#': if (!match_att_is_condition(node, "id", cond->val)) return 0; break;
	case '.': if (!match_att_has_condition(node, "class", cond->val)) return 0; break;
	case '[': if (!match_att_exists_condition(node, cond->key)) return 0; break;
	case '=': if (!match_att_is_condition(node, cond->key, cond->val)) return 0; break;
	case '~': if (!match_att_has_condition(node, cond->key, cond->val)) return 0; break;
	case '|': if (!match_att_is_condition(node, cond->key, cond->val)) return 0; break;
	}

	return match_condition(cond->next, node);
}

static int
match_selector(fz_css_selector *sel, fz_xml *node)
{
	if (!node)
		return 0;

	if (sel->combine)
	{
		/* descendant */
		if (sel->combine == ' ')
		{
			fz_xml *parent = fz_xml_up(node);
			while (parent)
			{
				if (match_selector(sel->left, parent))
					if (match_selector(sel->right, node))
						return 1;
				parent = fz_xml_up(parent);
			}
			return 0;
		}

		/* child */
		if (sel->combine == '>')
		{
			fz_xml *parent = fz_xml_up(node);
			if (!parent)
				return 0;
			if (!match_selector(sel->left, parent))
				return 0;
			if (!match_selector(sel->right, node))
				return 0;
		}

		/* adjacent */
		if (sel->combine == '+')
		{
			fz_xml *prev = fz_xml_prev(node);
			while (prev && !fz_xml_tag(prev))
				prev = fz_xml_prev(prev);
			if (!prev)
				return 0;
			if (!fz_xml_tag(prev))
				return 0;
			if (!match_selector(sel->left, prev))
				return 0;
			if (!match_selector(sel->right, node))
				return 0;
		}
	}

	if (sel->name)
	{
		if (!fz_xml_is_tag(node, sel->name))
			return 0;
	}

	if (sel->cond)
	{
		if (!match_condition(sel->cond, node))
			return 0;
	}

	return 1;
}

/*
 * Annotating nodes with properties and expanding shorthand forms.
 */

static int
count_values(fz_css_value *value)
{
	int n = 0;
	while (value)
	{
		n++;
		value = value->next;
	}
	return n;
}

static void add_property(fz_css_match *match, const char *name, fz_css_value *value, int spec);

static void
add_shorthand_trbl(fz_css_match *match, fz_css_value *value, int spec,
	const char *name_t, const char *name_r, const char *name_b, const char *name_l)
{
	int n = count_values(value);

	if (n == 1)
	{
		add_property(match, name_t, value, spec);
		add_property(match, name_r, value, spec);
		add_property(match, name_b, value, spec);
		add_property(match, name_l, value, spec);
	}

	if (n == 2)
	{
		fz_css_value *a = value;
		fz_css_value *b = value->next;

		add_property(match, name_t, a, spec);
		add_property(match, name_r, b, spec);
		add_property(match, name_b, a, spec);
		add_property(match, name_l, b, spec);
	}

	if (n == 3)
	{
		fz_css_value *a = value;
		fz_css_value *b = value->next;
		fz_css_value *c = value->next->next;

		add_property(match, name_t, a, spec);
		add_property(match, name_r, b, spec);
		add_property(match, name_b, c, spec);
		add_property(match, name_l, b, spec);
	}

	if (n == 4)
	{
		fz_css_value *a = value;
		fz_css_value *b = value->next;
		fz_css_value *c = value->next->next;
		fz_css_value *d = value->next->next->next;

		add_property(match, name_t, a, spec);
		add_property(match, name_r, b, spec);
		add_property(match, name_b, c, spec);
		add_property(match, name_l, d, spec);
	}
}

static void
add_shorthand_margin(fz_css_match *match, fz_css_value *value, int spec)
{
	add_shorthand_trbl(match, value, spec,
		"margin-top", "margin-right", "margin-bottom", "margin-left");
}

static void
add_shorthand_padding(fz_css_match *match, fz_css_value *value, int spec)
{
	add_shorthand_trbl(match, value, spec,
		"padding-top", "padding-right", "padding-bottom", "padding-left");
}

static void
add_shorthand_border_width(fz_css_match *match, fz_css_value *value, int spec)
{
	add_shorthand_trbl(match, value, spec,
		"border-top-width", "border-right-width", "border-bottom-width", "border-left-width");
}

static void
add_shorthand_border_color(fz_css_match *match, fz_css_value *value, int spec)
{
	add_shorthand_trbl(match, value, spec,
		"border-top-color", "border-right-color", "border-bottom-color", "border-left-color");
}

static void
add_shorthand_border_style(fz_css_match *match, fz_css_value *value, int spec)
{
	add_shorthand_trbl(match, value, spec,
		"border-top-style", "border-right-style", "border-bottom-style", "border-left-style");
}

static void
add_shorthand_border(fz_css_match *match, fz_css_value *value, int spec, int T, int R, int B, int L)
{
	while (value)
	{
		if (value->type == CSS_HASH)
		{
			if (T) add_property(match, "border-top-color", value, spec);
			if (R) add_property(match, "border-right-color", value, spec);
			if (B) add_property(match, "border-bottom-color", value, spec);
			if (L) add_property(match, "border-left-color", value, spec);
		}
		else if (value->type == CSS_KEYWORD)
		{
			if (keyword_in_list(value->data, border_width_kw, nelem(border_width_kw)))
			{
				if (T) add_property(match, "border-top-width", value, spec);
				if (R) add_property(match, "border-right-width", value, spec);
				if (B) add_property(match, "border-bottom-width", value, spec);
				if (L) add_property(match, "border-left-width", value, spec);
			}
			else if (keyword_in_list(value->data, border_style_kw, nelem(border_style_kw)))
			{
				if (T) add_property(match, "border-top-style", value, spec);
				if (R) add_property(match, "border-right-style", value, spec);
				if (B) add_property(match, "border-bottom-style", value, spec);
				if (L) add_property(match, "border-left-style", value, spec);
			}
			else if (keyword_in_list(value->data, color_kw, nelem(color_kw)))
			{
				if (T) add_property(match, "border-top-color", value, spec);
				if (R) add_property(match, "border-right-color", value, spec);
				if (B) add_property(match, "border-bottom-color", value, spec);
				if (L) add_property(match, "border-left-color", value, spec);
			}
		}
		else
		{
			if (T) add_property(match, "border-top-width", value, spec);
			if (R) add_property(match, "border-right-width", value, spec);
			if (B) add_property(match, "border-bottom-width", value, spec);
			if (L) add_property(match, "border-left-width", value, spec);
		}
		value = value->next;
	}
}

static void
add_shorthand_list_style(fz_css_match *match, fz_css_value *value, int spec)
{
	while (value)
	{
		if (value->type == CSS_KEYWORD)
		{
			if (keyword_in_list(value->data, list_style_type_kw, nelem(list_style_type_kw)))
			{
				add_property(match, "list-style-type", value, spec);
			}
			else if (keyword_in_list(value->data, list_style_position_kw, nelem(list_style_position_kw)))
			{
				add_property(match, "list-style-position", value, spec);
			}
		}
		value = value->next;
	}
}

static void
add_property(fz_css_match *match, const char *name, fz_css_value *value, int spec)
{
	int i;

	if (!strcmp(name, "margin"))
	{
		add_shorthand_margin(match, value, spec);
		return;
	}
	if (!strcmp(name, "padding"))
	{
		add_shorthand_padding(match, value, spec);
		return;
	}
	if (!strcmp(name, "border-width"))
	{
		add_shorthand_border_width(match, value, spec);
		return;
	}
	if (!strcmp(name, "border-color"))
	{
		add_shorthand_border_color(match, value, spec);
		return;
	}
	if (!strcmp(name, "border-style"))
	{
		add_shorthand_border_style(match, value, spec);
		return;
	}
	if (!strcmp(name, "border"))
	{
		add_shorthand_border(match, value, spec, 1, 1, 1, 1);
		return;
	}
	if (!strcmp(name, "border-top"))
	{
		add_shorthand_border(match, value, spec, 1, 0, 0, 0);
		return;
	}
	if (!strcmp(name, "border-right"))
	{
		add_shorthand_border(match, value, spec, 0, 1, 0, 0);
		return;
	}
	if (!strcmp(name, "border-bottom"))
	{
		add_shorthand_border(match, value, spec, 0, 0, 1, 0);
		return;
	}
	if (!strcmp(name, "border-left"))
	{
		add_shorthand_border(match, value, spec, 0, 0, 0, 1);
		return;
	}
	if (!strcmp(name, "list-style"))
	{
		add_shorthand_list_style(match, value, spec);
		return;
	}

	/* shorthand expansions: */
	/* TODO: border-color */
	/* TODO: border-style */
	/* TODO: font */
	/* TODO: list-style */
	/* TODO: background */

	for (i = 0; i < match->count; ++i)
	{
		if (!strcmp(match->prop[i].name, name))
		{
			if (match->prop[i].spec <= spec)
			{
				match->prop[i].value = value;
				match->prop[i].spec = spec;
			}
			return;
		}
	}

	if (match->count + 1 >= (int)nelem(match->prop))
	{
		// fz_warn(ctx, "too many css properties");
		return;
	}

	match->prop[match->count].name = name;
	match->prop[match->count].value = value;
	match->prop[match->count].spec = spec;
	++match->count;
}

static void
sort_properties(fz_css_match *match)
{
	int count = match->count;
	fz_css_match_prop *prop = match->prop;
	int i, k;

	/* Insertion sort. */
	for (i = 1; i < count; ++i)
	{
		k = i;
		while (k > 0 && strcmp(prop[k-1].name, prop[k].name) > 0)
		{
			fz_css_match_prop save = prop[k-1];
			prop[k-1] = prop[k];
			prop[k] = save;
			--k;
		}
	}
}

void
fz_match_css(fz_context *ctx, fz_css_match *match, fz_css *css, fz_xml *node)
{
	fz_css_rule *rule;
	fz_css_selector *sel;
	fz_css_property *prop;
	const char *s;

	for (rule = css->rule; rule; rule = rule->next)
	{
		sel = rule->selector;
		while (sel)
		{
			if (match_selector(sel, node))
			{
				for (prop = rule->declaration; prop; prop = prop->next)
					add_property(match, prop->name, prop->value, selector_specificity(sel, prop->important));
				break;
			}
			sel = sel->next;
		}
	}

	if (fz_use_document_css(ctx))
	{
		s = fz_xml_att(node, "style");
		if (s)
		{
			fz_try(ctx)
			{
				prop = fz_parse_css_properties(ctx, css->pool, s);
				while (prop)
				{
					add_property(match, prop->name, prop->value, INLINE_SPECIFICITY);
					prop = prop->next;
				}
				/* We can "leak" the property here, since it is freed along with the pool allocator. */
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "ignoring style attribute");
			}
		}
	}

	sort_properties(match); /* speed up subsequent value_from_raw_property lookups */
}

void
fz_match_css_at_page(fz_context *ctx, fz_css_match *match, fz_css *css)
{
	fz_css_rule *rule;
	fz_css_selector *sel;
	fz_css_property *prop;

	for (rule = css->rule; rule; rule = rule->next)
	{
		sel = rule->selector;
		while (sel)
		{
			if (sel->name && !strcmp(sel->name, "@page"))
			{
				for (prop = rule->declaration; prop; prop = prop->next)
					add_property(match, prop->name, prop->value, selector_specificity(sel, prop->important));
				break;
			}
			sel = sel->next;
		}
	}

	sort_properties(match); /* speed up subsequent value_from_raw_property lookups */
}

void
fz_add_css_font_face(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css_property *declaration)
{
	fz_html_font_face *custom;
	fz_css_property *prop;
	fz_font *font = NULL;
	fz_buffer *buf = NULL;
	int is_bold, is_italic, is_small_caps;
	char path[2048];

	const char *family = "serif";
	const char *weight = "normal";
	const char *style = "normal";
	const char *variant = "normal";
	const char *src = NULL;

	for (prop = declaration; prop; prop = prop->next)
	{
		if (!strcmp(prop->name, "font-family")) family = prop->value->data;
		if (!strcmp(prop->name, "font-weight")) weight = prop->value->data;
		if (!strcmp(prop->name, "font-style")) style = prop->value->data;
		if (!strcmp(prop->name, "font-variant")) variant = prop->value->data;
		if (!strcmp(prop->name, "src")) src = prop->value->data;
	}

	if (!src)
		return;

	is_bold = is_bold_from_font_weight(weight);
	is_italic = is_italic_from_font_style(style);
	is_small_caps = !strcmp(variant, "small-caps");

	fz_strlcpy(path, base_uri, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, src, sizeof path);
	fz_urldecode(path);
	fz_cleanname(path);

	for (custom = set->custom; custom; custom = custom->next)
		if (!strcmp(custom->src, path) && !strcmp(custom->family, family) &&
				custom->is_bold == is_bold &&
				custom->is_italic == is_italic &&
				custom->is_small_caps == is_small_caps)
			return; /* already loaded */

	fz_var(buf);
	fz_var(font);

	fz_try(ctx)
	{
		if (fz_has_archive_entry(ctx, zip, path))
			buf = fz_read_archive_entry(ctx, zip, path);
		else
			buf = fz_read_file(ctx, src);
		font = fz_new_font_from_buffer(ctx, NULL, buf, 0, 0);
		fz_add_html_font_face(ctx, set, family, is_bold, is_italic, is_small_caps, path, font);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_font(ctx, font);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "cannot load font-face: %s", src);
	}
}

void
fz_add_css_font_faces(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css *css)
{
	fz_css_rule *rule;
	fz_css_selector *sel;

	for (rule = css->rule; rule; rule = rule->next)
	{
		if (!rule->loaded)
		{
			rule->loaded = 1;
			sel = rule->selector;
			while (sel)
			{
				if (sel->name && !strcmp(sel->name, "@font-face"))
				{
					fz_add_css_font_face(ctx, set, zip, base_uri, rule->declaration);
					break;
				}
				sel = sel->next;
			}
		}
	}
}

static fz_css_value *
value_from_raw_property(fz_css_match *match, const char *name)
{
	fz_css_match_prop *prop = match->prop;
	int l = 0;
	int r = match->count - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(name, prop[m].name);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return prop[m].value;
	}
	return NULL;
}

static fz_css_value *
value_from_property(fz_css_match *match, const char *name)
{
	fz_css_value *value;

	value = value_from_raw_property(match, name);
	if (match->up)
	{
		if (value && !strcmp(value->data, "inherit"))
			if (strcmp(name, "font-size") != 0) /* never inherit 'font-size' textually */
				return value_from_property(match->up, name);
		if (!value && keyword_in_list(name, inherit_list, nelem(inherit_list)))
			return value_from_property(match->up, name);
	}
	return value;
}

static const char *
string_from_property(fz_css_match *match, const char *name, const char *initial)
{
	fz_css_value *value;
	value = value_from_property(match, name);
	if (!value)
		return initial;
	return value->data;
}

static fz_css_number
make_number(float v, int u)
{
	fz_css_number n;
	n.value = v;
	n.unit = u;
	return n;
}

/* Fast but inaccurate strtof. */
static float
fz_css_strtof(char *s, char **endptr)
{
	float sign = 1;
	float v = 0;
	float n = 0;
	float d = 1;

	if (*s == '-')
	{
		sign = -1;
		++s;
	}

	while (*s >= '0' && *s <= '9')
	{
		v = v * 10 + (*s - '0');
		++s;
	}

	if (*s == '.')
	{
		++s;
		while (*s >= '0' && *s <= '9')
		{
			n = n * 10 + (*s - '0');
			d = d * 10;
			++s;
		}
		v += n / d;
	}

	if (endptr)
		*endptr = s;

	return sign * v;
}

static fz_css_number
number_from_value(fz_css_value *value, float initial, int initial_unit)
{
	char *p;

	if (!value)
		return make_number(initial, initial_unit);

	if (value->type == CSS_PERCENT)
		return make_number(fz_css_strtof(value->data, NULL), N_PERCENT);

	if (value->type == CSS_NUMBER)
		return make_number(fz_css_strtof(value->data, NULL), N_NUMBER);

	if (value->type == CSS_LENGTH)
	{
		float x = fz_css_strtof(value->data, &p);

		if (p[0] == 'e' && p[1] == 'm' && p[2] == 0)
			return make_number(x, N_SCALE);
		if (p[0] == 'e' && p[1] == 'x' && p[2] == 0)
			return make_number(x / 2, N_SCALE);

		if (p[0] == 'i' && p[1] == 'n' && p[2] == 0)
			return make_number(x * 72, N_LENGTH);
		if (p[0] == 'c' && p[1] == 'm' && p[2] == 0)
			return make_number(x * 7200 / 254, N_LENGTH);
		if (p[0] == 'm' && p[1] == 'm' && p[2] == 0)
			return make_number(x * 720 / 254, N_LENGTH);
		if (p[0] == 'p' && p[1] == 'c' && p[2] == 0)
			return make_number(x * 12, N_LENGTH);

		if (p[0] == 'p' && p[1] == 't' && p[2] == 0)
			return make_number(x, N_LENGTH);
		if (p[0] == 'p' && p[1] == 'x' && p[2] == 0)
			return make_number(x, N_LENGTH);

		/* FIXME: 'rem' should be 'em' of root element. This is a bad approximation. */
		if (p[0] == 'r' && p[1] == 'e' && p[2] == 'm' && p[3] == 0)
			return make_number(x * 16, N_LENGTH);

		/* FIXME: 'ch' should be width of '0' character. This is an approximation. */
		if (p[0] == 'c' && p[1] == 'h' && p[2] == 0)
			return make_number(x / 2, N_LENGTH);

		return make_number(x, N_LENGTH);
	}

	if (value->type == CSS_KEYWORD)
	{
		if (!strcmp(value->data, "auto"))
			return make_number(0, N_AUTO);
	}

	return make_number(initial, initial_unit);
}

static fz_css_number
number_from_property(fz_css_match *match, const char *property, float initial, int initial_unit)
{
	return number_from_value(value_from_property(match, property), initial, initial_unit);
}

static fz_css_number
border_width_from_property(fz_css_match *match, const char *property)
{
	fz_css_value *value = value_from_property(match, property);
	if (value)
	{
		if (!strcmp(value->data, "thin"))
			return make_number(1, N_LENGTH);
		if (!strcmp(value->data, "medium"))
			return make_number(2, N_LENGTH);
		if (!strcmp(value->data, "thick"))
			return make_number(4, N_LENGTH);
		return number_from_value(value, 0, N_LENGTH);
	}
	return make_number(2, N_LENGTH); /* initial: 'medium' */
}

static int
border_style_from_property(fz_css_match *match, const char *property)
{
	fz_css_value *value = value_from_property(match, property);
	if (value)
	{
		if (!strcmp(value->data, "none")) return BS_NONE;
		else if (!strcmp(value->data, "hidden")) return BS_NONE;
		else if (!strcmp(value->data, "solid")) return BS_SOLID;
	}
	return BS_NONE;
}

float
fz_from_css_number(fz_css_number number, float em, float percent_value, float auto_value)
{
	switch (number.unit) {
	default:
	case N_NUMBER: return number.value;
	case N_LENGTH: return number.value;
	case N_SCALE: return number.value * em;
	case N_PERCENT: return number.value * 0.01f * percent_value;
	case N_AUTO: return auto_value;
	}
}

float
fz_from_css_number_scale(fz_css_number number, float scale)
{
	switch (number.unit) {
	default:
	case N_NUMBER: return number.value * scale;
	case N_LENGTH: return number.value;
	case N_SCALE: return number.value * scale;
	case N_PERCENT: return number.value * 0.01f * scale;
	case N_AUTO: return scale;
	}
}

static fz_css_color
make_color(int r, int g, int b, int a)
{
	fz_css_color c;
	c.r = r < 0 ? 0 : r > 255 ? 255 : r;
	c.g = g < 0 ? 0 : g > 255 ? 255 : g;
	c.b = b < 0 ? 0 : b > 255 ? 255 : b;
	c.a = a < 0 ? 0 : a > 255 ? 255 : a;
	return c;
}

static int tohex(int c)
{
	if (c - '0' < 10)
		return c - '0';
	return (c | 32) - 'a' + 10;
}

static fz_css_color
color_from_value(fz_css_value *value, fz_css_color initial)
{
	if (!value)
		return initial;

	if (value->type == CSS_HASH)
	{
		int r, g, b;
		size_t n;
hex_color:
		n = strlen(value->data);
		if (n == 3)
		{
			r = tohex(value->data[0]) * 16 + tohex(value->data[0]);
			g = tohex(value->data[1]) * 16 + tohex(value->data[1]);
			b = tohex(value->data[2]) * 16 + tohex(value->data[2]);
		}
		else if (n == 6)
		{
			r = tohex(value->data[0]) * 16 + tohex(value->data[1]);
			g = tohex(value->data[2]) * 16 + tohex(value->data[3]);
			b = tohex(value->data[4]) * 16 + tohex(value->data[5]);
		}
		else
		{
			r = g = b = 0;
		}
		return make_color(r, g, b, 255);
	}

	if (value->type == '(' && !strcmp(value->data, "rgb"))
	{
		fz_css_value *vr, *vg, *vb;
		int r, g, b;
		vr = value->args;
		vg = vr && vr->next ? vr->next->next : NULL; /* skip the ',' nodes */
		vb = vg && vg->next ? vg->next->next : NULL; /* skip the ',' nodes */
		r = fz_from_css_number(number_from_value(vr, 0, N_NUMBER), 255, 255, 0);
		g = fz_from_css_number(number_from_value(vg, 0, N_NUMBER), 255, 255, 0);
		b = fz_from_css_number(number_from_value(vb, 0, N_NUMBER), 255, 255, 0);
		return make_color(r, g, b, 255);
	}

	if (value->type == CSS_KEYWORD)
	{
		if (!strcmp(value->data, "transparent"))
			return make_color(0, 0, 0, 0);
		if (!strcmp(value->data, "maroon"))
			return make_color(0x80, 0x00, 0x00, 255);
		if (!strcmp(value->data, "red"))
			return make_color(0xFF, 0x00, 0x00, 255);
		if (!strcmp(value->data, "orange"))
			return make_color(0xFF, 0xA5, 0x00, 255);
		if (!strcmp(value->data, "yellow"))
			return make_color(0xFF, 0xFF, 0x00, 255);
		if (!strcmp(value->data, "olive"))
			return make_color(0x80, 0x80, 0x00, 255);
		if (!strcmp(value->data, "purple"))
			return make_color(0x80, 0x00, 0x80, 255);
		if (!strcmp(value->data, "fuchsia"))
			return make_color(0xFF, 0x00, 0xFF, 255);
		if (!strcmp(value->data, "white"))
			return make_color(0xFF, 0xFF, 0xFF, 255);
		if (!strcmp(value->data, "lime"))
			return make_color(0x00, 0xFF, 0x00, 255);
		if (!strcmp(value->data, "green"))
			return make_color(0x00, 0x80, 0x00, 255);
		if (!strcmp(value->data, "navy"))
			return make_color(0x00, 0x00, 0x80, 255);
		if (!strcmp(value->data, "blue"))
			return make_color(0x00, 0x00, 0xFF, 255);
		if (!strcmp(value->data, "aqua"))
			return make_color(0x00, 0xFF, 0xFF, 255);
		if (!strcmp(value->data, "teal"))
			return make_color(0x00, 0x80, 0x80, 255);
		if (!strcmp(value->data, "black"))
			return make_color(0x00, 0x00, 0x00, 255);
		if (!strcmp(value->data, "silver"))
			return make_color(0xC0, 0xC0, 0xC0, 255);
		if (!strcmp(value->data, "gray"))
			return make_color(0x80, 0x80, 0x80, 255);
		goto hex_color; /* last ditch attempt: maybe it's a #XXXXXX color without the # */
	}
	return initial;
}

static fz_css_color
color_from_property(fz_css_match *match, const char *property, fz_css_color initial)
{
	return color_from_value(value_from_property(match, property), initial);
}

int
fz_get_css_match_display(fz_css_match *match)
{
	fz_css_value *value = value_from_property(match, "display");
	if (value)
	{
		if (!strcmp(value->data, "none"))
			return DIS_NONE;
		if (!strcmp(value->data, "inline"))
			return DIS_INLINE;
		if (!strcmp(value->data, "block"))
			return DIS_BLOCK;
		if (!strcmp(value->data, "list-item"))
			return DIS_LIST_ITEM;
		if (!strcmp(value->data, "inline-block"))
			return DIS_INLINE_BLOCK;
		if (!strcmp(value->data, "table"))
			return DIS_TABLE;
		if (!strcmp(value->data, "table-row"))
			return DIS_TABLE_ROW;
		if (!strcmp(value->data, "table-cell"))
			return DIS_TABLE_CELL;
	}
	return DIS_INLINE;
}

static int
white_space_from_property(fz_css_match *match)
{
	fz_css_value *value = value_from_property(match, "white-space");
	if (value)
	{
		if (!strcmp(value->data, "normal")) return WS_NORMAL;
		else if (!strcmp(value->data, "pre")) return WS_PRE;
		else if (!strcmp(value->data, "nowrap")) return WS_NOWRAP;
		else if (!strcmp(value->data, "pre-wrap")) return WS_PRE_WRAP;
		else if (!strcmp(value->data, "pre-line")) return WS_PRE_LINE;
	}
	return WS_NORMAL;
}

static int
visibility_from_property(fz_css_match *match)
{
	fz_css_value *value = value_from_property(match, "visibility");
	if (value)
	{
		if (!strcmp(value->data, "visible")) return V_VISIBLE;
		else if (!strcmp(value->data, "hidden")) return V_HIDDEN;
		else if (!strcmp(value->data, "collapse")) return V_COLLAPSE;
	}
	return V_VISIBLE;
}

static int
page_break_from_property(fz_css_match *match, char *prop)
{
	fz_css_value *value = value_from_property(match, prop);
	if (value)
	{
		if (!strcmp(value->data, "auto")) return PB_AUTO;
		else if (!strcmp(value->data, "always")) return PB_ALWAYS;
		else if (!strcmp(value->data, "avoid")) return PB_AVOID;
		else if (!strcmp(value->data, "left")) return PB_LEFT;
		else if (!strcmp(value->data, "right")) return PB_RIGHT;
	}
	return PB_AUTO;
}

void
fz_default_css_style(fz_context *ctx, fz_css_style *style)
{
	memset(style, 0, sizeof *style);
	style->visibility = V_VISIBLE;
	style->text_align = TA_LEFT;
	style->vertical_align = VA_BASELINE;
	style->white_space = WS_NORMAL;
	style->list_style_type = LST_DISC;
	style->font_size = make_number(1, N_SCALE);
	style->width = make_number(0, N_AUTO);
	style->height = make_number(0, N_AUTO);
}

void
fz_apply_css_style(fz_context *ctx, fz_html_font_set *set, fz_css_style *style, fz_css_match *match)
{
	fz_css_value *value;

	fz_css_color black = { 0, 0, 0, 255 };
	fz_css_color transparent = { 0, 0, 0, 0 };

	fz_default_css_style(ctx, style);

	style->visibility = visibility_from_property(match);
	style->white_space = white_space_from_property(match);
	style->page_break_before = page_break_from_property(match, "page-break-before");
	style->page_break_after = page_break_from_property(match, "page-break-after");

	value = value_from_property(match, "text-align");
	if (value)
	{
		if (!strcmp(value->data, "left")) style->text_align = TA_LEFT;
		else if (!strcmp(value->data, "right")) style->text_align = TA_RIGHT;
		else if (!strcmp(value->data, "center")) style->text_align = TA_CENTER;
		else if (!strcmp(value->data, "justify")) style->text_align = TA_JUSTIFY;
	}

	value = value_from_property(match, "vertical-align");
	if (value)
	{
		if (!strcmp(value->data, "baseline")) style->vertical_align = VA_BASELINE;
		else if (!strcmp(value->data, "sub")) style->vertical_align = VA_SUB;
		else if (!strcmp(value->data, "super")) style->vertical_align = VA_SUPER;
		else if (!strcmp(value->data, "top")) style->vertical_align = VA_TOP;
		else if (!strcmp(value->data, "bottom")) style->vertical_align = VA_BOTTOM;
		else if (!strcmp(value->data, "text-top")) style->vertical_align = VA_TEXT_TOP;
		else if (!strcmp(value->data, "text-bottom")) style->vertical_align = VA_TEXT_BOTTOM;
	}

	value = value_from_property(match, "font-size");
	if (value)
	{
		if (!strcmp(value->data, "xx-large")) style->font_size = make_number(1.73f, N_SCALE);
		else if (!strcmp(value->data, "x-large")) style->font_size = make_number(1.44f, N_SCALE);
		else if (!strcmp(value->data, "large")) style->font_size = make_number(1.2f, N_SCALE);
		else if (!strcmp(value->data, "medium")) style->font_size = make_number(1.0f, N_SCALE);
		else if (!strcmp(value->data, "small")) style->font_size = make_number(0.83f, N_SCALE);
		else if (!strcmp(value->data, "x-small")) style->font_size = make_number(0.69f, N_SCALE);
		else if (!strcmp(value->data, "xx-small")) style->font_size = make_number(0.69f, N_SCALE);
		else if (!strcmp(value->data, "larger")) style->font_size = make_number(1.2f, N_SCALE);
		else if (!strcmp(value->data, "smaller")) style->font_size = make_number(1/1.2f, N_SCALE);
		else style->font_size = number_from_value(value, 12, N_LENGTH);
	}
	else
	{
		style->font_size = make_number(1, N_SCALE);
	}

	value = value_from_property(match, "list-style-type");
	if (value)
	{
		if (!strcmp(value->data, "none")) style->list_style_type = LST_NONE;
		else if (!strcmp(value->data, "disc")) style->list_style_type = LST_DISC;
		else if (!strcmp(value->data, "circle")) style->list_style_type = LST_CIRCLE;
		else if (!strcmp(value->data, "square")) style->list_style_type = LST_SQUARE;
		else if (!strcmp(value->data, "decimal")) style->list_style_type = LST_DECIMAL;
		else if (!strcmp(value->data, "decimal-leading-zero")) style->list_style_type = LST_DECIMAL_ZERO;
		else if (!strcmp(value->data, "lower-roman")) style->list_style_type = LST_LC_ROMAN;
		else if (!strcmp(value->data, "upper-roman")) style->list_style_type = LST_UC_ROMAN;
		else if (!strcmp(value->data, "lower-greek")) style->list_style_type = LST_LC_GREEK;
		else if (!strcmp(value->data, "upper-greek")) style->list_style_type = LST_UC_GREEK;
		else if (!strcmp(value->data, "lower-latin")) style->list_style_type = LST_LC_LATIN;
		else if (!strcmp(value->data, "upper-latin")) style->list_style_type = LST_UC_LATIN;
		else if (!strcmp(value->data, "lower-alpha")) style->list_style_type = LST_LC_ALPHA;
		else if (!strcmp(value->data, "upper-alpha")) style->list_style_type = LST_UC_ALPHA;
		else if (!strcmp(value->data, "armenian")) style->list_style_type = LST_ARMENIAN;
		else if (!strcmp(value->data, "georgian")) style->list_style_type = LST_GEORGIAN;
	}

	style->line_height = number_from_property(match, "line-height", 1.2f, N_SCALE);

	style->text_indent = number_from_property(match, "text-indent", 0, N_LENGTH);

	style->width = number_from_property(match, "width", 0, N_AUTO);
	style->height = number_from_property(match, "height", 0, N_AUTO);

	style->margin[0] = number_from_property(match, "margin-top", 0, N_LENGTH);
	style->margin[1] = number_from_property(match, "margin-right", 0, N_LENGTH);
	style->margin[2] = number_from_property(match, "margin-bottom", 0, N_LENGTH);
	style->margin[3] = number_from_property(match, "margin-left", 0, N_LENGTH);

	style->padding[0] = number_from_property(match, "padding-top", 0, N_LENGTH);
	style->padding[1] = number_from_property(match, "padding-right", 0, N_LENGTH);
	style->padding[2] = number_from_property(match, "padding-bottom", 0, N_LENGTH);
	style->padding[3] = number_from_property(match, "padding-left", 0, N_LENGTH);

	style->color = color_from_property(match, "color", black);
	style->background_color = color_from_property(match, "background-color", transparent);

	style->border_style_0 = border_style_from_property(match, "border-top-style");
	style->border_style_1 = border_style_from_property(match, "border-right-style");
	style->border_style_2 = border_style_from_property(match, "border-bottom-style");
	style->border_style_3 = border_style_from_property(match, "border-left-style");

	style->border_color[0] = color_from_property(match, "border-top-color", style->color);
	style->border_color[1] = color_from_property(match, "border-right-color", style->color);
	style->border_color[2] = color_from_property(match, "border-bottom-color", style->color);
	style->border_color[3] = color_from_property(match, "border-left-color", style->color);

	style->border_width[0] = border_width_from_property(match, "border-top-width");
	style->border_width[1] = border_width_from_property(match, "border-right-width");
	style->border_width[2] = border_width_from_property(match, "border-bottom-width");
	style->border_width[3] = border_width_from_property(match, "border-left-width");

	{
		const char *font_weight = string_from_property(match, "font-weight", "normal");
		const char *font_style = string_from_property(match, "font-style", "normal");
		const char *font_variant = string_from_property(match, "font-variant", "normal");
		int is_bold = is_bold_from_font_weight(font_weight);
		int is_italic = is_italic_from_font_style(font_style);
		style->small_caps = !strcmp(font_variant, "small-caps");
		value = value_from_property(match, "font-family");
		while (value)
		{
			if (strcmp(value->data, ",") != 0)
			{
				style->font = fz_load_html_font(ctx, set, value->data, is_bold, is_italic, style->small_caps);
				if (style->font)
					break;
			}
			value = value->next;
		}
		if (!style->font)
			style->font = fz_load_html_font(ctx, set, "serif", is_bold, is_italic, style->small_caps);
	}
}

#ifdef DEBUG_CSS_SPLAY
static void
do_verify_splay(const fz_css_style_splay *x)
{
	printf("%x<", x);
	if (x->lt)
	{
		assert(memcmp(&x->lt->style, &x->style, sizeof(x->style)) < 0);
		assert(x->lt->up == x);
		do_verify_splay(x->lt);
	}
	printf(",");
	if (x->gt)
	{
		assert(memcmp(&x->gt->style, &x->style, sizeof(x->style)) > 0);
		assert(x->gt->up == x);
		do_verify_splay(x->gt);
	}
	printf(">\n");
}

static void
verify_splay(const fz_css_style_splay *x)
{
	if (x == NULL)
		return;
	assert(x->up == NULL);
	do_verify_splay(x);
	printf("-----\n");
}
#endif

/* Lookup style in the splay tree, returning a pointer to the found instance
 * if there is one, creating and inserting (and moving to root) one if there
 * is not. */
const fz_css_style *
fz_css_enlist(fz_context *ctx, const fz_css_style *style, fz_css_style_splay **tree, fz_pool *pool)
{
	fz_css_style_splay **current = tree;
	fz_css_style_splay *x;
	fz_css_style_splay *y = NULL;

	/* Search for a match in the tree, if there is one, or for
	 * the insertion point, if there is not. */
	while (*current != NULL)
	{
		int cmp = memcmp(style, &(*current)->style, sizeof(*style));
		if (cmp == 0)
		{
			/* We have a match - break out and do move to root. */
			break;
		}
		y = (*current);
		if (cmp < 0)
			current = &y->lt;
		else
			current = &y->gt;
	}
	/* Create one if needed */
	if (*current == NULL)
	{
		x = *current = fz_pool_alloc(ctx, pool, sizeof(*y));
		x->style = *style;
		x->up = y;
		x->lt = NULL;
		x->gt = NULL;
	}
	else
		x = *current;
	/* Now move to root */
	/*
	The splaying steps used:

	Case 1:	|a)       z              x             b)     z                   x
		|     y       D  =>  A       y            A       y           y       D
		|   x   C                  B   z                B   x  =>  z     C
		|  A B                        C D                  C D    A B

	Case 2:	|a)       z              x             b)     z                   x
		|     y       D  =>   y     z             A       y    =>     z       y
		|   A   x            A B   C D                  x   D        A B     C D
		|      B C                                     B C

	Case 3:	|a)       y              x             b)     y                   x
		|      x     C   =>   A     y              A     x      =>     y     C
		|     A B                  B C                  B C           A B
	*/
#ifdef DEBUG_CSS_SPLAY
	printf("BEFORE\n");
	verify_splay(*tree);
#endif
	while ((y = x->up) != NULL ) /* While we're not at the root */
	{
		fz_css_style_splay *z = y->up;
		y->up = x;
		if (z == NULL)
		{
			if (y->lt == x)	/* Case 3a */
			{
				y->lt = x->gt;
				if (y->lt)
					y->lt->up = y;
				x->gt = y;
			}
			else /* Case 3b */
			{
				y->gt = x->lt;
				if (y->gt)
					y->gt->up = y;
				x->lt = y;
			}
			x->up = NULL;
			break;
		}
		x->up = z->up;
		if (z->up)
		{
			if (z->up->lt == z)
				z->up->lt = x;
			else
				z->up->gt = x;
		}
		if (z->lt == y)
		{
			if (y->lt == x) /* Case 1a */
			{
				z->lt = y->gt;
				if (z->lt)
					z->lt->up = z;
				y->lt = x->gt;
				if (y->lt)
					y->lt->up = y;
				y->gt = z;
				z->up = y;
				x->gt = y;
			}
			else /* Case 2a */
			{
				y->gt = x->lt;
				if (y->gt)
					y->gt->up = y;
				z->lt = x->gt;
				if (z->lt)
					z->lt->up = z;
				x->lt = y;
				x->gt = z;
				z->up = x;
			}
		}
		else
		{
			if (y->gt == x) /* Case 1b */
			{
				z->gt = y->lt;
				if (z->gt)
					z->gt->up = z;
				y->gt = x->lt;
				if (y->gt)
					y->gt->up = y;
				y->lt = z;
				z->up = y;
				x->lt = y;
			}
			else /* Case 2b */
			{
				z->gt = x->lt;
				if (z->gt)
					z->gt->up = z;
				y->lt = x->gt;
				if (y->lt)
					y->lt->up = y;
				x->gt = y;
				x->lt = z;
				z->up = x;
			}
		}
	}

	*tree = x;
#ifdef DEBUG_CSS_SPLAY
	printf("AFTER\n");
	verify_splay(x);
#endif

	return &x->style;
}
/*
 * Pretty printing
 */

static void print_value(fz_css_value *val)
{
	printf("%s", val->data);
	if (val->args)
	{
		printf("(");
		print_value(val->args);
		printf(")");
	}
	if (val->next)
	{
		printf(" ");
		print_value(val->next);
	}
}

static void print_property(fz_css_property *prop)
{
	printf("\t%s: ", prop->name);
	print_value(prop->value);
	if (prop->important)
		printf(" !important");
	printf(";\n");
}

static void print_condition(fz_css_condition *cond)
{
	if (cond->type == '=')
		printf("[%s=%s]", cond->key, cond->val);
	else if (cond->type == '[')
		printf("[%s]", cond->key);
	else
		printf("%c%s", cond->type, cond->val);
	if (cond->next)
		print_condition(cond->next);
}

static void print_selector(fz_css_selector *sel)
{
	if (sel->combine)
	{
		print_selector(sel->left);
		if (sel->combine == ' ')
			printf(" ");
		else
			printf(" %c ", sel->combine);
		print_selector(sel->right);
	}
	else if (sel->name)
		printf("%s", sel->name);
	else
		printf("*");
	if (sel->cond)
	{
		print_condition(sel->cond);
	}
}

static void print_rule(fz_css_rule *rule)
{
	fz_css_selector *sel;
	fz_css_property *prop;

	for (sel = rule->selector; sel; sel = sel->next)
	{
		print_selector(sel);
		printf(" /* %d */", selector_specificity(sel, 0));
		if (sel->next)
			printf(", ");
	}

	printf("\n{\n");
	for (prop = rule->declaration; prop; prop = prop->next)
	{
		print_property(prop);
	}
	printf("}\n");
}

void
fz_debug_css(fz_context *ctx, fz_css *css)
{
	fz_css_rule *rule = css->rule;
	while (rule)
	{
		print_rule(rule);
		rule = rule->next;
	}
}
