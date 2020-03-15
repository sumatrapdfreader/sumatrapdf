#ifndef _WDL_ADPCM_ENCODE_H_
#define _WDL_ADPCM_ENCODE_H_


#include "pcmfmtcvt.h"

void WDL_adpcm_encode_IMA(PCMFMTCVT_DBL_TYPE *samples, int numsamples, int nch, int bps,
                          unsigned char *bufout, int *bufout_used, short **predState);

#define WDL_adpcm_encode_IMA_samplesneededbytes(bytes,bps) ((((bytes)-4)*8)/(bps)+1)


// untested. also probably slow.
#ifdef WDL_ADPCM_ENCODE_IMPL


static signed char ima_adpcm_index_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8, }; 
static short ima_adpcm_step_table[89] = { 
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 
  19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
  876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
};

static char calcBestNibble(int *initial_step_index, int lastSpl, int thisSpl, short *lastsplout, int bps)
{

  int step=ima_adpcm_step_table[*initial_step_index];

  char sign=0;
  int adiff = thisSpl - lastSpl;
  if (adiff<0) { adiff=-adiff; sign=8; }

  // adiff == (nib*step)/4 + step/8
  // adiff - step/8 = nib*step/4
  // nib = 4*(adiff-step/8)/step = 4*adiff/step - 0.5
  int nib =  step ? ((4 * adiff - step/2) / step) : 0;
  if (nib<0) nib=0;
  else if(nib>7) nib=7;

  if (bps==2) nib&=4;

  int diff = (nib*step)/4 + step/8;
  *lastsplout = lastSpl + (sign?-diff:diff);


  *initial_step_index += ima_adpcm_index_table[nib];

  if (*initial_step_index<0)*initial_step_index=0;
  else if (*initial_step_index>88)*initial_step_index=88;

  return (char)nib|sign;
}



void WDL_adpcm_encode_IMA(PCMFMTCVT_DBL_TYPE *samples, int numsamples, int nch, int bps,
                          unsigned char *bufout, int *bufout_used, short **predState)
{
  int x;
  if (!*predState) *predState=(short *)calloc(nch,sizeof(short));

  short *pstate = *predState;
  for(x=0;x<nch;x++)
  {
    int left=numsamples;
    PCMFMTCVT_DBL_TYPE *spl = samples+x;
    unsigned char *wrptr = bufout + x*4;

    int step_index=pstate[x];

    short last_out;
    double_TO_INT16(last_out,*spl);
    left--;

    *wrptr++ = last_out&0xff; *wrptr++ = last_out>>8;
    spl+=nch;

    *wrptr++ = step_index&0xff; *wrptr++ = step_index>>8;

    wrptr += (nch-1)*4;

    
    int outpos=0;
    const int outblocklen = bps == 2 ? 16 : 8;
    unsigned char buildchar=0;
    
    while (left-->0)
    {
      short this_spl;
      double_TO_INT16(this_spl,*spl);
      char nib = calcBestNibble(&step_index,last_out,this_spl,&last_out,bps);

      spl+=nch;

      // update output
      if (bps == 2)
      {
        nib>>=2;
        switch (outpos&3)
        {
          case 0: buildchar = nib; break;
          case 1: buildchar |= nib<<2; break;
          case 2: buildchar |= nib<<4; break;
          case 3: *wrptr++ = buildchar | (nib<<6);  break;
        }        
      }
      else
      {
        if (!(outpos&1)) buildchar = nib;
        else *wrptr++ = buildchar | (nib<<4); 
      }

      // skip other channels
      if (++outpos == outblocklen) 
      {
        wrptr += ((nch-1)*outblocklen*bps)/8;
        outpos=0;
      }
    }
    pstate[x] = step_index;
  
  }
  *bufout_used = (((numsamples-1)*bps)/8 + 4)*nch;


}

#endif


#endif//_WDL_ADPCM_ENCODE_H_