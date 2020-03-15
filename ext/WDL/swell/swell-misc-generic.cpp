/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux/OSX)
   Copyright (C) 2006 and later, Cockos, Inc.

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
  
#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-internal.h"

#ifndef __APPLE__
#include <sys/types.h>
#include <sys/wait.h>
#endif

bool IsRightClickEmulateEnabled()
{
  return false;
}

void SWELL_EnableRightClickEmulate(BOOL enable)
{
}


HANDLE SWELL_CreateProcessFromPID(int pid)
{
  SWELL_InternalObjectHeader_PID *buf = (SWELL_InternalObjectHeader_PID*)malloc(sizeof(SWELL_InternalObjectHeader_PID));
  buf->hdr.type = INTERNAL_OBJECT_PID;
  buf->hdr.count = 1;
  buf->pid = (int) pid;
  buf->done = buf->result = 0;
  return (HANDLE) buf;
}

HANDLE SWELL_CreateProcess(const char *exe, int nparams, const char **params)
{
  void swell_cleanupZombies();
  swell_cleanupZombies();

  const pid_t pid = fork();
  if (pid == 0)
  {
    char **pp = (char **)calloc(nparams+2,sizeof(char*));
    pp[0] = strdup(exe);
    for (int x=0;x<nparams;x++) pp[x+1] = strdup(params[x]?params[x]:"");
    execvp(exe,pp);
    exit(0);
  }
  if (pid < 0) return NULL;

  return SWELL_CreateProcessFromPID(pid);
}

int SWELL_GetProcessExitCode(HANDLE hand)
{
  SWELL_InternalObjectHeader_PID *hdr=(SWELL_InternalObjectHeader_PID*)hand;
  if (!hdr || hdr->hdr.type != INTERNAL_OBJECT_PID|| !hdr->pid) return -1;
  if (hdr->done) return hdr->result;

  int wstatus=0;
  pid_t v = waitpid((pid_t)hdr->pid,&wstatus,WNOHANG);
  if (v <= 0) return -2;
  hdr->done = 1;
  return hdr->result = WEXITSTATUS(wstatus);
}



#endif
