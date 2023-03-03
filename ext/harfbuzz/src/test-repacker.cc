/*
 * Copyright © 2020  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Garret Rieger
 */

#include <string>

#include "hb-repacker.hh"
#include "hb-open-type.hh"
#include "graph/serialize.hh"

static void extend (const char* value,
                    unsigned len,
                    hb_serialize_context_t* c)
{
  char* obj = c->allocate_size<char> (len);
  hb_memcpy (obj, value, len);
}

static void start_object(const char* tag,
                         unsigned len,
                         hb_serialize_context_t* c)
{
  c->push ();
  extend (tag, len, c);
}

static unsigned add_object(const char* tag,
                           unsigned len,
                           hb_serialize_context_t* c)
{
  start_object (tag, len, c);
  return c->pop_pack (false);
}


static void add_offset (unsigned id,
                        hb_serialize_context_t* c)
{
  OT::Offset16* offset = c->start_embed<OT::Offset16> ();
  c->extend_min (offset);
  c->add_link (*offset, id);
}

static void add_24_offset (unsigned id,
                           hb_serialize_context_t* c)
{
  OT::Offset24* offset = c->start_embed<OT::Offset24> ();
  c->extend_min (offset);
  c->add_link (*offset, id);
}

static void add_wide_offset (unsigned id,
                             hb_serialize_context_t* c)
{
  OT::Offset32* offset = c->start_embed<OT::Offset32> ();
  c->extend_min (offset);
  c->add_link (*offset, id);
}

static void add_gsubgpos_header (unsigned lookup_list,
                                 hb_serialize_context_t* c)
{
  char header[] = {
    0, 1, // major
    0, 0, // minor
    0, 0, // script list
    0, 0, // feature list
  };

  start_object (header, 8, c);
  add_offset (lookup_list, c);
  c->pop_pack (false);
}

static unsigned add_lookup_list (const unsigned* lookups,
                                 char count,
                                 hb_serialize_context_t* c)
{
  char lookup_count[] = {0, count};
  start_object  ((char *) &lookup_count, 2, c);

  for (int i = 0; i < count; i++)
    add_offset (lookups[i], c);

  return c->pop_pack (false);
}

static void start_lookup (int8_t type,
                          int8_t num_subtables,
                          hb_serialize_context_t* c)
{
  char lookup[] = {
    0, (char)type, // type
    0, 0, // flag
    0, (char)num_subtables, // num subtables
  };

  start_object (lookup, 6, c);
}

static unsigned finish_lookup (hb_serialize_context_t* c)
{
  char filter[] = {0, 0};
  extend (filter, 2, c);
  return c->pop_pack (false);
}

static unsigned add_extension (unsigned child,
                               uint8_t type,
                               hb_serialize_context_t* c)
{
  char ext[] = {
    0, 1,
    0, (char) type,
  };

  start_object (ext, 4, c);
  add_wide_offset (child, c);

  return c->pop_pack (false);

}

// Adds coverage table fro [start, end]
static unsigned add_coverage (unsigned start, unsigned end,
                              hb_serialize_context_t* c)
{
  if (end - start == 1)
  {
    uint8_t coverage[] = {
      0, 1, // format
      0, 2, // count

      (uint8_t) ((start >> 8) & 0xFF),
      (uint8_t) (start & 0xFF), // glyph[0]

      (uint8_t) ((end >> 8) & 0xFF),
      (uint8_t) (end & 0xFF), // glyph[1]
    };
    return add_object ((char*) coverage, 8, c);
  }

  uint8_t coverage[] = {
    0, 2, // format
    0, 1, // range count

    (uint8_t) ((start >> 8) & 0xFF),
    (uint8_t) (start & 0xFF), // start

    (uint8_t) ((end >> 8) & 0xFF),
    (uint8_t) (end & 0xFF), // end

    0, 0,
  };
  return add_object ((char*) coverage, 10, c);
}


template<typename It>
static unsigned add_coverage (It it,
                              hb_serialize_context_t* c)
{
  c->push ();
  OT::Layout::Common::Coverage_serialize (c, it);
  return c->pop_pack (false);
}

// Adds a class that maps glyphs from [start_glyph, end_glyph)
// to classes 1...n
static unsigned add_class_def (uint16_t start_glyph,
                               uint16_t end_glyph,
                               hb_serialize_context_t* c)
{
  unsigned count = end_glyph - start_glyph;
  uint8_t header[] = {
    0, 1, // format

    (uint8_t) ((start_glyph >> 8) & 0xFF),
    (uint8_t) (start_glyph & 0xFF), // start_glyph

    (uint8_t) ((count >> 8) & 0xFF),
    (uint8_t) (count & 0xFF), // count
  };

  start_object ((char*) header, 6, c);
  for (uint16_t i = 1; i <= count; i++)
  {
    uint8_t class_value[] = {
      (uint8_t) ((i >> 8) & 0xFF),
      (uint8_t) (i & 0xFF), // count
    };
    extend ((char*) class_value, 2, c);
  }

  return c->pop_pack (false);
}

static unsigned add_pair_pos_1 (unsigned* pair_sets,
                                char count,
                                unsigned coverage,
                                hb_serialize_context_t* c)
{
  char format[] = {
    0, 1
  };

  start_object (format, 2, c);
  add_offset (coverage, c);

  char value_format[] = {
    0, 0,
    0, 0,
    0, count,
  };
  extend (value_format, 6, c);

  for (char i = 0; i < count; i++)
    add_offset (pair_sets[(unsigned) i], c);

  return c->pop_pack (false);
}

static unsigned add_pair_pos_2 (unsigned starting_class,
                                unsigned coverage,
                                unsigned class_def_1, uint16_t class_def_1_count,
                                unsigned class_def_2, uint16_t class_def_2_count,
                                unsigned* device_tables,
                                hb_serialize_context_t* c)
{
  uint8_t format[] = {
    0, 2
  };

  start_object ((char*) format, 2, c);
  add_offset (coverage, c);

  unsigned num_values = 4;
  uint8_t format1 = 0x01 | 0x02 | 0x08;
  uint8_t format2 = 0x04;
  if (device_tables) {
    format2 |= 0x20;
    num_values += 1;
  }
  uint8_t value_format[] = {
    0, format1,
    0, format2,
  };

  extend ((char*) value_format, 4, c);

  add_offset (class_def_1, c);
  add_offset (class_def_2, c);

  uint8_t class_counts[] = {
    (uint8_t) ((class_def_1_count >> 8) & 0xFF),
    (uint8_t) (class_def_1_count & 0xFF),
    (uint8_t) ((class_def_2_count >> 8) & 0xFF),
    (uint8_t) (class_def_2_count & 0xFF),
  };
  extend ((char*) class_counts, 4, c);

  unsigned num_bytes_per_record = class_def_2_count * num_values * 2;
  uint8_t* record = (uint8_t*) calloc (1, num_bytes_per_record);
  int device_index = 0;
  for (uint16_t i = 0; i < class_def_1_count; i++)
  {

    for (uint16_t j = 0; j < class_def_2_count; j++)
    {
      for (int k = 0; k < 4; k++) {
        uint8_t value[] = {
          (uint8_t) (i + starting_class),
          (uint8_t) (i + starting_class),
        };
        extend ((char*) value, 2, c);
      }

      if (device_tables) {
        add_offset (device_tables[device_index++], c);
      }
    }
  }
  free (record);

  return c->pop_pack (false);
}

static unsigned add_mark_base_pos_1 (unsigned mark_coverage,
                                     unsigned base_coverage,
                                     unsigned mark_array,
                                     unsigned base_array,
                                     unsigned class_count,
                                     hb_serialize_context_t* c)
{
  uint8_t format[] = {
    0, 1
  };

  start_object ((char*) format, 2, c);
  add_offset (mark_coverage, c);
  add_offset (base_coverage, c);

  uint8_t count[] = {
    (uint8_t) ((class_count >> 8) & 0xFF),
    (uint8_t) (class_count & 0xFF),
  };
  extend ((char*) count, 2, c);

  add_offset (mark_array, c);
  add_offset (base_array, c);

  return c->pop_pack (false);
}

template<int mark_count,
    int class_count,
    int base_count,
    int table_count>
struct MarkBasePosBuffers
{
  unsigned base_anchors[class_count * base_count];
  unsigned mark_anchors[mark_count];
  uint8_t anchor_buffers[class_count * base_count + 100];
  uint8_t class_buffer[class_count * 2];

  MarkBasePosBuffers(hb_serialize_context_t* c)
  {
    for (unsigned i = 0; i < sizeof(anchor_buffers) / 2; i++)
    {
      OT::HBUINT16* value = (OT::HBUINT16*) (&anchor_buffers[2*i]);
      *value = i;
    }

    for (unsigned i = 0; i < class_count * base_count; i++)
    {
      base_anchors[i] = add_object ((char*) &anchor_buffers[i], 100, c);
      if (i < class_count) {
        class_buffer[i*2] = (uint8_t) ((i >> 8) & 0xFF);
        class_buffer[i*2 + 1] = (uint8_t) (i & 0xFF);
      }
    }

    for (unsigned i = 0; i < mark_count; i++)
    {
      mark_anchors[i] = add_object ((char*) &anchor_buffers[i], 4, c);
    }
  }

  unsigned create_mark_base_pos_1 (unsigned table_index, hb_serialize_context_t* c)
  {
    unsigned class_per_table = class_count / table_count;
    unsigned mark_per_class = mark_count / class_count;
    unsigned start_class = class_per_table * table_index;
    unsigned end_class = class_per_table * (table_index + 1) - 1;

    // baseArray
    uint8_t base_count_buffer[] = {
      (uint8_t) ((base_count >> 8) & 0xFF),
      (uint8_t) (base_count & 0xFF),

    };
    start_object ((char*) base_count_buffer, 2, c);
    for (unsigned base = 0; base < base_count; base++)
    {
      for (unsigned klass = start_class; klass <= end_class; klass++)
      {
        unsigned i = base * class_count + klass;
        add_offset (base_anchors[i], c);
      }
    }
    unsigned base_array = c->pop_pack (false);

    // markArray
    unsigned num_marks = class_per_table * mark_per_class;
    uint8_t mark_count_buffer[] = {
      (uint8_t) ((num_marks >> 8) & 0xFF),
      (uint8_t) (num_marks & 0xFF),
    };
    start_object ((char*) mark_count_buffer, 2, c);
    for (unsigned mark = 0; mark < mark_count; mark++)
    {
      unsigned klass = mark % class_count;
      if (klass < start_class || klass > end_class) continue;
      klass -= start_class;

      extend ((char*) &class_buffer[2 * klass], 2, c);
      add_offset (mark_anchors[mark], c);
    }
    unsigned mark_array = c->pop_pack (false);

    // markCoverage
    auto it =
        + hb_range ((hb_codepoint_t) mark_count)
        | hb_filter ([&] (hb_codepoint_t mark) {
          unsigned klass = mark % class_count;
          return klass >= class_per_table * table_index &&
              klass < class_per_table * (table_index + 1);
        })
        ;
    unsigned mark_coverage = add_coverage (it, c);

    // baseCoverage
    unsigned base_coverage = add_coverage (10, 10 + base_count - 1, c);

    return add_mark_base_pos_1 (mark_coverage,
                                base_coverage,
                                mark_array,
                                base_array,
                                class_per_table,
                                c);
  }
};





static void run_resolve_overflow_test (const char* name,
                                       hb_serialize_context_t& overflowing,
                                       hb_serialize_context_t& expected,
                                       unsigned num_iterations = 0,
                                       bool recalculate_extensions = false,
                                       hb_tag_t tag = HB_TAG ('G', 'S', 'U', 'B'))
{
  printf (">>> Testing overflowing resolution for %s\n",
          name);

  graph_t graph (overflowing.object_graph ());
  graph_t expected_graph (expected.object_graph ());
  if (graph::will_overflow (expected_graph))
  {
    expected_graph.assign_spaces ();
    expected_graph.sort_shortest_distance ();
  }

  // Check that overflow resolution succeeds
  assert (overflowing.offset_overflow ());
  assert (hb_resolve_graph_overflows (tag,
                                      num_iterations,
                                      recalculate_extensions,
                                      graph));

  // Check the graphs can be serialized.
  hb_blob_t* out = graph::serialize (graph);
  assert (out);
  hb_blob_destroy (out);
  out = graph::serialize (expected_graph);
  assert (out);
  hb_blob_destroy (out);

  // Check the graphs are equivalent
  graph.normalize ();
  expected_graph.normalize ();
  assert (graph == expected_graph);
}

static void add_virtual_offset (unsigned id,
                                hb_serialize_context_t* c)
{
  c->add_virtual_link (id);
}

static void
populate_serializer_simple (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_1 = add_object ("ghi", 3, c);
  unsigned obj_2 = add_object ("def", 3, c);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_overflow (hb_serialize_context_t* c)
{
  std::string large_string(50000, 'a');
  c->start_serialize<char> ();

  unsigned obj_1 = add_object (large_string.c_str(), 10000, c);
  unsigned obj_2 = add_object (large_string.c_str(), 20000, c);
  unsigned obj_3 = add_object (large_string.c_str(), 50000, c);

  start_object ("abc", 3, c);
  add_offset (obj_3, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_priority_overflow (hb_serialize_context_t* c)
{
  std::string large_string(50000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e = add_object ("e", 1, c);
  unsigned obj_d = add_object ("d", 1, c);

  start_object (large_string.c_str (), 50000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str (), 20000, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_priority_overflow_expected (hb_serialize_context_t* c)
{
  std::string large_string(50000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e = add_object ("e", 1, c);

  start_object (large_string.c_str (), 50000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  unsigned obj_d = add_object ("d", 1, c);

  start_object (large_string.c_str (), 20000, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}


static void
populate_serializer_with_dedup_overflow (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_1 = add_object ("def", 3, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_1, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_4 = add_object ("4", 1, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_4, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("1", 1, c);
  add_wide_offset (obj_3, c);
  add_offset (obj_2, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_complex (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_e, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d, c);
  unsigned obj_h = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_c, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_d, c);
  unsigned obj_g = c->pop_pack (false);

  start_object (large_string.c_str(), 11000, c);
  add_offset (obj_d, c);
  unsigned obj_i = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_offset (obj_g, c);
  add_offset (obj_i, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_complex_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();


  // space 1

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f_prime, c);
  unsigned obj_e_prime = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_d_prime = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d_prime, c);
  unsigned obj_h = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_c, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  // space 0

  unsigned obj_f = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);


  start_object ("d", 1, c);
  add_offset (obj_e, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 11000, c);
  add_offset (obj_d, c);
  unsigned obj_i = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_d, c);
  unsigned obj_g = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_offset (obj_g, c);
  add_offset (obj_i, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_spaces (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_d = add_object ("f", 1, c);
  unsigned obj_e = add_object ("f", 1, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack ();

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack ();


  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack ();

  c->end_serialize();
}

static void
populate_serializer_spaces (hb_serialize_context_t* c, bool with_overflow)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_i;

  if (with_overflow)
    obj_i = add_object ("i", 1, c);

  // Space 2
  unsigned obj_h = add_object ("h", 1, c);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_h, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  // Space 1
  if (!with_overflow)
    obj_i = add_object ("i", 1, c);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_i, c);
  unsigned obj_g = c->pop_pack (false);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_i, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_g, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_f, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_spaces_16bit_connection (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_g = add_object ("g", 1, c);
  unsigned obj_h = add_object ("h", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g, c);
  unsigned obj_e = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_h, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_spaces_16bit_connection_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_g_prime = add_object ("g", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g_prime, c);
  unsigned obj_e_prime = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c = c->pop_pack (false);

  unsigned obj_h_prime = add_object ("h", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_h_prime, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  unsigned obj_g = add_object ("g", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g, c);
  unsigned obj_e = c->pop_pack (false);

  unsigned obj_h = add_object ("h", 1, c);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize ();
}

static void
populate_serializer_short_and_wide_subgraph_root (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_c, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_c, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_short_and_wide_subgraph_root_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e_prime = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c_prime = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_c_prime, c);
  unsigned obj_d = c->pop_pack (false);

  unsigned obj_e = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);


  start_object ("b", 1, c);
  add_offset (obj_c, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c_prime, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_2 (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_expected (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space

  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_prime, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_expected_2 (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space

  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  // Space 2

  unsigned obj_f_double_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_double_prime, c);
  unsigned obj_d_prime = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d_prime, c);
  unsigned obj_b_prime = c->pop_pack (false);

  // Space 1

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_prime, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  // Space 0

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  // Root
  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_b_prime, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_complex_2 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_5 = add_object ("mn", 2, c);

  unsigned obj_4 = add_object ("jkl", 3, c);

  start_object ("ghi", 3, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object ("def", 3, c);
  add_offset (obj_3, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_4, c);
  add_offset (obj_5, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_complex_3 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_6 = add_object ("opqrst", 6, c);

  unsigned obj_5 = add_object ("mn", 2, c);

  start_object ("jkl", 3, c);
  add_offset (obj_6, c);
  unsigned obj_4 = c->pop_pack (false);

  start_object ("ghi", 3, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object ("def", 3, c);
  add_offset (obj_3, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_4, c);
  add_offset (obj_5, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_virtual_link (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_d = add_object ("d", 1, c);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("e", 1, c);
  add_virtual_offset (obj_b, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_24_and_32_bit_offsets (hb_serialize_context_t* c)
{
  std::string large_string(60000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);
  unsigned obj_g = add_object ("g", 1, c);
  unsigned obj_j = add_object ("j", 1, c);
  unsigned obj_k = add_object ("k", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_j, c);
  unsigned obj_h = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_k, c);
  unsigned obj_i = c->pop_pack (false);

  start_object ("e", 1, c);
  add_wide_offset (obj_h, c);
  add_wide_offset (obj_i, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_24_offset (obj_c, c);
  add_24_offset (obj_d, c);
  add_24_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_24_offset (obj_b, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_extension_promotion (hb_serialize_context_t* c,
                                              int num_extensions = 0)
{
  constexpr int num_lookups = 5;
  constexpr int num_subtables = num_lookups * 2;
  unsigned int lookups[num_lookups];
  unsigned int subtables[num_subtables];
  unsigned int extensions[num_subtables];

  std::string large_string(60000, 'a');
  c->start_serialize<char> ();


  for (int i = num_subtables - 1; i >= 0; i--)
    subtables[i] = add_object(large_string.c_str (), 15000, c);

  for (int i = num_subtables - 1;
       i >= (num_lookups - num_extensions) * 2;
       i--)
  {
    unsigned ext_index = i - (num_lookups - num_extensions) * 2;
    unsigned subtable_index = num_subtables - ext_index - 1;
    extensions[i] = add_extension (subtables[subtable_index], 5, c);
  }

  for (int i = num_lookups - 1; i >= 0; i--)
  {
    bool is_ext = (i >= (num_lookups - num_extensions));

    start_lookup (is_ext ? (char) 7 : (char) 5,
                  2,
                  c);

    if (is_ext) {
      add_offset (extensions[i * 2], c);
      add_offset (extensions[i * 2 + 1], c);
    } else {
      add_offset (subtables[i * 2], c);
      add_offset (subtables[i * 2 + 1], c);
    }

    lookups[i] = finish_lookup (c);
  }

  unsigned lookup_list = add_lookup_list (lookups, num_lookups, c);

  add_gsubgpos_header (lookup_list, c);

  c->end_serialize();
}

template<int num_pair_pos_1, int num_pair_set>
static void
populate_serializer_with_large_pair_pos_1 (hb_serialize_context_t* c,
                                           bool as_extension = false)
{
  std::string large_string(60000, 'a');
  c->start_serialize<char> ();

  constexpr int total_pair_set = num_pair_pos_1 * num_pair_set;
  unsigned pair_set[total_pair_set];
  unsigned coverage[num_pair_pos_1];
  unsigned pair_pos_1[num_pair_pos_1];

  for (int i = num_pair_pos_1 - 1; i >= 0; i--)
  {
    for (int j = (i + 1) * num_pair_set - 1; j >= i * num_pair_set; j--)
      pair_set[j] = add_object (large_string.c_str (), 30000 + j, c);

    coverage[i] = add_coverage (i * num_pair_set,
                                (i + 1) * num_pair_set - 1, c);

    pair_pos_1[i] = add_pair_pos_1 (&pair_set[i * num_pair_set],
                                    num_pair_set,
                                    coverage[i],
                                    c);
  }

  unsigned pair_pos_2 = add_object (large_string.c_str(), 200, c);

  if (as_extension) {
    pair_pos_2 = add_extension (pair_pos_2, 2, c);
    for (int i = num_pair_pos_1 - 1; i >= 0; i--)
      pair_pos_1[i] = add_extension (pair_pos_1[i], 2, c);
  }

  start_lookup (as_extension ? 9 : 2, 1 + num_pair_pos_1, c);

  for (int i = 0; i < num_pair_pos_1; i++)
    add_offset (pair_pos_1[i], c);
  add_offset (pair_pos_2, c);

  unsigned lookup = finish_lookup (c);

  unsigned lookup_list = add_lookup_list (&lookup, 1, c);

  add_gsubgpos_header (lookup_list, c);

  c->end_serialize();
}

template<int num_pair_pos_2, int num_class_1, int num_class_2>
static void
populate_serializer_with_large_pair_pos_2 (hb_serialize_context_t* c,
                                           bool as_extension = false,
                                           bool with_device_tables = false,
                                           bool extra_table = true)
{
  std::string large_string(100000, 'a');
  c->start_serialize<char> ();

  unsigned coverage[num_pair_pos_2];
  unsigned class_def_1[num_pair_pos_2];
  unsigned class_def_2[num_pair_pos_2];
  unsigned pair_pos_2[num_pair_pos_2];

  unsigned* device_tables = (unsigned*) calloc (num_pair_pos_2 * num_class_1 * num_class_2,
                                                sizeof(unsigned));

  // Total glyphs = num_class_1 * num_pair_pos_2
  for (int i = num_pair_pos_2 - 1; i >= 0; i--)
  {
    unsigned start_glyph = 5 + i * num_class_1;
    if (num_class_2 >= num_class_1)
    {
      class_def_2[i] = add_class_def (11,
                                      10 + num_class_2, c);
      class_def_1[i] = add_class_def (start_glyph + 1,
                                      start_glyph + num_class_1,
                                      c);
    } else {
      class_def_1[i] = add_class_def (start_glyph + 1,
                                      start_glyph + num_class_1,
                                      c);
      class_def_2[i] = add_class_def (11,
                                      10 + num_class_2, c);
    }

    coverage[i] = add_coverage (start_glyph,
                                start_glyph + num_class_1 - 1,
                                c);

    if (with_device_tables)
    {
      for(int j = (i + 1) * num_class_1 * num_class_2 - 1;
          j >= i * num_class_1 * num_class_2;
          j--)
      {
        uint8_t table[] = {
          (uint8_t) ((j >> 8) & 0xFF),
          (uint8_t) (j & 0xFF),
        };
        device_tables[j] = add_object ((char*) table, 2, c);
      }
    }

    pair_pos_2[i] = add_pair_pos_2 (1 + i * num_class_1,
                                    coverage[i],
                                    class_def_1[i], num_class_1,
                                    class_def_2[i], num_class_2,
                                    with_device_tables
                                    ? &device_tables[i * num_class_1 * num_class_2]
                                    : nullptr,
                                    c);
  }


  unsigned pair_pos_1 = 0;
  if (extra_table) pair_pos_1 = add_object (large_string.c_str(), 100000, c);

  if (as_extension) {
    for (int i = num_pair_pos_2 - 1; i >= 0; i--)
      pair_pos_2[i] = add_extension (pair_pos_2[i], 2, c);

    if (extra_table)
      pair_pos_1 = add_extension (pair_pos_1, 2, c);
  }

  start_lookup (as_extension ? 9 : 2, 1 + num_pair_pos_2, c);

  if (extra_table)
    add_offset (pair_pos_1, c);

  for (int i = 0; i < num_pair_pos_2; i++)
    add_offset (pair_pos_2[i], c);

  unsigned lookup = finish_lookup (c);

  unsigned lookup_list = add_lookup_list (&lookup, 1, c);

  add_gsubgpos_header (lookup_list, c);

  c->end_serialize();

  free (device_tables);
}

template<int mark_count,
    int class_count,
    int base_count,
    int table_count>
static void
populate_serializer_with_large_mark_base_pos_1 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  MarkBasePosBuffers<mark_count, class_count, base_count, table_count> buffers (c);

  unsigned mark_base_pos[table_count];
  for (unsigned i = 0; i < table_count; i++)
    mark_base_pos[i] = buffers.create_mark_base_pos_1 (i, c);

  for (int i = 0; i < table_count; i++)
    mark_base_pos[i] = add_extension (mark_base_pos[i], 4, c);

  start_lookup (9, table_count, c);

  for (int i = 0; i < table_count; i++)
    add_offset (mark_base_pos[i], c);

  unsigned lookup = finish_lookup (c);

  unsigned lookup_list = add_lookup_list (&lookup, 1, c);

  add_gsubgpos_header (lookup_list, c);

  c->end_serialize();
}

static void test_sort_shortest ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);

  graph_t graph (c.object_graph ());
  graph.sort_shortest_distance ();
  assert (!graph.in_error ());

  assert(strncmp (graph.object (4).head, "abc", 3) == 0);
  assert(graph.object (4).real_links.length == 3);
  assert(graph.object (4).real_links[0].objidx == 2);
  assert(graph.object (4).real_links[1].objidx == 0);
  assert(graph.object (4).real_links[2].objidx == 3);

  assert(strncmp (graph.object (3).head, "mn", 2) == 0);
  assert(graph.object (3).real_links.length == 0);

  assert(strncmp (graph.object (2).head, "def", 3) == 0);
  assert(graph.object (2).real_links.length == 1);
  assert(graph.object (2).real_links[0].objidx == 1);

  assert(strncmp (graph.object (1).head, "ghi", 3) == 0);
  assert(graph.object (1).real_links.length == 1);
  assert(graph.object (1).real_links[0].objidx == 0);

  assert(strncmp (graph.object (0).head, "jkl", 3) == 0);
  assert(graph.object (0).real_links.length == 0);

  free (buffer);
}

static void test_duplicate_leaf ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);

  graph_t graph (c.object_graph ());
  graph.duplicate (4, 1);

  assert(strncmp (graph.object (5).head, "abc", 3) == 0);
  assert(graph.object (5).real_links.length == 3);
  assert(graph.object (5).real_links[0].objidx == 3);
  assert(graph.object (5).real_links[1].objidx == 4);
  assert(graph.object (5).real_links[2].objidx == 0);

  assert(strncmp (graph.object (4).head, "jkl", 3) == 0);
  assert(graph.object (4).real_links.length == 0);

  assert(strncmp (graph.object (3).head, "def", 3) == 0);
  assert(graph.object (3).real_links.length == 1);
  assert(graph.object (3).real_links[0].objidx == 2);

  assert(strncmp (graph.object (2).head, "ghi", 3) == 0);
  assert(graph.object (2).real_links.length == 1);
  assert(graph.object (2).real_links[0].objidx == 1);

  assert(strncmp (graph.object (1).head, "jkl", 3) == 0);
  assert(graph.object (1).real_links.length == 0);

  assert(strncmp (graph.object (0).head, "mn", 2) == 0);
  assert(graph.object (0).real_links.length == 0);

  free (buffer);
}

static void test_duplicate_interior ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_3 (&c);

  graph_t graph (c.object_graph ());
  graph.duplicate (3, 2);

  assert(strncmp (graph.object (6).head, "abc", 3) == 0);
  assert(graph.object (6).real_links.length == 3);
  assert(graph.object (6).real_links[0].objidx == 4);
  assert(graph.object (6).real_links[1].objidx == 2);
  assert(graph.object (6).real_links[2].objidx == 1);

  assert(strncmp (graph.object (5).head, "jkl", 3) == 0);
  assert(graph.object (5).real_links.length == 1);
  assert(graph.object (5).real_links[0].objidx == 0);

  assert(strncmp (graph.object (4).head, "def", 3) == 0);
  assert(graph.object (4).real_links.length == 1);
  assert(graph.object (4).real_links[0].objidx == 3);

  assert(strncmp (graph.object (3).head, "ghi", 3) == 0);
  assert(graph.object (3).real_links.length == 1);
  assert(graph.object (3).real_links[0].objidx == 5);

  assert(strncmp (graph.object (2).head, "jkl", 3) == 0);
  assert(graph.object (2).real_links.length == 1);
  assert(graph.object (2).real_links[0].objidx == 0);

  assert(strncmp (graph.object (1).head, "mn", 2) == 0);
  assert(graph.object (1).real_links.length == 0);

  assert(strncmp (graph.object (0).head, "opqrst", 6) == 0);
  assert(graph.object (0).real_links.length == 0);

  free (buffer);
}

static void
test_serialize ()
{
  size_t buffer_size = 100;
  void* buffer_1 = malloc (buffer_size);
  hb_serialize_context_t c1 (buffer_1, buffer_size);
  populate_serializer_simple (&c1);
  hb_bytes_t expected = c1.copy_bytes ();

  graph_t graph (c1.object_graph ());
  hb_blob_t* out = graph::serialize (graph);
  free (buffer_1);

  hb_bytes_t actual = out->as_bytes ();
  assert (actual == expected);
  expected.fini ();
  hb_blob_destroy (out);
}

static void test_will_overflow_1 ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);
  graph_t graph (c.object_graph ());

  assert (!graph::will_overflow (graph, nullptr));

  free (buffer);
}

static void test_will_overflow_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_overflow (&c);
  graph_t graph (c.object_graph ());

  assert (graph::will_overflow (graph, nullptr));

  free (buffer);
}

static void test_will_overflow_3 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_dedup_overflow (&c);
  graph_t graph (c.object_graph ());

  assert (graph::will_overflow (graph, nullptr));

  free (buffer);
}

static void test_resolve_overflows_via_sort ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_overflow (&c);
  graph_t graph (c.object_graph ());

  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG_NONE);
  assert (out);
  hb_bytes_t result = out->as_bytes ();
  assert (result.length == (80000 + 3 + 3 * 2));

  free (buffer);
  hb_blob_destroy (out);
}

static void test_resolve_overflows_via_duplication ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_dedup_overflow (&c);
  graph_t graph (c.object_graph ());

  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG_NONE);
  assert (out);
  hb_bytes_t result = out->as_bytes ();
  assert (result.length == (10000 + 2 * 2 + 60000 + 2 + 3 * 2));

  free (buffer);
  hb_blob_destroy (out);
}

static void test_resolve_overflows_via_space_assignment ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_spaces (&c, true);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_spaces (&e, false);

  run_resolve_overflow_test ("test_resolve_overflows_via_space_assignment",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolation ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow (&c);
  graph_t graph (c.object_graph ());

  assert (c.offset_overflow ());
  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), 0);
  assert (out);
  hb_bytes_t result = out->as_bytes ();
  assert (result.length == (1 + 10000 + 60000 + 1 + 1
                            + 4 + 3 * 2));

  free (buffer);
  hb_blob_destroy (out);
}

static void test_resolve_overflows_via_isolation_with_recursive_duplication ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow_complex (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_isolation_overflow_complex_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolation_with_recursive_duplication",
                             c,
                             e);
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolating_16bit_space ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_spaces_16bit_connection (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_spaces_16bit_connection_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolating_16bit_space",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolating_16bit_space_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_short_and_wide_subgraph_root (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_short_and_wide_subgraph_root_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolating_16bit_space_2",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolation_spaces ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow_spaces (&c);
  graph_t graph (c.object_graph ());

  assert (c.offset_overflow ());
  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), 0);
  assert (out);
  hb_bytes_t result = out->as_bytes ();

  unsigned expected_length = 3 + 2 * 60000; // objects
  expected_length += 2 * 4 + 2 * 2; // links
  assert (result.length == expected_length);

  free (buffer);
  hb_blob_destroy (out);
}

static void test_resolve_mixed_overflows_via_isolation_spaces ()
{
  size_t buffer_size = 200000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_24_and_32_bit_offsets (&c);
  graph_t graph (c.object_graph ());

  assert (c.offset_overflow ());
  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), 0);
  assert (out);
  hb_bytes_t result = out->as_bytes ();

  unsigned expected_length =
      // Objects
      7 +
      4 * 40000;

  expected_length +=
      // Links
      2 * 4 +  // 32
      4 * 3 +  // 24
      4 * 2;   // 16

  assert (result.length == expected_length);

  free (buffer);
  hb_blob_destroy (out);
}

static void test_resolve_with_extension_promotion ()
{
  size_t buffer_size = 200000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_extension_promotion (&c);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_extension_promotion (&e, 3);

  run_resolve_overflow_test ("test_resolve_with_extension_promotion",
                             c,
                             e,
                             20,
                             true);
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_basic_pair_pos_1_split ()
{
  size_t buffer_size = 200000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_pair_pos_1 <1, 4>(&c);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_pair_pos_1 <2, 2>(&e, true);

  run_resolve_overflow_test ("test_resolve_with_basic_pair_pos_1_split",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_extension_pair_pos_1_split ()
{
  size_t buffer_size = 200000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_pair_pos_1 <1, 4>(&c, true);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_pair_pos_1 <2, 2>(&e, true);

  run_resolve_overflow_test ("test_resolve_with_extension_pair_pos_1_split",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_basic_pair_pos_2_split ()
{
  size_t buffer_size = 300000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <1, 4, 3000>(&c);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <2, 2, 3000>(&e, true);

  run_resolve_overflow_test ("test_resolve_with_basic_pair_pos_2_split",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_close_to_limit_pair_pos_2_split ()
{
  size_t buffer_size = 300000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <1, 1596, 10>(&c, true, false, false);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <2, 798, 10>(&e, true, false, false);

  run_resolve_overflow_test ("test_resolve_with_close_to_limit_pair_pos_2_split",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_pair_pos_2_split_with_device_tables ()
{
  size_t buffer_size = 300000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <1, 4, 2000>(&c, false, true);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_pair_pos_2 <2, 2, 2000>(&e, true, true);

  run_resolve_overflow_test ("test_resolve_with_pair_pos_2_split_with_device_tables",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_with_basic_mark_base_pos_1_split ()
{
  size_t buffer_size = 200000;
  void* buffer = malloc (buffer_size);
  assert (buffer);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_large_mark_base_pos_1 <40, 10, 110, 1>(&c);

  void* expected_buffer = malloc (buffer_size);
  assert (expected_buffer);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_large_mark_base_pos_1 <40, 10, 110, 2>(&e);

  run_resolve_overflow_test ("test_resolve_with_basic_mark_base_pos_1_split",
                             c,
                             e,
                             20,
                             true,
                             HB_TAG('G', 'P', 'O', 'S'));
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_splitting_spaces ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_split_spaces (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_split_spaces_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_splitting_spaces",
                             c,
                             e,
                             1);

  free (buffer);
  free (expected_buffer);

}

static void test_resolve_overflows_via_splitting_spaces_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_split_spaces_2 (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_split_spaces_expected_2 (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_splitting_spaces_2",
                             c,
                             e,
                             1);
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_priority ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_priority_overflow (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_priority_overflow_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_priority",
                             c,
                             e,
                             3);
  free (buffer);
  free (expected_buffer);
}


static void test_virtual_link ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_virtual_link (&c);

  hb_blob_t* out = hb_resolve_overflows (c.object_graph (), HB_TAG_NONE);
  assert (out);

  hb_bytes_t result = out->as_bytes ();
  assert (result.length == 5 + 4 * 2);
  assert (result[0]  == 'a');
  assert (result[5]  == 'c');
  assert (result[8]  == 'e');
  assert (result[9]  == 'b');
  assert (result[12] == 'd');

  free (buffer);
  hb_blob_destroy (out);
}

static void
test_shared_node_with_virtual_links ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);

  c.start_serialize<char> ();

  unsigned obj_b = add_object ("b", 1, &c);
  unsigned obj_c = add_object ("c", 1, &c);

  start_object ("d", 1, &c);
  add_virtual_offset (obj_b, &c);
  unsigned obj_d_1 = c.pop_pack ();

  start_object ("d", 1, &c);
  add_virtual_offset (obj_c, &c);
  unsigned obj_d_2 = c.pop_pack ();

  assert (obj_d_1 == obj_d_2);

  start_object ("a", 1, &c);
  add_offset (obj_b, &c);
  add_offset (obj_c, &c);
  add_offset (obj_d_1, &c);
  add_offset (obj_d_2, &c);
  c.pop_pack ();
  c.end_serialize ();

  assert(c.object_graph() [obj_d_1]->virtual_links.length == 2);
  assert(c.object_graph() [obj_d_1]->virtual_links[0].objidx == obj_b);
  assert(c.object_graph() [obj_d_1]->virtual_links[1].objidx == obj_c);
  free(buffer);
}


// TODO(garretrieger): update will_overflow tests to check the overflows array.
// TODO(garretrieger): add tests for priority raising.

int
main (int argc, char **argv)
{
  test_serialize ();
  test_sort_shortest ();
  test_will_overflow_1 ();
  test_will_overflow_2 ();
  test_will_overflow_3 ();
  test_resolve_overflows_via_sort ();
  test_resolve_overflows_via_duplication ();
  test_resolve_overflows_via_priority ();
  test_resolve_overflows_via_space_assignment ();
  test_resolve_overflows_via_isolation ();
  test_resolve_overflows_via_isolation_with_recursive_duplication ();
  test_resolve_overflows_via_isolation_spaces ();
  test_resolve_overflows_via_isolating_16bit_space ();
  test_resolve_overflows_via_isolating_16bit_space_2 ();
  test_resolve_overflows_via_splitting_spaces ();
  test_resolve_overflows_via_splitting_spaces_2 ();
  test_resolve_mixed_overflows_via_isolation_spaces ();
  test_duplicate_leaf ();
  test_duplicate_interior ();
  test_virtual_link ();
  test_shared_node_with_virtual_links ();
  test_resolve_with_extension_promotion ();
  test_resolve_with_basic_pair_pos_1_split ();
  test_resolve_with_extension_pair_pos_1_split ();
  test_resolve_with_basic_pair_pos_2_split ();
  test_resolve_with_pair_pos_2_split_with_device_tables ();
  test_resolve_with_close_to_limit_pair_pos_2_split ();
  test_resolve_with_basic_mark_base_pos_1_split ();

  // TODO(grieger): have run overflow tests compare graph equality not final packed binary.
  // TODO(grieger): split test where multiple subtables in one lookup are split to test link ordering.
  // TODO(grieger): split test where coverage table in subtable that is being split is shared.
  // TODO(grieger): test with extensions already mixed in as well.
  // TODO(grieger): test two layer ext promotion setup.
  // TODO(grieger): test sorting by subtables per byte in ext. promotion.
}
