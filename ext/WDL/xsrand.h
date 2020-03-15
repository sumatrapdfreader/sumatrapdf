#ifndef _WDL_XSRAND_H_
#define _WDL_XSRAND_H_

#include "wdltypes.h"

class XS64Rand {
    WDL_UINT64 st;
  public:
    XS64Rand(WDL_UINT64 seed) { st=seed; if (!st) st++; }

    WDL_UINT64 rand64()
    {
      WDL_UINT64 x=st;
      x ^= x >> 12; // a
      x ^= x << 25; // b
      x ^= x >> 27; // c
      return (st=x) * WDL_UINT64_CONST(2685821657736338717);
    }

    void add_entropy(WDL_UINT64 value) { st+=value; if (!st) st++; }
};

class XS1024Rand {
     WDL_UINT64 st[16];
     int p;

  public:
     XS1024Rand(WDL_UINT64 seed) : p(0)
     {
       memset(st,0x80,sizeof(st));
       add_entropy(seed);
     }
     XS1024Rand(const void *buf, int bufsz) : p(0)
     {
       memset(st,0x80,sizeof(st));
       add_entropy(buf,bufsz);
     }

     WDL_UINT64 rand64()
     {
       WDL_UINT64 s0 = st[ p ], *wr = st + (p = (p+1)&15), s1 = *wr;
       s1 ^= s1 << 31; // a
       s1 ^= s1 >> 11; // b
       s0 ^= s0 >> 30; // c
       return ( *wr = s0 ^ s1 ) * WDL_UINT64_CONST(1181783497276652981);
     }

     void add_entropy(WDL_UINT64 value)
     {
       XS64Rand r(value);
       for (int x=0;x<16;x++) st[x]+=r.rand64();
     }
     void add_entropy(const void *buf, int bufsz)
     {
       unsigned char *wr = (unsigned char *)st;
       if (bufsz > (int) sizeof(st)) bufsz = (int) sizeof(st);
       if (buf)
       {
         const unsigned char *rd = (const unsigned char *)buf;
         while (bufsz-->0) *wr++ += *rd++;
       }
     }
};


#endif
