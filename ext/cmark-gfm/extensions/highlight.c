#include "highlight.h"

#include <parser.h>
#include <render.h>
#include <string.h>

cmark_node_type CMARK_NODE_HIGHLIGHT;

/* Obsidian ==highlight==. A run of exactly two '=' can open and close even
 * beside spaces ("== highlighted text =="). Longer runs stay plain text.
 * Code spans never reach this scanner. */
static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_node *parent, unsigned char character,
                         cmark_inline_parser *inline_parser) {
  cmark_node *res = NULL;
  int left_flanking, right_flanking, punct_before, punct_after, delims;
  char buffer[101];

  (void)self;
  (void)parent;

  if (character != '=')
    return NULL;

  delims = cmark_inline_parser_scan_delimiters(
      inline_parser, sizeof(buffer) - 1, '=', &left_flanking, &right_flanking,
      &punct_before, &punct_after);
  if (delims <= 0)
    return NULL;

  memset(buffer, '=', delims);
  buffer[delims] = 0;

  res = cmark_node_new_with_mem(CMARK_NODE_TEXT, parser->mem);
  if (res == NULL)
    return NULL;
  cmark_node_set_literal(res, buffer);
  res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
  res->start_column = cmark_inline_parser_get_column(inline_parser) - delims;

  if (delims == 2) {
    cmark_inline_parser_push_delimiter(inline_parser, character, 1, 1, res);
  }
  return res;
}

static delimiter *insert(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_inline_parser *inline_parser, delimiter *opener,
                         delimiter *closer) {
  cmark_node *mark;
  cmark_node *tmp, *next;
  delimiter *delim, *tmp_delim;
  delimiter *res = closer->next;

  (void)parser;
  mark = opener->inl_text;

  if (opener->inl_text->as.literal.len != closer->inl_text->as.literal.len)
    goto done;

  if (!cmark_node_set_type(mark, CMARK_NODE_HIGHLIGHT))
    goto done;

  cmark_node_set_syntax_extension(mark, self);

  tmp = cmark_node_next(opener->inl_text);
  while (tmp) {
    if (tmp == closer->inl_text)
      break;
    next = cmark_node_next(tmp);
    cmark_node_append_child(mark, tmp);
    tmp = next;
  }

  mark->end_column = closer->inl_text->start_column +
                     closer->inl_text->as.literal.len - 1;
  cmark_node_free(closer->inl_text);

done:
  delim = closer;
  while (delim != NULL && delim != opener) {
    tmp_delim = delim->previous;
    cmark_inline_parser_remove_delimiter(inline_parser, delim);
    delim = tmp_delim;
  }
  cmark_inline_parser_remove_delimiter(inline_parser, opener);
  return res;
}

static const char *get_type_string(cmark_syntax_extension *extension,
                                   cmark_node *node) {
  (void)extension;
  return node->type == CMARK_NODE_HIGHLIGHT ? "highlight" : "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  (void)extension;
  if (node->type != CMARK_NODE_HIGHLIGHT)
    return 0;
  return CMARK_NODE_TYPE_INLINE_P(child_type);
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  (void)extension;
  (void)node;
  (void)options;
  if (ev_type == CMARK_EVENT_ENTER)
    cmark_strbuf_puts(renderer->html, "<mark>");
  else
    cmark_strbuf_puts(renderer->html, "</mark>");
}

cmark_syntax_extension *create_highlight_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("highlight");
  cmark_llist *special_chars = NULL;
  cmark_mem *mem;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  CMARK_NODE_HIGHLIGHT = cmark_syntax_extension_add_node(1);

  cmark_syntax_extension_set_match_inline_func(ext, match);
  cmark_syntax_extension_set_inline_from_delim_func(ext, insert);

  mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)'=');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);
  cmark_syntax_extension_set_emphasis(ext, 1);
  return ext;
}
