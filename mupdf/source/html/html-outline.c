#include "mupdf/fitz.h"
#include "html-imp.h"

#include <string.h>

enum { T, R, B, L };

static int is_internal_uri(const char *uri)
{
	while (*uri >= 'a' && *uri <= 'z')
		++uri;
	if (uri[0] == ':' && uri[1] == '/' && uri[2] == '/')
		return 0;
	return 1;
}

static const char *box_href(fz_html_box *box)
{
	while (box)
	{
		const char *href = box->href;
		if (href)
			return href;
		box = box->up;
	}
	return NULL;
}

static int has_same_href(fz_html_box *box, const char *old_href)
{
	while (box)
	{
		const char *href = box->href;
		if (href)
			return !strcmp(old_href, href);
		box = box->up;
	}
	return 0;
}

static fz_link *load_link_flow(fz_context *ctx, fz_html_flow *flow, fz_link *head, int page, float page_h, const char *dir, const char *file)
{
	fz_link *link;
	fz_html_flow *next;
	char path[2048];
	fz_rect bbox;
	const char *dest;
	const char *href;
	float end;

	while (flow)
	{
		href = box_href(flow->box);
		next = flow->next;
		if (href && (int)(flow->y / page_h) == page)
		{
			/* Coalesce contiguous flow boxes into one link node */
			end = flow->x + flow->w;
			while (next &&
					next->y == flow->y &&
					next->h == flow->h &&
					has_same_href(next->box, href))
			{
				end = next->x + next->w;
				next = next->next;
			}

			bbox.x0 = flow->x;
			bbox.y0 = flow->y - page * page_h;
			bbox.x1 = end;
			bbox.y1 = bbox.y0 + flow->h;
			if (flow->type != FLOW_IMAGE)
			{
				/* flow->y is the baseline, adjust bbox appropriately */
				bbox.y0 -= 0.8f * flow->h;
				bbox.y1 -= 0.8f * flow->h;
			}

			if (is_internal_uri(href))
			{
				if (href[0] == '#')
				{
					fz_strlcpy(path, file, sizeof path);
					fz_strlcat(path, href, sizeof path);
				}
				else
				{
					fz_strlcpy(path, dir, sizeof path);
					fz_strlcat(path, "/", sizeof path);
					fz_strlcat(path, href, sizeof path);
				}
				fz_urldecode(path);
				fz_cleanname(path);

				dest = path;
			}
			else
			{
				dest = href;
			}

			link = fz_new_link(ctx, bbox, NULL, dest);
			link->next = head;
			head = link;
		}
		flow = next;
	}
	return head;
}

static fz_link *load_link_box(fz_context *ctx, fz_html_box *box, fz_link *head, int page, float page_h, const char *dir, const char *file)
{
	while (box)
	{
		if (box->flow_head)
			head = load_link_flow(ctx, box->flow_head, head, page, page_h, dir, file);
		if (box->down)
			head = load_link_box(ctx, box->down, head, page, page_h, dir, file);
		box = box->next;
	}
	return head;
}

fz_link *
fz_load_html_links(fz_context *ctx, fz_html *html, int page, const char *file, void *doc)
{
	fz_link *link, *head;
	char dir[2048];
	fz_dirname(dir, file, sizeof dir);

	head = load_link_box(ctx, html->root, NULL, page, html->page_h, dir, file);

	for (link = head; link; link = link->next)
	{
		/* Adjust for page margins */
		link->rect.x0 += html->page_margin[L];
		link->rect.x1 += html->page_margin[L];
		link->rect.y0 += html->page_margin[T];
		link->rect.y1 += html->page_margin[T];

		/* Set document pointer */
		link->doc = doc;
	}

	return head;
}

static fz_html_flow *
find_first_content(fz_html_box *box)
{
	while (box)
	{
		if (box->type == BOX_FLOW)
			return box->flow_head;
		box = box->down;
	}
	return NULL;
}

static float
find_flow_target(fz_html_flow *flow, const char *id)
{
	while (flow)
	{
		if (flow->box->id && !strcmp(id, flow->box->id))
			return flow->y;
		flow = flow->next;
	}
	return -1;
}

static float
find_box_target(fz_html_box *box, const char *id)
{
	float y;
	while (box)
	{
		if (box->id && !strcmp(id, box->id))
		{
			fz_html_flow *flow = find_first_content(box);
			if (flow)
				return flow->y;
			return box->y;
		}
		if (box->type == BOX_FLOW)
		{
			y = find_flow_target(box->flow_head, id);
			if (y >= 0)
				return y;
		}
		else
		{
			y = find_box_target(box->down, id);
			if (y >= 0)
				return y;
		}
		box = box->next;
	}
	return -1;
}

float
fz_find_html_target(fz_context *ctx, fz_html *html, const char *id)
{
	return find_box_target(html->root, id);
}

static fz_html_flow *
make_flow_bookmark(fz_context *ctx, fz_html_flow *flow, float y)
{
	while (flow)
	{
		if (flow->y >= y)
			return flow;
		flow = flow->next;
	}
	return NULL;
}

static fz_html_flow *
make_box_bookmark(fz_context *ctx, fz_html_box *box, float y)
{
	fz_html_flow *mark;
	while (box)
	{
		if (box->type == BOX_FLOW)
		{
			if (box->y >= y)
			{
				mark = make_flow_bookmark(ctx, box->flow_head, y);
				if (mark)
					return mark;
			}
		}
		else
		{
			mark = make_box_bookmark(ctx, box->down, y);
			if (mark)
				return mark;
		}
		box = box->next;
	}
	return NULL;
}

fz_bookmark
fz_make_html_bookmark(fz_context *ctx, fz_html *html, int page)
{
	return (fz_bookmark)make_box_bookmark(ctx, html->root, page * html->page_h);
}

static int
lookup_flow_bookmark(fz_context *ctx, fz_html_flow *flow, fz_html_flow *mark)
{
	while (flow)
	{
		if (flow == mark)
			return 1;
		flow = flow->next;
	}
	return 0;
}

static int
lookup_box_bookmark(fz_context *ctx, fz_html_box *box, fz_html_flow *mark)
{
	while (box)
	{
		if (box->type == BOX_FLOW)
		{
			if (lookup_flow_bookmark(ctx, box->flow_head, mark))
				return 1;
		}
		else
		{
			if (lookup_box_bookmark(ctx, box->down, mark))
				return 1;
		}
		box = box->next;
	}
	return 0;
}

int
fz_lookup_html_bookmark(fz_context *ctx, fz_html *html, fz_bookmark mark)
{
	fz_html_flow *flow = (fz_html_flow*)mark;
	if (flow && lookup_box_bookmark(ctx, html->root, flow))
		return (int)(flow->y / html->page_h);
	return -1;
}

struct outline_parser
{
	fz_html *html;
	fz_buffer *cat;
	fz_outline *head;
	fz_outline **tail[6];
	fz_outline **down[6];
	int level[6];
	int current;
	int id;
};

static void
cat_html_flow(fz_context *ctx, fz_buffer *cat, fz_html_flow *flow)
{
	while (flow)
	{
		switch (flow->type)
		{
		case FLOW_WORD:
			fz_append_string(ctx, cat, flow->content.text);
			break;
		case FLOW_SPACE:
		case FLOW_BREAK:
			fz_append_byte(ctx, cat, ' ');
			break;
		default:
			break;
		}
		flow = flow->next;
	}
}

static void
cat_html_box(fz_context *ctx, fz_buffer *cat, fz_html_box *box)
{
	while (box)
	{
		cat_html_flow(ctx, cat, box->flow_head);
		cat_html_box(ctx, cat, box->down);
		box = box->next;
	}
}

static const char *
cat_html_text(fz_context *ctx, struct outline_parser *x, fz_html_box *box)
{
	if (!x->cat)
		x->cat = fz_new_buffer(ctx, 1024);
	else
		fz_clear_buffer(ctx, x->cat);

	cat_html_flow(ctx, x->cat, box->flow_head);
	cat_html_box(ctx, x->cat, box->down);

	return fz_string_from_buffer(ctx, x->cat);
}

static void
add_html_outline(fz_context *ctx, struct outline_parser *x, fz_html_box *box)
{
	fz_outline *node;
	char buf[100];

	node = fz_new_outline(ctx);
	fz_try(ctx)
	{
		node->title = Memento_label(fz_strdup(ctx, cat_html_text(ctx, x, box)), "outline_title");
		if (!box->id)
		{
			fz_snprintf(buf, sizeof buf, "'%d", x->id++);
			box->id = Memento_label(fz_pool_strdup(ctx, x->html->pool, buf), "box_id");
		}
		node->uri = Memento_label(fz_asprintf(ctx, "#%s", box->id), "outline_uri");
		node->is_open = 1;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, node);
		fz_rethrow(ctx);
	}

	if (x->level[x->current] < (int)box->heading && x->current < 5)
	{
		x->tail[x->current+1] = x->down[x->current];
		x->current += 1;
	}
	else
	{
		while (x->current > 0 && x->level[x->current] > (int)box->heading)
		{
			x->current -= 1;
		}
	}
	x->level[x->current] = box->heading;

	*(x->tail[x->current]) = node;
	x->tail[x->current] = &node->next;
	x->down[x->current] = &node->down;
}

static void
load_html_outline(fz_context *ctx, struct outline_parser *x, fz_html_box *box)
{
	while (box)
	{
		if (box->heading)
			add_html_outline(ctx, x, box);
		if (box->down)
			load_html_outline(ctx, x, box->down);
		box = box->next;
	}
}

fz_outline *
fz_load_html_outline(fz_context *ctx, fz_html *html)
{
	struct outline_parser state;
	state.html = html;
	state.cat = NULL;
	state.head = NULL;
	state.tail[0] = &state.head;
	state.down[0] = NULL;
	state.level[0] = 99;
	state.current = 0;
	state.id = 1;
	fz_try(ctx)
		load_html_outline(ctx, &state, html->root);
	fz_always(ctx)
		fz_drop_buffer(ctx, state.cat);
	fz_catch(ctx)
	{
		fz_drop_outline(ctx, state.head);
		state.head = NULL;
	}
	return state.head;
}
