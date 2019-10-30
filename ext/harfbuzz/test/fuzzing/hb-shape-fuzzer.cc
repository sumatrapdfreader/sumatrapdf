#include "hb-fuzzer.hh"

#include <hb-ot.h>
#include <string.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  hb_blob_t *blob = hb_blob_create((const char *)data, size,
                                   HB_MEMORY_MODE_READONLY, NULL, NULL);
  hb_face_t *face = hb_face_create(blob, 0);
  hb_font_t *font = hb_font_create(face);
  hb_ot_font_set_funcs(font);
  hb_font_set_scale(font, 12, 12);

  {
    const char text[] = "ABCDEXYZ123@_%&)*$!";
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, NULL, 0);
    hb_buffer_destroy(buffer);
  }

  uint32_t text32[16];
  if (size > sizeof(text32)) {
    memcpy(text32, data + size - sizeof(text32), sizeof(text32));
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf32(buffer, text32, sizeof(text32)/sizeof(text32[0]), 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, NULL, 0);

    unsigned int len = hb_buffer_get_length (buffer);
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos (buffer, NULL);
    //hb_glyph_position_t *positions = hb_buffer_get_glyph_positions (buffer, NULL);
    for (unsigned int i = 0; i < len; i++)
    {
      hb_glyph_info_t info = infos[i];
      //hb_glyph_position_t pos = positions[i];

      hb_glyph_extents_t extents;
      hb_font_get_glyph_extents (font, info.codepoint, &extents);
    }

    hb_buffer_destroy(buffer);
  }


  hb_font_destroy(font);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
  return 0;
}
