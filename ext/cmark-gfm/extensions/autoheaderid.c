#include "autoheaderid.h"
#include <html.h>
#include <parser.h>
#include <render.h>
#include <string.h>
#include <utf8.h>
#include <stddef.h>

#if defined(_WIN32)
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

static cmark_node *postprocess(cmark_syntax_extension *ext, cmark_parser *parser, cmark_node *root) {
  cmark_iter *iter;
  cmark_event_type ev;
  cmark_node *node;

  cmark_consolidate_text_nodes(root);
  iter = cmark_iter_new(root);

  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    node = cmark_iter_get_node(iter);
    if (node->type == CMARK_NODE_HEADING && node->extension == NULL)
      node->extension = ext;
  }

  cmark_iter_free(iter);

  return root;
}

/* SumatraPDF: the heading's id is GitHub's anchor for it, as
 * MarkdownHeadingSlug() (src/shared/MarkdownToc.cpp) makes for the WebView2
 * view, so "#café-au-lait" links work in both: ASCII letters lowercased,
 * other characters (UTF-8 bytes >= 0x80) kept, digits, '_' and '-' kept, each
 * whitespace a '-', other punctuation dropped. The id is on the heading,
 * not on an <a> around it, so a link lands on the heading's page when
 * pagination moves the heading to the next one. (It decoded UTF-8 itself,
 * skipping a byte after every 2-byte character, and wrote non-ASCII
 * characters as single bytes, so such anchors never matched.) */
static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  cmark_strbuf *html = renderer->html;
  char start_heading[] = "<h0";
  char end_heading[] = "</h0";
  (void)extension;
  if (entering) {
    unsigned char *p = node->content.ptr;
    unsigned char *e = p + node->content.size;
    while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
      p++;
    while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
      e--;
    cmark_html_render_cr(html);
    start_heading[2] = (char)('0' + node->as.heading.level);
    cmark_strbuf_puts(html, start_heading);
    cmark_strbuf_puts(html, " id=\"");
    for (; p < e; p++) {
      unsigned int c = *p;
      if (c >= 'A' && c <= 'Z')
        c += 'a' - 'A';
      if (c >= 0x80 || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
        cmark_strbuf_putc(html, (int)c);
      else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        cmark_strbuf_putc(html, '-');
    }
    cmark_strbuf_putc(html, '"');
    cmark_html_render_sourcepos(node, html, options);
    cmark_strbuf_putc(html, '>');
  } else {
    end_heading[3] = (char)('0' + node->as.heading.level);
    cmark_strbuf_puts(html, end_heading);
    cmark_strbuf_puts(html, ">\n");
  }
}

cmark_syntax_extension *create_autoheaderid_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("autoheaderid");

  cmark_syntax_extension_set_postprocess_func(ext, postprocess);
  cmark_syntax_extension_set_html_render_func(ext, html_render);

  return ext;
}
