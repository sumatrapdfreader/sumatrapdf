#include <stdio.h>
#include "BaseUtil.h"
#include "ByteWriter.h"
#include "UtAssert.h"

void testByteWriter() {
  char buf[32];
  ByteWriter w = MakeByteWriterLE(buf, dimof(buf));
  utassert(w.Left() == 32);
  w.Write16(3);
  utassert(w.Left() == 30);
  utassert(buf[0] == 3);
  w.Write8(3);
  utassert(w.Left() == 29);

  w = MakeByteWriterBE(buf, dimof(buf));
  utassert(w.Left() == 32);
  w.Write16(3);
  utassert(w.Left() == 30);
  utassert(buf[0] == 0);
}

int main(int , char**) {
  testByteWriter();
  utassert_print_results();
}
