#ifndef _WDL_MERGESORT_H_
#define _WDL_MERGESORT_H_

#include "wdltypes.h"

static void WDL_STATICFUNC_UNUSED WDL_mergesort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *), char *tmpspace)
{
  char *b1,*b2;
  size_t n1, n2;

  if (nmemb < 2) return;

  n1 = nmemb / 2;
  b1 = (char *) base;
  n2 = nmemb - n1;
  b2 = b1 + (n1 * size);

  if (nmemb>2)
  {
    WDL_mergesort(b1, n1, size, compar, tmpspace);
    WDL_mergesort(b2, n2, size, compar, tmpspace);
  }


  do
  {
    if (compar(b1, b2) > 0) // out of order, go to full merge
    {
      size_t sofar = b1-(char*)base;
      memcpy(tmpspace,base,sofar);
      memcpy(tmpspace+sofar, b2, size);
      b2 += size;
      n2--;

      char *writeptr=tmpspace+sofar+size;
      while (n1 > 0 && n2 > 0)
      {
        if (compar(b1, b2) > 0)
        {
          memcpy(writeptr, b2, size);
          b2 += size;
          n2--;
        }
        else
        {
          memcpy(writeptr, b1, size);
          b1 += size;
          n1--;
        }
        writeptr += size;
      }

      if (n1 > 0) memcpy(writeptr, b1, n1 * size);
      memcpy(base, tmpspace, (nmemb - n2) * size);

      break;
    }

    // in order, just advance
    b1 += size;
    n1--;
  }
  while (n1 > 0 && n2 > 0);
}

#endif//_WDL_MERGESORT_H_
