#pragma once

#ifdef ZXING_BUILD_FOR_TEST
#define ZXING_EXPORT_TEST_ONLY
#define ZXING_IF_NOT_TEST(x)
#else
#define ZXING_EXPORT_TEST_ONLY static
#define ZXING_IF_NOT_TEST(x) x
#endif
