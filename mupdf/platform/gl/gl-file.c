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

#include "gl-app.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#define ICON_PC 0x1f4bb
#define ICON_HOME 0x1f3e0
#define ICON_FOLDER 0x1f4c1
#define ICON_DOCUMENT 0x1f4c4
#define ICON_DISK 0x1f4be
#define ICON_PIN 0x1f4cc

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct entry
{
	int is_dir;
	char name[FILENAME_MAX];
};

static struct
{
	int (*filter)(const char *fn);
	struct input input_dir;
	struct input input_file;
	struct list list_dir;
	char original_file_name[PATH_MAX];
	char curdir[PATH_MAX];
	int count;
	int max;
	struct entry *files;
	int selected;
	int confirm;
} fc;

static int cmp_entry(const void *av, const void *bv)
{
	const struct entry *a = av;
	const struct entry *b = bv;
	/* "." first */
	if (a->name[0] == '.' && a->name[1] == 0) return -1;
	if (b->name[0] == '.' && b->name[1] == 0) return 1;
	/* ".." second */
	if (a->name[0] == '.' && a->name[1] == '.' && a->name[2] == 0) return -1;
	if (b->name[0] == '.' && b->name[1] == '.' && b->name[2] == 0) return 1;
	/* directories before files */
	if (a->is_dir && !b->is_dir) return -1;
	if (b->is_dir && !a->is_dir) return 1;
	/* then alphabetically */
	return strcmp(a->name, b->name);
}

static void
ensure_one_more_file(void)
{
	if (fc.count == fc.max)
	{
		int new_max = fc.max == 0 ? 512 : fc.max*2;
		fc.files = fz_realloc_array(ctx, fc.files, new_max, struct entry);
		fc.max = new_max;
	}
}

#ifdef _WIN32

#include <strsafe.h>
#include <shlobj.h>

static void load_dir(const char *path)
{
	WIN32_FIND_DATAW ffd;
	HANDLE dir;
	wchar_t wpath[PATH_MAX];
	char buf[PATH_MAX];
	int i;

	fz_realpath(path, fc.curdir);
	if (!fz_is_directory(ctx, path))
		return;

	ui_input_init(&fc.input_dir, fc.curdir);

	fc.selected = -1;
	fc.count = 0;

	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_MAX);
	for (i=0; wpath[i]; ++i)
		if (wpath[i] == '/')
			wpath[i] = '\\';
	StringCchCatW(wpath, PATH_MAX, L"/*");
	dir = FindFirstFileW(wpath, &ffd);
	if (dir)
	{
		do
		{
			WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, buf, PATH_MAX, NULL, NULL);
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
				continue;
			ensure_one_more_file();
			fc.files[fc.count].is_dir = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			if (fc.files[fc.count].is_dir || !fc.filter || fc.filter(buf))
			{
				fz_strlcpy(fc.files[fc.count].name, buf, FILENAME_MAX);
				++fc.count;
			}
		}
		while (FindNextFileW(dir, &ffd));
		FindClose(dir);
	}

	qsort(fc.files, fc.count, sizeof fc.files[0], cmp_entry);
}

static void list_drives(void)
{
	static struct list drive_list;
	DWORD drives;
	char dir[PATH_MAX], vis[PATH_MAX], buf[100];
	const char *user, *home;
	char personal[MAX_PATH], desktop[MAX_PATH];
	int i, n;

	drives = GetLogicalDrives();
	n = 5; /* curdir + home + desktop + documents + downloads */
	for (i=0; i < 26; ++i)
		if (drives & (1<<i))
			++n;

	ui_list_begin(&drive_list, n, 0, 10 * ui.lineheight + 4);

	user = getenv("USERNAME");
	home = getenv("USERPROFILE");
	if (user && home)
	{
		fz_snprintf(vis, sizeof vis, "%C %s", ICON_HOME, user);
		if (ui_list_item(&drive_list, "~", vis, 0))
			load_dir(home);
	}

	if (SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) == S_OK)
	{
		fz_snprintf(vis, sizeof vis, "%C Desktop", ICON_PC);
		if (ui_list_item(&drive_list, "~/Desktop", vis, 0))
			load_dir(desktop);
	}

	if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, personal) == S_OK)
	{
		fz_snprintf(vis, sizeof vis, "%C Documents", ICON_FOLDER);
		if (ui_list_item(&drive_list, "~/Documents", vis, 0))
			load_dir(personal);
	}

	if (home)
	{
		fz_snprintf(vis, sizeof vis, "%C Downloads", ICON_FOLDER);
		fz_snprintf(dir, sizeof dir, "%s/Downloads", home);
		if (ui_list_item(&drive_list, "~/Downloads", vis, 0))
			load_dir(dir);
	}

	for (i = 0; i < 26; ++i)
	{
		if (drives & (1<<i))
		{
			fz_snprintf(dir, sizeof dir, "%c:\\", i+'A');
			if (!GetVolumeInformationA(dir, buf, sizeof buf, NULL, NULL, NULL, NULL, 0))
				buf[0] = 0;
			fz_snprintf(vis, sizeof vis, "%C %c: %s", ICON_DISK, i+'A', buf);
			if (ui_list_item(&drive_list, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"+i, vis, 0))
			{
				load_dir(dir);
			}
		}
	}

	fz_snprintf(vis, sizeof vis, "%C .", ICON_PIN);
	if (ui_list_item(&drive_list, ".", vis, 0))
		load_dir(".");

	ui_list_end(&drive_list);
}

#else

#include <dirent.h>

static void load_dir(const char *path)
{
	char buf[PATH_MAX];
	DIR *dir;
	struct dirent *dp;

	fz_realpath(path, fc.curdir);
	if (!fz_is_directory(ctx, fc.curdir))
		return;

	ui_input_init(&fc.input_dir, fc.curdir);

	fc.selected = -1;
	fc.count = 0;

	dir = opendir(fc.curdir);
	if (!dir)
	{
		ensure_one_more_file();
		fc.files[fc.count].is_dir = 1;
		fz_strlcpy(fc.files[fc.count].name, "..", FILENAME_MAX);
		++fc.count;
	}
	else
	{
		while ((dp = readdir(dir)))
		{
			/* skip hidden files */
			if (dp->d_name[0] == '.' && strcmp(dp->d_name, ".") && strcmp(dp->d_name, ".."))
				continue;
			fz_snprintf(buf, sizeof buf, "%s/%s", fc.curdir, dp->d_name);
			ensure_one_more_file();
			fc.files[fc.count].is_dir = fz_is_directory(ctx, buf);
			if (fc.files[fc.count].is_dir || !fc.filter || fc.filter(buf))
			{
				fz_strlcpy(fc.files[fc.count].name, dp->d_name, FILENAME_MAX);
				++fc.count;
			}
		}
		closedir(dir);
	}

	qsort(fc.files, fc.count, sizeof fc.files[0], cmp_entry);
}

static const struct {
	int icon;
	const char *name;
} common_dirs[] = {
	{ ICON_HOME, "~" },
	{ ICON_PC, "~/Desktop" },
	{ ICON_FOLDER, "~/Documents" },
	{ ICON_FOLDER, "~/Downloads" },
	{ ICON_FOLDER, "/" },
	{ ICON_DISK, "/Volumes" },
	{ ICON_DISK, "/media" },
	{ ICON_DISK, "/mnt" },
	{ ICON_PIN, "." },
};

static int has_dir(const char *home, const char *user, int i, char dir[PATH_MAX], char vis[PATH_MAX])
{
	const char *subdir = common_dirs[i].name;
	int icon = common_dirs[i].icon;
	if (subdir[0] == '~')
	{
		if (!home)
			return 0;
		if (subdir[1] == '/')
		{
			fz_snprintf(dir, PATH_MAX, "%s/%s", home, subdir+2);
			fz_snprintf(vis, PATH_MAX, "%C %s", icon, subdir+2);
		}
		else
		{
			fz_snprintf(dir, PATH_MAX, "%s", home);
			fz_snprintf(vis, PATH_MAX, "%C %s", icon, user ? user : "~");
		}
	}
	else
	{
		fz_strlcpy(dir, subdir, PATH_MAX);
		fz_snprintf(vis, PATH_MAX, "%C %s", icon, subdir);
	}
	return fz_is_directory(ctx, dir);
}

static void list_drives(void)
{
	static struct list drive_list;
	char dir[PATH_MAX], vis[PATH_MAX];
	const char *home = getenv("HOME");
	const char *user = getenv("USER");
	int i;

	ui_list_begin(&drive_list, nelem(common_dirs), 0, nelem(common_dirs) * ui.lineheight + 4);

	for (i = 0; i < (int)nelem(common_dirs); ++i)
		if (has_dir(home, user, i, dir, vis))
			if (ui_list_item(&drive_list, common_dirs[i].name, vis, 0))
				load_dir(dir);

	ui_list_end(&drive_list);
}

#endif

void ui_init_open_file(const char *dir, int (*filter)(const char *fn))
{
	fc.filter = filter;
	load_dir(dir);
}

int ui_open_file(char *filename, const char *label)
{
	static int last_click_time = 0;
	static int last_click_sel = -1;
	int i, rv = 0;

	ui_panel_begin(0, 0, ui.padsize*2, ui.padsize*2, 1);
	{
		if (label)
		{
			ui_layout(T, X, NW, ui.padsize*2, ui.padsize);
			ui_label(label);
		}
		ui_layout(L, Y, NW, 0, 0);
		ui_panel_begin(ui.gridsize*6, 0, 0, 0, 0);
		{
			ui_layout(T, X, NW, ui.padsize, ui.padsize);
			list_drives();
			ui_layout(B, X, NW, ui.padsize, ui.padsize);
			if (ui_button("Cancel") || (!ui.focus && ui.key == KEY_ESCAPE))
			{
				filename[0] = 0;
				rv = 1;
			}
		}
		ui_panel_end();

		ui_layout(T, X, NW, ui.padsize, ui.padsize);
		ui_panel_begin(0, ui.gridsize, 0, 0, 0);
		{
			int disabled = (fc.selected < 0);
			ui_layout(R, NONE, CENTER, 0, 0);
			if (ui_button_aux("Open", disabled) || (!disabled && !ui.focus && ui.key == KEY_ENTER))
			{
				fz_snprintf(filename, PATH_MAX, "%s/%s", fc.curdir, fc.files[fc.selected].name);
				rv = 1;
			}
			ui_spacer();
			ui_layout(ALL, X, CENTER, 0, 0);
			if (ui_input(&fc.input_dir, 0, 1) == UI_INPUT_ACCEPT)
				load_dir(fc.input_dir.text);
		}
		ui_panel_end();

		ui_layout(ALL, BOTH, NW, ui.padsize, ui.padsize);
		ui_list_begin(&fc.list_dir, fc.count, 0, 0);
		for (i = 0; i < fc.count; ++i)
		{
			const char *name = fc.files[i].name;
			char buf[PATH_MAX];
			if (fc.files[i].is_dir)
				fz_snprintf(buf, sizeof buf, "%C %s", ICON_FOLDER, name);
			else
				fz_snprintf(buf, sizeof buf, "%C %s", ICON_DOCUMENT, name);
			if (ui_list_item(&fc.list_dir, &fc.files[i], buf, i==fc.selected))
			{
				fc.selected = i;
				if (fc.files[i].is_dir)
				{
					fz_snprintf(buf, sizeof buf, "%s/%s", fc.curdir, name);
					load_dir(buf);
					ui.active = NULL;
					last_click_sel = -1;
				}
				else
				{
					int click_time = glutGet(GLUT_ELAPSED_TIME);
					if (i == last_click_sel && click_time < last_click_time + 250)
					{
						fz_snprintf(filename, PATH_MAX, "%s/%s", fc.curdir, name);
						rv = 1;
					}
					last_click_time = click_time;
					last_click_sel = i;
				}
			}
		}
		ui_list_end(&fc.list_dir);
	}
	ui_panel_end();

	return rv;
}

void ui_init_save_file(const char *path, int (*filter)(const char *fn))
{
	char dir[PATH_MAX], *p;
	fc.filter = filter;
	fz_strlcpy(dir, path, sizeof dir);
	for (p=dir; *p; ++p)
		if (*p == '\\') *p = '/';
	fz_cleanname(dir);
	p = strrchr(dir, '/');
	if (p)
	{
		*p = 0;
		load_dir(dir);
		ui_input_init(&fc.input_file, p+1);
	}
	else
	{
		load_dir(".");
		ui_input_init(&fc.input_file, dir);
	}
	fz_snprintf(fc.original_file_name, PATH_MAX, "%s/%s", fc.curdir, fc.input_file.text);
	fc.confirm = 0;
}

static void bump_file_version(int dir)
{
	char buf[PATH_MAX], *p, *n;
	char base[PATH_MAX], out[PATH_MAX];
	int x;
	fz_strlcpy(buf, fc.input_file.text, sizeof buf);
	p = strrchr(buf, '.');
	if (p)
	{
		n = p;
		while (n > buf && n[-1] >= '0' && n[-1] <= '9')
			--n;
		if (n != p)
			x = atoi(n) + dir;
		else
			x = dir;
		memcpy(base, buf, n-buf);
		base[n-buf] = 0;
		fz_snprintf(out, sizeof out, "%s%d%s", base, x, p);
		ui_input_init(&fc.input_file, out);
	}
}

static int ui_save_file_confirm(char *filename)
{
	int rv = 0;
	ui_dialog_begin(ui.gridsize*20, (ui.gridsize+7)*3);
	ui_layout(T, NONE, NW, ui.padsize, ui.padsize);
	ui_label("%C File %s already exists!", 0x26a0, filename); /* WARNING SIGN */
	ui_label("Do you want to replace it?");
	ui_layout(B, X, S, ui.padsize, ui.padsize);
	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	{
		ui_layout(R, NONE, S, 0, 0);
		if (ui_button("Replace"))
			rv = 1;
		ui_spacer();
		ui_layout(L, NONE, S, 0, 0);
		if (ui_button("Cancel") || ui.key == KEY_ESCAPE)
			fc.confirm = 0;
	}
	ui_panel_end();
	ui_dialog_end();
	return rv;
}

int ui_save_file(char *filename, void (*extra_panel)(void), const char *label)
{
	int i, rv = 0;

	if (fc.confirm)
	{
		return ui_save_file_confirm(filename);
	}

	ui_panel_begin(0, 0, ui.padsize*2, ui.padsize*2, 1);
	{
		if (label)
		{
			ui_layout(T, X, NW, ui.padsize*2, ui.padsize);
			ui_label(label);
		}
		ui_layout(L, Y, NW, 0, 0);
		ui_panel_begin(ui.gridsize*6, 0, 0, 0, 0);
		{
			ui_layout(T, X, NW, ui.padsize, ui.padsize);
			list_drives();
			if (extra_panel)
			{
				ui_spacer();
				extra_panel();
			}
			ui_layout(B, X, NW, ui.padsize, ui.padsize);
			if (ui_button("Cancel") || (!ui.focus && ui.key == KEY_ESCAPE))
			{
				filename[0] = 0;
				rv = 1;
			}
		}
		ui_panel_end();

		ui_layout(T, X, NW, ui.padsize, ui.padsize);
		if (ui_input(&fc.input_dir, 0, 1) == UI_INPUT_ACCEPT)
			load_dir(fc.input_dir.text);

		ui_layout(T, X, NW, ui.padsize, ui.padsize);
		ui_panel_begin(0, ui.gridsize, 0, 0, 0);
		{
			ui_layout(R, NONE, CENTER, 0, 0);
			if (ui_button("Save"))
			{
				fz_snprintf(filename, PATH_MAX, "%s/%s", fc.curdir, fc.input_file.text);
				rv = 1;

				/* Show confirmation dialog if we would overwrite another file. */
				if (strcmp(filename, fc.original_file_name))
				{
					if (fz_file_exists(ctx, filename))
					{
						fc.confirm = 1;
						rv = 0;
					}
				}
			}
			ui_spacer();
			if (ui_button("\xe2\x9e\x95")) /* U+2795 HEAVY PLUS */
				bump_file_version(1);
			if (ui_button("\xe2\x9e\x96")) /* U+2796 HEAVY MINUS */
				bump_file_version(-1);
			ui_spacer();
			ui_layout(ALL, X, CENTER, 0, 0);
			ui_input(&fc.input_file, 0, 1);
		}
		ui_panel_end();

		ui_layout(ALL, BOTH, NW, ui.padsize, ui.padsize);
		ui_list_begin(&fc.list_dir, fc.count, 0, 0);
		for (i = 0; i < fc.count; ++i)
		{
			const char *name = fc.files[i].name;
			char buf[PATH_MAX];
			if (fc.files[i].is_dir)
				fz_snprintf(buf, sizeof buf, "%C %s", ICON_FOLDER, name);
			else
				fz_snprintf(buf, sizeof buf, "%C %s", ICON_DOCUMENT, name);
			if (ui_list_item(&fc.list_dir, &fc.files[i], buf, i==fc.selected))
			{
				fc.selected = i;
				if (fc.files[i].is_dir)
				{
					fz_snprintf(buf, sizeof buf, "%s/%s", fc.curdir, name);
					load_dir(buf);
					ui.active = NULL;
				}
				else
				{
					ui_input_init(&fc.input_file, name);
				}
			}
		}
		ui_list_end(&fc.list_dir);
	}
	ui_panel_end();

	return rv;
}
