#ifndef _WDL_HASSTRINGS_H_
#define _WDL_HASSTRINGS_H_

#ifndef WDL_HASSTRINGS_EXPORT
#define WDL_HASSTRINGS_EXPORT
#endif

static bool hasStrings_isNonWordChar(int c)
{
  // treat as whitespace when searching for " foo "
  switch (c)
  {
    case ' ':
    case '\t':
    case '.':
    case '/':
    case '\\':

      return true;

    default:
      return false;
  }
}

WDL_HASSTRINGS_EXPORT bool WDL_hasStrings(const char *name, const LineParser *lp)
{
  if (!lp) return true;
  const int ntok = lp->getnumtokens();
  if (ntok<1) return true;

  char stack[1024]; // &1=not bit, 0x10 = ignoring subscopes, &2= state when 0x10 set
  int stacktop = 0;
  stack[0]=0;

  const int strlen_name = (int)strlen(name);
  char matched_local=-1; // -1 = first eval for scope, 0=did not pass scope, 1=OK, 2=ignore rest of scope
  for (int x = 0; x < ntok; x ++)
  {
    const char *n=lp->gettoken_str(x);
    
    if (n[0] == '(' && !n[1] && !lp->gettoken_quotingchar(x))
    {
      if (!(matched_local&1))
      {
        stack[stacktop] |= matched_local | 0x10;
        matched_local=2; // ignore subscope
      }
      else 
      {
        matched_local = -1; // new scope
      }

      if (stacktop < (int)sizeof(stack) - 1) stack[++stacktop] = 0;
    }
    else if (stacktop && n[0] == ')' && !n[1] && !lp->gettoken_quotingchar(x))
    {
      if (stack[--stacktop]&0x10) 
      {
        // restore state
        matched_local = stack[stacktop]&2;
      }
      else
      {
        matched_local = (matched_local != 0 ? 1 : 0) ^ (stack[stacktop]&1);
      }
      stack[stacktop] = 0;
    }
    else if (matched_local != 2 && !strcmp(n,"OR")) 
    { 
      matched_local = (matched_local > 0) ? 2 : -1;
      stack[stacktop] = 0;
    }
    else if (matched_local&1) // matches 1, -1
    {
      int ln;
      if (!strcmp(n,"NOT")) 
      {
        stack[stacktop]^=1; 
      }
      else if (!strcmp(n,"AND") && !lp->gettoken_quotingchar(x))
      {
        // ignore unquoted uppercase AND
      }
      else if ((ln=(int)strlen(n))>0)
      {
        int lt=strlen_name;
        const char *t=name;
        // ^foo -- string starts (or follows \1 separator with) foo
        // foo$ -- string ends with foo (or is immediately followed by \1 separator)
        // " foo ", "foo ", " foo" include end of string/start of string has whitespace
        int wc_left = 0; // 1=require \1 or start of string, 2=require space or \1 or start
        int wc_right = 0; // 1=require \1 or \0, 2 = require space or \1 or \0
        // perhaps wc_left/wc_right of 2 should also match non-alnum characters in addition to space?
        if (ln>1)
        {
          switch (*n) 
          {
            case ' ': 
              if (*++n != ' ') wc_left=2;
              // else { multiple characters of whitespace = literal whitespace search (two spaces requires a single space, etc) }

              ln--;
            break;
            case '^': 
              ln--; 
              n++; 
              wc_left=1; 
            break;
          }
        }
        if (ln>1)
        {
          switch (n[ln-1]) 
          {
            case ' ':               
              if (n[--ln - 1] != ' ') wc_right=2;
              // else { multiple characters of whitespace = literal whitespace search (two spaces requires a single space, etc) }
            break;
            case '$': 
              ln--; 
              wc_right++; 
            break;
          }
        }
        if (!wc_left && !wc_right && *n)
        {
          switch (lp->gettoken_quotingchar(x))
          {
            case '\'':
            case '"':
              { // if a quoted string has no whitespace in it, treat as whole word search
                const char *p = n;
                while (*p && *p != ' ' && *p != '\t') p++;
                if (!*p) wc_left=wc_right=2;
              }
            break;
          }

        }

        if (wc_left>0)
        {
          unsigned char lastchar = 1;
          while (lt>=ln)
          {
            if ((lastchar < 2 || (wc_left>1 && hasStrings_isNonWordChar(lastchar))) && !strnicmp(t,n,ln)) 
            {
              if (wc_right == 0) break;
              const unsigned char nc=((const unsigned char*)t)[ln];
              if (nc < 2 || (wc_right > 1 && hasStrings_isNonWordChar(nc))) break;
            }
            lastchar = *(unsigned char*)t++;
            lt--;
          }
        }
        else
        {
          while (lt>=ln)
          {
            if (!strnicmp(t,n,ln)) 
            {
              if (wc_right == 0) break;
              const unsigned char nc=((const unsigned char*)t)[ln];
              if (nc < 2 || (wc_right > 1 && hasStrings_isNonWordChar(nc))) break;
            }
            t++;
            lt--;
          }
        }

        matched_local = ((lt-ln)>=0) ^ (stack[stacktop]&1);
        stack[stacktop]=0;
      }
    }
  }
  while (stacktop > 0) 
  {
    if (stack[--stacktop] & 0x10) matched_local=stack[stacktop]&2;
    else matched_local = (matched_local > 0 ? 1 : 0) ^ (stack[stacktop]&1);
  }

  return matched_local!=0;
}

WDL_HASSTRINGS_EXPORT bool WDL_makeSearchFilter(const char *flt, LineParser *lp)
{
  if (WDL_NOT_NORMALLY(!lp)) return false;

  if (WDL_NOT_NORMALLY(!flt)) flt="";

#ifdef WDL_LINEPARSER_HAS_LINEPARSERINT
  if (lp->parse_ex(flt,true,false,true)) // allow unterminated quotes
#else
  if (lp->parse_ex(flt,true,false))
#endif
  {
    if (*flt) lp->set_one_token(flt); // failed parsing search string, search as a single token
  }

  return lp->getnumtokens()>0;
}

#endif
