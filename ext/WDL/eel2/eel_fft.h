#ifndef __EEL_FFT_H_
#define __EEL_FFT_H_

#include "../fft.h"
#if WDL_FFT_REALSIZE != EEL_F_SIZE
#error WDL_FFT_REALSIZE -- EEL_F_SIZE size mismatch
#endif

#ifndef EEL_FFT_MINBITLEN
#define EEL_FFT_MINBITLEN 4
#endif

#ifndef EEL_FFT_MAXBITLEN
#define EEL_FFT_MAXBITLEN 15
#endif

#ifndef EEL_FFT_MINBITLEN_REORDER
#define EEL_FFT_MINBITLEN_REORDER (EEL_FFT_MINBITLEN-1)
#endif

//#define EEL_SUPER_FAST_FFT_REORDERING // quite a bit faster (50-100%) than "normal", but uses a 256kb lookup
//#define EEL_SLOW_FFT_REORDERING // 20%-80% slower than normal, alloca() use, no reason to ever use this

#ifdef EEL_SUPER_FAST_FFT_REORDERING
static int *fft_reorder_table_for_bitsize(int bitsz)
{
  static int s_tab[ (2 << EEL_FFT_MAXBITLEN) + 24*(EEL_FFT_MAXBITLEN-EEL_FFT_MINBITLEN_REORDER+1) ]; // big 256kb table, ugh
  if (bitsz<=EEL_FFT_MINBITLEN_REORDER) return s_tab;
  return s_tab + (1<<bitsz) + (bitsz-EEL_FFT_MINBITLEN_REORDER) * 24;
}
static void fft_make_reorder_table(int bitsz, int *tab)
{
  const int fft_sz=1<<bitsz;
  char flag[1<<EEL_FFT_MAXBITLEN];
  int x;
  int *tabstart = tab;
  memset(flag,0,fft_sz);

  for (x=0;x<fft_sz;x++)
  {
    int fx;
    if (!flag[x] && (fx=WDL_fft_permute(fft_sz,x))!=x)
    {
      flag[x]=1;
      *tab++ = x;
      do
      {
        flag[fx]=1;
        *tab++ = fx;
        fx = WDL_fft_permute(fft_sz, fx);
      }
      while (fx != x);
      *tab++ = 0; // delimit a run
    }
    else flag[x]=1;
  }
  *tab++ = 0; // doublenull terminated
}

static void fft_reorder_buffer(int bitsz, WDL_FFT_COMPLEX *data, int fwd)
{
  const int *tab=fft_reorder_table_for_bitsize(bitsz);
  if (!fwd)
  {
    while (*tab)
    {
      const int sidx=*tab++;
      WDL_FFT_COMPLEX a=data[sidx];
      for (;;)
      {
        WDL_FFT_COMPLEX ta;
        const int idx=*tab++;
        if (!idx) break;
        ta=data[idx];
        data[idx]=a;
        a=ta;
      }
      data[sidx] = a;
    }
  }
  else
  {
    while (*tab)
    {
      const int sidx=*tab++;
      int lidx = sidx;
      const WDL_FFT_COMPLEX sta=data[lidx];
      for (;;)
      {
        const int idx=*tab++;
        if (!idx) break;

        data[lidx]=data[idx];
        lidx=idx;
      }
      data[lidx] = sta;
    }
  }
  return 1;
}
#else
#ifndef EEL_SLOW_FFT_REORDERING
 // moderate speed mode, minus the big 256k table

static void fft_reorder_buffer(int bitsz, WDL_FFT_COMPLEX *data, int fwd)
{
  // this is a good compromise, quite a bit faster than out of place reordering, but no separate 256kb lookup required
  /*
  these generated via:
      static void fft_make_reorder_table(int bitsz)
      {
        int fft_sz=1<<bitsz,x;
        char flag[65536]={0,};
        printf("static const int tab%d[]={ ",fft_sz);
        for (x=0;x<fft_sz;x++)
        {
          int fx;
          if (!flag[x] && (fx=WDL_fft_permute(fft_sz,x))!=x)
          {
            printf("%d, ",x);
            do { flag[fx]=1; fx = WDL_fft_permute(fft_sz, fx); } while (fx != x);
          }
          flag[x]=1;
        }
        printf(" 0 };\n");
      }
      */

  static const int tab4_8_32[]={ 1,  0 };
  static const int tab16[]={ 1, 3,  0 };
  static const int tab64[]={ 1, 3, 9,  0 };
  static const int tab128[]={ 1, 3, 4, 9, 14,  0 };
  static const int tab256[]={ 1, 3, 6, 12, 13, 14, 19,  0 };
  static const int tab512[]={ 1, 4, 7, 9, 18, 50, 115,  0 };
  static const int tab1024[]={ 1, 3, 4, 25, 26, 77, 79,  0 };
  static const int tab2048[]={ 1, 58, 59, 106, 135, 206, 210, 212,  0 };
  static const int tab4096[]={ 1, 3, 12, 25, 54, 221, 313, 431, 453,  0 };
  static const int tab8192[]={ 1, 12, 18, 26, 30, 100, 101, 106, 113, 144, 150, 237, 244, 247, 386, 468, 513, 1210, 4839, 0 };
  static const int tab16384[]={ 1, 3, 6, 24, 1219,  0 };
  static const int tab32768[]={ 1, 3, 4, 7, 13, 18, 31, 64, 113, 145, 203, 246, 594, 956, 1871, 2439, 4959, 19175,  0 };
  const int *tab;

  switch (bitsz)
  {
    case 1: return; // no reorder necessary
    case 2:
    case 3:
    case 5: tab = tab4_8_32; break;
    case 4: tab=tab16; break;
    case 6: tab=tab64; break;
    case 7: tab=tab128; break;
    case 8: tab=tab256; break;
    case 9: tab=tab512; break;
    case 10: tab=tab1024; break;
    case 11: tab=tab2048; break;
    case 12: tab=tab4096; break;
    case 13: tab=tab8192; break;
    case 14: tab=tab16384; break;
    case 15: tab=tab32768; break;
    default: return; // no reorder possible
  }

  const int fft_sz=1<<bitsz;
  const int *tb2 = WDL_fft_permute_tab(fft_sz);
  if (!tb2) return; // ugh

  if (!fwd)
  {
    while (*tab)
    {
      const int sidx=*tab++;
      WDL_FFT_COMPLEX a=data[sidx];
      int idx=sidx;
      for (;;)
      {
        WDL_FFT_COMPLEX ta;
        idx=tb2[idx];
        if (idx==sidx) break;
        ta=data[idx];
        data[idx]=a;
        a=ta;
      }
      data[sidx] = a;
    }
  }
  else
  {
    while (*tab)
    {
      const int sidx=*tab++;
      int lidx = sidx;
      const WDL_FFT_COMPLEX sta=data[lidx];
      for (;;)
      {
        const int idx=tb2[lidx];
        if (idx==sidx) break;

        data[lidx]=data[idx];
        lidx=idx;
      }
      data[lidx] = sta;
    }
  }
}

#endif // not fast ,not slow, just right

#endif

//#define TIMING
//#include "../timing.h"

// 0=fw, 1=iv, 2=fwreal, 3=ireal, 4=permutec, 6=permuter
// low bit: is inverse
// second bit: was isreal, but no longer used
// third bit: is permute
static void FFT(int sizebits, EEL_F *data, int dir)
{
  if (dir >= 4 && dir < 8)
  {
    if (dir == 4 || dir == 5)
    {
      //timingEnter(0);
#if defined(EEL_SUPER_FAST_FFT_REORDERING) || !defined(EEL_SLOW_FFT_REORDERING)
      fft_reorder_buffer(sizebits,(WDL_FFT_COMPLEX*)data,dir==4);
#else
      // old blech
      const int flen=1<<sizebits;
      int x;
      EEL_F *tmp=(EEL_F*)alloca(sizeof(EEL_F)*flen*2);
    	const int flen2=flen+flen;
	    // reorder entries, now
      memcpy(tmp,data,sizeof(EEL_F)*flen*2);

      if (dir == 4)
      {
        for (x = 0; x < flen2; x += 2)
        {
          int y=WDL_fft_permute(flen,x/2)*2;
          data[x]=tmp[y];
          data[x+1]=tmp[y+1];
        }
      }
      else
      {
        for (x = 0; x < flen2; x += 2)
        {
          int y=WDL_fft_permute(flen,x/2)*2;
          data[y]=tmp[x];
          data[y+1]=tmp[x+1];
         }
       }
#endif
      //timingLeave(0);
    }
  }
  else if (dir >= 0 && dir < 2)
  {
    WDL_fft((WDL_FFT_COMPLEX*)data,1<<sizebits,dir&1);
  }
  else if (dir >= 2 && dir < 4)
  {
    WDL_real_fft((WDL_FFT_REAL*)data,1<<sizebits,dir&1);
  }
}



static EEL_F * fft_func(int dir, EEL_F **blocks, EEL_F *start, EEL_F *length)
{
	const int offs = (int)(*start + 0.0001);
  const int itemSizeShift=(dir&2)?0:1;
	int l=(int)(*length + 0.0001);
	int bitl=0;
	int ilen;
	EEL_F *ptr;
	while (l>1 && bitl < EEL_FFT_MAXBITLEN)
	{
		bitl++;
		l>>=1;
	}
	if (bitl < ((dir&4) ? EEL_FFT_MINBITLEN_REORDER : EEL_FFT_MINBITLEN))  // smallest FFT is 16 item, smallest reorder is 8 item
	{ 
		return start; 
	}
	ilen=1<<bitl;


	// check to make sure we don't cross a boundary
	if (offs/NSEEL_RAM_ITEMSPERBLOCK != (offs + (ilen<<itemSizeShift) - 1)/NSEEL_RAM_ITEMSPERBLOCK) 
	{ 
		return start; 
	}

	ptr=__NSEEL_RAMAlloc(blocks,offs);
	if (!ptr || ptr==&nseel_ramalloc_onfail)
	{ 
		return start; 
	}

	FFT(bitl,ptr,dir);

	return start;
}

static EEL_F * NSEEL_CGEN_CALL  eel_fft(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(0,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL  eel_ifft(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(1,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL  eel_fft_real(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(2,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL  eel_ifft_real(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(3,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL  eel_fft_permute(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(4,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL  eel_ifft_permute(EEL_F **blocks, EEL_F *start, EEL_F *length)
{
  return fft_func(5,blocks,start,length);
}

static EEL_F * NSEEL_CGEN_CALL eel_convolve_c(EEL_F **blocks,EEL_F *dest, EEL_F *src, EEL_F *lenptr)
{
	const int dest_offs = (int)(*dest + 0.0001);
	const int src_offs = (int)(*src + 0.0001);
  const int len = ((int)(*lenptr + 0.0001)) * 2;
  EEL_F *srcptr,*destptr;

  if (len < 1 || len > NSEEL_RAM_ITEMSPERBLOCK || dest_offs < 0 || src_offs < 0 || 
      dest_offs >= NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK || src_offs >= NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK) return dest;
  if ((dest_offs&(NSEEL_RAM_ITEMSPERBLOCK-1)) + len > NSEEL_RAM_ITEMSPERBLOCK) return dest;
  if ((src_offs&(NSEEL_RAM_ITEMSPERBLOCK-1)) + len > NSEEL_RAM_ITEMSPERBLOCK) return dest;

  srcptr = __NSEEL_RAMAlloc(blocks,src_offs);
  if (!srcptr || srcptr==&nseel_ramalloc_onfail) return dest;
  destptr = __NSEEL_RAMAlloc(blocks,dest_offs);
  if (!destptr || destptr==&nseel_ramalloc_onfail) return dest;


  WDL_fft_complexmul((WDL_FFT_COMPLEX*)destptr,(WDL_FFT_COMPLEX*)srcptr,(len/2)&~1);

  return dest;
}

void EEL_fft_register()
{
  WDL_fft_init();
#if defined(EEL_SUPER_FAST_FFT_REORDERING)
  if (!fft_reorder_table_for_bitsize(EEL_FFT_MINBITLEN_REORDER)[0])
  {
    int x;
    for (x=EEL_FFT_MINBITLEN_REORDER;x<=EEL_FFT_MAXBITLEN;x++) fft_make_reorder_table(x,fft_reorder_table_for_bitsize(x));
  }
#endif
  NSEEL_addfunc_retptr("convolve_c",3,NSEEL_PProc_RAM,&eel_convolve_c);
  NSEEL_addfunc_retptr("fft",2,NSEEL_PProc_RAM,&eel_fft);
  NSEEL_addfunc_retptr("ifft",2,NSEEL_PProc_RAM,&eel_ifft);
  NSEEL_addfunc_retptr("fft_real",2,NSEEL_PProc_RAM,&eel_fft_real);
  NSEEL_addfunc_retptr("ifft_real",2,NSEEL_PProc_RAM,&eel_ifft_real);
  NSEEL_addfunc_retptr("fft_permute",2,NSEEL_PProc_RAM,&eel_fft_permute);
  NSEEL_addfunc_retptr("fft_ipermute",2,NSEEL_PProc_RAM,&eel_ifft_permute);
}

#ifdef EEL_WANT_DOCUMENTATION
static const char *eel_fft_function_reference =
"convolve_c\tdest,src,size\tMultiplies each of size complex pairs in dest by the complex pairs in src. Often used for convolution.\0"
"fft\tbuffer,size\tPerforms a FFT on the data in the local memory buffer at the offset specified by the first parameter. The size of the FFT is specified "
                  "by the second parameter, which must be 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, or 32768. The outputs are permuted, so if "
                  "you plan to use them in-order, call fft_permute(buffer, size) before and fft_ipermute(buffer,size) after your in-order use. Your inputs or "
                  "outputs will need to be scaled down by 1/size, if used.\n"
                  "Note that fft()/ifft() require real / imaginary input pairs, so a 256 point FFT actually works with 512 items.\n"
                  "Note that fft()/ifft() must NOT cross a 65,536 item boundary, so be sure to specify the offset accordingly.\0"
"ifft\tbuffer,size\tPerform an inverse FFT. For more information see fft().\0"
"fft_real\tbuffer,size\tPerforms an FFT, but takes size input samples and produces size/2 complex output pairs. Usually used along with fft_permute(size/2). Inputs/outputs will need to be scaled by 0.5/size.\0"
"ifft_real\tbuffer,size\tPerforms an inverse FFT, but takes size/2 complex input pairs and produces size real output values. Usually used along with fft_ipermute(size/2).\0"
"fft_permute\tbuffer,size\tPermute the output of fft() to have bands in-order. See fft() for more information.\0"
"fft_ipermute\tbuffer,size\tPermute the input for ifft(), taking bands from in-order to the order ifft() requires. See fft() for more information.\0"
;
#endif


#endif
