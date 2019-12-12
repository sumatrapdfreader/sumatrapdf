/* To run these regression tests, you need an external PDF file:
   sumatra-search-across-pages-20170615.pdf (SHA1 1922e3a9dcfa5c6341ed23ac3882fc5d3c149e7c)
   https://drive.google.com/file/d/0B2EXZJHDEYllMnkzMUZWWGdueDA/view?usp=sharing
 */

void SearchTestWithDir(const WCHAR* searchFile, const WCHAR* searchTerm, const TextSearchDirection direction,
                       const TextSel* expected, const int expectedLen) {
    EngineBase* engine = EngineManager::CreateEngine(searchFile, nullptr);
    PageTextCache* textCache = new PageTextCache(engine);
    TextSearch* tsrch = new TextSearch(engine, textCache);
    tsrch->SetDirection(direction);
    int findCount = 0;
    int startPage;
    int expIndex, expIncr;
    if (TextSearchDirection::Forward == direction) {
        startPage = 1;
        expIndex = 0;
        expIncr = 1;
    } else {
        startPage = engine->PageCount();
        expIndex = expectedLen - 1;
        expIncr = -1;
    }
    for (auto tsel = tsrch->FindFirst(startPage, searchTerm); nullptr != tsel;
         tsel = tsrch->FindNext(), ++findCount, expIndex += expIncr) {
        if (0 == expected[expIndex].len) {
            wprintf(L"Found %s %i times, not expecting another match\n", searchTerm, expIndex);
            CrashIf(true);
        }
        if (expected[expIndex].len != tsel->len) {
            wprintf(L"Text selection length mismatch for %s at occurence %i: got %i, wanted %i\n", searchTerm,
                    findCount, expected[expIndex].len, tsel->len);
            CrashIf(true);
        }
        for (int i = 0; i < tsel->len; ++i) {
            if ((expected[expIndex].pages[i] != tsel->pages[i]) || (expected[expIndex].rects[i] != tsel->rects[i])) {
                wprintf(
                    L"Text selection page or rectangle mismatch for %s, "
                    L"expected pg %d rx=%d ry=%d rdx=%d rdy=%d "
                    L"got pg %d rx=%d ry=%d rdx=%d rdy=%d\n",
                    searchTerm, expected[expIndex].pages[i], expected[expIndex].rects[i].x,
                    expected[expIndex].rects[i].y, expected[expIndex].rects[i].dx, expected[expIndex].rects[i].dy,
                    tsel->pages[i], tsel->rects[i].x, tsel->rects[i].y, tsel->rects[i].dx, tsel->rects[i].dy);
                CrashIf(true);
            }
        }
    }
    if (TextSearchDirection::Forward == direction) {
        if (findCount != expectedLen) {
            wprintf(L"Found only %d matches of '%s', expected %d\n", expIndex, searchTerm, expectedLen);
            CrashIf(true);
        }
    } else {
        if (findCount != expectedLen) {
            wprintf(L"Found only %d matches of '%s', expected %d\n", expectedLen - expIndex - 1, searchTerm,
                    expectedLen);
            CrashIf(true);
        }
    }
    delete tsrch;
    delete textCache;
}

#include "Regress03.h"

const TextSel* BuildTextSelList(RegressSearchInfo& info) {
    TextSel* result = new TextSel[info.count + 1];
    result[info.count].len = 0;
    result[info.count].pages = (int*)nullptr;
    result[info.count].rects = (RectI*)nullptr;
    auto offs = 0;
    for (auto i = 0; i < info.count; ++i) {
        result[i].len = info.rectCounts[i];
        result[i].pages = &(info.pages[offs]);
        result[i].rects = &(info.rects[offs]);
        offs += info.rectCounts[i];
    }
    return result;
}

void RegressSearch(const WCHAR* filePath, RegressSearchInfo& info) {
    const WCHAR* searchTerm = info.searchPhrase;
    const TextSel* expected = BuildTextSelList(info);
    SearchTestWithDir(filePath, searchTerm, TextSearchDirection::Forward, expected, info.count);
    SearchTestWithDir(filePath, searchTerm, TextSearchDirection::Backward, expected, info.count);
    delete[] expected;
}

void Regress03() {
    WCHAR* filePath = path::Join(TestFilesDir(), L"sumatra-search-across-pages-20170615.pdf");
    VerifyFileExists(filePath);
    // searches with hits that are all located completely in one page
    RegressSearch(filePath, data_suspendisse);
    RegressSearch(filePath, data_fermentum_ultricies);
    RegressSearch(filePath, data_spendis);
    RegressSearch(filePath, data_euismod_ac);
    RegressSearch(filePath, data_s);
    RegressSearch(filePath, data_xyzzy);
    RegressSearch(filePath, data_S);
    RegressSearch(filePath, data_ut_efficitur);
    RegressSearch(filePath, data_rhoncus_posuere);
    // searches with hits that start on one page and end on the next
    RegressSearch(filePath, data_sit_amet_massa);
    RegressSearch(filePath, data_morbi_mattis);
    RegressSearch(filePath, data_sem_eu_augue_pellentesque_accumsan);
    RegressSearch(filePath, data_convallis_libero_nibh);
}
