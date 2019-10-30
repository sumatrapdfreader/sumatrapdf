#include "hb-fuzzer.hh"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hb-subset.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  hb_blob_t *blob = hb_blob_create ((const char *)data, size,
                                    HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create (blob, 0);

  hb_set_t *output = hb_set_create();
  hb_face_collect_unicodes (face, output);

  hb_set_destroy (output);
  hb_face_destroy (face);
  hb_blob_destroy (blob);

  return 0;
}
