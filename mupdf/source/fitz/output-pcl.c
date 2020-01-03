#include "mupdf/fitz.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Lifted from ghostscript gdevjlm.h */
/*
 * The notion that there is such a thing as a "PCL printer" is a fiction: no
 * two "PCL" printers, even at the same PCL level, have identical command
 * sets. (The H-P documentation isn't fully accurate either; for example,
 * it doesn't reveal that the DeskJet printers implement anything beyond PCL
 * 3.)
 *
 * This file contains feature definitions for a generic monochrome PCL
 * driver (gdevdljm.c), and the specific feature values for all such
 * printers that Ghostscript currently supports.
 */

/* Printer spacing capabilities. Include at most one of these. */
#define PCL_NO_SPACING	0	/* no vertical spacing capability, must be 0 */
#define PCL3_SPACING	1	/* <ESC>*p+<n>Y (PCL 3) */
#define PCL4_SPACING	2	/* <ESC>*b<n>Y (PCL 4) */
#define PCL5_SPACING	4	/* <ESC>*b<n>Y and clear seed row (PCL 5) */
/* The following is only used internally. */
#define PCL_ANY_SPACING \
	(PCL3_SPACING | PCL4_SPACING | PCL5_SPACING)

/* Individual printer properties. Any subset of these may be included. */
#define PCL_MODE_2_COMPRESSION		8	/* compression mode 2 supported */
						/* (PCL 4) */
#define PCL_MODE_3_COMPRESSION		16	/* compression modes 2 & 3 supported */
						/* (PCL 5) */
#define PCL_END_GRAPHICS_DOES_RESET	32	/* <esc>*rB resets all parameters */
#define PCL_HAS_DUPLEX			64	/* <esc>&l<duplex>S supported */
#define PCL_CAN_SET_PAPER_SIZE		128	/* <esc>&l<sizecode>A supported */
#define PCL_CAN_PRINT_COPIES		256	/* <esc>&l<copies>X supported */
#define HACK__IS_A_LJET4PJL		512
#define HACK__IS_A_OCE9050		1024
#define PCL_HAS_ORIENTATION             2048
#define PCL_CAN_SET_CUSTOM_PAPER_SIZE   4096
#define PCL_HAS_RICOH_PAPER_SIZES       8192

/* Shorthands for the most common spacing/compression combinations. */
#define PCL_MODE0 PCL3_SPACING
#define PCL_MODE0NS PCL_NO_SPACING
#define PCL_MODE2 (PCL4_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE2P (PCL_NO_SPACING | PCL_MODE_2_COMPRESSION)
#define PCL_MODE3 (PCL5_SPACING | PCL_MODE_3_COMPRESSION)
#define PCL_MODE3NS (PCL_NO_SPACING | PCL_MODE_3_COMPRESSION)

#define MIN_SKIP_LINES 7
static const char *const from2to3 = "\033*b3M";
static const char *const from3to2 = "\033*b2M";
static const int penalty_from2to3 = 5; /* strlen(from2to3); */
static const int penalty_from3to2 = 5; /* strlen(from3to2); */

/* Generic */
static const fz_pcl_options fz_pcl_options_generic =
{
	(PCL_MODE2 | PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_SET_CUSTOM_PAPER_SIZE),
	"\033&k1W\033*b2M",
	"\033&k1W\033*b2M"
};

/* H-P DeskJet */
static const fz_pcl_options fz_pcl_options_ljet4 =
{
	(PCL_MODE2 | PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE),
	"\033&k1W\033*b2M",
	"\033&k1W\033*b2M"
};

/* H-P DeskJet 500 */
static const fz_pcl_options fz_pcl_options_dj500 =
{
	(PCL_MODE3 | PCL_END_GRAPHICS_DOES_RESET | PCL_CAN_SET_PAPER_SIZE),
	"\033&k1W",
	"\033&k1W"
};

/* Kyocera FS-600 */
static const fz_pcl_options fz_pcl_options_fs600 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033*r0F\033&u%dD",
	"\033*r0F\033&u%dD"
};

/* H-P original LaserJet */
/* H-P LaserJet Plus */
static const fz_pcl_options fz_pcl_options_lj =
{
	(PCL_MODE0),
	"\033*b0M",
	"\033*b0M"
};

/* H-P LaserJet IIp, IId */
static const fz_pcl_options fz_pcl_options_lj2 =
{
	(PCL_MODE2P | PCL_CAN_SET_PAPER_SIZE),
	"\033*r0F\033*b2M",
	"\033*r0F\033*b2M"
};

/* H-P LaserJet III* */
static const fz_pcl_options fz_pcl_options_lj3 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F",
	"\033&l-180u36Z\033*r0F"
};

/* H-P LaserJet IIId */
static const fz_pcl_options fz_pcl_options_lj3d =
{
	(PCL_MODE3 | PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F",
	"\033&l180u36Z\033*r0F"
};

/* H-P LaserJet 4 */
static const fz_pcl_options fz_pcl_options_lj4 =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l-180u36Z\033*r0F\033&u%dD"
};

/* H-P LaserJet 4 PL */
static const fz_pcl_options fz_pcl_options_lj4pl =
{
	(PCL_MODE3 | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES | HACK__IS_A_LJET4PJL),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l-180u36Z\033*r0F\033&u%dD"
};

/* H-P LaserJet 4d */
static const fz_pcl_options fz_pcl_options_lj4d =
{
	(PCL_MODE3 | PCL_HAS_DUPLEX | PCL_CAN_SET_PAPER_SIZE | PCL_CAN_PRINT_COPIES),
	"\033&l-180u36Z\033*r0F\033&u%dD",
	"\033&l180u36Z\033*r0F\033&u%dD"
};

/* H-P 2563B line printer */
static const fz_pcl_options fz_pcl_options_lp2563b =
{
	(PCL_MODE0NS | PCL_CAN_SET_PAPER_SIZE),
	"\033*b0M",
	"\033*b0M"
};

/* OCE 9050 line printer */
static const fz_pcl_options fz_pcl_options_oce9050 =
{
	(PCL_MODE3NS | PCL_CAN_SET_PAPER_SIZE | HACK__IS_A_OCE9050),
	"\033*b0M",
	"\033*b0M"
};

enum {
	eLetterPaper = 0,
	eLegalPaper,
	eA4Paper,
	eExecPaper,
	eLedgerPaper,
	eA3Paper,
	eCOM10Envelope,
	eMonarchEnvelope,
	eC5Envelope,
	eDLEnvelope,
	eJB4Paper,
	eJB5Paper,
	eB5Envelope,
	eB5Paper,                   /* 2.1 */
	eJPostcard,
	eJDoublePostcard,
	eA5Paper,
	eA6Paper,                   /* 2.0 */
	eJB6Paper,                  /* 2.0 */
	eJIS8K,                     /* 2.1 */
	eJIS16K,                    /* 2.1 */
	eJISExec,                   /* 2.1 */
	eDefaultPaperSize = 96,     /* 2.1 */
	eCustomPaperSize = 101,
	eB6JIS = 201,               /* non-standard, Ricoh printers */
	eC6Envelope = 202,          /* non-standard, Ricoh printers */
	e8Kai  = 203,               /* non-standard, Ricoh printers */
	e16Kai = 204,               /* non-standard, Ricoh printers */
	e12x18 = 205,               /* non-standard, Ricoh printers */
	e13x19_2 = 212,             /* non-standard, Ricoh printers */
	e13x19 = 213,               /* non-standard, Ricoh printers */
	e12_6x19_2 = 214,           /* non-standard, Ricoh printers */
	e12_6x18_5 = 215,           /* non-standard, Ricoh printers */
	e13x18  = 216,              /* non-standard, Ricoh printers */
	eSRA3 = 217,                /* non-standard, Ricoh printers */
	eSRA4 = 218,                /* non-standard, Ricoh printers */
	e226x310 = 219,             /* non-standard, Ricoh printers */
	e310x432 = 220,             /* non-standard, Ricoh printers */
	eEngQuatro = 221,           /* non-standard, Ricoh printers */
	e11x14 = 222,               /* non-standard, Ricoh printers */
	e11x15 = 223,               /* non-standard, Ricoh printers */
	e10x14 = 224,               /* non-standard, Ricoh printers */
};

static void copy_opts(fz_pcl_options *dst, const fz_pcl_options *src)
{
	if (dst)
		*dst = *src;
}

const char *fz_pcl_write_options_usage =
	"PCL output options:\n"
	"\tcolorspace=mono: render 1-bit black and white page\n"
	"\tcolorspace=rgb: render full color page\n"
	"\tpreset=generic|ljet4|dj500|fs600|lj|lj2|lj3|lj3d|lj4|lj4pl|lj4d|lp2563b|oce9050\n"
	"\tspacing=0: No vertical spacing capability\n"
	"\tspacing=1: PCL 3 spacing (<ESC>*p+<n>Y)\n"
	"\tspacing=2: PCL 4 spacing (<ESC>*b<n>Y)\n"
	"\tspacing=3: PCL 5 spacing (<ESC>*b<n>Y and clear seed row)\n"
	"\tmode2: Enable mode 2 graphics compression\n"
	"\tmode3: Enable mode 3 graphics compression\n"
	"\teog_reset: End of graphics (<ESC>*rB) resets all parameters\n"
	"\thas_duplex: Duplex supported (<ESC>&l<duplex>S)\n"
	"\thas_papersize: Papersize setting supported (<ESC>&l<sizecode>A)\n"
	"\thas_copies: Number of copies supported (<ESC>&l<copies>X)\n"
	"\tis_ljet4pjl: Disable/Enable HP 4PJL model-specific output\n"
	"\tis_oce9050: Disable/Enable Oce 9050 model-specific output\n"
	"\n";

/*
	Initialize PCL option struct for a given preset.

	Currently defined presets include:

		generic	Generic PCL printer
		ljet4	HP DeskJet
		dj500	HP DeskJet 500
		fs600	Kyocera FS-600
		lj	HP LaserJet, HP LaserJet Plus
		lj2	HP LaserJet IIp, HP LaserJet IId
		lj3	HP LaserJet III
		lj3d	HP LaserJet IIId
		lj4	HP LaserJet 4
		lj4pl	HP LaserJet 4 PL
		lj4d	HP LaserJet 4d
		lp2563b	HP 2563B line printer
		oce9050	Oce 9050 Line printer
*/
void fz_pcl_preset(fz_context *ctx, fz_pcl_options *opts, const char *preset)
{
	if (preset == NULL || *preset == 0 || !strcmp(preset, "generic"))
		copy_opts(opts, &fz_pcl_options_generic);
	else if (!strcmp(preset, "ljet4"))
		copy_opts(opts, &fz_pcl_options_ljet4);
	else if (!strcmp(preset, "dj500"))
		copy_opts(opts, &fz_pcl_options_dj500);
	else if (!strcmp(preset, "fs600"))
		copy_opts(opts, &fz_pcl_options_fs600);
	else if (!strcmp(preset, "lj"))
		copy_opts(opts, &fz_pcl_options_lj);
	else if (!strcmp(preset, "lj2"))
		copy_opts(opts, &fz_pcl_options_lj2);
	else if (!strcmp(preset, "lj3"))
		copy_opts(opts, &fz_pcl_options_lj3);
	else if (!strcmp(preset, "lj3d"))
		copy_opts(opts, &fz_pcl_options_lj3d);
	else if (!strcmp(preset, "lj4"))
		copy_opts(opts, &fz_pcl_options_lj4);
	else if (!strcmp(preset, "lj4pl"))
		copy_opts(opts, &fz_pcl_options_lj4pl);
	else if (!strcmp(preset, "lj4d"))
		copy_opts(opts, &fz_pcl_options_lj4d);
	else if (!strcmp(preset, "lp2563b"))
		copy_opts(opts, &fz_pcl_options_lp2563b);
	else if (!strcmp(preset, "oce9050"))
		copy_opts(opts, &fz_pcl_options_oce9050);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unknown preset '%s'", preset);
}

/*
	Parse PCL options.

	Currently defined options and values are as follows:

		preset=X	Either "generic" or one of the presets as for fz_pcl_preset.
		spacing=0	No vertical spacing capability
		spacing=1	PCL 3 spacing (<ESC>*p+<n>Y)
		spacing=2	PCL 4 spacing (<ESC>*b<n>Y)
		spacing=3	PCL 5 spacing (<ESC>*b<n>Y and clear seed row)
		mode2		Disable/Enable mode 2 graphics compression
		mode3		Disable/Enable mode 3 graphics compression
		eog_reset	End of graphics (<ESC>*rB) resets all parameters
		has_duplex	Duplex supported (<ESC>&l<duplex>S)
		has_papersize	Papersize setting supported (<ESC>&l<sizecode>A)
		has_copies	Number of copies supported (<ESC>&l<copies>X)
		is_ljet4pjl	Disable/Enable HP 4PJL model-specific output
		is_oce9050	Disable/Enable Oce 9050 model-specific output
*/
fz_pcl_options *
fz_parse_pcl_options(fz_context *ctx, fz_pcl_options *opts, const char *args)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, args, "preset", &val))
		fz_pcl_preset(ctx, opts, val);
	else
		fz_pcl_preset(ctx, opts, "generic");

	if (fz_has_option(ctx, args, "spacing", &val))
	{
		switch (atoi(val))
		{
		case 0: opts->features &= ~PCL_ANY_SPACING; break;
		case 1: opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL3_SPACING; break;
		case 2: opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL4_SPACING; break;
		case 3: opts->features = (opts->features & ~PCL_ANY_SPACING) | PCL5_SPACING; break;
		default: fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PCL spacing %d (0-3 only)", atoi(val));
		}
	}
	if (fz_has_option(ctx, args, "mode2", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_MODE_2_COMPRESSION;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_MODE_2_COMPRESSION;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for mode2 value");
	}
	if (fz_has_option(ctx, args, "mode3", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_MODE_3_COMPRESSION;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_MODE_3_COMPRESSION;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for mode3 value");
	}
	if (fz_has_option(ctx, args, "eog_reset", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_END_GRAPHICS_DOES_RESET;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_END_GRAPHICS_DOES_RESET;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for eog_reset value");
	}
	if (fz_has_option(ctx, args, "has_duplex", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_HAS_DUPLEX;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_HAS_DUPLEX;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for has_duplex value");
	}
	if (fz_has_option(ctx, args, "has_papersize", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_CAN_SET_PAPER_SIZE;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_CAN_SET_PAPER_SIZE;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for has_papersize value");
	}
	if (fz_has_option(ctx, args, "has_copies", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~PCL_CAN_PRINT_COPIES;
		else if (fz_option_eq(val, "yes"))
			opts->features |= PCL_CAN_PRINT_COPIES;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for has_papersize value");
	}
	if (fz_has_option(ctx, args, "is_ljet4pjl", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~HACK__IS_A_LJET4PJL;
		else if (fz_option_eq(val, "yes"))
			opts->features |= HACK__IS_A_LJET4PJL;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for is_ljet4pjl value");
	}
	if (fz_has_option(ctx, args, "is_oce9050", &val))
	{
		if (fz_option_eq(val, "no"))
			opts->features &= ~HACK__IS_A_OCE9050;
		else if (fz_option_eq(val, "yes"))
			opts->features |= HACK__IS_A_OCE9050;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Expected 'yes' or 'no' for is_oce9050 value");
	}

	return opts;
}

static void
make_init(fz_pcl_options *pcl, char *buf, unsigned long len, const char *str, int res)
{
	int paper_source = -1;

	fz_snprintf(buf, len, str, res);

	if (pcl->manual_feed_set && pcl->manual_feed)
		paper_source = 2;
	else if (pcl->media_position_set && pcl->media_position >= 0)
		paper_source = pcl->media_position;
	if (paper_source >= 0)
	{
		char buf2[40];
		fz_snprintf(buf2, sizeof(buf2), "\033&l%dH", paper_source);
		strncat(buf, buf2, len);
	}
}

static void
pcl_header(fz_context *ctx, fz_output *out, fz_pcl_options *pcl, int num_copies, int xres, int yres, int w, int h)
{
	char odd_page_init[80];
	char even_page_init[80];

	make_init(pcl, odd_page_init, sizeof(odd_page_init), pcl->odd_page_init, xres);
	make_init(pcl, even_page_init, sizeof(even_page_init), pcl->even_page_init, xres);

	if (pcl->page_count == 0)
	{
		if (pcl->features & HACK__IS_A_LJET4PJL)
			fz_write_string(ctx, out, "\033%-12345X@PJL\r\n@PJL ENTER LANGUAGE = PCL\r\n");
		fz_write_string(ctx, out, "\033E"); /* reset printer */
		/* Reset the margins */
		fz_write_string(ctx, out, "\033&10e-180u36Z");
		/* If the printer supports it, set orientation */
		if (pcl->features & PCL_HAS_ORIENTATION)
		{
			fz_write_printf(ctx, out, "\033&l%dO", pcl->orientation);
		}
		/* If the printer supports it, set the paper size */
		/* based on the actual requested size. */
		if (pcl->features & PCL_CAN_SET_PAPER_SIZE)
		{
			/* It probably never hurts to define the page explicitly */
			{
				int decipointw = (w * 720 + (xres>>1)) / xres;
				int decipointh = (h * 720 + (yres>>1)) / yres;

				fz_write_printf(ctx, out, "\033&f%dI", decipointw);
				fz_write_printf(ctx, out, "\033&f%dJ", decipointh);
			}
			fz_write_printf(ctx, out, "\033&l%dA", pcl->paper_size);
		}
		/* If printer can duplex, set duplex mode appropriately. */
		if (pcl->features & PCL_HAS_DUPLEX)
		{
			if (pcl->duplex_set)
			{
				if (pcl->duplex)
				{
					if (!pcl->tumble)
						fz_write_string(ctx, out, "\033&l1S");
					else
						fz_write_string(ctx, out, "\033&l2S");
				}
				else
					fz_write_string(ctx, out, "\033&l0S");
			}
			else
			{
				/* default to duplex for this printer */
				fz_write_string(ctx, out, "\033&l1S");
			}
		}
	}

	/* Put out per-page initialization. */
	/* In duplex mode the sheet is already in process, so there are some
	 * commands which must not be sent to the printer for the 2nd page,
	 * as these commands will cause the printer to eject the sheet with
	 * only the 1st page printed. These commands are:
	 * \033&l%dA (setting paper size)
	 * \033&l%dH (setting paper tray)
	 * in simplex mode we set these parameters for each page,
	 * in duplex mode we set these parameters for each odd page
	 */

	if ((pcl->features & PCL_HAS_DUPLEX) && pcl->duplex_set && pcl->duplex)
	{
		/* We are printing duplex, so change margins as needed */
		if (((pcl->page_count/num_copies)%2) == 0)
		{
			if (pcl->page_count != 0 && (pcl->features & PCL_CAN_SET_PAPER_SIZE))
			{
				fz_write_printf(ctx, out, "\033&l%dA", pcl->paper_size);
			}
			fz_write_string(ctx, out, "\033&l0o0l0E");
			fz_write_string(ctx, out, pcl->odd_page_init);
		}
		else
			fz_write_string(ctx, out, pcl->even_page_init);
	}
	else
	{
		if (pcl->features & PCL_CAN_SET_PAPER_SIZE)
		{
			fz_write_printf(ctx, out, "\033&l%dA", pcl->paper_size);
		}
		fz_write_string(ctx, out, "\033&l0o0l0E");
		fz_write_string(ctx, out, pcl->odd_page_init);
	}

	fz_write_printf(ctx, out, "\033&l%dX", num_copies); /* # of copies */

	/* End raster graphics, position cursor at top. */
	fz_write_string(ctx, out, "\033*rB\033*p0x0Y");

	/* The DeskJet and DeskJet Plus reset everything upon */
	/* receiving \033*rB, so we must reinitialize graphics mode. */
	if (pcl->features & PCL_END_GRAPHICS_DOES_RESET)
	{
		fz_write_string(ctx, out, pcl->odd_page_init); /* Assume this does the right thing */
		fz_write_printf(ctx, out, "\033&l%dX", num_copies); /* # of copies */
	}

	/* Set resolution. */
	fz_write_printf(ctx, out, "\033*t%dR", xres);

	/* Raster units */
	/* 96,100,120,144,150,160,180,200,225,240,288,300,360,400,450,480,600,720,800,900,1200,1440,1800,2400,3600,7200 */
	/* FIXME: xres vs yres */
	fz_write_printf(ctx, out, "\033&u%dD", xres);

	pcl->page_count++;
}

typedef struct pcl_papersize_s
{
	int code;
	const char *text;
	int width;
	int height;
} pcl_papersize;

static const pcl_papersize papersizes[] =
{
	{ eLetterPaper,      "letter",       2550, 3300},
	{ eLegalPaper,       "legal",        2550, 4200},
	{ eA4Paper,          "a4",           2480, 3507},
	{ eExecPaper,        "executive",    2175, 3150},
	{ eLedgerPaper,      "ledger",       3300, 5100},
	{ eA3Paper,          "a3",           3507, 4960},
	{ eCOM10Envelope,    "com10",        1237, 2850},
	{ eMonarchEnvelope,  "monarch",      1162, 2250},
	{ eC5Envelope,       "c5",           1913, 2704},
	{ eDLEnvelope,       "dl",           1299, 2598},
	{ eJB4Paper,         "jisb4",        3035, 4299},
	{ eJB4Paper,         "jis b4",       3035, 4299},
	{ eJB5Paper,         "jisb5",        2150, 3035},
	{ eJB5Paper,         "jis b5",       2150, 3035},
	{ eB5Envelope,       "b5",           2078, 2952},
	{ eB5Paper,          "b5paper",      2150, 3035},
	{ eJPostcard,        "jpost",        1181, 1748},
	{ eJDoublePostcard,  "jpostd",       2362, 1748},
	{ eA5Paper,          "a5",           1748, 2480},
	{ eA6Paper,          "a6",           1240, 1748},
	{ eJB6Paper,         "jisb6",        1512, 2150},
	{ eJIS8K,            "jis8K",        3154, 4606},
	{ eJIS16K,           "jis16K",       2303, 3154},
	{ eJISExec,          "jisexec",      2551, 3898},
	{ eB6JIS,            "B6 (JIS)",     1512, 2150},
	{ eC6Envelope,       "C6",           1345, 1912},
	{ e8Kai,             "8Kai",         3154, 4608},
	{ e16Kai,            "16Kai",        2304, 3154},
	{ e12x18,            "12x18",        3600, 5400},
	{ e13x19_2,          "13x19.2",      3900, 5758},
	{ e13x19,            "13x19",        3900, 5700},
	{ e12_6x19_2,        "12.6x19.2",    3779, 5758},
	{ e12_6x18_5,        "12.6x18.5",    3779, 5550},
	{ e13x18,            "13x18",        3900, 5400},
	{ eSRA3,             "SRA3",         3779, 5316},
	{ eSRA4,             "SRA4",         2658, 3779},
	{ e226x310,          "226x310",      2670, 3662},
	{ e310x432,          "310x432",      3662, 5104},
	{ eEngQuatro,        "EngQuatro",    2400, 3000},
	{ e11x14,            "11x14",        3300, 4200},
	{ e11x15,            "11x15",        3300, 4500},
	{ e10x14,            "10x14",        3000, 4200}
};

#define num_elems(X) (sizeof(X)/sizeof(*X))

static void guess_paper_size(fz_pcl_options *pcl, int w, int h, int xres, int yres)
{
	int size;
	int rotated = 0;

	/* If we've been given a paper size, live with it */
	if (pcl->paper_size != 0)
		return;

	w = w * 300 / xres;
	h = h * 300 / xres;

	/* Look for an exact match */
	for (size = 0; size < (int)num_elems(papersizes); size++)
	{
		if (papersizes[size].code > eCustomPaperSize && (pcl->features & PCL_HAS_RICOH_PAPER_SIZES) == 0)
			continue;
		if (w == papersizes[size].width && h == papersizes[size].height)
			break;
		if ((pcl->features & PCL_HAS_ORIENTATION) && w == papersizes[size].height && h == papersizes[size].width)
		{
			rotated = 1;
			break;
		}
	}

	/* If we didn't find an exact match, find the smallest one that's
	 * larger. Consider orientation if our printer supports it. */
	if (size == num_elems(papersizes))
	{
		if ((pcl->features & PCL_CAN_SET_CUSTOM_PAPER_SIZE) != 0)
		{
			/* Send it as a custom size */
			size = eCustomPaperSize;
		}
		else
		{
			/* Send the next larger one (minimise waste) */
			int i;
			int best_waste = INT_MAX;
			for (i = 0; i < (int)num_elems(papersizes); i++)
			{
				int waste;
				if (papersizes[i].code > eCustomPaperSize && (pcl->features & PCL_HAS_RICOH_PAPER_SIZES) == 0)
					continue;
				waste = papersizes[i].width * papersizes[i].height - w * h;
				if (waste > best_waste)
					continue;
				if (w <= papersizes[i].width && h <= papersizes[i].height)
				{
					best_waste = waste;
					rotated = 0;
					size = i;
				}
				if ((pcl->features & PCL_HAS_ORIENTATION) && w <= papersizes[i].height && h <= papersizes[i].width)
				{
					best_waste = waste;
					rotated = 1;
					size = i;
				}
			}
		}
	}

	/* Now, size = The best size we have (or num_elems(papersizes)) if it's too big */

	if (size < (int)num_elems(papersizes))
		pcl->paper_size = papersizes[size].code;
	else
		pcl->paper_size = eCustomPaperSize; /* Custom */

	pcl->orientation = rotated;
}

/* Copy a line, returning true if the line was blank. */
static int
line_is_blank(unsigned char *dst, const unsigned char *sp, int w)
{
	int zero = 0;

	while (w-- > 0)
	{
		zero |= (*dst++ = *sp++);
		zero |= (*dst++ = *sp++);
		zero |= (*dst++ = *sp++);
	}

	return zero == 0;
}

static int
delta_compression(unsigned char *curr, unsigned char *prev, unsigned char *comp, int ds, int space)
{
	int left = space;
	int x = ds;

	while (x > 0)
	{
		/* Count matching bytes */
		int match = 0;
		int diff = 0;
		while (x > 0 && *curr == *prev)
		{
			curr++;
			prev++;
			match++;
			x--;
		}

		/* Count different bytes */
		while (x > 0 && *curr != *prev)
		{
			curr++;
			prev++;
			diff++;
			x--;
		}

		while (diff > 0)
		{
			int exts;
			int mini_diff = diff;
			if (mini_diff > 8)
				mini_diff = 8;

			exts = (match+255-31)/255;
			left -= 1 + mini_diff + exts;
			if (left < 0)
				return 0;
			*comp++ = ((mini_diff-1)<<5) | (match < 31 ? match : 31);
			if (exts > 0)
			{
				match -= 31;
				while (--exts)
				{
					*comp++ = 255;
					match -= 255;
				}
				*comp++ = match;
			}
			memcpy(comp, curr-diff, mini_diff);
			comp += mini_diff;

			match = 0;
			diff -= mini_diff;
		}
	}
	return space - left;
}

void
fz_write_pixmap_as_pcl(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pcl_options *pcl)
{
	fz_band_writer *writer;

	if (!pixmap || !out)
		return;

	writer = fz_new_color_pcl_band_writer(ctx, out, pcl);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

typedef struct color_pcl_band_writer_s
{
	fz_band_writer super;
	fz_pcl_options options;
	unsigned char *linebuf;
	unsigned char compbuf[32768];
	unsigned char compbuf2[32768];
} color_pcl_band_writer;

static void
color_pcl_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	color_pcl_band_writer *writer = (color_pcl_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int s = writer->super.s;
	int a = writer->super.alpha;
	int xres = writer->super.xres;
	int yres = writer->super.yres;

	if (a != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color PCL cannot write alpha channel");
	if (s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color PCL cannot write spot colors");
	if (n != 3)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color PCL must be RGB");

	writer->linebuf = Memento_label(fz_malloc(ctx, w * 3 * 2), "color_pcl_linebuf");

	guess_paper_size(&writer->options, w, h, xres, yres);

	pcl_header(ctx, out, &writer->options, 1, xres, yres, w, h);

	/* Raster presentation */
	/* Print in orientation of the logical page */
	fz_write_string(ctx, out, "\033&r0F");

	/* Set color mode */
	fz_write_data(ctx, out, "\033*v6W"
		"\000"	/* Colorspace 0 = Device RGB */
		"\003"	/* Pixel encoding mode: 3 = Direct by Pixel*/
		"\000"	/* Bits per index: 0 = no palette */
		"\010"	/* Red bits */
		"\010"	/* Green bits */
		"\010",	/* Blue bits */
		11
		);

	/* Raster resolution */
	/* Supposed to be strictly 75, 100, 150, 200, 300, 600 */
	/* FIXME: xres vs yres */
	fz_write_printf(ctx, out, "\033*t%dR", xres);
}

static void flush_if_not_room(fz_context *ctx, fz_output *out, const unsigned char *comp, int *fill, int len)
{
	if (len + *fill >= 32767)
	{
		/* Can't fit any data, so flush */
		fz_write_printf(ctx, out, "\033*b%dW", *fill);
		fz_write_data(ctx, out, comp, *fill);
		*fill = 0;
	}
}

static void
color_pcl_compress_column(fz_context *ctx, color_pcl_band_writer *writer, const unsigned char *sp, int w, int h, int stride)
{
	fz_output *out = writer->super.out;
	int ss = w * 3;
	int seed_valid = 0;
	int fill = 0;
	int y = 0;
	unsigned char *prev = writer->linebuf + w * 3;
	unsigned char *curr = writer->linebuf;
	unsigned char *comp = writer->compbuf;
	unsigned char *comp2 = writer->compbuf2;

	while (y < h)
	{
		/* Skip over multiple blank lines */
		int blanks;
		do
		{
			blanks = 0;
			while (blanks < 32767 && y < h)
			{
				if (!line_is_blank(curr, sp, w))
					break;
				blanks++;
				y++;
			}

			if (blanks)
			{
				flush_if_not_room(ctx, out, comp, &fill, 3);
				comp[fill++] = 4; /* Empty row */
				comp[fill++] = blanks>>8;
				comp[fill++] = blanks & 0xFF;
				seed_valid = 0;
			}
		}
		while (blanks == 32767);

		if (y == h)
			break;

		/* So, at least 1 more line to copy, and it's in curr */
		if (seed_valid && memcmp(curr, prev, ss) == 0)
		{
			int count = 1;
			sp += stride;
			y++;
			while (count < 32767 && y < h)
			{
				if (memcmp(sp-stride, sp, ss) != 0)
					break;
				count++;
				sp += stride;
				y++;
			}
			flush_if_not_room(ctx, out, comp, &fill, 3);
			comp[fill++] = 5; /* Duplicate row */
			comp[fill++] = count>>8;
			comp[fill++] = count & 0xFF;
		}
		else
		{
			unsigned char *tmp;
			int len = 0;

			/* Compress the line into our fixed buffer. */
			if (seed_valid)
				len = delta_compression(curr, prev, comp2, ss, fz_mini(ss-1, 32767-3));

			if (len > 0)
			{
				/* Delta compression */
				flush_if_not_room(ctx, out, comp, &fill, len+3);
				comp[fill++] = 3; /* Delta compression */
				comp[fill++] = len>>8;
				comp[fill++] = len & 0xFF;
				memcpy(&comp[fill], comp2, len);
				fill += len;
			}
			else
			{
				flush_if_not_room(ctx, out, comp, &fill, 3 + ss);

				/* PCL requires that all rows MUST fit in at most 1 block, so
				 * we are carefully sending columns that are only so wide. */

				/* Unencoded */
				/* Transfer Raster Data: ss+3 bytes, 0 = Unencoded, count high, count low */
				comp[fill++] = 0;
				comp[fill++] = ss>>8;
				comp[fill++] = ss & 0xFF;
				memcpy(&comp[fill], curr, ss);
				fill += ss;
				seed_valid = 1;
			}

			/* curr becomes prev */
			tmp = prev; prev = curr; curr = tmp;
			sp += stride;
			y++;
		}
	}
	/* And flush */
	if (fill) {
		fz_write_printf(ctx, out, "\033*b%dW", fill);
		fz_write_data(ctx, out, comp, fill);
	}

	/* End Raster Graphics */
	fz_write_string(ctx, out, "\033*rC");
}

static void
color_pcl_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *sp)
{
	color_pcl_band_writer *writer = (color_pcl_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int xres = writer->super.xres;
	int cw;
	int x;

	if (!out)
		return;

	if (band_start+band_height >= h)
		band_height = h - band_start;

	/* We have to specify image output size in decipoints (720dpi).
	 * Most usual PCL resolutions are a multiple of 75.
	 * Pick our maximum column size to be 10800 = 15*720 = 144*75
	 * to give us good results. 10800 * 3 = 32400 < 32760 */
	cw = 10800; /* Limited by how much rowdata we can send at once */
	if (cw > w)
		cw = w;

	for (x = 0; x*cw < w; x++)
	{
		int col_w = w - cw*x;
		if (col_w > cw)
			col_w = cw;

		/* Top left corner */
		fz_write_printf(ctx, out, "\033*p%dx%dY", x*cw, band_start);

		/* Raster height */
		fz_write_printf(ctx, out, "\033*r%dT", band_height);

		/* Raster width */
		fz_write_printf(ctx, out, "\033*r%dS", col_w);

		/* Destination height */
		fz_write_printf(ctx, out, "\033*t%dV", band_height*720/xres);

		/* Destination width */
		fz_write_printf(ctx, out, "\033*t%dH", col_w*720/xres);

		/* start raster graphics */
		/* 1 = start at cursor position */
		fz_write_string(ctx, out, "\033*r3A");

		/* Now output the actual bitmap */
		/* Adaptive Compression */
		fz_write_string(ctx, out, "\033*b5M");

		color_pcl_compress_column(ctx, writer, sp + x * cw * 3, col_w, band_height, stride);
	}
}

static void
color_pcl_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
}

static void
color_pcl_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	color_pcl_band_writer *writer = (color_pcl_band_writer *)writer_;
	fz_free(ctx, writer->linebuf);
}

fz_band_writer *fz_new_color_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options)
{
	color_pcl_band_writer *writer = fz_new_band_writer(ctx, color_pcl_band_writer, out);

	writer->super.header = color_pcl_write_header;
	writer->super.band = color_pcl_write_band;
	writer->super.trailer = color_pcl_write_trailer;
	writer->super.drop = color_pcl_drop_band_writer;

	if (options)
		writer->options = *options;
	else
		fz_pcl_preset(ctx, &writer->options, "generic");

	return &writer->super;
}

/*
 * Mode 2 Row compression routine for the HP DeskJet & LaserJet IIp.
 * Compresses data from row up to end_row, storing the result
 * starting at out. Returns the number of bytes stored.
 * Runs of K<=127 literal bytes are encoded as K-1 followed by
 * the bytes; runs of 2<=K<=127 identical bytes are encoded as
 * 257-K followed by the byte.
 * In the worst case, the result is N+(N/127)+1 bytes long,
 * where N is the original byte count (end_row - row).
 */
int
mode2compress(unsigned char *out, const unsigned char *in, int in_len)
{
	int x;
	int out_len = 0;
	int run;

	for (x = 0; x < in_len; x += run)
	{
		/* How far do we have to look to find a value that isn't repeated? */
		for (run = 1; run < 127 && x+run < in_len; run++)
			if (in[0] != in[run])
				break;
		if (run > 1)
		{
			/* We have a run of matching bytes */
			out[out_len++] = 257-run;
			out[out_len++] = in[0];
		}
		else
		{
			/* Now copy as many literals as possible. We only
			 * break the run at a length of 127, at the end,
			 * or where we have 3 repeated values. */
			int i;

			/* How many literals do we need to copy? */
			for (; run < 127 && x+run+2 < in_len; run++)
				if (in[run] == in[run+1] && in[run] == in[run+2])
					break;
			/* Don't leave stragglers at the end */
			if (x + run + 2 >= in_len)
			{
				run = in_len - x;
				if (run > 127)
					run = 127;
			}
			out[out_len++] = run-1;
			for (i = 0; i < run; i++)
			{
				out[out_len++] = in[i];
			}
		}
		in += run;
	}

	return out_len;
}

/*
 * Mode 3 compression routine for the HP LaserJet III family.
 * Compresses bytecount bytes starting at current, storing the result
 * in compressed, comparing against and updating previous.
 * Returns the number of bytes stored.	In the worst case,
 * the number of bytes is bytecount+(bytecount/8)+1.
 */
int
mode3compress(unsigned char *out, const unsigned char *in, unsigned char *prev, int in_len)
{
	unsigned char *compressed = out;
	const unsigned char *cur = in;
	const unsigned char *end = in + in_len;

	while (cur < end) {		/* Detect a maximum run of unchanged bytes. */
		const unsigned char *run = cur;
		const unsigned char *diff;
		const unsigned char *stop;
		int offset, cbyte;

		while (cur < end && *cur == *prev) {
			cur++, prev++;
		}
		if (cur == end)
			break;		/* rest of row is unchanged */
		/* Detect a run of up to 8 changed bytes. */
		/* We know that *cur != *prev. */
		diff = cur;
		stop = (end - cur > 8 ? cur + 8 : end);
		do
		{
			*prev++ = *cur++;
		}
		while (cur < stop && *cur != *prev);
		/* Now [run..diff) are unchanged, and */
		/* [diff..cur) are changed. */
		/* Generate the command byte(s). */
		offset = diff - run;
		cbyte = (cur - diff - 1) << 5;
		if (offset < 31)
			*out++ = cbyte + offset;
		else {
			*out++ = cbyte + 31;
			offset -= 31;
			while (offset >= 255)
				*out++ = 255, offset -= 255;
			*out++ = offset;
		}
		/* Copy the changed data. */
		while (diff < cur)
			*out++ = *diff++;
	}
	return out - compressed;
}

void
fz_write_bitmap_as_pcl(fz_context *ctx, fz_output *out, const fz_bitmap *bitmap, const fz_pcl_options *pcl)
{
	fz_band_writer *writer;

	if (!bitmap || !out)
		return;

	writer = fz_new_mono_pcl_band_writer(ctx, out, pcl);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, bitmap->w, bitmap->h, 1, 0, bitmap->xres, bitmap->yres, 0, NULL, NULL);
		fz_write_band(ctx, writer, bitmap->stride, bitmap->h, bitmap->samples);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

typedef struct mono_pcl_band_writer_s
{
	fz_band_writer super;
	fz_pcl_options options;
	unsigned char *prev;
	unsigned char *mode2buf;
	unsigned char *mode3buf;
	int top_of_page;
	int num_blank_lines;
} mono_pcl_band_writer;

static void
mono_pcl_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	mono_pcl_band_writer *writer = (mono_pcl_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int xres = writer->super.xres;
	int yres = writer->super.yres;
	int line_size;
	int max_mode_2_size;
	int max_mode_3_size;

	if (writer->super.alpha != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "mono PCL cannot write alpha channel");
	if (writer->super.s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "mono PCL cannot write spot colors");
	if (writer->super.n != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "mono PCL must be grayscale");

	line_size = (w + 7)/8;
	max_mode_2_size = line_size + (line_size/127) + 1;
	max_mode_3_size = line_size + (line_size/8) + 1;

	writer->prev = fz_calloc(ctx, line_size, sizeof(unsigned char));
	writer->mode2buf = fz_calloc(ctx, max_mode_2_size, sizeof(unsigned char));
	writer->mode3buf = fz_calloc(ctx, max_mode_3_size, sizeof(unsigned char));
	writer->num_blank_lines = 0;
	writer->top_of_page = 1;

	guess_paper_size(&writer->options, w, h, xres, yres);

	if (writer->options.features & HACK__IS_A_OCE9050)
	{
		/* Enter HPGL/2 mode, begin plot, Initialise (start plot), Enter PCL mode */
		fz_write_string(ctx, out, "\033%1BBPIN;\033%1A");
	}

	pcl_header(ctx, out, &writer->options, 1, xres, yres, w, h);
}

static void
mono_pcl_write_band(fz_context *ctx, fz_band_writer *writer_, int ss, int band_start, int band_height, const unsigned char *data)
{
	mono_pcl_band_writer *writer = (mono_pcl_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int yres = writer->super.yres;
	const unsigned char *out_data;
	int y, rmask, line_size;
	int num_blank_lines;
	int compression = -1;
	unsigned char *prev = NULL;
	unsigned char *mode2buf = NULL;
	unsigned char *mode3buf = NULL;
	int out_count;
	const fz_pcl_options *pcl;

	if (!out)
		return;

	num_blank_lines = writer->num_blank_lines;
	rmask = ~0 << (-w & 7);
	line_size = (w + 7)/8;
	prev = writer->prev;
	mode2buf = writer->mode2buf;
	mode3buf = writer->mode3buf;
	pcl = &writer->options;

	/* Transfer raster graphics. */
	for (y = 0; y < band_height; y++, data += ss)
	{
		const unsigned char *end_data = data + line_size;

		if ((end_data[-1] & rmask) == 0)
		{
			end_data--;
			while (end_data > data && end_data[-1] == 0)
				end_data--;
		}
		if (end_data == data)
		{
			/* Blank line */
			num_blank_lines++;
			continue;
		}

		/* We've reached a non-blank line. */
		/* Put out a spacing command if necessary. */
		if (writer->top_of_page)
		{
			writer->top_of_page = 0;
			/* We're at the top of a page. */
			if (pcl->features & PCL_ANY_SPACING)
			{
				if (num_blank_lines > 0)
					fz_write_printf(ctx, out, "\033*p+%dY", num_blank_lines);
				/* Start raster graphics. */
				fz_write_string(ctx, out, "\033*r1A");
			}
			else if (pcl->features & PCL_MODE_3_COMPRESSION)
			{
				/* Start raster graphics. */
				fz_write_string(ctx, out, "\033*r1A");
				for (; num_blank_lines; num_blank_lines--)
					fz_write_string(ctx, out, "\033*b0W");
			}
			else
			{
				/* Start raster graphics. */
				fz_write_string(ctx, out, "\033*r1A");
				for (; num_blank_lines; num_blank_lines--)
					fz_write_string(ctx, out, "\033*bW");
			}
		}

		/* Skip blank lines if any */
		else if (num_blank_lines != 0)
		{
			/* Moving down from current position causes head
			 * motion on the DeskJet, so if the number of lines
			 * is small, we're better off printing blanks.
			 *
			 * For Canon LBP4i and some others, <ESC>*b<n>Y
			 * doesn't properly clear the seed row if we are in
			 * compression mode 3.
			 */
			if ((num_blank_lines < MIN_SKIP_LINES && compression != 3) ||
					!(pcl->features & PCL_ANY_SPACING))
			{
				int mode_3ns = ((pcl->features & PCL_MODE_3_COMPRESSION) && !(pcl->features & PCL_ANY_SPACING));
				if (mode_3ns && compression != 2)
				{
					/* Switch to mode 2 */
					fz_write_string(ctx, out, from3to2);
					compression = 2;
				}
				if (pcl->features & PCL_MODE_3_COMPRESSION)
				{
					/* Must clear the seed row. */
					fz_write_string(ctx, out, "\033*b1Y");
					num_blank_lines--;
				}
				if (mode_3ns)
				{
					for (; num_blank_lines; num_blank_lines--)
						fz_write_string(ctx, out, "\033*b0W");
				}
				else
				{
					for (; num_blank_lines; num_blank_lines--)
						fz_write_string(ctx, out, "\033*bW");
				}
			}
			else if (pcl->features & PCL3_SPACING)
				fz_write_printf(ctx, out, "\033*p+%dY", num_blank_lines * yres);
			else
				fz_write_printf(ctx, out, "\033*b%dY", num_blank_lines);
			/* Clear the seed row (only matters for mode 3 compression). */
			memset(prev, 0, line_size);
		}
		num_blank_lines = 0;

		/* Choose the best compression mode for this particular line. */
		if (pcl->features & PCL_MODE_3_COMPRESSION)
		{
			/* Compression modes 2 and 3 are both available. Try
			 * both and see which produces the least output data.
			 */
			int count3 = mode3compress(mode3buf, data, prev, line_size);
			int count2 = mode2compress(mode2buf, data, line_size);
			int penalty3 = (compression == 3 ? 0 : penalty_from2to3);
			int penalty2 = (compression == 2 ? 0 : penalty_from3to2);

			if (count3 + penalty3 < count2 + penalty2)
			{
				if (compression != 3)
					fz_write_string(ctx, out, from2to3);
				compression = 3;
				out_data = (unsigned char *)mode3buf;
				out_count = count3;
			}
			else
			{
				if (compression != 2)
					fz_write_string(ctx, out, from3to2);
				compression = 2;
				out_data = (unsigned char *)mode2buf;
				out_count = count2;
			}
		}
		else if (pcl->features & PCL_MODE_2_COMPRESSION)
		{
			out_data = mode2buf;
			out_count = mode2compress(mode2buf, data, line_size);
		}
		else
		{
			out_data = data;
			out_count = line_size;
		}

		/* Transfer the data */
		fz_write_printf(ctx, out, "\033*b%dW", out_count);
		fz_write_data(ctx, out, out_data, out_count);
	}

	writer->num_blank_lines = num_blank_lines;
}

static void
mono_pcl_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
	mono_pcl_band_writer *writer = (mono_pcl_band_writer *)writer_;
	fz_output *out = writer->super.out;

	/* end raster graphics and eject page */
	fz_write_string(ctx, out, "\033*rB\f");

	if (writer->options.features & HACK__IS_A_OCE9050)
	{
		/* Pen up, pen select, advance full page, reset */
		fz_write_string(ctx, out, "\033%1BPUSP0PG;\033E");
	}
}

static void
mono_pcl_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	mono_pcl_band_writer *writer = (mono_pcl_band_writer *)writer_;

	fz_free(ctx, writer->prev);
	fz_free(ctx, writer->mode2buf);
	fz_free(ctx, writer->mode3buf);
}

fz_band_writer *fz_new_mono_pcl_band_writer(fz_context *ctx, fz_output *out, const fz_pcl_options *options)
{
	mono_pcl_band_writer *writer = fz_new_band_writer(ctx, mono_pcl_band_writer, out);

	writer->super.header = mono_pcl_write_header;
	writer->super.band = mono_pcl_write_band;
	writer->super.trailer = mono_pcl_write_trailer;
	writer->super.drop = mono_pcl_drop_band_writer;

	if (options)
		writer->options = *options;
	else
		fz_pcl_preset(ctx, &writer->options, "generic");

	return &writer->super;
}

void
fz_save_pixmap_as_pcl(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pcl_options *pcl)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		fz_write_pixmap_as_pcl(ctx, out, pixmap, pcl);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_save_bitmap_as_pcl(fz_context *ctx, fz_bitmap *bitmap, char *filename, int append, const fz_pcl_options *pcl)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		fz_write_bitmap_as_pcl(ctx, out, bitmap, pcl);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* High-level document writer interface */

typedef struct fz_pcl_writer_s fz_pcl_writer;

struct fz_pcl_writer_s
{
	fz_document_writer super;
	fz_draw_options draw;
	fz_pcl_options pcl;
	fz_pixmap *pixmap;
	int mono;
	fz_output *out;
};

static fz_device *
pcl_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_pcl_writer *wri = (fz_pcl_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->draw, mediabox, &wri->pixmap);
}

static void
pcl_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_pcl_writer *wri = (fz_pcl_writer*)wri_;
	fz_bitmap *bitmap = NULL;

	fz_var(bitmap);

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		if (wri->mono)
		{
			bitmap = fz_new_bitmap_from_pixmap(ctx, wri->pixmap, NULL);
			fz_write_bitmap_as_pcl(ctx, wri->out, bitmap, &wri->pcl);
		}
		else
		{
			fz_write_pixmap_as_pcl(ctx, wri->out, wri->pixmap, &wri->pcl);
		}
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_bitmap(ctx, bitmap);
		fz_drop_pixmap(ctx, wri->pixmap);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pcl_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pcl_writer *wri = (fz_pcl_writer*)wri_;
	fz_close_output(ctx, wri->out);
}

static void
pcl_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pcl_writer *wri = (fz_pcl_writer*)wri_;
	fz_drop_pixmap(ctx, wri->pixmap);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_pcl_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_pcl_writer *wri = fz_new_derived_document_writer(ctx, fz_pcl_writer, pcl_begin_page, pcl_end_page, pcl_close_writer, pcl_drop_writer);
	const char *val;

	fz_try(ctx)
	{
		fz_parse_draw_options(ctx, &wri->draw, options);
		fz_parse_pcl_options(ctx, &wri->pcl, options);
		if (fz_has_option(ctx, options, "colorspace", &val))
			if (fz_option_eq(val, "mono"))
				wri->mono = 1;
		wri->out = out;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_pcl_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.pcl", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_pcl_writer_with_output(ctx, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}
