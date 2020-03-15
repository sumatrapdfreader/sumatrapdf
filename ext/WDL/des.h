/* Loosely based on:
 *
 * D3DES (V5.09) -
 *
 * A portable, public domain, version of the Data Encryption Standard.
 *
 * Written with Symantec's THINK (Lightspeed) C by Richard Outerbridge.
 *
 * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge.
 * (GEnie : OUTER; CIS : [71755,204]) Graven Imagery, 1992.
 */


#ifndef _WDL_DES_H_
#define _WDL_DES_H_


class WDL_DES
{
public:
  WDL_DES();
  ~WDL_DES();

  void SetKey(const unsigned char *key8, bool isEncrypt);

  void Process8(unsigned char *buf8);

private:

  unsigned int m_keydata[32];

};

#endif