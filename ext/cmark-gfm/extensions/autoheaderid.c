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

/* SumatraPDF: a character github-slugger drops: punctuation (except '-' and
 * '_'), controls and symbols such as emoji, arrows and Latin-1 signs. */
static int slug_drops(int32_t c) {
  if (c == '-' || c == '_')
    return 0;
  if (c < 0x20 || c == 0x7f || cmark_utf8proc_is_punctuation(c))
    return 1;
  /* Latin-1 signs: all of 0xA0..0xBF but the letters and numbers among them */
  if (c >= 0xa0 && c <= 0xbf)
    return !(c == 0xaa || c == 0xb2 || c == 0xb3 || c == 0xb5 || c == 0xb9 || c == 0xba ||
             (c >= 0xbc && c <= 0xbe));
  if (c == 0xd7 || c == 0xf7)
    return 1;
  /* arrows, math and technical symbols, dingbats, emoji, variation selectors, ZWJ */
  return (c >= 0x2190 && c <= 0x2bff) || (c >= 0x1f000 && c <= 0x1faff) ||
         (c >= 0xfe00 && c <= 0xfe0f) || c == 0x200d;
}

/* Appends c lowercased as JavaScript's toLowerCase() does. Case folding is
 * the same except it expands some lowercase letters: keep those. */
static void slug_append_lower(cmark_strbuf *out, int32_t c, const uint8_t *bytes, bufsize_t n) {
  if (c == 0x1e9e) /* capital sharp s */
    c = 0xdf;
  if (c == 0xdf || c == 0x3c2 || (c >= 0xfb00 && c <= 0xfb06)) {
    cmark_utf8proc_encode_char(c, out);
    return;
  }
  cmark_utf8proc_case_fold(out, bytes, n);
}

static void heading_slug(cmark_strbuf *out, const unsigned char *p, const unsigned char *e) {
  while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
    p++;
  while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
    e--;
  while (p < e) {
    int32_t c;
    int n = cmark_utf8proc_iterate(p, (bufsize_t)(e - p), &c);
    if (n < 0) {
      /* not UTF-8: keep the byte */
      cmark_strbuf_putc(out, *p);
      p++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
      cmark_strbuf_putc(out, '-');
    else if (!slug_drops(c))
      slug_append_lower(out, c, p, n);
    p += n;
  }
}

char *cmark_gfm_heading_slug(const char *s, size_t len, size_t *out_len) {
  cmark_strbuf buf = CMARK_BUF_INIT(cmark_get_default_mem_allocator());
  heading_slug(&buf, (const unsigned char *)s, (const unsigned char *)s + len);
  if (out_len)
    *out_len = (size_t)buf.size;
  return (char *)cmark_strbuf_detach(&buf);
}

/* SumatraPDF: the heading's id is GitHub's anchor for it (cmark_gfm_heading_slug,
 * which MarkdownHeadingSlug() in src/shared/MarkdownToc.cpp uses for the
 * WebView2 view too), so "#café-au-lait" links work in both. The id is on the
 * heading, not on an <a> around it, so a link lands on the heading's page when
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
    cmark_html_render_cr(html);
    start_heading[2] = (char)('0' + node->as.heading.level);
    cmark_strbuf_puts(html, start_heading);
    cmark_strbuf_puts(html, " id=\"");
    heading_slug(html, node->content.ptr, node->content.ptr + node->content.size);
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
