/*
 * Author     :  Paul Kocher
 * E-mail     :  pck@netcom.com
 * Date       :  1997
 * Description:  C implementation of the Blowfish algorithm.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned int P[16 + 2];
  unsigned int S[4*256];
} BLOWFISH_CTX;

void Blowfish_Init(BLOWFISH_CTX *ctx, unsigned char *key, int keyLen);
void Blowfish_Encrypt(BLOWFISH_CTX *ctx, unsigned int *xl, unsigned int *xr);
void Blowfish_Decrypt(BLOWFISH_CTX *ctx, unsigned int *xl, unsigned int *xr);

#ifdef __cplusplus
};
#endif