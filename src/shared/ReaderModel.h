/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocumentLayout;
struct DocumentLayoutParams;
class EngineBase;
struct PasswordUI;

struct ReaderModel : NonCopyable {
    ~ReaderModel();

    static ReaderModel* Create(Str path, PasswordUI* pwdUI = nullptr);

    int PageCount() const;
    bool Layout(const DocumentLayoutParams& params, DocumentLayout* layout) const;
    EngineBase* GetEngine() const;

  private:
    explicit ReaderModel(EngineBase* engine) : engine(engine) {}
    EngineBase* engine;
};
