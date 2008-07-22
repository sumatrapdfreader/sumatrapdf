/* Most of this code is extracted from public domain libtomcrypt 1.17 
   (http://libtom.org/) */

#include "fitz-base.h"
#include "fitz-stream.h"

#define LTC_NO_ASM 1

/* This part extracted from tomcrypt.h in libtomcrypt 1.17 */

enum {
   CRYPT_OK=0,             /* Result OK */
   CRYPT_ERROR,            /* Generic Error */
   CRYPT_INVALID_KEYSIZE,  /* Invalid key size given */
   CRYPT_INVALID_ROUNDS,   /* Invalid number of rounds */
   CRYPT_INVALID_ARG,      /* Generic invalid argument */
};

/* End of part extracted from tomcrypt.h */

#define STORE32H(x, y)                                                                     \
     { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
       (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                            \
     { x = ((unsigned long)((y)[0] & 255)<<24) | \
           ((unsigned long)((y)[1] & 255)<<16) | \
           ((unsigned long)((y)[2] & 255)<<8)  | \
           ((unsigned long)((y)[3] & 255)); }

/* 32-bit Rotates */
#if defined(_MSC_VER)

/* instrinsic rotate */
#include <stdlib.h>
#pragma intrinsic(_lrotr,_lrotl)
#define ROR(x,n) _lrotr(x,n)
#define ROL(x,n) _lrotl(x,n)
#define RORc(x,n) _lrotr(x,n)
#define ROLc(x,n) _lrotl(x,n)

#elif !defined(__STRICT_ANSI__) && defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && !defined(INTEL_CC) && !defined(LTC_NO_ASM)

static inline unsigned ROL(unsigned word, int i)
{
   asm ("roll %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

static inline unsigned ROR(unsigned word, int i)
{
   asm ("rorl %%cl,%0"
      :"=r" (word)
      :"0" (word),"c" (i));
   return word;
}

#ifndef LTC_NO_ROLC

static inline unsigned ROLc(unsigned word, const int i)
{
   asm ("roll %2,%0"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

static inline unsigned RORc(unsigned word, const int i)
{
   asm ("rorl %2,%0"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

#else

#define ROLc ROL
#define RORc ROR

#endif

#elif !defined(__STRICT_ANSI__) && defined(LTC_PPC32)

static inline unsigned ROL(unsigned word, int i)
{
   asm ("rotlw %0,%0,%2"
      :"=r" (word)
      :"0" (word),"r" (i));
   return word;
}

static inline unsigned ROR(unsigned word, int i)
{
   asm ("rotlw %0,%0,%2"
      :"=r" (word)
      :"0" (word),"r" (32-i));
   return word;
}

#ifndef LTC_NO_ROLC

static inline unsigned ROLc(unsigned word, const int i)
{
   asm ("rotlwi %0,%0,%2"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

static inline unsigned RORc(unsigned word, const int i)
{
   asm ("rotrwi %0,%0,%2"
      :"=r" (word)
      :"0" (word),"I" (i));
   return word;
}

#else

#define ROLc ROL
#define RORc ROR

#endif


#else

/* rotates the hard way */
#define ROL(x, y) ( (((unsigned long)(x)<<(unsigned long)((y)&31)) | (((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROR(x, y) ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROLc(x, y) ( (((unsigned long)(x)<<(unsigned long)((y)&31)) | (((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define RORc(x, y) ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)

#endif

/* extract a byte portably */
#ifdef _MSC_VER
   #define byte(x, n) ((unsigned char)((x) >> (8 * (n))))
#else
   #define byte(x, n) (((x) >> (8 * (n))) & 255)
#endif

/* End of part extracted from tomcrypt_macros.h in libtomcrypt 1.17 */

/* This part extracted from tomcrypt_argchk.h */
#define LTC_ARGCHK(x)   if (!(x)) return CRYPT_INVALID_ARG;
#define LTC_ARGCHKVD(x) if (!(x)) return;
/* End of part extracted from tomcrypt_argchk.h */

/* This part extracted from aes_tab.c in libtomcrypt 1.17 */

/* The precomputed tables for AES */
/*
Te0[x] = S [x].[02, 01, 01, 03];
Te1[x] = S [x].[03, 02, 01, 01];
Te2[x] = S [x].[01, 03, 02, 01];
Te3[x] = S [x].[01, 01, 03, 02];
Te4[x] = S [x].[01, 01, 01, 01];

Td0[x] = Si[x].[0e, 09, 0d, 0b];
Td1[x] = Si[x].[0b, 0e, 09, 0d];
Td2[x] = Si[x].[0d, 0b, 0e, 09];
Td3[x] = Si[x].[09, 0d, 0b, 0e];
Td4[x] = Si[x].[01, 01, 01, 01];
*/

/**
  @file aes_tab.c
  AES tables
*/  
static const ulong32 TE0[256] = {
    0xc66363a5UL, 0xf87c7c84UL, 0xee777799UL, 0xf67b7b8dUL,
    0xfff2f20dUL, 0xd66b6bbdUL, 0xde6f6fb1UL, 0x91c5c554UL,
    0x60303050UL, 0x02010103UL, 0xce6767a9UL, 0x562b2b7dUL,
    0xe7fefe19UL, 0xb5d7d762UL, 0x4dababe6UL, 0xec76769aUL,
    0x8fcaca45UL, 0x1f82829dUL, 0x89c9c940UL, 0xfa7d7d87UL,
    0xeffafa15UL, 0xb25959ebUL, 0x8e4747c9UL, 0xfbf0f00bUL,
    0x41adadecUL, 0xb3d4d467UL, 0x5fa2a2fdUL, 0x45afafeaUL,
    0x239c9cbfUL, 0x53a4a4f7UL, 0xe4727296UL, 0x9bc0c05bUL,
    0x75b7b7c2UL, 0xe1fdfd1cUL, 0x3d9393aeUL, 0x4c26266aUL,
    0x6c36365aUL, 0x7e3f3f41UL, 0xf5f7f702UL, 0x83cccc4fUL,
    0x6834345cUL, 0x51a5a5f4UL, 0xd1e5e534UL, 0xf9f1f108UL,
    0xe2717193UL, 0xabd8d873UL, 0x62313153UL, 0x2a15153fUL,
    0x0804040cUL, 0x95c7c752UL, 0x46232365UL, 0x9dc3c35eUL,
    0x30181828UL, 0x379696a1UL, 0x0a05050fUL, 0x2f9a9ab5UL,
    0x0e070709UL, 0x24121236UL, 0x1b80809bUL, 0xdfe2e23dUL,
    0xcdebeb26UL, 0x4e272769UL, 0x7fb2b2cdUL, 0xea75759fUL,
    0x1209091bUL, 0x1d83839eUL, 0x582c2c74UL, 0x341a1a2eUL,
    0x361b1b2dUL, 0xdc6e6eb2UL, 0xb45a5aeeUL, 0x5ba0a0fbUL,
    0xa45252f6UL, 0x763b3b4dUL, 0xb7d6d661UL, 0x7db3b3ceUL,
    0x5229297bUL, 0xdde3e33eUL, 0x5e2f2f71UL, 0x13848497UL,
    0xa65353f5UL, 0xb9d1d168UL, 0x00000000UL, 0xc1eded2cUL,
    0x40202060UL, 0xe3fcfc1fUL, 0x79b1b1c8UL, 0xb65b5bedUL,
    0xd46a6abeUL, 0x8dcbcb46UL, 0x67bebed9UL, 0x7239394bUL,
    0x944a4adeUL, 0x984c4cd4UL, 0xb05858e8UL, 0x85cfcf4aUL,
    0xbbd0d06bUL, 0xc5efef2aUL, 0x4faaaae5UL, 0xedfbfb16UL,
    0x864343c5UL, 0x9a4d4dd7UL, 0x66333355UL, 0x11858594UL,
    0x8a4545cfUL, 0xe9f9f910UL, 0x04020206UL, 0xfe7f7f81UL,
    0xa05050f0UL, 0x783c3c44UL, 0x259f9fbaUL, 0x4ba8a8e3UL,
    0xa25151f3UL, 0x5da3a3feUL, 0x804040c0UL, 0x058f8f8aUL,
    0x3f9292adUL, 0x219d9dbcUL, 0x70383848UL, 0xf1f5f504UL,
    0x63bcbcdfUL, 0x77b6b6c1UL, 0xafdada75UL, 0x42212163UL,
    0x20101030UL, 0xe5ffff1aUL, 0xfdf3f30eUL, 0xbfd2d26dUL,
    0x81cdcd4cUL, 0x180c0c14UL, 0x26131335UL, 0xc3ecec2fUL,
    0xbe5f5fe1UL, 0x359797a2UL, 0x884444ccUL, 0x2e171739UL,
    0x93c4c457UL, 0x55a7a7f2UL, 0xfc7e7e82UL, 0x7a3d3d47UL,
    0xc86464acUL, 0xba5d5de7UL, 0x3219192bUL, 0xe6737395UL,
    0xc06060a0UL, 0x19818198UL, 0x9e4f4fd1UL, 0xa3dcdc7fUL,
    0x44222266UL, 0x542a2a7eUL, 0x3b9090abUL, 0x0b888883UL,
    0x8c4646caUL, 0xc7eeee29UL, 0x6bb8b8d3UL, 0x2814143cUL,
    0xa7dede79UL, 0xbc5e5ee2UL, 0x160b0b1dUL, 0xaddbdb76UL,
    0xdbe0e03bUL, 0x64323256UL, 0x743a3a4eUL, 0x140a0a1eUL,
    0x924949dbUL, 0x0c06060aUL, 0x4824246cUL, 0xb85c5ce4UL,
    0x9fc2c25dUL, 0xbdd3d36eUL, 0x43acacefUL, 0xc46262a6UL,
    0x399191a8UL, 0x319595a4UL, 0xd3e4e437UL, 0xf279798bUL,
    0xd5e7e732UL, 0x8bc8c843UL, 0x6e373759UL, 0xda6d6db7UL,
    0x018d8d8cUL, 0xb1d5d564UL, 0x9c4e4ed2UL, 0x49a9a9e0UL,
    0xd86c6cb4UL, 0xac5656faUL, 0xf3f4f407UL, 0xcfeaea25UL,
    0xca6565afUL, 0xf47a7a8eUL, 0x47aeaee9UL, 0x10080818UL,
    0x6fbabad5UL, 0xf0787888UL, 0x4a25256fUL, 0x5c2e2e72UL,
    0x381c1c24UL, 0x57a6a6f1UL, 0x73b4b4c7UL, 0x97c6c651UL,
    0xcbe8e823UL, 0xa1dddd7cUL, 0xe874749cUL, 0x3e1f1f21UL,
    0x964b4bddUL, 0x61bdbddcUL, 0x0d8b8b86UL, 0x0f8a8a85UL,
    0xe0707090UL, 0x7c3e3e42UL, 0x71b5b5c4UL, 0xcc6666aaUL,
    0x904848d8UL, 0x06030305UL, 0xf7f6f601UL, 0x1c0e0e12UL,
    0xc26161a3UL, 0x6a35355fUL, 0xae5757f9UL, 0x69b9b9d0UL,
    0x17868691UL, 0x99c1c158UL, 0x3a1d1d27UL, 0x279e9eb9UL,
    0xd9e1e138UL, 0xebf8f813UL, 0x2b9898b3UL, 0x22111133UL,
    0xd26969bbUL, 0xa9d9d970UL, 0x078e8e89UL, 0x339494a7UL,
    0x2d9b9bb6UL, 0x3c1e1e22UL, 0x15878792UL, 0xc9e9e920UL,
    0x87cece49UL, 0xaa5555ffUL, 0x50282878UL, 0xa5dfdf7aUL,
    0x038c8c8fUL, 0x59a1a1f8UL, 0x09898980UL, 0x1a0d0d17UL,
    0x65bfbfdaUL, 0xd7e6e631UL, 0x844242c6UL, 0xd06868b8UL,
    0x824141c3UL, 0x299999b0UL, 0x5a2d2d77UL, 0x1e0f0f11UL,
    0x7bb0b0cbUL, 0xa85454fcUL, 0x6dbbbbd6UL, 0x2c16163aUL,
};

static const ulong32 Te4[256] = {
    0x63636363UL, 0x7c7c7c7cUL, 0x77777777UL, 0x7b7b7b7bUL,
    0xf2f2f2f2UL, 0x6b6b6b6bUL, 0x6f6f6f6fUL, 0xc5c5c5c5UL,
    0x30303030UL, 0x01010101UL, 0x67676767UL, 0x2b2b2b2bUL,
    0xfefefefeUL, 0xd7d7d7d7UL, 0xababababUL, 0x76767676UL,
    0xcacacacaUL, 0x82828282UL, 0xc9c9c9c9UL, 0x7d7d7d7dUL,
    0xfafafafaUL, 0x59595959UL, 0x47474747UL, 0xf0f0f0f0UL,
    0xadadadadUL, 0xd4d4d4d4UL, 0xa2a2a2a2UL, 0xafafafafUL,
    0x9c9c9c9cUL, 0xa4a4a4a4UL, 0x72727272UL, 0xc0c0c0c0UL,
    0xb7b7b7b7UL, 0xfdfdfdfdUL, 0x93939393UL, 0x26262626UL,
    0x36363636UL, 0x3f3f3f3fUL, 0xf7f7f7f7UL, 0xccccccccUL,
    0x34343434UL, 0xa5a5a5a5UL, 0xe5e5e5e5UL, 0xf1f1f1f1UL,
    0x71717171UL, 0xd8d8d8d8UL, 0x31313131UL, 0x15151515UL,
    0x04040404UL, 0xc7c7c7c7UL, 0x23232323UL, 0xc3c3c3c3UL,
    0x18181818UL, 0x96969696UL, 0x05050505UL, 0x9a9a9a9aUL,
    0x07070707UL, 0x12121212UL, 0x80808080UL, 0xe2e2e2e2UL,
    0xebebebebUL, 0x27272727UL, 0xb2b2b2b2UL, 0x75757575UL,
    0x09090909UL, 0x83838383UL, 0x2c2c2c2cUL, 0x1a1a1a1aUL,
    0x1b1b1b1bUL, 0x6e6e6e6eUL, 0x5a5a5a5aUL, 0xa0a0a0a0UL,
    0x52525252UL, 0x3b3b3b3bUL, 0xd6d6d6d6UL, 0xb3b3b3b3UL,
    0x29292929UL, 0xe3e3e3e3UL, 0x2f2f2f2fUL, 0x84848484UL,
    0x53535353UL, 0xd1d1d1d1UL, 0x00000000UL, 0xededededUL,
    0x20202020UL, 0xfcfcfcfcUL, 0xb1b1b1b1UL, 0x5b5b5b5bUL,
    0x6a6a6a6aUL, 0xcbcbcbcbUL, 0xbebebebeUL, 0x39393939UL,
    0x4a4a4a4aUL, 0x4c4c4c4cUL, 0x58585858UL, 0xcfcfcfcfUL,
    0xd0d0d0d0UL, 0xefefefefUL, 0xaaaaaaaaUL, 0xfbfbfbfbUL,
    0x43434343UL, 0x4d4d4d4dUL, 0x33333333UL, 0x85858585UL,
    0x45454545UL, 0xf9f9f9f9UL, 0x02020202UL, 0x7f7f7f7fUL,
    0x50505050UL, 0x3c3c3c3cUL, 0x9f9f9f9fUL, 0xa8a8a8a8UL,
    0x51515151UL, 0xa3a3a3a3UL, 0x40404040UL, 0x8f8f8f8fUL,
    0x92929292UL, 0x9d9d9d9dUL, 0x38383838UL, 0xf5f5f5f5UL,
    0xbcbcbcbcUL, 0xb6b6b6b6UL, 0xdadadadaUL, 0x21212121UL,
    0x10101010UL, 0xffffffffUL, 0xf3f3f3f3UL, 0xd2d2d2d2UL,
    0xcdcdcdcdUL, 0x0c0c0c0cUL, 0x13131313UL, 0xececececUL,
    0x5f5f5f5fUL, 0x97979797UL, 0x44444444UL, 0x17171717UL,
    0xc4c4c4c4UL, 0xa7a7a7a7UL, 0x7e7e7e7eUL, 0x3d3d3d3dUL,
    0x64646464UL, 0x5d5d5d5dUL, 0x19191919UL, 0x73737373UL,
    0x60606060UL, 0x81818181UL, 0x4f4f4f4fUL, 0xdcdcdcdcUL,
    0x22222222UL, 0x2a2a2a2aUL, 0x90909090UL, 0x88888888UL,
    0x46464646UL, 0xeeeeeeeeUL, 0xb8b8b8b8UL, 0x14141414UL,
    0xdedededeUL, 0x5e5e5e5eUL, 0x0b0b0b0bUL, 0xdbdbdbdbUL,
    0xe0e0e0e0UL, 0x32323232UL, 0x3a3a3a3aUL, 0x0a0a0a0aUL,
    0x49494949UL, 0x06060606UL, 0x24242424UL, 0x5c5c5c5cUL,
    0xc2c2c2c2UL, 0xd3d3d3d3UL, 0xacacacacUL, 0x62626262UL,
    0x91919191UL, 0x95959595UL, 0xe4e4e4e4UL, 0x79797979UL,
    0xe7e7e7e7UL, 0xc8c8c8c8UL, 0x37373737UL, 0x6d6d6d6dUL,
    0x8d8d8d8dUL, 0xd5d5d5d5UL, 0x4e4e4e4eUL, 0xa9a9a9a9UL,
    0x6c6c6c6cUL, 0x56565656UL, 0xf4f4f4f4UL, 0xeaeaeaeaUL,
    0x65656565UL, 0x7a7a7a7aUL, 0xaeaeaeaeUL, 0x08080808UL,
    0xbabababaUL, 0x78787878UL, 0x25252525UL, 0x2e2e2e2eUL,
    0x1c1c1c1cUL, 0xa6a6a6a6UL, 0xb4b4b4b4UL, 0xc6c6c6c6UL,
    0xe8e8e8e8UL, 0xddddddddUL, 0x74747474UL, 0x1f1f1f1fUL,
    0x4b4b4b4bUL, 0xbdbdbdbdUL, 0x8b8b8b8bUL, 0x8a8a8a8aUL,
    0x70707070UL, 0x3e3e3e3eUL, 0xb5b5b5b5UL, 0x66666666UL,
    0x48484848UL, 0x03030303UL, 0xf6f6f6f6UL, 0x0e0e0e0eUL,
    0x61616161UL, 0x35353535UL, 0x57575757UL, 0xb9b9b9b9UL,
    0x86868686UL, 0xc1c1c1c1UL, 0x1d1d1d1dUL, 0x9e9e9e9eUL,
    0xe1e1e1e1UL, 0xf8f8f8f8UL, 0x98989898UL, 0x11111111UL,
    0x69696969UL, 0xd9d9d9d9UL, 0x8e8e8e8eUL, 0x94949494UL,
    0x9b9b9b9bUL, 0x1e1e1e1eUL, 0x87878787UL, 0xe9e9e9e9UL,
    0xcecececeUL, 0x55555555UL, 0x28282828UL, 0xdfdfdfdfUL,
    0x8c8c8c8cUL, 0xa1a1a1a1UL, 0x89898989UL, 0x0d0d0d0dUL,
    0xbfbfbfbfUL, 0xe6e6e6e6UL, 0x42424242UL, 0x68686868UL,
    0x41414141UL, 0x99999999UL, 0x2d2d2d2dUL, 0x0f0f0f0fUL,
    0xb0b0b0b0UL, 0x54545454UL, 0xbbbbbbbbUL, 0x16161616UL,
};

#ifndef ENCRYPT_ONLY

static const ulong32 TD0[256] = {
    0x51f4a750UL, 0x7e416553UL, 0x1a17a4c3UL, 0x3a275e96UL,
    0x3bab6bcbUL, 0x1f9d45f1UL, 0xacfa58abUL, 0x4be30393UL,
    0x2030fa55UL, 0xad766df6UL, 0x88cc7691UL, 0xf5024c25UL,
    0x4fe5d7fcUL, 0xc52acbd7UL, 0x26354480UL, 0xb562a38fUL,
    0xdeb15a49UL, 0x25ba1b67UL, 0x45ea0e98UL, 0x5dfec0e1UL,
    0xc32f7502UL, 0x814cf012UL, 0x8d4697a3UL, 0x6bd3f9c6UL,
    0x038f5fe7UL, 0x15929c95UL, 0xbf6d7aebUL, 0x955259daUL,
    0xd4be832dUL, 0x587421d3UL, 0x49e06929UL, 0x8ec9c844UL,
    0x75c2896aUL, 0xf48e7978UL, 0x99583e6bUL, 0x27b971ddUL,
    0xbee14fb6UL, 0xf088ad17UL, 0xc920ac66UL, 0x7dce3ab4UL,
    0x63df4a18UL, 0xe51a3182UL, 0x97513360UL, 0x62537f45UL,
    0xb16477e0UL, 0xbb6bae84UL, 0xfe81a01cUL, 0xf9082b94UL,
    0x70486858UL, 0x8f45fd19UL, 0x94de6c87UL, 0x527bf8b7UL,
    0xab73d323UL, 0x724b02e2UL, 0xe31f8f57UL, 0x6655ab2aUL,
    0xb2eb2807UL, 0x2fb5c203UL, 0x86c57b9aUL, 0xd33708a5UL,
    0x302887f2UL, 0x23bfa5b2UL, 0x02036abaUL, 0xed16825cUL,
    0x8acf1c2bUL, 0xa779b492UL, 0xf307f2f0UL, 0x4e69e2a1UL,
    0x65daf4cdUL, 0x0605bed5UL, 0xd134621fUL, 0xc4a6fe8aUL,
    0x342e539dUL, 0xa2f355a0UL, 0x058ae132UL, 0xa4f6eb75UL,
    0x0b83ec39UL, 0x4060efaaUL, 0x5e719f06UL, 0xbd6e1051UL,
    0x3e218af9UL, 0x96dd063dUL, 0xdd3e05aeUL, 0x4de6bd46UL,
    0x91548db5UL, 0x71c45d05UL, 0x0406d46fUL, 0x605015ffUL,
    0x1998fb24UL, 0xd6bde997UL, 0x894043ccUL, 0x67d99e77UL,
    0xb0e842bdUL, 0x07898b88UL, 0xe7195b38UL, 0x79c8eedbUL,
    0xa17c0a47UL, 0x7c420fe9UL, 0xf8841ec9UL, 0x00000000UL,
    0x09808683UL, 0x322bed48UL, 0x1e1170acUL, 0x6c5a724eUL,
    0xfd0efffbUL, 0x0f853856UL, 0x3daed51eUL, 0x362d3927UL,
    0x0a0fd964UL, 0x685ca621UL, 0x9b5b54d1UL, 0x24362e3aUL,
    0x0c0a67b1UL, 0x9357e70fUL, 0xb4ee96d2UL, 0x1b9b919eUL,
    0x80c0c54fUL, 0x61dc20a2UL, 0x5a774b69UL, 0x1c121a16UL,
    0xe293ba0aUL, 0xc0a02ae5UL, 0x3c22e043UL, 0x121b171dUL,
    0x0e090d0bUL, 0xf28bc7adUL, 0x2db6a8b9UL, 0x141ea9c8UL,
    0x57f11985UL, 0xaf75074cUL, 0xee99ddbbUL, 0xa37f60fdUL,
    0xf701269fUL, 0x5c72f5bcUL, 0x44663bc5UL, 0x5bfb7e34UL,
    0x8b432976UL, 0xcb23c6dcUL, 0xb6edfc68UL, 0xb8e4f163UL,
    0xd731dccaUL, 0x42638510UL, 0x13972240UL, 0x84c61120UL,
    0x854a247dUL, 0xd2bb3df8UL, 0xaef93211UL, 0xc729a16dUL,
    0x1d9e2f4bUL, 0xdcb230f3UL, 0x0d8652ecUL, 0x77c1e3d0UL,
    0x2bb3166cUL, 0xa970b999UL, 0x119448faUL, 0x47e96422UL,
    0xa8fc8cc4UL, 0xa0f03f1aUL, 0x567d2cd8UL, 0x223390efUL,
    0x87494ec7UL, 0xd938d1c1UL, 0x8ccaa2feUL, 0x98d40b36UL,
    0xa6f581cfUL, 0xa57ade28UL, 0xdab78e26UL, 0x3fadbfa4UL,
    0x2c3a9de4UL, 0x5078920dUL, 0x6a5fcc9bUL, 0x547e4662UL,
    0xf68d13c2UL, 0x90d8b8e8UL, 0x2e39f75eUL, 0x82c3aff5UL,
    0x9f5d80beUL, 0x69d0937cUL, 0x6fd52da9UL, 0xcf2512b3UL,
    0xc8ac993bUL, 0x10187da7UL, 0xe89c636eUL, 0xdb3bbb7bUL,
    0xcd267809UL, 0x6e5918f4UL, 0xec9ab701UL, 0x834f9aa8UL,
    0xe6956e65UL, 0xaaffe67eUL, 0x21bccf08UL, 0xef15e8e6UL,
    0xbae79bd9UL, 0x4a6f36ceUL, 0xea9f09d4UL, 0x29b07cd6UL,
    0x31a4b2afUL, 0x2a3f2331UL, 0xc6a59430UL, 0x35a266c0UL,
    0x744ebc37UL, 0xfc82caa6UL, 0xe090d0b0UL, 0x33a7d815UL,
    0xf104984aUL, 0x41ecdaf7UL, 0x7fcd500eUL, 0x1791f62fUL,
    0x764dd68dUL, 0x43efb04dUL, 0xccaa4d54UL, 0xe49604dfUL,
    0x9ed1b5e3UL, 0x4c6a881bUL, 0xc12c1fb8UL, 0x4665517fUL,
    0x9d5eea04UL, 0x018c355dUL, 0xfa877473UL, 0xfb0b412eUL,
    0xb3671d5aUL, 0x92dbd252UL, 0xe9105633UL, 0x6dd64713UL,
    0x9ad7618cUL, 0x37a10c7aUL, 0x59f8148eUL, 0xeb133c89UL,
    0xcea927eeUL, 0xb761c935UL, 0xe11ce5edUL, 0x7a47b13cUL,
    0x9cd2df59UL, 0x55f2733fUL, 0x1814ce79UL, 0x73c737bfUL,
    0x53f7cdeaUL, 0x5ffdaa5bUL, 0xdf3d6f14UL, 0x7844db86UL,
    0xcaaff381UL, 0xb968c43eUL, 0x3824342cUL, 0xc2a3405fUL,
    0x161dc372UL, 0xbce2250cUL, 0x283c498bUL, 0xff0d9541UL,
    0x39a80171UL, 0x080cb3deUL, 0xd8b4e49cUL, 0x6456c190UL,
    0x7bcb8461UL, 0xd532b670UL, 0x486c5c74UL, 0xd0b85742UL,
};

static const ulong32 Td4[256] = {
    0x52525252UL, 0x09090909UL, 0x6a6a6a6aUL, 0xd5d5d5d5UL,
    0x30303030UL, 0x36363636UL, 0xa5a5a5a5UL, 0x38383838UL,
    0xbfbfbfbfUL, 0x40404040UL, 0xa3a3a3a3UL, 0x9e9e9e9eUL,
    0x81818181UL, 0xf3f3f3f3UL, 0xd7d7d7d7UL, 0xfbfbfbfbUL,
    0x7c7c7c7cUL, 0xe3e3e3e3UL, 0x39393939UL, 0x82828282UL,
    0x9b9b9b9bUL, 0x2f2f2f2fUL, 0xffffffffUL, 0x87878787UL,
    0x34343434UL, 0x8e8e8e8eUL, 0x43434343UL, 0x44444444UL,
    0xc4c4c4c4UL, 0xdedededeUL, 0xe9e9e9e9UL, 0xcbcbcbcbUL,
    0x54545454UL, 0x7b7b7b7bUL, 0x94949494UL, 0x32323232UL,
    0xa6a6a6a6UL, 0xc2c2c2c2UL, 0x23232323UL, 0x3d3d3d3dUL,
    0xeeeeeeeeUL, 0x4c4c4c4cUL, 0x95959595UL, 0x0b0b0b0bUL,
    0x42424242UL, 0xfafafafaUL, 0xc3c3c3c3UL, 0x4e4e4e4eUL,
    0x08080808UL, 0x2e2e2e2eUL, 0xa1a1a1a1UL, 0x66666666UL,
    0x28282828UL, 0xd9d9d9d9UL, 0x24242424UL, 0xb2b2b2b2UL,
    0x76767676UL, 0x5b5b5b5bUL, 0xa2a2a2a2UL, 0x49494949UL,
    0x6d6d6d6dUL, 0x8b8b8b8bUL, 0xd1d1d1d1UL, 0x25252525UL,
    0x72727272UL, 0xf8f8f8f8UL, 0xf6f6f6f6UL, 0x64646464UL,
    0x86868686UL, 0x68686868UL, 0x98989898UL, 0x16161616UL,
    0xd4d4d4d4UL, 0xa4a4a4a4UL, 0x5c5c5c5cUL, 0xccccccccUL,
    0x5d5d5d5dUL, 0x65656565UL, 0xb6b6b6b6UL, 0x92929292UL,
    0x6c6c6c6cUL, 0x70707070UL, 0x48484848UL, 0x50505050UL,
    0xfdfdfdfdUL, 0xededededUL, 0xb9b9b9b9UL, 0xdadadadaUL,
    0x5e5e5e5eUL, 0x15151515UL, 0x46464646UL, 0x57575757UL,
    0xa7a7a7a7UL, 0x8d8d8d8dUL, 0x9d9d9d9dUL, 0x84848484UL,
    0x90909090UL, 0xd8d8d8d8UL, 0xababababUL, 0x00000000UL,
    0x8c8c8c8cUL, 0xbcbcbcbcUL, 0xd3d3d3d3UL, 0x0a0a0a0aUL,
    0xf7f7f7f7UL, 0xe4e4e4e4UL, 0x58585858UL, 0x05050505UL,
    0xb8b8b8b8UL, 0xb3b3b3b3UL, 0x45454545UL, 0x06060606UL,
    0xd0d0d0d0UL, 0x2c2c2c2cUL, 0x1e1e1e1eUL, 0x8f8f8f8fUL,
    0xcacacacaUL, 0x3f3f3f3fUL, 0x0f0f0f0fUL, 0x02020202UL,
    0xc1c1c1c1UL, 0xafafafafUL, 0xbdbdbdbdUL, 0x03030303UL,
    0x01010101UL, 0x13131313UL, 0x8a8a8a8aUL, 0x6b6b6b6bUL,
    0x3a3a3a3aUL, 0x91919191UL, 0x11111111UL, 0x41414141UL,
    0x4f4f4f4fUL, 0x67676767UL, 0xdcdcdcdcUL, 0xeaeaeaeaUL,
    0x97979797UL, 0xf2f2f2f2UL, 0xcfcfcfcfUL, 0xcecececeUL,
    0xf0f0f0f0UL, 0xb4b4b4b4UL, 0xe6e6e6e6UL, 0x73737373UL,
    0x96969696UL, 0xacacacacUL, 0x74747474UL, 0x22222222UL,
    0xe7e7e7e7UL, 0xadadadadUL, 0x35353535UL, 0x85858585UL,
    0xe2e2e2e2UL, 0xf9f9f9f9UL, 0x37373737UL, 0xe8e8e8e8UL,
    0x1c1c1c1cUL, 0x75757575UL, 0xdfdfdfdfUL, 0x6e6e6e6eUL,
    0x47474747UL, 0xf1f1f1f1UL, 0x1a1a1a1aUL, 0x71717171UL,
    0x1d1d1d1dUL, 0x29292929UL, 0xc5c5c5c5UL, 0x89898989UL,
    0x6f6f6f6fUL, 0xb7b7b7b7UL, 0x62626262UL, 0x0e0e0e0eUL,
    0xaaaaaaaaUL, 0x18181818UL, 0xbebebebeUL, 0x1b1b1b1bUL,
    0xfcfcfcfcUL, 0x56565656UL, 0x3e3e3e3eUL, 0x4b4b4b4bUL,
    0xc6c6c6c6UL, 0xd2d2d2d2UL, 0x79797979UL, 0x20202020UL,
    0x9a9a9a9aUL, 0xdbdbdbdbUL, 0xc0c0c0c0UL, 0xfefefefeUL,
    0x78787878UL, 0xcdcdcdcdUL, 0x5a5a5a5aUL, 0xf4f4f4f4UL,
    0x1f1f1f1fUL, 0xddddddddUL, 0xa8a8a8a8UL, 0x33333333UL,
    0x88888888UL, 0x07070707UL, 0xc7c7c7c7UL, 0x31313131UL,
    0xb1b1b1b1UL, 0x12121212UL, 0x10101010UL, 0x59595959UL,
    0x27272727UL, 0x80808080UL, 0xececececUL, 0x5f5f5f5fUL,
    0x60606060UL, 0x51515151UL, 0x7f7f7f7fUL, 0xa9a9a9a9UL,
    0x19191919UL, 0xb5b5b5b5UL, 0x4a4a4a4aUL, 0x0d0d0d0dUL,
    0x2d2d2d2dUL, 0xe5e5e5e5UL, 0x7a7a7a7aUL, 0x9f9f9f9fUL,
    0x93939393UL, 0xc9c9c9c9UL, 0x9c9c9c9cUL, 0xefefefefUL,
    0xa0a0a0a0UL, 0xe0e0e0e0UL, 0x3b3b3b3bUL, 0x4d4d4d4dUL,
    0xaeaeaeaeUL, 0x2a2a2a2aUL, 0xf5f5f5f5UL, 0xb0b0b0b0UL,
    0xc8c8c8c8UL, 0xebebebebUL, 0xbbbbbbbbUL, 0x3c3c3c3cUL,
    0x83838383UL, 0x53535353UL, 0x99999999UL, 0x61616161UL,
    0x17171717UL, 0x2b2b2b2bUL, 0x04040404UL, 0x7e7e7e7eUL,
    0xbabababaUL, 0x77777777UL, 0xd6d6d6d6UL, 0x26262626UL,
    0xe1e1e1e1UL, 0x69696969UL, 0x14141414UL, 0x63636363UL,
    0x55555555UL, 0x21212121UL, 0x0c0c0c0cUL, 0x7d7d7d7dUL,
};
#endif /* ENCRYPT_ONLY */

#define Te0(x) TE0[x]
#define Te1(x) RORc(TE0[x], 8)
#define Te2(x) RORc(TE0[x], 16)
#define Te3(x) RORc(TE0[x], 24)

#define Td0(x) TD0[x]
#define Td1(x) RORc(TD0[x], 8)
#define Td2(x) RORc(TD0[x], 16)
#define Td3(x) RORc(TD0[x], 24)

#define Te4_0 0x000000FF & Te4
#define Te4_1 0x0000FF00 & Te4
#define Te4_2 0x00FF0000 & Te4
#define Te4_3 0xFF000000 & Te4

static const ulong32 rcon[] = {
    0x01000000UL, 0x02000000UL, 0x04000000UL, 0x08000000UL,
    0x10000000UL, 0x20000000UL, 0x40000000UL, 0x80000000UL,
    0x1B000000UL, 0x36000000UL, /* for 128-bit blocks, Rijndael never uses more than 10 rcon values */
};

/* This part extracted from aes.c in libtomcrypt 1.17 */

#ifndef ENCRYPT_ONLY 

#define SETUP    rijndael_setup
#define ECB_ENC  rijndael_ecb_encrypt
#define ECB_DEC  rijndael_ecb_decrypt
#define ECB_DONE rijndael_done
#define ECB_TEST rijndael_test
#define ECB_KS   rijndael_keysize

static ulong32 setup_mix(ulong32 temp)
{
   return (Te4_3[byte(temp, 2)]) ^
          (Te4_2[byte(temp, 1)]) ^
          (Te4_1[byte(temp, 0)]) ^
          (Te4_0[byte(temp, 3)]);
}
#endif

#ifndef ENCRYPT_ONLY
static ulong32 setup_mix2(ulong32 temp)
{
   return Td0(255 & Te4[byte(temp, 3)]) ^
          Td1(255 & Te4[byte(temp, 2)]) ^
          Td2(255 & Te4[byte(temp, 1)]) ^
          Td3(255 & Te4[byte(temp, 0)]);
}
#endif

 /**
    Initialize the AES (Rijndael) block cipher
    @param key The symmetric key you wish to pass
    @param keylen The key length in bytes
    @param num_rounds The number of rounds desired (0 for default)
    @param skey The key in as scheduled by this function.
    @return CRYPT_OK if successful
 */
int SETUP(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey)
{
    int i, j;
    ulong32 temp, *rk;
#ifndef ENCRYPT_ONLY
    ulong32 *rrk;
#endif    
    LTC_ARGCHK(key  != NULL);
    LTC_ARGCHK(skey != NULL);

    if (keylen != 16 && keylen != 24 && keylen != 32) {
       return CRYPT_INVALID_KEYSIZE;
    }

    if (num_rounds != 0 && num_rounds != (10 + ((keylen/8)-2)*2)) {
       return CRYPT_INVALID_ROUNDS;
    }

    skey->rijndael.Nr = 10 + ((keylen/8)-2)*2;

    /* setup the forward key */
    i                 = 0;
    rk                = skey->rijndael.eK;
    LOAD32H(rk[0], key     );
    LOAD32H(rk[1], key +  4);
    LOAD32H(rk[2], key +  8);
    LOAD32H(rk[3], key + 12);
    if (keylen == 16) {
        j = 44;
        for (;;) {
            temp  = rk[3];
            rk[4] = rk[0] ^ setup_mix(temp) ^ rcon[i];
            rk[5] = rk[1] ^ rk[4];
            rk[6] = rk[2] ^ rk[5];
            rk[7] = rk[3] ^ rk[6];
            if (++i == 10) {
               break;
            }
            rk += 4;
        }
    } else if (keylen == 24) {
        j = 52;   
        LOAD32H(rk[4], key + 16);
        LOAD32H(rk[5], key + 20);
        for (;;) {
        #ifdef _MSC_VER
            temp = skey->rijndael.eK[rk - skey->rijndael.eK + 5]; 
        #else
            temp = rk[5];
        #endif
            rk[ 6] = rk[ 0] ^ setup_mix(temp) ^ rcon[i];
            rk[ 7] = rk[ 1] ^ rk[ 6];
            rk[ 8] = rk[ 2] ^ rk[ 7];
            rk[ 9] = rk[ 3] ^ rk[ 8];
            if (++i == 8) {
                break;
            }
            rk[10] = rk[ 4] ^ rk[ 9];
            rk[11] = rk[ 5] ^ rk[10];
            rk += 6;
        }
    } else if (keylen == 32) {
        j = 60;
        LOAD32H(rk[4], key + 16);
        LOAD32H(rk[5], key + 20);
        LOAD32H(rk[6], key + 24);
        LOAD32H(rk[7], key + 28);
        for (;;) {
        #ifdef _MSC_VER
            temp = skey->rijndael.eK[rk - skey->rijndael.eK + 7]; 
        #else
            temp = rk[7];
        #endif
            rk[ 8] = rk[ 0] ^ setup_mix(temp) ^ rcon[i];
            rk[ 9] = rk[ 1] ^ rk[ 8];
            rk[10] = rk[ 2] ^ rk[ 9];
            rk[11] = rk[ 3] ^ rk[10];
            if (++i == 7) {
                break;
            }
            temp = rk[11];
            rk[12] = rk[ 4] ^ setup_mix(RORc(temp, 8));
            rk[13] = rk[ 5] ^ rk[12];
            rk[14] = rk[ 6] ^ rk[13];
            rk[15] = rk[ 7] ^ rk[14];
            rk += 8;
        }
    } else {
       /* this can't happen */
       return CRYPT_ERROR;
    }

#ifndef ENCRYPT_ONLY    
    /* setup the inverse key now */
    rk   = skey->rijndael.dK;
    rrk  = skey->rijndael.eK + j - 4; 

    /* apply the inverse MixColumn transform to all round keys but the first and the last: */
    /* copy first */
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk   = *rrk;
    rk -= 3; rrk -= 3;

    for (i = 1; i < skey->rijndael.Nr; i++) {
        rrk -= 4;
        rk  += 4;
        temp = rrk[0];
        rk[0] = setup_mix2(temp);
        temp = rrk[1];
        rk[1] = setup_mix2(temp);
        temp = rrk[2];
        rk[2] = setup_mix2(temp);
        temp = rrk[3];
        rk[3] = setup_mix2(temp);
    }

    /* copy last */
    rrk -= 4;
    rk  += 4;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk++ = *rrk++;
    *rk   = *rrk;
#endif /* ENCRYPT_ONLY */

    return CRYPT_OK;   
}

/**
  Encrypts a block of text with AES
  @param pt The input plaintext (16 bytes)
  @param ct The output ciphertext (16 bytes)
  @param skey The key as scheduled
  @return CRYPT_OK if successful
*/
int ECB_ENC(const unsigned char *pt, unsigned char *ct, symmetric_key *skey)
{
    ulong32 s0, s1, s2, s3, t0, t1, t2, t3, *rk;
    int Nr, r;

    LTC_ARGCHK(pt != NULL);
    LTC_ARGCHK(ct != NULL);
    LTC_ARGCHK(skey != NULL);

    Nr = skey->rijndael.Nr;
    rk = skey->rijndael.eK;

    /*
     * map byte array block to cipher state
     * and add initial round key:
     */
    LOAD32H(s0, pt      ); s0 ^= rk[0];
    LOAD32H(s1, pt  +  4); s1 ^= rk[1];
    LOAD32H(s2, pt  +  8); s2 ^= rk[2];
    LOAD32H(s3, pt  + 12); s3 ^= rk[3];

    for (r = 0; ; r++) {
        rk += 4;
        t0 =
            Te0(byte(s0, 3)) ^
            Te1(byte(s1, 2)) ^
            Te2(byte(s2, 1)) ^
            Te3(byte(s3, 0)) ^
            rk[0];
        t1 =
            Te0(byte(s1, 3)) ^
            Te1(byte(s2, 2)) ^
            Te2(byte(s3, 1)) ^
            Te3(byte(s0, 0)) ^
            rk[1];
        t2 =
            Te0(byte(s2, 3)) ^
            Te1(byte(s3, 2)) ^
            Te2(byte(s0, 1)) ^
            Te3(byte(s1, 0)) ^
            rk[2];
        t3 =
            Te0(byte(s3, 3)) ^
            Te1(byte(s0, 2)) ^
            Te2(byte(s1, 1)) ^
            Te3(byte(s2, 0)) ^
            rk[3];
        if (r == Nr-2) { 
           break;
        }
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    rk += 4;

    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Te4_3[byte(t0, 3)]) ^
        (Te4_2[byte(t1, 2)]) ^
        (Te4_1[byte(t2, 1)]) ^
        (Te4_0[byte(t3, 0)]) ^
        rk[0];
    STORE32H(s0, ct);
    s1 =
        (Te4_3[byte(t1, 3)]) ^
        (Te4_2[byte(t2, 2)]) ^
        (Te4_1[byte(t3, 1)]) ^
        (Te4_0[byte(t0, 0)]) ^
        rk[1];
    STORE32H(s1, ct+4);
    s2 =
        (Te4_3[byte(t2, 3)]) ^
        (Te4_2[byte(t3, 2)]) ^
        (Te4_1[byte(t0, 1)]) ^
        (Te4_0[byte(t1, 0)]) ^
        rk[2];
    STORE32H(s2, ct+8);
    s3 =
        (Te4_3[byte(t3, 3)]) ^
        (Te4_2[byte(t0, 2)]) ^
        (Te4_1[byte(t1, 1)]) ^
        (Te4_0[byte(t2, 0)]) ^ 
        rk[3];
    STORE32H(s3, ct+12);

    return CRYPT_OK;
}

int ECB_DEC(const unsigned char *ct, unsigned char *pt, symmetric_key *skey)
{
    ulong32 s0, s1, s2, s3, t0, t1, t2, t3, *rk;
    int Nr, r;

    LTC_ARGCHK(pt != NULL);
    LTC_ARGCHK(ct != NULL);
    LTC_ARGCHK(skey != NULL);
    
    Nr = skey->rijndael.Nr;
    rk = skey->rijndael.dK;

    /*
     * map byte array block to cipher state
     * and add initial round key:
     */
    LOAD32H(s0, ct      ); s0 ^= rk[0];
    LOAD32H(s1, ct  +  4); s1 ^= rk[1];
    LOAD32H(s2, ct  +  8); s2 ^= rk[2];
    LOAD32H(s3, ct  + 12); s3 ^= rk[3];

    for (r = 0; ; r++) {
        rk += 4;
        t0 =
            Td0(byte(s0, 3)) ^
            Td1(byte(s3, 2)) ^
            Td2(byte(s2, 1)) ^
            Td3(byte(s1, 0)) ^
            rk[0];
        t1 =
            Td0(byte(s1, 3)) ^
            Td1(byte(s0, 2)) ^
            Td2(byte(s3, 1)) ^
            Td3(byte(s2, 0)) ^
            rk[1];
        t2 =
            Td0(byte(s2, 3)) ^
            Td1(byte(s1, 2)) ^
            Td2(byte(s0, 1)) ^
            Td3(byte(s3, 0)) ^
            rk[2];
        t3 =
            Td0(byte(s3, 3)) ^
            Td1(byte(s2, 2)) ^
            Td2(byte(s1, 1)) ^
            Td3(byte(s0, 0)) ^
            rk[3];
        if (r == Nr-2) {
           break; 
        }
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    rk += 4;

    /*
     * apply last round and
     * map cipher state to byte array block:
     */
    s0 =
        (Td4[byte(t0, 3)] & 0xff000000) ^
        (Td4[byte(t3, 2)] & 0x00ff0000) ^
        (Td4[byte(t2, 1)] & 0x0000ff00) ^
        (Td4[byte(t1, 0)] & 0x000000ff) ^
        rk[0];
    STORE32H(s0, pt);
    s1 =
        (Td4[byte(t1, 3)] & 0xff000000) ^
        (Td4[byte(t0, 2)] & 0x00ff0000) ^
        (Td4[byte(t3, 1)] & 0x0000ff00) ^
        (Td4[byte(t2, 0)] & 0x000000ff) ^
        rk[1];
    STORE32H(s1, pt+4);
    s2 =
        (Td4[byte(t2, 3)] & 0xff000000) ^
        (Td4[byte(t1, 2)] & 0x00ff0000) ^
        (Td4[byte(t0, 1)] & 0x0000ff00) ^
        (Td4[byte(t3, 0)] & 0x000000ff) ^
        rk[2];
    STORE32H(s2, pt+8);
    s3 =
        (Td4[byte(t3, 3)] & 0xff000000) ^
        (Td4[byte(t2, 2)] & 0x00ff0000) ^
        (Td4[byte(t1, 1)] & 0x0000ff00) ^
        (Td4[byte(t0, 0)] & 0x000000ff) ^
        rk[3];
    STORE32H(s3, pt+12);

    return CRYPT_OK;
}

/**
  CBC encrypt
  @param pt     Plaintext
  @param ct     [out] Ciphertext
  @param len    The number of bytes to process (must be multiple of block length)
  @param cbc    CBC state
  @return CRYPT_OK if successful
*/
int aes_cbc_encrypt(const unsigned char *pt, unsigned char *ct, unsigned long len, symmetric_CBC *cbc)
{
   int x, err;

   LTC_ARGCHK(pt != NULL);
   LTC_ARGCHK(ct != NULL);
   LTC_ARGCHK(cbc != NULL);

   /* is blocklen valid? */
   if (cbc->blocklen < 1 || cbc->blocklen > (int)sizeof(cbc->IV)) {
      return CRYPT_INVALID_ARG;
   }    

   if (len % cbc->blocklen) {
      return CRYPT_INVALID_ARG;
   }

    while (len) {
         /* xor IV against plaintext */
         for (x = 0; x < cbc->blocklen; x++) {
              cbc->IV[x] ^= pt[x];
         }

         /* encrypt */
         if ((err = ECB_ENC(cbc->IV, ct, &cbc->key)) != CRYPT_OK) {
            return err;
         }

         for (x = 0; x < cbc->blocklen; x++) {
                cbc->IV[x] = ct[x];
          }
        
        ct  += cbc->blocklen;
        pt  += cbc->blocklen;
        len -= cbc->blocklen;
   }
   return CRYPT_OK;
}

/**
  CBC decrypt
  @param ct     Ciphertext
  @param pt     [out] Plaintext
  @param len    The number of bytes to process (must be multiple of block length)
  @param cbc    CBC state
  @return CRYPT_OK if successful
*/
int aes_cbc_decrypt(const unsigned char *ct, unsigned char *pt, unsigned long len, symmetric_CBC *cbc)
{
   int x, err;
   unsigned char tmp[16];
   unsigned char tmpy;

   LTC_ARGCHK(pt  != NULL);
   LTC_ARGCHK(ct  != NULL);
   LTC_ARGCHK(cbc != NULL);
   
   /* is blocklen valid? */
   if (cbc->blocklen < 1 || cbc->blocklen > (int)sizeof(cbc->IV)) {
      return CRYPT_INVALID_ARG;
   }    

   if (len % cbc->blocklen) {
      return CRYPT_INVALID_ARG;
   }
   
    while (len) {
         /* decrypt */
         if ((err = ECB_DEC(ct, tmp, &cbc->key)) != CRYPT_OK) {
            return err;
         }

         for (x = 0; x < cbc->blocklen; x++) {
               tmpy       = tmp[x] ^ cbc->IV[x];
               cbc->IV[x] = ct[x];
               pt[x]      = tmpy;
         }
       
         ct  += cbc->blocklen;
         pt  += cbc->blocklen;
         len -= cbc->blocklen;
   }
   return CRYPT_OK;
}

void
fz_aesinit(fz_aes *aes, unsigned char *key, unsigned keylen)
{
	int res;
	aes->ivinited = 0;
	aes->cbckey.blocklen = 16;
	res = SETUP(key, keylen, 0, &aes->cbckey.key);
	assert(CRYPT_OK == res);
}

void fz_setiv(fz_aes *aes, unsigned char *iv)
{
	memmove(aes->cbckey.IV, iv, 16);
	aes->ivinited = 1;
}

void
fz_aesdecrypt(fz_aes *aes, unsigned char *dest, unsigned char *src, unsigned len)
{
	assert(aes->ivinited);
	if (len > 0)
		aes_cbc_decrypt(src, dest, len, &aes->cbckey);
}