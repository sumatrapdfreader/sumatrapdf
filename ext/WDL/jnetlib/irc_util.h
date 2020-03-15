#ifndef _WDL_JNL_IRC_UTIL_H_
#define _WDL_JNL_IRC_UTIL_H_

#include "netinc.h"

static void FormatIRCMessage(char *bufout, const char *fmt, ...) // bufout should be 1024 bytes to be safe
{
  va_list arglist;
	va_start(arglist, fmt);
  #ifdef _WIN32
	int written = _vsnprintf(bufout, 1024-16, fmt, arglist);
  #else
	int written = vsnprintf(bufout, 1024-16, fmt, arglist);
  #endif
  if (written < 0) written = 0;
  else if (written > 510) written=510;
  bufout[written]=0;
	va_end(arglist);

  strcat(bufout,"\r\n");
}


static void ParseIRCMessage(char *buf, char **prefix, char *tokens[16], int *tokensvalid, bool *lastHadColon) // destroys buf
{
  if (lastHadColon) *lastHadColon=false;
  *tokensvalid=0;
  if (prefix) *prefix=NULL;
  if (*buf==':')
  {
    if (prefix) *prefix=buf;
    while (*buf && *buf != ' ') buf++;
    if (*buf==' ')
    {
      *buf++=0;
      while (*buf== ' ') buf++;
    }
  }

  while (*buf && *tokensvalid < 16)
  {
    tokens[(*tokensvalid)++] = buf[0] == ':' ? buf+1 : buf;
    if (buf[0] == ':' || *tokensvalid == 16) 
    {
      if (buf[0] == ':' && lastHadColon) *lastHadColon=true;
      break;
    }

    // skip over parameter
    while (*buf && *buf != ' ') buf++;
    if (*buf == ' ')
    {
      *buf++=0;
      while (*buf== ' ') buf++;
    }

  }
}
#endif