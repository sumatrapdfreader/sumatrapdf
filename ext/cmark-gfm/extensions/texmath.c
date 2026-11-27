#include "texmath.h"

#include <houdini.h>
#include <parser.h>
#include <render.h>

cmark_node_type CMARK_NODE_MATH;

/* TeX math, as GitHub writes it: $inline$, $$display$$ and $`inline`$.
 * The TeX is taken verbatim, so markdown never sees it: "\," and "\\" keep
 * their backslashes and "a_k b_k" doesn't become emphasis. The renderer
 * emits it HTML-escaped in <span class="math math-inline|math-display">,
 * for a script on the page to typeset.
 *
 * Inline $...$ follows pandoc's rule so prices stay text ("$5 and $10"):
 * the opening '$' is followed by a non-space, the closing '$' is preceded by
 * a non-space and not followed by a digit. Inside $ and $$ a backslash
 * escapes the next character, so "\$" doesn't close. A '\' before the
 * opening '$' is a markdown escape and never reaches this scanner, nor do
 * code spans. Display math may span lines of the same paragraph. */

static int is_space(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int has_non_space(const unsigned char *s, int start, int end) {
  for (int i = start; i < end; i++) {
    if (!is_space(s[i]))
      return 1;
  }
  return 0;
}

/* s[pos] is '$'. On a match returns the offset just past the closing
 * delimiter and sets the TeX's start and length; returns -1 otherwise. */
static int scan_math(const unsigned char *s, int len, int pos, int *tex_start,
                     int *tex_len, int *display) {
  int start, i;

  if (pos + 1 < len && s[pos + 1] == '$') {
    start = pos + 2;
    for (i = start; i + 1 < len; i++) {
      if (s[i] == '\\') {
        i++;
        continue;
      }
      if (s[i] == '$' && s[i + 1] == '$') {
        if (!has_non_space(s, start, i))
          return -1;
        *tex_start = start;
        *tex_len = i - start;
        *display = 1;
        return i + 2;
      }
    }
    return -1;
  }

  if (pos + 1 < len && s[pos + 1] == '`') {
    start = pos + 2;
    for (i = start; i + 1 < len; i++) {
      if (s[i] == '`' && s[i + 1] == '$') {
        if (!has_non_space(s, start, i))
          return -1;
        *tex_start = start;
        *tex_len = i - start;
        *display = 0;
        return i + 2;
      }
    }
    return -1;
  }

  start = pos + 1;
  if (start >= len || is_space(s[start]))
    return -1;
  for (i = start; i < len; i++) {
    if (s[i] == '\\') {
      i++;
      continue;
    }
    if (s[i] != '$')
      continue;
    if (is_space(s[i - 1]))
      continue;
    if (i + 1 < len && s[i + 1] >= '0' && s[i + 1] <= '9')
      continue;
    *tex_start = start;
    *tex_len = i - start;
    *display = 0;
    return i + 1;
  }
  return -1;
}

static cmark_node *match(cmark_syntax_extension *self, cmark_parser *parser,
                         cmark_node *parent, unsigned char character,
                         cmark_inline_parser *inline_parser) {
  cmark_node *res;
  cmark_chunk *chunk;
  int pos, end, tex_start = 0, tex_len = 0, display = 0;

  (void)parent;

  if (character != '$')
    return NULL;

  chunk = cmark_inline_parser_get_chunk(inline_parser);
  pos = cmark_inline_parser_get_offset(inline_parser);
  end = scan_math(chunk->data, chunk->len, pos, &tex_start, &tex_len, &display);
  if (end < 0)
    return NULL;

  res = cmark_node_new_with_mem_and_ext(CMARK_NODE_MATH, parser->mem, self);
  if (res == NULL)
    return NULL;
  cmark_strbuf_put(&res->content, chunk->data + tex_start, tex_len);
  res->as.opaque = display ? (void *)1 : NULL;
  res->start_line = res->end_line = cmark_inline_parser_get_line(inline_parser);
  res->start_column = cmark_inline_parser_get_column(inline_parser);
  res->end_column = res->start_column + (end - pos) - 1;

  cmark_inline_parser_set_offset(inline_parser, end);
  return res;
}

int cmark_gfm_extensions_is_math(cmark_node *node) {
  return node && node->type == CMARK_NODE_MATH;
}

int cmark_gfm_extensions_math_is_display(cmark_node *node) {
  return cmark_gfm_extensions_is_math(node) && node->as.opaque != NULL;
}

static const char *get_type_string(cmark_syntax_extension *extension,
                                   cmark_node *node) {
  (void)extension;
  return node->type == CMARK_NODE_MATH ? "math" : "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  (void)extension;
  (void)node;
  (void)child_type;
  return 0;
}

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  (void)extension;
  (void)options;
  if (ev_type != CMARK_EVENT_ENTER)
    return;
  cmark_strbuf_puts(renderer->html, cmark_gfm_extensions_math_is_display(node)
                                        ? "<span class=\"math math-display\">"
                                        : "<span class=\"math math-inline\">");
  houdini_escape_html0(renderer->html, node->content.ptr, node->content.size, 0);
  cmark_strbuf_puts(renderer->html, "</span>");
}

cmark_syntax_extension *create_math_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("math");
  cmark_llist *special_chars = NULL;
  cmark_mem *mem;

  cmark_syntax_extension_set_get_type_string_func(ext, get_type_string);
  cmark_syntax_extension_set_can_contain_func(ext, can_contain);
  cmark_syntax_extension_set_html_render_func(ext, html_render);
  CMARK_NODE_MATH = cmark_syntax_extension_add_node(1);

  cmark_syntax_extension_set_match_inline_func(ext, match);

  mem = cmark_get_default_mem_allocator();
  special_chars = cmark_llist_append(mem, special_chars, (void *)'$');
  cmark_syntax_extension_set_special_inline_chars(ext, special_chars);
  return ext;
}
