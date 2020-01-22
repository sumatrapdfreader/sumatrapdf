#include "../fitz/fitz-imp.h"
#include "mupdf/ucdn.h"
#include "html-imp.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

enum { T, R, B, L };

#define DEFAULT_DIR FZ_BIDI_LTR

static const char *html_default_css =
"@page{margin:3em 2em}"
"a{color:#06C;text-decoration:underline}"
"address{display:block;font-style:italic}"
"b{font-weight:bold}"
"bdo{direction:rtl;unicode-bidi:bidi-override}"
"blockquote{display:block;margin:1em 40px}"
"body{display:block;margin:1em}"
"cite{font-style:italic}"
"code{font-family:monospace}"
"dd{display:block;margin:0 0 0 40px}"
"del{text-decoration:line-through}"
"div{display:block}"
"dl{display:block;margin:1em 0}"
"dt{display:block}"
"em{font-style:italic}"
"h1{display:block;font-size:2em;font-weight:bold;margin:0.67em 0;page-break-after:avoid}"
"h2{display:block;font-size:1.5em;font-weight:bold;margin:0.83em 0;page-break-after:avoid}"
"h3{display:block;font-size:1.17em;font-weight:bold;margin:1em 0;page-break-after:avoid}"
"h4{display:block;font-size:1em;font-weight:bold;margin:1.33em 0;page-break-after:avoid}"
"h5{display:block;font-size:0.83em;font-weight:bold;margin:1.67em 0;page-break-after:avoid}"
"h6{display:block;font-size:0.67em;font-weight:bold;margin:2.33em 0;page-break-after:avoid}"
"head{display:none}"
"hr{border-style:solid;border-width:1px;display:block;margin-bottom:0.5em;margin-top:0.5em;text-align:center}"
"html{display:block}"
"i{font-style:italic}"
"ins{text-decoration:underline}"
"kbd{font-family:monospace}"
"li{display:list-item}"
"menu{display:block;list-style-type:disc;margin:1em 0;padding:0 0 0 30pt}"
"ol{display:block;list-style-type:decimal;margin:1em 0;padding:0 0 0 30pt}"
"p{display:block;margin:1em 0}"
"pre{display:block;font-family:monospace;margin:1em 0;white-space:pre}"
"samp{font-family:monospace}"
"script{display:none}"
"small{font-size:0.83em}"
"strong{font-weight:bold}"
"style{display:none}"
"sub{font-size:0.83em;vertical-align:sub}"
"sup{font-size:0.83em;vertical-align:super}"
"table{display:table}"
"tbody{display:table-row-group}"
"td{display:table-cell;padding:1px}"
"tfoot{display:table-footer-group}"
"th{display:table-cell;font-weight:bold;padding:1px;text-align:center}"
"thead{display:table-header-group}"
"tr{display:table-row}"
"ul{display:block;list-style-type:disc;margin:1em 0;padding:0 0 0 30pt}"
"ul ul{list-style-type:circle}"
"ul ul ul{list-style-type:square}"
"var{font-style:italic}"
"svg{display:none}"
;

static const char *fb2_default_css =
"@page{margin:3em 2em}"
"FictionBook{display:block;margin:1em}"
"stylesheet,binary{display:none}"
"description>*{display:none}"
"description>title-info{display:block}"
"description>title-info>*{display:none}"
"description>title-info>coverpage{display:block;page-break-before:always;page-break-after:always}"
"body,section,title,subtitle,p,cite,epigraph,text-author,date,poem,stanza,v,empty-line{display:block}"
"image{display:block}"
"p>image{display:inline}"
"table{display:table}"
"tr{display:table-row}"
"th,td{display:table-cell}"
"a{color:#06C;text-decoration:underline}"
"a[type=note]{font-size:small;vertical-align:super}"
"code{white-space:pre;font-family:monospace}"
"emphasis{font-style:italic}"
"strikethrough{text-decoration:line-through}"
"strong{font-weight:bold}"
"sub{font-size:small;vertical-align:sub}"
"sup{font-size:small;vertical-align:super}"
"image{margin:1em 0;text-align:center}"
"cite,poem{margin:1em 2em}"
"subtitle,epigraph,stanza{margin:1em 0}"
"title>p{text-align:center;font-size:x-large}"
"subtitle{text-align:center;font-size:large}"
"p{margin-top:1em;text-align:justify}"
"empty-line{padding-top:1em}"
"p+p{margin-top:0;text-indent:1.5em}"
"empty-line+p{margin-top:0}"
"section>title{page-break-before:always}"
;

struct genstate
{
	fz_pool *pool;
	fz_html_font_set *set;
	fz_archive *zip;
	fz_tree *images;
	int is_fb2;
	const char *base_uri;
	fz_css *css;
	int at_bol;
	int emit_white;
	int last_brk_cls;
	fz_css_style_splay *styles;
};

static int iswhite(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_all_white(const char *s)
{
	while (*s)
	{
		if (!iswhite(*s))
			return 0;
		++s;
	}
	return 1;
}

/* TODO: pool allocator for flow nodes */
/* TODO: store text by pointing to a giant buffer */

static void fz_drop_html_flow(fz_context *ctx, fz_html_flow *flow)
{
	while (flow)
	{
		fz_html_flow *next = flow->next;
		if (flow->type == FLOW_IMAGE)
			fz_drop_image(ctx, flow->content.image);
		flow = next;
	}
}

static fz_html_flow *add_flow(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box, int type, int extras)
{
	size_t size = (type == FLOW_IMAGE ? sizeof(fz_html_flow) : offsetof(fz_html_flow, content) + extras);
	fz_html_flow *flow = fz_pool_alloc(ctx, pool, size);
	flow->type = type;
	flow->expand = 0;
	flow->bidi_level = 0;
	flow->markup_lang = 0;
	flow->breaks_line = 0;
	flow->box = inline_box;
	*top->flow_tail = flow;
	top->flow_tail = &flow->next;
	return flow;
}

static void add_flow_space(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box)
{
	fz_html_flow *flow = add_flow(ctx, pool, top, inline_box, FLOW_SPACE, 0);
	flow->expand = 1;
}

static void add_flow_break(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box)
{
	(void)add_flow(ctx, pool, top, inline_box, FLOW_BREAK, 0);
}

static void add_flow_sbreak(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box)
{
	(void)add_flow(ctx, pool, top, inline_box, FLOW_SBREAK, 0);
}

static void add_flow_shyphen(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box)
{
	(void)add_flow(ctx, pool, top, inline_box, FLOW_SHYPHEN, 0);
}

static void add_flow_word(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box, const char *a, const char *b, int lang)
{
	fz_html_flow *flow = add_flow(ctx, pool, top, inline_box, FLOW_WORD, b - a + 1);
	memcpy(flow->content.text, a, b - a);
	flow->content.text[b - a] = 0;
	flow->markup_lang = lang;
}

static void add_flow_image(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box, fz_image *img)
{
	fz_html_flow *flow = add_flow(ctx, pool, top, inline_box, FLOW_IMAGE, 0);
	flow->content.image = fz_keep_image(ctx, img);
}

static void add_flow_anchor(fz_context *ctx, fz_pool *pool, fz_html_box *top, fz_html_box *inline_box)
{
	(void)add_flow(ctx, pool, top, inline_box, FLOW_ANCHOR, 0);
}

static fz_html_flow *split_flow(fz_context *ctx, fz_pool *pool, fz_html_flow *flow, size_t offset)
{
	fz_html_flow *new_flow;
	char *text;
	size_t len;

	assert(flow->type == FLOW_WORD);

	if (offset == 0)
		return flow;
	text = flow->content.text;
	while (*text && offset)
	{
		int rune;
		text += fz_chartorune(&rune, text);
		offset--;
	}
	len = strlen(text);
	new_flow = fz_pool_alloc(ctx, pool, offsetof(fz_html_flow, content) + len+1);
	memcpy(new_flow, flow, offsetof(fz_html_flow, content));
	new_flow->next = flow->next;
	flow->next = new_flow;
	strcpy(new_flow->content.text, text);
	*text = 0;
	return new_flow;
}

static void flush_space(fz_context *ctx, fz_html_box *flow, fz_html_box *inline_box, int lang, struct genstate *g)
{
	static const char *space = " ";
	int bsp = inline_box->style->white_space & WS_ALLOW_BREAK_SPACE;
	fz_pool *pool = g->pool;
	if (g->emit_white)
	{
		if (!g->at_bol)
		{
			if (bsp)
				add_flow_space(ctx, pool, flow, inline_box);
			else
				add_flow_word(ctx, pool, flow, inline_box, space, space+1, lang);
		}
		g->emit_white = 0;
	}
}

/* pair-wise lookup table for UAX#14 linebreaks */
static const char *pairbrk[29] =
{
/*	-OCCQGNESIPPNAHIIHBBBZCWHHJJJR- */
/*	-PLPULSXYSROULLDNYAB2WMJ23LVTI- */
	"^^^^^^^^^^^^^^^^^^^^^^^^^^^^^", /* OP open punctuation */
	"_^^%%^^^^%%_____%%__^^^______", /* CL close punctuation */
	"_^^%%^^^^%%%%%__%%__^^^______", /* CP close parenthesis */
	"^^^%%%^^^%%%%%%%%%%%^^^%%%%%%", /* QU quotation */
	"%^^%%%^^^%%%%%%%%%%%^^^%%%%%%", /* GL non-breaking glue */
	"_^^%%%^^^_______%%__^^^______", /* NS nonstarters */
	"_^^%%%^^^______%%%__^^^______", /* EX exclamation/interrogation */
	"_^^%%%^^^__%_%__%%__^^^______", /* SY symbols allowing break after */
	"_^^%%%^^^__%%%__%%__^^^______", /* IS infix numeric separator */
	"%^^%%%^^^__%%%%_%%__^^^%%%%%_", /* PR prefix numeric */
	"%^^%%%^^^__%%%__%%__^^^______", /* PO postfix numeric */
	"%^^%%%^^^%%%%%_%%%__^^^______", /* NU numeric */
	"%^^%%%^^^__%%%_%%%__^^^______", /* AL ordinary alphabetic and symbol characters */
	"%^^%%%^^^__%%%_%%%__^^^______", /* HL hebrew letter */
	"_^^%%%^^^_%____%%%__^^^______", /* ID ideographic */
	"_^^%%%^^^______%%%__^^^______", /* IN inseparable characters */
	"_^^%_%^^^__%____%%__^^^______", /* HY hyphens */
	"_^^%_%^^^_______%%__^^^______", /* BA break after */
	"%^^%%%^^^%%%%%%%%%%%^^^%%%%%%", /* BB break before */
	"_^^%%%^^^_______%%_^^^^______", /* B2 break opportunity before and after */
	"____________________^________", /* ZW zero width space */
	"%^^%%%^^^__%%%_%%%__^^^______", /* CM combining mark */
	"%^^%%%^^^%%%%%%%%%%%^^^%%%%%%", /* WJ word joiner */
	"_^^%%%^^^_%____%%%__^^^___%%_", /* H2 hangul leading/vowel syllable */
	"_^^%%%^^^_%____%%%__^^^____%_", /* H3 hangul leading/vowel/trailing syllable */
	"_^^%%%^^^_%____%%%__^^^%%%%__", /* JL hangul leading jamo */
	"_^^%%%^^^_%____%%%__^^^___%%_", /* JV hangul vowel jamo */
	"_^^%%%^^^_%____%%%__^^^____%_", /* JT hangul trailing jamo */
	"_^^%%%^^^_______%%__^^^_____%", /* RI regional indicator */
};

static void generate_text(fz_context *ctx, fz_html_box *box, const char *text, int lang, struct genstate *g)
{
	fz_html_box *flow;
	fz_pool *pool = g->pool;
	int collapse = box->style->white_space & WS_COLLAPSE;
	int bsp = box->style->white_space & WS_ALLOW_BREAK_SPACE;
	int bnl = box->style->white_space & WS_FORCE_BREAK_NEWLINE;

	static const char *space = " ";

	flow = box;
	while (flow->type != BOX_FLOW)
		flow = flow->up;

	while (*text)
	{
		if (bnl && (*text == '\n' || *text == '\r'))
		{
			if (text[0] == '\r' && text[1] == '\n')
				text += 2;
			else
				text += 1;
			add_flow_break(ctx, pool, flow, box);
			g->at_bol = 1;
		}
		else if (iswhite(*text))
		{
			if (collapse)
			{
				if (bnl)
					while (*text == ' ' || *text == '\t')
						++text;
				else
					while (iswhite(*text))
						++text;
				g->emit_white = 1;
			}
			else
			{
				// TODO: tabs
				if (bsp)
					add_flow_space(ctx, pool, flow, box);
				else
					add_flow_word(ctx, pool, flow, box, space, space+1, lang);
				++text;
			}
			g->last_brk_cls = UCDN_LINEBREAK_CLASS_WJ; /* don't add sbreaks after a space */
		}
		else
		{
			const char *prev, *mark = text;
			int c;

			flush_space(ctx, flow, box, lang, g);

			if (g->at_bol)
				g->last_brk_cls = UCDN_LINEBREAK_CLASS_WJ;

			while (*text && !iswhite(*text))
			{
				prev = text;
				text += fz_chartorune(&c, text);
				if (c == 0xAD) /* soft hyphen */
				{
					if (mark != prev)
						add_flow_word(ctx, pool, flow, box, mark, prev, lang);
					add_flow_shyphen(ctx, pool, flow, box);
					mark = text;
					g->last_brk_cls = UCDN_LINEBREAK_CLASS_WJ; /* don't add sbreaks after a soft hyphen */
				}
				else if (bsp) /* allow soft breaks */
				{
					int this_brk_cls = ucdn_get_resolved_linebreak_class(c);
					if (this_brk_cls < UCDN_LINEBREAK_CLASS_RI)
					{
						int brk = pairbrk[g->last_brk_cls][this_brk_cls];

						/* we handle spaces elsewhere, so ignore these classes */
						if (brk == '@') brk = '^';
						if (brk == '#') brk = '^';
						if (brk == '%') brk = '^';

						if (brk == '_')
						{
							if (mark != prev)
								add_flow_word(ctx, pool, flow, box, mark, prev, lang);
							add_flow_sbreak(ctx, pool, flow, box);
							mark = prev;
						}

						g->last_brk_cls = this_brk_cls;
					}
				}
			}
			if (mark != text)
				add_flow_word(ctx, pool, flow, box, mark, text, lang);

			g->at_bol = 0;
		}
	}
}

static fz_image *load_html_image(fz_context *ctx, fz_archive *zip, const char *base_uri, const char *src)
{
	char path[2048];
	fz_image *img = NULL;
	fz_buffer *buf = NULL;

	fz_var(img);
	fz_var(buf);

	fz_try(ctx)
	{
		if (!strncmp(src, "data:image/jpeg;base64,", 23))
			buf = fz_new_buffer_from_base64(ctx, src+23, 0);
		else if (!strncmp(src, "data:image/png;base64,", 22))
			buf = fz_new_buffer_from_base64(ctx, src+22, 0);
		else
		{
			fz_strlcpy(path, base_uri, sizeof path);
			fz_strlcat(path, "/", sizeof path);
			fz_strlcat(path, src, sizeof path);
			fz_urldecode(path);
			fz_cleanname(path);
			buf = fz_read_archive_entry(ctx, zip, path);
		}
#if FZ_ENABLE_SVG
		if (strstr(src, ".svg"))
			img = fz_new_image_from_svg(ctx, buf, base_uri, zip);
		else
#endif
			img = fz_new_image_from_buffer(ctx, buf);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_warn(ctx, "html: cannot load image src='%s'", src);

	return img;
}

static fz_image *load_svg_image(fz_context *ctx, fz_archive *zip, const char *base_uri, fz_xml *xml)
{
	fz_image *img = NULL;
	fz_try(ctx)
		img = fz_new_image_from_svg_xml(ctx, xml, base_uri, zip);
	fz_catch(ctx)
		fz_warn(ctx, "html: cannot load embedded svg document");
	return img;
}

static void generate_anchor(fz_context *ctx, fz_html_box *box, struct genstate *g)
{
	fz_pool *pool = g->pool;
	fz_html_box *flow = box;
	while (flow->type != BOX_FLOW)
		flow = flow->up;
	add_flow_anchor(ctx, pool, flow, box);
}

static void generate_image(fz_context *ctx, fz_html_box *box, fz_image *img, struct genstate *g)
{
	fz_html_box *flow = box;
	fz_pool *pool = g->pool;
	while (flow->type != BOX_FLOW)
		flow = flow->up;

	flush_space(ctx, flow, box, 0, g);

	if (!img)
	{
		const char *alt = "[image]";
		add_flow_word(ctx, pool, flow, box, alt, alt + 7, 0);
	}
	else
	{
		fz_try(ctx)
		{
			add_flow_sbreak(ctx, pool, flow, box);
			add_flow_image(ctx, pool, flow, box, img);
			add_flow_sbreak(ctx, pool, flow, box);
		}
		fz_always(ctx)
		{
			fz_drop_image(ctx, img);
		}
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	g->at_bol = 0;
}

static void init_box(fz_context *ctx, fz_html_box *box, fz_bidi_direction markup_dir)
{
	box->type = BOX_BLOCK;
	box->x = box->y = 0;
	box->w = box->b = 0;

	box->up = NULL;
	box->down = NULL;
	box->next = NULL;

	box->flow_head = NULL;
	box->flow_tail = &box->flow_head;
	box->markup_dir = markup_dir;
	box->style = NULL;
}

static void fz_drop_html_box(fz_context *ctx, fz_html_box *box)
{
	while (box)
	{
		fz_html_box *next = box->next;
		fz_drop_html_flow(ctx, box->flow_head);
		fz_drop_html_box(ctx, box->down);
		box = next;
	}
}

static void fz_drop_html_imp(fz_context *ctx, fz_storable *stor)
{
	fz_html *html = (fz_html *)stor;
	fz_drop_html_box(ctx, html->root);
	fz_drop_pool(ctx, html->pool);
}

void fz_drop_html(fz_context *ctx, fz_html *html)
{
	fz_defer_reap_start(ctx);
	fz_drop_storable(ctx, &html->storable);
	fz_defer_reap_end(ctx);
}

fz_html *fz_keep_html(fz_context *ctx, fz_html *html)
{
	return fz_keep_storable(ctx, &html->storable);
}

static fz_html_box *new_box(fz_context *ctx, fz_pool *pool, fz_bidi_direction markup_dir)
{
	fz_html_box *box = fz_pool_alloc(ctx, pool, sizeof *box);
	init_box(ctx, box, markup_dir);
	return box;
}

static fz_html_box *new_short_box(fz_context *ctx, fz_pool *pool, fz_bidi_direction markup_dir)
{
	fz_html_box *box = fz_pool_alloc(ctx, pool, offsetof(fz_html_box, padding));
	init_box(ctx, box, markup_dir);
	return box;
}

static void insert_box(fz_context *ctx, fz_html_box *box, int type, fz_html_box *top)
{
	box->type = type;

	box->up = top;

	if (top)
	{
		/* Here 'next' really means 'last of my children'. This will
		 * be fixed up in a pass at the end of parsing. */
		if (!top->next)
		{
			top->down = top->next = box;
		}
		else
		{
			top->next->next = box;
			/* Here next actually means next */
			top->next = box;
		}
	}
}

static fz_html_box *insert_block_box(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	if (top->type == BOX_BLOCK)
	{
		insert_box(ctx, box, BOX_BLOCK, top);
	}
	else if (top->type == BOX_FLOW)
	{
		while (top->type != BOX_BLOCK)
			top = top->up;
		insert_box(ctx, box, BOX_BLOCK, top);
	}
	else if (top->type == BOX_INLINE)
	{
		while (top->type != BOX_BLOCK)
			top = top->up;
		insert_box(ctx, box, BOX_BLOCK, top);
	}
	return top;
}

static fz_html_box *insert_table_box(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	top = insert_block_box(ctx, box, top);
	box->type = BOX_TABLE;
	return top;
}

static fz_html_box *insert_table_row_box(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	fz_html_box *table = top;
	while (table && table->type != BOX_TABLE)
		table = table->up;
	if (table)
	{
		insert_box(ctx, box, BOX_TABLE_ROW, table);
		return table;
	}
	fz_warn(ctx, "table-row not inside table element");
	insert_block_box(ctx, box, top);
	return top;
}

static fz_html_box *insert_table_cell_box(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	fz_html_box *tr = top;
	while (tr && tr->type != BOX_TABLE_ROW)
		tr = tr->up;
	if (tr)
	{
		insert_box(ctx, box, BOX_TABLE_CELL, tr);
		return tr;
	}
	fz_warn(ctx, "table-cell not inside table-row element");
	insert_block_box(ctx, box, top);
	return top;
}

static fz_html_box *insert_break_box(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	if (top->type == BOX_BLOCK)
	{
		insert_box(ctx, box, BOX_BREAK, top);
	}
	else if (top->type == BOX_FLOW)
	{
		while (top->type != BOX_BLOCK)
			top = top->up;
		insert_box(ctx, box, BOX_BREAK, top);
	}
	else if (top->type == BOX_INLINE)
	{
		while (top->type != BOX_BLOCK)
			top = top->up;
		insert_box(ctx, box, BOX_BREAK, top);
	}
	return top;
}

static void insert_inline_box(fz_context *ctx, fz_html_box *box, fz_html_box *top, int markup_dir, struct genstate *g)
{
	if (top->type == BOX_FLOW || top->type == BOX_INLINE)
	{
		insert_box(ctx, box, BOX_INLINE, top);
	}
	else
	{
		while (top->type != BOX_BLOCK && top->type != BOX_TABLE_CELL)
			top = top->up;

		/* Here 'next' actually means 'last of my children' */
		if (top->next && top->next->type == BOX_FLOW)
		{
			insert_box(ctx, box, BOX_INLINE, top->next);
		}
		else
		{
			fz_css_style style;
			fz_html_box *flow = new_short_box(ctx, g->pool, markup_dir);
			flow->is_first_flow = !top->next;
			fz_default_css_style(ctx, &style);
			flow->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
			insert_box(ctx, flow, BOX_FLOW, top);
			insert_box(ctx, box, BOX_INLINE, flow);
			g->at_bol = 1;
		}
	}
}

static fz_html_box *
generate_boxes(fz_context *ctx,
	fz_xml *node,
	fz_html_box *top,
	fz_css_match *up_match,
	int list_counter,
	int section_depth,
	int markup_dir,
	int markup_lang,
	struct genstate *g)
{
	fz_css_match match;
	fz_html_box *box, *last_top;
	const char *tag;
	int display;
	fz_css_style style;

	while (node)
	{
		match.up = up_match;
		match.count = 0;

		tag = fz_xml_tag(node);
		if (tag)
		{
			fz_match_css(ctx, &match, g->css, node);

			display = fz_get_css_match_display(&match);

			if (tag[0]=='b' && tag[1]=='r' && tag[2]==0)
			{
				if (top->type == BOX_INLINE)
				{
					fz_html_box *flow = top;
					while (flow->type != BOX_FLOW)
						flow = flow->up;
					add_flow_break(ctx, g->pool, flow, top);
				}
				else
				{
					box = new_short_box(ctx, g->pool, markup_dir);
					fz_apply_css_style(ctx, g->set, &style, &match);
					box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
					top = insert_break_box(ctx, box, top);
				}
				g->at_bol = 1;
			}

			else if (tag[0]=='i' && tag[1]=='m' && tag[2]=='g' && tag[3]==0)
			{
				const char *src = fz_xml_att(node, "src");
				if (src)
				{
					int w, h;
					const char *w_att = fz_xml_att(node, "width");
					const char *h_att = fz_xml_att(node, "height");
					box = new_short_box(ctx, g->pool, markup_dir);
					fz_apply_css_style(ctx, g->set, &style, &match);
					if (w_att && (w = fz_atoi(w_att)) > 0)
					{
						style.width.value = w;
						style.width.unit = strchr(w_att, '%') ? N_PERCENT : N_LENGTH;
					}
					if (h_att && (h = fz_atoi(h_att)) > 0)
					{
						style.height.value = h;
						style.height.unit = strchr(h_att, '%') ? N_PERCENT : N_LENGTH;
					}
					box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
					insert_inline_box(ctx, box, top, markup_dir, g);
					generate_image(ctx, box, load_html_image(ctx, g->zip, g->base_uri, src), g);
				}
			}

			else if (tag[0]=='s' && tag[1]=='v' && tag[2]=='g' && tag[3]==0)
			{
				box = new_short_box(ctx, g->pool, markup_dir);
				fz_apply_css_style(ctx, g->set, &style, &match);
				box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
				insert_inline_box(ctx, box, top, markup_dir, g);
				generate_image(ctx, box, load_svg_image(ctx, g->zip, g->base_uri, node), g);
			}

			else if (g->is_fb2 && tag[0]=='i' && tag[1]=='m' && tag[2]=='a' && tag[3]=='g' && tag[4]=='e' && tag[5]==0)
			{
				const char *src = fz_xml_att(node, "l:href");
				if (!src)
					src = fz_xml_att(node, "xlink:href");
				if (src && src[0] == '#')
				{
					fz_image *img = fz_tree_lookup(ctx, g->images, src+1);
					if (display == DIS_BLOCK)
					{
						fz_html_box *imgbox;
						box = new_box(ctx, g->pool, markup_dir);
						fz_default_css_style(ctx, &style);
						fz_apply_css_style(ctx, g->set, &style, &match);
						box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
						top = insert_block_box(ctx, box, top);
						imgbox = new_short_box(ctx, g->pool, markup_dir);
						fz_apply_css_style(ctx, g->set, &style, &match);
						imgbox->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
						insert_inline_box(ctx, imgbox, box, markup_dir, g);
						generate_image(ctx, imgbox, fz_keep_image(ctx, img), g);
					}
					else if (display == DIS_INLINE)
					{
						box = new_short_box(ctx, g->pool, markup_dir);
						fz_apply_css_style(ctx, g->set, &style, &match);
						box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
						insert_inline_box(ctx, box, top, markup_dir, g);
						generate_image(ctx, box, fz_keep_image(ctx, img), g);
					}
				}
			}

			else if (display != DIS_NONE)
			{
				const char *dir, *lang, *id, *href;
				int child_dir = markup_dir;
				int child_lang = markup_lang;

				dir = fz_xml_att(node, "dir");
				if (dir)
				{
					if (!strcmp(dir, "auto"))
						child_dir = FZ_BIDI_NEUTRAL;
					else if (!strcmp(dir, "rtl"))
						child_dir = FZ_BIDI_RTL;
					else if (!strcmp(dir, "ltr"))
						child_dir = FZ_BIDI_LTR;
					else
						child_dir = DEFAULT_DIR;
				}

				lang = fz_xml_att(node, "lang");
				if (lang)
					child_lang = fz_text_language_from_string(lang);

				if (display == DIS_INLINE)
					box = new_short_box(ctx, g->pool, child_dir);
				else
					box = new_box(ctx, g->pool, child_dir);
				fz_default_css_style(ctx, &style);
				fz_apply_css_style(ctx, g->set, &style, &match);
				box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);

				id = fz_xml_att(node, "id");
				if (id)
					box->id = fz_pool_strdup(ctx, g->pool, id);

				if (display == DIS_BLOCK || display == DIS_INLINE_BLOCK)
				{
					top = insert_block_box(ctx, box, top);
					if (g->is_fb2)
					{
						if (!strcmp(tag, "title") || !strcmp(tag, "subtitle"))
							box->heading = fz_mini(section_depth, 6);
					}
					else
					{
						if (tag[0]=='h' && tag[1]>='1' && tag[1]<='6' && tag[2]==0)
							box->heading = tag[1] - '0';
					}
				}
				else if (display == DIS_LIST_ITEM)
				{
					top = insert_block_box(ctx, box, top);
					box->list_item = ++list_counter;
				}
				else if (display == DIS_INLINE)
				{
					insert_inline_box(ctx, box, top, child_dir, g);
					if (id)
						generate_anchor(ctx, box, g);
					if (tag[0]=='a' && tag[1]==0)
					{
						if (g->is_fb2)
						{
							href = fz_xml_att(node, "l:href");
							if (!href)
								href = fz_xml_att(node, "xlink:href");
						}
						else
							href = fz_xml_att(node, g->is_fb2 ? "l:href" : "href");
						if (href)
							box->href = fz_pool_strdup(ctx, g->pool, href);
					}
				}
				else if (display == DIS_TABLE)
				{
					top = insert_table_box(ctx, box, top);
				}
				else if (display == DIS_TABLE_ROW)
				{
					top = insert_table_row_box(ctx, box, top);
				}
				else if (display == DIS_TABLE_CELL)
				{
					top = insert_table_cell_box(ctx, box, top);
				}
				else
				{
					fz_warn(ctx, "unknown box display type");
					insert_box(ctx, box, BOX_BLOCK, top);
				}

				if (fz_xml_down(node))
				{
					int child_counter = list_counter;
					int child_section = section_depth;
					if (!strcmp(tag, "ul") || !strcmp(tag, "ol"))
						child_counter = 0;
					if (!strcmp(tag, "section"))
						++child_section;
					last_top = generate_boxes(ctx,
						fz_xml_down(node),
						box,
						&match,
						child_counter,
						child_section,
						child_dir,
						child_lang,
						g);
					if (last_top != box)
						top = last_top;
				}
			}
		}
		else
		{
			const char *text = fz_xml_text(node);
			int collapse = top->style->white_space & WS_COLLAPSE;
			if (collapse && is_all_white(text))
			{
				g->emit_white = 1;
			}
			else
			{
				if (top->type != BOX_INLINE)
				{
					/* Create anonymous inline box, with the same style as the top block box. */
					fz_css_style style;
					box = new_short_box(ctx, g->pool, markup_dir);
					fz_default_css_style(ctx, &style);
					box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
					insert_inline_box(ctx, box, top, markup_dir, g);
					style = *top->style;
					/* Make sure not to recursively multiply font sizes. */
					style.font_size.value = 1;
					style.font_size.unit = N_SCALE;
					box->style = fz_css_enlist(ctx, &style, &g->styles, g->pool);
					generate_text(ctx, box, text, markup_lang, g);
				}
				else
				{
					generate_text(ctx, top, text, markup_lang, g);
				}
			}
		}

		node = fz_xml_next(node);
	}

	return top;
}

static char *concat_text(fz_context *ctx, fz_xml *root)
{
	fz_xml *node;
	size_t i = 0, n = 1;
	char *s;
	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		const char *text = fz_xml_text(node);
		n += text ? strlen(text) : 0;
	}
	s = Memento_label(fz_malloc(ctx, n), "concat_html");
	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		const char *text = fz_xml_text(node);
		if (text)
		{
			n = strlen(text);
			memcpy(s+i, text, n);
			i += n;
		}
	}
	s[i] = 0;
	return s;
}

static void
html_load_css_link(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css *css, fz_xml *root, const char *href)
{
	char path[2048];
	char css_base_uri[2048];
	fz_buffer *buf;

	fz_var(buf);

	fz_strlcpy(path, base_uri, sizeof path);
	fz_strlcat(path, "/", sizeof path);
	fz_strlcat(path, href, sizeof path);
	fz_urldecode(path);
	fz_cleanname(path);

	fz_dirname(css_base_uri, path, sizeof css_base_uri);

	buf = NULL;
	fz_try(ctx)
	{
		buf = fz_read_archive_entry(ctx, zip, path);
		fz_parse_css(ctx, css, fz_string_from_buffer(ctx, buf), path);
		fz_add_css_font_faces(ctx, set, zip, css_base_uri, css);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_warn(ctx, "ignoring stylesheet %s", path);
}

static void
html_load_css(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css *css, fz_xml *root)
{
	fz_xml *html, *head, *node;

	html = fz_xml_find(root, "html");
	head = fz_xml_find_down(html, "head");
	for (node = fz_xml_down(head); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "link"))
		{
			char *rel = fz_xml_att(node, "rel");
			if (rel && !fz_strcasecmp(rel, "stylesheet"))
			{
				char *type = fz_xml_att(node, "type");
				if ((type && !strcmp(type, "text/css")) || !type)
				{
					char *href = fz_xml_att(node, "href");
					if (href)
					{
						html_load_css_link(ctx, set, zip, base_uri, css, root, href);
					}
				}
			}
		}
		else if (fz_xml_is_tag(node, "style"))
		{
			char *s = concat_text(ctx, node);
			fz_try(ctx)
			{
				fz_parse_css(ctx, css, s, "<style>");
				fz_add_css_font_faces(ctx, set, zip, base_uri, css);
			}
			fz_catch(ctx)
				fz_warn(ctx, "ignoring inline stylesheet");
			fz_free(ctx, s);
		}
	}
}

static void
fb2_load_css(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css *css, fz_xml *root)
{
	fz_xml *fictionbook, *stylesheet;

	fictionbook = fz_xml_find(root, "FictionBook");
	stylesheet = fz_xml_find_down(fictionbook, "stylesheet");
	if (stylesheet)
	{
		char *s = concat_text(ctx, stylesheet);
		fz_try(ctx)
		{
			fz_parse_css(ctx, css, s, "<stylesheet>");
			fz_add_css_font_faces(ctx, set, zip, base_uri, css);
		}
		fz_catch(ctx)
			fz_warn(ctx, "ignoring inline stylesheet");
		fz_free(ctx, s);
	}
}

static fz_tree *
load_fb2_images(fz_context *ctx, fz_xml *root)
{
	fz_xml *fictionbook, *binary;
	fz_tree *images = NULL;

	fictionbook = fz_xml_find(root, "FictionBook");
	for (binary = fz_xml_find_down(fictionbook, "binary"); binary; binary = fz_xml_find_next(binary, "binary"))
	{
		const char *id = fz_xml_att(binary, "id");
		char *b64 = NULL;
		fz_buffer *buf = NULL;
		fz_image *img = NULL;

		fz_var(b64);
		fz_var(buf);

		fz_try(ctx)
		{
			b64 = concat_text(ctx, binary);
			buf = fz_new_buffer_from_base64(ctx, b64, strlen(b64));
			img = fz_new_image_from_buffer(ctx, buf);
		}
		fz_always(ctx)
		{
			fz_drop_buffer(ctx, buf);
			fz_free(ctx, b64);
		}
		fz_catch(ctx)
			fz_rethrow(ctx);

		images = fz_tree_insert(ctx, images, id, img);
	}

	return images;
}

typedef struct
{
	uint32_t *data;
	size_t cap;
	size_t len;
} uni_buf;

typedef struct
{
	fz_context *ctx;
	fz_pool *pool;
	fz_html_flow *flow;
	uni_buf *buffer;
} bidi_data;

static void fragment_cb(const uint32_t *fragment,
			size_t fragment_len,
			int bidi_level,
			int script,
			void *arg)
{
	bidi_data *data = (bidi_data *)arg;
	size_t fragment_offset = fragment - data->buffer->data;

	/* We are guaranteed that fragmentOffset will be at the beginning
	 * of flow. */
	while (fragment_len > 0)
	{
		size_t len;

		if (data->flow->type == FLOW_SPACE)
		{
			len = 1;
		}
		else if (data->flow->type == FLOW_BREAK || data->flow->type == FLOW_SBREAK ||
				data->flow->type == FLOW_SHYPHEN || data->flow->type == FLOW_ANCHOR)
		{
			len = 0;
		}
		else
		{
			/* Must be text */
			len = fz_utflen(data->flow->content.text);
			if (len > fragment_len)
			{
				/* We need to split this flow box */
				(void)split_flow(data->ctx, data->pool, data->flow, fragment_len);
				len = fz_utflen(data->flow->content.text);
			}
		}

		/* This flow box is entirely contained within this fragment. */
		data->flow->bidi_level = bidi_level;
		data->flow->script = script;
		data->flow = data->flow->next;
		fragment_offset += len;
		fragment_len -= len;
	}
}

static fz_bidi_direction
detect_flow_directionality(fz_context *ctx, fz_pool *pool, uni_buf *buffer, fz_bidi_direction bidi_dir, fz_html_flow *flow)
{
	fz_html_flow *end = flow;
	bidi_data data;

	while (end)
	{
		int level = end->bidi_level;

		/* Gather the text from the flow up into a single buffer (at
		 * least, as much of it as has the same direction markup). */
		buffer->len = 0;
		while (end && (level & 1) == (end->bidi_level & 1))
		{
			size_t len = 0;
			const char *text = "";
			int broken = 0;

			switch (end->type)
			{
			case FLOW_WORD:
				len = fz_utflen(end->content.text);
				text = end->content.text;
				break;
			case FLOW_SPACE:
				len = 1;
				text = " ";
				break;
			case FLOW_SHYPHEN:
			case FLOW_SBREAK:
				break;
			case FLOW_BREAK:
			case FLOW_IMAGE:
				broken = 1;
				break;
			}

			end = end->next;

			if (broken)
				break;

			/* Make sure the buffer is large enough */
			if (buffer->len + len > buffer->cap)
			{
				size_t newcap = buffer->cap;
				if (newcap < 128)
					newcap = 128; /* Sensible small default */

				while (newcap < buffer->len + len)
					newcap = (newcap * 3) / 2;

				buffer->data = fz_realloc_array(ctx, buffer->data, newcap, uint32_t);
				buffer->cap = newcap;
			}

			/* Expand the utf8 text into Unicode and store it in the buffer */
			while (*text)
			{
				int rune;
				text += fz_chartorune(&rune, text);
				buffer->data[buffer->len++] = rune;
			}
		}

		/* Detect directionality for the buffer */
		data.ctx = ctx;
		data.pool = pool;
		data.flow = flow;
		data.buffer = buffer;
		fz_bidi_fragment_text(ctx, buffer->data, buffer->len, &bidi_dir, fragment_cb, &data, 0 /* Flags */);
		flow = end;
	}
	return bidi_dir;
}

static void
detect_box_directionality(fz_context *ctx, fz_pool *pool, uni_buf *buffer, fz_html_box *box)
{
	while (box)
	{
		if (box->flow_head)
			box->markup_dir = detect_flow_directionality(ctx, pool, buffer, box->markup_dir, box->flow_head);
		detect_box_directionality(ctx, pool, buffer, box->down);
		box = box->next;
	}
}

static void
detect_directionality(fz_context *ctx, fz_pool *pool, fz_html_box *box)
{
	uni_buf buffer = { NULL };

	fz_try(ctx)
		detect_box_directionality(ctx, pool, &buffer, box);
	fz_always(ctx)
		fz_free(ctx, buffer.data);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Here we look for places where box->next actually means
 * 'the last of my children', and correct it by setting
 * next == NULL. We can spot these because box->next->up == box. */
static void
fix_nexts(fz_html_box *box)
{
	while (box)
	{
		if (box->down)
			fix_nexts(box->down);
		if (box->next && box->next->up == box)
		{
			box->next = NULL;
			break;
		}
		box = box->next;
	}
}

fz_html *
fz_parse_html(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css)
{
	fz_xml_doc *xml;
	fz_xml *root, *node;
	fz_html *html = NULL;
	char *title;

	fz_css_match match;
	struct genstate g;

	g.pool = NULL;
	g.set = set;
	g.zip = zip;
	g.images = NULL;
	g.base_uri = base_uri;
	g.css = NULL;
	g.at_bol = 0;
	g.emit_white = 0;
	g.last_brk_cls = UCDN_LINEBREAK_CLASS_OP;
	g.styles = NULL;

	xml = fz_parse_xml(ctx, buf, 1, 1);
	root = fz_xml_root(xml);

	fz_try(ctx)
		g.css = fz_new_css(ctx);
	fz_catch(ctx)
	{
		fz_drop_xml(ctx, xml);
		fz_rethrow(ctx);
	}

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_XML")))
		fz_debug_xml(root, 0);
#endif

	fz_try(ctx)
	{
		if (fz_xml_find(root, "FictionBook"))
		{
			g.is_fb2 = 1;
			fz_parse_css(ctx, g.css, fb2_default_css, "<default:fb2>");
			if (fz_use_document_css(ctx))
				fb2_load_css(ctx, g.set, g.zip, g.base_uri, g.css, root);
			g.images = load_fb2_images(ctx, root);
		}
		else
		{
			g.is_fb2 = 0;
			fz_parse_css(ctx, g.css, html_default_css, "<default:html>");
			if (fz_use_document_css(ctx))
				html_load_css(ctx, g.set, g.zip, g.base_uri, g.css, root);
			g.images = NULL;
		}

		if (user_css)
		{
			fz_parse_css(ctx, g.css, user_css, "<user>");
			fz_add_css_font_faces(ctx, g.set, g.zip, ".", g.css);
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "ignoring styles due to errors: %s", fz_caught_message(ctx));
	}

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_CSS")))
		fz_debug_css(ctx, g.css);
#endif

	fz_try(ctx)
	{
		fz_css_style style;

		g.pool = fz_new_pool(ctx);
		html = fz_pool_alloc(ctx, g.pool, sizeof *html);
		FZ_INIT_STORABLE(html, 1, fz_drop_html_imp);
		html->pool = g.pool;
		html->root = new_box(ctx, g.pool, DEFAULT_DIR);
		html->layout_w = 0;
		html->layout_h = 0;
		html->layout_em = 0;
		fz_default_css_style(ctx, &style);

		match.up = NULL;
		match.count = 0;
		fz_match_css_at_page(ctx, &match, g.css);
		fz_apply_css_style(ctx, g.set, &style, &match);
		html->root->style = fz_css_enlist(ctx, &style, &g.styles, g.pool);
		// TODO: transfer page margins out of this hacky box

		generate_boxes(ctx, root, html->root, &match, 0, 0, DEFAULT_DIR, FZ_LANG_UNSET, &g);
		fix_nexts(html->root);

		detect_directionality(ctx, g.pool, html->root);

		if (g.is_fb2)
		{
			node = fz_xml_find(root, "FictionBook");
			node = fz_xml_find_down(node, "description");
			node = fz_xml_find_down(node, "title-info");
			node = fz_xml_find_down(node, "book-title");
			title = fz_xml_text(fz_xml_down(node));
			if (title)
				html->title = fz_pool_strdup(ctx, g.pool, title);
		}
		else
		{
			node = fz_xml_find(root, "html");
			node = fz_xml_find_down(node, "head");
			node = fz_xml_find_down(node, "title");
			title = fz_xml_text(fz_xml_down(node));
			if (title)
				html->title = fz_pool_strdup(ctx, g.pool, title);
		}
	}
	fz_always(ctx)
	{
		fz_drop_tree(ctx, g.images, (void(*)(fz_context*,void*))fz_drop_image);
		fz_drop_css(ctx, g.css);
		fz_drop_xml(ctx, xml);
	}
	fz_catch(ctx)
	{
		fz_drop_html(ctx, html);
		fz_rethrow(ctx);
	}

	return html;
}

static void indent(int level)
{
	while (level-- > 0)
		putchar('\t');
}

static void
fz_debug_html_flow(fz_context *ctx, fz_html_flow *flow, int level)
{
	fz_html_box *sbox = NULL;
	while (flow)
	{
		if (flow->box != sbox) {
			if (sbox) {
				indent(level);
				printf("}\n");
			}
			sbox = flow->box;
			indent(level);
			printf("span em=%g font='%s'", sbox->em, fz_font_name(ctx, sbox->style->font));
			if (fz_font_is_serif(ctx, sbox->style->font))
				printf(" serif");
			else
				printf(" sans");
			if (fz_font_is_monospaced(ctx, sbox->style->font))
				printf(" monospaced");
			if (fz_font_is_bold(ctx, sbox->style->font))
				printf(" bold");
			if (fz_font_is_italic(ctx, sbox->style->font))
				printf(" italic");
			if (sbox->style->small_caps)
				printf(" small-caps");
			printf("\n");
			indent(level);
			printf("{\n");
		}

		indent(level+1);
		switch (flow->type) {
		case FLOW_WORD: printf("word "); break;
		case FLOW_SPACE: printf("space"); break;
		case FLOW_SBREAK: printf("sbrk "); break;
		case FLOW_SHYPHEN: printf("shy  "); break;
		case FLOW_BREAK: printf("break"); break;
		case FLOW_IMAGE: printf("image"); break;
		case FLOW_ANCHOR: printf("anchor"); break;
		}
		printf(" y=%g x=%g w=%g", flow->y, flow->x, flow->w);
		if (flow->type == FLOW_IMAGE)
			printf(" h=%g", flow->h);
		if (flow->type == FLOW_WORD)
			printf(" text='%s'", flow->content.text);
		printf("\n");
		if (flow->breaks_line) {
			indent(level+1);
			printf("*\n");
		}

		flow = flow->next;
	}
	indent(level);
	printf("}\n");
}

static void
fz_debug_html_box(fz_context *ctx, fz_html_box *box, int level)
{
	while (box)
	{
		indent(level);
		switch (box->type) {
		case BOX_BLOCK: printf("block"); break;
		case BOX_BREAK: printf("break"); break;
		case BOX_FLOW: printf("flow"); break;
		case BOX_INLINE: printf("inline"); break;
		case BOX_TABLE: printf("table"); break;
		case BOX_TABLE_ROW: printf("table-row"); break;
		case BOX_TABLE_CELL: printf("table-cell"); break;
		}

		printf(" em=%g x=%g y=%g w=%g b=%g\n", box->em, box->x, box->y, box->w, box->b);

		indent(level);
		printf("{\n");
		if (box->type == BOX_BLOCK) {
			indent(level+1);
			printf("margin=%g %g %g %g\n", box->margin[0], box->margin[1], box->margin[2], box->margin[3]);
		}
		if (box->is_first_flow) {
			indent(level+1);
			printf("is-first-flow\n");
		}
		if (box->list_item) {
			indent(level+1);
			printf("list=%d\n", box->list_item);
		}
		if (box->id) {
			indent(level+1);
			printf("id=%s\n", box->id);
		}
		if (box->href) {
			indent(level+1);
			printf("href=%s\n", box->href);
		}

		if (box->down)
			fz_debug_html_box(ctx, box->down, level + 1);
		if (box->flow_head)
			fz_debug_html_flow(ctx, box->flow_head, level + 1);

		indent(level);
		printf("}\n");

		box = box->next;
	}
}

void
fz_debug_html(fz_context *ctx, fz_html_box *box)
{
	fz_debug_html_box(ctx, box, 0);
}

static size_t
fz_html_size(fz_context *ctx, fz_html *html)
{
	return html ? fz_pool_size(ctx, html->pool) : 0;
}

/* Magic to make html storable. */
typedef struct {
	int refs;
	void *doc;
	int chapter_num;
} fz_html_key;

static int
fz_make_hash_html_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	fz_html_key *key = (fz_html_key *)key_;
	hash->u.pi.ptr = key->doc;
	hash->u.pi.i = key->chapter_num;
	return 1;
}

static void *
fz_keep_html_key(fz_context *ctx, void *key_)
{
	fz_html_key *key = (fz_html_key *)key_;
	return fz_keep_imp(ctx, key, &key->refs);
}

static void
fz_drop_html_key(fz_context *ctx, void *key_)
{
	fz_html_key *key = (fz_html_key *)key_;
	if (fz_drop_imp(ctx, key, &key->refs))
	{
		fz_free(ctx, key);
	}
}

static int
fz_cmp_html_key(fz_context *ctx, void *k0_, void *k1_)
{
	fz_html_key *k0 = (fz_html_key *)k0_;
	fz_html_key *k1 = (fz_html_key *)k1_;
	return k0->doc == k1->doc && k0->chapter_num == k1->chapter_num;
}

static void
fz_format_html_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	fz_html_key *key = (fz_html_key *)key_;
	fz_snprintf(s, n, "(html doc=%p, ch=%d)", key->doc, key->chapter_num);
}

static const fz_store_type fz_html_store_type =
{
	fz_make_hash_html_key,
	fz_keep_html_key,
	fz_drop_html_key,
	fz_cmp_html_key,
	fz_format_html_key,
	NULL
};

fz_html *fz_store_html(fz_context *ctx, fz_html *html, void *doc, int chapter)
{
	fz_html_key *key = NULL;
	fz_html *other_html;

	/* Stick the parsed html in the store */
	fz_var(key);

	fz_try(ctx)
	{
		key = fz_malloc_struct(ctx, fz_html_key);
		key->refs = 1;
		key->doc = doc;
		key->chapter_num = chapter;
		other_html = fz_store_item(ctx, key, html, fz_html_size(ctx, html), &fz_html_store_type);
		if (other_html)
		{
			fz_drop_html(ctx, html);
			html = other_html;
		}
	}
	fz_always(ctx)
		fz_drop_html_key(ctx, key);
	fz_catch(ctx)
	{
		/* Do nothing */
	}

	return html;
}

fz_html *fz_find_html(fz_context *ctx, void *doc, int chapter)
{
	fz_html_key key;

	key.refs = 1;
	key.doc = doc;
	key.chapter_num = chapter;
	return fz_find_item(ctx, &fz_drop_html_imp, &key, &fz_html_store_type);
}

static int
html_filter_store(fz_context *ctx, void *doc, void *key_)
{
	fz_html_key *key = (fz_html_key *)key_;

	return (doc == key->doc);
}

void fz_purge_stored_html(fz_context *ctx, void *doc)
{
	fz_filter_store(ctx, html_filter_store, doc, &fz_html_store_type);
}
