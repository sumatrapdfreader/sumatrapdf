/*
    WDL - xmlparse.h
    Copyright (C) 2016 and later, Cockos Incorporated
  
    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
      
very very very lightweight XML parser

reads: <?xml, <!DOCTYPE, <![CDATA[, &lt;&gt;&amp;&quot;&apos;&#xABC;&#123; ignores unknown <?tag blocks?>
always uses 8-bit characters, uses UTF-8 encoding for &#xyz
relatively strict. for overflow safety, enforces a token length limit of 512MB

*/

#ifndef _WDL_XML_PARSE_H_
#define _WDL_XML_PARSE_H_
#include "ptrlist.h"
#include "assocarray.h"
#include "wdlstring.h"
#include "wdlutf8.h"

class wdl_xml_element {
    static int attr_cmp(char **a, char **b) { return strcmp(*a,*b); }
    static void attr_free(char *a) { free(a); }
  public:
    wdl_xml_element(const char *_name, int _line, int _col, bool _sort_attr=true) : 
      attributes(attr_cmp,NULL,attr_free,attr_free), name(strdup(_name)), line(_line), col(_col), 
      m_sort_attributes(_sort_attr), m_has_discrete_close(false) { }
    ~wdl_xml_element() { free(name); elements.Empty(true); }

    WDL_PtrList<wdl_xml_element> elements;
    WDL_AssocArray<char *, char *> attributes;
    WDL_FastString value; // value excluding any leading whitespace and excluding any elements

    char *name;
    int line, col;
    bool m_sort_attributes;
    bool m_has_discrete_close;

    const char *get_attribute(const char *v, const char *def=NULL) const
    {
      if (!m_sort_attributes)
      {
        const int n = attributes.GetSize();
        for (int x = 0; x < n; x ++) 
        {
          char *key = NULL;
          const char *val = attributes.Enumerate(x,&key);
          if (key && !strcmp(key,v)) return val;
        }
      }
      return attributes.Get((char*)v,(char*)def);
    }
};

class wdl_xml_parser {
  public:
    wdl_xml_parser(const char *rdptr, int rdptr_len, bool sort_attributes=true) : 
      element_xml(NULL), element_root(NULL), 
      m_rdptr((const unsigned char *)rdptr), m_err(NULL),
      m_rdptr_len(rdptr_len), m_line(1), m_col(0), m_lastchar(0), 
      m_last_line(1),m_last_col(0),
      m_sort_attributes(sort_attributes)
    { 
    }
    virtual ~wdl_xml_parser() 
    { 
      delete element_xml; 
      delete element_root; 
      element_doctype_tokens.Empty(true,free); 
    }

    const char *parse() // call only once, returns NULL on success, error message on failure
    {
      m_lastchar = nextchar();

      if (!m_tok.ResizeOK(256)) return "token buffer malloc fail";

      const char *p = parse_element_body(NULL);
      if (m_err) return m_err;
      if (p) return p;
      if (get_tok()) return "document: extra characters following root element";

      return NULL;
    }

    // output
    WDL_PtrList<char> element_doctype_tokens; // tokens after <!DOCTYPE
    wdl_xml_element *element_xml, *element_root;

    // get location  after parse() returns error
    int getLine() const { return m_last_line; }
    int getCol() const { return m_last_col; }


  private:

    WDL_HeapBuf m_tok;
    const unsigned char *m_rdptr;
    const char *m_err;
    int m_rdptr_len, m_line, m_col, m_lastchar, m_last_line,m_last_col;
    bool m_sort_attributes;

    virtual int moredata(const char **dataOut) { return 0; }

    int nextchar()
    {
      if (m_rdptr_len < 1 && (m_rdptr_len = moredata((const char **)&m_rdptr)) < 1) return -1;

      m_rdptr_len--;
      const int ret = (int)*m_rdptr++;

      if (ret == '\n') { m_line++; m_col=0; }
      else m_col++;

      return ret;
    }

    int skip_whitespace()
    {
      int rv=0, lc = m_lastchar;
      while (char_type(lc) < 0) { lc = nextchar(); rv++; }
      m_lastchar = lc;
      return rv;
    }

    static int char_type(int c)
    {
      switch (c)
      {
        case ' ': case '\r': case '\n': case '\t':
          return -1;

        case '/': case '!': case '\\': case '\'': case '"': case '#': case '$': 
        case '%': case '(': case ')': case '*': case '+': case ',': case ';': 
        case '=': case '>': case '?': case '@': case '[': case ']': case '^': 
        case '`': case '{': case '|': case '}': case '~':
          return 1;

        case '<': case '&':
          return 2;

        case '-': case '.': 
          return 4;
      }
      return 0;
    }

    unsigned char *realloc_tok(int &tok_sz)
    {
      tok_sz += tok_sz + tok_sz / 4;
      if (tok_sz >= (1<<29)) 
      {
        m_err="token buffer tried to malloc() more than 512MB, probably unsafe and invalid XML";
        return NULL;
      }
      unsigned char *t = (unsigned char *) m_tok.ResizeOK(tok_sz);
      if (!t) m_err="token buffer malloc fail";
      return t;
    }

    // gets a token, normally skipping whitespace, but if get_tok(true), then return NULL on whitespace
    const char *get_tok(bool no_skip_whitespace=false)
    {
      if (!no_skip_whitespace) skip_whitespace();

      m_last_line = m_line;
      m_last_col = m_col;

      int wrpos=0, lc = m_lastchar, tok_sz = m_tok.GetSize();
      unsigned char *tok_buf = (unsigned char *)m_tok.Get();
      switch (lc > 0 ? char_type(lc) : -2)
      {
        case 0:
          do
          {
            tok_buf[wrpos++] = lc;
            if (WDL_unlikely(wrpos >= tok_sz) && WDL_unlikely(!(tok_buf=realloc_tok(tok_sz)))) return NULL;
            lc = nextchar();
          }
          while (lc > 0 && !(char_type(lc)&~4));
        break;

        case 1:
        case 2:
        case 4:
          if (lc == '\'' || lc == '\"')
          {
            const int endc = lc;
            tok_buf[wrpos++] = lc;
            lc = nextchar();
            while (lc > 0)
            {
              if (lc == '<')
              {
                m_last_line=m_line; m_last_col=m_col;
                m_err="illegal '<' character in quoted string";
                m_lastchar = lc;
                return NULL;
              }

              if (lc == '&')
              {
                m_lastchar = lc;
                if (WDL_unlikely(wrpos+8 >= tok_sz) && WDL_unlikely(!(tok_buf=realloc_tok(tok_sz)))) return NULL;

                const int tmp[2]={m_line,m_col};
                if (!decode_entity((char*)tok_buf+wrpos)) 
                {
                  m_last_line=tmp[0]; m_last_col=tmp[1];
                  m_err="unknown entity in quoted string";
                  return NULL;
                }
                lc = m_lastchar;
                while (tok_buf[wrpos]) wrpos++;
              }
              else
              {
                const int llc = lc;
                tok_buf[wrpos++] = lc;
                if (WDL_unlikely(wrpos >= tok_sz) && WDL_unlikely(!(tok_buf=realloc_tok(tok_sz)))) return NULL;
                lc = nextchar();

                if (llc == endc) break;
              }
            }
          }
          else
          {
            tok_buf[wrpos++] = lc;
            if (WDL_unlikely(wrpos >= tok_sz) && WDL_unlikely(!(tok_buf=realloc_tok(tok_sz)))) return NULL;

            lc = nextchar();
          }
        break;
        case -1:
          m_err="unexpected whitespace";
        return NULL; 
        default: 
          m_err="unexpected end of file";
        return NULL; 
      }
      tok_buf[wrpos]=0;
      m_lastchar = lc;
      return (char *)tok_buf;
    }

    bool decode_entity(char *wr) // will never write more than 8 bytes
    {
      char tmp[32];
      int i=0;
      while (i < 31 && (m_lastchar = nextchar()) > 0 && m_lastchar != ';')
      {
        if (char_type(m_lastchar) && m_lastchar != '#') break;
        tmp[i++] = m_lastchar;
      }
      int byteval = 0;
      if (m_lastchar == ';') 
      {
        tmp[i]=0;
        if (!strcmp(tmp,"lt")) byteval = '<';
        else if (!strcmp(tmp,"gt")) byteval = '>';
        else if (!strcmp(tmp,"amp")) byteval = '&';
        else if (!strcmp(tmp,"apos")) byteval = '\'';
        else if (!strcmp(tmp,"quot")) byteval = '"';
        else if (tmp[0] == '#')
        {
          if (tmp[1] >= '0' && tmp[1] <= '9') byteval = atoi(tmp+1);
          if (tmp[1] == 'x') byteval = strtol(tmp+1,NULL,16);
        }
      }
      if (!byteval) return false;
      WDL_MakeUTFChar((char*)wr,byteval,8);
      m_lastchar = nextchar();
      return true;
    }

    bool skip_until(const char *tok, const char *a, const char *b)
    {
      bool state=false;
      if (!tok) tok = get_tok();
      while (tok)
      {
        if (state && !strcmp(b,tok)) return true;
        state = !strcmp(a,tok);
        if (state && !b) return true;

        if (skip_whitespace()) state=false;
        tok = get_tok(true);
      }

      return false;
    }

    const char *parse_element_attributes(wdl_xml_element *elem)
    {
      char *attr_name=NULL;
      for (;;)
      {
        const char *tok = get_tok();
        if (!tok) break;

        if (*tok == '-' || *tok == '.' || (*tok >= '0' && *tok <= '9')) { m_err="attribute must not begin with .- or number"; break; }

        if (char_type(*tok)) return tok;

        attr_name = strdup(tok);
        if (!attr_name) { m_err="malloc fail"; break; }

        if (m_sort_attributes && 
            elem->attributes.Get(attr_name)) 
        { 
          m_err="attribute specified more than once"; 
          break; 
        }

        tok = get_tok();
        if (!tok) break;
        if (*tok != '=') { m_err="attribute name must be followed by '='"; break; }

        tok = get_tok();
        if (!tok) break;
        if (*tok != '\'' && *tok != '"') { m_err="attribute value must be quoted string"; break; }

        const size_t tok_len = strlen(tok);
        if (tok_len < 2 || tok[tok_len-1] != tok[0]) { m_err="attribute value missing trailing quote"; break;  }

        char *value = (char *)malloc(tok_len-2+1);
        if (!value) { m_err="malloc fail"; break; }

        memcpy(value,tok+1,tok_len-2);
        value[tok_len-2]=0;

        if (m_sort_attributes)
          elem->attributes.Insert(attr_name,value);
        else
          elem->attributes.AddUnsorted(attr_name,value);

        attr_name = NULL;
      }
      free(attr_name);
      return NULL;
    }

    const char *parse_element_body(wdl_xml_element *elem) // return NULL on success, error message on failure
    {
      int cnt=0;
      for (;;)
      {
        if (elem)
        {
          bool want_add = elem->value.GetLength() > 0;
          while (m_lastchar != '<' && m_lastchar > 0)
          { 
            if (!want_add && char_type(m_lastchar)>=0) want_add=true;

            bool adv=true;
            if (m_lastchar == '&')
            {
              m_last_line=m_line; m_last_col=m_col;
              char buf[8];
              if (!decode_entity(buf)) return "unknown entity in element body";
              elem->value.Append(buf);
              adv=false;
            }
            else if (want_add)
            {
              unsigned char c = (unsigned char)m_lastchar;
              elem->value.Append((const char *)&c,1);
            }

            if (adv) m_lastchar = nextchar();
          }
        }

        const char *tok = get_tok(elem != NULL);
        const int start_line = m_last_line, start_col = m_last_col;
        if (!tok) return elem ? "unterminated block" : NULL;
        if (*tok != '<') return "expected < tag";
    
        tok = get_tok(true);
        if (!tok) return "expected token after <";

        if (tok[0] == '!')
        {
          tok = get_tok(true);
          if (!tok) return "expected token following <!";

          if (*tok == '-')
          {
            tok = get_tok(true);
            if (!tok) return "expected token following <!-";
            if (*tok != '-') return "unknown token following <!-";
            if (!skip_until(NULL,"-","-")) 
            {
              m_last_line=start_line;
              m_last_col=start_col;
              return m_err = "unterminated comment";
            }
            tok = get_tok(true);
            if (!tok || tok[0] != '>') return "-- not allowed in comment";
          }
          else if (*tok == '[')
          {
            if (!elem) return "<![ not allowed at document level";
            tok = get_tok(true);
            if (!tok || strcmp(tok,"CDATA")) return "unknown token beginning <![";
            tok=get_tok(true);
            if (!tok || tok[0] != '[') return "unknown token beginning <![CDATA but without trailing [";

            // add content literally until ]]>
            int lc=m_lastchar, last1=0,last2=0;
            for (;;)
            {
              if (lc == '>' && last1 == ']' && last2 == ']') break;

              unsigned char c = (unsigned char)lc;
              elem->value.Append((const char *)&c,1);
              last2 = last1;
              last1 = lc;

              lc = nextchar(); 
              if (lc <= 0)
              {
                m_lastchar = -1;
                m_last_line=start_line;
                m_last_col=start_col;
                return m_err = "unterminated <![CDATA[";
              }
            }
            elem->value.SetLen(elem->value.GetLength()-2); // remove ]]
            m_lastchar = nextchar(); 

          }
          else if (!strcmp(tok,"DOCTYPE"))
          {
            if (elem) return "<!DOCTYPE must be at top level";
            if (element_doctype_tokens.GetSize()) return "<!DOCTYPE already specified";

            tok = get_tok();
            if (!tok || char_type(*tok)) return "expected document type token following <!DOCTYPE";
            do
            {
              element_doctype_tokens.Add(strdup(tok));
              tok = get_tok();
              if (!tok) 
              {
                m_last_line=start_line;
                m_last_col=start_col;
                return m_err = "unterminated <!DOCTYPE";
              }
            } while (tok[0] != '>');
          }
          else return "unknown token following <!";
        }
        else if (tok[0] == '?')
        {
          tok = get_tok(true);
          if (!tok) return "expected token following <?";

          if (!strcmp(tok,"xml"))
          {
            if (elem || cnt || element_xml) return "<?xml must begin document";

            element_xml = new wdl_xml_element("xml",start_line,start_col,m_sort_attributes);
            tok = parse_element_attributes(element_xml);
            if (!tok || tok[0] != '?' || !(tok=get_tok(true)) || tok[0] != '>')
              return "<?xml not terminated";
          }
          else
          {
            if (!skip_until(tok, "?",">")) 
            {
              m_last_line=start_line;
              m_last_col=start_col;
              return m_err = "unterminated <? block";
            }
          }
        }
        else if (tok[0] == '/')
        {
          if (!elem) return "unexpected </ at root level";

          tok = get_tok(true);
          if (strcmp(tok,elem->name)) 
          {
            return "mismatched </ tag name";
          }

          tok = get_tok();
          if (!tok || tok[0] != '>') return "expected > following </tag";
          // done!
          elem->m_has_discrete_close = true;
          return NULL;
        }
        else
        {
          if (*tok == '-' || *tok == '.' || (*tok >= '0' && *tok <= '9'))
            return "element name must not begin with .- or number";

          wdl_xml_element *sub = new wdl_xml_element(tok,start_line,start_col,m_sort_attributes);
          if (elem) elem->elements.Add(sub);
          else element_root = sub;

          tok = parse_element_attributes(sub);
          if (!tok) return "unterminated element";

          if (*tok == '/') 
          {
            tok = get_tok(true);
            if (!tok || *tok != '>') return "expected > following / to end element";
          }
          else if (*tok == '>')
          {
            const char *ret = parse_element_body(sub);
            if (ret) return ret;
          }
          else 
          {
            return "unknown token in element"; 
          }
          if (!elem) return NULL; // finish after parsing a top level block
        }
        cnt++;
      }
    }
};

class wdl_xml_fileread : public wdl_xml_parser {
  FILE *m_fp;
  char m_buf[1024];
  int m_charset;

  virtual int moredata(const char **dataptr) 
  { 
    *dataptr = m_buf;
    const int cs = m_charset;
    if (m_fp) switch (cs)
    {
      case 0:
        return (int) fread(m_buf,1,sizeof(m_buf),m_fp);
      case 1:
      case 2:
        {
          unsigned char tmp[128];
          const int l = (int) fread(tmp,1,sizeof(tmp),m_fp);
          int rd=0, wpos=0;
          while (rd+1 < l)
          {
            const int amt=wdl_utf8_makechar(cs==1 ? ((tmp[rd]<<8)|tmp[rd+1]) : (tmp[rd]|(tmp[rd+1]<<8)), 
                m_buf+wpos,
                (int)sizeof(m_buf)-wpos);
            if (amt>0) wpos += amt;

            rd+=2;
          }
          return wpos;
        }
    }
    return 0;
  }
public:
  wdl_xml_fileread(FILE *fp) : wdl_xml_parser(NULL,0) 
  { 
    m_fp=fp; 
    m_charset=0; // default to utf-8
    if (fp)
    {
      unsigned char bom[2];
      if (fread(bom,1,2,fp)==2)
      {
        if (bom[0] == 0xEF && bom[1] == 0xBB && fgetc(fp) == 0xBF) m_charset=0;
        else if (bom[0] == 0xFE && bom[1] == 0xFF) m_charset=1; // utf-16 BE
        else if (bom[0] == 0xFF && bom[1] == 0xFE) m_charset=2; // utf-16 LE
        else fseek(fp,0,SEEK_SET); // rewind
      }
    }
  }
  virtual ~wdl_xml_fileread() { if (m_fp) fclose(m_fp); }
};

#endif

