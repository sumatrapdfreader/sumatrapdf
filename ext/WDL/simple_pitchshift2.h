#ifndef _WDL_SIMPLEPITCHSHIFT_H_
#define _WDL_SIMPLEPITCHSHIFT_H_


#include "queue.h"

#ifndef WDL_SIMPLEPITCHSHIFT_SAMPLETYPE
#define WDL_SIMPLEPITCHSHIFT_SAMPLETYPE double
#endif


// this one isnt done yet, but stretches, then (optionally) resamples

#ifdef WDL_SIMPLEPITCHSHIFT_PARENTCLASS
class WDL_SimplePitchShifter2 : public WDL_SIMPLEPITCHSHIFT_PARENTCLASS
#else
class WDL_SimplePitchShifter2
#endif
{
public:
  WDL_SimplePitchShifter2()
  {
    m_last_nch=1;
    m_srate=44100.0;
    m_last_tempo=1.0;
    m_last_shift=1.0;
    m_qual=0;

    Reset();
  }
  ~WDL_SimplePitchShifter2() {   }

  void Reset()
  {
    m_hadinput=0;
    m_pspos=0.0;
    m_pswritepos=0;
    m_tempo_fracpos=0.0;
    m_queue.Clear();
    m_rsbuf.Resize(0,false);
    m_psbuf.Resize(0,false);
  }

  bool IsReset()
  {
    return !m_queue.Available() && !m_hadinput;
  }



  void set_srate(double srate) { m_srate=srate; }
  void set_nch(int nch) { if (m_last_nch!=nch) { m_queue.Clear(); m_last_nch=nch; m_tempo_fracpos=0.0; } }
  void set_shift(double shift) { m_last_shift=shift; }
  void set_tempo(double tempo) { m_last_tempo=tempo; }
  void set_formant_shift(double shift)
  {
  }

  WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *GetBuffer(int size)
  {
    return m_inbuf.Resize(size*m_last_nch);
  }
  void BufferDone(int input_filled);
  void FlushSamples() {}

  static char *enumQual(int q);
 static bool GetSizes(int qv, int *ws, int *os);

  int GetSamples(int requested_output, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *buffer);


 void SetQualityParameter(int parm)
 {
   m_qual=parm;
 }


  int Stretch(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, int maxoutlen, double stretch, double srate, int ws_ms, int os_ms);

private:
  int StretchBlock(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, int maxoutlen, double stretch, int bsize, int olsize, double srate);


private:
  double m_pspos WDL_FIXALIGN;
  double m_tempo_fracpos;
  double m_srate,m_last_tempo,m_last_shift;

  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_psbuf;
  WDL_Queue m_queue;
  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_inbuf;
  WDL_TypedBuf<WDL_SIMPLEPITCHSHIFT_SAMPLETYPE> m_rsbuf;

  int m_pswritepos;
  int m_last_nch;
  int m_qual;
  int m_hadinput;

};


#ifdef WDL_SIMPLEPITCHSHIFT_IMPLEMENT
void WDL_SimplePitchShifter2::BufferDone(int input_filled)
{
  if (input_filled>0)
  {
    m_hadinput=1;
    int ws,os;
    GetSizes(m_qual,&ws,&os);
    int max_outputlen=(int) (input_filled * m_last_shift / m_last_tempo * 1.1f + 32.0f);

    if (fabs(m_last_shift-1.0)<0.0000000001)
    {
      int valid_amt = Stretch(m_inbuf.Get(),(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *)m_queue.Add(NULL,max_outputlen*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)),m_last_nch,
        input_filled,max_outputlen,1.0/m_last_tempo,m_srate,ws,os);

      if (valid_amt < max_outputlen)
        m_queue.Add(NULL,(valid_amt-max_outputlen)*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));
    }
    else
    {
      int needclear=m_rsbuf.GetSize()<m_last_nch;

      WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *bufi=m_rsbuf.Resize((max_outputlen+1)*m_last_nch,false);
      if (needclear)
        memset(bufi,0,m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));

      int valid_amt = Stretch(m_inbuf.Get(),bufi+m_last_nch,m_last_nch,input_filled,max_outputlen,m_last_shift/m_last_tempo,m_srate,ws,os);

      double adv=m_last_shift;
      double fp=m_tempo_fracpos;

      int out_max = (int) (input_filled / m_last_tempo * 1.1f + 32.0f);

      WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *bufo = (WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *)m_queue.Add(NULL,out_max*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));
      // resample bufi to bufo
      int i,nch=m_last_nch;
      for (i = 0; i < out_max; i ++)
      {
        double rdpos=floor(fp);
        int idx=((int)rdpos);
        if (idx>=valid_amt) 
        {
          // un-add any missing samples
          m_queue.Add(NULL,(i-out_max)*m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE));
          break;
        }
        rdpos = (fp-rdpos);
        int a;
        idx*=nch;
        for (a = 0; a < nch; a ++)
        {
          *bufo++ = bufi[idx+a]*(1.0-rdpos)+bufi[idx+nch+a]*rdpos;
        }
        fp += adv;
      }
        
      memcpy(bufi,bufi+m_last_nch*valid_amt,m_last_nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)); // save last sample for interpolation later
      //
      m_tempo_fracpos=fp-floor(fp);
    }
  }    
}

char *WDL_SimplePitchShifter2::enumQual(int q)
{
  int ws,os;
  if (!GetSizes(q,&ws,&os)) return NULL;
  static char buf[128];
  sprintf(buf,"%dms window, %dms fade",ws,os);
  return buf;
}

bool WDL_SimplePitchShifter2::GetSizes(int qv, int *ws, int *os)
{
  int windows[]={50,75,100,150,225,300,40,30,20,10,5,3};
  int divs[]={2,3,5,7};

  int wd=qv/(sizeof(divs)/sizeof(divs[0]));
  if (wd >= sizeof(windows)/sizeof(windows[0])) wd=-1;

  *ws=windows[wd>=0?wd:0];
  *os = *ws / divs[qv%(sizeof(divs)/sizeof(divs[0]))];
  if (*os<1) *os=1;

  return wd>=0;
}

int WDL_SimplePitchShifter2::GetSamples(int requested_output, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *buffer)
{
  if (!m_last_nch||requested_output<1) return 0;

  int l=m_queue.Available()/sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)/m_last_nch;
  if (requested_output>l) requested_output=l;
  int sz=requested_output*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)*m_last_nch;
  memcpy(buffer,m_queue.Get(),sz);
  m_queue.Advance(sz);
  m_queue.Compact();
  return requested_output;
}

int WDL_SimplePitchShifter2::Stretch(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, int maxoutlen, double stretch, double srate, int ws_ms, int os_ms)
{
  int bsize=(int) (ws_ms * 0.001 * srate);
  if (bsize<16) bsize=16;
  else if (bsize>128*1024)bsize=128*1024;

  int olsize=(int) (os_ms * 0.001 * srate);
  if (olsize > bsize/2) olsize=bsize/2;
  if (olsize<1)olsize=1;
  if (m_psbuf.GetSize() != bsize*nch)
  {
    memset(m_psbuf.Resize(bsize*nch,false),0,sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE)*bsize*nch);
    m_pspos=(double) (bsize/2);
    m_pswritepos=0;
  }

  return StretchBlock(inputs,outputs,nch,length,maxoutlen,stretch,bsize,olsize,srate);
}

int WDL_SimplePitchShifter2::StretchBlock(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *inputs, WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *outputs, int nch, int length, int maxoutlen, double stretch, int bsize, int olsize, double srate)
{
  double iolsize=1.0/olsize;

  WDL_SIMPLEPITCHSHIFT_SAMPLETYPE *psbuf=m_psbuf.Get();

  double pspos=m_pspos;
  int writepos=m_pswritepos;
  int writeposnch = writepos*nch;
  int bsizench = bsize*nch;
  int olsizench = olsize*nch;
  int output_used=0;
  int i=length;
  int chunksize = nch*sizeof(WDL_SIMPLEPITCHSHIFT_SAMPLETYPE);
  while (i--)
  {
    int cnt = (int) (pspos + stretch) - (int) pspos;
    double pspos_fracadd=stretch - cnt;
    while (cnt-- > 0)
    {
      if (output_used>=maxoutlen) return maxoutlen;
      int ipos1=(int)pspos;

      ipos1*=nch;

      memcpy(outputs,psbuf+ipos1,chunksize);

      double tv=pspos;
      if (stretch >= 1.0)
      {
        if (tv > writepos) tv-=bsize;

        if (tv >= writepos-olsize && tv < writepos)
        {
          double tfrac=(writepos-tv)*iolsize;
          int tmp=ipos1+olsizench;
          if (tmp>=bsizench) tmp-=bsizench;

          int a;
          for(a=0;a<nch;a++) outputs[a]= outputs[a]*tfrac + (1-tfrac)*psbuf[tmp+a];

          if (tv+stretch >= writepos) 
          {
            pspos+=olsize;
          }
        }

      }
      else
      {
        if (tv<writepos) tv+=bsize;

        if (tv >= writepos && tv < writepos+olsize)
        {
          double tfrac=(tv-writepos)*iolsize;
          int tmp=ipos1+olsizench;
          if (tmp>=bsizench) tmp -= bsizench;
          int a;
          for(a=0;a<nch;a++) outputs[a] = outputs[a]*tfrac + (1-tfrac)*psbuf[tmp+a];
        
          // this is wrong, but blehhh?
          if (tv+stretch < writepos+1) 
          {
  //          pspos += olsize;
          }
          if (tv+stretch >= writepos+olsize) pspos += olsize;
        }
      }


      if ((pspos+=1) >= bsize)  pspos -= bsize;
      outputs += nch;
      output_used++;
    }
    pspos += pspos_fracadd;
    if (pspos>=bsize) pspos-=bsize;



    memcpy(psbuf+writeposnch,inputs,chunksize);

    writeposnch += nch;
    if (++writepos >= bsize) writeposnch = writepos=0;

    inputs += nch;
  } // sample loop
  m_pspos=pspos;
  m_pswritepos=writepos;
  return output_used;

}

#endif

#endif