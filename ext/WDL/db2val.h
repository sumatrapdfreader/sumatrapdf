#ifndef _WDL_DB2VAL_H_
#define _WDL_DB2VAL_H_

#include <math.h>

#define TWENTY_OVER_LN10 8.6858896380650365530225783783321
#define LN10_OVER_TWENTY 0.11512925464970228420089957273422
#define DB2VAL(x) exp((x)*LN10_OVER_TWENTY)

static inline double VAL2DB(double x)
{
  if (x < 0.0000000298023223876953125) return -150.0;
  double v=log(x)*TWENTY_OVER_LN10;
  return v<-150.0?-150.0:v;
}

static inline double VAL2DB_EX(double x, double mindb)
{
  return x <= DB2VAL(mindb) ? mindb : (log(x)*TWENTY_OVER_LN10);
}

#endif