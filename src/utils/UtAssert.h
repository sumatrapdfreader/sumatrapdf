/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef UtAssert_h
#define UtAssert_h

/* This is assert for unit tests that can be used in non-interactive usage.
Instead of showing a UI to the user, like regular assert(), it simply
remembers number of failed asserts. */

// TODO: add file name/line number
void utassert_func(bool ok, const char *expr_str);
void utassert_get_stats(int *total_asserts, int *failed_asserts);

#define utassert(_expr) \
    utassert_func(_expr, #_expr)

// TODO: temporary. Unit tests should use utassert explicitly
#undef assert
#define assert utassert

#endif
