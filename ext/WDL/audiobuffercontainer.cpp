#include "audiobuffercontainer.h"
#include "queue.h"
#include <assert.h>

void ChannelPinMapper::SetNPins(int nPins)
{
  if (nPins<0) nPins=0;
  else if (nPins>CHANNELPINMAPPER_MAXPINS) nPins=CHANNELPINMAPPER_MAXPINS;
  int i;
  for (i = m_nPins; i < nPins; ++i) {
    ClearPin(i);
    if (i < m_nCh) {
      SetPin(i, i, true);
    }
  }
  m_nPins = nPins;
}

void ChannelPinMapper::SetNChannels(int nCh)
{
  int i;
  for (i = m_nCh; i < nCh && i < m_nPins; ++i) {
    SetPin(i, i, true);
  }
  m_nCh = nCh;
}

void ChannelPinMapper::Init(const WDL_UINT64* pMapping, int nPins)
{
  if (nPins<0) nPins=0;
  else if (nPins>CHANNELPINMAPPER_MAXPINS) nPins=CHANNELPINMAPPER_MAXPINS;
  memcpy(m_mapping, pMapping, nPins*sizeof(WDL_UINT64));
  memset(m_mapping+nPins, 0, (CHANNELPINMAPPER_MAXPINS-nPins)*sizeof(WDL_UINT64));
  m_nPins = m_nCh = nPins;
}

#define BITMASK64(bitIdx) (((WDL_UINT64)1)<<(bitIdx))
 
void ChannelPinMapper::ClearPin(int pinIdx)
{
  if (pinIdx >=0 && pinIdx < CHANNELPINMAPPER_MAXPINS) m_mapping[pinIdx] = 0;
}

void ChannelPinMapper::SetPin(int pinIdx, int chIdx, bool on)
{
  if (pinIdx >=0 && pinIdx < CHANNELPINMAPPER_MAXPINS)
  {
    if (on) 
    {
      m_mapping[pinIdx] |= BITMASK64(chIdx);
    }
    else 
    {
      m_mapping[pinIdx] &= ~BITMASK64(chIdx);
    }
  }
}

bool ChannelPinMapper::TogglePin(int pinIdx, int chIdx) 
{
  bool on = GetPin(pinIdx, chIdx);
  on = !on;
  SetPin(pinIdx, chIdx, on); 
  return on;
}

bool ChannelPinMapper::GetPin(int pinIdx, int chIdx) const
{
  if (pinIdx >= 0 && pinIdx < CHANNELPINMAPPER_MAXPINS)
  {
    WDL_UINT64 map = m_mapping[pinIdx];
    return !!(map & BITMASK64(chIdx));
  }
  return false;
}

bool ChannelPinMapper::PinHasMoreMappings(int pinIdx, int chIdx) const
{
  if (pinIdx >= 0 && pinIdx < CHANNELPINMAPPER_MAXPINS)
  {
    WDL_UINT64 map = m_mapping[pinIdx];
    return (chIdx < 63 && map >= BITMASK64(chIdx+1));
  }
  return false;
}

bool ChannelPinMapper::IsStraightPassthrough() const
{
  if (m_nCh != m_nPins) return false;
  const WDL_UINT64* pMap = m_mapping;
  int i;
  for (i = 0; i < m_nPins; ++i, ++pMap) {
    if (*pMap != BITMASK64(i)) return false;
  }
  return true;
}

#define PINMAPPER_MAGIC 1000

// return is on the heap
char* ChannelPinMapper::SaveStateNew(int* pLen)
{
  m_cfgret.Clear();
  int magic = PINMAPPER_MAGIC;
  WDL_Queue__AddToLE(&m_cfgret, &magic);
  WDL_Queue__AddToLE(&m_cfgret, &m_nCh);
  WDL_Queue__AddToLE(&m_cfgret, &m_nPins);
  WDL_Queue__AddDataToLE(&m_cfgret, m_mapping, m_nPins*sizeof(WDL_UINT64), sizeof(WDL_UINT64));
  *pLen = m_cfgret.GetSize();
  return (char*)m_cfgret.Get();
}

bool ChannelPinMapper::LoadState(const char* buf, int len)
{
  WDL_Queue chunk;
  chunk.Add(buf, len);
  int* pMagic = WDL_Queue__GetTFromLE(&chunk, (int*)0);
  if (!pMagic || *pMagic != PINMAPPER_MAGIC) return false;
  int* pNCh = WDL_Queue__GetTFromLE(&chunk, (int*) 0);
  int* pNPins = WDL_Queue__GetTFromLE(&chunk, (int*) 0);
  if (!pNCh || !pNPins) return false;
  SetNPins(*pNCh);
  SetNChannels(*pNCh);
  int maplen = *pNPins*sizeof(WDL_UINT64);
  if (chunk.Available() < maplen) return false;
  void* pMap = WDL_Queue__GetDataFromLE(&chunk, maplen, sizeof(WDL_UINT64));
  
  int sz= m_nPins*sizeof(WDL_UINT64);
  if (sz>maplen) sz=maplen;
  memcpy(m_mapping, pMap, sz);

  return true;
}

template <class TDEST, class TSRC> void BufConvertT(TDEST* dest, const TSRC* src, int nFrames, int destStride, int srcStride)
{
  int i;
  for (i = 0; i < nFrames; ++i)
  {
    dest[i*destStride] = (TDEST)src[i*srcStride];
  }
}

template <class T> void BufMixT(T* dest, const T* src, int nFrames, bool addToDest, double wt_start, double wt_end)
{
  int i;
  
  if (wt_start == 1.0 && wt_end == 1.0)
  {
    if (addToDest)
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] += src[i];    
      }
    }
    else
    {
      memcpy(dest, src, nFrames*sizeof(T));
    }  
  }
  else
  {
    double dw = (wt_end-wt_start)/(double)nFrames;
    double cw = wt_start;
   
    if (addToDest)
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] += (T)(1.0-cw)*dest[i]+(T)cw*src[i];
        cw += dw;  
      }
    }
    else
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] = (T)(1.0-cw)*dest[i]+(T)cw*src[i];
        cw += dw;  
      }
    }
  }
}

// static 
bool AudioBufferContainer::BufConvert(void* dest, const void* src, int destFmt, int srcFmt, int nFrames, int destStride, int srcStride)
{
  if (destFmt == FMT_32FP)
  {
    if (srcFmt == FMT_32FP)
    {
      BufConvertT((float*)dest, (float*)src, nFrames, destStride, srcStride);
      return true;
    }
    else if (srcFmt == FMT_64FP)
    {
      BufConvertT((float*)dest, (double*)src, nFrames, destStride, srcStride);
      return true;
    }
  }
  else if (destFmt == FMT_64FP)
  {
    if (srcFmt == FMT_32FP)
    {
      BufConvertT((double*)dest, (float*)src, nFrames, destStride, srcStride);
      return true;
    }
    else if (srcFmt == FMT_64FP)
    {
      BufConvertT((double*)dest, (double*)src, nFrames, destStride, srcStride);
      return true;
    }
  }
  return false;
}

AudioBufferContainer::AudioBufferContainer()
{
  m_nCh = 0;
  m_nFrames = 0;
  m_fmt = FMT_32FP;
  m_interleaved = true;
  m_hasData = false;
}

void AudioBufferContainer::Resize(int nCh, int nFrames, bool preserveData)
{
  if (!m_hasData) 
  {
    preserveData = false;
  }

  const int newsz = nCh*nFrames*(int)m_fmt;

  if (preserveData && (nCh != m_nCh || nFrames != m_nFrames))
  {
    GetAllChannels(m_fmt, true);  // causes m_data to be interleaved

    if (newsz > m_data.GetSize()) m_data.Resize(newsz);
    if (nCh != m_nCh && m_data.GetSize() >= newsz)
    {
      char *out = (char *)m_data.Get();
      const char *in = out;
      const int in_adv = m_nCh * m_fmt, out_adv = nCh * m_fmt;
      const int copysz = wdl_min(in_adv,out_adv);

      int n = wdl_min(nFrames,m_nFrames);
      if (out_adv < in_adv) // decreasing channel count, left to right
      {
        while (n--)
        {
          if (out!=in) memmove(out,in,copysz);
          out+=out_adv;
          in+=in_adv;
        }
      }
      else // increasing channel count, copy right to left
      {
        out += n * out_adv;
        in += n * in_adv;
        while (n--)
        {
          out-=out_adv;
          in-=in_adv;
          if (out!=in) memmove(out,in,copysz);
        }
      }
      // adjust interleaving
    }
  }
  
  m_data.Resize(newsz);
  m_hasData = preserveData;
  m_nCh = nCh;
  m_nFrames = nFrames;
}

void AudioBufferContainer::Reformat(int fmt, bool preserveData)
{
  if (!m_hasData) 
  {
    preserveData = false;   
  }
  
  int newsz = m_nCh*m_nFrames*(int)fmt; 
  
  if (preserveData && fmt != m_fmt)
  {
    int oldsz = m_data.GetSize();
    void* src = m_data.Resize(oldsz+newsz);
    void* dest = (unsigned char*)src+oldsz;
    BufConvert(dest, src, fmt, m_fmt, m_nCh*m_nFrames, 1, 1);
    memmove(src, dest, newsz); 
  }
  
  m_data.Resize(newsz);    
  m_hasData = preserveData;
  m_fmt = fmt;
}

// src=NULL to memset(0)
void* AudioBufferContainer::SetAllChannels(int fmt, const void* src, int nCh, int nFrames)
{
  Reformat(fmt, false);
  Resize(nCh, nFrames, false);
  
  int sz = nCh*nFrames*(int)fmt;
  void* dest = GetAllChannels(fmt, false);
  if (src)
  {
    memcpy(dest, src, sz);
  }
  else
  {
    memset(dest, 0, sz);
  }
  
  m_interleaved = true;
  m_hasData = true;  
  return dest;
}

// src=NULL to memset(0)
void* AudioBufferContainer::SetChannel(int fmt, const void* src, int chIdx, int nFrames)
{
  Reformat(fmt, true);
  if (nFrames > m_nFrames || chIdx >= m_nCh) 
  {
    int maxframes = (nFrames > m_nFrames ? nFrames : m_nFrames);
    Resize(chIdx+1, maxframes, true);        
  }
  
  int sz = nFrames*(int)fmt;
  void* dest = GetChannel(fmt, chIdx, true);
  if (src)
  {
    memcpy(dest, src, sz);
  }
  else
  {
    memset(dest, 0, sz);
  }
   
  m_interleaved = false;
  m_hasData = true;
  return dest;
}

void* AudioBufferContainer::MixChannel(int fmt, const void* src, int chIdx, int nFrames, bool addToDest, double wt_start, double wt_end)
{
  Reformat(fmt, true);
  if (nFrames > m_nFrames || chIdx >= m_nCh) 
  {
    int maxframes = (nFrames > m_nFrames ? nFrames : m_nFrames);
    Resize(chIdx+1, maxframes, true);        
  }
  
  void* dest = GetChannel(fmt, chIdx, true);
  
  if (fmt == FMT_32FP)
  {
    BufMixT((float*)dest, (float*)src, nFrames, addToDest, wt_start, wt_end);
  }
  else if (fmt == FMT_64FP)
  {
    BufMixT((double*)dest, (double*)src, nFrames, addToDest, wt_start, wt_end);
  }
  
  m_interleaved = false;
  m_hasData = true;
  return dest;
}


void* AudioBufferContainer::GetAllChannels(int fmt, bool preserveData)
{
  Reformat(fmt, preserveData);
  ReLeave(true, preserveData);
  
  m_hasData = true;   // because caller may use the returned pointer to populate the container
  
  return m_data.Get();
}

void* AudioBufferContainer::GetChannel(int fmt, int chIdx, bool preserveData)
{
  Reformat(fmt, preserveData); 
  if (chIdx >= m_nCh)
  {
    Resize(chIdx+1, m_nFrames, true);
  }
  ReLeave(false, preserveData);
  
  m_hasData = true;   // because caller may use the returned pointer to populate the container
  
  int offsz = chIdx*m_nFrames*(int)fmt;
  return (unsigned char*)m_data.Get()+offsz;
}

void AudioBufferContainer::ReLeave(bool interleave, bool preserveData)
{
  if (interleave != m_interleaved && preserveData && m_hasData)
  {
    int elemsz = (int)m_fmt;
    int chansz = m_nFrames*elemsz;
    int bufsz = m_nCh*chansz;
    int i;

    unsigned char* src = (unsigned char*)m_data.Resize(bufsz*2);
    unsigned char* dest = src+bufsz;    
    
    if (interleave)
    { 
      for (i = 0; i < m_nCh; ++i)
      {
        BufConvert((void*)(dest+i*elemsz), (void*)(src+i*chansz), m_fmt, m_fmt, m_nFrames, m_nCh, 1);
      }
    }
    else
    {
      for (i = 0; i < m_nCh; ++i)
      {
        BufConvert((void*)(dest+i*chansz), (void*)(src+i*elemsz), m_fmt, m_fmt, m_nFrames, 1, m_nCh);
      }
    }
    
    memcpy(src, dest, bufsz); // no overlap
    m_data.Resize(bufsz);
  }
  
  m_hasData = preserveData;
  m_interleaved = interleave;
}

void AudioBufferContainer::CopyFrom(const AudioBufferContainer* rhs)
{
  int sz = rhs->m_data.GetSize();
  void* dest = m_data.Resize(sz);    
  
  if (rhs->m_hasData)
  {
    void* src = rhs->m_data.Get();
    memcpy(dest, src, sz);
  }

  m_nCh = rhs->m_nCh;
  m_nFrames = rhs->m_nFrames;
  m_fmt = rhs->m_fmt;
  m_interleaved = rhs->m_interleaved;
  m_hasData = rhs->m_hasData;
}


void SetPinsFromChannels(AudioBufferContainer* dest, AudioBufferContainer* src, ChannelPinMapper* mapper, int forceMinChanCnt)
{
  if (mapper->IsStraightPassthrough())
  {
    dest->CopyFrom(src);
    return;
  }

  const int nch = mapper->GetNChannels();
  const int npins = mapper->GetNPins();
  const int nframes = src->GetNFrames();
  const int fmt = src->GetFormat();
  const int np = wdl_max(npins,forceMinChanCnt);
  
  dest->Resize(np, nframes, false);
  
  int c, p;
  for (p = 0; p < np; ++p)
  {
    bool pinused = false;  
    if (p < npins) for (c = 0; c < nch; ++c)
    {
      if (mapper->GetPin(p, c))
      {
        void* srcbuf = src->GetChannel(fmt, c, true);
        dest->MixChannel(fmt, srcbuf, p, nframes, pinused, 1.0, 1.0);
        pinused = true;
        
        if (!mapper->PinHasMoreMappings(p, c))
        {
          break;
        }
      }
    }
    
    if (!pinused)
    {
      dest->SetChannel(fmt, 0, p, nframes);   // clear unused pins
    }
  }
}

void SetChannelsFromPins(AudioBufferContainer* dest, AudioBufferContainer* src, const ChannelPinMapper* mapper, double wt_start, double wt_end)
{
  if (wt_start == 1.0 && wt_end == 1.0 && mapper->IsStraightPassthrough())
  {
    dest->CopyFrom(src);
    return;
  }   

  int nch = mapper->GetNChannels();
  int npins = mapper->GetNPins();
  int nframes = src->GetNFrames();
  int fmt = src->GetFormat();

  dest->Resize(nch, nframes, true);
  
  int c, p;
  for (c = 0; c < nch; ++c) 
  {
    bool chanused = false;
    for (p = 0; p < npins; ++p) 
    {
      if (mapper->GetPin(p, c)) 
      {
        void* srcbuf = src->GetChannel(fmt, p, true);
        dest->MixChannel(fmt, srcbuf, c, nframes, chanused, wt_start, wt_end);        
        chanused = true;
      }
    }
    // don't clear unused channels
  }
}





// converts interleaved buffer to interleaved buffer, using min(len_in,len_out) and zeroing any extra samples
// isInput means it reads from track channels and writes to plugin pins
// wantZeroExcessOutput=false means that untouched channels will be preserved in buf_out
void PinMapperConvertBuffers(const double *buf, int len_in, int nch_in, 
                             double *buf_out, int len_out, int nch_out,
                             const ChannelPinMapper *pinmap, bool isInput, bool wantZeroExcessOutput) 
{

  if (pinmap->IsStraightPassthrough() || !pinmap->GetNPins())
  {
    int x;
    char *op = (char *)buf_out;
    const char *ip = (const char *)buf;

    const int ip_adv = nch_in * sizeof(double);

    const int clen = wdl_min(nch_in, nch_out) * sizeof(double);
    const int zlen = nch_out > nch_in ? (nch_out - nch_in) * sizeof(double) : 0;

    const int cplen = wdl_min(len_in,len_out);

    for (x=0;x<cplen;x++)
    {
      memcpy(op,ip,clen);
      op += clen;
      if (zlen) 
      {
        if (wantZeroExcessOutput) memset(op,0,zlen);
        op += zlen;
      }
      ip += ip_adv;
    }
    if (x < len_out && wantZeroExcessOutput) memset(op, 0, (len_out-x)*sizeof(double)*nch_out);
  }
  else
  {
    if (wantZeroExcessOutput) memset(buf_out,0,len_out*nch_out*sizeof(double));

    const int npins = wdl_min(pinmap->GetNPins(),isInput ? nch_out : nch_in);
    const int nchan = isInput ? nch_in : nch_out;

    int p;
    WDL_UINT64 clearmask=0;
    for (p = 0; p < npins; p ++)
    {
      WDL_UINT64 map = pinmap->m_mapping[p];
      int x;
      for (x = 0; x < nchan && map; x ++)
      {
        if (map & 1)
        {
          int i=len_in;
          const double *ip = buf + (isInput ? x : p);
          const int out_idx = (isInput ? p : x);

          bool want_zero=false;
          if (!wantZeroExcessOutput)
          {
            WDL_UINT64 m = ((WDL_UINT64)1)<<out_idx;
            if (!(clearmask & m))
            {
              clearmask|=m;
              want_zero=true;
            }
          }

          double *op = buf_out + out_idx;

          if (want_zero)
          {
            while (i-- > 0) 
            {
              *op = *ip;
              op += nch_out;
              ip += nch_in;
            }
          }
          else
          {
            while (i-- > 0) 
            {
              *op += *ip;
              op += nch_out;
              ip += nch_in;
            }
          }
        }
        map >>= 1;
      }
    }
  }
}
