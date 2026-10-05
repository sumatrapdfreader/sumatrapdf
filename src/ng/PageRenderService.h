/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct PageRenderKey;
enum class PageRenderPriority;

struct PageRenderService {
    void* data = nullptr;

    PageRenderService() = default;
    PageRenderService(const PageRenderService&) = delete;
    PageRenderService& operator=(const PageRenderService&) = delete;
    ~PageRenderService();

    static PageRenderService* Create(EngineBase* engine, const Func0& onPageReady, i64 maxBytes = 96 * 1024 * 1024);

    void NewGeneration();
    void Request(PageRenderKey key, PageRenderPriority priority);
    Pixmap* CopyPage(PageRenderKey key);
    // ng: orig also has DrawPage(Gfx*, ...); the gpui canvas draws the Pixmap
    // itself (step 7), so there is no Gfx abstraction here
    i64 CacheBytes() const;
};
