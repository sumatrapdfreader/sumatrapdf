/*
 * Benchmarks for hb_set_t operations.
 */
#include "benchmark/benchmark.h"

#include "hb-ot.h"

static void BM_hb_ot_tags_from_script_and_language (benchmark::State& state,
						    hb_script_t script,
						    const char *language_str) {

  hb_language_t language = hb_language_from_string (language_str, -1);

  for (auto _ : state)
  {
    hb_tag_t script_tags[HB_OT_MAX_TAGS_PER_SCRIPT];
    unsigned script_count = HB_OT_MAX_TAGS_PER_SCRIPT;

    hb_tag_t language_tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
    unsigned language_count = HB_OT_MAX_TAGS_PER_LANGUAGE;

    hb_ot_tags_from_script_and_language (script,
					 language,
					 &script_count /* IN/OUT */,
					 script_tags /* OUT */,
					 &language_count /* IN/OUT */,
					 language_tags /* OUT */);
  }
}
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON zh_abcd, HB_SCRIPT_COMMON, "zh_abcd");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON zh_hans, HB_SCRIPT_COMMON, "zh_hans");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON ab_abcd, HB_SCRIPT_COMMON, "ab_abcd");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON ab_abc, HB_SCRIPT_COMMON, "ab_abc");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON abcdef_XY, HB_SCRIPT_COMMON, "abcdef_XY");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON abcd_XY, HB_SCRIPT_COMMON, "abcd_XY");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON cxy_CN, HB_SCRIPT_COMMON, "cxy_CN");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON exy_CN, HB_SCRIPT_COMMON, "exy_CN");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON zh_CN, HB_SCRIPT_COMMON, "zh_CN");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON en_US, HB_SCRIPT_COMMON, "en_US");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, LATIN en_US, HB_SCRIPT_LATIN, "en_US");
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, COMMON none, HB_SCRIPT_LATIN, nullptr);
BENCHMARK_CAPTURE (BM_hb_ot_tags_from_script_and_language, LATIN none, HB_SCRIPT_LATIN, nullptr);

BENCHMARK_MAIN();
