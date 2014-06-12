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

pid_t
__archive_create_child(const char *cmd, int *child_stdin, int *child_stdout)
{
	HANDLE childStdout[2], childStdin[2],childStderr;
	SECURITY_ATTRIBUTES secAtts;
	STARTUPINFO staInfo;
	PROCESS_INFORMATION childInfo;
	struct archive_string cmdline;
	struct archive_string fullpath;
	struct archive_cmdline *acmd;
	char *arg0, *ext;
	int i, l;
	DWORD fl, fl_old;

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
	WaitForInputIdle(childInfo.hProcess, INFINITE);
	CloseHandle(childInfo.hProcess);
	CloseHandle(childInfo.hThread);

	*child_stdout = _open_osfhandle((intptr_t)childStdout[0], _O_RDONLY);
	*child_stdin = _open_osfhandle((intptr_t)childStdin[1], _O_WRONLY);
	
	CloseHandle(childStdout[1]);
	CloseHandle(childStdin[0]);

	archive_string_free(&cmdline);
	archive_string_free(&fullpath);
	__archive_cmdline_free(acmd);
	return (childInfo.dwProcessId);

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
	return (-1);
}

void
__archive_check_child(int in, int out)
{
	(void)in; /* UNUSED */
	(void)out; /* UNUSED */
	Sleep(100);
}

#endif /* _WIN32 && !__CYGWIN__ */
