/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

class EngineBase;
struct Gfx;
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
    bool DrawPage(Gfx* gfx, PageRenderKey key, const Rect& target);
    i64 CacheBytes() const;
};
