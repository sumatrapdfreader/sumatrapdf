// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>

#define OPEN_KEY(parent, name, ptr) \
	RegCreateKeyExA(parent, name, 0, 0, 0, KEY_WRITE, 0, ptr, 0)
#define SET_VALUE(parent, name, value) \
	RegSetValueExA(parent, name, 0, REG_SZ, (const BYTE *)(value), (DWORD)strlen(value) + 1)

void win_install(void)
{
	char command_str[2048], argv0[2048];
	HKEY software, classes, mupdf;
	HKEY supported_types, shell, open, command;
	HKEY dotpdf, dotxps, dotcbz, dotepub, dotfb2;
	HKEY pdf_progids, xps_progids, cbz_progids, epub_progids, fb2_progids;

	GetModuleFileNameA(NULL, argv0, sizeof argv0);
	_snprintf(command_str, sizeof command_str, "\"%s\" \"%%1\"", argv0);

	OPEN_KEY(HKEY_CURRENT_USER, "Software", &software);
	OPEN_KEY(software, "Classes", &classes);
	{
		OPEN_KEY(classes, "MuPDF", &mupdf);
		{
			OPEN_KEY(mupdf, "SupportedTypes", &supported_types);
			{
				SET_VALUE(supported_types, ".pdf", "");
				SET_VALUE(supported_types, ".xps", "");
				SET_VALUE(supported_types, ".cbz", "");
				SET_VALUE(supported_types, ".epub", "");
				SET_VALUE(supported_types, ".fb2", "");
			}
			RegCloseKey(supported_types);
			OPEN_KEY(mupdf, "shell", &shell);
			OPEN_KEY(shell, "open", &open);
			OPEN_KEY(open, "command", &command);
			{
				SET_VALUE(open, "FriendlyAppName", "MuPDF");
				SET_VALUE(command, "", command_str);
			}
			RegCloseKey(command);
			RegCloseKey(open);
			RegCloseKey(shell);
		}
		RegCloseKey(mupdf);

		OPEN_KEY(classes, ".pdf", &dotpdf);
		OPEN_KEY(classes, ".xps", &dotxps);
		OPEN_KEY(classes, ".cbz", &dotcbz);
		OPEN_KEY(classes, ".epub", &dotepub);
		OPEN_KEY(classes, ".fb2", &dotfb2);
		{
			OPEN_KEY(dotpdf, "OpenWithProgids", &pdf_progids);
			OPEN_KEY(dotxps, "OpenWithProgids", &xps_progids);
			OPEN_KEY(dotcbz, "OpenWithProgids", &cbz_progids);
			OPEN_KEY(dotepub, "OpenWithProgids", &epub_progids);
			OPEN_KEY(dotfb2, "OpenWithProgids", &fb2_progids);
			{
				SET_VALUE(pdf_progids, "MuPDF", "");
				SET_VALUE(xps_progids, "MuPDF", "");
				SET_VALUE(cbz_progids, "MuPDF", "");
				SET_VALUE(epub_progids, "MuPDF", "");
				SET_VALUE(fb2_progids, "MuPDF", "");
			}
			RegCloseKey(pdf_progids);
			RegCloseKey(xps_progids);
			RegCloseKey(cbz_progids);
			RegCloseKey(epub_progids);
			RegCloseKey(fb2_progids);
		}
		RegCloseKey(dotpdf);
		RegCloseKey(dotxps);
		RegCloseKey(dotcbz);
		RegCloseKey(dotepub);
		RegCloseKey(dotfb2);
	}
	RegCloseKey(classes);
	RegCloseKey(software);
}

#endif
