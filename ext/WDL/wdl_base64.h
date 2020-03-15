#ifndef _WDL_BASE64_H_
#define _WDL_BASE64_H_

#ifndef WDL_BASE64_FUNCDECL
#define WDL_BASE64_FUNCDECL static
#endif

static const char wdl_base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
WDL_BASE64_FUNCDECL void wdl_base64encode(const unsigned char *in, char *out, int len)
{
  while (len >= 6)
  {
    const int accum = (in[0] << 16) + (in[1] << 8) + in[2];
    const int accum2 = (in[3] << 16) + (in[4] << 8) + in[5];
    out[0] = wdl_base64_alphabet[(accum >> 18) & 0x3F];
    out[1] = wdl_base64_alphabet[(accum >> 12) & 0x3F];
    out[2] = wdl_base64_alphabet[(accum >> 6) & 0x3F];
    out[3] = wdl_base64_alphabet[accum & 0x3F];
    out[4] = wdl_base64_alphabet[(accum2 >> 18) & 0x3F];
    out[5] = wdl_base64_alphabet[(accum2 >> 12) & 0x3F];
    out[6] = wdl_base64_alphabet[(accum2 >> 6) & 0x3F];
    out[7] = wdl_base64_alphabet[accum2 & 0x3F];
    out+=8;
    in+=6;
    len-=6;
  }

  if (len >= 3)
  {
    const int accum = (in[0]<<16)|(in[1]<<8)|in[2];
    out[0] = wdl_base64_alphabet[(accum >> 18) & 0x3F];
    out[1] = wdl_base64_alphabet[(accum >> 12) & 0x3F];
    out[2] = wdl_base64_alphabet[(accum >> 6) & 0x3F];
    out[3] = wdl_base64_alphabet[accum & 0x3F];    
    in+=3;
    len-=3;
    out+=4;
  }

  if (len>0)
  {
    if (len == 2)
    {
      const int accum = (in[0] << 8) | in[1];
      out[0] = wdl_base64_alphabet[(accum >> 10) & 0x3F];
      out[1] = wdl_base64_alphabet[(accum >> 4) & 0x3F];
      out[2] = wdl_base64_alphabet[(accum & 0xF)<<2];
    }
    else
    {
      const int accum = in[0];
      out[0] = wdl_base64_alphabet[(accum >> 2) & 0x3F];
      out[1] = wdl_base64_alphabet[(accum & 0x3)<<4];
      out[2]='=';  
    }
    out[3]='=';  
    out+=4;
  }

  out[0]=0;
}

WDL_BASE64_FUNCDECL int wdl_base64decode(const char *src, unsigned char *dest, int destsize)
{
  static const char *tab = // poolable string
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff\xff\x3f"
    "\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xff\xff\xff"
    "\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
    "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff"
    "\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28"
    "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";


  int accum=0, nbits=0, wpos=0;

  if (destsize <= 0) return 0;

  for (;;)
  {
    const int v=(int)tab[*(unsigned char *)src++];
    if (v<0) return wpos;

    accum += v;
    nbits += 6;   

    if (nbits >= 8)
    {
      nbits-=8;
      dest[wpos] = (accum>>nbits)&0xff;
      if (++wpos >= destsize) return wpos;
    }
    accum <<= 6;
  }
}


#endif
