#ifndef _AUDIOBUFFERCONTAINER_
#define _AUDIOBUFFERCONTAINER_

#include "wdltypes.h"
#include <string.h>
#include <stdlib.h>
#include "ptrlist.h"
#include "queue.h"


#define CHANNELPINMAPPER_MAXPINS 64

class ChannelPinMapper
{
public: 

  ChannelPinMapper() : m_nCh(0), m_nPins(0) {}
  ~ChannelPinMapper() {}

  void SetNPins(int nPins);
  void SetNChannels(int nCh);
  // or ...
  void Init(const WDL_UINT64* pMapping, int nPins);

  int GetNPins() const { return m_nPins; }
  int GetNChannels() const { return m_nCh; }

  void ClearPin(int pinIdx);
  void SetPin(int pinIdx, int chIdx, bool on);
  bool TogglePin(int pinIdx, int chIdx);

  // true if this pin is mapped to this channel
  bool GetPin(int pinIdx, int chIdx) const;
  // true if this pin is to any higher channel
  bool PinHasMoreMappings(int pinIdx, int chIdx) const;
  // true if this mapper is a straight 1:1 passthrough
  bool IsStraightPassthrough() const;

  char* SaveStateNew(int* pLen); // owned
  bool LoadState(const char* buf, int len);

  WDL_UINT64 m_mapping[CHANNELPINMAPPER_MAXPINS];

private:

  WDL_Queue m_cfgret;
  int m_nCh, m_nPins;
};

// converts interleaved buffer to interleaved buffer, using min(len_in,len_out) and zeroing any extra samples
// isInput means it reads from track channels and writes to plugin pins
// wantZeroExcessOutput=false means that untouched channels will be preserved in buf_out
void PinMapperConvertBuffers(const double *buf, int len_in, int nch_in, 
                             double *buf_out, int len_out, int nch_out,
                             const ChannelPinMapper *pinmap, bool isInput, bool wantZeroExcessOutput);

// use for float and double only ... ints will break it
class AudioBufferContainer
{
public:

  AudioBufferContainer();
  ~AudioBufferContainer() {}

  enum 
  {
    FMT_32FP=4,
    FMT_64FP=8
  };

  static bool BufConvert(void* dest, const void* src, int destFmt, int srcFmt, int nFrames, int destStride, int srcStride);

  int GetNChannels() const { return m_nCh; }
  int GetNFrames() const { return m_nFrames; }
  int GetFormat() const { return m_fmt; }
    
  void Resize(int nCh, int nFrames, bool preserveData);  
  // call Reformat(GetFormat(), false) to discard current data (for efficient repopulating)
  void Reformat(int fmt, bool preserveData); 
    
  // src=NULL to memset(0)
  void* SetAllChannels(int fmt, const void* src, int nCh, int nFrames);
  
  // src=NULL to memset(0)
  void* SetChannel(int fmt, const void* src, int chIdx, int nFrames);
  
  void* MixChannel(int fmt, const void* src, int chIdx, int nFrames, bool addToDest, double wt_start, double wt_end);
  
  void* GetAllChannels(int fmt, bool preserveData);
  void* GetChannel(int fmt, int chIdx, bool preserveData);
  
  void CopyFrom(const AudioBufferContainer* rhs);
  
private:

  void ReLeave(bool interleave, bool preserveData);

  WDL_HeapBuf m_data;
  int m_nCh;
  int m_nFrames;

  int m_fmt;
  bool m_interleaved;
  bool m_hasData;
} WDL_FIXALIGN;


void SetPinsFromChannels(AudioBufferContainer* dest, AudioBufferContainer* src, const ChannelPinMapper* mapper, int forceMinChanCnt=0);
void SetChannelsFromPins(AudioBufferContainer* dest, AudioBufferContainer* src, const ChannelPinMapper* mapper, double wt_start=1.0, double wt_end=1.0);


#endif
