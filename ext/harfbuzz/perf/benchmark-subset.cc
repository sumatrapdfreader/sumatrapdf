#include "benchmark/benchmark.h"
#include <cassert>
#include <cstring>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hb-subset.h"


enum operation_t
{
  subset_codepoints,
  subset_glyphs,
  instance,
};

struct axis_location_t
{
  hb_tag_t axis_tag;
  float axis_value;
};

static const axis_location_t
_roboto_flex_instance_opts[] =
{
  {HB_TAG ('w', 'g', 'h', 't'), 600.f},
  {HB_TAG ('w', 'd', 't', 'h'), 75.f},
  {HB_TAG ('o', 'p', 's', 'z'), 90.f},
  {HB_TAG ('G', 'R', 'A', 'D'), -100.f},
  {HB_TAG ('s', 'l', 'n', 't'), -3.f},
  {HB_TAG ('X', 'T', 'R', 'A'), 500.f},
  {HB_TAG ('X', 'O', 'P', 'Q'), 150.f},
  {HB_TAG ('Y', 'O', 'P', 'Q'), 100.f},
  {HB_TAG ('Y', 'T', 'L', 'C'), 480.f},
  {HB_TAG ('Y', 'T', 'U', 'C'), 600.f},
  {HB_TAG ('Y', 'T', 'A', 'S'), 800.f},
  {HB_TAG ('Y', 'T', 'D', 'E'), -50.f},
  {HB_TAG ('Y', 'T', 'F', 'I'), 600.f},
};

static const axis_location_t
_mplus_instance_opts[] =
{
  {HB_TAG ('w', 'g', 'h', 't'), 800.f},
};

template <typename Type, unsigned int n>
static inline unsigned int ARRAY_LEN (const Type (&)[n]) { return n; }

#define SUBSET_FONT_BASE_PATH "test/subset/data/fonts/"

struct test_input_t
{
  const char *font_path;
  const unsigned max_subset_size;
  const axis_location_t *instance_opts;
  const unsigned num_instance_opts;
} tests[] =
{
  {SUBSET_FONT_BASE_PATH "Roboto-Regular.ttf", 4000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "Amiri-Regular.ttf", 4000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "NotoNastaliqUrdu-Regular.ttf", 1000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "NotoSansDevanagari-Regular.ttf", 1000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "Mplus1p-Regular.ttf", 10000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "SourceHanSans-Regular_subset.otf", 10000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "SourceSansPro-Regular.otf", 2000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "MPLUS1-Variable.ttf", 6000, _mplus_instance_opts, ARRAY_LEN (_mplus_instance_opts)},
  {SUBSET_FONT_BASE_PATH "RobotoFlex-Variable.ttf", 900, _roboto_flex_instance_opts, ARRAY_LEN (_roboto_flex_instance_opts)},
#if 0
  {"perf/fonts/NotoSansCJKsc-VF.ttf", 100000},
#endif
};

void AddCodepoints(const hb_set_t* codepoints_in_font,
                   unsigned subset_size,
                   hb_subset_input_t* input)
{
  auto *unicodes = hb_subset_input_unicode_set (input);
  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  for (unsigned i = 0; i < subset_size; i++) {
    // TODO(garretrieger): pick randomly.
    if (!hb_set_next (codepoints_in_font, &cp)) return;
    hb_set_add (unicodes, cp);
  }
}

void AddGlyphs(unsigned num_glyphs_in_font,
               unsigned subset_size,
               hb_subset_input_t* input)
{
  auto *glyphs = hb_subset_input_glyph_set (input);
  for (unsigned i = 0; i < subset_size && i < num_glyphs_in_font; i++) {
    // TODO(garretrieger): pick randomly.
    hb_set_add (glyphs, i);
  }
}

/* benchmark for subsetting a font */
static void BM_subset (benchmark::State &state,
                       operation_t operation,
                       const test_input_t &test_input)
{
  unsigned subset_size = state.range(0);

  hb_face_t *face;
  {
    hb_blob_t *blob = hb_blob_create_from_file_or_fail (test_input.font_path);
    assert (blob);
    face = hb_face_create (blob, 0);
    hb_blob_destroy (blob);
  }

  hb_subset_input_t* input = hb_subset_input_create_or_fail ();
  assert (input);

  switch (operation)
  {
    case subset_codepoints:
    {
      hb_set_t* all_codepoints = hb_set_create ();
      hb_face_collect_unicodes (face, all_codepoints);
      AddCodepoints(all_codepoints, subset_size, input);
      hb_set_destroy (all_codepoints);
    }
    break;

    case subset_glyphs:
    {
      unsigned num_glyphs = hb_face_get_glyph_count (face);
      AddGlyphs(num_glyphs, subset_size, input);
    }
    break;

    case instance:
#ifdef HB_EXPERIMENTAL_API
    {
      hb_set_t* all_codepoints = hb_set_create ();
      hb_face_collect_unicodes (face, all_codepoints);
      AddCodepoints(all_codepoints, subset_size, input);
      hb_set_destroy (all_codepoints);

      for (unsigned i = 0; i < test_input.num_instance_opts; i++)
        hb_subset_input_pin_axis_location (input, face,
                                           test_input.instance_opts[i].axis_tag,
                                           test_input.instance_opts[i].axis_value);
    }
#endif
    break;
  }

  for (auto _ : state)
  {
    hb_face_t* subset = hb_subset_or_fail (face, input);
    assert (subset);
    hb_face_destroy (subset);
  }

  hb_subset_input_destroy (input);
  hb_face_destroy (face);
}

static void test_subset (operation_t op,
                         const char *op_name,
                         benchmark::TimeUnit time_unit,
                         const test_input_t &test_input)
{
  if (op == instance && test_input.instance_opts == nullptr)
    return;

  char name[1024] = "BM_subset/";
  strcat (name, op_name);
  strcat (name, strrchr (test_input.font_path, '/'));

  benchmark::RegisterBenchmark (name, BM_subset, op, test_input)
      ->Range(10, test_input.max_subset_size)
      ->Unit(time_unit);
}

static void test_operation (operation_t op,
                            const char *op_name,
                            benchmark::TimeUnit time_unit)
{
  for (auto& test_input : tests)
  {
      test_subset (op, op_name, time_unit, test_input);
  }
}

int main(int argc, char** argv)
{
#define TEST_OPERATION(op, time_unit) test_operation (op, #op, time_unit)

  TEST_OPERATION (subset_glyphs, benchmark::kMillisecond);
  TEST_OPERATION (subset_codepoints, benchmark::kMillisecond);
#ifdef HB_EXPERIMENTAL_API
  TEST_OPERATION (instance, benchmark::kMillisecond);
#endif

#undef TEST_OPERATION

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
}
