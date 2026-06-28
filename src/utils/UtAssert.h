/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This is assert for unit tests that can be used in non-interactive usage.
Instead of showing a UI to the user, like regular assert(), it simply
remembers number of failed asserts. */

void utassert_func(bool ok, Str exprStr, Str file, int lineNo);
int utassert_print_results();
void utassert_set_for_ai(bool enabled);

#define utassert(_expr) utassert_func(_expr, #_expr, __FILE__, __LINE__)

#undef assert
#define assert use_utassert_insteadof_assert
