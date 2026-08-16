/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocumentLayout;
struct DocumentLayoutParams;
class EngineBase;
struct Pixmap;

struct ReaderModel {
    EngineBase* engine = nullptr;

    ReaderModel() = default;
    ReaderModel(const ReaderModel&) = delete;
    ReaderModel& operator=(const ReaderModel&) = delete;
    ~ReaderModel();

    static ReaderModel* Create(Str path);

    Str FilePath() const;
    int PageCount() const;
    RectF PageMediabox(int pageNo) const;
    float FileDPI() const;
    bool Layout(const DocumentLayoutParams& params, DocumentLayout* layout) const;
    Pixmap* RenderPage(int pageNo, float zoom, int rotation) const;
    Pixmap* RenderPageForPrint(int pageNo, float zoom, int rotation) const;
    EngineBase* GetEngine() const;
};
