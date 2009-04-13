/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filter library and the mupdf parser.
 */

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
	FZ_INDIRECT,
	FZ_POINTER
} fz_objkind;

struct fz_keyval_s
{
	fz_obj *k;
	fz_obj *v;
};

struct fz_obj_s
{
	unsigned short refs;
#ifdef DEBUG
	fz_objkind kind;
#else
	char kind;				/* fz_objkind takes 4 bytes :( */
#endif
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
			int oid;
			int gid;
		} r;
		void *p;
	} u;
};

fz_error fz_newnull(fz_obj **op);
fz_error fz_newbool(fz_obj **op, int b);
fz_error fz_newint(fz_obj **op, int i);
fz_error fz_newreal(fz_obj **op, float f);
fz_error fz_newname(fz_obj **op, char *str);
fz_error fz_newstring(fz_obj **op, char *str, int len);
fz_error fz_newindirect(fz_obj **op, int oid, int gid);
fz_error fz_newpointer(fz_obj **op, void *p);

fz_error fz_newarray(fz_obj **op, int initialcap);
fz_error fz_newdict(fz_obj **op, int initialcap);
fz_error fz_copyarray(fz_obj **op, fz_obj *array);
fz_error fz_copydict(fz_obj **op, fz_obj *dict);
fz_error fz_deepcopyarray(fz_obj **op, fz_obj *array);
fz_error fz_deepcopydict(fz_obj **op, fz_obj *dict);

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
int fz_ispointer(fz_obj *obj);

int fz_objcmp(fz_obj *a, fz_obj *b);

/* silent failure, no error reporting */
int fz_tobool(fz_obj *obj);
int fz_toint(fz_obj *obj);
float fz_toreal(fz_obj *obj);
char *fz_toname(fz_obj *obj);
char *fz_tostrbuf(fz_obj *obj);
int fz_tostrlen(fz_obj *obj);
int fz_tonum(fz_obj *obj);
int fz_togen(fz_obj *obj);
void *fz_topointer(fz_obj *obj);

fz_error fz_newnamefromstring(fz_obj **op, fz_obj *str);

int fz_arraylen(fz_obj *array);
fz_obj *fz_arrayget(fz_obj *array, int i);
fz_error fz_arrayput(fz_obj *array, int i, fz_obj *obj);
fz_error fz_arraypush(fz_obj *array, fz_obj *obj);

int fz_dictlen(fz_obj *dict);
fz_obj *fz_dictgetkey(fz_obj *dict, int idx);
fz_obj *fz_dictgetval(fz_obj *dict, int idx);
fz_obj *fz_dictget(fz_obj *dict, fz_obj *key);
fz_obj *fz_dictgets(fz_obj *dict, char *key);
fz_obj *fz_dictgetsa(fz_obj *dict, char *key, char *abbrev);
fz_error fz_dictput(fz_obj *dict, fz_obj *key, fz_obj *val);
fz_error fz_dictputs(fz_obj *dict, char *key, fz_obj *val);
fz_error fz_dictdel(fz_obj *dict, fz_obj *key);
fz_error fz_dictdels(fz_obj *dict, char *key);
void fz_sortdict(fz_obj *dict);

int fz_sprintobj(char *s, int n, fz_obj *obj, int tight);
int fz_fprintobj(FILE *fp, fz_obj *obj, int tight);
void fz_debugobj(fz_obj *obj);

fz_error fz_parseobj(fz_obj **objp, char *s);
fz_error fz_packobj(fz_obj **objp, char *fmt, ...);
fz_error fz_unpackobj(fz_obj *obj, char *fmt, ...);

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

fz_error fz_newbuffer(fz_buffer **bufp, int size);
fz_error fz_newbufferwithmemory(fz_buffer **bufp, unsigned char *data, int size);

fz_error fz_rewindbuffer(fz_buffer *buf);
fz_error fz_growbuffer(fz_buffer *buf);

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
 *    any other error object -- oops, something blew up.
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

#define FZ_NEWFILTER(TYPE,VAR,NAME)                                         \
	fz_error fz_process ## NAME (fz_filter*,fz_buffer*,fz_buffer*);   \
	void fz_drop ## NAME (fz_filter*);                                  \
	TYPE *VAR;                                                          \
	*fp = fz_malloc(sizeof(TYPE));                                      \
	if (!*fp) return fz_throw("outofmem: %s filter struct", #NAME);     \
	(*fp)->refs = 1;                                                    \
	(*fp)->process = fz_process ## NAME ;                               \
	(*fp)->drop = fz_drop ## NAME ;                                     \
	(*fp)->consumed = 0;                                                \
	(*fp)->produced = 0;                                                \
	(*fp)->count = 0;                                                   \
	(*fp)->done = 0;                                                    \
	VAR = (TYPE*) *fp

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

fz_error fz_newpipeline(fz_filter **fp, fz_filter *head, fz_filter *tail);
fz_error fz_chainpipeline(fz_filter **fp, fz_filter *head, fz_filter *tail, fz_buffer *buf);
void fz_unchainpipeline(fz_filter *pipe, fz_filter **oldfp, fz_buffer **oldbp);

/* stop and reverse! special case needed for postscript only */
void fz_pushbackahxd(fz_filter *filter, fz_buffer *in, fz_buffer *out, int n);

fz_error fz_newnullfilter(fz_filter **fp, int len);
fz_error fz_newarc4filter(fz_filter **fp, unsigned char *key, unsigned keylen);
fz_error fz_newaesfilter(fz_filter **fp, unsigned char *key, unsigned keylen);
fz_error fz_newa85d(fz_filter **filterp, fz_obj *param);
fz_error fz_newa85e(fz_filter **filterp, fz_obj *param);
fz_error fz_newahxd(fz_filter **filterp, fz_obj *param);
fz_error fz_newahxe(fz_filter **filterp, fz_obj *param);
fz_error fz_newrld(fz_filter **filterp, fz_obj *param);
fz_error fz_newrle(fz_filter **filterp, fz_obj *param);
fz_error fz_newdctd(fz_filter **filterp, fz_obj *param);
fz_error fz_newdcte(fz_filter **filterp, fz_obj *param);
fz_error fz_newfaxd(fz_filter **filterp, fz_obj *param);
fz_error fz_newfaxe(fz_filter **filterp, fz_obj *param);
fz_error fz_newflated(fz_filter **filterp, fz_obj *param);
fz_error fz_newflatee(fz_filter **filterp, fz_obj *param);
fz_error fz_newlzwd(fz_filter **filterp, fz_obj *param);
fz_error fz_newlzwe(fz_filter **filterp, fz_obj *param);
fz_error fz_newpredictd(fz_filter **filterp, fz_obj *param);
fz_error fz_newpredicte(fz_filter **filterp, fz_obj *param);
fz_error fz_newjbig2d(fz_filter **filterp, fz_obj *param);
fz_error fz_newjpxd(fz_filter **filterp, fz_obj *param);

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

/* TODO: sha1 */
/* aes crypto */

/* Minimal definition from libtomcrypt.h */

/* max size of either a cipher/hash block or symmetric key [largest of the two] */
#define MAXBLOCKSIZE  128

/* this is the "32-bit at least" data type
 * Re-define it to suit your platform but it must be at least 32-bits
 */
#if defined(__x86_64__) || (defined(__sparc__) && defined(__arch64__))
   typedef unsigned ulong32;
#else
   typedef unsigned long ulong32;
#endif

/* This part extracted from tomcrypt_cipher.h */
struct rijndael_key {
   ulong32 eK[60], dK[60];
   int Nr;
};

typedef union Symmetric_key {
   struct rijndael_key rijndael;
} symmetric_key;

/** A block cipher CBC structure */
typedef struct {
   /** The block size of the given cipher */
   int                 blocklen;
   /** The current IV */
   unsigned char       IV[MAXBLOCKSIZE];
   /** The scheduled key */
   symmetric_key       key;
} symmetric_CBC;

/* End of part extracted from tomcrypt_cipher.h */

typedef struct fz_aes_s fz_aes;

struct fz_aes_s
{
        /* For aes IV is first 16 bytes of string/stream data so we must
           know whether IV is not yet set (first iteration of encrypt)
           or set (subsequent iterations) */
        int ivinited;
        symmetric_CBC cbckey;
};

void fz_aesinit(fz_aes *state, unsigned char *key, unsigned len);
void fz_setiv(fz_aes *state, unsigned char *iv);
void fz_aesdecrypt(fz_aes *state, unsigned char *dest, unsigned char *src, unsigned len);

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
#ifdef WIN32
#include <wchar.h>
fz_error fz_openrfilew(fz_stream **stmp, const wchar_t *path);
#endif

/* write to memory buffers! */
fz_error fz_openrmemory(fz_stream **stmp, unsigned char *mem, int len);
fz_error fz_openrbuffer(fz_stream **stmp, fz_buffer *buf);

/* almost like fork() exec() pipe() */
fz_error fz_openrfilter(fz_stream **stmp, fz_filter *flt, fz_stream *chain);

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
fz_error fz_readall(fz_buffer **bufp, fz_stream *stm, int sizehint);
fz_error fz_readline(fz_stream *stm, char *buf, int max);

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

