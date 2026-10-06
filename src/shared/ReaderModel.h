/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocumentLayout;
struct DocumentLayoutParams;
class EngineBase;
struct PasswordUI;
struct Pixmap;

struct ReaderModel : NonCopyable {
    ~ReaderModel();

    static ReaderModel* Create(Str path, PasswordUI* pwdUI = nullptr);

    int PageCount() const;
    RectF PageMediabox(int pageNo) const;
    float FileDPI() const;
    bool Layout(const DocumentLayoutParams& params, DocumentLayout* layout) const;
    Pixmap* RenderPageForPrint(int pageNo, float zoom, int rotation) const;
    EngineBase* GetEngine() const;

  private:
    explicit ReaderModel(EngineBase* engine) : engine(engine) {}
    EngineBase* engine;
};
