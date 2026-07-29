#ifndef OPJ_BIO_H
#define OPJ_BIO_H

#include <stddef.h>

typedef struct opj_bio {

    OPJ_BYTE *start;

    OPJ_BYTE *end;

    OPJ_BYTE *bp;

    OPJ_UINT32 buf;

    OPJ_UINT32 ct;
} opj_bio_t;

opj_bio_t* opj_bio_create(void);

void opj_bio_destroy(opj_bio_t *bio);

ptrdiff_t opj_bio_numbytes(opj_bio_t *bio);

void opj_bio_init_enc(opj_bio_t *bio, OPJ_BYTE *bp, OPJ_UINT32 len);

void opj_bio_init_dec(opj_bio_t *bio, OPJ_BYTE *bp, OPJ_UINT32 len);

void opj_bio_write(opj_bio_t *bio, OPJ_UINT32 v, OPJ_UINT32 n);

void opj_bio_putbit(opj_bio_t *bio, OPJ_UINT32 b);

OPJ_UINT32 opj_bio_read(opj_bio_t *bio, OPJ_UINT32 n);

OPJ_BOOL opj_bio_flush(opj_bio_t *bio);

OPJ_BOOL opj_bio_inalign(opj_bio_t *bio);

#endif
