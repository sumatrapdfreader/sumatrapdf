/*
  Expression Evaluator Library (NS-EEL) v2
  Copyright (C) 2004-2013 Cockos Incorporated
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  nseel-eval.c

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
*/

#include <string.h>
#include <ctype.h>
#include "ns-eel-int.h"
#include "../wdlcstring.h"


static const char *nseel_skip_space_and_comments(const char *p, const char *endptr)
{
  for (;;)
  {
    while (p < endptr && isspace(p[0])) p++;
    if (p >= endptr-1 || *p != '/') return p;

    if (p[1]=='/')
    {
      while (p < endptr && *p != '\r' && *p != '\n') p++;
    }
    else if (p[1] == '*')
    {
      p+=2;
      while (p < endptr-1 && (p[0] != '*' || p[1] != '/')) p++;
      p+=2;
      if (p>=endptr) return endptr;
    }
    else return p;
  }
}

// removes any escaped characters, also will convert pairs delim_char into single delim_chars
int nseel_filter_escaped_string(char *outbuf, int outbuf_sz, const char *rdptr, size_t rdptr_size, char delim_char) 
{
  int outpos = 0;
  const char *rdptr_end = rdptr + rdptr_size;
  while (rdptr < rdptr_end && outpos < outbuf_sz-1)
  {
    char thisc=*rdptr;
    if (thisc == '\\' && rdptr < rdptr_end-1) 
    {
      const char nc = rdptr[1];
      if (nc == 'r' || nc == 'R') { thisc = '\r'; }
      else if (nc == 'n' || nc == 'N') { thisc = '\n'; }
      else if (nc == 't' || nc == 'T') { thisc = '\t'; }
      else if (nc == 'b' || nc == 'B') { thisc = '\b'; }
      else if ((nc >= '0' && nc <= '9') || nc == 'x' || nc == 'X')
      {
        unsigned char c=0;
        char base_shift = 3; 
        char num_top = '7';

        rdptr++; // skip backslash
        if (nc > '9') // implies xX
        {
          base_shift = 4;
          num_top = '9';
          rdptr ++; // skip x
        }

        while (rdptr < rdptr_end)
        {
          char tc=*rdptr;
          if (tc >= '0' && tc <= num_top) 
          {
            c = (c<<base_shift) + tc - '0';
          }
          else if (base_shift==4)
          {
            if (tc >= 'a' && tc <= 'f')
            {
              c = (c<<base_shift) + (tc - 'a' + 10);
            }
            else if (tc >= 'A' && tc <= 'F') 
            {
              c = (c<<base_shift) + (tc - 'A' + 10);
            }
            else break;
          }
          else break;

          rdptr++;
        }
        outbuf[outpos++] = (char)c;
        continue;
      }
      else  // \c where c is an unknown character drops the backslash -- works for \, ', ", etc
      { 
        thisc = nc; 
      }
      rdptr+=2; 
    }
    else 
    {
      if (thisc == delim_char) break; 
      rdptr++;
    }
    outbuf[outpos++] = thisc;
  }
  outbuf[outpos]=0;
  return outpos;
}

int nseel_stringsegments_tobuf(char *bufOut, int bufout_sz, struct eelStringSegmentRec *list) // call with NULL to calculate size, or non-null to generate to buffer (returning size used)
{
  int pos=0;
  while (list)
  {
    if (!bufOut)
    {
      pos += list->str_len;
    }
    else if (list->str_len > 1) 
    {
      if (pos >= bufout_sz) break;
      pos += nseel_filter_escaped_string(bufOut + pos, bufout_sz-pos,  list->str_start+1, list->str_len-1, list->str_start[0]); 
    }
    list = list->_next;
  }
  return pos;
}



// state can be NULL, it will be set if finished with unterminated thing: 1 for multiline comment, ' or " for string
const char *nseel_simple_tokenizer(const char **ptr, const char *endptr, int *lenOut, int *state)
{
  const char *p = *ptr;
  const char *rv = p;
  char delim;

  if (state) // if state set, returns comments as tokens
  {
    if (*state == 1) goto in_comment;

    #ifndef NSEEL_EEL1_COMPAT_MODE
      if (*state == '\'' || *state == '\"')
      {
        delim = (char)*state;
        goto in_string;
      }
    #endif

    // skip any whitespace
    while (p < endptr && isspace(p[0])) p++;
  }
  else
  {
    // state not passed, skip comments (do not return them as tokens)
    p = nseel_skip_space_and_comments(p,endptr);
  }

  if (p >= endptr) 
  {
    *ptr = endptr;
    *lenOut = 0;
    return NULL;
  }

  rv=p;

  if (*p == '$' && p+3 < endptr && p[1] == '\'' && p[3] == '\'')
  {
    p+=4;
  }
  else if (state && *p == '/' && p < endptr-1 && (p[1] == '/' || p[1] == '*'))
  {
    if (p[1] == '/')
    {
      while (p < endptr && *p != '\r' && *p != '\n') p++; // advance to end of line
    }
    else
    {
      if (state) *state=1;
      p+=2;
in_comment:
      while (p < endptr)
      {
        const char c = *p++;
        if (c == '*' && p < endptr && *p == '/')
        {
          p++;
          if (state) *state=0;
          break;
        }
      }

    }
  }
  else if (isalnum(*p) || *p == '_' || *p == '#' || *p == '$')
  {
    if (*p == '$' && p < endptr-1 && p[1] == '~') p++;
    p++;
    while (p < endptr && (isalnum(*p) || *p == '_' || *p == '.')) p++;
  }
#ifndef NSEEL_EEL1_COMPAT_MODE
  else if (*p == '\'' || *p == '\"')
  {    
    delim = *p++;
    if (state) *state=delim;
in_string:

    while (p < endptr)
    {
      const char c = *p++;
      if (p < endptr && c == '\\') p++;  // skip escaped characters
      else if (c == delim) 
      {
        if (state) *state=0;
        break;
      }
    }
  }
#endif
  else 
  {  
    p++;
  }
  *ptr = p;
  *lenOut = (int) (p - rv);
  return p>rv ? rv : NULL;
}



#ifdef NSEEL_SUPER_MINIMAL_LEXER

  int nseellex(opcodeRec **output, YYLTYPE * yylloc_param, compileContext *scctx)
  {
    int rv=0,toklen=0;
    const char *rdptr = scctx->rdbuf;
    const char *endptr = scctx->rdbuf_end;
    const char *tok = nseel_simple_tokenizer(&rdptr,endptr,&toklen,NULL);
    *output = 0;
    if (tok)
    {
      rv = tok[0];
      if (rv == '$')
      {
        if (rdptr != tok+1)
        {
          *output = nseel_translate(scctx,tok,rdptr-tok);
          if (*output) rv=VALUE;
        }
      }
#ifndef NSEEL_EEL1_COMPAT_MODE
      else if (rv == '#' && scctx->onNamedString)
      {
        *output = nseel_translate(scctx,tok,rdptr-tok);
        if (*output) rv=STRING_IDENTIFIER;
      }
      else if (rv == '\'')
      {
        if (toklen > 1 && tok[toklen-1] == '\'')
        {
          *output = nseel_translate(scctx, tok, toklen); 
          if (*output) rv = VALUE;
        }
        else scctx->gotEndOfInput|=8;
      }
      else if (rv == '\"' && scctx->onString)
      {
        if (toklen > 1 && tok[toklen-1] == '\"')
        {
          *output = (opcodeRec *)nseel_createStringSegmentRec(scctx,tok,toklen);
          if (*output) rv = STRING_LITERAL;
        }
        else scctx->gotEndOfInput|=16;
      }
#endif
      else if (isalpha(rv) || rv == '_')
      {
        // toklen already valid
        char buf[NSEEL_MAX_VARIABLE_NAMELEN*2];
        if (toklen > sizeof(buf) - 1) toklen=sizeof(buf) - 1;
        memcpy(buf,tok,toklen);
        buf[toklen]=0;
        *output = nseel_createCompiledValuePtr(scctx, NULL, buf); 
        if (*output) rv = IDENTIFIER; 
      }
      else if ((rv >= '0' && rv <= '9') || (rv == '.' && (rdptr < endptr && rdptr[0] >= '0' && rdptr[0] <= '9')))
      {
        if (rv == '0' && rdptr < endptr && (rdptr[0] == 'x' || rdptr[0] == 'X'))
        {
          rdptr++;
          while (rdptr < endptr && (rv=rdptr[0]) && ((rv>='0' && rv<='9') || (rv>='a' && rv<='f') || (rv>='A' && rv<='F'))) rdptr++;
        }
        else
        {
          int pcnt=rv == '.';
          while (rdptr < endptr && (rv=rdptr[0]) && ((rv>='0' && rv<='9') || (rv == '.' && !pcnt++))) rdptr++;       
        }
        *output = nseel_translate(scctx,tok,rdptr-tok);
        if (*output) rv=VALUE;
      }
      else if (rv == '<')
      {
        const char nc=*rdptr;
        if (nc == '<')
        {
          rdptr++;
          rv=TOKEN_SHL;
        }
        else if (nc == '=')
        {
          rdptr++;
          rv=TOKEN_LTE;
        }
      }
      else if (rv == '>')
      {
        const char nc=*rdptr;
        if (nc == '>')
        {
          rdptr++;
          rv=TOKEN_SHR;
        }
        else if (nc == '=')
        {
          rdptr++;
          rv=TOKEN_GTE;
        }
      }
      else if (rv == '&' && *rdptr == '&')
      {
        rdptr++;
        rv = TOKEN_LOGICAL_AND;
      }      
      else if (rv == '|' && *rdptr == '|')
      {
        rdptr++;
        rv = TOKEN_LOGICAL_OR;
      }
      else if (*rdptr == '=')
      {         
        switch (rv)
        {
          case '+': rv=TOKEN_ADD_OP; rdptr++; break;
          case '-': rv=TOKEN_SUB_OP; rdptr++; break;
          case '%': rv=TOKEN_MOD_OP; rdptr++; break;
          case '|': rv=TOKEN_OR_OP;  rdptr++; break;
          case '&': rv=TOKEN_AND_OP; rdptr++; break;
          case '~': rv=TOKEN_XOR_OP; rdptr++; break;
          case '/': rv=TOKEN_DIV_OP; rdptr++; break;
          case '*': rv=TOKEN_MUL_OP; rdptr++; break;
          case '^': rv=TOKEN_POW_OP; rdptr++; break;
          case '!':
            rdptr++;
            if (rdptr < endptr && *rdptr == '=')
            {
              rdptr++;
              rv=TOKEN_NE_EXACT;
            }
            else
              rv=TOKEN_NE;
          break;
          case '=':
            rdptr++;
            if (rdptr < endptr && *rdptr == '=')
            {
              rdptr++;
              rv=TOKEN_EQ_EXACT;
            }
            else
              rv=TOKEN_EQ;
          break;
        }
      }
    }

    scctx->rdbuf = rdptr;
    yylloc_param->first_column = (int)(tok - scctx->rdbuf_start);
    return rv;
  }


  void nseelerror(YYLTYPE *pos,compileContext *ctx, const char *str)
  {
    ctx->errVar=pos->first_column>0?pos->first_column:(int)(ctx->rdbuf_end - ctx->rdbuf_start);
  }


#else

  int nseel_gets(compileContext *ctx, char *buf, size_t sz)
  {
    int n=0;
    const char *endptr = ctx->rdbuf_end;
    const char *rdptr = ctx->rdbuf;
    if (!rdptr) return 0;
    
    while (n < sz && rdptr < endptr) buf[n++] = *rdptr++;
    ctx->rdbuf=rdptr;
    return n;

  }


  //#define EEL_TRACE_LEX

  #ifdef EEL_TRACE_LEX
  #define nseellex nseellex2

  #endif
  #include "lex.nseel.c"

  #ifdef EEL_TRACE_LEX

  #undef nseellex

  int nseellex(YYSTYPE * yylval_param, YYLTYPE * yylloc_param , yyscan_t yyscanner)
  {
    int a=nseellex2(yylval_param,yylloc_param,yyscanner);

    char buf[512];
    sprintf(buf,"tok: %c (%d)\n",a,a);
    OutputDebugString(buf);
    return a;
  }
  #endif//EEL_TRACE_LEX


  void nseelerror(YYLTYPE *pos,compileContext *ctx, const char *str)
  {
    ctx->errVar=pos->first_column>0?pos->first_column:(int)(ctx->rdbuf_end - ctx->rdbuf_start);
  }
#endif // !NSEEL_SUPER_MINIMAL_LEXER
