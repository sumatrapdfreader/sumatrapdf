/*
 * Copyright (c) 2002-2003 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * Copyright (c) 2001-2003 Michael David Adams
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/******************************************************************************\
* Includes
\******************************************************************************/

#include <jasper/jasper.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>

/******************************************************************************\
*
\******************************************************************************/

#define MAXCMPTS	256
#define BIGPAN		0.90
#define SMALLPAN	0.05
#define	BIGZOOM		2.0
#define	SMALLZOOM	1.41421356237310

#define	min(x, y)	(((x) < (y)) ? (x) : (y))
#define	max(x, y)	(((x) > (y)) ? (x) : (y))

typedef struct {

	/* The number of image files to view. */
	int numfiles;

	/* The names of the image files. */
	char **filenames;

	/* The title for the window. */
	char *title;

	/* The time to wait before advancing to the next image (in ms). */
	int tmout;

	/* Loop indefinitely over all images. */
	int loop;

	int verbose;

} cmdopts_t;

typedef struct {

	int width;

	int height;

	GLshort *data;

} pixmap_t;

typedef struct {

	/* The index of the current image file. */
	int filenum;

	/* The image. */
	jas_image_t *image;
	jas_image_t *altimage;

	/* The x-coordinate of viewport center. */
	float vcx;

	/* The y-coordinate of viewport center. */
	float vcy;

	/* The x scale factor. */
	float sx;

	/* The y scale factor. */
	float sy;

	/* The viewport pixmap buffer. */
	pixmap_t vp;

	/* The active timer ID. */
	int activetmid;

	/* The next available timer ID. */
	int nexttmid;

	int monomode;

	int cmptno;

	int dirty;

} gs_t;

/******************************************************************************\
*
\******************************************************************************/

static void display(void);
static void reshape(int w, int h);
static void keyboard(unsigned char key, int x, int y);
static void special(int key, int x, int y);
static void timer(int value);

static void usage(void);
static void nextimage(void);
static void previmage(void);
static void nextcmpt(void);
static void prevcmpt(void);
static int loadimage(void);
static void unloadimage(void);
static void adjust(void);
static int jas_image_render2(jas_image_t *image, int cmptno, float vtlx, float vtly,
  float vsx, float vsy, int vw, int vh, GLshort *vdata);
static int jas_image_render(jas_image_t *image, float vtlx, float vtly,
  float vsx, float vsy, int vw, int vh, GLshort *vdata);

static void dumpstate(void);
static int pixmap_resize(pixmap_t *p, int w, int h);
static void pixmap_clear(pixmap_t *p);
static void cmdinfo(void);

static void cleanupandexit(int);
static void init(void);

/******************************************************************************\
*
\******************************************************************************/

jas_opt_t opts[] = {
	{'V', "version", 0},
	{'v', "v", 0},
	{'h', "help", 0},
	{'w', "wait", JAS_OPT_HASARG},
	{'l', "loop", 0},
	{'t', "title", JAS_OPT_HASARG},
	{-1, 0, 0}
};

char *cmdname = 0;
cmdopts_t cmdopts;
gs_t gs;
jas_stream_t *streamin = 0;

/******************************************************************************\
*
\******************************************************************************/

int main(int argc, char **argv)
{
	int c;

	init();

	/* Determine the base name of this command. */
	if ((cmdname = strrchr(argv[0], '/'))) {
		++cmdname;
	} else {
		cmdname = argv[0];
	}

	/* Initialize the JasPer library. */
	if (jas_init()) {
		abort();
	}

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
	glutCreateWindow(cmdname);
	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutSpecialFunc(special);
	glutKeyboardFunc(keyboard);

	cmdopts.numfiles = 0;
	cmdopts.filenames = 0;
	cmdopts.title = 0;
	cmdopts.tmout = 0;
	cmdopts.loop = 0;
	cmdopts.verbose = 0;

	while ((c = jas_getopt(argc, argv, opts)) != EOF) {
		switch (c) {
		case 'w':
			cmdopts.tmout = atof(jas_optarg) * 1000;
			break;
		case 'l':
			cmdopts.loop = 1;
			break;
		case 't':
			cmdopts.title = jas_optarg;
			break;
		case 'v':
			cmdopts.verbose = 1;
			break;
		case 'V':
			jas_eprintf("%s\n", JAS_VERSION);
			jas_eprintf("libjasper %s\n", jas_getversion());
			cleanupandexit(EXIT_SUCCESS);
			break;
		default:
		case 'h':
			usage();
			break;
		}
	}

	if (jas_optind < argc) {
		/* The images are to be read from one or more explicitly named
		  files. */
		cmdopts.numfiles = argc - jas_optind;
		cmdopts.filenames = &argv[jas_optind];
	} else {
		/* The images are to be read from standard input. */
		static char *null = 0;
		cmdopts.filenames = &null;
		cmdopts.numfiles = 1;
	}

	streamin = jas_stream_fdopen(0, "rb");

	/* Load the next image. */
	nextimage();

	/* Start the GLUT main event handler loop. */
	glutMainLoop();

	return EXIT_SUCCESS;
}

/******************************************************************************\
*
\******************************************************************************/

static void cmdinfo()
{
	jas_eprintf("JasPer Image Viewer (Version %s).\n",
	  JAS_VERSION);
	jas_eprintf("Copyright (c) 2002-2003 Michael David Adams.\n"
	  "All rights reserved.\n");
	jas_eprintf("%s\n", JAS_NOTES);
}

static char *helpinfo[] = {
"The following options are supported:\n",
"    --help                  Print this help information and exit.\n",
"    --version               Print version information and exit.\n",
"    --loop                  Loop indefinitely through images.\n",
"    --wait N                Advance to next image after N seconds.\n",
0
};

static void usage()
{
	char *s;
	int i;
	cmdinfo();
	jas_eprintf("usage: %s [options] [file1 file2 ...]\n", cmdname);
	for (i = 0, s = helpinfo[i]; s; ++i, s = helpinfo[i]) {
		jas_eprintf("%s", s);
	}
	cleanupandexit(EXIT_FAILURE);
}

/******************************************************************************\
* GLUT Callback Functions
\******************************************************************************/

/* Display callback function. */

static void display()
{
	float vtlx;
	float vtly;

	if (cmdopts.verbose) {
		jas_eprintf("display()\n");
		dumpstate();
	}

	if (!gs.dirty) {
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawPixels(gs.vp.width, gs.vp.height, GL_RGBA,
		  GL_UNSIGNED_SHORT, gs.vp.data);
		glFlush();
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT);
	pixmap_clear(&gs.vp);
	glDrawPixels(gs.vp.width, gs.vp.height, GL_RGBA, GL_UNSIGNED_SHORT,
	  gs.vp.data);
	glFlush();

	vtlx = gs.vcx - 0.5 * gs.sx * gs.vp.width;
	vtly = gs.vcy - 0.5 * gs.sy * gs.vp.height;
	if (cmdopts.verbose) {
		jas_eprintf("vtlx=%f, vtly=%f, vsx=%f, vsy=%f\n",
		  vtlx, vtly, gs.sx, gs.sy);
	}
	if (gs.monomode) {
		if (cmdopts.verbose) {
			jas_eprintf("component %d\n", gs.cmptno);
		}
		jas_image_render2(gs.image, gs.cmptno, vtlx, vtly,
		  gs.sx, gs.sy, gs.vp.width, gs.vp.height, gs.vp.data);
	} else {
		if (cmdopts.verbose) {
			jas_eprintf("color\n");
		}
		jas_image_render(gs.altimage, vtlx, vtly, gs.sx, gs.sy,
		  gs.vp.width, gs.vp.height, gs.vp.data);
	}
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawPixels(gs.vp.width, gs.vp.height, GL_RGBA, GL_UNSIGNED_SHORT,
	  gs.vp.data);
	glFlush();
	gs.dirty = 0;
}

/* Reshape callback function. */

static void reshape(int w, int h)
{
	if (cmdopts.verbose) {
		jas_eprintf("reshape(%d, %d)\n", w, h);
		dumpstate();
	}

	if (pixmap_resize(&gs.vp, w, h)) {
		cleanupandexit(EXIT_FAILURE);
	}
	pixmap_clear(&gs.vp);
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho( 0, w, 0, h, 0.f, 1.f );

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, 0);
	glRasterPos2i(0, 0);

	if (gs.vp.width > jas_image_width(gs.image) / gs.sx) {
		gs.vcx = (jas_image_tlx(gs.image) + jas_image_brx(gs.image)) / 2.0;
	}
	if (gs.vp.height > jas_image_height(gs.image) / gs.sy) {
		gs.vcy = (jas_image_tly(gs.image) + jas_image_bry(gs.image)) / 2.0;
	}
	gs.dirty = 1;
}

/* Keyboard callback function. */

static void keyboard(unsigned char key, int x, int y)
{
	if (cmdopts.verbose) {
		jas_eprintf("keyboard(%d, %d, %d)\n", key, x, y);
	}

	switch (key) {
	case ' ':
		nextimage();
		break;
	case '\b':
		previmage();
		break;
	case '>':
		gs.sx /= BIGZOOM;
		gs.sy /= BIGZOOM;
		gs.dirty = 1;
		glutPostRedisplay();
		break;
	case '.':
		gs.sx /= SMALLZOOM;
		gs.sy /= SMALLZOOM;
		gs.dirty = 1;
		glutPostRedisplay();
		break;
	case '<':
		gs.sx *= BIGZOOM;
		gs.sy *= BIGZOOM;
		adjust();
		gs.dirty = 1;
		glutPostRedisplay();
		break;
	case ',':
		gs.sx *= SMALLZOOM;
		gs.sy *= SMALLZOOM;
		gs.dirty = 1;
		adjust();
		glutPostRedisplay();
		break;
	case 'c':
		nextcmpt();
		break;
	case 'C':
		prevcmpt();
		break;
	case 'q':
		cleanupandexit(EXIT_SUCCESS);
		break;
	}
}

/* Special keyboard callback function. */

static void special(int key, int x, int y)
{
	if (cmdopts.verbose) {
		jas_eprintf("special(%d, %d, %d)\n", key, x, y);
	}

	switch (key) {
	case GLUT_KEY_UP:
		{
			float oldvcy;
			float vh;
			float pan;
			if (gs.vp.height < jas_image_height(gs.image) / gs.sy) {
				pan = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) ?
				  BIGPAN : SMALLPAN;
				oldvcy = gs.vcy;
				vh = gs.sy * gs.vp.height;
				gs.vcy = max(gs.vcy - pan * vh, jas_image_tly(gs.image) +
				  0.5 * vh);
				if (gs.vcy != oldvcy) {
					gs.dirty = 1;
					glutPostRedisplay();
				}
			}
		}
		break;
	case GLUT_KEY_DOWN:
		{
			float oldvcy;
			float vh;
			float pan;
			if (gs.vp.height < jas_image_height(gs.image) / gs.sy) {
				pan = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) ?
				  BIGPAN : SMALLPAN;
				oldvcy = gs.vcy;
				vh = gs.sy * gs.vp.height;
				gs.vcy = min(gs.vcy + pan * vh, jas_image_bry(gs.image) -
				  0.5 * vh);
				if (gs.vcy != oldvcy) {
					gs.dirty = 1;
					glutPostRedisplay();
				}
			}
		}
		break;
	case GLUT_KEY_LEFT:
		{
			float oldvcx;
			float vw;
			float pan;
			if (gs.vp.width < jas_image_width(gs.image) / gs.sx) {
				pan = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) ?
				  BIGPAN : SMALLPAN;
				oldvcx = gs.vcx;
				vw = gs.sx * gs.vp.width;
				gs.vcx = max(gs.vcx - pan * vw, jas_image_tlx(gs.image) +
				  0.5 * vw);
				if (gs.vcx != oldvcx) {
					gs.dirty = 1;
					glutPostRedisplay();
				}
			}
		}
		break;
	case GLUT_KEY_RIGHT:
		{
			float oldvcx;
			float vw;
			float pan;
			if (gs.vp.width < jas_image_width(gs.image) / gs.sx) {
				pan = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) ?
				  BIGPAN : SMALLPAN;
				oldvcx = gs.vcx;
				vw = gs.sx * gs.vp.width;
				gs.vcx = min(gs.vcx + pan * vw, jas_image_brx(gs.image) -
				  0.5 * vw);
				if (gs.vcx != oldvcx) {
					gs.dirty = 1;
					glutPostRedisplay();
				}
			}
		}
		break;
	default:
		break;
	}
}

/* Timer callback function. */

static void timer(int value)
{
	if (cmdopts.verbose) {
		jas_eprintf("timer(%d)\n", value);
	}
	if (value == gs.activetmid) {
		nextimage();
	}
}

/******************************************************************************\
*
\******************************************************************************/

static void nextcmpt()
{
	if (gs.monomode) {
		if (gs.cmptno == jas_image_numcmpts(gs.image) - 1) {
			if (gs.altimage) {
				gs.monomode = 0;
			} else {
				gs.cmptno = 0;
			}
		} else {
			++gs.cmptno;
		}
	} else {
		gs.monomode = 1;
		gs.cmptno = 0;
	}
	gs.dirty = 1;
	glutPostRedisplay();
}

static void prevcmpt()
{
	if (gs.monomode) {
		if (!gs.cmptno) {
			gs.monomode = 0;
		} else {
			--gs.cmptno;
		}
	} else {
		gs.monomode = 1;
		gs.cmptno = jas_image_numcmpts(gs.image) - 1;
	}
	gs.dirty = 1;
	glutPostRedisplay();
}

static void nextimage()
{
	int n;
	unloadimage();
	for (n = cmdopts.numfiles; n > 0; --n) {
		++gs.filenum;
		if (gs.filenum >= cmdopts.numfiles) {
			if (cmdopts.loop) {
				gs.filenum = 0;
			} else {
				cleanupandexit(EXIT_SUCCESS);
			}
		}
		if (!loadimage()) {
			return;
		}
		jas_eprintf("cannot load image\n");
	}
	cleanupandexit(EXIT_SUCCESS);
}

static void previmage()
{
	int n;
	unloadimage();
	for (n = cmdopts.numfiles; n > 0; --n) {
		--gs.filenum;
		if (gs.filenum < 0) {
			if (cmdopts.loop) {
				gs.filenum = cmdopts.numfiles - 1;
			} else {
				cleanupandexit(EXIT_SUCCESS);
			}
		}
		if (!loadimage()) {
			return;
		}
	}
	cleanupandexit(EXIT_SUCCESS);
}

static int loadimage()
{
	int reshapeflag;
	jas_stream_t *in;
	int scrnwidth;
	int scrnheight;
	int vh;
	int vw;
	char *pathname;
	jas_cmprof_t *outprof;

	assert(!gs.image);
	assert(!gs.altimage);

	gs.image = 0;
	gs.altimage = 0;

	pathname = cmdopts.filenames[gs.filenum];

	if (pathname && pathname[0] != '\0') {
#if 1
	jas_eprintf("opening %s\n", pathname);
#endif
		/* The input image is to be read from a file. */
		if (!(in = jas_stream_fopen(pathname, "rb"))) {
			jas_eprintf("error: cannot open file %s\n", pathname);
			goto error;
		}
	} else {
		/* The input image is to be read from standard input. */
		in = streamin;
	}

	/* Get the input image data. */
	if (!(gs.image = jas_image_decode(in, -1, 0))) {
		jas_eprintf("error: cannot load image data\n");
		goto error;
	}

	/* Close the input stream. */
	if (in != streamin) {
		jas_stream_close(in);
	}

	if (!(outprof = jas_cmprof_createfromclrspc(JAS_CLRSPC_SRGB)))
		goto error;
	if (!(gs.altimage = jas_image_chclrspc(gs.image, outprof, JAS_CMXFORM_INTENT_PER)))
		goto error;

	if ((scrnwidth = glutGet(GLUT_SCREEN_WIDTH)) < 0) {
		scrnwidth = 256;
	}
	if ((scrnheight = glutGet(GLUT_SCREEN_HEIGHT)) < 0) {
		scrnheight = 256;
	}

	vw = min(jas_image_width(gs.image), 0.95 * scrnwidth);
	vh = min(jas_image_height(gs.image), 0.95 * scrnheight);

	gs.vcx = (jas_image_tlx(gs.image) + jas_image_brx(gs.image)) / 2.0;
	gs.vcy = (jas_image_tly(gs.image) + jas_image_bry(gs.image)) / 2.0;
	gs.sx = 1.0;
	gs.sy = 1.0;
	if (gs.altimage) {
		gs.monomode = 0;
	} else {
		gs.monomode = 1;
		gs.cmptno = 0;
	}

#if 1
	jas_eprintf("num of components %d\n", jas_image_numcmpts(gs.image));
#endif

	if (vw < jas_image_width(gs.image)) {
		gs.sx = jas_image_width(gs.image) / ((float) vw);
	}
	if (vh < jas_image_height(gs.image)) {
		gs.sy = jas_image_height(gs.image) / ((float) vh);
	}
	if (gs.sx > gs.sy) {
		gs.sy = gs.sx;
	} else if (gs.sx < gs.sy) {
		gs.sx = gs.sy;
	}
	vw = jas_image_width(gs.image) / gs.sx;
	vh = jas_image_height(gs.image) / gs.sy;
	gs.dirty = 1;

	reshapeflag = 0;
	if (vw != glutGet(GLUT_WINDOW_WIDTH) ||
	  vh != glutGet(GLUT_WINDOW_HEIGHT)) {
		glutReshapeWindow(vw, vh);
		reshapeflag = 1;
	}
	if (cmdopts.title) {
		glutSetWindowTitle(cmdopts.title);
	} else {
		glutSetWindowTitle((pathname && pathname[0] != '\0') ? pathname :
		  "stdin");
	}
	/* If we reshaped the window, GLUT will automatically invoke both
	  the reshape and display callback (in this order).  Therefore, we
	  only need to explicitly force the display callback to be invoked
	  if the window was not reshaped. */
	if (!reshapeflag) {
		glutPostRedisplay();
	}

	if (cmdopts.tmout != 0) {
		glutTimerFunc(cmdopts.tmout, timer, gs.nexttmid);
		gs.activetmid = gs.nexttmid;
		++gs.nexttmid;
	}

	return 0;

error:
	unloadimage();
	return -1;
}

static void unloadimage()
{
	if (gs.image) {
		jas_image_destroy(gs.image);
		gs.image = 0;
	}
	if (gs.altimage) {
		jas_image_destroy(gs.altimage);
		gs.altimage = 0;
	}
}

/******************************************************************************\
*
\******************************************************************************/

static void adjust()
{
	if (gs.vp.width < jas_image_width(gs.image) / gs.sx) {
		float mnx;
		float mxx;
		mnx = jas_image_tlx(gs.image) + 0.5 * gs.vp.width * gs.sx;
		mxx = jas_image_brx(gs.image) - 0.5 * gs.vp.width * gs.sx;
		if (gs.vcx < mnx) {
			gs.vcx = mnx;
		} else if (gs.vcx > mxx) {
			gs.vcx = mxx;
		}
	} else {
		gs.vcx = (jas_image_tlx(gs.image) + jas_image_brx(gs.image)) / 2.0;
	}
	if (gs.vp.height < jas_image_height(gs.image) / gs.sy) {
		float mny;
		float mxy;
		mny = jas_image_tly(gs.image) + 0.5 * gs.vp.height * gs.sy;
		mxy = jas_image_bry(gs.image) - 0.5 * gs.vp.height * gs.sy;
		if (gs.vcy < mny) {
			gs.vcy = mny;
		} else if (gs.vcy > mxy) {
			gs.vcy = mxy;
		}
	} else {
		gs.vcy = (jas_image_tly(gs.image) + jas_image_bry(gs.image)) / 2.0;
	}
}

static void pixmap_clear(pixmap_t *p)
{
	memset(p->data, 0, 4 * p->width * p->height * sizeof(GLshort));
}

static int pixmap_resize(pixmap_t *p, int w, int h)
{
	p->width = w;
	p->height = h;
	if (!(p->data = realloc(p->data, w * h * 4 * sizeof(GLshort)))) {
		return -1;
	}
	return 0;
}

static void dumpstate()
{
	printf("vcx=%f vcy=%f sx=%f sy=%f dirty=%d\n", gs.vcx, gs.vcy, gs.sx, gs.sy, gs.dirty);
}

#define	vctocc(i, co, cs, vo, vs) \
  (((vo) + (i) * (vs) - (co)) / (cs))

static int jas_image_render(jas_image_t *image, float vtlx, float vtly,
  float vsx, float vsy, int vw, int vh, GLshort *vdata)
{
	int i;
	int j;
	int k;
	int x;
	int y;
	int v[3];
	GLshort *vdatap;
	int cmptlut[3];
	int width;
	int height;
	int hs;
	int vs;
	int tlx;
	int tly;

	if ((cmptlut[0] = jas_image_getcmptbytype(image,
	  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R))) < 0 ||
	  (cmptlut[1] = jas_image_getcmptbytype(image,
	  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G))) < 0 ||
	  (cmptlut[2] = jas_image_getcmptbytype(image,
	  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B))) < 0)
		goto error;
	width = jas_image_cmptwidth(image, cmptlut[0]);
	height = jas_image_cmptheight(image, cmptlut[0]);
	tlx = jas_image_cmpttlx(image, cmptlut[0]);
	tly = jas_image_cmpttly(image, cmptlut[0]);
	vs = jas_image_cmptvstep(image, cmptlut[0]);
	hs = jas_image_cmpthstep(image, cmptlut[0]);
	for (i = 1; i < 3; ++i) {
		if (jas_image_cmptwidth(image, cmptlut[i]) != width ||
		  jas_image_cmptheight(image, cmptlut[i]) != height)
			goto error;
	}
	for (i = 0; i < vh; ++i) {
		vdatap = &vdata[(vh - 1 - i) * (4 * vw)];
		for (j = 0; j < vw; ++j) {
			x = vctocc(j, tlx, hs, vtlx, vsx);
			y = vctocc(i, tly, vs, vtly, vsy);
			if (x >= 0 && x < width && y >= 0 && y < height) {
				for (k = 0; k < 3; ++k) {
					v[k] = jas_image_readcmptsample(image, cmptlut[k], x, y);
					v[k] <<= 16 - jas_image_cmptprec(image, cmptlut[k]);
					if (v[k] < 0) {
						v[k] = 0;
					} else if (v[k] > 65535) {
						v[k] = 65535;
					}
				}
			} else {
				v[0] = 0;
				v[1] = 0;
				v[2] = 0;
			}	
			*vdatap++ = v[0];
			*vdatap++ = v[1];
			*vdatap++ = v[2];
			*vdatap++ = 0;
		}
	}
	return 0;
error:
	return -1;
}

int jas_image_render2(jas_image_t *image, int cmptno, float vtlx, float vtly,
  float vsx, float vsy, int vw, int vh, GLshort *vdata)
{
	int i;
	int j;
	int x;
	int y;
	int v;
	GLshort *vdatap;

	if (cmptno < 0 || cmptno >= image->numcmpts_) {
		jas_eprintf("bad parameter\n");
		goto error;
	}
	for (i = 0; i < vh; ++i) {
		vdatap = &vdata[(vh - 1 - i) * (4 * vw)];
		for (j = 0; j < vw; ++j) {
			x = vctocc(j, jas_image_cmpttlx(image, cmptno), jas_image_cmpthstep(image, cmptno), vtlx, vsx);
			y = vctocc(i, jas_image_cmpttly(image, cmptno), jas_image_cmptvstep(image, cmptno), vtly, vsy);
			v = (x >= 0 && x < jas_image_cmptwidth(image, cmptno) && y >=0 && y < jas_image_cmptheight(image, cmptno)) ? jas_image_readcmptsample(image, cmptno, x, y) : 0;
			v <<= 16 - jas_image_cmptprec(image, cmptno);
			if (v < 0) {
				v = 0;
			} else if (v > 65535) {
				v = 65535;
			}
			*vdatap++ = v;
			*vdatap++ = v;
			*vdatap++ = v;
			*vdatap++ = 0;
		}
	}
	return 0;
error:
	return -1;
}

#if 0

#define	vctocc(i, co, cs, vo, vs) \
  (((vo) + (i) * (vs) - (co)) / (cs))

static void drawview(jas_image_t *image, float vtlx, float vtly,
  float sx, float sy, pixmap_t *p)
{
	int i;
	int j;
	int k;
	int red;
	int grn;
	int blu;
	int lum;
	GLshort *datap;
	int x;
	int y;
	int *cmptlut;
	int numcmpts;
	int v[4];
	int u[4];
	int color;

	cmptlut = gs.cmptlut;
	switch (jas_image_colorspace(gs.image)) {
	case JAS_IMAGE_CS_RGB:
	case JAS_IMAGE_CS_YCBCR:
		color = 1;
		numcmpts = 3;
		break;
	case JAS_IMAGE_CS_GRAY:
	default:
		numcmpts = 1;
		color = 0;
		break;
	}

	for (i = 0; i < p->height; ++i) {
		datap = &p->data[(p->height - 1 - i) * (4 * p->width)];
		for (j = 0; j < p->width; ++j) {
			if (!gs.monomode && color) {
				for (k = 0; k < numcmpts; ++k) {
					x = vctocc(j, jas_image_cmpttlx(gs.image, cmptlut[k]), jas_image_cmpthstep(gs.image, cmptlut[k]), vtlx, sx);
					y = vctocc(i, jas_image_cmpttly(gs.image, cmptlut[k]), jas_image_cmptvstep(gs.image, cmptlut[k]), vtly, sy);
					v[k] = (x >= 0 && x < jas_image_cmptwidth(gs.image, cmptlut[k]) && y >=0 && y < jas_image_cmptheight(gs.image, cmptlut[k])) ? jas_matrix_get(gs.cmpts[cmptlut[k]], y, x) : 0;
					v[k] <<= 16 - jas_image_cmptprec(gs.image, cmptlut[k]);
				}
				switch (jas_image_colorspace(gs.image)) {
				case JAS_IMAGE_CS_RGB:
					break;
				case JAS_IMAGE_CS_YCBCR:
					u[0] = (1/1.772) * (v[0] + 1.402 * v[2]);
					u[1] = (1/1.772) * (v[0] - 0.34413 * v[1] - 0.71414 * v[2]);
					u[2] = (1/1.772) * (v[0] + 1.772 * v[1]);
					v[0] = u[0];
					v[1] = u[1];
					v[2] = u[2];
					break;
				}
			} else {
				x = vctocc(j, jas_image_cmpttlx(gs.image, gs.cmptno), jas_image_cmpthstep(gs.image, gs.cmptno), vtlx, sx);
				y = vctocc(i, jas_image_cmpttly(gs.image, gs.cmptno), jas_image_cmptvstep(gs.image, gs.cmptno), vtly, sy);
				v[0] = (x >= 0 && x < jas_image_cmptwidth(gs.image, gs.cmptno) && y >=0 && y < jas_image_cmptheight(gs.image, gs.cmptno)) ? jas_matrix_get(gs.cmpts[gs.cmptno], y, x) : 0;
				v[0] <<= 16 - jas_image_cmptprec(gs.image, gs.cmptno);
				v[1] = v[0];
				v[2] = v[0];
				v[3] = 0;
			}

for (k = 0; k < 3; ++k) {
	if (v[k] < 0) {
		v[k] = 0;
	} else if (v[k] > 65535) {
		v[k] = 65535;
	}
}

			*datap++ = v[0];
			*datap++ = v[1];
			*datap++ = v[2];
			*datap++ = 0;
		}
	}
}

#endif

static void cleanupandexit(int status)
{
	unloadimage();
	exit(status);
}

static void init()
{
	gs.filenum = -1;
	gs.image = 0;
	gs.altimage = 0;
	gs.nexttmid = 0;
	gs.vp.width = 0;
	gs.vp.height = 0;
	gs.vp.data = 0;
	gs.dirty = 1;
}
