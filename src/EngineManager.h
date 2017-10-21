/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class EngineType {
    None = 0,
    // the EngineManager tries to create a new engine
    // in the following order (types on the same line
    // share common code and reside in the same file)
    PDF,
    XPS,
    DjVu,
    Image,
    ImageDir,
    ComicBook,
    PostScript,
    Epub,
    Fb2,
    Mobi,
    Pdb,
    Chm,
    Html,
    Txt,
};

namespace EngineManager {

bool IsSupportedFile(const WCHAR* filePath, bool sniff = false, bool enableEbookEngines = true);
BaseEngine* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI = nullptr, EngineType* typeOut = nullptr,
                         bool enableChmEngine = true, bool enableEbookEngines = true);
}
