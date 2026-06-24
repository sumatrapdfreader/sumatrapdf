/*-
 * Copyright (c) 2009-2012 Michihiro NAKAJIMA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include "archive_cmdline_private.h"
#include "archive_string.h"

#include "filter_fork.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
/* There are some editions of Windows ("nano server," for example) that
 * do not host user32.dll. If we want to keep running on those editions,
 * we need to delay-load WaitForInputIdle. */

static int
failing_wait(HANDLE hProcess, DWORD dwMilliseconds) {
	/* An inability to wait for input idle is
	 * not _good_, but it is not catastrophic. */
	(void)hProcess; /* UNUSED */
	(void)dwMilliseconds; /* UNUSED */
	return WAIT_FAILED;
}

# if _WIN32_WINNT < _WIN32_WINNT_VISTA
static int
la_WaitForInputIdle(HANDLE hProcess, DWORD dwMilliseconds)
{
	static DWORD (WINAPI * volatile f)(HANDLE, DWORD);

	if (f == NULL) {
		HINSTANCE lib;
		void *old;
		DWORD (WINAPI *tmp)(HANDLE, DWORD);

		lib = LoadLibrary(TEXT("user32.dll"));
		tmp = (lib != NULL) ?
		    (PVOID)GetProcAddress(lib, "WaitForInputIdle") :
		    failing_wait;
		old = InterlockedCompareExchangePointer((volatile PVOID *)&f,
		    tmp, NULL);
		if (old != NULL && lib != NULL)
			FreeLibrary(lib);
	}

	return (*f)(hProcess, dwMilliseconds);
}
# else
static BOOL CALLBACK
load_WaitForInputIdle(PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context) {
	HMODULE user32 = LoadLibrary(TEXT("user32.dll"));

	(void)InitOnce; /* UNUSED */
	(void)Parameter; /* UNUSED */

	*Context = (user32 != NULL) ?
	    (PVOID)GetProcAddress(user32, "WaitForInputIdle") : failing_wait;

	return TRUE;
}

static int
la_WaitForInputIdle(HANDLE hProcess, DWORD dwMilliseconds)
{
	static DWORD (WINAPI *f)(HANDLE, DWORD);
	static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

	InitOnceExecuteOnce(&once, load_WaitForInputIdle, NULL, (PVOID)&f);

	return (*f)(hProcess, dwMilliseconds);
}
# endif

int
__archive_create_child(const char *cmd, int *child_stdin, int *child_stdout,
		HANDLE *out_child)
{
	HANDLE childStdout[2], childStdin[2],childStderr;
	SECURITY_ATTRIBUTES secAtts;
	STARTUPINFOA staInfo;
	PROCESS_INFORMATION childInfo;
	struct archive_string cmdline;
	struct archive_string fullpath;
	struct archive_cmdline *acmd;
	char *arg0, *ext;
	int i, l;
	DWORD fl, fl_old;
	HANDLE child;

	childStdout[0] = childStdout[1] = INVALID_HANDLE_VALUE;
	childStdin[0] = childStdin[1] = INVALID_HANDLE_VALUE;
	childStderr = INVALID_HANDLE_VALUE;
	archive_string_init(&cmdline);
	archive_string_init(&fullpath);

	acmd = __archive_cmdline_allocate();
	if (acmd == NULL)
		goto fail;
	if (__archive_cmdline_parse(acmd, cmd) != ARCHIVE_OK)
		goto fail;

	/*
	 * Search the full path of 'path'.
	 * NOTE: This does not need if we give CreateProcessA 'path' as
	 * a part of the cmdline and give CreateProcessA NULL as first
	 * parameter, but I do not like that way.
	 */
	ext = strrchr(acmd->path, '.');
	if (ext == NULL || strlen(ext) > 4)
		/* 'path' does not have a proper extension, so we have to
		 * give SearchPath() ".exe" as the extension. */
		ext = ".exe";
	else
		ext = NULL;/* 'path' has an extension. */

	fl = MAX_PATH;
	do {
		if (archive_string_ensure(&fullpath, fl) == NULL)
			goto fail;
		fl_old = fl;
		fl = SearchPathA(NULL, acmd->path, ext, fl, fullpath.s,
			&arg0);
	} while (fl != 0 && fl > fl_old);
	if (fl == 0)
		goto fail;

	/*
	 * Make a command line.
	 */
	for (l = 0, i = 0;  acmd->argv[i] != NULL; i++) {
		if (i == 0)
			continue;
		l += (int)strlen(acmd->argv[i]) + 1;
	}
	if (archive_string_ensure(&cmdline, l + 1) == NULL)
		goto fail;
	for (i = 0;  acmd->argv[i] != NULL; i++) {
		if (i == 0) {
			const char *p, *sp;

			if ((p = strchr(acmd->argv[i], '/')) != NULL ||
			    (p = strchr(acmd->argv[i], '\\')) != NULL)
				p++;
			else
				p = acmd->argv[i];
			if ((sp = strchr(p, ' ')) != NULL)
				archive_strappend_char(&cmdline, '"');
			archive_strcat(&cmdline, p);
			if (sp != NULL)
				archive_strappend_char(&cmdline, '"');
		} else {
			archive_strappend_char(&cmdline, ' ');
			archive_strcat(&cmdline, acmd->argv[i]);
		}
	}
	if (i <= 1) {
		const char *sp;

		if ((sp = strchr(arg0, ' ')) != NULL)
			archive_strappend_char(&cmdline, '"');
		archive_strcat(&cmdline, arg0);
		if (sp != NULL)
			archive_strappend_char(&cmdline, '"');
	}

	secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAtts.bInheritHandle = TRUE;
	secAtts.lpSecurityDescriptor = NULL;
	if (CreatePipe(&childStdout[0], &childStdout[1], &secAtts, 0) == 0)
		goto fail;
	if (!SetHandleInformation(childStdout[0], HANDLE_FLAG_INHERIT, 0))
		goto fail;
	if (CreatePipe(&childStdin[0], &childStdin[1], &secAtts, 0) == 0)
		goto fail;
	if (!SetHandleInformation(childStdin[1], HANDLE_FLAG_INHERIT, 0))
		goto fail;
	if (DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE),
	    GetCurrentProcess(), &childStderr, 0, TRUE,
	    DUPLICATE_SAME_ACCESS) == 0)
		goto fail;

	memset(&staInfo, 0, sizeof(staInfo));
	staInfo.cb = sizeof(staInfo);
	staInfo.hStdError = childStderr;
	staInfo.hStdOutput = childStdout[1];
	staInfo.hStdInput = childStdin[0];
	staInfo.wShowWindow = SW_HIDE;
	staInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	if (CreateProcessA(fullpath.s, cmdline.s, NULL, NULL, TRUE, 0,
	      NULL, NULL, &staInfo, &childInfo) == 0)
		goto fail;
	la_WaitForInputIdle(childInfo.hProcess, INFINITE);
	CloseHandle(childInfo.hProcess);
	CloseHandle(childInfo.hThread);

	*child_stdout = _open_osfhandle((intptr_t)childStdout[0], _O_RDONLY);
	*child_stdin = _open_osfhandle((intptr_t)childStdin[1], _O_WRONLY);
	
	child = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE,
		childInfo.dwProcessId);
	if (child == NULL) // INVALID_HANDLE_VALUE ?
		goto fail;

	*out_child = child;

	CloseHandle(childStdout[1]);
	CloseHandle(childStdin[0]);

	archive_string_free(&cmdline);
	archive_string_free(&fullpath);
	__archive_cmdline_free(acmd);
	return ARCHIVE_OK;

fail:
	if (childStdout[0] != INVALID_HANDLE_VALUE)
		CloseHandle(childStdout[0]);
	if (childStdout[1] != INVALID_HANDLE_VALUE)
		CloseHandle(childStdout[1]);
	if (childStdin[0] != INVALID_HANDLE_VALUE)
		CloseHandle(childStdin[0]);
	if (childStdin[1] != INVALID_HANDLE_VALUE)
		CloseHandle(childStdin[1]);
	if (childStderr != INVALID_HANDLE_VALUE)
		CloseHandle(childStderr);
	archive_string_free(&cmdline);
	archive_string_free(&fullpath);
	__archive_cmdline_free(acmd);
	return ARCHIVE_FAILED;
}
#else /* !WINAPI_PARTITION_DESKTOP */
int
__archive_create_child(const char *cmd, int *child_stdin, int *child_stdout, HANDLE *out_child)
{
	(void)cmd; (void)child_stdin; (void) child_stdout; (void) out_child;
	return ARCHIVE_FAILED;
}
#endif /* !WINAPI_PARTITION_DESKTOP */

void
__archive_check_child(int in, int out)
{
	(void)in; /* UNUSED */
	(void)out; /* UNUSED */
	Sleep(100);
}

#endif /* _WIN32 && !__CYGWIN__ */
