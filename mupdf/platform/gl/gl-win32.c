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
