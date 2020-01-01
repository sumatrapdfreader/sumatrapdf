/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This is assert for unit tests that can be used in non-interactive usage.
Instead of showing a UI to the user, like regular assert(), it simply
remembers number of failed asserts. */

void utassert_func(bool ok, const char* exprStr, const char* file, int lineNo);
int utassert_print_results();

#define utassert(_expr) utassert_func(_expr, #_expr, __FILE__, __LINE__)

#undef assert
#define assert use_utassert_insteadof_assert
