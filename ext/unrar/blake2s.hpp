// Based on public domain code written in 2012 by Samuel Neves
#ifndef _RAR_BLAKE2_
#define _RAR_BLAKE2_

#define BLAKE2_DIGEST_SIZE 32

enum blake2s_constant
{
  BLAKE2S_BLOCKBYTES = 64,
  BLAKE2S_OUTBYTES   = 32
};


// Alignment to 64 improves performance of both SSE and non-SSE versions.
// Alignment to n*16 is required for SSE version, so we selected 64.
// We use the custom alignment scheme instead of __declspec(align(x)),
// because it is less compiler dependent. Also the compiler directive
// does not help if structure is a member of class allocated through
// 'new' operator.
struct blake2s_state
{
  enum { BLAKE_ALIGNMENT = 64 };

  byte ubuf[48 + 2 * BLAKE2S_BLOCKBYTES + BLAKE_ALIGNMENT];

  byte   *buf;       // byte   buf[2 * BLAKE2S_BLOCKBYTES].
  uint32 *h, *t, *f; // uint32 h[8], t[2], f[2].

  size_t   buflen;
  byte  last_node;

  void init()
  {
    memset( this, 0, sizeof( blake2s_state ) );
    buf = (byte *) ALIGN_VALUE(ubuf, BLAKE_ALIGNMENT);
    h   = (uint32 *) (buf + 2 * BLAKE2S_BLOCKBYTES);
    t   = h + 8;
    f   = t + 2;
  }
};


#ifdef RAR_SMP
class ThreadPool;
#endif

typedef struct __blake2sp_state
{
  blake2s_state S[8][1];
  blake2s_state R[1];
  byte buf[8 * BLAKE2S_BLOCKBYTES];
  size_t buflen;

#ifdef RAR_SMP
  ThreadPool *ThPool;
  uint MaxThreads;
#endif

} blake2sp_state;

void blake2sp_init( blake2sp_state *S );
void blake2sp_update( blake2sp_state *S, const byte *in, size_t inlen );
void blake2sp_final( blake2sp_state *S, byte *digest );

#endif

