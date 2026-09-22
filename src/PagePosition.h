/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct DocController;
struct Location;
struct FileState;

// marks a persisted FileState/TabState PageNo string as an engine bookmark
// instead of a flat page number
constexpr Str kBookmarkPrefix = StrL("bm:");

// parsed form of a persisted PageNo string
struct StoredPagePos {
    int pageNo = 1;
    Str bookmark; // view into the parsed input; set only for "bm:..."
};

StoredPagePos ParseStoredPagePos(Str s);
TempStr FormatStoredPagePosTemp(int pageNo);
TempStr FormatStoredBookmarkTemp(Str bookmark);

TempStr StoredPagePosFromCtrlTemp(DocController* ctrl);
TempStr StoredPagePosForPageTemp(DocController* ctrl, int pageNo);
int PageNoFromStoredPagePos(DocController* ctrl, Str stored);

Location BookmarkLocationHint(Str bookmark);

Location LocationFromFlatPageNo(DocController* ctrl, int flatPageNo);
bool MigrateStoredPagePos(DocController* ctrl, Str* pageNoStr);
bool MigrateFileStatePagePos(DocController* ctrl, FileState* fs);
TempStr FormatFileStateProgressTemp(const FileState* fs);

// toolbar, Go to Page, page info: chapter and page, instead of one flat page number
bool ShowChapterUi(DocController* ctrl);
