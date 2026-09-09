/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PageRenderPolicy.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

void PageRenderPolicy_UnitTests() {
    Vec<PageRenderPolicyRequest> requests;
    PageRenderPolicyUpsert(requests, {{1, 1.0f, 0}, PageRenderPriority::Background, 4, 1});
    PageRenderPolicyUpsert(requests, {{2, 1.0f, 0}, PageRenderPriority::Nearby, 4, 2});
    PageRenderPolicyUpsert(requests, {{3, 1.0f, 0}, PageRenderPriority::Visible, 4, 3});
    utassert(PageRenderPolicyPickRequest(requests) == 2);

    PageRenderPolicyUpsert(requests, {{1, 2.0f, 90}, PageRenderPriority::Visible, 4, 4});
    utassert(len(requests) == 3);
    PageRenderKey replacement{1, 2.0f, 90};
    utassert(requests[0].key == replacement);
    utassert(requests[0].priority == PageRenderPriority::Visible);

    PageRenderPolicyUpsert(requests, {{4, 1.0f, 0}, PageRenderPriority::Visible, 3, 5});
    PageRenderPolicyDropStale(requests, 4);
    utassert(len(requests) == 3);
    for (const PageRenderPolicyRequest& request : requests) {
        utassert(request.generation == 4);
    }

    Vec<PageRenderPolicyCacheEntry> cache;
    VecAppend(cache, {10, 20});
    VecAppend(cache, {10, 5});
    VecAppend(cache, {10, 12});
    utassert(PageRenderPolicyPickEviction(cache, 0) == 1);
    utassert(PageRenderPolicyPickEviction(cache, 1) == 2);
}
