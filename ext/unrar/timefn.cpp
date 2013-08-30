#include "rar.hpp"

RarTime::RarTime()
{
  Reset();
}

#ifdef _WIN_ALL
RarTime& RarTime::operator =(FILETIME &ft)
{
  _ULARGE_INTEGER ul = {ft.dwLowDateTime, ft.dwHighDateTime};
  itime=ul.QuadPart;
  return *this;
}


void RarTime::GetWin32(FILETIME *ft)
{
  _ULARGE_INTEGER ul;
  ul.QuadPart=itime;
  ft->dwLowDateTime=ul.LowPart;
  ft->dwHighDateTime=ul.HighPart;
}
#endif


RarTime& RarTime::operator =(time_t ut)
{
  uint64 ushift=INT32TO64(0x19DB1DE,0xD53E8000); // 116444736000000000.
  itime=uint64(ut)*10000000+ushift;
  return *this;
}


time_t RarTime::GetUnix()
{
  uint64 ushift=INT32TO64(0x19DB1DE,0xD53E8000); // 116444736000000000.
  time_t ut=(itime-ushift)/10000000;
  return ut;
}


void RarTime::GetLocal(RarLocalTime *lt)
{
#ifdef _WIN_ALL
  FILETIME ft;
  GetWin32(&ft);
  FILETIME lft;
  FileTimeToLocalFileTime(&ft,&lft);
  SYSTEMTIME st;
  FileTimeToSystemTime(&lft,&st);
  lt->Year=st.wYear;
  lt->Month=st.wMonth;
  lt->Day=st.wDay;
  lt->Hour=st.wHour;
  lt->Minute=st.wMinute;
  lt->Second=st.wSecond;
  lt->wDay=st.wDayOfWeek;
  lt->yDay=lt->Day-1;

  static int mdays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  for (uint I=1;I<lt->Month && I<=ASIZE(mdays);I++)
    lt->yDay+=mdays[I-1];

  if (lt->Month>2 && IsLeapYear(lt->Year))
    lt->yDay++;

  st.wMilliseconds=0;
  FILETIME zft;
  SystemTimeToFileTime(&st,&zft);

  // Calculate the time reminder, which is the part of time smaller
  // than 1 second, represented in 100-nanosecond intervals.
  lt->Reminder=INT32TO64(lft.dwHighDateTime,lft.dwLowDateTime)-
               INT32TO64(zft.dwHighDateTime,zft.dwLowDateTime);
#else
  time_t ut=GetUnix();
  struct tm *t;
  t=localtime(&ut);

  lt->Year=t->tm_year+1900;
  lt->Month=t->tm_mon+1;
  lt->Day=t->tm_mday;
  lt->Hour=t->tm_hour;
  lt->Minute=t->tm_min;
  lt->Second=t->tm_sec;
  lt->Reminder=0;
  lt->wDay=t->tm_wday;
  lt->yDay=t->tm_yday;
#endif
}


void RarTime::SetLocal(RarLocalTime *lt)
{
#ifdef _WIN_ALL
  SYSTEMTIME st;
  st.wYear=lt->Year;
  st.wMonth=lt->Month;
  st.wDay=lt->Day;
  st.wHour=lt->Hour;
  st.wMinute=lt->Minute;
  st.wSecond=lt->Second;
  st.wMilliseconds=0;
  FILETIME lft;
  if (SystemTimeToFileTime(&st,&lft))
  {
    lft.dwLowDateTime+=lt->Reminder;
    if (lft.dwLowDateTime<lt->Reminder)
      lft.dwHighDateTime++;
    FILETIME ft;
    LocalFileTimeToFileTime(&lft,&ft);
    *this=ft;
  }
  else
    Reset();
#else
  struct tm t;

  t.tm_sec=lt->Second;
  t.tm_min=lt->Minute;
  t.tm_hour=lt->Hour;
  t.tm_mday=lt->Day;
  t.tm_mon=lt->Month-1;
  t.tm_year=lt->Year-1900;
  t.tm_isdst=-1;
  *this=mktime(&t);
#endif
}


// Return the stored time as 64-bit number of 100-nanosecond intervals since 
// 01.01.1601. Actually we do not care since which date this time starts from
// as long as this date is the same for GetRaw and SetRaw. We use the value
// returned by GetRaw() for time comparisons, for relative operations
// like SetRaw(GetRaw()-C) and for compact time storage when necessary.
uint64 RarTime::GetRaw()
{
  return itime;
}


void RarTime::SetRaw(uint64 RawTime)
{
  itime=RawTime;
}


uint RarTime::GetDos()
{
  RarLocalTime lt;
  GetLocal(&lt);
  uint DosTime=(lt.Second/2)|(lt.Minute<<5)|(lt.Hour<<11)|
               (lt.Day<<16)|(lt.Month<<21)|((lt.Year-1980)<<25);
  return DosTime;
}


void RarTime::SetDos(uint DosTime)
{
  RarLocalTime lt;
  lt.Second=(DosTime & 0x1f)*2;
  lt.Minute=(DosTime>>5) & 0x3f;
  lt.Hour=(DosTime>>11) & 0x1f;
  lt.Day=(DosTime>>16) & 0x1f;
  lt.Month=(DosTime>>21) & 0x0f;
  lt.Year=(DosTime>>25)+1980;
  lt.Reminder=0;
  SetLocal(&lt);
}


#if !defined(GUI) || !defined(SFX_MODULE)
void RarTime::GetText(wchar *DateStr,size_t MaxSize,bool FullYear,bool FullMS)
{
  if (IsSet())
  {
    RarLocalTime lt;
    GetLocal(&lt);
    if (FullMS)
      swprintf(DateStr,MaxSize,L"%u-%02u-%02u %02u:%02u,%03u",lt.Year,lt.Month,lt.Day,lt.Hour,lt.Minute,lt.Reminder/10000);
    else
      if (FullYear)
        swprintf(DateStr,MaxSize,L"%02u-%02u-%u %02u:%02u",lt.Day,lt.Month,lt.Year,lt.Hour,lt.Minute);
      else
        swprintf(DateStr,MaxSize,L"%02u-%02u-%02u %02u:%02u",lt.Day,lt.Month,lt.Year%100,lt.Hour,lt.Minute);
  }
  else
  {
    // We use escape before '?' to avoid weird C trigraph characters.
    wcscpy(DateStr,FullYear ? L"\?\?-\?\?-\?\?\?\? \?\?:\?\?":L"\?\?-\?\?-\?\? \?\?:\?\?");
  }
}
#endif


#ifndef SFX_MODULE
void RarTime::SetIsoText(const wchar *TimeText)
{
  int Field[6];
  memset(Field,0,sizeof(Field));
  for (uint DigitCount=0;*TimeText!=0;TimeText++)
    if (IsDigit(*TimeText))
    {
      int FieldPos=DigitCount<4 ? 0:(DigitCount-4)/2+1;
      if (FieldPos<ASIZE(Field))
        Field[FieldPos]=Field[FieldPos]*10+*TimeText-'0';
      DigitCount++;
    }
  RarLocalTime lt;
  lt.Second=Field[5];
  lt.Minute=Field[4];
  lt.Hour=Field[3];
  lt.Day=Field[2]==0 ? 1:Field[2];
  lt.Month=Field[1]==0 ? 1:Field[1];
  lt.Year=Field[0];
  lt.Reminder=0;
  SetLocal(&lt);
}
#endif


#ifndef SFX_MODULE
void RarTime::SetAgeText(const wchar *TimeText)
{
  uint Seconds=0,Value=0;
  for (int I=0;TimeText[I]!=0;I++)
  {
    int Ch=TimeText[I];
    if (IsDigit(Ch))
      Value=Value*10+Ch-'0';
    else
    {
      switch(etoupper(Ch))
      {
        case 'D':
          Seconds+=Value*24*3600;
          break;
        case 'H':
          Seconds+=Value*3600;
          break;
        case 'M':
          Seconds+=Value*60;
          break;
        case 'S':
          Seconds+=Value;
          break;
      }
      Value=0;
    }
  }
  SetCurrentTime();
  int64 RawTime=GetRaw();
  SetRaw(itime-uint64(Seconds)*10000000);
}
#endif


void RarTime::SetCurrentTime()
{
#ifdef _WIN_ALL
  FILETIME ft;
  SYSTEMTIME st;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st,&ft);
  *this=ft;
#else
  time_t st;
  time(&st);
  *this=st;
#endif
}


#ifndef SFX_MODULE
const wchar *GetMonthName(int Month)
{
#ifdef SILENT
  return L"";
#else
  static MSGID MonthID[]={
         MMonthJan,MMonthFeb,MMonthMar,MMonthApr,MMonthMay,MMonthJun,
         MMonthJul,MMonthAug,MMonthSep,MMonthOct,MMonthNov,MMonthDec
  };
  return St(MonthID[Month]);
#endif
}
#endif


bool IsLeapYear(int Year)
{
  return (Year&3)==0 && (Year%100!=0 || Year%400==0);
}
