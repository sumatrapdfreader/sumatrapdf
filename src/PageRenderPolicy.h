/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class PageRenderPriority {
    Visible,
    Nearby,
    Background,
};

struct PageRenderKey {
    int pageNo = 0;
    float zoom = 0;
    int rotation = 0;

    bool operator==(const PageRenderKey& other) const;
};

struct PageRenderPolicyRequest {
    PageRenderKey key;
    PageRenderPriority priority = PageRenderPriority::Background;
    u32 generation = 0;
    u64 serial = 0;
};

struct PageRenderPolicyCacheEntry {
    i64 bytes = 0;
    u64 lastUse = 0;
};

void PageRenderPolicyUpsert(Vec<PageRenderPolicyRequest>& requests, const PageRenderPolicyRequest& request);
void PageRenderPolicyDropStale(Vec<PageRenderPolicyRequest>& requests, u32 generation);
int PageRenderPolicyPickRequest(const Vec<PageRenderPolicyRequest>& requests);
int PageRenderPolicyPickEviction(const Vec<PageRenderPolicyCacheEntry>& entries, int protectedIndex);

#if IS_DEBUG
void PageRenderPolicy_UnitTests();
#endif
