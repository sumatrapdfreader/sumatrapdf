#ifndef CRTEST_H
#define CRTEST_H

#include "lvtypes.h"
#include "lvstring.h"
#include "lvstream.h"

#define MYASSERT(x,t) \
    if (!(x)) { \
            CRLog::error("Assertion failed at %s #%d : %s", __FILE__, __LINE__, t); \
            crFatalError(1111, "Exiting: UnitTest assertion failed"); \
    }


LVStreamRef LVCreateCompareTestStream( LVStreamRef stream1, LVStreamRef stream2 );

void runCRUnitTests();

#endif // CRTEST_H
