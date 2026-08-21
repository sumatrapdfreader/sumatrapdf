/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "PageRenderPolicy.h"

bool PageRenderKey::operator==(const PageRenderKey& other) const {
    return pageNo == other.pageNo && zoom == other.zoom && rotation == other.rotation;
}

// Keep at most one queued request per page and generation. A newer viewport
// request replaces obsolete render parameters and can raise its priority.
void PageRenderPolicyUpsert(Vec<PageRenderPolicyRequest>& requests, const PageRenderPolicyRequest& request) {
    for (int i = 0; i < len(requests); i++) {
        PageRenderPolicyRequest& existing = requests[i];
        if (existing.generation != request.generation || existing.key.pageNo != request.key.pageNo) {
            continue;
        }
        existing = request;
        return;
    }
    requests.Append(request);
}

void PageRenderPolicyDropStale(Vec<PageRenderPolicyRequest>& requests, u32 generation) {
    for (int i = len(requests) - 1; i >= 0; i--) {
        if (requests[i].generation != generation) {
            requests.RemoveAt(i);
        }
    }
}

// Lower enum values are more urgent. Requests within the same priority remain
// FIFO so fast scrolling does not permanently starve an older visible page.
int PageRenderPolicyPickRequest(const Vec<PageRenderPolicyRequest>& requests) {
    int best = -1;
    for (int i = 0; i < len(requests); i++) {
        if (best < 0 || requests[i].priority < requests[best].priority ||
            (requests[i].priority == requests[best].priority && requests[i].serial < requests[best].serial)) {
            best = i;
        }
    }
    return best;
}

int PageRenderPolicyPickEviction(const Vec<PageRenderPolicyCacheEntry>& entries, int protectedIndex) {
    int oldest = -1;
    for (int i = 0; i < len(entries); i++) {
        if (i == protectedIndex) {
            continue;
        }
        if (oldest < 0 || entries[i].lastUse < entries[oldest].lastUse) {
            oldest = i;
        }
    }
    return oldest;
}

#if defined(DEBUG)

// must be last to over-write assert()
#include "base/UtAssert.h"

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
    cache.Append({10, 20});
    cache.Append({10, 5});
    cache.Append({10, 12});
    utassert(PageRenderPolicyPickEviction(cache, 0) == 1);
    utassert(PageRenderPolicyPickEviction(cache, 1) == 2);
}

#endif
