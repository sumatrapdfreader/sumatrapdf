/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filter library and the mupdf parser.
 */

typedef struct pdf_xref_s pdf_xref; /* this file is about to be merged with mupdf */

typedef struct fz_obj_s fz_obj;
typedef struct fz_keyval_s fz_keyval;

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
			pdf_xref *xref;
			fz_obj *obj;
		} r;
	} u;
};

fz_obj * fz_newnull(void);
fz_obj * fz_newbool(int b);
fz_obj * fz_newint(int i);
fz_obj * fz_newreal(float f);
fz_obj * fz_newname(char *str);
fz_obj * fz_newstring(char *str, int len);
fz_obj * fz_newindirect(int num, int gen, pdf_xref *xref);

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

fz_obj * fz_newnamefromstring(fz_obj *str);

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

fz_error fz_parseobj(fz_obj **, pdf_xref *xref, char *s);
fz_error fz_packobj(fz_obj **, pdf_xref *xref, char *fmt, ...);

char *fz_objkindstr(fz_obj *obj);

/*
 * Data buffers for streams and filters.
 *
 *   bp is the pointer to the allocated memory
 *   rp is read-position (*in->rp++ to read data)
 *   wp is write-position (*out->wp++ to write data)
 *   ep is the sentinel
 *
 * Only the data between rp and wp is valid data.
 *
 * Writers set eof to true at the end.
 * Readers look at eof.
 *
 * A buffer owns the memory it has allocated, unless ownsdata is false,
 * in which case the creator of the buffer owns it.
 */

typedef struct fz_buffer_s fz_buffer;

#define FZ_BUFSIZE (8 * 1024)

struct fz_buffer_s
{
	int refs;
	int ownsdata;
	unsigned char *bp;
	unsigned char *rp;
	unsigned char *wp;
	unsigned char *ep;
	int eof;
};

fz_buffer * fz_newbuffer(int size);
fz_buffer * fz_newbufferwithmemory(unsigned char *data, int size);

void fz_rewindbuffer(fz_buffer *buf);
void fz_growbuffer(fz_buffer *buf);

fz_buffer *fz_keepbuffer(fz_buffer *buf);
void fz_dropbuffer(fz_buffer *buf);

/*
 * Data filters for encryption, compression and decompression.
 *
 * A filter has one method, process, that takes an input and an output buffer.
 *
 * It returns one of three statuses:
 *    ioneedin -- input buffer exhausted, please give me more data (wp-rp)
 *    ioneedout -- output buffer exhausted, please provide more space (ep-wp)
 *    iodone -- finished, please never call me again. ever!
 * or...
 *    any other error code -- oops, something blew up.
 *
 * To make using the filter easier, three variables are updated:
 *    produced -- if we actually produced any new data
 *    consumed -- like above
 *    count -- number of bytes produced in total since the beginning
 *    done -- remember if we've ever returned fz_iodone
 *
 * Most filters take fz_obj as a way to specify parameters.
 * In most cases, this is a dictionary that contains the same keys
 * that the corresponding PDF filter would expect.
 *
 * The pipeline filter is special, and needs some care when chaining
 * and unchaining new filters.
 */

typedef struct fz_filter_s fz_filter;

#define fz_ioneedin ((fz_error)1)
#define fz_ioneedout ((fz_error)2)
#define fz_iodone ((fz_error)3)

/*
 * Evil looking macro to create an initialize a filter struct.
 */

#define FZ_NEWFILTER(TYPE,VAR,NAME) \
	fz_error fz_process ## NAME (fz_filter*,fz_buffer*,fz_buffer*); \
	void fz_drop ## NAME (fz_filter*); \
	TYPE *VAR; \
	VAR = fz_malloc(sizeof(TYPE));	 \
	((fz_filter*)VAR)->refs = 1; \
	((fz_filter*)VAR)->process = fz_process ## NAME ;  \
	((fz_filter*)VAR)->drop = fz_drop ## NAME ;  \
	((fz_filter*)VAR)->consumed = 0;  \
	((fz_filter*)VAR)->produced = 0; \
	((fz_filter*)VAR)->count = 0; \
	((fz_filter*)VAR)->done = 0;

struct fz_filter_s
{
	int refs;
	fz_error (*process)(fz_filter *filter, fz_buffer *in, fz_buffer *out);
	void (*drop)(fz_filter *filter);
	int consumed;
	int produced;
	int count;
	int done;
};

fz_error fz_process(fz_filter *f, fz_buffer *in, fz_buffer *out);
fz_filter *fz_keepfilter(fz_filter *f);
void fz_dropfilter(fz_filter *f);

fz_filter * fz_newpipeline(fz_filter *head, fz_filter *tail);
fz_filter * fz_chainpipeline(fz_filter *head, fz_filter *tail, fz_buffer *buf);
void fz_unchainpipeline(fz_filter *pipe, fz_filter **oldfp, fz_buffer **oldbp);

/* stop and reverse! special case needed for postscript only */
void fz_pushbackahxd(fz_filter *filter, fz_buffer *in, fz_buffer *out, int n);

fz_filter * fz_newnullfilter(int len);
fz_filter * fz_newcopyfilter();
fz_filter * fz_newarc4filter(unsigned char *key, unsigned keylen);
fz_filter * fz_newaesdfilter(unsigned char *key, unsigned keylen);
fz_filter * fz_newa85d(fz_obj *param);
fz_filter * fz_newahxd(fz_obj *param);
fz_filter * fz_newrld(fz_obj *param);
fz_filter * fz_newdctd(fz_obj *param);
fz_filter * fz_newfaxd(fz_obj *param);
fz_filter * fz_newflated(fz_obj *param);
fz_filter * fz_newlzwd(fz_obj *param);
fz_filter * fz_newpredictd(fz_obj *param);
fz_filter * fz_newjbig2d(fz_obj *param);
fz_filter * fz_newjpxd(fz_obj *param);

fz_error fz_setjbig2dglobalstream(fz_filter *filter, unsigned char *buf, int len);

/*
 * Basic crypto functions.
 * Independent of the rest of fitz.
 * For further encapsulation in filters, or not.
 */

/* crc-32 checksum */

unsigned long fz_crc32(unsigned long crc, unsigned char *buf, int len);

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

#define AES_DECRYPT 0
#define AES_ENCRYPT 1

struct fz_aes_s
{
	int nr;                     /* number of rounds */
	unsigned long *rk;          /* AES round keys */
	unsigned long buf[68];      /* unaligned data */
};

typedef struct fz_aes_s fz_aes;

void aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_crypt_cbc( fz_aes *ctx, int mode, int length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

/*
 * Stream API for Fitz.
 * Read and write data to and from files, memory buffers and filters.
 */

typedef struct fz_stream_s fz_stream;

enum { FZ_SFILE, FZ_SBUFFER, FZ_SFILTER };

struct fz_stream_s
{
	int refs;
	int kind;
	int dead;
	fz_buffer *buffer;
	fz_filter *filter;
	fz_stream *chain;
	fz_error error; /* delayed error from readbyte and peekbyte */
	int file;
};

/*
 * Various stream creation functions.
 */

fz_error fz_openrfile(fz_stream **stmp, char *filename);
fz_stream * fz_openrmemory(unsigned char *mem, int len);
fz_stream * fz_openrbuffer(fz_buffer *buf);
fz_stream * fz_openrfilter(fz_filter *flt, fz_stream *chain);

/*
 * Functions that are common to both input and output streams.
 */

fz_stream *fz_keepstream(fz_stream *stm);
void fz_dropstream(fz_stream *stm);

int fz_tell(fz_stream *stm);
fz_error fz_seek(fz_stream *stm, int offset, int whence);

/*
 * Input stream functions.
 */

int fz_rtell(fz_stream *stm);
fz_error fz_rseek(fz_stream *stm, int offset, int whence);

fz_error fz_readimp(fz_stream *stm);
fz_error fz_read(int *np, fz_stream *stm, unsigned char *buf, int len);
fz_error fz_readline(fz_stream *stm, char *buf, int max);
fz_buffer * fz_readall(fz_stream *stm, int sizehint);

/*
 * Error handling when reading with readbyte/peekbyte is non-standard.
 * The cause of an error is stuck into the stream struct,
 * and EOF is returned. Not good, but any other way is too painful.
 * So we have to be careful to check the error status eventually.
 */

fz_error fz_readerror(fz_stream *stm);
int fz_readbytex(fz_stream *stm);
int fz_peekbytex(fz_stream *stm);

#ifdef DEBUG

#define fz_readbyte fz_readbytex
#define fz_peekbyte fz_peekbytex

#else

static inline int fz_readbyte(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;
	if (buf->rp < buf->wp)
		return *buf->rp++;
	return fz_readbytex(stm);
}

static inline int fz_peekbyte(fz_stream *stm)
{
	fz_buffer *buf = stm->buffer;
	if (buf->rp < buf->wp)
		return *buf->rp;
	return fz_peekbytex(stm);
}

#endif

