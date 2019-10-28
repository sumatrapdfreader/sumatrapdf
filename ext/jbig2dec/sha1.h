/* public api for steve reid's public domain SHA-1 implementation */
/* this file is in the public domain */

#ifndef __SHA1_H
#define __SHA1_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} SHA1_CTX;

#define SHA1_DIGEST_SIZE 20

void SHA1_Init(SHA1_CTX *context);
void SHA1_Update(SHA1_CTX *context, const uint8_t *data, const size_t len);
void SHA1_Final(SHA1_CTX *context, uint8_t digest[SHA1_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif                          /* __SHA1_H */
