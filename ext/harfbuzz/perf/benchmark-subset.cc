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
  unsigned max_subset_size;
  const axis_location_t *instance_opts;
  unsigned num_instance_opts;
} default_tests[] =
{
  {SUBSET_FONT_BASE_PATH "Roboto-Regular.ttf", 1000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "Amiri-Regular.ttf", 4096, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "NotoNastaliqUrdu-Regular.ttf", 1400, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "NotoSansDevanagari-Regular.ttf", 1000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "Mplus1p-Regular.ttf", 10000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "SourceHanSans-Regular_subset.otf", 10000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "SourceSansPro-Regular.otf", 2000, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "AdobeVFPrototype.otf", 300, nullptr, 0},
  {SUBSET_FONT_BASE_PATH "MPLUS1-Variable.ttf", 6000, _mplus_instance_opts, ARRAY_LEN (_mplus_instance_opts)},
  {SUBSET_FONT_BASE_PATH "RobotoFlex-Variable.ttf", 900, _roboto_flex_instance_opts, ARRAY_LEN (_roboto_flex_instance_opts)},
#if 0
  {"perf/fonts/NotoSansCJKsc-VF.ttf", 100000},
#endif
};

static test_input_t *tests = default_tests;
static unsigned num_tests = sizeof (default_tests) / sizeof (default_tests[0]);


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

// Preprocess face and populate the subset accelerator on it to speed up
// the subsetting operations.
static hb_face_t* preprocess_face(hb_face_t* face)
{
  hb_face_t* new_face = hb_subset_preprocess(face);
  hb_face_destroy(face);
  return new_face;
}

/* benchmark for subsetting a font */
static void BM_subset (benchmark::State &state,
                       operation_t operation,
                       const test_input_t &test_input,
                       bool hinting)
{
  unsigned subset_size = state.range(0);

  hb_face_t *face = nullptr;

  static hb_face_t *cached_face;
  static const char *cached_font_path;

  if (!cached_font_path || strcmp (cached_font_path, test_input.font_path))
  {
    hb_blob_t *blob = hb_blob_create_from_file_or_fail (test_input.font_path);
    assert (blob);
    face = hb_face_create (blob, 0);
    hb_blob_destroy (blob);

    face = preprocess_face (face);

    if (cached_face)
      hb_face_destroy (cached_face);

    cached_face = hb_face_reference (face);
    cached_font_path = test_input.font_path;
  }
  else
    face = hb_face_reference (cached_face);

  hb_subset_input_t* input = hb_subset_input_create_or_fail ();
  assert (input);

  if (!hinting)
    hb_subset_input_set_flags (input, HB_SUBSET_FLAGS_NO_HINTING);

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
                         bool hinting,
                         benchmark::TimeUnit time_unit,
                         const test_input_t &test_input)
{
  if (op == instance && test_input.instance_opts == nullptr)
    return;

  char name[1024] = "BM_subset/";
  strcat (name, op_name);
  strcat (name, "/");
  const char *p = strrchr (test_input.font_path, '/');
  strcat (name, p ? p + 1 : test_input.font_path);
  if (!hinting)
    strcat (name, "/nohinting");

  benchmark::RegisterBenchmark (name, BM_subset, op, test_input, hinting)
      ->Range(10, test_input.max_subset_size)
      ->Unit(time_unit);
}

static void test_operation (operation_t op,
                            const char *op_name,
                            const test_input_t *tests,
                            unsigned num_tests,
                            benchmark::TimeUnit time_unit)
{
  for (unsigned i = 0; i < num_tests; i++)
  {
    auto& test_input = tests[i];
    test_subset (op, op_name, true, time_unit, test_input);
    test_subset (op, op_name, false, time_unit, test_input);
  }
}

int main(int argc, char** argv)
{
  benchmark::Initialize(&argc, argv);

  if (argc > 1)
  {
    num_tests = (argc - 1) / 2;
    tests = (test_input_t *) calloc (num_tests, sizeof (test_input_t));
    for (unsigned i = 0; i < num_tests; i++)
    {
      tests[i].font_path = argv[1 + i * 2];
      tests[i].max_subset_size = atoi (argv[2 + i * 2]);
    }
  }

#define TEST_OPERATION(op, time_unit) test_operation (op, #op, tests, num_tests, time_unit)

  TEST_OPERATION (subset_glyphs, benchmark::kMillisecond);
  TEST_OPERATION (subset_codepoints, benchmark::kMillisecond);
  TEST_OPERATION (instance, benchmark::kMillisecond);

#undef TEST_OPERATION

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  if (tests != default_tests)
    free (tests);
}
