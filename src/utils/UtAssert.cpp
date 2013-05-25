/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "UtAssert.h"

static int g_total_asserts = 0;
static int g_failed_asserts = 0;

// TODO: a way to return failed messages or print them to a descriptor

#define MAX_FAILED_MSGS 32

static const char *g_failed_msgs[MAX_FAILED_MSGS];
static int g_nfailed_msgs = 0;

void utassert_func(bool ok, const char *expr_str)
{
    ++g_total_asserts;
    if (ok)
        return;
    ++g_failed_asserts;
    if (g_nfailed_msgs < MAX_FAILED_MSGS) {
        g_failed_msgs[g_nfailed_msgs] = expr_str;
        ++g_nfailed_msgs;
    }
}

void utassert_get_stats(int *total_asserts, int *failed_asserts)
{
    if (total_asserts)
        *total_asserts = g_total_asserts;
    if (failed_asserts)
        *failed_asserts = g_failed_asserts;
}
