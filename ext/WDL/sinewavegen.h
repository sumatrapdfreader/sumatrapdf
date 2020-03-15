#ifndef _WDL_SINEWAVEGEN_H_
#define _WDL_SINEWAVEGEN_H_


// note: calling new WDL_SineWaveGenerator isnt strictly necessary, you can also do WDL_SineWaveGenerator *gens = (WDL_SineWaveGenerator *)malloc(512*sizeof(WDL_SineWaveGenerator));
// as long as you call Reset() and SetFreq() it should be fine.


// note: won't really work for high frequencies...

class WDL_SineWaveGenerator 
{
  double m_lastfreq;
  double m_mul1,m_mul2;
  double m_pos,m_vel;

public:
  WDL_SineWaveGenerator() { Reset(); m_mul1=m_mul2=m_pos=m_vel=0.0; } 
  ~WDL_SineWaveGenerator() { }

  void Reset() { m_lastfreq=0.0; } // must call this before anything

  void SetFreq(double freq) // be sure to call this before calling Gen(), or on freq change, or after a Reset()
                            // freq is frequency/(samplerate*0.5) (so 0..1 is valid, though over 0.3 is probably not a good idea)
  {    
    freq*=3.1415926535897932384626433832795; // scale to freq*PI

    if (m_lastfreq<=0.0)
    {
      m_pos=0.0;
      m_vel = 1.0/freq;
    }
    else
    {
      if (freq==m_lastfreq) return;
      m_vel *= m_lastfreq/freq;
    }
    m_lastfreq=freq;

    double tmp2 = 1.0/(1.0+(freq*=freq));
    m_mul1 = (1.0-freq)*tmp2;
    m_mul2 = freq*2.0*tmp2;
  }

  double Gen() // returns sine
  {
    double rv=m_pos;
    m_vel -= rv + (m_pos = rv*m_mul1 + m_vel*m_mul2);
    return rv;
  }

  double GetNextCos()  // call BEFORE Gen() if you want the cosine of the next value
  {
    return m_vel * m_lastfreq;
  }

};

#endif
