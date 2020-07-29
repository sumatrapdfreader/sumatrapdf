#ifndef __EEL__STRINGS_H__
#define __EEL__STRINGS_H__

#include "ns-eel-int.h"
#include "../wdlcstring.h"
#include "../wdlstring.h"

// required for context
// #define EEL_STRING_GET_CONTEXT_POINTER(opaque) (((sInst *)opaque)->m_eel_string_state)


/*
  // writeable user-strings are 0..1023 (EEL_STRING_MAX_USER_STRINGS-1), and can be up to about EEL_STRING_MAXUSERSTRING_LENGTH_HINT bytes long

   printf("string %d blah");             -- output to log, allows %d %u %f etc, if host implements formats [1]
   sprintf(str,"string %d blah");        -- output to str [1]
   strlen(str);                          -- returns string length
   match("*test*", "this is a test")     -- search for first parameter regex-style in second parameter
   matchi("*test*", "this is a test")    -- search for first parameter regex-style in second parameter (case insensitive)
          // %s means 1 or more chars
          // %0s means 0 or more chars
          // %5s means exactly 5 chars
          // %5-s means 5 or more chars
          // %-10s means 1-10 chars
          // %3-5s means 3-5 chars. 
          // %0-5s means 0-5 chars. 
          // %S (uppercase) indicates lazy match (%D, %F, %X, etc)

   strcpy(str, srcstr);                  -- replaces str with srcstr
   strcat(str, srcstr);                  -- appends srcstr to str 
   strcmp(str, str2)                     -- compares strings
   stricmp(str, str2)                    -- compares strings (ignoring case)
   strncmp(str, str2, maxlen)            -- compares strings up to maxlen bytes
   strnicmp(str, str2, maxlen)           -- compares strings (ignoring case) up to maxlen bytes
   strncpy(str, srcstr, maxlen);         -- replaces str with srcstr, up to maxlen (-1 for unlimited)
   strncat(str, srcstr, maxlen);         -- appends up to maxlen of srcstr to str (-1 for unlimited)
   strcpy_from(str,srcstr, offset);      -- copies srcstr to str, but starts reading srcstr at offset offset
   strcpy_substr(str, srcstr, offs, ml)  -- php-style (start at offs, offs<0 means from end, ml for maxlen, ml<0 = reduce length by this amt)
   str_getchar(str, offset[, type]);     -- returns value at offset offset, type can be omitted or 0 or 'c', 's', 'S', 'i', 'I', 'f', 'F', 'd', 'D', 'uc', 'us', 'US', 'ui', 'UI'
                                         -- negative offset is offset from end of string
   str_setchar(str, offset, val[, type]); - sets value at offset offset, type optional. offset must be [0,length], if length it can lengthen string, if >length then call fails
                                         -- negative offset is offset from end of string

   str_setlen(str, len);                 -- sets length of string (if increasing, will be space-padded)
   str_delsub(str, pos, len);            -- deletes len chars at pos
   str_insert(str, srcstr, pos);         -- inserts srcstr at pos

  [1]: note: printf/sprintf are NOT binary safe when using %s with modifiers (such as %100s etc) -- the source string being formatted
             will terminate at the first NULL character
  );



  */

// define this to allow modifying literal strings via code, i.e. foobar = strcat("foo","bar");
// disabled by default, so literals can be pooled/reused/etc
// #define EEL_STRINGS_MUTABLE_LITERALS

#ifndef EEL_STRING_MAXUSERSTRING_LENGTH_HINT
#define EEL_STRING_MAXUSERSTRING_LENGTH_HINT 16384
#endif

#ifndef EEL_STRING_MAX_USER_STRINGS
// strings 0...x-1
#define EEL_STRING_MAX_USER_STRINGS 1024
#endif

#ifndef EEL_STRING_LITERAL_BASE
// strings defined by "xyz"
#define EEL_STRING_LITERAL_BASE 10000
#endif

// base for named mutable strings (#xyz)
#ifndef EEL_STRING_NAMED_BASE 
#define EEL_STRING_NAMED_BASE  90000
#endif

// base for unnamed mutable strings (#)
#ifndef EEL_STRING_UNNAMED_BASE 
#define EEL_STRING_UNNAMED_BASE  190000
#endif

// define EEL_STRING_MUTEXLOCK_SCOPE for custom, otherwise EEL_STRING_WANT_MUTEX for builtin locking
#ifndef EEL_STRING_MUTEXLOCK_SCOPE
  #ifdef EEL_STRING_WANT_MUTEX
     #include "../mutex.h"
     #define EEL_STRING_MUTEXLOCK_SCOPE WDL_MutexLock __lock(&(EEL_STRING_GET_CONTEXT_POINTER(opaque)->m_mutex));
  #else
    #define EEL_STRING_MUTEXLOCK_SCOPE
  #endif
#endif

// allow overriding behavior
#ifndef EEL_STRING_GET_FOR_INDEX
#define EEL_STRING_GET_FOR_INDEX(x, wr) (EEL_STRING_GET_CONTEXT_POINTER(opaque)->GetStringForIndex(x, wr, false))
#endif

#ifndef EEL_STRING_GET_FOR_WRITE
#define EEL_STRING_GET_FOR_WRITE(x, wr) (EEL_STRING_GET_CONTEXT_POINTER(opaque)->GetStringForIndex(x, wr, true))
#endif

#ifndef EEL_STRING_GETFMTVAR
#define EEL_STRING_GETFMTVAR(x) (EEL_STRING_GET_CONTEXT_POINTER(opaque)->GetVarForFormat(x))
#endif

#ifndef EEL_STRING_GETNAMEDVAR
#define EEL_STRING_GETNAMEDVAR(x,createOK,altOut) (EEL_STRING_GET_CONTEXT_POINTER(opaque)->GetNamedVar(x,createOK,altOut))
#endif





#ifndef EEL_STRING_STORAGECLASS
#define EEL_STRING_STORAGECLASS WDL_FastString
#endif



class eel_string_context_state
{
  public:
    static int cmpistr(const char **a, const char **b) { return stricmp(*a,*b); }

    eel_string_context_state()  : m_named_strings_names(false), m_varname_cache(cmpistr)
    {
      m_vm=0;
      memset(m_user_strings,0,sizeof(m_user_strings));
    }
    ~eel_string_context_state()
    {
      clear_state(true);
    }

    void clear_state(bool full)
    {
      if (full)
      {
        int x;
        for (x=0;x<EEL_STRING_MAX_USER_STRINGS;x++) 
        {
          delete m_user_strings[x];
          m_user_strings[x]=0;
        }
        m_named_strings_names.DeleteAll();
        m_named_strings.Empty(true);
      }
      if (full) m_literal_strings.Empty(true);
      m_varname_cache.DeleteAll();
      m_unnamed_strings.Empty(true);
    }

    void update_named_vars(NSEEL_VMCTX vm) // call after compiling any code, or freeing code, etc
    {
      m_vm = vm;
      m_varname_cache.DeleteAll();
      if (vm) NSEEL_VM_enumallvars(vm,varEnumProc, this);
      m_varname_cache.Resort();
    }

    EEL_F *GetVarForFormat(int formatidx) 
    { 
      return NULL;  // must use %{xyz}s syntax -- override to change defaults
    }

    // if named variables are used, must call update_named_vars(vm) to set context/generate list
    EEL_F *GetNamedVar(const char *s, bool createIfNotExists, EEL_F *altOut)
    {
      if (!s || !*s) return NULL;

      if (s[0] == '#')
      {
        if (!s[1] || !altOut) return NULL;

        int idx = m_named_strings_names.Get(s+1);
        if (!idx && createIfNotExists)
        {
          idx = m_named_strings.GetSize() + EEL_STRING_NAMED_BASE;
          m_named_strings.Add(new EEL_STRING_STORAGECLASS);
          m_named_strings_names.Insert(s+1,idx);
        }
        if (!idx) return NULL;

        *altOut = (EEL_F) idx;
        return altOut;
      }

      EEL_F *r = m_varname_cache.Get(s);
      if (r || !createIfNotExists || !m_vm) return r;

      const char *p=NULL;
      r=nseel_int_register_var((compileContext*)m_vm,s,0,&p);
      if (r&&p) m_varname_cache.Insert(p,r);
      return r;
    }

    const char *GetStringForIndex(EEL_F val, EEL_STRING_STORAGECLASS **stringContainerOut=NULL, bool is_for_write=false)
    {
      int idx = (int) (val+0.5);
      if (idx>=0 && idx < EEL_STRING_MAX_USER_STRINGS)
      {
        if (stringContainerOut)
        {
          if (!m_user_strings[idx]) m_user_strings[idx] = new EEL_STRING_STORAGECLASS;
          *stringContainerOut = m_user_strings[idx];
        }
        return m_user_strings[idx]?m_user_strings[idx]->Get():"";
      }
      EEL_STRING_STORAGECLASS *s;
      s = m_unnamed_strings.Get(idx - EEL_STRING_UNNAMED_BASE);
      if (!s) s= m_named_strings.Get(idx - EEL_STRING_NAMED_BASE);

      if (s)
      {
        // mutable string
        if (stringContainerOut) *stringContainerOut=s;
      }
      else
      {
        s = m_literal_strings.Get(idx - EEL_STRING_LITERAL_BASE);
        #ifdef EEL_STRINGS_MUTABLE_LITERALS
          if (stringContainerOut) *stringContainerOut=s;
        #else
          if (stringContainerOut) *stringContainerOut=is_for_write ? NULL : s;
        #endif
      }
      return s ? s->Get() : NULL;
    }

    WDL_PtrList<EEL_STRING_STORAGECLASS> m_literal_strings; // "this kind", normally immutable
    WDL_PtrList<EEL_STRING_STORAGECLASS> m_unnamed_strings; // #
    WDL_PtrList<EEL_STRING_STORAGECLASS> m_named_strings;  // #xyz by index, but stringkeyed below for names
    WDL_StringKeyedArray<int> m_named_strings_names; // #xyz->index

    EEL_STRING_STORAGECLASS *m_user_strings[EEL_STRING_MAX_USER_STRINGS]; // indices 0-1023 (etc)
    WDL_AssocArray<const char *, EEL_F_PTR> m_varname_cache; // cached pointers when using %{xyz}s, %{#xyz}s bypasses

    NSEEL_VMCTX m_vm;
#ifdef EEL_STRING_WANT_MUTEX
    WDL_Mutex m_mutex;
#endif

    static EEL_F addNamedStringCallback(void *opaque, const char *name)
    {
      if (!opaque) return -1.0;
      eel_string_context_state *_this = EEL_STRING_GET_CONTEXT_POINTER(opaque);
      if (!_this) return -1.0;

      EEL_STRING_MUTEXLOCK_SCOPE
      if (!name || !name[0])
      {
        _this->m_unnamed_strings.Add(new EEL_STRING_STORAGECLASS);
        return (EEL_F) (_this->m_unnamed_strings.GetSize()-1 + EEL_STRING_UNNAMED_BASE);
      }

      int a = _this->m_named_strings_names.Get(name);
      if (a) return (EEL_F)a;

      a = _this->m_named_strings.GetSize() + EEL_STRING_NAMED_BASE;
      _this->m_named_strings.Add(new EEL_STRING_STORAGECLASS);
      _this->m_named_strings_names.Insert(name,a);

      return (EEL_F)a;
    }

    static EEL_F addStringCallback(void *opaque, struct eelStringSegmentRec *list)
    {
      if (!opaque) return -1.0;

      eel_string_context_state *_this = EEL_STRING_GET_CONTEXT_POINTER(opaque);
      if (!_this) return -1.0;

      EEL_STRING_STORAGECLASS *ns = new EEL_STRING_STORAGECLASS;
      // could probably do a faster implementation using AddRaw() etc but this should also be OK
      int sz=nseel_stringsegments_tobuf(NULL,0,list);
      ns->SetLen(sz+32);
      sz=nseel_stringsegments_tobuf((char *)ns->Get(),sz,list);
      ns->SetLen(sz);
      EEL_STRING_MUTEXLOCK_SCOPE
      return (EEL_F)_this->AddString(ns);
   }
   int AddString(EEL_STRING_STORAGECLASS *ns)
   {
#ifdef EEL_STRINGS_MUTABLE_LITERALS
     m_literal_strings.Add(ns);
     return m_literal_strings.GetSize()-1+EEL_STRING_LITERAL_BASE;
#else
     const int l = ns->GetLength();
     const int sz=m_literal_strings.GetSize();
     int x;
     for (x=0;x<sz;x++)
     {
       EEL_STRING_STORAGECLASS *s = m_literal_strings.Get(x);
       if (ns->GetLength() == l && !strcmp(s->Get(),ns->Get())) break;
     }
     if (x<sz) delete ns;
     else m_literal_strings.Add(ns);
     return x+EEL_STRING_LITERAL_BASE;
#endif
   }

   static int varEnumProc(const char *name, EEL_F *val, void *ctx)
   {
     ((eel_string_context_state *)ctx)->m_varname_cache.AddUnsorted(name,val);
     return 1;
   }

};



static int eel_validate_format_specifier(const char *fmt_in, char *typeOut, 
                                         char *fmtOut, int fmtOut_sz, 
                                         char *varOut, int varOut_sz, 
                                         int *varOut_used
                                         )
{
  const char *fmt = fmt_in+1;
  int state=0;
  if (fmt_in[0] != '%') return 0; // ugh passed a non-specifier

  *varOut_used = 0;
  *varOut = 0;

  if (fmtOut_sz-- < 2) return 0;
  *fmtOut++ = '%';

  while (*fmt)
  {
    const char c = *fmt++;
    if (fmtOut_sz < 2) return 0;

    if (c == 'f'|| c=='e' || c=='E' || c=='g' || c=='G' || c == 'd' || c == 'u' || 
        c == 'x' || c == 'X' || c == 'c' || c == 'C' || c =='s' || c=='S' || c=='i') 
    {
      *typeOut = c;
      fmtOut[0] = c;
      fmtOut[1] = 0;
      return (int) (fmt - fmt_in);
    }
    else if (c == '.')
    {
      *fmtOut++ = c; fmtOut_sz--;
      if (state&(2)) break;
      state |= 2;
    }
    else if (c == '+') 
    {
      *fmtOut++ = c; fmtOut_sz--;
      if (state&(32|16|8|4)) break;
      state |= 8;
    }
    else if (c == '-' || c == ' ') 
    {
      *fmtOut++ = c; fmtOut_sz--;
      if (state&(32|16|8|4)) break;
      state |= 16;
    }
    else if (c >= '0' && c <= '9') 
    {
      *fmtOut++ = c; fmtOut_sz--;
      state|=4;
    }
    else if (c == '{')
    {
      if (state & 64) break;
      state|=64;
      if (*fmt == '.' || (*fmt >= '0' && *fmt <= '9')) return 0; // symbol name can't start with 0-9 or .

      while (*fmt != '}')
      {
        if ((*fmt >= 'a' && *fmt <= 'z') ||
            (*fmt >= 'A' && *fmt <= 'Z') ||
            (*fmt >= '0' && *fmt <= '9') ||
            *fmt == '_' || *fmt == '.' || *fmt == '#')
        {
          if (varOut_sz < 2) return 0;
          *varOut++ = *fmt++;
          varOut_sz -- ;
        }
        else
        {
          return 0; // bad character in variable name
        }
      }
      fmt++;
      *varOut = 0;
      *varOut_used=1;
    }
    else
    {
      break;
    }
  }
  return 0;
}

int eel_format_strings(void *opaque, const char *fmt, const char *fmt_end, char *buf, int buf_sz, int num_fmt_parms, EEL_F **fmt_parms)
{
  int fmt_parmpos = 0;
  char *op = buf;
  while ((fmt_end ? fmt < fmt_end : *fmt) && op < buf+buf_sz-128)
  {
    if (fmt[0] == '%' && fmt[1] == '%') 
    {
      *op++ = '%';
      fmt+=2;
    }
    else if (fmt[0] == '%')
    {
      char ct=0;
      char fs[128];
      char varname[128];
      int varname_used=0;
      const int l=eel_validate_format_specifier(fmt,&ct,fs,sizeof(fs),varname,sizeof(varname),&varname_used);
      if (!l || !ct) 
      {
        *op=0;
        return -1;
      }

      EEL_F vv=0.0;
      const EEL_F *varptr = NULL;
      if (varname_used)
      {
#ifdef EEL_STRING_GETNAMEDVAR
        if (varname[0]) varptr=EEL_STRING_GETNAMEDVAR(varname,0,&vv);
#endif
      }
      else
      {
        if (fmt_parmpos < num_fmt_parms) varptr = fmt_parms[fmt_parmpos];
#ifdef EEL_STRING_GETFMTVAR
        if (!varptr) varptr = EEL_STRING_GETFMTVAR(fmt_parmpos);
#endif
        fmt_parmpos++;
      }
      double v = varptr ? (double)*varptr : 0.0;

      if (ct == 's' || ct=='S')
      {
        EEL_STRING_STORAGECLASS *wr=NULL;
        const char *str = EEL_STRING_GET_FOR_INDEX(v,&wr);
        const int maxl=(int) (buf+buf_sz - 2 - op);
        if (wr && !fs[2]) // %s or %S -- todo: implement padding modes for binary compat too?
        {
          int wl = wr->GetLength();
          if (wl > maxl) wl=maxl;
          memcpy(op,wr->Get(),wl);
          op += wl;
          *op=0;
        }
        else
        {
          snprintf(op,maxl,fs,str ? str : "");
        }
      }
      else
      {
        if (varptr == &vv) // passed %{#str}d etc, convert to float
        {
          const char *str = EEL_STRING_GET_FOR_INDEX(v,NULL);
          v = str ? atof(str) : 0.0;
        }

        if (ct == 'x' || ct == 'X' || ct == 'd' || ct == 'u' || ct=='i')
        {
          snprintf(op,64,fs,(int) (v));
        }
        else if (ct == 'c')
        {
          *op++=(char) (int)v;
          *op=0;
        }
        else if (ct == 'C')
        {
          const unsigned int iv = (unsigned int) v;
          int bs = 0;
          if (iv &      0xff000000) bs=24;
          else if (iv & 0x00ff0000) bs=16;
          else if (iv & 0x0000ff00) bs=8;
          while (bs>=0)
          {
            const char c=(char) (iv>>bs);
            *op++=c?c:' ';
            bs-=8;
          }
          *op=0;
        }
        else
        {
#if !defined(_WIN32) && !defined(__arm__) && !defined(__aarch64__)
          // x86 and x86_64 set rounding to truncate (ugh)
          // apparently on Windows it doesn't matter for sprintf(), though.
          // this is safe to call on other platforms, too, just perhaps wasteful
          int fpstate[2];
          eel_enterfp(fpstate);
          eel_setfp_round();
#endif

          snprintf(op,64,fs,v);

#if !defined(_WIN32) && !defined(__arm__) && !defined(__aarch64__)
          eel_leavefp(fpstate);
#endif
        }
      }

      while (*op) op++;

      fmt += l;
    }
    else 
    {
      *op++ = *fmt++;
    }

  }
  *op=0;
  return (int) (op - buf);
}



static int eel_string_match(void *opaque, const char *fmt, const char *msg, int match_fmt_pos, int ignorecase, const char *fmt_endptr, const char *msg_endptr, int num_fmt_parms, EEL_F **fmt_parms)
{
  // check for match, updating EEL_STRING_GETFMTVAR(*) as necessary
  // %d=12345
  // %f=12345[.678]
  // %c=any nonzero char, ascii value
  // %x=12354ab
  // %*, %?, %+, %% literals
  // * ? +  match minimal groups of 0+,1, or 1+ chars
  for (;;)
  {
    if (fmt>=fmt_endptr)
    {
      if (msg>=msg_endptr) return 1;
      return 0; // format ends before matching string
    }

    // if string ends and format is not on a wildcard, early-out to 0
    if (msg>=msg_endptr && *fmt != '*' && *fmt != '%') return 0;

    switch (*fmt)
    {
      case '*':
      case '+':
        // if last char of search pattern, we're done!
        if (fmt+1>=fmt_endptr || (fmt[1] == '?' && fmt+2>=fmt_endptr)) return *fmt == '*' || msg<msg_endptr;

        if (fmt[0] == '+')  msg++; // skip a character for + . Note that in this case msg[1] is valid, because of the !*msg && *fmt != '*' check above

        fmt++;
        if (*fmt == '?')
        {
          // *? or +? are lazy matches
          fmt++;

          while (msg<msg_endptr && !eel_string_match(opaque,fmt, msg,match_fmt_pos,ignorecase,fmt_endptr, msg_endptr,num_fmt_parms,fmt_parms)) msg++;
          return msg<msg_endptr;
        }
        else
        {
          // greedy match
          int len = (int) (msg_endptr-msg);
          while (len >= 0 && !eel_string_match(opaque,fmt, msg+len,match_fmt_pos,ignorecase,fmt_endptr, msg_endptr,num_fmt_parms,fmt_parms)) len--;
          return len >= 0;
        }
      break;
      case '?':
        fmt++;
        msg++;
      break;
      case '%':
        {
          fmt++;
          unsigned short fmt_minlen = 1, fmt_maxlen = 0;
          if (*fmt >= '0' && *fmt <= '9')
          {
            fmt_minlen = *fmt++ - '0';
            while (*fmt >= '0' && *fmt <= '9') fmt_minlen = fmt_minlen * 10 + (*fmt++ - '0');
            fmt_maxlen = fmt_minlen;
          }
          if (*fmt == '-')
          {
            fmt++;
            fmt_maxlen = 0;
            while (*fmt >= '0' && *fmt <= '9') fmt_maxlen = fmt_maxlen * 10 + (*fmt++ - '0');
          }
          const char *dest_varname=NULL;
          if (*fmt == '{')
          {
            dest_varname=++fmt;
            while (*fmt && fmt < fmt_endptr && *fmt != '}') fmt++;
            if (fmt >= fmt_endptr-1 || *fmt != '}') return 0; // malformed %{var}s
            fmt++; // skip '}'
          }

          char fmt_char = *fmt++;
          if (!fmt_char) return 0; // malformed

          if (fmt_char == '*' || 
              fmt_char == '?' || 
              fmt_char == '+' || 
              fmt_char == '%')
          {
            if (*msg++ != fmt_char) return 0;
          }
          else if (fmt_char == 'c')
          {
            EEL_F *varOut = NULL;
            EEL_F vv=0.0;
            if (!dest_varname) 
            {
              if (match_fmt_pos < num_fmt_parms) varOut = fmt_parms[match_fmt_pos];
#ifdef EEL_STRING_GETFMTVAR
              if (!varOut) varOut = EEL_STRING_GETFMTVAR(match_fmt_pos);
#endif
              match_fmt_pos++;
            }
            else
            {
#ifdef EEL_STRING_GETNAMEDVAR 
              char tmp[128];
              int idx=0;
              while (dest_varname < fmt_endptr && *dest_varname && *dest_varname != '}' && idx<(int)sizeof(tmp)-1) tmp[idx++] = *dest_varname++;
              tmp[idx]=0;
              if (idx>0) varOut = EEL_STRING_GETNAMEDVAR(tmp,1,&vv);
#endif
            }
            if (msg >= msg_endptr) return 0; // out of chars

            if (varOut) 
            {
              if (varOut == &vv) // %{#foo}c
              {
                EEL_STRING_STORAGECLASS *wr=NULL;
                EEL_STRING_GET_FOR_WRITE(vv, &wr);
                if (wr) wr->Set(msg,1);
              }
              else
              {
                *varOut = (EEL_F)*(unsigned char *)msg;
              }
            }
            msg++;
          }
          else 
          {
            int len=0;
            int lazy=0;
            if (fmt_char>='A'&&fmt_char<='Z') { lazy=1; fmt_char += 'a' - 'A'; }

            if (fmt_char == 's')
            {
              len = (int) (msg_endptr-msg);
            }
            else if (fmt_char == 'x')
            {
              while ((msg[len] >= '0' && msg[len] <= '9') ||
                     (msg[len] >= 'A' && msg[len] <= 'F') ||
                     (msg[len] >= 'a' && msg[len] <= 'f')) len++;
            }
            else if (fmt_char == 'f')
            {
              if (msg[len] == '-') len++;
              while (msg[len] >= '0' && msg[len] <= '9') len++;
              if (msg[len] == '.') 
              { 
                len++; 
                while (msg[len] >= '0' && msg[len] <= '9') len++;
              }
            }
            else if (fmt_char == 'd' || fmt_char == 'u' || fmt_char == 'i')
            {
              if (fmt_char != 'u' && msg[len] == '-') len++;
              while (msg[len] >= '0' && msg[len] <= '9') len++;
            }
            else 
            {
              // bad format
              return 0;
            }

            if (fmt_maxlen>0 && len > fmt_maxlen) len = fmt_maxlen;

            if (!dest_varname) match_fmt_pos++;

            if (lazy)
            {
              if (fmt_maxlen<1 || fmt_maxlen>len) fmt_maxlen=len;
              len=fmt_minlen;
              while (len <= fmt_maxlen && !eel_string_match(opaque,fmt, msg+len,match_fmt_pos,ignorecase,fmt_endptr, msg_endptr,num_fmt_parms,fmt_parms)) len++;
              if (len > fmt_maxlen) return 0;
            }
            else
            {
              while (len >= fmt_minlen && !eel_string_match(opaque,fmt, msg+len,match_fmt_pos,ignorecase,fmt_endptr, msg_endptr,num_fmt_parms,fmt_parms)) len--;
              if (len < fmt_minlen) return 0;
            }

            EEL_F vv=0.0;
            EEL_F *varOut = NULL;
            if (!dest_varname) 
            {
              if (match_fmt_pos>0 && match_fmt_pos-1 < num_fmt_parms) varOut = fmt_parms[match_fmt_pos-1];
#ifdef EEL_STRING_GETFMTVAR
              if (!varOut) varOut = EEL_STRING_GETFMTVAR(match_fmt_pos-1);
#endif
            }
            else
            {
#ifdef EEL_STRING_GETNAMEDVAR 
              char tmp[128];
              int idx=0;
              while (dest_varname < fmt_endptr && *dest_varname  && *dest_varname != '}' && idx<(int)sizeof(tmp)-1) tmp[idx++] = *dest_varname++;
              tmp[idx]=0;
              if (idx>0) varOut = EEL_STRING_GETNAMEDVAR(tmp,1,&vv);
#endif
            }
            if (varOut)
            {
              if (fmt_char == 's')
              {
                EEL_STRING_STORAGECLASS *wr=NULL;
                EEL_STRING_GET_FOR_WRITE(*varOut, &wr);
                if (wr)
                {
                  if (msg_endptr >= wr->Get() && msg_endptr <= wr->Get() + wr->GetLength())
                  {
#ifdef EEL_STRING_DEBUGOUT
                    EEL_STRING_DEBUGOUT("match: destination specifier passed is also haystack, will not update");
#endif
                  }
                  else if (fmt_endptr >= wr->Get() && fmt_endptr <= wr->Get() + wr->GetLength())
                  {
#ifdef EEL_STRING_DEBUGOUT
                    EEL_STRING_DEBUGOUT("match: destination specifier passed is also format, will not update");
#endif
                  }
                  else 
                  {
                    wr->SetRaw(msg,len);
                  }
                }
                else
                {
#ifdef EEL_STRING_DEBUGOUT
                   EEL_STRING_DEBUGOUT("match: bad destination specifier passed as %d: %f",match_fmt_pos,*varOut);
#endif
                }
              }
              else
              {
                char tmp[128];
                lstrcpyn_safe(tmp,msg,wdl_min(len+1,(int)sizeof(tmp)));
                if (varOut == &vv) 
                {
                  EEL_STRING_STORAGECLASS *wr=NULL;
                  EEL_STRING_GET_FOR_WRITE(vv, &wr);
                  if (wr) wr->Set(tmp);
                }
                else
                {
                  char *bl=(char*)msg;
                  if (fmt_char == 'u')
                    *varOut = (EEL_F)strtoul(tmp,&bl,10);
                  else if (fmt_char == 'x')
                    *varOut = (EEL_F)strtoul(msg,&bl,16);
                  else
                    *varOut = (EEL_F)atof(tmp);
                }
              }
            }
            return 1;
          }
        }
      break;
      default:
        if (ignorecase ? (toupper(*fmt) != toupper(*msg)) : (*fmt!= *msg)) return 0;
        fmt++;
        msg++;
      break;
    }
  }
}



static EEL_F NSEEL_CGEN_CALL _eel_sprintf(void *opaque, INT_PTR num_param, EEL_F **parms)
{
  if (num_param<2) return 0.0;

  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_WRITE(*(parms[0]), &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("sprintf: bad destination specifier passed %f",*(parms[0]));
#endif
    }
    else
    {
      EEL_STRING_STORAGECLASS *wr_src=NULL;
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*(parms[1]),&wr_src);
      if (fmt)
      {
        char buf[16384];
        const int fmt_len = eel_format_strings(opaque,fmt,wr_src?(fmt+wr_src->GetLength()):NULL,buf,(int)sizeof(buf), (int)num_param-2, parms+2);

        if (fmt_len>=0)
        {
          wr->SetRaw(buf,fmt_len);
        }
        else
        {
#ifdef EEL_STRING_DEBUGOUT
          EEL_STRING_DEBUGOUT("sprintf: bad format string %s",fmt);
#endif
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("sprintf: bad format specifier passed %f",*(parms[1]));
#endif
      }
    }
  }
  return *(parms[0]);
}


static EEL_F NSEEL_CGEN_CALL _eel_strncat(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL, *wr_src=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str%scat: bad destination specifier passed %f",maxlen ? "n":"",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,&wr_src);
      if (fmt||wr_src)
      {
        if (wr->GetLength() > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
        {
#ifdef EEL_STRING_DEBUGOUT
          EEL_STRING_DEBUGOUT("str%scat: will not grow string since it is already %d bytes",maxlen ? "n":"",wr->GetLength());
#endif
        }
        else
        {
          int ml=0;
          if (maxlen && *maxlen > 0) ml = (int)*maxlen;
          if (wr_src)
          {
            EEL_STRING_STORAGECLASS tmp;
            if (wr_src == wr) *(wr_src=&tmp) = *wr;
            wr->AppendRaw(wr_src->Get(), ml > 0 && ml < wr_src->GetLength() ? ml : wr_src->GetLength());
          }
          else
            wr->Append(fmt, ml);
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("str%scat: bad format specifier passed %f",maxlen ? "n":"",*fmt_index);
#endif
      }
    }
  }
  return *strOut;
}

static EEL_F NSEEL_CGEN_CALL _eel_strcpysubstr(void *opaque, INT_PTR nparm, EEL_F **parms) //EEL_F *strOut, EEL_F *fmt_index, EEL_F *offs
{
  if (opaque && nparm>=3)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL, *wr_src=NULL;
    EEL_STRING_GET_FOR_WRITE(parms[0][0], &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("strcpy_substr: bad destination specifier passed %f",parms[0][0]);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(parms[1][0],&wr_src);
      if (fmt)
      {
        const int fmt_len  = wr_src ? wr_src->GetLength() : (int) strlen(fmt);
        int maxlen, o = (int) parms[2][0];
        if (o < 0) 
        {
          o = fmt_len + o;
          if (o < 0) o=0;
        }
        maxlen = fmt_len - o;
        if (nparm >= 4)
        {
          const int a = (int) parms[3][0];
          if (a<0) maxlen += a;
          else if (a<maxlen) maxlen=a;
        }

        if (maxlen < 1 || o >= fmt_len) 
        {
          wr->Set("");
        }
        else 
        {
          if (wr_src==wr) 
          {
            wr->DeleteSub(0,o);
            if (wr->GetLength() > maxlen) wr->SetLen(maxlen);
          }
          else 
          {
            wr->SetRaw(fmt+o,maxlen);
          }
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("strcpy_substr: bad format specifier passed %f",parms[1][0]);
#endif
      }
    }
    return parms[0][0];
  }
  return 0.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strncpy(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL, *wr_src=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str%scpy: bad destination specifier passed %f",maxlen ? "n":"",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,&wr_src);
      if (fmt)
      {
        int ml=-1;
        if (maxlen && *maxlen >= 0) ml = (int)*maxlen;

        if (wr_src == wr) 
        {
          if (ml>=0 && ml < wr->GetLength()) wr->SetLen(ml); // shorten string if strncpy(x,x,len) and len >=0
          return *strOut; 
        }

        if (wr_src) wr->SetRaw(fmt, ml>0 && ml < wr_src->GetLength() ? ml : wr_src->GetLength());
        else wr->Set(fmt,ml);
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("str%scpy: bad format specifier passed %f",maxlen ? "n":"",*fmt_index);
#endif
      }
    }
  }
  return *strOut;
}

static EEL_F _eel_strcmp_int(const char *a, int a_len, const char *b, int b_len, int ml, bool ignorecase)
{
  // binary-safe comparison (at least if a_len>=0 etc)
  int pos = 0;
  for (;;)
  {
    if (ml > 0 && pos == ml) return 0.0;
    const bool a_end = a_len >= 0 ? pos == a_len : !a[pos];
    const bool b_end = b_len >= 0 ? pos == b_len : !b[pos];
    if (a_end || b_end)
    {
      if (!b_end) return -1.0; // b[pos] is nonzero, a[pos] is zero
      if (!a_end) return 1.0;  
      return 0.0;
    }
    char av = a[pos];
    char bv = b[pos];
    if (ignorecase) 
    {
      av=toupper(av);
      bv=toupper(bv);
    }
    if (bv > av) return -1.0;
    if (av > bv) return 1.0;

    pos++;
  }
}

static EEL_F NSEEL_CGEN_CALL _eel_strncmp(void *opaque, EEL_F *aa, EEL_F *bb, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr_a=NULL,*wr_b=NULL;
    const char *a = EEL_STRING_GET_FOR_INDEX(*aa,&wr_a);
    const char *b = EEL_STRING_GET_FOR_INDEX(*bb,&wr_b);
    if (!a || !b)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str%scmp: bad specifier(s) passed %f/%f",maxlen ? "n" : "",*aa,*bb);
#endif
    }
    else
    {
      const int ml = maxlen ? (int) *maxlen : -1;
      if (!ml || a==b) return 0; // strncmp(x,y,0) == 0

      return _eel_strcmp_int(a,wr_a ? wr_a->GetLength() : -1,b,wr_b ? wr_b->GetLength() : -1, ml, false);
    }
  }
  return -1.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strnicmp(void *opaque, EEL_F *aa, EEL_F *bb, EEL_F *maxlen)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr_a=NULL,*wr_b=NULL;
    const char *a = EEL_STRING_GET_FOR_INDEX(*aa,&wr_a);
    const char *b = EEL_STRING_GET_FOR_INDEX(*bb,&wr_b);
    if (!a || !b)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str%sicmp: bad specifier(s) passed %f/%f",maxlen ? "n" : "",*aa,*bb);
#endif
    }
    else
    {
      const int ml = maxlen ? (int) *maxlen : -1;
      if (!ml || a==b) return 0; // strncmp(x,y,0) == 0
      return _eel_strcmp_int(a,wr_a ? wr_a->GetLength() : -1,b,wr_b ? wr_b->GetLength() : -1, ml, true);
    }
  }
  return -1.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_strcat(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncat(opaque,strOut,fmt_index,NULL);
}

static EEL_F NSEEL_CGEN_CALL _eel_strcpy(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncpy(opaque,strOut,fmt_index,NULL);
}


static EEL_F NSEEL_CGEN_CALL _eel_strcmp(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strncmp(opaque,strOut,fmt_index,NULL);
}

static EEL_F NSEEL_CGEN_CALL _eel_stricmp(void *opaque, EEL_F *strOut, EEL_F *fmt_index)
{
  return _eel_strnicmp(opaque,strOut,fmt_index,NULL);
}


static EEL_F NSEEL_CGEN_CALL _eel_strgetchar(void *opaque, EEL_F *strOut, EEL_F *idx)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*strOut, &wr);
    if (!fmt)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_getchar: bad specifier passed %f",*strOut);
#endif
    }
    else
    {
      const int wl=(wr?wr->GetLength():(int)strlen(fmt));
      int l = (int) *idx;
      if (*idx < 0.0) l+=wl;
      if (l>=0 && l < wl) return ((unsigned char *)fmt)[l];
    }
  }
  return 0;
}

#define EEL_GETCHAR_FLAG_ENDIANSWAP 0x10
#define EEL_GETCHAR_FLAG_UNSIGNED 0x20
#define EEL_GETCHAR_FLAG_FLOAT 0x40
static int eel_getchar_flag(int type)
{
#ifdef __ppc__
  int ret=EEL_GETCHAR_FLAG_ENDIANSWAP; // default to LE
#else
  int ret=0;
#endif

  if (toupper((type>>8)&0xff) == 'U') ret|=EEL_GETCHAR_FLAG_UNSIGNED;
  else if (type>255 && toupper(type&0xff) == 'U') { ret|=EEL_GETCHAR_FLAG_UNSIGNED; type>>=8; }
  type&=0xff;

  if (isupper(type)) ret^=EEL_GETCHAR_FLAG_ENDIANSWAP;
  else type += 'A'-'a';

  switch (type)
  {
    case 'F': return ret|4|EEL_GETCHAR_FLAG_FLOAT;
    case 'D': return ret|8|EEL_GETCHAR_FLAG_FLOAT;
    case 'S': return ret|2;
    case 'I': return ret|4;
  }

  return ret|1;
}

static void eel_setchar_do(int flag, char *dest, EEL_F val)
{
  union
  {
    char buf[8];
    float asFloat;
    double asDouble;
    int asInt;
    short asShort;
    char asChar;
    unsigned int asUInt;
    unsigned short asUShort;
    unsigned char asUChar;
  } a;
  const int type_sz=flag&0xf;

  if (flag & EEL_GETCHAR_FLAG_FLOAT)
  {
    if (type_sz==8) a.asDouble=val;
    else a.asFloat=(float)val;
  }
  else if (flag & EEL_GETCHAR_FLAG_UNSIGNED)
  {
    if (type_sz==4) a.asUInt=(unsigned int)val;
    else if (type_sz==2) a.asUShort=(unsigned short)val;
    else a.asUChar=(unsigned char)val;
  }
  else if (type_sz==4) a.asInt=(int)val;
  else if (type_sz==2) a.asShort=(short)val;
  else a.asChar=(char)val;

  if (flag & EEL_GETCHAR_FLAG_ENDIANSWAP)
  {
    dest += type_sz;
    int x;
    for(x=0;x<type_sz;x++) *--dest=a.buf[x];
  }
  else
    memcpy(dest,a.buf,type_sz);

}


static EEL_F eel_getchar_do(int flag, const char *src)
{
  union
  {
    char buf[8];
    float asFloat;
    double asDouble;
    int asInt;
    short asShort;
    char asChar;
    unsigned int asUInt;
    unsigned short asUShort;
    unsigned char asUChar;
  } a;
  const int type_sz=flag&0xf;

  memset(&a, 0, sizeof(a));
  if (flag & EEL_GETCHAR_FLAG_ENDIANSWAP)
  {
    src += type_sz;
    int x;
    for(x=0;x<type_sz;x++) a.buf[x]=*--src;
  }
  else
    memcpy(a.buf,src,type_sz);

  if (flag & EEL_GETCHAR_FLAG_FLOAT)
  {
    if (type_sz==8) return a.asDouble;
    return a.asFloat;
  }
  if (flag & EEL_GETCHAR_FLAG_UNSIGNED)
  {
    if (type_sz==4) return a.asUInt;
    if (type_sz==2) return a.asUShort;
    return a.asUChar;
  }
  if (type_sz==4) return a.asInt;
  if (type_sz==2) return a.asShort;
  return a.asChar;
}

static EEL_F NSEEL_CGEN_CALL _eel_strgetchar2(void *opaque, INT_PTR np, EEL_F **parms)
{
  if (opaque && np>=3)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(parms[0][0], &wr);
    if (!fmt)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_getchar: bad specifier passed %f",parms[0][0]);
#endif
    }
    else
    {
      const int wl=(wr?wr->GetLength():(int)strlen(fmt));
      const int flags=eel_getchar_flag((int) parms[2][0]);
      int l = (int) parms[1][0];
      if (parms[1][0] < 0.0) l+=wl;

      if (l>=0 && l <= wl-(flags&0xf)) 
      {
        return eel_getchar_do(flags,fmt+l);
      }
    }
  }
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_strsetchar2(void *opaque, INT_PTR np, EEL_F **parms)
{
  if (opaque && np>=4)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_WRITE(parms[0][0], &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_setchar: bad destination specifier passed %f",parms[0][0]);
#endif
    }
    else
    {
      const int wl=wr->GetLength();

      int l = (int) parms[1][0];
      if (parms[1][0] < 0.0) l+=wl;
      if (l>=0 && l <= wl) 
      {
        const int flags=eel_getchar_flag((int) parms[3][0]);
        if (l==wl) 
        {
          if (wl > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
          {
#ifdef EEL_STRING_DEBUGOUT
            EEL_STRING_DEBUGOUT("str_setchar: will not grow string since it is already %d bytes",wl);
#endif
          }
          else
          {
            char buf[32];
            eel_setchar_do(flags,buf,parms[2][0]);
            wr->AppendRaw(buf,flags&0xf);
          }
        }
        else
          eel_setchar_do(flags,(char*)wr->Get()+l,parms[2][0]);
      }
    }
  }
  return parms[0][0];
}

static EEL_F NSEEL_CGEN_CALL _eel_strsetchar(void *opaque, EEL_F *strOut, EEL_F *idx, EEL_F *val)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_setchar: bad destination specifier passed %f",*strOut);
#endif
    }
    else
    {
      const int wl=wr->GetLength();
      int l = (int) *idx;
      if (*idx < 0.0) l+=wl;
      if (l>=0 && l <= wl) 
      {
        const unsigned char v=((int)*val)&255;
        if (l==wl) 
        {
          if (wl > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
          {
#ifdef EEL_STRING_DEBUGOUT
            EEL_STRING_DEBUGOUT("str_setchar: will not grow string since it is already %d bytes",wl);
#endif
          }
          else
            wr->AppendRaw((const char*)&v,1);
        }
        else
          ((unsigned char *)wr->Get())[l]=v; // allow putting nulls in string, strlen() will still get the full size
      }
    }
  }
  return *strOut;
}

static EEL_F NSEEL_CGEN_CALL _eel_strinsert(void *opaque, EEL_F *strOut, EEL_F *fmt_index, EEL_F *pos)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL, *wr_src=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_insert: bad destination specifier passed %f",*strOut);
#endif
    }
    else
    {
      const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,&wr_src);
      if (fmt)
      {
        EEL_STRING_STORAGECLASS tmp;
        if (wr_src == wr) *(wr_src=&tmp) = *wr; // insert from copy

        // if wr_src, fmt is guaranteed to be wr_src.Get()
        int p = (int)*pos;
        int insert_l = wr_src ? wr_src->GetLength() : (int)strlen(fmt);
        if (p < 0) 
        {
          insert_l += p; // decrease insert_l
          fmt -= p; // advance fmt -- if fmt gets advanced past NULL term, insert_l will be < 0 anyway
          p=0;
        }

        if (insert_l>0)
        {
          if (wr->GetLength() > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
          {
#ifdef EEL_STRING_DEBUGOUT
            EEL_STRING_DEBUGOUT("str_insert: will not grow string since it is already %d bytes",wr->GetLength());
#endif
          }
          else
          {
            if (wr_src) 
            {
              wr->InsertRaw(fmt,p, insert_l); 
            }
            else
            {
              wr->Insert(fmt,p);
            }
          }
        }
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("str_insert: bad source specifier passed %f",*fmt_index);
#endif
      }
    }
  }
  return *strOut;
}

static EEL_F NSEEL_CGEN_CALL _eel_strdelsub(void *opaque, EEL_F *strOut, EEL_F *pos, EEL_F *len)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_delsub: bad destination specifier passed %f",*strOut);
#endif
    }
    else
    {
      int p=(int)*pos;
      int l=(int)*len;
      if (p<0)
      {
        l+=p;
        p=0;
      }
      if (l>0)
        wr->DeleteSub(p,l);
    }
  }
  return *strOut;
}

static EEL_F NSEEL_CGEN_CALL _eel_strsetlen(void *opaque, EEL_F *strOut, EEL_F *newlen)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    EEL_STRING_GET_FOR_WRITE(*strOut, &wr);
    if (!wr)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("str_setlen: bad destination specifier passed %f",*strOut);
#endif
    }
    else
    {
      int l = (int) *newlen;
      if (l < 0) l=0;
      if (l > EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
      {
#ifdef EEL_STRING_DEBUGOUT
         EEL_STRING_DEBUGOUT("str_setlen: clamping requested length of %d to %d",l,EEL_STRING_MAXUSERSTRING_LENGTH_HINT);
#endif
        l=EEL_STRING_MAXUSERSTRING_LENGTH_HINT;
      }
      wr->SetLen(l);

    }
  }
  return *strOut;
}


static EEL_F NSEEL_CGEN_CALL _eel_strlen(void *opaque, EEL_F *fmt_index)
{
  if (opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*fmt_index,&wr);
    if (wr) return (EEL_F) wr->GetLength();
    if (fmt) return (EEL_F) strlen(fmt);
  }
  return 0.0;
}




static EEL_F NSEEL_CGEN_CALL _eel_printf(void *opaque, INT_PTR num_param, EEL_F **parms)
{
  if (num_param>0 && opaque)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *wr_src=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*(parms[0]),&wr_src);
    if (fmt)
    {
      char buf[16384];
      const int len = eel_format_strings(opaque,fmt,wr_src?(fmt+wr_src->GetLength()):NULL,buf,(int)sizeof(buf), (int)num_param-1, parms+1);

      if (len >= 0)
      {
#ifdef EEL_STRING_STDOUT_WRITE
        EEL_STRING_STDOUT_WRITE(buf,len);
#endif
        return 1.0;
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("printf: bad format string %s",fmt);
#endif
      }
    }
    else
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("printf: bad format specifier passed %f",*(parms[0]));
#endif
    }
  }
  return 0.0;
}



static EEL_F NSEEL_CGEN_CALL _eel_match(void *opaque, INT_PTR num_parms, EEL_F **parms)
{
  if (opaque && num_parms >= 2)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *fmt_wr=NULL, *msg_wr=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*(parms[0]),&fmt_wr);
    const char *msg = EEL_STRING_GET_FOR_INDEX(*(parms[1]),&msg_wr);

    if (fmt && msg) return eel_string_match(opaque,fmt,msg,0,0, fmt + (fmt_wr?fmt_wr->GetLength():strlen(fmt)), msg + (msg_wr?msg_wr->GetLength():strlen(msg)),(int)num_parms-2,parms+2) ? 1.0 : 0.0;
  }
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_matchi(void *opaque, INT_PTR num_parms, EEL_F **parms)
{
  if (opaque && num_parms >= 2)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    EEL_STRING_STORAGECLASS *fmt_wr=NULL, *msg_wr=NULL;
    const char *fmt = EEL_STRING_GET_FOR_INDEX(*(parms[0]),&fmt_wr);
    const char *msg = EEL_STRING_GET_FOR_INDEX(*(parms[1]),&msg_wr);

    if (fmt && msg) return eel_string_match(opaque,fmt,msg,0,1, fmt + (fmt_wr?fmt_wr->GetLength():strlen(fmt)), msg + (msg_wr?msg_wr->GetLength():strlen(msg)),(int)num_parms-2,parms+2) ? 1.0 : 0.0;
  }
  return 0.0;
}

void EEL_string_register()
{
  NSEEL_addfunc_retval("strlen",1,NSEEL_PProc_THIS,&_eel_strlen);

  NSEEL_addfunc_retval("strcat",2,NSEEL_PProc_THIS,&_eel_strcat);
  NSEEL_addfunc_retval("strcpy",2,NSEEL_PProc_THIS,&_eel_strcpy);
  NSEEL_addfunc_retval("strcmp",2,NSEEL_PProc_THIS,&_eel_strcmp);
  NSEEL_addfunc_retval("stricmp",2,NSEEL_PProc_THIS,&_eel_stricmp);

  NSEEL_addfunc_retval("strncat",3,NSEEL_PProc_THIS,&_eel_strncat);
  NSEEL_addfunc_retval("strncpy",3,NSEEL_PProc_THIS,&_eel_strncpy);
  NSEEL_addfunc_retval("strncmp",3,NSEEL_PProc_THIS,&_eel_strncmp);
  NSEEL_addfunc_retval("strnicmp",3,NSEEL_PProc_THIS,&_eel_strnicmp);
  NSEEL_addfunc_retval("str_setlen",2,NSEEL_PProc_THIS,&_eel_strsetlen);
  NSEEL_addfunc_exparms("strcpy_from",3,NSEEL_PProc_THIS,&_eel_strcpysubstr); // alias
  NSEEL_addfunc_exparms("strcpy_substr",3,NSEEL_PProc_THIS,&_eel_strcpysubstr); // only allow 3 or 4 parms
  NSEEL_addfunc_exparms("strcpy_substr",4,NSEEL_PProc_THIS,&_eel_strcpysubstr);
  
  NSEEL_addfunc_retval("str_getchar",2,NSEEL_PProc_THIS,&_eel_strgetchar);
  NSEEL_addfunc_retval("str_setchar",3,NSEEL_PProc_THIS,&_eel_strsetchar);
  NSEEL_addfunc_exparms("str_getchar",3,NSEEL_PProc_THIS,&_eel_strgetchar2);
  NSEEL_addfunc_exparms("str_setchar",4,NSEEL_PProc_THIS,&_eel_strsetchar2);

  NSEEL_addfunc_retval("str_insert",3,NSEEL_PProc_THIS,&_eel_strinsert);
  NSEEL_addfunc_retval("str_delsub",3,NSEEL_PProc_THIS,&_eel_strdelsub);

  NSEEL_addfunc_varparm("sprintf",2,NSEEL_PProc_THIS,&_eel_sprintf);
  NSEEL_addfunc_varparm("printf",1,NSEEL_PProc_THIS,&_eel_printf);

  NSEEL_addfunc_varparm("match",2,NSEEL_PProc_THIS,&_eel_match);
  NSEEL_addfunc_varparm("matchi",2,NSEEL_PProc_THIS,&_eel_matchi);

}
void eel_string_initvm(NSEEL_VMCTX vm)
{
  NSEEL_VM_SetStringFunc(vm, eel_string_context_state::addStringCallback, eel_string_context_state::addNamedStringCallback);
}

#ifdef EEL_WANT_DOCUMENTATION
static const char *eel_strings_function_reference =
#ifdef EEL_STRING_STDOUT_WRITE
"printf\t\"format\"[, ...]\tOutput formatted string to system-specific destination, see sprintf() for more information\0"
#endif
"sprintf\t#dest,\"format\"[, ...]\tFormats a string and stores it in #dest. Format specifiers begin with %, and may include:\3\n"
  "\4 %% = %\n"
  "\4 %s = string from parameter\n"
  "\4 %d = parameter as integer\n"
  "\4 %i = parameter as integer\n"
  "\4 %u = parameter as unsigned integer\n"
  "\4 %x = parameter as hex (lowercase) integer\n"
  "\4 %X = parameter as hex (uppercase) integer\n"
  "\4 %c = parameter as character\n"
  "\4 %f = parameter as floating point\n"
  "\4 %e = parameter as floating point (scientific notation, lowercase)\n"
  "\4 %E = parameter as floating point (scientific notation, uppercase)\n"
  "\4 %g = parameter as floating point (shortest representation, lowercase)\n"
  "\4 %G = parameter as floating point (shortest representation, uppercase)\n"
  "\2\n"
  "Many standard C printf() modifiers can be used, including:\3\n"
  "\4 %.10s = string, but only print up to 10 characters\n"
  "\4 %-10s = string, left justified to 10 characters\n"
  "\4 %10s = string, right justified to 10 characters\n"
  "\4 %+f = floating point, always show sign\n"
  "\4 %.4f = floating point, minimum of 4 digits after decimal point\n"
  "\4 %10d = integer, minimum of 10 digits (space padded)\n"
  "\4 %010f = integer, minimum of 10 digits (zero padded)\n"
  "\2\n"
  "Values for format specifiers can be specified as additional parameters to sprintf, or within {} in the format specifier (such as %{varname}d, in that case a global variable is always used).\0"

  "matchi\t\"needle\",\"haystack\"[, ...]\tCase-insensitive version of match().\0"
  "match\t\"needle\",\"haystack\"[, ...]\tSearches for the first parameter in the second parameter, using a simplified regular expression syntax.\3\n"
    "\4* = match 0 or more characters\n"
    "\4*? = match 0 or more characters, lazy\n"
    "\4+ = match 1 or more characters\n"
    "\4+? = match 1 or more characters, lazy\n"
    "\4? = match one character\n"
    "\2\n"
    "You can also use format specifiers to match certain types of data, and optionally put that into a variable:\3\n"
    "\4%s means 1 or more chars\n"
    "\4%0s means 0 or more chars\n"
    "\4%5s means exactly 5 chars\n"
    "\4%5-s means 5 or more chars\n"
    "\4%-10s means 1-10 chars\n"
    "\4%3-5s means 3-5 chars\n"
    "\4%0-5s means 0-5 chars\n"
    "\4%x, %d, %u, and %f are available for use similarly\n"
    "\4%c can be used, but can't take any length modifiers\n"
    "\4Use uppercase (%S, %D, etc) for lazy matching\n"
    "\2\n"
    "See also sprintf() for other notes, including specifying direct variable references via {}.\0"

  "strlen\t\"str\"\tReturns the length of the string passed as a parameter\0"
  "strcpy\t#str,\"srcstr\"\tCopies the contents of srcstr to #str, and returns #str\0"
  "strcat\t#str,\"srcstr\"\tAppends srcstr to #str, and returns #str\0"
  "strcmp\t\"str\",\"str2\"\tCompares strings, returning 0 if equal\0"
  "stricmp\t\"str\",\"str2\"\tCompares strings ignoring case, returning 0 if equal\0"
  "strncmp\t\"str\",\"str2\",maxlen\tCompares strings giving up after maxlen characters, returning 0 if equal\0"
  "strnicmp\t\"str\",\"str2\",maxlen\tCompares strings giving up after maxlen characters, ignoring case, returning 0 if equal\0"
  "strncpy\t#str,\"srcstr\",maxlen\tCopies srcstr to #str, stopping after maxlen characters. Returns #str.\0"
  "strncat\t#str,\"srcstr\",maxlen\tAppends srcstr to #str, stopping after maxlen characters of srcstr. Returns #str.\0"
  "strcpy_from\t#str,\"srcstr\",offset\tCopies srcstr to #str, but starts reading srcstr at offset offset\0"
  "strcpy_substr\t#str,\"srcstr\",offs,ml)\tPHP-style (start at offs, offs<0 means from end, ml for maxlen, ml<0 = reduce length by this amt)\0"
  "str_getchar\t\"str\",offset[,type]\tReturns the data at byte-offset offset of str. If offset is negative, position is relative to end of string."
    "type defaults to signed char, but can be specified to read raw binary data in other formats (note the single quotes, these are single/multi-byte characters):\3\n"
    "\4'c' - signed char\n"
    "\4'cu' - unsigned char\n"
    "\4's' - signed short\n"
    "\4'S' - signed short, big endian\n"
    "\4'su' - unsigned short\n"
    "\4'Su' - unsigned short, big endian\n"
    "\4'i' - signed int\n"
    "\4'I' - signed int, big endian\n"
    "\4'iu' - unsigned int\n"
    "\4'Iu' - unsigned int, big endian\n"
    "\4'f' - float\n"
    "\4'F' - float, big endian\n"
    "\4'd' - double\n"
    "\4'D' - double, big endian\n"
    "\2\0"
  "str_setchar\t#str,offset,val[,type])\tSets value at offset offset, type optional. offset may be negative to refer to offset relative to end of string, or between 0 and length, inclusive, and if set to length it will lengthen string. See str_getchar() for more information on types.\0"
   "str_setlen\t#str,len\tSets length of #str (if increasing, will be space-padded), and returns #str.\0"
   "str_delsub\t#str,pos,len\tDeletes len characters at offset pos from #str, and returns #str.\0"
   "str_insert\t#str,\"srcstr\",pos\tInserts srcstr into #str at offset pos. Returns #str\0"
;
#endif


#endif

