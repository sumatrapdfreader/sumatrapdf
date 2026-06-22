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

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  cmark_strbuf *html = renderer->html;
  char start_heading[] = "<h0";
  char end_heading[] = "</h0";
    if (entering) {
      unsigned char *p = node->content.ptr;
      unsigned char *e = p + node->content.size;
      int minus_pending = 0;
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "<a id=\"");
      for (; p < e; p++)
      {
        unsigned int c = *p;

        /* UTF 8 reading. */
        if (c >= 0x80)
        {
          if (c >= 0xF8)
            goto bad;/* Not a valid UTF-8 codepoint */
          if (c >= 0xF0)
          {
            if (p+3 >= e)
              goto bad;
            c = ((p[0] & 0x07)<<18) + ((p[1] & 0x3f)<<12) + ((p[2] & 0x3f)<<6) + (p[3] & 0x3f);
            p += 3;
          }
          else if (c >= 0xE0)
          {
            if (p+2 >= e)
              goto bad;
            c = ((p[0] & 0x0F)<<12) + ((p[1] & 0x3f)<<6) + (p[2] & 0x3f);
            p += 2;
          }
          else if (c >= 0xC0)
          {
            if (p+1 >= e)
              goto bad;
            c = ((p[0] & 0x1F)<<6) + (p[1] & 0x3f);
            p += 2;
          }
          else
            goto bad;
        }

        /* C is unicode. */
        if (c >= 'A' && c <= 'Z')
        {
          c += 'a' - 'A';
        }
        else if (c >= 'a' && c <= 'z')
        {
        }
        else if (c >= '0' && c <= '9')
        {
        }
        else if (c >= 0xa0 && c <= 0xbf)
        {
          /* Ascii punctuation in unicode */
          goto bad;
        }
        else if ((c >= 0xc0 && c <= 0xc5) || (c >= 0xE0 && c <= 0xeb) || (c >= 0x100 && c <= 0x105))
        {
          /* A or a with accents */
          c = 'a';
        }
        else if (c == 0xC7 || c == 0xE7)
        {
          /* C cedilla */
          c = 'c';
        }
        else if ((c >= 0xc8 && c <= 0xcb) || (c >= 0xE8 && c <= 0xeb))
        {
          /* E or e with accents */
          c = 'e';
        }
        else if ((c >= 0xcc && c <= 0xcf) || (c >= 0xEC && c <= 0xef))
        {
          /* I or i with accents */
          c = 'i';
        }
        else if (c == 0xD1 || c == 0xF1)
        {
          /* N or n with tilde */
          c = 'n';
        }
        else if ((c >= 0xd2 && c <= 0xd8) || (c >= 0xEC && c <= 0xef))
        {
          /* O or o with accents */
          c = 'o';
        }
        else if ((c >= 0xd9 && c <= 0xdc) || (c >= 0xF9 && c <= 0xfc))
        {
          /* U or u with accents */
          c = 'u';
        }
        else if (c >= 0x80)
        {
          /* Assuming we're using valid UTF8 here, this means we've got
           * a unicode char. We don't have the tables to do fiddling, so
           * just send this through as is. */
        }
        else
        {
bad:
          c = '-';
        }

        /* C is now 0 or a 7 bit value. */
        if (c == '-')
          minus_pending = 1;
        else if (c)
        {
          if (minus_pending)
            cmark_strbuf_putc(html, '-');
          cmark_strbuf_putc(html, c);
          minus_pending = 0;
        }
      }
      cmark_strbuf_puts(html, "\">");
      start_heading[2] = (char)('0' + node->as.heading.level);
      cmark_strbuf_puts(html, start_heading);
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      end_heading[3] = (char)('0' + node->as.heading.level);
      cmark_strbuf_puts(html, end_heading);
      cmark_strbuf_puts(html, ">\n");
      cmark_strbuf_puts(html, "</a>");
    }
}

cmark_syntax_extension *create_autoheaderid_extension(void) {
  cmark_syntax_extension *ext = cmark_syntax_extension_new("autoheaderid");

  cmark_syntax_extension_set_postprocess_func(ext, postprocess);
  cmark_syntax_extension_set_html_render_func(ext, html_render);

  return ext;
}
