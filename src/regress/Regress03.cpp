/* To run these regression tests, you need an external PDF file:
   sumatra-search-across-pages-20170615.pdf (SHA1 1922e3a9dcfa5c6341ed23ac3882fc5d3c149e7c)
   https://drive.google.com/file/d/0B2EXZJHDEYllMnkzMUZWWGdueDA/view?usp=sharing
 */

void SearchTestWithDir(Str searchFile, Str searchTerm, const TextSearch::Direction direction,
                       const Vec<TextSel>* expected, const int expectedLen) {
    EngineBase* engine = CreateEngineFromFile(searchFile, nullptr, true);
    TextSearch* tsrch = new TextSearch(engine);
    tsrch->SetDirection(direction);
    int findCount = 0;
    int startPage;
    int expIndex, expIncr;
    if (TextSearch::Direction::Forward == direction) {
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
        if (0 == len(expected[expIndex])) {
            printf("Found %.*s %i times, not expecting another match\n", searchTerm.len, searchTerm.s, expIndex);
            ReportIf(true);
        }
        if (len(expected[expIndex]) != len(*tsel)) {
            printf("Text selection length mismatch for %.*s at occurrence %i: got %i, wanted %i\n", searchTerm.len,
                   searchTerm.s, findCount, len(expected[expIndex]), len(*tsel));
            ReportIf(true);
        }
        for (int i = 0; i < len(*tsel); ++i) {
            if ((expected[expIndex][i].pageNo != (*tsel)[i].pageNo) ||
                (expected[expIndex][i].rect != (*tsel)[i].rect)) {
                printf(
                    "Text selection page or rectangle mismatch for %.*s, "
                    "expected pg %d rx=%d ry=%d rdx=%d rdy=%d "
                    "got pg %d rx=%d ry=%d rdx=%d rdy=%d\n",
                    searchTerm.len, searchTerm.s, expected[expIndex][i].pageNo, expected[expIndex][i].rect.x,
                    expected[expIndex][i].rect.y, expected[expIndex][i].rect.dx, expected[expIndex][i].rect.dy,
                    (*tsel)[i].pageNo, (*tsel)[i].rect.x, (*tsel)[i].rect.y, (*tsel)[i].rect.dx, (*tsel)[i].rect.dy);
                ReportIf(true);
            }
        }
    }
    if (TextSearch::Direction::Forward == direction) {
        if (findCount != expectedLen) {
            printf("Found only %d matches of '%.*s', expected %d\n", expIndex, searchTerm.len, searchTerm.s,
                   expectedLen);
            ReportIf(true);
        }
    } else {
        if (findCount != expectedLen) {
            printf("Found only %d matches of '%.*s', expected %d\n", expectedLen - expIndex - 1, searchTerm.len,
                   searchTerm.s, expectedLen);
            ReportIf(true);
        }
    }
    delete tsrch;
}

#include "Regress03.h"

const Vec<TextSel>* BuildTextSelList(RegressSearchInfo& info) {
    Vec<TextSel>* result = new Vec<TextSel>[info.count + 1];
    auto offs = 0;
    for (auto i = 0; i < info.count; ++i) {
        for (int j = 0; j < info.rectCounts[i]; j++) {
            VecAppend(result[i], TextSel{info.pages[offs + j], info.rects[offs + j], {}});
        }
        offs += info.rectCounts[i];
    }
    return result;
}

void RegressSearch(Str filePath, RegressSearchInfo& info) {
    TempStr searchTerm = ToUtf8Temp(info.searchPhrase);
    const Vec<TextSel>* expected = BuildTextSelList(info);
    SearchTestWithDir(filePath, searchTerm, TextSearch::Direction::Forward, expected, info.count);
    SearchTestWithDir(filePath, searchTerm, TextSearch::Direction::Backward, expected, info.count);
    delete[] expected;
}

void Regress03() {
    TempStr filePath = path::JoinTemp(TestFilesDir(), StrL("sumatra-search-across-pages-20170615.pdf"));
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
