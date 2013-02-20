#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"
#include "mucbz.h"
#include "pdfapp.h"
#include <ctype.h>

/*
	A useful bit of bash script to call this is:
	for f in ../ghostpcl/tests_private/pdf/forms/v1.3/ *.pdf ; do g=${f%.*} ; echo $g ; win32/debug/mujstest-v8.exe -o $g-%d.png -p ../ghostpcl/ $g.mjs > $g.log 2>&1 ; done

	Remove the space from "/ *.pdf" before running - can't leave that
	in here, as it causes a warning about a possibly malformed comment.
*/

static pdfapp_t gapp;
static int file_open = 0;
static char filename[1024] = "";
static char *scriptname;
static char *output = NULL;
static char *prefix = NULL;
static int shotcount = 0;
static int verbosity = 0;

#define LONGLINE 4096

static char getline_buffer[LONGLINE];

void winwarn(pdfapp_t *app, char *msg)
{
	fprintf(stderr, "warning: %s\n", msg);
}

void winerror(pdfapp_t *app, char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void winalert(pdfapp_t *app, fz_alert_event *alert)
{
	fprintf(stderr, "Alert %s: %s", alert->title, alert->message);
	switch (alert->button_group_type)
	{
	case FZ_ALERT_BUTTON_GROUP_OK:
	case FZ_ALERT_BUTTON_GROUP_OK_CANCEL:
		alert->button_pressed = FZ_ALERT_BUTTON_OK;
		break;
	case FZ_ALERT_BUTTON_GROUP_YES_NO:
	case FZ_ALERT_BUTTON_GROUP_YES_NO_CANCEL:
		alert->button_pressed = FZ_ALERT_BUTTON_YES;
		break;
	}
}

void winadvancetimer(pdfapp_t *app, float duration)
{
}

void winprint(pdfapp_t *app)
{
	fprintf(stderr, "The MuPDF library supports printing, but this application currently does not");
}

static char pd_password[256] = "";
static char td_textinput[LONGLINE] = "";

char *winpassword(pdfapp_t *app, char *filename)
{
	if (pd_password[0] == 0)
		return NULL;
	return pd_password;
}

char *wintextinput(pdfapp_t *app, char *inittext, int retry)
{
	if (retry)
		return NULL;

	if (td_textinput[0] != 0)
		return td_textinput;
	return inittext;
}

int winchoiceinput(pdfapp_t *app, int nopts, char *opts[], int *nvals, char *vals[])
{
	return 0;
}

void winhelp(pdfapp_t*app)
{
}

void winclose(pdfapp_t *app)
{
	pdfapp_close(app);
	exit(0);
}

int winsavequery(pdfapp_t *app)
{
	return DISCARD;
}

int wingetsavepath(pdfapp_t *app, char *buf, int len)
{
	return 0;
}

void winreplacefile(char *source, char *target)
{
}

void wincursor(pdfapp_t *app, int curs)
{
}

void wintitle(pdfapp_t *app, char *title)
{
}

void windrawrect(pdfapp_t *app, int x0, int y0, int x1, int y1)
{
}

void windrawstring(pdfapp_t *app, int x, int y, char *s)
{
}

void winresize(pdfapp_t *app, int w, int h)
{
}

void winrepaint(pdfapp_t *app)
{
}

void winrepaintsearch(pdfapp_t *app)
{
}

void winfullscreen(pdfapp_t *app, int state)
{
}

/*
 * Event handling
 */

void windocopy(pdfapp_t *app)
{
}

void winreloadfile(pdfapp_t *app)
{
	pdfapp_close(app);
	pdfapp_open(app, filename, 1);
}

void winopenuri(pdfapp_t *app, char *buf)
{
}

static void
usage(void)
{
	fprintf(stderr, "mujstest: Scriptable tester for mupdf + js\n");
	fprintf(stderr, "\nSyntax: mujstest -o <filename> [ -p <prefix> ] [-v] <scriptfile>\n");
	fprintf(stderr, "\n<filename> should sensibly be of the form file-%%d.png\n");
	fprintf(stderr, "\n<prefix> is a path prefix to apply to filenames within the script\n");
	fprintf(stderr, "\n-v\tverbose\n");
	fprintf(stderr, "\nscriptfile contains a list of commands:\n");
	fprintf(stderr, "\tPASSWORD <password>\tSet the password\n");
	fprintf(stderr, "\tOPEN <filename>\tOpen a file\n");
	fprintf(stderr, "\tGOTO <page>\tJump to a particular page\n");
	fprintf(stderr, "\tSCREENSHOT\tSave a screenshot\n");
	fprintf(stderr, "\tRESIZE <w> <h>\tResize the screen to a given size\n");
	fprintf(stderr, "\tCLICK <x> <y> <btn>\tClick at a given position\n");
	fprintf(stderr, "\tTEXT <string>\tSet a value to be entered\n");
	exit(1);
}

static char *
my_getline(FILE *file)
{
	int c;
	char *d = getline_buffer;
	int space = sizeof(getline_buffer)-1;

	/* Skip over any prefix of whitespace */
	do
	{
		c = fgetc(file);
	}
	while (isspace(c));

	if (c < 0)
		return NULL;

	/* Read the line in */
	do
	{
		*d++ = (char)c;
		c = fgetc(file);
	}
	while (c >= 32 && space--);

	/* If we ran out of space, skip the rest of the line */
	if (space == 0)
	{
		while (c >= 32)
			c = fgetc(file);
	}

	*d = 0;

	return getline_buffer;
}

static int
match(char **line, const char *match)
{
	char *s = *line;

	if (s == NULL)
		return 0;

	while (isspace(*(unsigned char *)s))
		s++;

	while (*s == *match)
	{
		if (*s == 0)
		{
			*line = s;
			return 1;
		}
		s++;
		match++;
	}

	if (*match != 0)
		return 0;

	/* We matched! Skip over any whitespace */
	while (isspace(*(unsigned char *)s))
		s++;

	*line = s;

	/* Trim whitespace off the end of the line */
	/* Run to the end of the line */
	while (*s)
		s++;

	/* Run back until we find where we started, or non whitespace */
	while (s != *line && isspace((unsigned char)s[-1]))
		s--;

	/* Remove the suffix of whitespace */
	*s = 0;

	return 1;
}

static void unescape_string(char *d, const char *s)
{
	char c;

	while ((c = *s++) != 0)
	{
		if (c == '\\')
		{
			c = *s++;
			switch(c)
			{
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			}
		}
		*d++ = c;
	}
	*d = 0;
}

int
main(int argc, char *argv[])
{
	fz_context *ctx;
	FILE *script = NULL;
	int c;

	while ((c = fz_getopt(argc, argv, "o:p:v")) != -1)
	{
		switch(c)
		{
		case 'o': output = fz_optarg; break;
		case 'p': prefix = fz_optarg; break;
		case 'v': verbosity ^= 1; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}
	pdfapp_init(ctx, &gapp);
	gapp.scrw = 640;
	gapp.scrh = 480;
	gapp.colorspace = fz_device_rgb;

	fz_try(ctx)
	{
		while (fz_optind < argc)
		{
			scriptname = argv[fz_optind++];
			script = fopen(scriptname, "rb");
			if (script == NULL)
				fz_throw(ctx, "cannot open script: %s", scriptname);

			do
			{
				char *line = my_getline(script);
				if (line == NULL)
					continue;
				if (verbosity)
					fprintf(stderr, "'%s'\n", line);
				if (match(&line, "%"))
				{
					/* Comment */
				}
				else if (match(&line, "PASSWORD"))
				{
					strcpy(pd_password, line);
				}
				else if (match(&line, "OPEN"))
				{
					char path[1024];
					if (file_open)
						pdfapp_close(&gapp);
					strcpy(filename, line);
					if (prefix)
					{
						sprintf(path, "%s%s", prefix, line);
					}
					else
					{
						strcpy(path, line);
					}
					pdfapp_open(&gapp, path, 0);
					file_open = 1;
				}
				else if (match(&line, "GOTO"))
				{
					pdfapp_gotopage(&gapp, atoi(line)-1);
				}
				else if (match(&line, "SCREENSHOT"))
				{
					char text[1024];

					sprintf(text, output, ++shotcount);
					if (strstr(text, ".pgm") || strstr(text, ".ppm") || strstr(text, ".pnm"))
						fz_write_pnm(ctx, gapp.image, text);
					else
						fz_write_png(ctx, gapp.image, text, 0);
				}
				else if (match(&line, "RESIZE"))
				{
					int w, h;
					sscanf(line, "%d %d", &w, &h);
					pdfapp_onresize(&gapp, w, h);
				}
				else if (match(&line, "CLICK"))
				{
					float x, y, b;
					int n;
					n = sscanf(line, "%f %f %f", &x, &y, &b);
					if (n < 1)
						x = 0.0f;
					if (n < 2)
						y = 0.0f;
					if (n < 3)
						b = 1;
					/* state = 1 = transition down */
					pdfapp_onmouse(&gapp, (int)x, (int)y, b, 0, 1);
					/* state = -1 = transition up */
					pdfapp_onmouse(&gapp, (int)x, (int)y, b, 0, -1);
				}
				else if (match(&line, "TEXT"))
				{
					unescape_string(td_textinput, line);
				}
				else
				{
					fprintf(stderr, "Unmatched: %s\n", line);
				}
			}
			while (!feof(script));

			fclose(script);
		}
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "error: cannot execute '%s'\n", scriptname);
	}

	if (file_open)
		pdfapp_close(&gapp);

	fz_free_context(ctx);

	return 0;
}
