#ifdef _WIN32
#include <windows.h>
#else
#include "../swell/swell.h"
#endif
#include <stdlib.h>
#include <string.h>
#ifndef CURSES_INSTANCE
#define CURSES_INSTANCE ((win32CursesCtx *)m_cursesCtx)
#endif
#include "curses.h"
#include "eel_edit.h"
#include "../wdlutf8.h"
#include "../win32_utf8.h"
#include "../wdlcstring.h"
#include "../eel2/ns-eel-int.h"


EEL_Editor::EEL_Editor(void *cursesCtx) : WDL_CursesEditor(cursesCtx)
{
  m_added_funclist=NULL;
  m_suggestion_x=m_suggestion_y=-1;
  m_case_sensitive=false;
  m_comment_str="//"; // todo IsWithinComment() or something?
  m_function_prefix = "function ";
}

EEL_Editor::~EEL_Editor()
{
}

#define sh_func_ontoken(x,y)

int EEL_Editor::namedTokenHighlight(const char *tokStart, int len, int state)
{
  if (len == 4 && !strnicmp(tokStart,"this",4)) return SYNTAX_KEYWORD;
  if (len == 7 && !strnicmp(tokStart,"_global",7)) return SYNTAX_KEYWORD;
  if (len == 5 && !strnicmp(tokStart,"local",5)) return SYNTAX_KEYWORD;
  if (len == 8 && !strnicmp(tokStart,"function",8)) return SYNTAX_KEYWORD;
  if (len == 6 && !strnicmp(tokStart,"static",6)) return SYNTAX_KEYWORD;
  if (len == 8 && !strnicmp(tokStart,"instance",8)) return SYNTAX_KEYWORD;
  if (len == 6 && !strnicmp(tokStart,"global",6)) return SYNTAX_KEYWORD;
  if (len == 7 && !strnicmp(tokStart,"globals",7)) return SYNTAX_KEYWORD;

  if (len == 5 && !strnicmp(tokStart,"while",5)) return SYNTAX_KEYWORD;
  if (len == 4 && !strnicmp(tokStart,"loop",4)) return SYNTAX_KEYWORD;

  if (len == 17 && !strnicmp(tokStart,"__denormal_likely",17)) return SYNTAX_FUNC;
  if (len == 19 && !strnicmp(tokStart,"__denormal_unlikely",19)) return SYNTAX_FUNC;

  char buf[512];
  lstrcpyn_safe(buf,tokStart,wdl_min(sizeof(buf),len+1));
  if (m_added_funclist)
  {
    char **r=m_added_funclist->GetPtr(buf);
    if (r) return *r ? SYNTAX_FUNC : SYNTAX_REGVAR;
  }

  NSEEL_VMCTX vm = peek_want_VM_funcs() ? peek_get_VM() : NULL;
  if (nseel_getFunctionByName((compileContext*)vm,buf,NULL)) return SYNTAX_FUNC;

  return A_NORMAL;
}

int EEL_Editor::parse_format_specifier(const char *fmt_in, int *var_offs, int *var_len)
{
  const char *fmt = fmt_in+1;
  *var_offs = 0;
  *var_len = 0;
  if (fmt_in[0] != '%') return 0; // passed a non-specifier

  while (*fmt)
  {
    const char c = *fmt++;

    if (c>0 && isalpha(c)) 
    {
      return (int) (fmt - fmt_in);
    }

    if (c == '.' || c == '+' || c == '-' || c == ' ' || (c>='0' && c<='9')) 
    {
    }
    else if (c == '{')
    {
      if (*var_offs!=0) return 0; // already specified
      *var_offs = (int)(fmt-fmt_in);
      if (*fmt == '.' || (*fmt >= '0' && *fmt <= '9')) return 0; // symbol name can't start with 0-9 or .

      while (*fmt != '}')
      {
        if ((*fmt >= 'a' && *fmt <= 'z') ||
            (*fmt >= 'A' && *fmt <= 'Z') ||
            (*fmt >= '0' && *fmt <= '9') ||
            *fmt == '_' || *fmt == '.' || *fmt == '#')
        {
          fmt++;
        }
        else
        {
          return 0; // bad character in variable name
        }
      }
      *var_len = (int)((fmt-fmt_in) - *var_offs);
      fmt++;
    }
    else
    {
      break;
    }
  }
  return 0;
}


void EEL_Editor::draw_string(int *skipcnt, const char *str, int amt, int *attr, int newAttr, int comment_string_state)
{
  if (amt > 0 && comment_string_state=='"')
  {
    while (amt > 0 && *str)
    {
      const char *str_scan = str;
      int varpos,varlen,l=0;

      while (!l && *str_scan)
      {
        while (*str_scan && *str_scan != '%' && str_scan < str+amt) str_scan++;
        if (str_scan >= str+amt) break;

        l = parse_format_specifier(str_scan,&varpos,&varlen);
        if (!l && *str_scan)  if (*++str_scan == '%') str_scan++;
      }
      if (!*str_scan || str_scan >= str+amt) break; // allow default processing to happen if we reached the end of the string

      if (l > amt) l=amt;

      if (str_scan > str) 
      {
        const int sz=wdl_min((int)(str_scan-str),amt);
        draw_string_urlchk(skipcnt,str,sz,attr,newAttr);
        str += sz;
        amt -= sz;
      }

      {
        const int sz=(varlen>0) ? wdl_min(varpos,amt) : wdl_min(l,amt);
        if (sz>0) 
        {
          draw_string_internal(skipcnt,str,sz,attr,SYNTAX_HIGHLIGHT2);
          str += sz;
          amt -= sz;
        }
      }

      if (varlen>0) 
      {
        int sz = wdl_min(varlen,amt);
        if (sz>0)
        {
          draw_string_internal(skipcnt,str,sz,attr,*str == '#' ? SYNTAX_STRINGVAR : SYNTAX_HIGHLIGHT1);
          amt -= sz;
          str += sz;
        }

        sz = wdl_min(l - varpos - varlen, amt);
        if (sz>0)
        {
          draw_string_internal(skipcnt,str,sz,attr,SYNTAX_HIGHLIGHT2);
          amt-=sz;
          str+=sz;
        }
      }
    }
  }
  draw_string_urlchk(skipcnt,str,amt,attr,newAttr);
}

void EEL_Editor::draw_string_urlchk(int *skipcnt, const char *str, int amt, int *attr, int newAttr)
{
  if (amt > 0 && (newAttr == SYNTAX_COMMENT || newAttr == SYNTAX_STRING))
  {
    const char *sstr=str;
    while (amt > 0 && *str)
    {
      const char *str_scan = str;
      int l=0;
      
      while (l < 10 && *str_scan)
      {
        str_scan += l;
        l=0;
        while (*str_scan &&
               (strncmp(str_scan,"http://",7) || (sstr != str_scan && str_scan[-1] > 0 && isalnum(str_scan[-1]))) &&
               str_scan < str+amt) str_scan++;
        if (!*str_scan || str_scan >= str+amt) break;
        while (str_scan[l] && str_scan[l] != ')' && str_scan[l] != '\"' && str_scan[l] != ')' && str_scan[l] != ' ' && str_scan[l] != '\t') l++;
      }
      if (!*str_scan || str_scan >= str+amt) break; // allow default processing to happen if we reached the end of the string
      
      if (l > amt) l=amt;
      
      if (str_scan > str)
      {
        const int sz=wdl_min((int)(str_scan-str),amt);
        draw_string_internal(skipcnt,str,sz,attr,newAttr);
        str += sz;
        amt -= sz;
      }
      
      const int sz=wdl_min(l,amt);
      if (sz>0)
      {
        draw_string_internal(skipcnt,str,sz,attr,SYNTAX_HIGHLIGHT1);
        str += sz;
        amt -= sz;
      }
    }
  }
  draw_string_internal(skipcnt,str,amt,attr,newAttr);
}
  
void EEL_Editor::draw_string_internal(int *skipcnt, const char *str, int amt, int *attr, int newAttr)
{
  // *skipcnt is in characters, amt is in bytes
  while (*skipcnt > 0 && amt > 0)
  {
    const int clen = wdl_utf8_parsechar(str,NULL);
    str += clen;
    amt -= clen;
    *skipcnt -= 1;
  }

  if (amt>0)
  {
    if (*attr != newAttr) 
    {
      attrset(newAttr);
      *attr = newAttr;
    }
    addnstr(str,amt);
  }
}


WDL_TypedBuf<char> EEL_Editor::s_draw_parentokenstack;
bool EEL_Editor::sh_draw_parenttokenstack_pop(char c)
{
  int sz = s_draw_parentokenstack.GetSize();
  while (--sz >= 0)
  {
    char tc = s_draw_parentokenstack.Get()[sz];
    if (tc == c)
    {
      s_draw_parentokenstack.Resize(sz,false);
      return false;
    }

    switch (c)
    {
      case '?': 
        // any open paren or semicolon is enough to cause error for ?:

      return true;
      case '(':
        if (tc == '[') return true;
      break;
      case '[':
        if (tc == '(') return true;
      break;
    }
  }

  return true;
}
bool EEL_Editor::sh_draw_parentokenstack_update(const char *tok, int toklen)
{
  if (toklen == 1)
  {
    switch (*tok)
    {
      case '(':
      case '[':
      case ';':
      case '?':
        s_draw_parentokenstack.Add(*tok);
      break;
      case ':': return sh_draw_parenttokenstack_pop('?');
      case ')': return sh_draw_parenttokenstack_pop('(');
      case ']': return sh_draw_parenttokenstack_pop('[');
    }
  }
  return false;
}


void EEL_Editor::draw_line_highlight(int y, const char *p, int *c_comment_state)
{
  int last_attr = A_NORMAL;
  attrset(last_attr);
  move(y, 0);
  int rv = do_draw_line(p, c_comment_state, last_attr);
  attrset(rv< 0 ? SYNTAX_ERROR : A_NORMAL);
  clrtoeol();
  if (rv < 0) attrset(A_NORMAL);
}

int EEL_Editor::do_draw_line(const char *p, int *c_comment_state, int last_attr)
{
  //skipcnt = m_offs_x
  if (is_code_start_line(p)) 
  {
    *c_comment_state=0;
    s_draw_parentokenstack.Resize(0,false);
  }

  int skipcnt = m_offs_x;
  int ignoreSyntaxState = overrideSyntaxDrawingForLine(&skipcnt, &p, c_comment_state, &last_attr);

  if (ignoreSyntaxState>0)
  {
    int len = (int)strlen(p);
    draw_string(&skipcnt,p,len,&last_attr,ignoreSyntaxState==100 ? SYNTAX_ERROR : 
        ignoreSyntaxState==2 ? SYNTAX_COMMENT : A_NORMAL);
    return len-m_offs_x < COLS;
  }


  // syntax highlighting
  const char *endptr = p+strlen(p);
  const char *tok;
  const char *lp = p;
  int toklen=0;
  int last_comment_state=*c_comment_state;
  while (NULL != (tok = sh_tokenize(&p,endptr,&toklen,c_comment_state)) || lp < endptr)
  {
    if (tok && *tok < 0 && toklen == 1)
    {
      while (tok[toklen] < 0) {p++; toklen++; } // utf-8 skip
    }
    if (last_comment_state>0) // if in a multi-line string or comment
    {
      // draw empty space between lp and p as a string. in this case, tok/toklen includes our string, so we quickly finish after
      draw_string(&skipcnt,lp,(int)(p-lp),&last_attr, last_comment_state==1 ? SYNTAX_COMMENT:SYNTAX_STRING, last_comment_state);
      last_comment_state=0;
      lp = p;
      continue;
    }
    sh_func_ontoken(tok,toklen);

    // draw empty space between lp and tok/endptr as normal
    const char *adv_to = tok ? tok : endptr;
    if (adv_to > lp) draw_string(&skipcnt,lp,(int)(adv_to-lp),&last_attr, A_NORMAL);

    if (adv_to >= endptr) break;

    last_comment_state=0;
    lp = p;
    if (adv_to == p) continue;

    // draw token
    int attr = A_NORMAL;
    int err_left=0;
    int err_right=0;
    int start_of_tok = 0;

    if (tok[0] == '/' && toklen > 1 && (tok[1] == '*' || tok[1] == '/'))
    {
      attr = SYNTAX_COMMENT;
    }
    else if (tok[0] > 0 && (isalpha(tok[0]) || tok[0] == '_' || tok[0] == '#'))
    {
      int def_attr = A_NORMAL;
      bool isf=true;
      if (tok[0] == '#')
      {
        def_attr = SYNTAX_STRINGVAR;
        draw_string(&skipcnt,tok,1,&last_attr,def_attr);
        tok++;
        toklen--;
      }
      while (toklen > 0)
      {
        // divide up by .s, if any
        int this_len=0;
        while (this_len < toklen && tok[this_len] != '.') this_len++;
        if (this_len > 0)
        {
          int attr=isf?namedTokenHighlight(tok,this_len,*c_comment_state):def_attr;
          if (isf && attr == A_NORMAL)
          {
            int ntok_len=0, cc = *c_comment_state;
            const char *pp=lp,*ntok = sh_tokenize(&pp,endptr,&ntok_len,&cc);
            if (ntok && ntok_len>0 && *ntok == '(') def_attr = attr = SYNTAX_FUNC2;
          }

          draw_string(&skipcnt,tok,this_len,&last_attr,attr==A_NORMAL?def_attr:attr);
          tok += this_len;
          toklen -= this_len;
        }
        if (toklen > 0)
        {
          draw_string(&skipcnt,tok,1,&last_attr,SYNTAX_HIGHLIGHT1);
          tok++;
          toklen--;
        }
        isf=false;
      }
      continue;
    }
    else if (tok[0] == '.' ||
             (tok[0] >= '0' && tok[0] <= '9') ||
             (toklen > 1 && tok[0] == '$' && (tok[1] == 'x' || tok[1]=='X')))
    {
      attr = SYNTAX_HIGHLIGHT2;

      int x=1,mode=0;
      if (tok[0] == '.') mode=1;
      else if (toklen > 1 && (tok[0] == '$' || tok[0] == '0') && (tok[1] == 'x' || tok[1] == 'X')) { mode=2;  x++; }
      for(;x<toklen;x++)
      {
        if (tok[x] == '.'  && !mode) mode=1;
        else if (tok[x] < '0' || tok[x] > '9') 
        {
          if (mode != 2 || ((tok[x] < 'a' || tok[x] > 'f') && (tok[x] < 'A' || tok[x] > 'F')))
                break;
        }
      }
      if (x<toklen) err_right=toklen-x;
    }
    else if (tok[0] == '\'' || tok[0] == '\"')
    {
      start_of_tok = tok[0];
      attr = SYNTAX_STRING;
    }
    else if (tok[0] == '$')
    {
      attr = SYNTAX_HIGHLIGHT2;

      if (toklen >= 3 && !strnicmp(tok,"$pi",3)) err_right = toklen - 3;
      else if (toklen >= 2 && !strnicmp(tok,"$e",2)) err_right = toklen - 2;
      else if (toklen >= 4 && !strnicmp(tok,"$phi",4)) err_right = toklen - 4;
      else if (toklen == 4 && tok[1] == '\'' && tok[3] == '\'') { }
      else if (toklen > 1 && tok[1] == '~')
      {
        int x;
        for(x=2;x<toklen;x++) if (tok[x] < '0' || tok[x] > '9') break;
        if (x<toklen) err_right=toklen-x;
      }
      else err_right = toklen;
    }
    else if (ignoreSyntaxState==-1 && (tok[0] == '{' || tok[0] == '}'))
    {
      attr = SYNTAX_HIGHLIGHT1;
    }
    else
    {
      const char *h="()+*-=/,|&%;!<>?:^!~[]";
      while (*h && *h != tok[0]) h++;
      if (*h)
      {
        if (*c_comment_state != STATE_BEFORE_CODE && sh_draw_parentokenstack_update(tok,toklen))
          attr = SYNTAX_ERROR;
        else
          attr = SYNTAX_HIGHLIGHT1;
      }
      else 
      {
        err_left=1;
        if (tok[0] < 0) while (err_left < toklen && tok[err_left]<0) err_left++; // utf-8 skip
      }
    }

    if (ignoreSyntaxState) err_left = err_right = 0;

    if (err_left > 0) 
    {
      if (err_left > toklen) err_left=toklen;
      draw_string(&skipcnt,tok,err_left,&last_attr,SYNTAX_ERROR);
      tok+=err_left;
      toklen -= err_left;
    }
    if (err_right > toklen) err_right=toklen;

    draw_string(&skipcnt, tok, toklen - err_right, &last_attr, attr, start_of_tok);

    if (err_right > 0)
      draw_string(&skipcnt,tok+toklen-err_right,err_right,&last_attr,SYNTAX_ERROR);

    if (ignoreSyntaxState == -1 && tok[0] == '>')
    {
      draw_string(&skipcnt,p,strlen(p),&last_attr,ignoreSyntaxState==2 ? SYNTAX_COMMENT : A_NORMAL);
      break;
    }
  }
  return 1;
}

int EEL_Editor::GetCommentStateForLineStart(int line)
{
  if (m_write_leading_tabs<=0) m_indent_size=2;
  const bool uses_code_start_lines = !!is_code_start_line(NULL);

  int state=0;
  int x=0;

  if (uses_code_start_lines)
  {
    state=STATE_BEFORE_CODE;
    for (;;x++)
    {
      WDL_FastString *t = m_text.Get(x);
      if (!t || is_code_start_line(t->Get())) break;
    
      const char *p=t->Get();

      if (!strnicmp(p,"tabsize:",8))
      {
        int a = atoi(p+8);
        if (a>0 && a < 32) m_indent_size = a;
      }
    }


    // scan backwards to find line starting with @
    for (x=line;x>=0;x--)
    {
      WDL_FastString *t = m_text.Get(x);
      if (!t) break;
      if (is_code_start_line(t->Get()))
      {
        state=0;
        break;
      }
    }
    x++;
  }

  s_draw_parentokenstack.Resize(0,false);

  for (;x<line;x++)
  {
    WDL_FastString *t = m_text.Get(x);
    const char *p = t?t->Get():"";
    if (is_code_start_line(p)) 
    {
      s_draw_parentokenstack.Resize(0,false);
      state=0; 
    }
    else if (state != STATE_BEFORE_CODE)
    {
      const int ll=t?t->GetLength():0;
      const char *endp = p+ll;
      int toklen;
      const char *tok;
      while (NULL != (tok=sh_tokenize(&p,endp,&toklen,&state))) // eat all tokens, updating state
      {
        sh_func_ontoken(tok,toklen);
        sh_draw_parentokenstack_update(tok,toklen);
      }
    }
  }
  return state;
}

const char *EEL_Editor::sh_tokenize(const char **ptr, const char *endptr, int *lenOut, int *state)
{
  return nseel_simple_tokenizer(ptr, endptr, lenOut, state);
}


bool EEL_Editor::LineCanAffectOtherLines(const char *txt, int spos, int slen) // if multiline comment etc
{
  const char *special_start = txt + spos;
  const char *special_end = txt + spos + slen;
  while (*txt)
  {
    if (txt >= special_start-1 && txt < special_end)
    {
      const char c = txt[0];
      if (c == '*' && txt[1] == '/') return true;
      if (c == '/' && (txt[1] == '/' || txt[1] == '*')) return true;
      if (c == '\\' && (txt[1] == '\"' || txt[1] == '\'')) return true;

      if (txt >= special_start)
      {
        if (c == '\"' || c == '\'') return true;
        if (c == '(' || c == '[' || c == ')' || c == ']' || c == ':' || c == ';' || c == '?') return true;
      }
    }
    txt++;
  }
  return false;
}


struct eel_sh_token
{
  int line, col, end_col; 
  unsigned int data; // packed char for token type, plus 24 bits for linecnt (0=single line, 1=extra line, etc)

  eel_sh_token(int _line, int _col, int toklen, unsigned char c)
  {
    line = _line;
    col = _col;
    end_col = col + toklen;
    data = c;
  }
  ~eel_sh_token() { }

  void add_linecnt(int endcol) { data += 256; end_col = endcol; }
  int get_linecnt() const { return (data >> 8); }
  char get_c() const { return (char) (data & 255); }

  bool is_comment() const {
    return get_c() == '/' && (get_linecnt() || end_col>col+1);
  };
};

static int eel_sh_get_token_for_pos(const WDL_TypedBuf<eel_sh_token> *toklist, int line, int col, bool *is_after)
{
  const int sz = toklist->GetSize();
  int x;
  for (x=0; x < sz; x ++)
  {
    const eel_sh_token *tok = toklist->Get()+x;
    const int first_line = tok->line;
    const int last_line = first_line+tok->get_linecnt(); // last affected line (usually same as first)

    if (last_line >= line) // if this token doesn't end before the line we care about
    {
      // check to see if the token starts after our position
      if (first_line > line || (first_line == line && tok->col > col)) break; 

      // token started before line/col, see if it ends after line/col
      if (last_line > line || tok->end_col > col) 
      {
        // direct hit
        *is_after = false;
        return x;
      } 
    }
  }
  *is_after = true;
  return x-1;
}

static void eel_sh_generate_token_list(const WDL_PtrList<WDL_FastString> *lines, WDL_TypedBuf<eel_sh_token> *toklist, int start_line, EEL_Editor *editor)
{
  toklist->Resize(0,false);
  int state=0;
  int l;
  int end_line = lines->GetSize();
  if (editor->is_code_start_line(NULL))
  {
    for (l = start_line; l < end_line; l ++)
    {
      WDL_FastString *s = lines->Get(l);
      if (s && editor->is_code_start_line(s->Get()))
      {
        end_line = l;
        break;
      }
    }
    for (; start_line >= 0; start_line--)
    {
      WDL_FastString *s = lines->Get(start_line);
      if (s && editor->is_code_start_line(s->Get())) break;
    }
    if (start_line < 0) return; // before any code
  
    start_line++;
  }
  else
  {
    start_line = 0;
  }
  

  for (l=start_line;l<end_line;l++)
  {
    WDL_FastString *t = lines->Get(l);
    const int ll = t?t->GetLength():0;
    const char *start_p = t?t->Get():"";
    const char *p = start_p;
    const char *endp = start_p+ll;
   
    const char *tok;
    int last_state=state;
    int toklen;
    while (NULL != (tok=editor->sh_tokenize(&p,endp,&toklen,&state))||last_state)
    {
      if (last_state == '\'' || last_state == '"' || last_state==1)
      {
        const int sz=toklist->GetSize();
        // update last token to include this data
        if (sz) toklist->Get()[sz-1].add_linecnt((int) ((tok ? p:endp) - start_p));
      }
      else
      {
        if (tok) switch (tok[0])
        {
          case '{':
          case '}':
          case '?':
          case ':':
          case ';':
          case '(':
          case '[':
          case ')':
          case ']':
          case '\'':
          case '"':
          case '/': // comment
            {
              eel_sh_token t(l,(int)(tok-start_p),toklen,tok[0]);
              toklist->Add(t);
            }
          break;
        }
      }
      last_state=0;
    }
  }
}

static bool eel_sh_get_matching_pos_for_pos(WDL_PtrList<WDL_FastString> *text, int curx, int cury, int *newx, int *newy, const char **errmsg, EEL_Editor *editor)
{
  static WDL_TypedBuf<eel_sh_token> toklist;
  eel_sh_generate_token_list(text,&toklist, cury,editor);
  bool is_after;
  const int hit_tokidx = eel_sh_get_token_for_pos(&toklist, cury, curx, &is_after);
  const eel_sh_token *hit_tok = hit_tokidx >= 0 ? toklist.Get() + hit_tokidx : NULL;

  if (!is_after && hit_tok && (hit_tok->get_c() == '"' || hit_tok->get_c() == '\'' || hit_tok->is_comment()))
  {
    eel_sh_token tok = *hit_tok; // save a copy, toklist might get destroyed recursively here
    hit_tok = &tok;

    //if (tok.get_c() == '"')
    {
      // the user could be editing code in code, tokenize it and see if we can make sense of it
      WDL_FastString start, end;
      WDL_PtrList<WDL_FastString> tmplist;
      WDL_FastString *s = text->Get(tok.line);
      if (s && s->GetLength() > tok.col+1)
      {
        int maxl = tok.get_linecnt()>0 ? 0 : tok.end_col - tok.col - 2;
        start.Set(s->Get() + tok.col+1, maxl);
      }
      tmplist.Add(&start);
      const int linecnt = tok.get_linecnt();
      if (linecnt>0)
      {
        for (int a=1; a < linecnt; a ++)
        {
          s = text->Get(tok.line + a);
          if (s) tmplist.Add(s);
        }
        s = text->Get(tok.line + linecnt);
        if (s)
        {
          if (tok.end_col>1) end.Set(s->Get(), tok.end_col-1);
          tmplist.Add(&end);
        }
      }

      int lx = curx, ly = cury - tok.line;
      if (cury == tok.line) lx -= (tok.col+1);

      // this will destroy the token 
      if (eel_sh_get_matching_pos_for_pos(&tmplist, lx, ly, newx, newy, errmsg, editor))
      {
        *newy += tok.line;
        if (cury == tok.line) *newx += tok.col + 1;
        return true;
      }
    }

    // if within a string or comment, move to start, unless already at start, move to end
    if (cury == hit_tok->line && curx == hit_tok->col)
    {
      *newx=hit_tok->end_col-1;
      *newy=hit_tok->line + hit_tok->get_linecnt();
    }
    else
    {
      *newx=hit_tok->col;
      *newy=hit_tok->line;
    }
    return true;
  }

  if (!hit_tok) return false;

  const int toksz=toklist.GetSize();
  int tokpos = hit_tokidx;
  int pc1=0,pc2=0; // (, [ count
  int pc3=0; // : or ? count depending on mode
  int dir=-1, mode=0;  // default to scan to previous [(
  if (!is_after) 
  {
    switch (hit_tok->get_c())
    {
      case '(': mode=1; dir=1; break;
      case '[': mode=2; dir=1; break;
      case ')': mode=3; dir=-1; break;
      case ']': mode=4; dir=-1; break;
      case '?': mode=5; dir=1; break;
      case ':': mode=6; break;
      case ';': mode=7; break;
    }
    // if hit a token, exclude this token from scanning
    tokpos += dir;
  }

  while (tokpos>=0 && tokpos<toksz)
  {
    const eel_sh_token *tok = toklist.Get() + tokpos;
    const char this_c = tok->get_c();
    if (!pc1 && !pc2)
    {
      bool match=false, want_abort=false;
      switch (mode)
      {
        case 0: match = this_c == '(' || this_c == '['; break;
        case 1: match = this_c == ')'; break;
        case 2: match = this_c == ']'; break;
        case 3: match = this_c == '('; break;
        case 4: match = this_c == '['; break;
        case 5: 
          // scan forward to nearest : or ;
          if (this_c == '?') pc3++;
          else if (this_c == ':')
          {
            if (pc3>0) pc3--;
            else match=true;
          }
          else if (this_c == ';') match=true;
          else if (this_c == ')' || this_c == ']') 
          {
            want_abort=true; // if you have "(x<y?z)", don't match for the ?
          }
        break;
        case 6:  // scanning back from : to ?, if any
        case 7:  // semicolon searches same as colon, effectively
          if (this_c == ':') pc3++;
          else if (this_c == '?')
          {
            if (pc3>0) pc3--;
            else match = true;
          }
          else if (this_c == ';' || this_c == '(' || this_c == '[') 
          {
            want_abort=true;
          }
        break;
      }

      if (want_abort) break;
      if (match)
      {
        *newx=tok->col;
        *newy=tok->line;
        return true;
      }
    }
    switch (this_c)
    {
      case '[': pc2++; break;
      case ']': pc2--; break;
      case '(': pc1++; break;
      case ')': pc1--; break;
    }
    tokpos+=dir;
  }    

  if (errmsg)
  {
    if (!mode) *errmsg = "Could not find previous [ or (";
    else if (mode == 1) *errmsg = "Could not find matching )";
    else if (mode == 2) *errmsg = "Could not find matching ]";
    else if (mode == 3) *errmsg = "Could not find matching (";
    else if (mode == 4) *errmsg = "Could not find matching [";
    else if (mode == 5) *errmsg = "Could not find matching : or ; for ?";
    else if (mode == 6) *errmsg = "Could not find matching ? for :";
    else if (mode == 7) *errmsg = "Could not find matching ? for ;";
  }
  return false;
}


void EEL_Editor::doParenMatching()
{
  WDL_FastString *curstr;
  const char *errmsg = "";
  if (NULL != (curstr=m_text.Get(m_curs_y)))
  {
    int bytex = WDL_utf8_charpos_to_bytepos(curstr->Get(),m_curs_x);
    if (bytex >= curstr->GetLength()) bytex=curstr->GetLength()-1;
    if (bytex<0) bytex = 0;

    int new_x,new_y;
    if (eel_sh_get_matching_pos_for_pos(&m_text, bytex,m_curs_y,&new_x,&new_y,&errmsg,this))
    {
      curstr = m_text.Get(new_y);
      if (curstr) new_x = WDL_utf8_bytepos_to_charpos(curstr->Get(),new_x);

      m_curs_x=new_x;
      m_curs_y=new_y;
      m_want_x=-1;
      draw();
      setCursor(1);
    }
    else if (errmsg[0])
    {
      draw_message(errmsg);
      setCursor(0);
    }
  }
}

int EEL_Editor::peek_get_function_info(const char *name, char *sstr, size_t sstr_sz, int chkmask, int ignoreline)
{
  if ((chkmask&4) && m_function_prefix && *m_function_prefix)
  {
    const size_t nlen = strlen(name);
    const char *prefix = m_function_prefix;
    const int prefix_len = (int) strlen(m_function_prefix);
    for (int i=0; i < m_text.GetSize(); ++i)
    {
      WDL_FastString* s=m_text.Get(i);
      if (s && i != ignoreline)
      {
        const char* p= s->Get();
        while (*p)
        {
          if (m_case_sensitive ? !strncmp(p,prefix,prefix_len) : !strnicmp(p,prefix,prefix_len))
          {
            p+=prefix_len;
            while (*p == ' ') p++;
            if (m_case_sensitive ? !strncmp(p,name,nlen) : !strnicmp(p,name,nlen))
            {
              const char *np = p+nlen;
              while (*np == ' ') np++;

              if (*np == '(')
              {
                lstrcpyn_safe(sstr,p,sstr_sz);
                return 4;
              }
            }
          }
          p++;
        }
      }
    }
  }

  if ((chkmask&2) && m_added_funclist)
  {
    char **p=m_added_funclist->GetPtr(name);
    if (p && *p)
    {
      lstrcpyn_safe(sstr,*p,sstr_sz);
      return 2;
    }
  }

  if (chkmask & 1)
  {
    peek_lock();
    NSEEL_VMCTX vm = peek_want_VM_funcs() ? peek_get_VM() : NULL;
    functionType *f = nseel_getFunctionByName((compileContext*)vm,name,NULL);
    if (f)
    {
      snprintf(sstr,sstr_sz,"'%s' is a function that requires %d parameters", f->name,f->nParams&0xff);
      peek_unlock();
      return 1;
    }
    peek_unlock();
  }

  return 0;
}
bool EEL_Editor::peek_get_variable_info(const char *name, char *sstr, size_t sstr_sz)
{
  peek_lock();
  NSEEL_VMCTX vm = peek_get_VM();
  EEL_F *vptr=NSEEL_VM_getvar(vm,name);
  double v=0.0;
  if (vptr) v=*vptr;
  peek_unlock();

  if (!vptr)  return false;

  int good_len=-1;
  snprintf(sstr,sstr_sz,"%s=%.14f",name,v);

  if (vm && v > -1.0 && v < NSEEL_RAM_ITEMSPERBLOCK*NSEEL_RAM_BLOCKS)
  {
    const unsigned int w = (unsigned int) (v+NSEEL_CLOSEFACTOR);
    EEL_F *dv = NSEEL_VM_getramptr_noalloc(vm,w,NULL);
    if (dv)
    {
      snprintf_append(sstr,sstr_sz," [0x%06x]=%.14f",w,*dv);
      good_len=-2;
    }
    else
    {
      good_len = strlen(sstr);
      snprintf_append(sstr,sstr_sz," [0x%06x]=<uninit>",w);
    }
  }

  char buf[512];
  buf[0]=0;
  if (peek_get_numbered_string_value(v,buf,sizeof(buf)))
  {
    if (good_len==-2)
      snprintf_append(sstr,sstr_sz," %.0f(str)=%s",v,buf);
    else
    {
      if (good_len>=0) sstr[good_len]=0; // remove [addr]=<uninit> if a string and no ram
      snprintf_append(sstr,sstr_sz," (str)=%s",buf);
    }
  }
  return true;
}

void EEL_Editor::doWatchInfo(int c)
{
    // determine the word we are on, check its value in the effect
  char sstr[512], buf[512];
  lstrcpyn_safe(sstr,"Use this on a valid symbol name", sizeof(sstr));
  WDL_FastString *t=m_text.Get(m_curs_y);
  char curChar=0;
  if (t)
  {
    const char *p=t->Get();
    const int bytex = WDL_utf8_charpos_to_bytepos(p,m_curs_x);
    if (bytex >= 0 && bytex < t->GetLength()) curChar = p[bytex];
    if (c != KEY_F1 && (m_selecting || 
             curChar == '(' || 
             curChar == '[' ||
             curChar == ')' ||
             curChar == ']'
             ))
    {
      WDL_FastString code;
      int miny,maxy,minx,maxx;
      bool ok = false;
      if (!m_selecting)
      {
        if (eel_sh_get_matching_pos_for_pos(&m_text,minx=m_curs_x,miny=m_curs_y,&maxx, &maxy,NULL,this))
        {
          if (maxy==miny)
          {
            if (maxx < minx)
            {
              int tmp = minx;
              minx=maxx;
              maxx=tmp;
            }
          }
          else if (maxy < miny)
          {
            int tmp=maxy;
            maxy=miny;
            miny=tmp;
            tmp = minx;
            minx=maxx;
            maxx=tmp;
          }
          ok = true;
          minx++; // skip leading (
        }
      }
      else
      {
        ok=true; 
        getselectregion(minx,miny,maxx,maxy); 
        WDL_FastString *s;
        s = m_text.Get(miny);
        if (s) minx = WDL_utf8_charpos_to_bytepos(s->Get(),minx);
        s = m_text.Get(maxy);
        if (s) maxx = WDL_utf8_charpos_to_bytepos(s->Get(),maxx);
      }

      if (ok)
      {
        int x;
        for (x = miny; x <= maxy; x ++)
        {
          WDL_FastString *s=m_text.Get(x);
          if (s) 
          {
            const char *str=s->Get();
            int sx,ex;
            if (x == miny) sx=wdl_max(minx,0);
            else sx=0;
            int tmp=s->GetLength();
            if (sx > tmp) sx=tmp;
      
            if (x == maxy) ex=wdl_min(maxx,tmp);
            else ex=tmp;
      
            if (code.GetLength()) code.Append("\r\n");
            code.Append(ex-sx?str+sx:"",ex-sx);
          }
        }
      }
      if (code.Get()[0])
      {
        if (m_selecting && (GetAsyncKeyState(VK_SHIFT)&0x8000))
        {
          peek_lock();
          NSEEL_CODEHANDLE ch;
          NSEEL_VMCTX vm = peek_get_VM();

          if (vm && (ch = NSEEL_code_compile_ex(vm,code.Get(),1,0)))
          {
            codeHandleType *p = (codeHandleType*)ch;
            code.Ellipsize(3,20);
            const char *errstr = "failed writing to";
            if (p->code)
            {
              buf[0]=0;
              GetTempPath(sizeof(buf)-64,buf);
              lstrcatn(buf,"jsfx-out",sizeof(buf));
              FILE *fp = fopen(buf,"wb");
              if (fp)
              {
                errstr="wrote to";
                fwrite(p->code,1,p->code_size,fp);
                fclose(fp);
              }
            }
            snprintf(sstr,sizeof(sstr),"Expression '%s' compiled to %d bytes, %s temp/jsfx-out",code.Get(),p->code_size, errstr);
            NSEEL_code_free(ch);
          }
          else
          {
            code.Ellipsize(3,20);
            snprintf(sstr,sizeof(sstr),"Expression '%s' could not compile",code.Get());
          }
          peek_unlock();
        }
        else
        {
          WDL_FastString code2;
          code2.Set("__debug_watch_value = (((((");
          code2.Append(code.Get());
          code2.Append(")))));");
      
          peek_lock();

          NSEEL_VMCTX vm = peek_get_VM();

          EEL_F *vptr=NULL;
          double v=0.0;
          const char *err="Invalid context";
          if (vm)
          {
            NSEEL_CODEHANDLE ch = NSEEL_code_compile_ex(vm,code2.Get(),1,0);
            if (!ch) err = "Error parsing";
            else
            {
              NSEEL_code_execute(ch);
              NSEEL_code_free(ch);
              vptr = NSEEL_VM_getvar(vm,"__debug_watch_value");
              if (vptr) v = *vptr;
            }
          }

          peek_unlock();

          {
            // remove whitespace from code for display
            int x;
            bool lb=true;
            for (x=0;x<code.GetLength();x++)
            {
              if (isspace(code.Get()[x]))
              {
                if (lb) code.DeleteSub(x--,1);
                lb=true;
              }
              else
              {
                lb=false;
              }
            }
            if (lb && code.GetLength()>0) code.SetLen(code.GetLength()-1);
          }

          code.Ellipsize(3,20);
          if (vptr)
          {
            snprintf(sstr,sizeof(sstr),"Expression '%s' evaluates to %.14f",code.Get(),v);
          }
          else
          {
            snprintf(sstr,sizeof(sstr),"Error evaluating '%s': %s",code.Get(),err?err:"Unknown error");
          }
        }
      }
      // compile+execute code within () as debug_watch_value = ( code )
      // show value (or err msg)
    }
    else if (curChar>0 && (isalnum(curChar) || curChar == '_' || curChar == '.' || curChar == '#')) 
    {
      const int bytex = WDL_utf8_charpos_to_bytepos(p,m_curs_x);
      const char *lp=p+bytex;
      const char *rp=lp + WDL_utf8_charpos_to_bytepos(lp,1);
      while (lp >= p && *lp > 0 && (isalnum(*lp) || *lp == '_' || (*lp == '.' && (lp==p || lp[-1]!='.')))) lp--;
      if (lp < p || *lp != '#') lp++;
      while (*rp && *rp > 0 && (isalnum(*rp) || *rp == '_' || (*rp == '.' && rp[1] != '.'))) rp++;

      if (*lp == '#' && rp > lp+1)
      {
        WDL_FastString n;
        lp++;
        n.Set(lp,(int)(rp-lp));
        int idx;
        if ((idx=peek_get_named_string_value(n.Get(),buf,sizeof(buf)))>=0) snprintf(sstr,sizeof(sstr),"#%s(%d)=%s",n.Get(),idx,buf);
        else snprintf(sstr,sizeof(sstr),"#%s not found",n.Get());
      }
      else if (*lp > 0 && (isalpha(*lp) || *lp == '_') && rp > lp)
      {
        WDL_FastString n;
        n.Set(lp,(int)(rp-lp));

        if (c==KEY_F1)
        {
          on_help(n.Get(),0);
          return;
        }

        int f = peek_get_function_info(n.Get(),sstr,sizeof(sstr),~0,-1);

        if (!f) f = peek_get_variable_info(n.Get(),sstr,sizeof(sstr))?1:0;
        if (!f) snprintf(sstr,sizeof(sstr),"'%s' NOT FOUND",n.Get());
      }
    }
  }
  if (c==KEY_F1)
  {
    on_help(NULL,(int)curChar);
    return;
  }

  setCursor();
  draw_message(sstr);
}


void EEL_Editor::draw_bottom_line()
{
#define BOLD(x) { attrset(COLOR_BOTTOMLINE|A_BOLD); addstr(x); attrset(COLOR_BOTTOMLINE&~A_BOLD); }
  addstr("ma"); BOLD("T"); addstr("ch");
  BOLD(" S"); addstr("ave");
  if (peek_get_VM())
  {
    addstr(" pee"); BOLD("K");
  }
  if (GetTabCount()>1)
  {
    addstr(" | tab: ");
    BOLD("[], F?"); addstr("=switch ");
    BOLD("W"); addstr("=close");
  }
#undef BOLD
}

#define CTRL_KEY_DOWN (GetAsyncKeyState(VK_CONTROL)&0x8000)
#define SHIFT_KEY_DOWN (GetAsyncKeyState(VK_SHIFT)&0x8000)
#define ALT_KEY_DOWN (GetAsyncKeyState(VK_MENU)&0x8000)

int EEL_Editor::onChar(int c)
{
  if ((m_ui_state == UI_STATE_NORMAL || m_ui_state == UI_STATE_MESSAGE) && 
      (c == 'K'-'A'+1 || c == 'S'-'A'+1 || !SHIFT_KEY_DOWN) && !ALT_KEY_DOWN) switch (c)
  {
  case KEY_F1:
    if (CTRL_KEY_DOWN) break;
  case 'K'-'A'+1:
    doWatchInfo(c);
  return 0;
  case 'S'-'A'+1:
   {
     WDL_DestroyCheck chk(&destroy_check);
     if(updateFile())
     {
       if (chk.isOK())
         draw_message("Error writing file, changes not saved!");
     }
     if (chk.isOK())
       setCursor();
   }
  return 0;

  case 'R'-'A'+1:
    if (!m_selecting)
    {
      WDL_FastString *txtstr=m_text.Get(m_curs_y);
      const char *txt=txtstr?txtstr->Get():NULL;
      char fnp[2048];
      if (txt && line_has_openable_file(txt,WDL_utf8_charpos_to_bytepos(txt,m_curs_x),fnp,sizeof(fnp)))
      {
        WDL_CursesEditor::OpenFileInTab(fnp);
      }
    }
  return 0;
  case KEY_F4:
  case 'T'-'A'+1:
    doParenMatching();
  return 0;
  }

  return WDL_CursesEditor::onChar(c);
}

void EEL_Editor::draw_top_line()
{
  if (m_curs_x >= m_suggestion_x && m_curs_y == m_suggestion_y && m_suggestion.GetLength())
  {
    const char *p=m_suggestion.Get();
    char str[512];
    if (WDL_utf8_get_charlen(m_suggestion.Get()) > COLS)
    {
      int l = WDL_utf8_charpos_to_bytepos(m_suggestion.Get(),COLS-4);
      if (l > sizeof(str)-6) l = sizeof(str)-6;
      lstrcpyn(str, m_suggestion.Get(), l+1);
      strcat(str, "...");
      p=str;
    }

    attrset(COLOR_TOPLINE|A_BOLD);
    bkgdset(COLOR_TOPLINE);
    move(0, 0);
    addstr(p);
    clrtoeol();
    attrset(0);
    bkgdset(0);
  }
  else
  {
    m_suggestion_x=m_suggestion_y=-1;
    if (m_suggestion.GetLength()) m_suggestion.Set("");
    WDL_CursesEditor::draw_top_line();
  }
}


void EEL_Editor::onRightClick(HWND hwnd)
{
  WDL_LogicalSortStringKeyedArray<int> flist(m_case_sensitive);
  int i;
  if (!(GetAsyncKeyState(VK_CONTROL)&0x8000) && m_function_prefix && *m_function_prefix)
  {
    const char *prefix = m_function_prefix;
    const int prefix_len = (int) strlen(m_function_prefix);
    const int comment_len=(int)strlen(m_comment_str);
    for (i=0; i < m_text.GetSize(); ++i)
    {
      WDL_FastString* s=m_text.Get(i);
      const char* p=s ? s->Get() : NULL;
      if (p) while (*p)
      {
        if (!strncmp(p, m_comment_str, comment_len)) break;

        if (m_case_sensitive ? !strncmp(p,prefix,prefix_len) : !strnicmp(p,prefix,prefix_len))
        {
          p+=prefix_len;
          while (*p == ' ') p++;
          if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_')
          {
            const char *q = p+1;
            while ((*q >= '0' && *q <= '9') || 
                   (*q >= 'a' && *q <= 'z') || 
                   (*q >= 'A' && *q <= 'Z') || 
                   *q == ':' || // lua
                   *q == '_' || *q == '.') q++;

            while (*q == ' ') q++;
            if (*q == '(')
            {
              while (*q && *q != ')') q++;
              if (*q) q++;

              char buf[128];
              lstrcpyn(buf, p, wdl_min(q-p+1, sizeof(buf)));
              if (strlen(buf) > sizeof(buf)-2) lstrcpyn(buf+sizeof(buf)-5, "...", 4);
              flist.AddUnsorted(buf, i);
            }
          }
        }
        p++;
      }
    }
  }
  if (flist.GetSize())
  {
    flist.Resort();
    if (m_case_sensitive) flist.Resort(WDL_LogicalSortStringKeyedArray<int>::cmpistr);

    HMENU hm=CreatePopupMenu();
    int pos=0;
    for (i=0; i < flist.GetSize(); ++i)
    {
      const char* fname=NULL;
      int line=flist.Enumerate(i, &fname);
      InsertMenu(hm, pos++, MF_STRING|MF_BYPOSITION, line+1, fname);
    }
    POINT p;
    GetCursorPos(&p);
    int ret=TrackPopupMenu(hm, TPM_NONOTIFY|TPM_RETURNCMD, p.x, p.y, 0, hwnd, NULL);
    DestroyMenu(hm);
    if (ret-- > 0)
    {
      m_curs_y=ret;
      m_select_x1=0;
      m_select_x2=strlen(m_text.Get(ret)->Get());
      m_select_y1=m_select_y2=ret;
      m_selecting=1;
      setCursor(0,0.25);
    }
  }
  else
  {
    doWatchInfo(0);
  }
}


#ifdef WDL_IS_FAKE_CURSES

LRESULT EEL_Editor::onMouseMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_LBUTTONDBLCLK:
      if (CURSES_INSTANCE && CURSES_INSTANCE->m_font_w && CURSES_INSTANCE->m_font_h)
      {
        const int y = ((short)HIWORD(lParam)) / CURSES_INSTANCE->m_font_h - m_top_margin;
        //const int x = ((short)LOWORD(lParam)) / CURSES_INSTANCE->m_font_w + m_offs_x;
        WDL_FastString *fs=m_text.Get(y + m_paneoffs_y[m_curpane]);
        if (fs && y >= 0)
        {
          if (!strncmp(fs->Get(),"import",6) && isspace(fs->Get()[6]))
          {
            onChar('R'-'A'+1); // open imported file
            return 1;
          }
        }
      }

    break;

  }
  return WDL_CursesEditor::onMouseMessage(hwnd,uMsg,wParam,lParam);
}
#endif
