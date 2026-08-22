/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void utassert_func(bool ok, Str exprStr, Str file, int lineNo);
int utassert_print_results();
void utassert_set_for_ai(bool enabled);

#define utassert(_expr) utassert_func(_expr, StrL(#_expr), StrL(__FILE__), __LINE__)

#undef assert
#define assert use_utassert_insteadof_assert
