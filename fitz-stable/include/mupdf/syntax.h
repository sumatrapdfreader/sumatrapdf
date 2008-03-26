/*
 * tokenizer and low-level object parser
 */

enum
{
	PDF_TERROR, PDF_TEOF,
	PDF_TOARRAY, PDF_TCARRAY,
	PDF_TODICT, PDF_TCDICT,
	PDF_TOBRACE, PDF_TCBRACE,
	PDF_TNAME, PDF_TINT, PDF_TREAL, PDF_TSTRING, PDF_TKEYWORD,
	PDF_TR, PDF_TTRUE, PDF_TFALSE, PDF_TNULL,
	PDF_TOBJ, PDF_TENDOBJ,
	PDF_TSTREAM, PDF_TENDSTREAM,
	PDF_TXREF, PDF_TTRAILER, PDF_TSTARTXREF,
	PDF_NTOKENS
};

/* lex.c */
fz_error *pdf_lex(int *token, fz_stream *f, unsigned char *buf, int n, int *len);

/* parse.c */
fz_error *pdf_parsearray(fz_obj **op, fz_stream *f, char *buf, int cap);
fz_error *pdf_parsedict(fz_obj **op, fz_stream *f, char *buf, int cap);
fz_error *pdf_parsestmobj(fz_obj **op, fz_stream *f, char *buf, int cap);
fz_error *pdf_parseindobj(fz_obj **op, fz_stream *f, char *buf, int cap, int *oid, int *gid, int *stmofs);

fz_rect pdf_torect(fz_obj *array);
fz_matrix pdf_tomatrix(fz_obj *array);
fz_error *pdf_toutf8(char **dstp, fz_obj *src);
fz_error *pdf_toucs2(unsigned short **dstp, fz_obj *src);

/*
 * Encryption
 */

typedef struct pdf_crypt_s pdf_crypt;

struct pdf_crypt_s
{
	unsigned char o[32];
	unsigned char u[32];
	unsigned int p;
	int r;
	int n;

	fz_obj *encrypt;
	fz_obj *id;

	unsigned char key[16];
	int keylen;
};

/* crypt.c */
fz_error *pdf_newdecrypt(pdf_crypt **cp, fz_obj *enc, fz_obj *id);
fz_error *pdf_newencrypt(pdf_crypt **cp, char *userpw, char *ownerpw, int p, int n, fz_obj *id);

int pdf_setpassword(pdf_crypt *crypt, char *pw);
int pdf_setuserpassword(pdf_crypt *crypt, char *pw, int pwlen);
int pdf_setownerpassword(pdf_crypt *crypt, char *pw, int pwlen);

fz_error *pdf_cryptstream(fz_filter **fp, pdf_crypt *crypt, int oid, int gid);
void pdf_cryptobj(pdf_crypt *crypt, fz_obj *obj, int oid, int gid);
void pdf_dropcrypt(pdf_crypt *crypt);

