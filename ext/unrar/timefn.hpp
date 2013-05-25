#ifndef _RAR_TIMEFN_
#define _RAR_TIMEFN_

struct RarLocalTime
{
  uint Year;
  uint Month;
  uint Day;
  uint Hour;
  uint Minute;
  uint Second;
  uint Reminder; // Part of time smaller than 1 second, represented in 100-nanosecond intervals.
  uint wDay;
  uint yDay;
};


class RarTime
{
  private:
    // Internal FILETIME like time representation in 100 nanoseconds
    // since 01.01.1601.
    uint64 itime;
  public:
    RarTime();
#ifdef _WIN_ALL
    RarTime& operator =(FILETIME &ft);
    void GetWin32(FILETIME *ft);
#endif
    RarTime& operator =(time_t ut);
    time_t GetUnix();
    bool operator == (RarTime &rt) {return itime==rt.itime;}
    bool operator < (RarTime &rt)  {return itime<rt.itime;}
    bool operator <= (RarTime &rt) {return itime<rt.itime || itime==rt.itime;}
    bool operator > (RarTime &rt)  {return itime>rt.itime;}
    bool operator >= (RarTime &rt) {return itime>rt.itime || itime==rt.itime;}
    void GetLocal(RarLocalTime *lt);
    void SetLocal(RarLocalTime *lt);
    uint64 GetRaw();
    void SetRaw(uint64 RawTime);
    uint GetDos();
    void SetDos(uint DosTime);
    void GetText(wchar *DateStr,size_t MaxSize,bool FullYear,bool FullMS);
    void SetIsoText(const wchar *TimeText);
    void SetAgeText(const wchar *TimeText);
    void SetCurrentTime();
    void Reset() {itime=0;}
    bool IsSet() {return(itime!=0);}
};

const wchar *GetMonthName(int Month);
bool IsLeapYear(int Year);

#endif
