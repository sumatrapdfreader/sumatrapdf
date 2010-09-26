#ifndef _FITZ_H_
#define _FITZ_H_

#ifdef DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

/*
 * Include the standard libc headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <assert.h>
#include <errno.h>
#include <float.h>	/* FLT_EPSILON */
#include <fcntl.h>	/* O_RDONLY & co */

#define nil ((void*)0)

#define nelem(x) (sizeof(x)/sizeof((x)[0]))

/*
 * Some differences in libc can be smoothed over
 */

#ifdef _MSC_VER /* Microsoft Visual C */

#pragma warning( disable: 4244 ) /* conversion from X to Y, possible loss of data */
#pragma warning( disable: 4996 ) /* The POSIX name for this item is deprecated */
#pragma warning( disable: 4996 ) /* This function or variable may be unsafe */

#include <io.h>

int gettimeofday(struct timeval *tv, struct timezone *tz);

#define snprintf _snprintf
#define strtoll _strtoi64

#else /* Unix or close enough */

#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/*
 * Variadic macros, inline and restrict keywords
 */

#if __STDC_VERSION__ == 199901L /* C99 */

#define fz_throw(...) fz_throwimp(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define fz_rethrow(cause, ...) fz_rethrowimp(__FILE__, __LINE__, __func__, cause, __VA_ARGS__)
#define fz_catch(cause, ...) fz_catchimp(__FILE__, __LINE__, __func__, cause, __VA_ARGS__)

#elif _MSC_VER >= 1500 /* MSVC 9 or newer */

#define inline __inline
#define restrict __restrict
#define fz_throw(...) fz_throwimp(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define fz_rethrow(cause, ...) fz_rethrowimp(__FILE__, __LINE__, __FUNCTION__, cause, __VA_ARGS__)
#define fz_catch(cause, ...) fz_catchimp(__FILE__, __LINE__, __FUNCTION__, cause, __VA_ARGS__)

#elif __GNUC__ >= 3 /* GCC 3 or newer */

#define inline __inline
#define restrict __restrict
#define fz_throw(fmt...) fz_throwimp(__FILE__, __LINE__, __FUNCTION__, fmt)
#define fz_rethrow(cause, fmt...) fz_rethrowimp(__FILE__, __LINE__, __FUNCTION__, cause, fmt)
#define fz_catch(cause, fmt...) fz_catchimp(__FILE__, __LINE__, __FUNCTION__, cause, fmt)

#else /* Unknown or ancient */

#define inline
#define restrict
#define fz_throw fz_throwimpx
#define fz_rethrow fz_rethrowimpx
#define fz_catch fz_catchimpx

#endif

/*
 * GCC can do type checking of printf strings
 */

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

/*
 * Error handling
 */

typedef int fz_error;

void fz_warn(char *fmt, ...) __printflike(1, 2);

fz_error fz_throwimp(const char *file, int line, const char *func, char *fmt, ...) __printflike(4, 5);
fz_error fz_rethrowimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...) __printflike(5, 6);
void fz_catchimp(const char *file, int line, const char *func, fz_error cause, char *fmt, ...) __printflike(5, 6);

fz_error fz_throwimpx(char *fmt, ...) __printflike(1, 2);
fz_error fz_rethrowimpx(fz_error cause, char *fmt, ...) __printflike(2, 3);
void fz_catchimpx(fz_error cause, char *fmt, ...) __printflike(2, 3);

#define fz_okay ((fz_error)0)

/*
 * Basic runtime and utility functions
 */

#define ABS(x) ( (x) < 0 ? -(x) : (x) )
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#define CLAMP(x,a,b) ( (x) > (b) ? (b) : ( (x) < (a) ? (a) : (x) ) )

/* memory allocation */
void *fz_malloc(int n);
void *fz_realloc(void *p, int n);
void fz_free(void *p);
char *fz_strdup(char *s);

/* runtime (hah!) test for endian-ness */
int fz_isbigendian(void);

/* safe string functions */
char *fz_strsep(char **stringp, const char *delim);
int fz_strlcpy(char *dst, const char *src, int n);
int fz_strlcat(char *dst, const char *src, int n);

/* utf-8 encoding and decoding */
int chartorune(int *rune, char *str);
int runetochar(char *str, int *rune);
int runelen(int c);

/* getopt */
extern int fz_getopt(int nargc, char * const * nargv, const char *ostr);
extern int fz_optind;
extern char *fz_optarg;

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hashtable_s fz_hashtable;

fz_hashtable * fz_newhash(int initialsize, int keylen);
void fz_debughash(fz_hashtable *table);
void fz_emptyhash(fz_hashtable *table);
void fz_freehash(fz_hashtable *table);

void *fz_hashfind(fz_hashtable *table, void *key);
void fz_hashinsert(fz_hashtable *table, void *key, void *val);
void fz_hashremove(fz_hashtable *table, void *key);

int fz_hashlen(fz_hashtable *table);
void *fz_hashgetkey(fz_hashtable *table, int idx);
void *fz_hashgetval(fz_hashtable *table, int idx);

/*
 * Math and geometry
 */

/* Multiply scaled two integers in the 0..255 range */
static inline int fz_mul255(int a, int b)
{
	/* see Jim Blinn's book "Dirty Pixels" for how this works */
	int x = a * b + 128;
	x += x >> 8;
	return x >> 8;
}

/* Expand a value A from the 0...255 range to the 0..256 range */
#define FZ_EXPAND(A) ((A)+((A)>>7))

/* Combine values A (in any range) and B (in the 0..256 range),
 * to give a single value in the same range as A was. */
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/* Blend SRC and DST (in the same range) together according to
 * AMOUNT (in the 0...256 range). */
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

typedef struct fz_matrix_s fz_matrix;
typedef struct fz_point_s fz_point;
typedef struct fz_rect_s fz_rect;
typedef struct fz_bbox_s fz_bbox;

extern const fz_rect fz_unitrect;
extern const fz_rect fz_emptyrect;
extern const fz_rect fz_infiniterect;

extern const fz_bbox fz_unitbbox;
extern const fz_bbox fz_emptybbox;
extern const fz_bbox fz_infinitebbox;

#define fz_isemptyrect(r) ((r).x0 == (r).x1)
#define fz_isinfiniterect(r) ((r).x0 > (r).x1)

struct fz_matrix_s
{
	float a, b, c, d, e, f;
};

struct fz_point_s
{
	float x, y;
};

struct fz_rect_s
{
	float x0, y0;
	float x1, y1;
};

struct fz_bbox_s
{
	int x0, y0;
	int x1, y1;
};

extern const fz_matrix fz_identity;

fz_matrix fz_concat(fz_matrix one, fz_matrix two);
fz_matrix fz_scale(float sx, float sy);
fz_matrix fz_rotate(float theta);
fz_matrix fz_translate(float tx, float ty);
fz_matrix fz_invertmatrix(fz_matrix m);
int fz_isrectilinear(fz_matrix m);
float fz_matrixexpansion(fz_matrix m);

fz_bbox fz_roundrect(fz_rect r);
fz_bbox fz_intersectbbox(fz_bbox a, fz_bbox b);
fz_bbox fz_unionbbox(fz_bbox a, fz_bbox b);

fz_point fz_transformpoint(fz_matrix m, fz_point p);
fz_point fz_transformvector(fz_matrix m, fz_point p);
fz_rect fz_transformrect(fz_matrix m, fz_rect r);

/*
 * Basic crypto functions.
 * Independent of the rest of fitz.
 * For further encapsulation in filters, or not.
 */

/* md5 digests */

typedef struct fz_md5_s fz_md5;

struct fz_md5_s
{
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
};

void fz_md5init(fz_md5 *state);
void fz_md5update(fz_md5 *state, const unsigned char *input, const unsigned inlen);
void fz_md5final(fz_md5 *state, unsigned char digest[16]);

/* arc4 crypto */

typedef struct fz_arc4_s fz_arc4;

struct fz_arc4_s
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
};

void fz_arc4init(fz_arc4 *state, const unsigned char *key, const unsigned len);
unsigned char fz_arc4next(fz_arc4 *state);
void fz_arc4encrypt(fz_arc4 *state, unsigned char *dest, const unsigned char *src, const unsigned len);

/* AES block cipher implementation from XYSSL */

typedef struct fz_aes_s fz_aes;

#define AES_DECRYPT 0
#define AES_ENCRYPT 1

struct fz_aes_s
{
	int nr; /* number of rounds */
	unsigned long *rk; /* AES round keys */
	unsigned long buf[68]; /* unaligned data */
};

void aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_crypt_cbc( fz_aes *ctx, int mode, int length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct fz_obj_s fz_obj;
typedef struct fz_keyval_s fz_keyval;

struct pdf_xref_s;

typedef enum fz_objkind_e
{
	FZ_NULL,
	FZ_BOOL,
	FZ_INT,
	FZ_REAL,
	FZ_STRING,
	FZ_NAME,
	FZ_ARRAY,
	FZ_DICT,
	FZ_INDIRECT
} fz_objkind;

struct fz_keyval_s
{
	fz_obj *k;
	fz_obj *v;
};

struct fz_obj_s
{
	int refs;
	fz_objkind kind;
	union
	{
		int b;
		int i;
		float f;
		struct {
			unsigned short len;
			char buf[1];
		} s;
		char n[1];
		struct {
			int len;
			int cap;
			fz_obj **items;
		} a;
		struct {
			char sorted;
			int len;
			int cap;
			fz_keyval *items;
		} d;
		struct {
			int num;
			int gen;
			struct pdf_xref_s *xref;
		} r;
	} u;
};

fz_obj * fz_newnull(void);
fz_obj * fz_newbool(int b);
fz_obj * fz_newint(int i);
fz_obj * fz_newreal(float f);
fz_obj * fz_newname(char *str);
fz_obj * fz_newstring(char *str, int len);
fz_obj * fz_newindirect(int num, int gen, struct pdf_xref_s *xref);

fz_obj * fz_newarray(int initialcap);
fz_obj * fz_newdict(int initialcap);
fz_obj * fz_copyarray(fz_obj *array);
fz_obj * fz_copydict(fz_obj *dict);

fz_obj *fz_keepobj(fz_obj *obj);
void fz_dropobj(fz_obj *obj);

/* type queries */
int fz_isnull(fz_obj *obj);
int fz_isbool(fz_obj *obj);
int fz_isint(fz_obj *obj);
int fz_isreal(fz_obj *obj);
int fz_isname(fz_obj *obj);
int fz_isstring(fz_obj *obj);
int fz_isarray(fz_obj *obj);
int fz_isdict(fz_obj *obj);
int fz_isindirect(fz_obj *obj);

int fz_objcmp(fz_obj *a, fz_obj *b);

fz_obj *fz_resolveindirect(fz_obj *obj);

/* silent failure, no error reporting */
int fz_tobool(fz_obj *obj);
int fz_toint(fz_obj *obj);
float fz_toreal(fz_obj *obj);
char *fz_toname(fz_obj *obj);
char *fz_tostrbuf(fz_obj *obj);
int fz_tostrlen(fz_obj *obj);
int fz_tonum(fz_obj *obj);
int fz_togen(fz_obj *obj);

int fz_arraylen(fz_obj *array);
fz_obj *fz_arrayget(fz_obj *array, int i);
void fz_arrayput(fz_obj *array, int i, fz_obj *obj);
void fz_arraypush(fz_obj *array, fz_obj *obj);

int fz_dictlen(fz_obj *dict);
fz_obj *fz_dictgetkey(fz_obj *dict, int idx);
fz_obj *fz_dictgetval(fz_obj *dict, int idx);
fz_obj *fz_dictget(fz_obj *dict, fz_obj *key);
fz_obj *fz_dictgets(fz_obj *dict, char *key);
fz_obj *fz_dictgetsa(fz_obj *dict, char *key, char *abbrev);
void fz_dictput(fz_obj *dict, fz_obj *key, fz_obj *val);
void fz_dictputs(fz_obj *dict, char *key, fz_obj *val);
void fz_dictdel(fz_obj *dict, fz_obj *key);
void fz_dictdels(fz_obj *dict, char *key);
void fz_sortdict(fz_obj *dict);

int fz_sprintobj(char *s, int n, fz_obj *obj, int tight);
int fz_fprintobj(FILE *fp, fz_obj *obj, int tight);
void fz_debugobj(fz_obj *obj);

char *fz_objkindstr(fz_obj *obj);

/*
 * Data buffers.
 *
 * A buffer owns the memory it has allocated, unless ownsdata is false,
 * in which case the creator of the buffer owns it.
 */

typedef struct fz_buffer_s fz_buffer;

#define FZ_BUFSIZE (8 * 1024)

struct fz_buffer_s
{
	int refs;
	unsigned char *data;
	int cap, len;
};

fz_buffer * fz_newbuffer(int size);
fz_buffer * fz_newbufferwithmemory(unsigned char *data, int size);

void fz_resizebuffer(fz_buffer *buf, int size);
void fz_growbuffer(fz_buffer *buf);

fz_buffer *fz_keepbuffer(fz_buffer *buf);
void fz_dropbuffer(fz_buffer *buf);

/*
 * Buffered reader.
 * Only the data between rp and wp is valid data.
 */

typedef struct fz_stream_s fz_stream;

enum { FZ_SFILE, FZ_SBUFFER, FZ_SFILTER };

struct fz_stream_s
{
	int refs;
	int dead;
	int pos;
	unsigned char *bp, *rp, *wp, *ep;
	void *state;
	int (*read)(fz_stream *stm, unsigned char *buf, int len);
	void (*close)(fz_stream *stm);
	void (*seek)(fz_stream *stm, int offset, int whence);
	unsigned char buf[4096];
};

fz_stream *fz_openfile(int file);
fz_stream *fz_openbuffer(fz_buffer *buf);
void fz_close(fz_stream *stm);

fz_stream *fz_newstream(void*, int(*)(fz_stream*, unsigned char*, int), void(*)(fz_stream *));
fz_stream *fz_keepstream(fz_stream *stm);
void fz_fillbuffer(fz_stream *stm);

int fz_tell(fz_stream *stm);
void fz_seek(fz_stream *stm, int offset, int whence);

int fz_read(fz_stream *stm, unsigned char *buf, int len);
void fz_readline(fz_stream *stm, char *buf, int max);
fz_error fz_readall(fz_buffer **bufp, fz_stream *stm, int initial);

static inline int fz_readbyte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fillbuffer(stm);
		return stm->rp < stm->wp ? *stm->rp++ : EOF;
	}
	return *stm->rp++;
}

static inline int fz_peekbyte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fillbuffer(stm);
		return stm->rp < stm->wp ? *stm->rp : EOF;
	}
	return *stm->rp;
}

static inline void fz_unreadbyte(fz_stream *stm)
{
	if (stm->rp > stm->bp)
		stm->rp--;
}

/*
 * Data filters.
 */

fz_stream * fz_opencopy(fz_stream *chain);
fz_stream * fz_opennull(fz_stream *chain, int len);
fz_stream * fz_openarc4(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream * fz_openaesd(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream * fz_opena85d(fz_stream *chain);
fz_stream * fz_openahxd(fz_stream *chain);
fz_stream * fz_openrld(fz_stream *chain);
fz_stream * fz_opendctd(fz_stream *chain, fz_obj *param);
fz_stream * fz_openfaxd(fz_stream *chain, fz_obj *param);
fz_stream * fz_openflated(fz_stream *chain);
fz_stream * fz_openlzwd(fz_stream *chain, fz_obj *param);
fz_stream * fz_openpredict(fz_stream *chain, fz_obj *param);
fz_stream * fz_openjbig2d(fz_stream *chain, fz_buffer *global);

/*
 * Resources and other graphics related objects.
 */

enum { FZ_MAXCOLORS = 32 };

typedef enum fz_blendmode_e
{
	/* PDF 1.4 -- standard separable */
	FZ_BNORMAL,
	FZ_BMULTIPLY,
	FZ_BSCREEN,
	FZ_BOVERLAY,
	FZ_BDARKEN,
	FZ_BLIGHTEN,
	FZ_BCOLORDODGE,
	FZ_BCOLORBURN,
	FZ_BHARDLIGHT,
	FZ_BSOFTLIGHT,
	FZ_BDIFFERENCE,
	FZ_BEXCLUSION,

	/* PDF 1.4 -- standard non-separable */
	FZ_BHUE,
	FZ_BSATURATION,
	FZ_BCOLOR,
	FZ_BLUMINOSITY,
} fz_blendmode;

extern const char *fz_blendnames[];

/*
 * Pixmaps have n components per pixel. the last is always alpha.
 * premultiplied alpha when rendering, but non-premultiplied for colorspace
 * conversions and rescaling.
 */

typedef struct fz_pixmap_s fz_pixmap;
typedef struct fz_colorspace_s fz_colorspace;

struct fz_pixmap_s
{
	int refs;
	int x, y, w, h, n;
	fz_pixmap *mask; /* explicit soft/image mask */
	fz_colorspace *colorspace;
	unsigned char *samples;
};

fz_pixmap * fz_newpixmapwithrect(fz_colorspace *, fz_bbox bbox);
fz_pixmap * fz_newpixmap(fz_colorspace *, int x, int y, int w, int h);
fz_pixmap *fz_keeppixmap(fz_pixmap *pix);
void fz_droppixmap(fz_pixmap *pix);
void fz_clearpixmap(fz_pixmap *pix, int value);
void fz_gammapixmap(fz_pixmap *pix, float gamma);
fz_pixmap *fz_alphafromgray(fz_pixmap *gray, int luminosity);
fz_bbox fz_boundpixmap(fz_pixmap *pix);

fz_pixmap * fz_scalepixmap(fz_pixmap *src, int xdenom, int ydenom);
fz_pixmap * fz_smoothscalepixmap(fz_pixmap *src, float x, float y, float w, float h);

fz_error fz_writepnm(fz_pixmap *pixmap, char *filename);
fz_error fz_writepam(fz_pixmap *pixmap, char *filename, int savealpha);
fz_error fz_writepng(fz_pixmap *pixmap, char *filename, int savealpha);

fz_error fz_loadjpximage(fz_pixmap **imgp, unsigned char *data, int size);

/*
 * Colorspace resources.
 */

extern fz_colorspace *fz_devicegray;
extern fz_colorspace *fz_devicergb;
extern fz_colorspace *fz_devicebgr;
extern fz_colorspace *fz_devicecmyk;

struct fz_colorspace_s
{
	int refs;
	char name[16];
	int n;
	void (*toxyz)(fz_colorspace *, float *src, float *xyz);
	void (*fromxyz)(fz_colorspace *, float *xyz, float *dst);
	void (*freedata)(fz_colorspace *);
	void *data;
};

fz_colorspace *fz_newcolorspace(char *name, int n);
fz_colorspace *fz_keepcolorspace(fz_colorspace *cs);
void fz_dropcolorspace(fz_colorspace *cs);

void fz_convertcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_convertpixmap(fz_pixmap *src, fz_pixmap *dst);

/*
 * Fonts come in two variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 */

struct fz_device_s;
struct pdf_xref_s;

typedef struct fz_font_s fz_font;
char *ft_errorstring(int err);

struct fz_font_s
{
	int refs;
	char name[32];

	void *ftface; /* has an FT_Face if used */
	int ftsubstitute; /* ... substitute metrics */
	int fthint; /* ... force hinting for DynaLab fonts */

	fz_matrix t3matrix;
	fz_obj *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	void *t3xref; /* a pdf_xref for the callback */
	fz_error (*t3run)(struct pdf_xref_s *xref, fz_obj *resources, fz_buffer *contents,
		struct fz_device_s *dev, fz_matrix ctm);

	fz_rect bbox;

	/* substitute metrics */
	int widthcount;
	int *widthtable;

	/* SumatraPDF */
	const char *_data; /* font file content or file path */
	int _data_len;     /* 0 for file paths               */
};

fz_error fz_newfreetypefont(fz_font **fontp, char *name, int substitute);
fz_error fz_loadfreetypefontfile(fz_font *font, char *path, int index);
fz_error fz_loadfreetypefontbuffer(fz_font *font, unsigned char *data, int len, int index);
fz_font * fz_newtype3font(char *name, fz_matrix matrix);

fz_error fz_newfontfrombuffer(fz_font **fontp, unsigned char *data, int len, int index);
fz_error fz_newfontfromfile(fz_font **fontp, char *path, int index);

fz_font * fz_keepfont(fz_font *font);
void fz_dropfont(fz_font *font);

void fz_debugfont(fz_font *font);
void fz_setfontbbox(fz_font *font, float xmin, float ymin, float xmax, float ymax);

/*
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or evenodd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef struct fz_path_s fz_path;
typedef struct fz_strokestate_s fz_strokestate;

typedef union fz_pathel_s fz_pathel;

typedef enum fz_pathelkind_e
{
	FZ_MOVETO,
	FZ_LINETO,
	FZ_CURVETO,
	FZ_CLOSEPATH
} fz_pathelkind;

union fz_pathel_s
{
	fz_pathelkind k;
	float v;
};

struct fz_strokestate_s
{
	int linecap;
	int linejoin;
	float linewidth;
	float miterlimit;
	float dashphase;
	int dashlen;
	float dashlist[32];
};

struct fz_path_s
{
	int len, cap;
	fz_pathel *els;
};

fz_path *fz_newpath(void);
void fz_moveto(fz_path*, float x, float y);
void fz_lineto(fz_path*, float x, float y);
void fz_curveto(fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_path*, float, float, float, float);
void fz_curvetoy(fz_path*, float, float, float, float);
void fz_closepath(fz_path*);
void fz_freepath(fz_path *path);

fz_path *fz_clonepath(fz_path *old);

fz_rect fz_boundpath(fz_path *path, fz_strokestate *stroke, fz_matrix ctm);
void fz_debugpath(fz_path *, int indent);

/*
 * Text buffer.
 *
 * The trm field contains the a, b, c and d coefficients.
 * The e and f coefficients come from the individual elements,
 * together they form the transform matrix for the glyph.
 *
 * Glyphs are referenced by glyph ID.
 * The Unicode text equivalent is kept in a separate array
 * with indexes into the glyph array.
 */

typedef struct fz_text_s fz_text;
typedef struct fz_textel_s fz_textel;

struct fz_textel_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
};

struct fz_text_s
{
	fz_font *font;
	fz_matrix trm;
	int wmode;
	int len, cap;
	fz_textel *els;
};

fz_text * fz_newtext(fz_font *face, fz_matrix trm, int wmode);
void fz_addtext(fz_text *text, int gid, int ucs, float x, float y);
void fz_endtext(fz_text *text);
void fz_freetext(fz_text *text);
void fz_debugtext(fz_text*, int indent);
fz_rect fz_boundtext(fz_text *text, fz_matrix ctm);
fz_text *fz_clonetext(fz_text *old);

/*
 * The shading code uses gouraud shaded triangle meshes.
 */

typedef struct fz_shade_s fz_shade;

struct fz_shade_s
{
	int refs;

	fz_rect bbox;		/* can be fz_infiniterect */
	fz_colorspace *cs;

	fz_matrix matrix;	/* matrix from pattern dict */
	int usebackground;	/* background color for fills but not 'sh' */
	float background[FZ_MAXCOLORS];

	int usefunction;
	float function[256][FZ_MAXCOLORS];

	int meshlen;
	int meshcap;
	float *mesh; /* [x y t] or [x y c1 ... cn] */
};

fz_shade *fz_keepshade(fz_shade *shade);
void fz_dropshade(fz_shade *shade);
void fz_debugshade(fz_shade *shade);

fz_rect fz_boundshade(fz_shade *shade, fz_matrix ctm);
void fz_rendershade(fz_shade *shade, fz_matrix ctm, fz_pixmap *dst, fz_bbox bbox);

/*
 * Glyph cache
 */

typedef struct fz_glyphcache_s fz_glyphcache;

fz_glyphcache * fz_newglyphcache(void);
fz_pixmap * fz_renderftglyph(fz_font *font, int cid, fz_matrix trm);
fz_pixmap * fz_rendert3glyph(fz_font *font, int cid, fz_matrix trm);
fz_pixmap * fz_renderftstrokedglyph(fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_strokestate *state);
fz_pixmap * fz_renderglyph(fz_glyphcache*, fz_font*, int, fz_matrix);
fz_pixmap * fz_renderstrokedglyph(fz_glyphcache*, fz_font*, int, fz_matrix, fz_matrix, fz_strokestate *stroke);
void fz_freeglyphcache(fz_glyphcache *);

/*
 * Scan converter
 */

typedef struct fz_edge_s fz_edge;
typedef struct fz_gel_s fz_gel;
typedef struct fz_ael_s fz_ael;

struct fz_edge_s
{
	int x, e, h, y;
	int adjup, adjdown;
	int xmove;
	int xdir, ydir; /* -1 or +1 */
};

struct fz_gel_s
{
	fz_bbox clip;
	fz_bbox bbox;
	int cap;
	int len;
	fz_edge *edges;
};

struct fz_ael_s
{
	int cap;
	int len;
	fz_edge **edges;
};

fz_gel * fz_newgel(void);
void fz_insertgel(fz_gel *gel, float x0, float y0, float x1, float y1);
fz_bbox fz_boundgel(fz_gel *gel);
void fz_resetgel(fz_gel *gel, fz_bbox clip);
void fz_sortgel(fz_gel *gel);
void fz_freegel(fz_gel *gel);
int fz_isrectgel(fz_gel *gel);

fz_ael * fz_newael(void);
void fz_freeael(fz_ael *ael);

fz_error fz_scanconvert(fz_gel *gel, fz_ael *ael, int eofill,
	fz_bbox clip, fz_pixmap *pix, unsigned char *colorbv);

void fz_fillpath(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness);
void fz_strokepath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth);
void fz_dashpath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth);

/*
 * The device interface.
 */

enum
{
	FZ_IGNOREIMAGE = 1,
	FZ_IGNORESHADE = 2,
};

typedef struct fz_device_s fz_device;

struct fz_device_s
{
	int hints;

	void *user;
	void (*freeuser)(void *);

	void (*fillpath)(void *, fz_path *, int evenodd, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*strokepath)(void *, fz_path *, fz_strokestate *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clippath)(void *, fz_path *, int evenodd, fz_matrix);
	void (*clipstrokepath)(void *, fz_path *, fz_strokestate *, fz_matrix);

	void (*filltext)(void *, fz_text *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroketext)(void *, fz_text *, fz_strokestate *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*cliptext)(void *, fz_text *, fz_matrix, int accumulate);
	void (*clipstroketext)(void *, fz_text *, fz_strokestate *, fz_matrix);
	void (*ignoretext)(void *, fz_text *, fz_matrix);

	void (*fillshade)(void *, fz_shade *shd, fz_matrix ctm, float alpha);
	void (*fillimage)(void *, fz_pixmap *img, fz_matrix ctm, float alpha);
	void (*fillimagemask)(void *, fz_pixmap *img, fz_matrix ctm, fz_colorspace *, float *color, float alpha);
	void (*clipimagemask)(void *, fz_pixmap *img, fz_matrix ctm);

	void (*popclip)(void *);

	void (*beginmask)(void *, fz_rect, int luminosity, fz_colorspace *cs, float *bc);
	void (*endmask)(void *);
	void (*begingroup)(void *, fz_rect, int isolated, int knockout, fz_blendmode blendmode, float alpha);
	void (*endgroup)(void *);
};

fz_device *fz_newdevice(void *user);
void fz_freedevice(fz_device *dev);

fz_device *fz_newtracedevice(void);

fz_device *fz_newbboxdevice(fz_bbox *bboxp);

fz_device *fz_newdrawdevice(fz_glyphcache *cache, fz_pixmap *dest);

/* SumatraPDF: GDI+ draw device */
fz_device *fz_newgdiplusdevice(void *hDC, fz_bbox baseClip);

/*
 * Text extraction device
 */

typedef struct fz_textspan_s fz_textspan;
typedef struct fz_textchar_s fz_textchar;

struct fz_textchar_s
{
	int c;
	fz_bbox bbox;
};

struct fz_textspan_s
{
	fz_font *font;
	float size;
	int wmode;
	int len, cap;
	fz_textchar *text;
	fz_textspan *next;
	int eol;
};

fz_textspan * fz_newtextspan(void);
void fz_freetextspan(fz_textspan *line);
void fz_debugtextspan(fz_textspan *line);
void fz_debugtextspanxml(fz_textspan *span);

fz_device *fz_newtextdevice(fz_textspan *text);

/*
 * Display list device -- record and play back device commands.
 */

typedef struct fz_displaylist_s fz_displaylist;
typedef struct fz_displaynode_s fz_displaynode;

typedef enum fz_displaycommand_e
{
	FZ_CMDFILLPATH,
	FZ_CMDSTROKEPATH,
	FZ_CMDCLIPPATH,
	FZ_CMDCLIPSTROKEPATH,
	FZ_CMDFILLTEXT,
	FZ_CMDSTROKETEXT,
	FZ_CMDCLIPTEXT,
	FZ_CMDCLIPSTROKETEXT,
	FZ_CMDIGNORETEXT,
	FZ_CMDFILLSHADE,
	FZ_CMDFILLIMAGE,
	FZ_CMDFILLIMAGEMASK,
	FZ_CMDCLIPIMAGEMASK,
	FZ_CMDPOPCLIP,
	FZ_CMDBEGINMASK,
	FZ_CMDENDMASK,
	FZ_CMDBEGINGROUP,
	FZ_CMDENDGROUP,
} fz_displaycommand;

struct fz_displaylist_s
{
	fz_displaynode *first;
	fz_displaynode *last;
};

struct fz_displaynode_s
{
	fz_displaycommand cmd;
	fz_displaynode *next;
	fz_rect rect;
	union {
		fz_path *path;
		fz_text *text;
		fz_shade *shade;
		fz_pixmap *image;
		fz_blendmode blendmode;
	} item;
	fz_strokestate *stroke;
	int flag; /* evenodd, accumulate, isolated/knockout... */
	fz_matrix ctm;
	fz_colorspace *colorspace;
	float alpha;
	float color[FZ_MAXCOLORS];
};

fz_displaylist *fz_newdisplaylist(void);
void fz_freedisplaylist(fz_displaylist *list);
fz_device *fz_newlistdevice(fz_displaylist *list);
void fz_executedisplaylist(fz_displaylist *list, fz_device *dev, fz_matrix ctm);

/*
 * Function pointers for plotting functions.
 * They can be replaced by cpu-optimized versions.
 */

/*
These are the blending primitives:

span over span					(text and path drawing to clip mask)
span in alpha over span
span in span over span
color in span over span			(text and path drawing)

	fz_paintspan(dp, sp);
		fz_paintspanalpha(dp, sp, alpha)
	fz_paintspanmask(dp, sp, mask);
	fz_paintspancolor(dp, color, mask);

pixmap over pixmap			(shading with function lookup)
pixmap in alpha over pixmap	(xobject/shading with ca)
pixmap in pixmap over pixmap	(xobject with softmask / clip)

	fz_paintpixmap()
		fz_paintpixmapalpha()
	fz_paintpixmapmask()

affine over span
affine in alpha over span
color in affine over span

	fz_paintaffine()
		fz_paintaffinealpha()
	fz_paintaffinecolor()

image over pixmap				(image fill)
image in alpha over pixmap		(image fill with ca)
color in image over pixmap		(image mask fill)

	fz_paintimage()
		fz_paintimagealpha()
	fz_paintimagecolor()

pixmap BLEND pixmap
pixmap in alpha BLEND pixmap

	fz_blendpixmap()
		fz_blendpixmapalpha()
*/

void fz_accelerate(void);
void fz_acceleratearch(void);

void fz_decodetile(fz_pixmap *pix, float *decode);
void fz_decodeindexedtile(fz_pixmap *pix, float *decode, int maxval);
void fz_unpacktile(fz_pixmap *dst, unsigned char * restrict src, int n, int depth, int stride, int scale);

void fz_paintspan(unsigned char * restrict dp, unsigned char * restrict sp, int n, int w, int alpha);
void fz_paintspancolor(unsigned char * restrict dp, unsigned char * restrict mp, int n, int w, unsigned char *color);
void fz_paintspanmask(unsigned char * restrict dp, unsigned char * restrict sp, unsigned char * restrict mp, int n, int w);

void fz_paintaffine(unsigned char *dp, unsigned char *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha);
void fz_paintaffinecolor(unsigned char *dp, unsigned char *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, unsigned char *color);

void fz_paintimage(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm, int alpha);
void fz_paintimagecolor(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm, unsigned char *colorbv);

void fz_paintpixmap(fz_pixmap *dst, fz_pixmap *src, int alpha);
void fz_paintpixmapmask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk);

void fz_blendpixmap(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_blendmode blendmode);

extern void (*fz_srown)(unsigned char *restrict, unsigned char *restrict, int w, int denom, int n);
extern void (*fz_srow1)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_srow2)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_srow4)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_srow5)(unsigned char *restrict, unsigned char *restrict, int w, int denom);

extern void (*fz_scoln)(unsigned char *restrict, unsigned char *restrict, int w, int denom, int n);
extern void (*fz_scol1)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_scol2)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_scol4)(unsigned char *restrict, unsigned char *restrict, int w, int denom);
extern void (*fz_scol5)(unsigned char *restrict, unsigned char *restrict, int w, int denom);

#endif
