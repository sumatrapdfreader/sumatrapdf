#ifndef _WDL_DIFFCALC_H_
#define _WDL_DIFFCALC_H_

#include "assocarray.h"



// Based on "An O(ND) Difference Algorithm and Its Variations", Myers
// http://xmailserver.org/diff2.pdf


template <class T> class WDL_DiffCalc
{
public:
  WDL_DiffCalc() {}
  virtual ~WDL_DiffCalc() {}

  // cmp() returns 0 if the elements are equal.
  // returns the length of the merged list and populates m_rx, m_ry.
  int Diff(const T* x, const T* y, int nx, int ny, int (*cmp)(const T*, const T*))
  {
    m_rx.Resize(0, false);
    m_ry.Resize(0, false);
    ClearV();

    if (!nx && !ny) return 0;
    if (!nx || !ny)
    {
      int i, n=max(nx, ny);
      for (i=0; i < n; ++i)
      {
        m_rx.Add(nx ? i : -1);
        m_ry.Add(ny ? i : -1);
      }
      return n;
    }

    if (!cmp(x, y)) // special case
    {
      int i, n;
      for (n=1; n < min(nx, ny); ++n)
      {
        if (cmp(x+n, y+n)) break;
      }
      int len=Diff(x+n, y+n, nx-n, ny-n, cmp);
      int *rx=m_rx.Get(), *ry=m_ry.Get();
      for (i=0; i < len; ++i)
      {
        if (rx[i] >= 0) rx[i] += n;
        if (ry[i] >= 0) ry[i] += n;
      }
      len += n;
      while (n--)
      {
        m_rx.Insert(n, 0);
        m_ry.Insert(n, 0);
      }
      return len;
    }

    SetV(0, 1, 0);
    int d, k, xi, yi;
    for (d=0; d <= nx+ny; ++d)
    {
      for (k=-d; k <= d ; k += 2)
      {
        if (k == -d || (k != d && GetV(d, k-1) < GetV(d, k+1)))
        {
          xi=GetV(d, k+1);
        }
        else
        {
          xi=GetV(d, k-1)+1;
        }
        yi=xi-k;
        while (xi < nx && yi < ny && !cmp(x+xi, y+yi))
        {
          ++xi;
          ++yi;
        }
        SetV(d+1, k, xi);
        if (xi >= nx && yi >= ny) break;
      }
      if (xi >= nx && yi >= ny) break;
    }

    int len=(nx+ny+d)/2;
    int *rx=m_rx.Resize(len);
    int *ry=m_ry.Resize(len);
    int pos=len;

    while (d)
    {
      while (xi > 0 && yi > 0 && !cmp(x+xi-1, y+yi-1))
      {
        --pos;
        rx[pos]=--xi;
        ry[pos]=--yi;
      }
      --pos;
      if (k == -d || (k != d && GetV(d, k-1) < GetV(d, k+1)))
      {
        ++k;
        rx[pos]=-1;
        ry[pos]=--yi;
      }
      else
      {
        --k;
        rx[pos]=--xi;
        ry[pos]=-1;
      }
      --d;
    }

    return len;
  }

  // m_rx, m_ry hold the index of each merged list element in x and y,
  // or -1 if the merged list element is not an element in that source list.
  // example: X="ABCABBA", Y="CBABAC"
  //             0123456      012345
  // WDL_Merge() returns "ABCBABBAC"
  //                      012 3456
  //                        0123 45
  // m_rx={ 0,  1,  2, -1,  3,  4,  5,  6, -1}
  // m_ry={-1, -1,  0,  1,  2,  3, -1,  4,  5}
   WDL_TypedBuf<int> m_rx, m_ry;

private:

  WDL_IntKeyedArray<int> m_v; // x coord of d-contour on row k
  void ClearV()
  {
    m_v.DeleteAll();
  }
  void SetV(int d, int k, int xi)
  {
    m_v.Insert(_key(d, k), xi);
  }
  int GetV(int d, int k)
  {
    return m_v.Get(_key(d, k));
  }
  static int _key(int d, int k) { return (d<<16)|(k+(1<<15)); }
};


// this is a separate function from WDL_DiffCalc only because it requires T::operator=
template <class T> int WDL_Merge(const T* x, const T* y, int nx, int ny,
  int (*cmp)(const T*, const T*), T* list)
{
  WDL_DiffCalc<T> dc;
  int i, n=dc.Diff(x, y, nx, ny, cmp);
  int *rx=dc.m_rx.Get(), *ry=dc.m_ry.Get();
  for (i=0; i < n; ++i)
  {
    if (list) list[i]=(rx[i] >= 0 ? x[rx[i]] : y[ry[i]]);
  }
  return n;
}

#endif