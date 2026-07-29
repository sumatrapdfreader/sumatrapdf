/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/GdiPlus.h"
#include "base/Http.h"
#include "base/JsonParser.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "SumatraDialogs.h"
#include "Theme.h"
#include "Translations.h"
#include "FileHistory.h"
#include "Toolbar.h"
#include "HomePage.h"
#include "ImageReader.h"
#include "LibraryPage.h"

constexpr int kMaxBooks = 4096;
constexpr int kMaxSeries = 256;
constexpr int kMaxPeople = 60;
constexpr int kMaxScreen = 8;
constexpr int kMaxFamily = 24;
constexpr int kMaxCovers = 400;
constexpr int kCoverWorkers = 3;

constexpr const char* kLinkSeries = "<Library,Series>";
constexpr const char* kLinkBook = "<Library,Book>";
constexpr const char* kLinkBack = "<Library,Back>";
constexpr const char* kLinkRead = "<Library,Read>";
constexpr const char* kLinkTab = "<Library,Tab>";
constexpr const char* kLinkPerson = "<Library,Person>";
constexpr const char* kLinkRescan = "<Library,Rescan>";
constexpr const char* kLinkClassic = "<Library,Classic>";
constexpr const char* kLinkAllBooks = "<Library,All>";
constexpr const char* kLinkOpen = "<Library,Open>";
constexpr const char* kLinkPage = "<Library,Page>";
constexpr const char* kLinkChapter = "<Library,Chapter>";
constexpr const char* kLinkTopic = "<Library,Topic>";
constexpr const char* kLinkTopicList = "<Library,Topics>";
constexpr const char* kLinkScreenTitle = "<Library,Imdb>";
constexpr const char* kLinkSort = "<Library,Sort>";

constexpr int kMaxChapters = 512;
constexpr int kMaxKnowers = 40;

constexpr int kMenuOpenResume = 1;
constexpr int kMenuOpenStart = 2;
constexpr int kMenuPlayAudiobook = 3;
constexpr int kMenuNewPartition = 4;
constexpr int kMenuTakeOutOfPartition = 5;
constexpr int kMenuRenamePartition = 6;
constexpr int kMenuDeletePartition = 7;
constexpr int kMenuPartitionFirst = 100;
constexpr int kMaxPartitions = 64;

enum class LibTab {
    Overview,
    People,
    Family,
    Places,
    Knows,
    Screen
};

struct LibBook {
    Str id;
    Str title;
    Str author;
    Str series;
    Str keys;
    Str path;
    Str ext;
    Str wiki;
    int pages = 0;
    int year = 0;
    int volume = 0;
    bool booknlp = false;
    bool cover = false;
};

struct LibSeries {
    Str key;
    Str name;
    Str author;
    Str parent;
    Str wiki;
    Str genre;
    Str sub;
    Str head;
    Str subhead;
    Str kind;
    Str guessed;
    int books = 0;
    int booknlp = 0;
    int facts = 0;
    int depth = 0;
};

struct LibPartition {
    Str key;
    Str name;
    Str parent;
    int books = 0;
    int depth = 0;
};

struct LibChapter {
    Str title;
    int page = 0;
    int depth = 0;
    int parent = -1;
    int kids = 0;
    bool open = false;
};

struct LibKnower {
    Str name;
    Str book;
    int page = 0;
    int mentions = 0;
};

struct LibScreen {
    Str title;
    Str kind;
    Str poster;
    Str stars;
    Str via;
    Str imdbId;
    int year = 0;
};

struct LibFamilyRow {
    Str relation;
    Str name;
    int idx = 0;
    int mentions = 0;
};

struct LibModel {
    LibBook books[kMaxBooks];
    int nBooks = 0;
    LibSeries series[kMaxSeries];
    int nSeries = 0;
    Str filter;
    Str filterName;
    Str error;
    bool loaded = false;
    bool loading = false;
    bool scanning = false;
    int scanDone = 0;
    int scanTotal = 0;
    int total = 0;
};

struct LibDetail {
    Str id;
    Str title;
    Str author;
    Str series;
    Str wiki;
    Str path;
    Str description;
    Str subjects;
    Str person;
    Str personTraits;
    Str personNot;
    Str personKin;
    Str personSpeech;
    Str personVoice;
    Str personPlaces;
    Str personKnows;
    Str personQuote;
    Str personQuoteBook;
    int personQuotePage = 0;
    int personBooks = 0;
    bool personLoaded = false;
    int year = 0;
    int pages = 0;
    bool booknlp = false;
    bool loading = false;
    bool screenLoading = false;
    bool screenDone = false;
    LibTab tab = LibTab::Overview;
    Str people[kMaxPeople];
    int nPeople = 0;
    Str places[kMaxPeople];
    int nPlaces = 0;
    Str topics[kMaxPeople];
    int nTopics = 0;
    LibFamilyRow family[kMaxFamily];
    int nFamily = 0;
    LibScreen screen[kMaxScreen];
    int nScreen = 0;
    LibChapter chapters[kMaxChapters];
    int nChapters = 0;
    bool chaptersLoading = false;
    bool chaptersDone = false;
    Str topic;
    LibKnower knowers[kMaxKnowers];
    int nKnowers = 0;
    bool topicLoaded = false;
};

struct CoverSlot {
    Str key;
    Str bytes;
    RenderedBitmap* bmp = nullptr;
    bool wanted = false;
    bool fetching = false;
    bool failed = false;
    bool decoded = false;
};

static LibModel gModel;
static LibDetail gDetail;
static LibPartition gPartitions[kMaxPartitions];
static int gNPartitions = 0;
static CoverSlot gCovers[kMaxCovers];
static int gNCovers = 0;
static CRITICAL_SECTION gLock;
static bool gLockReady = false;
static int gWorkers = 0;
static HWND gNotifyHwnd = nullptr;
static bool gDetailOpen = false;
static int gPort = 0;

static void EnterLib() {
    if (!gLockReady) {
        InitializeCriticalSection(&gLock);
        gLockReady = true;
    }
    EnterCriticalSection(&gLock);
}

static void LeaveLib() {
    LeaveCriticalSection(&gLock);
}

bool LibraryHomeEnabled() {
    return gGlobalPrefs && gGlobalPrefs->audiobook.libraryHome;
}

void SetLibraryHomeEnabled(bool enabled) {
    if (!gGlobalPrefs) {
        return;
    }
    gGlobalPrefs->audiobook.libraryHome = enabled;
}

bool LibraryHasBooks() {
    return gModel.loaded && gModel.nBooks > 0;
}

int LibraryServicePort() {
    if (gPort > 0) {
        return gPort;
    }
    int p = gGlobalPrefs ? gGlobalPrefs->audiobook.libraryPort : 0;
    gPort = (p > 0) ? p : kLibraryServicePortDefault;
    return gPort;
}

static void Repaint() {
    if (gNotifyHwnd && IsWindow(gNotifyHwnd)) {
        InvalidateRect(gNotifyHwnd, nullptr, FALSE);
    }
}

static TempStr UrlEncodeTemp(Str s) {
    str::Builder b;
    for (int i = 0; i < len(s); i++) {
        u8 c = (u8)s.s[i];
        bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                    c == '_' || c == '.' || c == '~';
        if (safe) {
            char one[2] = {(char)c, 0};
            b.Append(Str(one, 1));
        } else {
            b.Append(fmt("%%%02X", (int)c));
        }
    }
    return str::DupTemp(ToStr(b));
}

static bool ServiceGet(Str path, HttpRsp* rsp) {
    TempStr url = fmt("http://127.0.0.1:%d%s", LibraryServicePort(), path);
    if (!HttpGet(Str(url), rsp)) {
        return false;
    }
    return IsHttpRspOk(rsp);
}

static TempStr ServiceGetTextTemp(Str path) {
    HttpRsp rsp;
    if (!ServiceGet(path, &rsp)) {
        return nullptr;
    }
    return str::DupTemp(ToStr(rsp.data));
}

static bool ServicePost(const char* path, Str body) {
    str::Builder hdrs;
    hdrs.Append("Content-Type: application/json\r\n");
    str::Builder b;
    if (len(body) > 0) {
        b.Append(body);
    }
    return HttpPost(StrL("127.0.0.1"), LibraryServicePort(), Str(path), &hdrs, &b);
}

static int IndexIn(Str path, const char* prefix) {
    if (!str::StartsWith(path, Str(prefix))) {
        return -1;
    }
    const char* s = path.s + strlen(prefix);
    if (*s != '[') {
        return -1;
    }
    return atoi(s + 1);
}

static bool IsTrue(Str v) {
    return str::Eq(v, StrL("true"));
}

struct LibraryParser : json::ValueVisitor {
    LibModel* m;

    explicit LibraryParser(LibModel* model) : m(model) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        int bi = IndexIn(path, "/books");
        if (bi >= 0 && bi < kMaxBooks) {
            if (bi + 1 > m->nBooks) {
                m->nBooks = bi + 1;
            }
            LibBook& b = m->books[bi];
            if (str::EndsWith(path, StrL("/id"))) {
                str::ReplaceWithCopy(&b.id, value);
            } else if (str::EndsWith(path, StrL("/title"))) {
                str::ReplaceWithCopy(&b.title, value);
            } else if (str::EndsWith(path, StrL("/author"))) {
                str::ReplaceWithCopy(&b.author, value);
            } else if (str::EndsWith(path, StrL("/series"))) {
                str::ReplaceWithCopy(&b.series, value);
            } else if (str::EndsWith(path, StrL("/series_keys"))) {
                str::ReplaceWithCopy(&b.keys, value);
            } else if (str::EndsWith(path, StrL("/path"))) {
                str::ReplaceWithCopy(&b.path, value);
            } else if (str::EndsWith(path, StrL("/ext"))) {
                str::ReplaceWithCopy(&b.ext, value);
            } else if (str::EndsWith(path, StrL("/wiki"))) {
                str::ReplaceWithCopy(&b.wiki, value);
            } else if (str::EndsWith(path, StrL("/pages"))) {
                b.pages = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/year"))) {
                b.year = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/booknlp"))) {
                b.booknlp = IsTrue(value);
            } else if (str::EndsWith(path, StrL("/cover"))) {
                b.cover = IsTrue(value);
            } else if (IndexIn(path, "/books") >= 0 && str::Contains(path, "/volumes[0]")) {
                b.volume = atoi(value.s);
            }
            return true;
        }
        int si = IndexIn(path, "/series");
        if (si >= 0 && si < kMaxSeries) {
            if (si + 1 > m->nSeries) {
                m->nSeries = si + 1;
            }
            LibSeries& s = m->series[si];
            if (str::EndsWith(path, StrL("/name"))) {
                str::ReplaceWithCopy(&s.name, value);
            } else if (str::EndsWith(path, StrL("/key"))) {
                str::ReplaceWithCopy(&s.key, value);
            } else if (str::EndsWith(path, StrL("/depth"))) {
                s.depth = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/author"))) {
                str::ReplaceWithCopy(&s.author, value);
            } else if (str::EndsWith(path, StrL("/parent"))) {
                str::ReplaceWithCopy(&s.parent, value);
            } else if (str::EndsWith(path, StrL("/wiki"))) {
                str::ReplaceWithCopy(&s.wiki, value);
            } else if (str::EndsWith(path, StrL("/genre"))) {
                str::ReplaceWithCopy(&s.genre, value);
            } else if (str::EndsWith(path, StrL("/subgenre"))) {
                str::ReplaceWithCopy(&s.sub, value);
            } else if (str::EndsWith(path, StrL("/head"))) {
                str::ReplaceWithCopy(&s.head, value);
            } else if (str::EndsWith(path, StrL("/subhead"))) {
                str::ReplaceWithCopy(&s.subhead, value);
            } else if (str::EndsWith(path, StrL("/kind"))) {
                str::ReplaceWithCopy(&s.kind, value);
            } else if (str::EndsWith(path, StrL("/guessed"))) {
                str::ReplaceWithCopy(&s.guessed, value);
            } else if (str::EndsWith(path, StrL("/books"))) {
                s.books = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/booknlp"))) {
                s.booknlp = atoi(value.s);
            } else if (str::EndsWith(path, StrL("/facts"))) {
                s.facts = atoi(value.s);
            }
            return true;
        }
        if (str::Eq(path, StrL("/total"))) {
            m->total = atoi(value.s);
        } else if (str::Eq(path, StrL("/status/scanning"))) {
            m->scanning = IsTrue(value);
        } else if (str::Eq(path, StrL("/status/scan_done"))) {
            m->scanDone = atoi(value.s);
        } else if (str::Eq(path, StrL("/status/scan_total"))) {
            m->scanTotal = atoi(value.s);
        }
        return true;
    }
};

struct PartitionParser : json::ValueVisitor {
    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        int i = IndexIn(path, "/partitions");
        if (i < 0 || i >= kMaxPartitions) {
            return true;
        }
        if (i + 1 > gNPartitions) {
            gNPartitions = i + 1;
        }
        LibPartition& p = gPartitions[i];
        if (str::EndsWith(path, StrL("/key"))) {
            str::ReplaceWithCopy(&p.key, value);
        } else if (str::EndsWith(path, StrL("/name"))) {
            str::ReplaceWithCopy(&p.name, value);
        } else if (str::EndsWith(path, StrL("/parent"))) {
            str::ReplaceWithCopy(&p.parent, value);
        } else if (str::EndsWith(path, StrL("/books"))) {
            p.books = atoi(value.s);
        }
        return true;
    }
};

static void FreePartitions() {
    for (int i = 0; i < gNPartitions; i++) {
        str::Free(gPartitions[i].key);
        str::Free(gPartitions[i].name);
        str::Free(gPartitions[i].parent);
        gPartitions[i] = LibPartition{};
    }
    gNPartitions = 0;
}

static void RankPartitions() {
    for (int i = 0; i < gNPartitions; i++) {
        LibPartition& p = gPartitions[i];
        p.depth = 0;
        Str up = p.parent;
        for (int step = 0; step < 8 && len(up) > 0; step++) {
            bool found = false;
            for (int j = 0; j < gNPartitions; j++) {
                if (str::Eq(gPartitions[j].key, up)) {
                    p.depth++;
                    up = gPartitions[j].parent;
                    found = true;
                    break;
                }
            }
            if (!found) {
                break;
            }
        }
    }
}

static LibPartition* PartitionByKey(Str key) {
    if (len(key) == 0) {
        return nullptr;
    }
    for (int i = 0; i < gNPartitions; i++) {
        if (str::Eq(gPartitions[i].key, key)) {
            return &gPartitions[i];
        }
    }
    return nullptr;
}

struct DetailParser : json::ValueVisitor {
    LibDetail* d;

    explicit DetailParser(LibDetail* det) : d(det) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        if (str::Eq(path, StrL("/title"))) {
            str::ReplaceWithCopy(&d->title, value);
        } else if (str::Eq(path, StrL("/author"))) {
            str::ReplaceWithCopy(&d->author, value);
        } else if (str::Eq(path, StrL("/series"))) {
            str::ReplaceWithCopy(&d->series, value);
        } else if (str::Eq(path, StrL("/wiki"))) {
            str::ReplaceWithCopy(&d->wiki, value);
        } else if (str::Eq(path, StrL("/path"))) {
            str::ReplaceWithCopy(&d->path, value);
        } else if (str::Eq(path, StrL("/description"))) {
            str::ReplaceWithCopy(&d->description, value);
        } else if (str::Eq(path, StrL("/year"))) {
            d->year = atoi(value.s);
        } else if (str::Eq(path, StrL("/pages"))) {
            d->pages = atoi(value.s);
        } else if (str::Eq(path, StrL("/booknlp"))) {
            d->booknlp = IsTrue(value);
        } else if (IndexIn(path, "/subjects") >= 0) {
            str::Builder b;
            if (len(d->subjects) > 0) {
                b.Append(d->subjects);
                b.Append(" \xc2\xb7 ");
            }
            b.Append(value);
            str::ReplaceWithCopy(&d->subjects, ToStr(b));
        } else {
            int pi = IndexIn(path, "/wiki_summary/people");
            if (pi >= 0 && pi < kMaxPeople) {
                if (pi + 1 > d->nPeople) {
                    d->nPeople = pi + 1;
                }
                str::ReplaceWithCopy(&d->people[pi], value);
                return true;
            }
            int li = IndexIn(path, "/wiki_summary/places");
            if (li >= 0 && li < kMaxPeople) {
                if (li + 1 > d->nPlaces) {
                    d->nPlaces = li + 1;
                }
                str::ReplaceWithCopy(&d->places[li], value);
                return true;
            }
            int ti = IndexIn(path, "/wiki_summary/topics");
            if (ti >= 0 && ti < kMaxPeople) {
                if (ti + 1 > d->nTopics) {
                    d->nTopics = ti + 1;
                }
                str::ReplaceWithCopy(&d->topics[ti], value);
                return true;
            }
        }
        return true;
    }
};

struct ScreenParser : json::ValueVisitor {
    LibDetail* d;

    explicit ScreenParser(LibDetail* det) : d(det) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        int i = IndexIn(path, "/screen");
        if (i < 0 || i >= kMaxScreen) {
            return true;
        }
        if (i + 1 > d->nScreen) {
            d->nScreen = i + 1;
        }
        LibScreen& s = d->screen[i];
        if (str::EndsWith(path, StrL("/title"))) {
            str::ReplaceWithCopy(&s.title, value);
        } else if (str::EndsWith(path, StrL("/kind"))) {
            str::ReplaceWithCopy(&s.kind, value);
        } else if (str::EndsWith(path, StrL("/poster"))) {
            str::ReplaceWithCopy(&s.poster, value);
        } else if (str::EndsWith(path, StrL("/imdb_id"))) {
            str::ReplaceWithCopy(&s.imdbId, value);
        } else if (str::EndsWith(path, StrL("/via"))) {
            str::ReplaceWithCopy(&s.via, value);
        } else if (str::EndsWith(path, StrL("/year"))) {
            s.year = atoi(value.s);
        } else if (str::Contains(path, "/stars[")) {
            str::Builder b;
            if (len(s.stars) > 0) {
                b.Append(s.stars);
                b.Append(", ");
            }
            b.Append(value);
            str::ReplaceWithCopy(&s.stars, ToStr(b));
        }
        return true;
    }
};

struct FamilyParser : json::ValueVisitor {
    LibDetail* d;
    Str want;

    FamilyParser(LibDetail* det, Str name) : d(det), want(name) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        if (!str::StartsWith(path, StrL("/nodes/"))) {
            return true;
        }
        const char* rest = path.s + 7;
        const char* mark = strstr(rest, "/relations/");
        if (!mark) {
            return true;
        }
        int whoLen = (int)(mark - rest);
        if (!str::EqI(Str(rest, whoLen), want)) {
            return true;
        }
        const char* after = mark + 11;
        const char* bracket = strchr(after, '[');
        if (!bracket) {
            return true;
        }
        if (!str::EndsWith(path, StrL("/name")) && !str::EndsWith(path, StrL("/mentions"))) {
            return true;
        }
        int relLen = (int)(bracket - after);
        int idx = atoi(bracket + 1);
        Str rel(after, relLen);
        int slot = -1;
        for (int i = 0; i < d->nFamily; i++) {
            if (d->family[i].idx == idx && str::Eq(d->family[i].relation, rel)) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            if (d->nFamily >= kMaxFamily) {
                return true;
            }
            slot = d->nFamily++;
            str::ReplaceWithCopy(&d->family[slot].relation, rel);
            d->family[slot].idx = idx;
        }
        if (str::EndsWith(path, StrL("/name"))) {
            str::ReplaceWithCopy(&d->family[slot].name, value);
        } else {
            d->family[slot].mentions = atoi(value.s);
        }
        return true;
    }
};

static int ChapterDepthOf(Str path) {
    int depth = 0;
    const char* s = path.s;
    const char* end = path.s + len(path);
    while (s < end) {
        const char* hit = strstr(s, "/children[");
        if (!hit || hit >= end) {
            break;
        }
        depth++;
        s = hit + 10;
    }
    return depth;
}

struct ChapterParser : json::ValueVisitor {
    LibDetail* d;

    explicit ChapterParser(LibDetail* det) : d(det) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        if (!str::StartsWith(path, StrL("/chapters["))) {
            return true;
        }
        bool isTitle = str::EndsWith(path, StrL("/title"));
        bool isPage = str::EndsWith(path, StrL("/page"));
        if (!isTitle && !isPage) {
            return true;
        }
        if (isTitle) {
            if (d->nChapters >= kMaxChapters) {
                return true;
            }
            LibChapter& c = d->chapters[d->nChapters];
            c.depth = ChapterDepthOf(path);
            c.parent = -1;
            for (int i = d->nChapters - 1; i >= 0; i--) {
                if (d->chapters[i].depth < c.depth) {
                    c.parent = i;
                    d->chapters[i].kids++;
                    break;
                }
            }
            str::ReplaceWithCopy(&c.title, value);
            d->nChapters++;
        } else if (d->nChapters > 0) {
            d->chapters[d->nChapters - 1].page = atoi(value.s);
        }
        return true;
    }
};

struct KnowsParser : json::ValueVisitor {
    LibDetail* d;

    explicit KnowsParser(LibDetail* det) : d(det) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        int i = IndexIn(path, "/people");
        if (i < 0 || i >= kMaxKnowers) {
            return true;
        }
        if (i + 1 > d->nKnowers) {
            d->nKnowers = i + 1;
        }
        LibKnower& k = d->knowers[i];
        if (str::EndsWith(path, StrL("/name"))) {
            str::ReplaceWithCopy(&k.name, value);
        } else if (str::EndsWith(path, StrL("/mentions"))) {
            k.mentions = atoi(value.s);
        } else if (str::Contains(path, "/evidence[0]/")) {
            if (str::EndsWith(path, StrL("/book"))) {
                str::ReplaceWithCopy(&k.book, value);
            } else if (str::EndsWith(path, StrL("/page"))) {
                k.page = atoi(value.s);
            }
        }
        return true;
    }
};

static TempStr PrettyRelationTemp(Str rel) {
    TempStr s = str::DupTemp(rel);
    for (int i = 0; i < len(s); i++) {
        if (s.s[i] == '_') {
            s.s[i] = ' ';
        }
    }
    return s;
}

struct PersonParser : json::ValueVisitor {
    LibDetail* d;
    int nTrait = 0;
    int nNot = 0;
    int nSpeech = 0;
    int nVoice = 0;
    int nPlace = 0;
    int nKnow = 0;
    int nKin = 0;

    PersonParser(LibDetail* det) : d(det) {}

    static void Add(Str* dst, Str value, int* n, int maxN) {
        if (*n >= maxN || len(value) == 0) {
            return;
        }
        if (*n == 0) {
            str::ReplaceWithCopy(dst, value);
        } else {
            str::ReplaceWithCopy(dst, fmt("%s \xc2\xb7 %s", *dst, value));
        }
        *n = *n + 1;
    }

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        if (str::StartsWith(path, StrL("/books["))) {
            d->personBooks++;
            return true;
        }
        if (str::StartsWith(path, StrL("/description/describes[0]/evidence[0]/"))) {
            if (str::EndsWith(path, StrL("/text"))) {
                str::ReplaceWithCopy(&d->personQuote, value);
            } else if (str::EndsWith(path, StrL("/book"))) {
                str::ReplaceWithCopy(&d->personQuoteBook, value);
            } else if (str::EndsWith(path, StrL("/page"))) {
                d->personQuotePage = atoi(value.s);
            }
            return true;
        }
        if (!str::EndsWith(path, StrL("/value"))) {
            return true;
        }
        if (str::StartsWith(path, StrL("/description/describes["))) {
            Add(&d->personTraits, value, &nTrait, 14);
        } else if (str::StartsWith(path, StrL("/description/describes_not["))) {
            Add(&d->personNot, value, &nNot, 8);
        } else if (str::StartsWith(path, StrL("/voice/speaks["))) {
            Add(&d->personSpeech, value, &nSpeech, 12);
        } else if (str::StartsWith(path, StrL("/voice/voice["))) {
            Add(&d->personVoice, value, &nVoice, 8);
        } else if (str::StartsWith(path, StrL("/places/"))) {
            Add(&d->personPlaces, value, &nPlace, 12);
        } else if (str::StartsWith(path, StrL("/knows/knows["))) {
            Add(&d->personKnows, value, &nKnow, 14);
        } else if (str::StartsWith(path, StrL("/family/"))) {
            const char* rel = path.s + 8;
            const char* bracket = strchr(rel, '[');
            if (bracket && atoi(bracket + 1) == 0) {
                TempStr pretty = PrettyRelationTemp(Str(rel, (int)(bracket - rel)));
                Add(&d->personKin, fmt("%s %s", Str(pretty), value), &nKin, 12);
            }
        }
        return true;
    }
};

static void FreeModel(LibModel* m) {
    for (int i = 0; i < m->nBooks; i++) {
        LibBook& b = m->books[i];
        str::Free(b.id);
        str::Free(b.title);
        str::Free(b.author);
        str::Free(b.series);
        str::Free(b.keys);
        str::Free(b.path);
        str::Free(b.ext);
        str::Free(b.wiki);
        b = LibBook{};
    }
    m->nBooks = 0;
    for (int i = 0; i < m->nSeries; i++) {
        LibSeries& s = m->series[i];
        str::Free(s.key);
        str::Free(s.name);
        str::Free(s.author);
        str::Free(s.parent);
        str::Free(s.wiki);
        str::Free(s.genre);
        str::Free(s.sub);
        str::Free(s.head);
        str::Free(s.subhead);
        str::Free(s.kind);
        str::Free(s.guessed);
        s = LibSeries{};
    }
    m->nSeries = 0;
}

static void FreeDetail(LibDetail* d) {
    str::Free(d->id);
    str::Free(d->title);
    str::Free(d->author);
    str::Free(d->series);
    str::Free(d->wiki);
    str::Free(d->path);
    str::Free(d->description);
    str::Free(d->subjects);
    str::Free(d->person);
    str::Free(d->personTraits);
    str::Free(d->personNot);
    str::Free(d->personKin);
    str::Free(d->personSpeech);
    str::Free(d->personVoice);
    str::Free(d->personPlaces);
    str::Free(d->personKnows);
    str::Free(d->personQuote);
    str::Free(d->personQuoteBook);
    for (int i = 0; i < d->nPeople; i++) {
        str::Free(d->people[i]);
    }
    for (int i = 0; i < d->nPlaces; i++) {
        str::Free(d->places[i]);
    }
    for (int i = 0; i < d->nTopics; i++) {
        str::Free(d->topics[i]);
    }
    for (int i = 0; i < d->nFamily; i++) {
        str::Free(d->family[i].relation);
        str::Free(d->family[i].name);
    }
    for (int i = 0; i < d->nScreen; i++) {
        str::Free(d->screen[i].title);
        str::Free(d->screen[i].kind);
        str::Free(d->screen[i].poster);
        str::Free(d->screen[i].stars);
        str::Free(d->screen[i].via);
        str::Free(d->screen[i].imdbId);
    }
    for (int i = 0; i < d->nChapters; i++) {
        str::Free(d->chapters[i].title);
    }
    for (int i = 0; i < d->nKnowers; i++) {
        str::Free(d->knowers[i].name);
        str::Free(d->knowers[i].book);
    }
    str::Free(d->topic);
    LibTab keepTab = d->tab;
    *d = LibDetail{};
    d->tab = keepTab;
}

struct LibJob {
    Str a;
    Str b;
};

static LibJob* NewJob(Str a, Str b = {}) {
    auto j = new LibJob();
    j->a = str::Dup(a);
    j->b = str::Dup(b);
    return j;
}

static void FreeJob(LibJob* j) {
    str::Free(j->a);
    str::Free(j->b);
    delete j;
}

static Str LibrarySortOrder() {
    Str how = gGlobalPrefs ? gGlobalPrefs->audiobook.librarySort : Str{};
    if (len(how) == 0) {
        return StrL("alpha");
    }
    return how;
}

static void LoadModelThread(LibJob* job) {
    FreeJob(job);
    LibraryEnsureService();
    str::Builder path;
    path.Append("/library?limit=4096&sort=");
    path.Append(UrlEncodeTemp(LibrarySortOrder()));
    TempStr body = ServiceGetTextTemp(ToStr(path));
    TempStr parts = ServiceGetTextTemp("/partitions");
    EnterLib();
    FreePartitions();
    if (len(parts) > 0) {
        PartitionParser p;
        json::Parse(Str(parts), &p);
        RankPartitions();
    }
    FreeModel(&gModel);
    if (len(body) > 0) {
        LibraryParser p(&gModel);
        json::Parse(Str(body), &p);
        gModel.loaded = true;
        str::FreePtr(&gModel.error);
    } else {
        str::ReplaceWithCopy(&gModel.error, StrL("the library service is not answering"));
    }
    gModel.loading = false;
    LeaveLib();
    Repaint();
}

static void EnsureModel() {
    if (gModel.loaded || gModel.loading) {
        return;
    }
    gModel.loading = true;
    RunAsync(MkFunc0<LibJob>(LoadModelThread, NewJob({})), "libModel");
}

struct ScanStatus {
    bool scanning = false;
    int done = 0;
    int total = 0;
};

struct StatusParser : json::ValueVisitor {
    ScanStatus* st;

    explicit StatusParser(ScanStatus* status) : st(status) {}

    bool Visit(Str path, Str value, json::Type type) override {
        if (type == json::Type::Null) {
            return true;
        }
        if (str::Eq(path, StrL("/scanning"))) {
            st->scanning = IsTrue(value);
        } else if (str::Eq(path, StrL("/scan_done"))) {
            st->done = atoi(value.s);
        } else if (str::Eq(path, StrL("/scan_total"))) {
            st->total = atoi(value.s);
        }
        return true;
    }
};

static void RescanThread(LibJob* job) {
    FreeJob(job);
    LibraryEnsureService();
    ServicePost("/refresh", StrL("{}"));
    for (int i = 0; i < 600; i++) {
        SleepInMs(1000);
        TempStr body = ServiceGetTextTemp("/status");
        if (len(body) == 0) {
            continue;
        }
        ScanStatus probe;
        StatusParser p(&probe);
        json::Parse(Str(body), &p);
        EnterLib();
        gModel.scanning = probe.scanning;
        gModel.scanDone = probe.done;
        gModel.scanTotal = probe.total;
        if (!probe.scanning) {
            gModel.loaded = false;
        }
        LeaveLib();
        Repaint();
        if (!probe.scanning) {
            return;
        }
    }
}

void LibraryRefresh(MainWindow* win, bool rescan) {
    gNotifyHwnd = win ? win->hwndCanvas : gNotifyHwnd;
    if (rescan) {
        RunAsync(MkFunc0<LibJob>(RescanThread, NewJob({})), "libRescan");
    }
    EnterLib();
    gModel.loaded = false;
    LeaveLib();
    EnsureModel();
    Repaint();
}

static void LoadDetailThread(LibJob* job) {
    TempStr id = str::DupTemp(job->a);
    FreeJob(job);
    TempStr path = fmt("/book?id=%s", id);
    TempStr body = ServiceGetTextTemp(path);
    EnterLib();
    if (str::Eq(gDetail.id, Str(id))) {
        if (len(body) > 0) {
            DetailParser p(&gDetail);
            json::Parse(Str(body), &p);
        }
        gDetail.loading = false;
    }
    LeaveLib();
    Repaint();
}

static void LoadScreenThread(LibJob* job) {
    TempStr id = str::DupTemp(job->a);
    FreeJob(job);
    TempStr path = fmt("/screen?id=%s", id);
    TempStr body = ServiceGetTextTemp(path);
    EnterLib();
    if (str::Eq(gDetail.id, Str(id))) {
        if (len(body) > 0) {
            ScreenParser p(&gDetail);
            json::Parse(Str(body), &p);
        }
        gDetail.screenLoading = false;
        gDetail.screenDone = true;
    }
    LeaveLib();
    Repaint();
}

static void LoadPersonThread(LibJob* job) {
    TempStr series = str::DupTemp(job->a);
    TempStr who = str::DupTemp(job->b);
    FreeJob(job);
    TempStr path = fmt("/wiki?q=character&series=%s&name=%s", UrlEncodeTemp(series), UrlEncodeTemp(who));
    TempStr body = ServiceGetTextTemp(path);
    TempStr famPath = fmt("/wiki?q=family&series=%s&name=%s", UrlEncodeTemp(series), UrlEncodeTemp(who));
    TempStr fam = ServiceGetTextTemp(famPath);
    EnterLib();
    if (str::EqI(gDetail.person, Str(who))) {
        if (len(body) > 0) {
            PersonParser p(&gDetail);
            json::Parse(Str(body), &p);
        }
        gDetail.personLoaded = true;
        for (int i = 0; i < gDetail.nFamily; i++) {
            str::Free(gDetail.family[i].relation);
            str::Free(gDetail.family[i].name);
            gDetail.family[i] = LibFamilyRow{};
        }
        gDetail.nFamily = 0;
        if (len(fam) > 0) {
            FamilyParser p(&gDetail, Str(who));
            json::Parse(Str(fam), &p);
        }
    }
    LeaveLib();
    Repaint();
}

static void LoadChaptersThread(LibJob* job) {
    TempStr id = str::DupTemp(job->a);
    FreeJob(job);
    TempStr body = ServiceGetTextTemp(fmt("/chapters?id=%s", id));
    EnterLib();
    if (str::Eq(gDetail.id, Str(id))) {
        if (len(body) > 0) {
            ChapterParser p(&gDetail);
            json::Parse(Str(body), &p);
        }
        for (int i = 0; i < gDetail.nChapters; i++) {
            gDetail.chapters[i].open = gDetail.chapters[i].depth > 0;
        }
        gDetail.chaptersLoading = false;
        gDetail.chaptersDone = true;
    }
    LeaveLib();
    Repaint();
}

static void LoadTopicThread(LibJob* job) {
    TempStr series = str::DupTemp(job->a);
    TempStr what = str::DupTemp(job->b);
    FreeJob(job);
    TempStr path = fmt("/wiki?q=knows&series=%s&topic=%s", UrlEncodeTemp(series), UrlEncodeTemp(what));
    TempStr body = ServiceGetTextTemp(path);
    EnterLib();
    if (str::Eq(gDetail.topic, Str(what))) {
        if (len(body) > 0) {
            KnowsParser p(&gDetail);
            json::Parse(Str(body), &p);
        }
        gDetail.topicLoaded = true;
    }
    LeaveLib();
    Repaint();
}

static void OpenDetail(Str id) {
    EnterLib();
    FreeDetail(&gDetail);
    str::ReplaceWithCopy(&gDetail.id, id);
    gDetail.loading = true;
    gDetail.tab = LibTab::Overview;
    LeaveLib();
    gDetailOpen = true;
    RunAsync(MkFunc0<LibJob>(LoadDetailThread, NewJob(id)), "libDetail");
}

static void EnsureScreen() {
    if (gDetail.screenLoading || gDetail.screenDone || len(gDetail.id) == 0) {
        return;
    }
    gDetail.screenLoading = true;
    RunAsync(MkFunc0<LibJob>(LoadScreenThread, NewJob(gDetail.id)), "libScreen");
}

static void EnsureChapters() {
    if (gDetail.chaptersLoading || gDetail.chaptersDone || len(gDetail.id) == 0) {
        return;
    }
    gDetail.chaptersLoading = true;
    RunAsync(MkFunc0<LibJob>(LoadChaptersThread, NewJob(gDetail.id)), "libChapters");
}

static void OpenTopic(Str what) {
    if (len(gDetail.wiki) == 0) {
        return;
    }
    EnterLib();
    for (int i = 0; i < gDetail.nKnowers; i++) {
        str::Free(gDetail.knowers[i].name);
        str::Free(gDetail.knowers[i].book);
        gDetail.knowers[i] = LibKnower{};
    }
    gDetail.nKnowers = 0;
    gDetail.topicLoaded = false;
    str::ReplaceWithCopy(&gDetail.topic, what);
    LeaveLib();
    RunAsync(MkFunc0<LibJob>(LoadTopicThread, NewJob(gDetail.wiki, what)), "libTopic");
}

static void OpenPerson(Str who) {
    if (len(gDetail.wiki) == 0) {
        return;
    }
    EnterLib();
    str::ReplaceWithCopy(&gDetail.person, who);
    str::FreePtr(&gDetail.personTraits);
    str::FreePtr(&gDetail.personNot);
    str::FreePtr(&gDetail.personKin);
    str::FreePtr(&gDetail.personSpeech);
    str::FreePtr(&gDetail.personVoice);
    str::FreePtr(&gDetail.personPlaces);
    str::FreePtr(&gDetail.personKnows);
    str::FreePtr(&gDetail.personQuote);
    str::FreePtr(&gDetail.personQuoteBook);
    gDetail.personQuotePage = 0;
    gDetail.personBooks = 0;
    gDetail.personLoaded = false;
    LeaveLib();
    RunAsync(MkFunc0<LibJob>(LoadPersonThread, NewJob(gDetail.wiki, who)), "libPerson");
}

static CoverSlot* SlotFor(Str key) {
    for (int i = 0; i < gNCovers; i++) {
        if (str::Eq(gCovers[i].key, key)) {
            return &gCovers[i];
        }
    }
    if (gNCovers >= kMaxCovers) {
        return nullptr;
    }
    CoverSlot* s = &gCovers[gNCovers++];
    str::ReplaceWithCopy(&s->key, key);
    return s;
}

static void CoverWorker(LibJob* job) {
    FreeJob(job);
    for (;;) {
        CoverSlot* pick = nullptr;
        EnterLib();
        for (int i = 0; i < gNCovers; i++) {
            CoverSlot& s = gCovers[i];
            if (s.wanted && !s.fetching && !s.failed && len(s.bytes) == 0) {
                s.fetching = true;
                pick = &s;
                break;
            }
        }
        if (!pick) {
            gWorkers--;
            LeaveLib();
            return;
        }
        TempStr key = str::DupTemp(pick->key);
        LeaveLib();

        HttpRsp rsp;
        TempStr path;
        if (str::StartsWith(key, StrL("http"))) {
            path = fmt("/poster?url=%s&key=%s", UrlEncodeTemp(key), UrlEncodeTemp(key));
        } else {
            path = fmt("/cover?id=%s", key);
        }
        bool ok = ServiceGet(path, &rsp);

        EnterLib();
        pick->fetching = false;
        if (ok && len(ToStr(rsp.data)) > 64) {
            pick->bytes = str::Dup(ToStr(rsp.data));
        } else {
            pick->failed = true;
        }
        LeaveLib();
        Repaint();
    }
}

static void WakeCoverWorkers() {
    while (gWorkers < kCoverWorkers) {
        gWorkers++;
        RunAsync(MkFunc0<LibJob>(CoverWorker, NewJob({})), "libCover");
    }
}

static RenderedBitmap* CoverBitmap(Str key) {
    if (len(key) == 0) {
        return nullptr;
    }
    EnterLib();
    CoverSlot* s = SlotFor(key);
    if (!s) {
        LeaveLib();
        return nullptr;
    }
    s->wanted = true;
    bool needFetch = !s->decoded && !s->failed && len(s->bytes) == 0;
    bool canDecode = !s->decoded && len(s->bytes) > 0;
    RenderedBitmap* out = s->bmp;
    LeaveLib();

    if (needFetch) {
        WakeCoverWorkers();
        return nullptr;
    }
    if (!canDecode) {
        return out;
    }

    EnterLib();
    Str raw = s->bytes;
    Pixmap* px = PixmapFromData(raw);
    s->decoded = true;
    if (px) {
        s->bmp = RenderedBitmapFromPixmap(px);
    }
    out = s->bmp;
    LeaveLib();
    return out;
}

static TempStr JsonStrTemp(Str s) {
    str::Builder b;
    b.Append("\"");
    for (int i = 0; i < len(s); i++) {
        char c = s.s[i];
        if (c == '"' || c == '\\') {
            b.Append(fmt("\\%c", c));
        } else if ((u8)c < 0x20) {
            b.Append(fmt("\\u%04x", (int)(u8)c));
        } else {
            char one[2] = {c, 0};
            b.Append(Str(one, 1));
        }
    }
    b.Append("\"");
    return str::DupTemp(ToStr(b));
}

static void PartitionThread(LibJob* job) {
    LibraryEnsureService();
    ServicePost(job->a.s, job->b);
    FreeJob(job);
    EnterLib();
    gModel.loading = true;
    LeaveLib();
    LoadModelThread(NewJob({}));
}

static void PostPartition(const char* path, Str body) {
    RunAsync(MkFunc0<LibJob>(PartitionThread, NewJob(Str(path), body)), "libPartition");
}

void LibraryFreeCache() {
    EnterLib();
    FreePartitions();
    for (int i = 0; i < gNCovers; i++) {
        delete gCovers[i].bmp;
        str::Free(gCovers[i].key);
        str::Free(gCovers[i].bytes);
        gCovers[i] = CoverSlot{};
    }
    gNCovers = 0;
    FreeModel(&gModel);
    FreeDetail(&gDetail);
    str::Free(gModel.filter);
    gModel.filter = {};
    gModel.loaded = false;
    LeaveLib();
}

struct LibHit {
    Rect rect;
    Str target;
    Str tip;
};

struct LibLayout {
    MainWindow* win = nullptr;
    HDC hdc = nullptr;
    Rect rc;
    Rect rcRail;
    Rect rcMain;
    int contentDy = 0;
    int scrollY = 0;
};

static int gContentDy = 0;
static int gRailScrollY = 0;
static int gRailDy = 0;
static Rect gRailBand;

struct LibScrollBar {
    Rect track;
    Rect thumb;
    int contentDy = 0;
    int viewDy = 0;
    bool live = false;
};

static LibScrollBar gRailBar;
static LibScrollBar gMainBar;
static LibScrollBar* gBarDrag = nullptr;
static LibScrollBar* gBarHot = nullptr;
static int gBarGrabDy = 0;

static void AddLink(MainWindow* win, Rect r, Str target, Str tip = {}) {
    if (r.dy <= 0 || r.dx <= 0) {
        return;
    }
    win->staticLinks.Append(new StaticLink(r, target, tip));
}

static void DrawTextIn(HDC hdc, Rect r, Str text, UINT fmtFlags, COLORREF col) {
    if (len(text) == 0 || r.dy <= 0) {
        return;
    }
    TempWStr ws = ToWStrTemp(text);
    RECT rc = {r.x, r.y, r.x + r.dx, r.y + r.dy};
    SetTextColor(hdc, col);
    DrawTextW(hdc, ws.s, ws.len, &rc, fmtFlags);
}

static int MeasureTextDy(HDC hdc, Rect r, Str text, UINT fmtFlags) {
    if (len(text) == 0) {
        return 0;
    }
    TempWStr ws = ToWStrTemp(text);
    RECT rc = {r.x, r.y, r.x + r.dx, r.y + r.dy};
    DrawTextW(hdc, ws.s, ws.len, &rc, fmtFlags | DT_CALCRECT);
    return rc.bottom - rc.top;
}

static void FillRound(HDC hdc, Rect r, COLORREF col, int radius) {
    AutoDeleteBrush br(CreateSolidBrush(col));
    AutoDeletePen pen(CreatePen(PS_SOLID, 1, col));
    ScopedSelectObject sb(hdc, br);
    ScopedSelectObject sp(hdc, pen);
    RoundRect(hdc, r.x, r.y, r.x + r.dx, r.y + r.dy, radius, radius);
}

static COLORREF Mix(COLORREF a, COLORREF b, int pct) {
    int r = (GetRValue(a) * (100 - pct) + GetRValue(b) * pct) / 100;
    int g = (GetGValue(a) * (100 - pct) + GetGValue(b) * pct) / 100;
    int bl = (GetBValue(a) * (100 - pct) + GetBValue(b) * pct) / 100;
    return RGB(r, g, bl);
}

static int BarWidth(HDC hdc) {
    return DpiScale(hdc, 12);
}

static void LayoutBar(HDC hdc, LibScrollBar* bar, Rect track, int contentDy, int viewDy, int pos) {
    *bar = LibScrollBar{};
    if (viewDy <= 0 || track.dy <= 0 || contentDy <= viewDy) {
        return;
    }
    bar->live = true;
    bar->track = track;
    bar->contentDy = contentDy;
    bar->viewDy = viewDy;
    int least = DpiScale(hdc, 30);
    int thumbDy = (int)((i64)track.dy * viewDy / contentDy);
    thumbDy = limitValue(thumbDy, least, track.dy);
    int span = track.dy - thumbDy;
    int most = contentDy - viewDy;
    int at = (most > 0) ? (int)((i64)span * limitValue(pos, 0, most) / most) : 0;
    bar->thumb = Rect(track.x, track.y + at, track.dx, thumbDy);
}

static void DrawBar(HDC hdc, const LibScrollBar& bar, bool hot) {
    if (!bar.live) {
        return;
    }
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    int inset = DpiScale(hdc, 3);
    int wide = bar.track.dx - 2 * inset;
    if (wide < 2) {
        wide = 2;
    }
    FillRound(hdc, Rect(bar.track.x + inset, bar.track.y, wide, bar.track.dy), Mix(bg, text, 7), wide);
    FillRound(hdc, Rect(bar.thumb.x + inset, bar.thumb.y, wide, bar.thumb.dy), Mix(bg, text, hot ? 55 : 30), wide);
}

static int PosFromBar(const LibScrollBar& bar, int y) {
    int span = bar.track.dy - bar.thumb.dy;
    if (span <= 0) {
        return 0;
    }
    int most = bar.contentDy - bar.viewDy;
    int at = limitValue(y - bar.track.y, 0, span);
    return (int)((i64)most * at / span);
}

static bool BookVisible(const LibBook& b) {
    if (len(gModel.filter) == 0) {
        return true;
    }
    if (len(b.keys) == 0) {
        return false;
    }
    TempStr want = fmt("|%s|", gModel.filter);
    return str::Contains(b.keys, want.s);
}

static TempStr SubtitleTemp(const LibBook& b) {
    str::Builder s;
    if (b.volume > 0) {
        s.Append(fmt("#%d", b.volume));
    }
    if (len(b.author) > 0) {
        if (len(ToStr(s)) > 0) {
            s.Append(" \xc2\xb7 ");
        }
        s.Append(b.author);
    }
    if (len(ToStr(s)) == 0 && b.pages > 0) {
        s.Append(fmt("%d pages", b.pages));
    }
    return str::DupTemp(ToStr(s));
}

static void DrawCoverTile(HDC hdc, MainWindow* win, Rect tile, const LibBook& b, HFONT fontTitle, HFONT fontSub) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int coverDy = tile.dy - DpiScale(hdc, 44);
    Rect rcCover(tile.x, tile.y, tile.dx, coverDy);

    RenderedBitmap* bmp = CoverBitmap(b.id);
    Rect fit = rcCover;
    if (bmp && bmp->IsValid()) {
        Size sz = bmp->GetSize();
        if (sz.dx > 0 && sz.dy > 0) {
            double want = (double)rcCover.dx / (double)rcCover.dy;
            double have = (double)sz.dx / (double)sz.dy;
            if (have > want) {
                fit.dy = (int)(rcCover.dx / have);
                fit.y = rcCover.y + (rcCover.dy - fit.dy);
            } else {
                fit.dx = (int)(rcCover.dy * have);
                fit.x = rcCover.x + (rcCover.dx - fit.dx) / 2;
            }
        }
        int saved = SaveDC(hdc);
        HRGN clip = CreateRoundRectRgn(fit.x, fit.y, fit.x + fit.dx + 1, fit.y + fit.dy + 1, 8, 8);
        ExtSelectClipRgn(hdc, clip, RGN_AND);
        bmp->Blit(hdc, fit);
        RestoreDC(hdc, saved);
        DeleteObject(clip);
    } else {
        FillRound(hdc, rcCover, Mix(bg, text, 12), 8);
        Rect inner(rcCover.x + DpiScale(hdc, 8), rcCover.y + rcCover.dy / 3, rcCover.dx - DpiScale(hdc, 16),
                   rcCover.dy / 3);
        SelectObject(hdc, fontSub);
        DrawTextIn(hdc, inner, b.title, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS, dim);
    }

    if (b.booknlp) {
        int d = DpiScale(hdc, 9);
        Rect dot(fit.x + fit.dx - d - DpiScale(hdc, 5), fit.y + DpiScale(hdc, 5), d, d);
        FillRound(hdc, dot, RGB(93, 160, 40), d);
    }

    Rect rcTitle(tile.x, tile.y + coverDy + DpiScale(hdc, 6), tile.dx, DpiScale(hdc, 18));
    SelectObject(hdc, fontTitle);
    DrawTextIn(hdc, rcTitle, b.title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);

    Rect rcSub(tile.x, rcTitle.y + rcTitle.dy, tile.dx, DpiScale(hdc, 16));
    SelectObject(hdc, fontSub);
    DrawTextIn(hdc, rcSub, SubtitleTemp(b), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, dim);

    TempStr open = fmt("%s%s", Str(kLinkOpen), b.path);
    if (len(b.path) > 0) {
        AddLink(win, rcCover, Str(open), StrL("Open where you left off"));
    }
    TempStr target = fmt("%s%s", Str(kLinkBook), b.id);
    Rect rcText(tile.x, rcTitle.y, tile.dx, rcTitle.dy + rcSub.dy);
    AddLink(win, rcText, Str(target), StrL("About this book"));
}

struct LibSortChoice {
    const char* key;
    const char* label;
    const char* tip;
};

static const LibSortChoice gSortChoices[] = {
    {"alpha", "A-Z", "Sort the series by name"},
    {"genre", "Genre", "Group the series under their genre"},
    {"most", "Most", "Biggest series first"},
    {"fewest", "Fewest", "Smallest series first"},
};

static int DrawSortRow(HDC hdc, MainWindow* win, Rect row, HFONT font) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);
    Str now = LibrarySortOrder();

    SelectObject(hdc, font);
    Rect rcLabel(row.x, row.y, DpiScale(hdc, 32), row.dy);
    DrawTextIn(hdc, rcLabel, StrL("Sort"), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, dim);
    int x = rcLabel.x + rcLabel.dx;
    int y = row.y;
    for (const LibSortChoice& c : gSortChoices) {
        Str label(c.label);
        TempWStr ws = ToWStrTemp(label);
        SIZE sz{};
        GetTextExtentPoint32W(hdc, ws.s, ws.len, &sz);
        int dx = sz.cx + DpiScale(hdc, 10);
        if (x + dx > row.x + row.dx) {
            x = rcLabel.x + rcLabel.dx;
            y += row.dy;
        }
        Rect one(x, y, dx, row.dy);
        bool active = str::Eq(now, Str(c.key));
        if (active) {
            FillRound(hdc, one, Mix(bg, text, 16), 5);
        }
        DrawTextIn(hdc, one, label, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX,
                   active ? text : ThemeWindowLinkColor());
        AddLink(win, one, fmt("%s%s", Str(kLinkSort), Str(c.key)), Str(c.tip));
        x += dx + DpiScale(hdc, 2);
    }
    return y + row.dy - row.y;
}

static void DrawRail(HDC hdc, MainWindow* win, Rect rail, HFONT fontRow, HFONT fontHead) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);
    COLORREF sel = Mix(bg, text, 14);

    FillRect(hdc, rail, Mix(bg, text, 5));

    int pad = DpiScale(hdc, 12);
    int y = rail.y + pad;
    int rowDy = DpiScale(hdc, 26);

    SelectObject(hdc, fontHead);
    Rect rcHead(rail.x + pad, y, rail.dx - 2 * pad, DpiScale(hdc, 22));
    DrawTextIn(hdc, rcHead, StrL("Library"), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, text);
    y += rcHead.dy + DpiScale(hdc, 10);

    SelectObject(hdc, fontRow);
    {
        Rect row(rail.x + DpiScale(hdc, 6), y, rail.dx - DpiScale(hdc, 12), rowDy);
        if (len(gModel.filter) == 0) {
            FillRound(hdc, row, sel, 6);
        }
        Rect label(row.x + DpiScale(hdc, 8), row.y, row.dx - DpiScale(hdc, 16), row.dy);
        TempStr all = fmt("All books  (%d)", gModel.total);
        DrawTextIn(hdc, label, Str(all), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS, text);
        AddLink(win, row, Str(kLinkAllBooks));
        y += rowDy + DpiScale(hdc, 4);
    }

    Rect rcSort(rail.x + DpiScale(hdc, 10), y, rail.dx - DpiScale(hdc, 16), DpiScale(hdc, 22));
    y += DrawSortRow(hdc, win, rcSort, fontRow) + DpiScale(hdc, 8);

    int footDy = DpiScale(hdc, 24);
    int lastY = rail.y + rail.dy - 2 * footDy - 2 * pad;
    int headDy = DpiScale(hdc, 20);
    int firstY = y;
    int bandDy = lastY - firstY;
    int stepDy = rowDy + DpiScale(hdc, 2);
    int needDy = 0;
    for (int i = 0; i < gModel.nSeries; i++) {
        const LibSeries& s = gModel.series[i];
        needDy += (len(s.head) > 0 ? headDy : 0) + (len(s.subhead) > 0 ? headDy : 0) + stepDy;
    }
    int barDx = BarWidth(hdc);
    gRailDy = needDy;
    gRailBand = Rect(rail.x, firstY, rail.dx, bandDy);
    gRailScrollY = limitValue(gRailScrollY, 0, needDy > bandDy ? needDy - bandDy : 0);
    LayoutBar(hdc, &gRailBar, Rect(rail.x + rail.dx - barDx, firstY, barDx, bandDy), needDy, bandDy, gRailScrollY);
    int gutter = gRailBar.live ? barDx : 0;
    if (gRailScrollY > 0) {
        y -= gRailScrollY;
    }
    for (int i = 0; i < gModel.nSeries; i++) {
        const LibSeries& s = gModel.series[i];
        int above = (len(s.head) > 0 ? headDy : 0) + (len(s.subhead) > 0 ? headDy : 0);
        if (y + above + rowDy > lastY) {
            y += above + rowDy + DpiScale(hdc, 2);
            continue;
        }
        if (y + above < firstY) {
            y += above + rowDy + DpiScale(hdc, 2);
            continue;
        }
        SelectObject(hdc, fontRow);
        if (len(s.head) > 0) {
            Rect rcGenre(rail.x + DpiScale(hdc, 8), y, rail.dx - DpiScale(hdc, 14) - gutter, headDy);
            DrawTextIn(hdc, rcGenre, s.head, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, text);
            y += headDy;
        }
        if (len(s.subhead) > 0) {
            Rect rcSub(rail.x + DpiScale(hdc, 14), y, rail.dx - DpiScale(hdc, 20) - gutter, headDy);
            DrawTextIn(hdc, rcSub, s.subhead, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, dim);
            y += headDy;
        }
        Rect row(rail.x + DpiScale(hdc, 6), y, rail.dx - DpiScale(hdc, 12) - gutter, rowDy);
        if (str::EqI(gModel.filter, s.key)) {
            FillRound(hdc, row, sel, 6);
        }
        int indent = DpiScale(hdc, 8) + s.depth * DpiScale(hdc, 14);
        Rect label(row.x + indent, row.y, row.dx - indent - DpiScale(hdc, 34), row.dy);
        DrawTextIn(hdc, label, s.name, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS, text);
        Rect count(row.x + row.dx - DpiScale(hdc, 32), row.y, DpiScale(hdc, 28), row.dy);
        DrawTextIn(hdc, count, fmt("%d", s.books), DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, dim);
        TempStr target = fmt("%s%s", Str(kLinkSeries), s.key);
        TempStr tip = fmt("%s \xc2\xb7 %d books", s.name, s.books);
        if (len(s.genre) > 0) {
            tip = fmt("%s \xc2\xb7 %s", Str(tip), s.genre);
        }
        if (len(s.sub) > 0) {
            tip = fmt("%s \xe2\x80\xba %s", Str(tip), s.sub);
        }
        if (len(s.author) > 0) {
            tip = fmt("%s \xc2\xb7 %s", Str(tip), s.author);
        }
        if (len(s.wiki) > 0) {
            tip = fmt("%s \xc2\xb7 wiki: %s (%d facts)", Str(tip), s.wiki, s.facts);
        }
        if (len(s.guessed) > 0) {
            tip = fmt("%s \xc2\xb7 put here because it says \"%s\"", Str(tip), s.guessed);
        }
        AddLink(win, row, Str(target), Str(tip));
        y += rowDy + DpiScale(hdc, 2);
    }
    DrawBar(hdc, gRailBar, gBarHot == &gRailBar || gBarDrag == &gRailBar);

    Rect rcRescan(rail.x + DpiScale(hdc, 6), rail.y + rail.dy - 2 * footDy - pad, rail.dx - DpiScale(hdc, 12), footDy);
    SelectObject(hdc, fontRow);
    Str rescanLabel = StrL("Rescan library");
    if (gModel.scanning) {
        rescanLabel = gModel.scanTotal > 0 ? Str(fmt("Scanning %d of %d...", gModel.scanDone, gModel.scanTotal))
                                           : StrL("Scanning...");
    }
    DrawTextIn(hdc, Rect(rcRescan.x + DpiScale(hdc, 8), rcRescan.y, rcRescan.dx, rcRescan.dy), rescanLabel,
               DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, ThemeWindowLinkColor());
    if (!gModel.scanning) {
        AddLink(win, rcRescan, Str(kLinkRescan), StrL("Look for new books on disk"));
    }

    Rect rcClassic(rcRescan.x, rcRescan.y + footDy, rcRescan.dx, footDy);
    DrawTextIn(hdc, Rect(rcClassic.x + DpiScale(hdc, 8), rcClassic.y, rcClassic.dx, rcClassic.dy),
               StrL("Frequently read"), DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, ThemeWindowLinkColor());
    AddLink(win, rcClassic, Str(kLinkClassic), StrL("Show the classic home page"));
}

static void DrawGrid(HDC hdc, MainWindow* win, Rect main, int scrollY, HFONT fontTitle, HFONT fontSub, HFONT fontHead) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int pad = DpiScale(hdc, 20);
    int tileDx = DpiScale(hdc, 132);
    int tileDy = DpiScale(hdc, 240);
    int gapX = DpiScale(hdc, 20);
    int gapY = DpiScale(hdc, 22);

    int avail = main.dx - 2 * pad;
    int perRow = (avail + gapX) / (tileDx + gapX);
    if (perRow < 1) {
        perRow = 1;
    }

    int headDy = DpiScale(hdc, 40);
    SelectObject(hdc, fontHead);
    Rect rcHead(main.x + pad, main.y + DpiScale(hdc, 12), avail, DpiScale(hdc, 26));
    Str title = len(gModel.filterName) > 0 ? gModel.filterName : StrL("Everything");
    DrawTextIn(hdc, rcHead, title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, text);

    int shown = 0;
    for (int i = 0; i < gModel.nBooks; i++) {
        if (BookVisible(gModel.books[i])) {
            shown++;
        }
    }
    SelectObject(hdc, fontSub);
    Rect rcCount(main.x + pad, rcHead.y + rcHead.dy, avail, DpiScale(hdc, 16));
    int withWiki = 0;
    for (int i = 0; i < gModel.nBooks; i++) {
        if (BookVisible(gModel.books[i]) && gModel.books[i].booknlp) {
            withWiki++;
        }
    }
    DrawTextIn(hdc, rcCount,
               fmt("%d %s \xc2\xb7 %d read by BookNLP", shown, shown == 1 ? StrL("book") : StrL("books"), withWiki),
               DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);

    int top = main.y + headDy + DpiScale(hdc, 22) - scrollY;
    int col = 0;
    int row = 0;
    for (int i = 0; i < gModel.nBooks; i++) {
        const LibBook& b = gModel.books[i];
        if (!BookVisible(b)) {
            continue;
        }
        int x = main.x + pad + col * (tileDx + gapX);
        int y = top + row * (tileDy + gapY);
        Rect tile(x, y, tileDx, tileDy);
        if (y + tileDy >= main.y && y <= main.y + main.dy) {
            DrawCoverTile(hdc, win, tile, b, fontTitle, fontSub);
        }
        col++;
        if (col >= perRow) {
            col = 0;
            row++;
        }
    }
    int rows = (shown + perRow - 1) / perRow;
    gContentDy = headDy + DpiScale(hdc, 22) + rows * (tileDy + gapY);
}

static void DrawTabs(HDC hdc, MainWindow* win, Rect r, HFONT font) {
    static const char* names[] = {"Overview", "Characters", "Family", "Places", "Who knows what", "On screen"};
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF sel = Mix(bg, text, 16);
    SelectObject(hdc, font);
    int x = r.x;
    for (int i = 0; i < 6; i++) {
        Str label(names[i]);
        Rect probe(0, 0, 400, 40);
        int dx = 0;
        {
            TempWStr ws = ToWStrTemp(label);
            SIZE sz{};
            GetTextExtentPoint32W(hdc, ws.s, ws.len, &sz);
            dx = sz.cx + DpiScale(hdc, 22);
            (void)probe;
        }
        Rect tab(x, r.y, dx, r.dy);
        bool active = (int)gDetail.tab == i;
        if (active) {
            FillRound(hdc, tab, sel, 6);
        }
        DrawTextIn(hdc, tab, label, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX,
                   active ? text : Mix(text, bg, 40));
        AddLink(win, tab, fmt("%s%d", Str(kLinkTab), i));
        x += dx + DpiScale(hdc, 4);
    }
}

static void DrawChips(HDC hdc, MainWindow* win, Rect area, Str* items, int n, const char* linkPrefix, HFONT font,
                      int* usedDy) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF chip = Mix(bg, text, 10);
    SelectObject(hdc, font);
    int x = area.x;
    int y = area.y;
    int chipDy = DpiScale(hdc, 24);
    for (int i = 0; i < n; i++) {
        if (len(items[i]) == 0) {
            continue;
        }
        TempWStr ws = ToWStrTemp(items[i]);
        SIZE sz{};
        GetTextExtentPoint32W(hdc, ws.s, ws.len, &sz);
        int dx = sz.cx + DpiScale(hdc, 18);
        if (x + dx > area.x + area.dx) {
            x = area.x;
            y += chipDy + DpiScale(hdc, 6);
        }
        if (y + chipDy > area.y + area.dy) {
            break;
        }
        Rect r(x, y, dx, chipDy);
        FillRound(hdc, r, chip, chipDy / 2);
        DrawTextIn(hdc, r, items[i], DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, text);
        if (linkPrefix) {
            AddLink(win, r, fmt("%s%s", Str(linkPrefix), items[i]));
        }
        x += dx + DpiScale(hdc, 6);
    }
    *usedDy = (y + chipDy) - area.y;
}

static void DrawScreenRow(HDC hdc, MainWindow* win, Rect area, HFONT fontTitle, HFONT fontSub) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);
    int posterDx = DpiScale(hdc, 104);
    int posterDy = DpiScale(hdc, 154);
    int gap = DpiScale(hdc, 18);
    int x = area.x;
    for (int i = 0; i < gDetail.nScreen; i++) {
        const LibScreen& s = gDetail.screen[i];
        if (x + posterDx > area.x + area.dx) {
            break;
        }
        Rect rcP(x, area.y, posterDx, posterDy);
        RenderedBitmap* bmp = len(s.poster) > 0 ? CoverBitmap(s.poster) : nullptr;
        if (bmp && bmp->IsValid()) {
            int saved = SaveDC(hdc);
            HRGN clip = CreateRoundRectRgn(rcP.x, rcP.y, rcP.x + rcP.dx + 1, rcP.y + rcP.dy + 1, 8, 8);
            ExtSelectClipRgn(hdc, clip, RGN_AND);
            bmp->Blit(hdc, rcP);
            RestoreDC(hdc, saved);
            DeleteObject(clip);
        } else {
            FillRound(hdc, rcP, Mix(bg, text, 12), 8);
        }
        SelectObject(hdc, fontTitle);
        Rect rcT(x, rcP.y + posterDy + DpiScale(hdc, 6), posterDx, DpiScale(hdc, 17));
        DrawTextIn(hdc, rcT, s.title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
        SelectObject(hdc, fontSub);
        Rect rcK(x, rcT.y + rcT.dy, posterDx, DpiScale(hdc, 16));
        TempStr sub = s.year > 0 ? fmt("%s \xc2\xb7 %d", s.kind, s.year) : str::DupTemp(s.kind);
        DrawTextIn(hdc, rcK, Str(sub), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, dim);
        Rect rcS(x, rcK.y + rcK.dy, posterDx, DpiScale(hdc, 30));
        DrawTextIn(hdc, rcS, s.stars, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX, dim);
        if (len(s.imdbId) > 0) {
            Rect hot(x, rcP.y, posterDx, posterDy + rcT.dy + rcK.dy);
            AddLink(win, hot, fmt("%s%s", Str(kLinkScreenTitle), s.imdbId), fmt("Open %s on IMDb", s.title));
        }
        x += posterDx + gap;
    }
}

static bool ChapterShown(int i) {
    int parent = gDetail.chapters[i].parent;
    while (parent >= 0) {
        if (!gDetail.chapters[parent].open) {
            return false;
        }
        parent = gDetail.chapters[parent].parent;
    }
    return true;
}

static int DrawChapters(HDC hdc, MainWindow* win, Rect body, HFONT fontSub, HFONT fontBody) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int lineDy = DpiScale(hdc, 19);
    int y = body.y;
    SelectObject(hdc, fontSub);
    if (!gDetail.chaptersDone) {
        DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), StrL("Reading the chapter list..."),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        return lineDy;
    }
    if (gDetail.nChapters == 0) {
        return 0;
    }
    DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), fmt("%d chapters", gDetail.nChapters),
               DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
    y += lineDy + DpiScale(hdc, 4);

    SelectObject(hdc, fontBody);
    for (int i = 0; i < gDetail.nChapters; i++) {
        const LibChapter& c = gDetail.chapters[i];
        if (!ChapterShown(i)) {
            continue;
        }
        if (y + lineDy > body.y + body.dy) {
            break;
        }
        int indent = c.depth * DpiScale(hdc, 16);
        Rect row(body.x + indent, y, body.dx - indent, lineDy);
        if (c.kids > 0) {
            Rect rcArrow(row.x, row.y, DpiScale(hdc, 14), lineDy);
            DrawTextIn(hdc, rcArrow, c.open ? StrL("-") : StrL("+"), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
            Rect rcName(row.x + DpiScale(hdc, 16), row.y, row.dx - DpiScale(hdc, 70), lineDy);
            DrawTextIn(hdc, rcName, c.title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
            AddLink(win, Rect(row.x, row.y, row.dx - DpiScale(hdc, 54), lineDy), fmt("%s%d", Str(kLinkChapter), i),
                    fmt("%d chapters inside", c.kids));
        } else {
            Rect rcName(row.x + DpiScale(hdc, 16), row.y, row.dx - DpiScale(hdc, 70), lineDy);
            DrawTextIn(hdc, rcName, c.title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
        }
        if (c.page > 0) {
            Rect rcPage(body.x + body.dx - DpiScale(hdc, 50), row.y, DpiScale(hdc, 46), lineDy);
            DrawTextIn(hdc, rcPage, fmt("p %d", c.page), DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX, dim);
            AddLink(win, rcPage, fmt("%s%d|%s", Str(kLinkPage), c.page, gDetail.path), StrL("Open at this page"));
        }
        y += lineDy;
    }
    return y - body.y;
}

static int DrawKnows(HDC hdc, MainWindow* win, Rect body, HFONT fontTitle, HFONT fontSub, HFONT fontBody) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int usedDy = 0;
    if (len(gDetail.topic) == 0) {
        SelectObject(hdc, fontSub);
        int lineDy = DpiScale(hdc, 18);
        DrawTextIn(hdc, Rect(body.x, body.y, body.dx, lineDy), StrL("Pick a subject to see who knows about it."),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        Rect rest(body.x, body.y + lineDy + DpiScale(hdc, 6), body.dx, body.dy - lineDy);
        int chipsDy = 0;
        DrawChips(hdc, win, rest, gDetail.topics, gDetail.nTopics, kLinkTopic, fontSub, &chipsDy);
        return lineDy + DpiScale(hdc, 6) + chipsDy;
    }

    int lineDy = DpiScale(hdc, 20);
    int y = body.y;
    SelectObject(hdc, fontSub);
    Rect rcBack(body.x, y, DpiScale(hdc, 130), lineDy);
    DrawTextIn(hdc, rcBack, StrL("< all subjects"), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, ThemeWindowLinkColor());
    AddLink(win, rcBack, Str(kLinkTopicList), StrL("Back to every subject"));
    y += lineDy + DpiScale(hdc, 6);

    SelectObject(hdc, fontTitle);
    DrawTextIn(hdc, Rect(body.x, y, body.dx, DpiScale(hdc, 22)), gDetail.topic, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX,
               text);
    y += DpiScale(hdc, 26);

    SelectObject(hdc, fontBody);
    if (!gDetail.topicLoaded) {
        DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), StrL("Reading the wiki..."),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        return y + lineDy - body.y;
    }
    if (gDetail.nKnowers == 0) {
        DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), StrL("Nobody in this series is recorded knowing about it."),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        return y + lineDy - body.y;
    }

    int indent = DpiScale(hdc, 18);
    int nameDx = DpiScale(hdc, 180);
    for (int i = 0; i < gDetail.nKnowers; i++) {
        const LibKnower& k = gDetail.knowers[i];
        if (len(k.name) == 0) {
            continue;
        }
        if (y + lineDy > body.y + body.dy) {
            break;
        }
        Rect tick(body.x, y, indent, lineDy);
        DrawTextIn(hdc, tick, StrL("\xc2\xb7"), DT_CENTER | DT_SINGLELINE | DT_NOPREFIX, dim);
        Rect rcName(body.x + indent, y, nameDx, lineDy);
        DrawTextIn(hdc, rcName, k.name, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
        AddLink(win, rcName, fmt("%s%s", Str(kLinkPerson), k.name), fmt("Everything about %s", k.name));
        TempStr note =
            k.mentions > 0 ? fmt("%d %s", k.mentions, k.mentions == 1 ? StrL("mention") : StrL("mentions")) : TempStr{};
        if (len(k.book) > 0) {
            TempStr where = k.page > 0 ? fmt("%s, page %d", k.book, k.page) : str::DupTemp(k.book);
            note = len(note) > 0 ? fmt("%s \xc2\xb7 %s", Str(note), Str(where)) : where;
        }
        Rect rcNote(body.x + indent + nameDx, y, body.dx - indent - nameDx, lineDy);
        DrawTextIn(hdc, rcNote, Str(note), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, dim);
        y += lineDy;
    }
    usedDy = y - body.y;
    return usedDy;
}

static int DrawPerson(HDC hdc, Rect body, HFONT fontTitle, HFONT fontSub, HFONT fontBody) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int y = body.y;
    int lineDy = DpiScale(hdc, 20);
    SelectObject(hdc, fontTitle);
    DrawTextIn(hdc, Rect(body.x, y, body.dx, DpiScale(hdc, 22)), gDetail.person, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX,
               text);
    y += DpiScale(hdc, 26);

    SelectObject(hdc, fontBody);
    if (!gDetail.personLoaded) {
        DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), StrL("Reading the wiki..."),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        return y + lineDy - body.y;
    }

    if (len(gDetail.personQuote) > 0) {
        TempStr quote = fmt("\"%s\"", gDetail.personQuote);
        if (len(gDetail.personQuoteBook) > 0) {
            quote = fmt("%s \xc2\xb7 %s", Str(quote), gDetail.personQuoteBook);
            if (gDetail.personQuotePage > 0) {
                quote = fmt("%s, page %d", Str(quote), gDetail.personQuotePage);
            }
        }
        int dy = MeasureTextDy(hdc, body, Str(quote), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        DrawTextIn(hdc, Rect(body.x, y, body.dx, dy), Str(quote), DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, text);
        y += dy + DpiScale(hdc, 14);
    }

    struct PersonRow {
        Str label;
        Str value;
    };
    PersonRow rows[] = {
        {StrL("Described as"), gDetail.personTraits}, {StrL("But not"), gDetail.personNot},
        {StrL("Family"), gDetail.personKin},          {StrL("Speaks"), gDetail.personSpeech},
        {StrL("Voice"), gDetail.personVoice},         {StrL("Places"), gDetail.personPlaces},
        {StrL("Knows about"), gDetail.personKnows},
    };
    int labelDx = DpiScale(hdc, 116);
    int gap = DpiScale(hdc, 12);
    int valueDx = body.dx - labelDx - gap;
    for (const PersonRow& row : rows) {
        if (len(row.value) == 0) {
            continue;
        }
        Rect rcValue(body.x + labelDx + gap, y, valueDx, body.dy);
        int dy = MeasureTextDy(hdc, rcValue, row.value, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        if (dy < lineDy) {
            dy = lineDy;
        }
        SelectObject(hdc, fontSub);
        DrawTextIn(hdc, Rect(body.x, y, labelDx, lineDy), row.label, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        SelectObject(hdc, fontBody);
        DrawTextIn(hdc, Rect(rcValue.x, y, valueDx, dy), row.value, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, text);
        y += dy + DpiScale(hdc, 8);
    }

    if (gDetail.personBooks > 0) {
        SelectObject(hdc, fontSub);
        DrawTextIn(hdc, Rect(body.x, y, body.dx, lineDy), fmt("Appears in %d books", gDetail.personBooks),
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
        y += lineDy;
    }
    return y - body.y;
}

static void DrawDetail(HDC hdc, MainWindow* win, Rect main, int scrollY, HFONT fontTitle, HFONT fontSub, HFONT fontHead,
                       HFONT fontBody) {
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF dim = Mix(text, bg, 45);

    int pad = DpiScale(hdc, 24);
    int y = main.y + DpiScale(hdc, 12) - scrollY;

    SelectObject(hdc, fontSub);
    Rect rcBack(main.x + pad, y, DpiScale(hdc, 190), DpiScale(hdc, 20));
    DrawTextIn(hdc, rcBack, StrL("< Back to the library"), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX,
               ThemeWindowLinkColor());
    AddLink(win, rcBack, Str(kLinkBack));
    y += rcBack.dy + DpiScale(hdc, 14);

    int coverDx = DpiScale(hdc, 168);
    int coverDy = DpiScale(hdc, 250);
    Rect rcCover(main.x + pad, y, coverDx, coverDy);
    RenderedBitmap* bmp = CoverBitmap(gDetail.id);
    if (bmp && bmp->IsValid()) {
        Size sz = bmp->GetSize();
        Rect fit = rcCover;
        if (sz.dx > 0 && sz.dy > 0) {
            double have = (double)sz.dx / (double)sz.dy;
            fit.dx = (int)(coverDy * have);
            if (fit.dx > coverDx) {
                fit.dx = coverDx;
                fit.dy = (int)(coverDx / have);
            }
        }
        bmp->Blit(hdc, fit);
    } else {
        FillRound(hdc, rcCover, Mix(bg, text, 12), 8);
    }

    int infoX = main.x + pad + coverDx + DpiScale(hdc, 22);
    int infoDx = main.x + main.dx - infoX - pad;
    int iy = y;

    SelectObject(hdc, fontHead);
    Rect rcTitle(infoX, iy, infoDx, DpiScale(hdc, 30));
    DrawTextIn(hdc, rcTitle, gDetail.title, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
    iy += rcTitle.dy + DpiScale(hdc, 2);

    SelectObject(hdc, fontSub);
    str::Builder meta;
    if (len(gDetail.author) > 0) {
        meta.Append(gDetail.author);
    }
    if (gDetail.year > 0) {
        if (len(ToStr(meta)) > 0) {
            meta.Append(" \xc2\xb7 ");
        }
        meta.Append(fmt("%d", gDetail.year));
    }
    if (len(gDetail.series) > 0) {
        if (len(ToStr(meta)) > 0) {
            meta.Append(" \xc2\xb7 ");
        }
        meta.Append(gDetail.series);
    }
    if (gDetail.pages > 0) {
        if (len(ToStr(meta)) > 0) {
            meta.Append(" \xc2\xb7 ");
        }
        meta.Append(fmt("%d pages", gDetail.pages));
    }
    Rect rcMeta(infoX, iy, infoDx, DpiScale(hdc, 18));
    DrawTextIn(hdc, rcMeta, ToStr(meta), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, dim);
    iy += rcMeta.dy + DpiScale(hdc, 10);

    SelectObject(hdc, fontSub);
    Rect rcRead(infoX, iy, DpiScale(hdc, 92), DpiScale(hdc, 26));
    FillRound(hdc, rcRead, Mix(bg, text, 16), 6);
    DrawTextIn(hdc, rcRead, StrL("Read"), DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX, text);
    if (len(gDetail.path) > 0) {
        AddLink(win, rcRead, fmt("%s%s", Str(kLinkRead), gDetail.path), gDetail.path);
    }
    iy += rcRead.dy + DpiScale(hdc, 12);

    SelectObject(hdc, fontBody);
    Str blurb = gDetail.description;
    if (len(blurb) == 0 && gDetail.loading) {
        blurb = StrL("Looking this book up...");
    }
    int maxDescDy = coverDy - (iy - y);
    Rect rcDesc(infoX, iy, infoDx, maxDescDy);
    if (len(blurb) > 0) {
        UINT flags = DT_LEFT | DT_WORDBREAK | DT_NOPREFIX;
        int want = MeasureTextDy(hdc, rcDesc, blurb, flags);
        rcDesc.dy = want < maxDescDy ? want : maxDescDy;
        DrawTextIn(hdc, rcDesc, blurb, flags, text);
    }

    y += coverDy + DpiScale(hdc, 18);

    Rect rcTabs(main.x + pad, y, main.dx - 2 * pad, DpiScale(hdc, 28));
    DrawTabs(hdc, win, rcTabs, fontSub);
    y += rcTabs.dy + DpiScale(hdc, 16);

    Rect body(main.x + pad, y, main.dx - 2 * pad, main.y + main.dy - y);
    int usedDy = 0;
    switch (gDetail.tab) {
        case LibTab::Overview: {
            SelectObject(hdc, fontBody);
            if (len(gDetail.subjects) > 0) {
                int dy = MeasureTextDy(hdc, body, gDetail.subjects, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
                DrawTextIn(hdc, Rect(body.x, body.y, body.dx, dy), gDetail.subjects,
                           DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, dim);
                usedDy = dy + DpiScale(hdc, 12);
            }
            if (len(gDetail.wiki) == 0) {
                Rect r(body.x, body.y + usedDy, body.dx, DpiScale(hdc, 40));
                DrawTextIn(hdc, r,
                           StrL("No wiki yet for this book. Read it aloud with the BookNLP analyser to build one."),
                           DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, dim);
                usedDy += DpiScale(hdc, 40);
            } else {
                int chipsDy = 0;
                Rect r(body.x, body.y + usedDy, body.dx, body.dy - usedDy);
                DrawChips(hdc, win, r, gDetail.people, gDetail.nPeople < 18 ? gDetail.nPeople : 18, kLinkPerson,
                          fontSub, &chipsDy);
                usedDy += chipsDy;
            }
            EnsureChapters();
            usedDy += DpiScale(hdc, 14);
            usedDy +=
                DrawChapters(hdc, win, Rect(body.x, body.y + usedDy, body.dx, body.dy - usedDy), fontSub, fontBody);
            break;
        }
        case LibTab::People: {
            if (len(gDetail.person) > 0) {
                usedDy = DrawPerson(hdc, body, fontTitle, fontSub, fontBody);
            } else {
                DrawChips(hdc, win, body, gDetail.people, gDetail.nPeople, kLinkPerson, fontSub, &usedDy);
            }
            break;
        }
        case LibTab::Family: {
            SelectObject(hdc, fontBody);
            if (len(gDetail.person) == 0) {
                DrawTextIn(hdc, body, StrL("Pick a character on the Characters tab to see their family."),
                           DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, dim);
                usedDy = DpiScale(hdc, 24);
                break;
            }
            int ry = body.y;
            int rowDy = DpiScale(hdc, 22);
            SelectObject(hdc, fontTitle);
            DrawTextIn(hdc, Rect(body.x, ry, body.dx, rowDy), gDetail.person, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX,
                       text);
            ry += rowDy + DpiScale(hdc, 6);
            SelectObject(hdc, fontBody);
            for (int i = 0; i < gDetail.nFamily; i++) {
                const LibFamilyRow& f = gDetail.family[i];
                if (len(f.name) == 0) {
                    continue;
                }
                if (ry + rowDy > body.y + body.dy) {
                    break;
                }
                Rect rcRel(body.x, ry, DpiScale(hdc, 120), rowDy);
                DrawTextIn(hdc, rcRel, Str(PrettyRelationTemp(f.relation)), DT_LEFT | DT_SINGLELINE | DT_NOPREFIX, dim);
                Rect rcName(body.x + DpiScale(hdc, 126), ry, body.dx - DpiScale(hdc, 126), rowDy);
                DrawTextIn(hdc, rcName, f.name, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, text);
                AddLink(win, rcName, fmt("%s%s", Str(kLinkPerson), f.name));
                ry += rowDy;
            }
            usedDy = ry - body.y;
            break;
        }
        case LibTab::Places:
            DrawChips(hdc, win, body, gDetail.places, gDetail.nPlaces, nullptr, fontSub, &usedDy);
            break;
        case LibTab::Knows:
            usedDy = DrawKnows(hdc, win, body, fontTitle, fontSub, fontBody);
            break;
        case LibTab::Screen: {
            EnsureScreen();
            if (gDetail.nScreen == 0) {
                SelectObject(hdc, fontBody);
                Str msg = gDetail.screenLoading ? StrL("Looking for films and TV...")
                                                : StrL("No film or TV adaptation found for this book.");
                DrawTextIn(hdc, body, msg, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX, dim);
                usedDy = DpiScale(hdc, 24);
            } else {
                DrawScreenRow(hdc, win, body, fontTitle, fontSub);
                usedDy = DpiScale(hdc, 230);
            }
            break;
        }
    }
    gContentDy = (y - main.y + scrollY) + usedDy + DpiScale(hdc, 40);
}

void DrawLibraryPage(MainWindow* win, HDC hdc) {
    gNotifyHwnd = win->hwndCanvas;
    DeleteVecMembers(win->staticLinks);
    EnsureModel();

    Rect rc = ClientRect(win->hwndCanvas);
    COLORREF bg = ThemeMainWindowBackgroundColor();
    COLORREF text = ThemeWindowTextColor();
    FillRect(hdc, rc, bg);
    SetBkMode(hdc, TRANSPARENT);

    HFONT fontHead = CreateSimpleFont(hdc, "MS Shell Dlg", 20);
    HFONT fontTitle = CreateSimpleFont(hdc, "MS Shell Dlg", 13);
    HFONT fontSub = CreateSimpleFont(hdc, "MS Shell Dlg", 12);
    HFONT fontBody = CreateSimpleFont(hdc, "MS Shell Dlg", 13);

    int railDx = DpiScale(hdc, 210);
    if (railDx > rc.dx / 3) {
        railDx = rc.dx / 3;
    }
    Rect rail(rc.x, rc.y, railDx, rc.dy);
    Rect main(rc.x + railDx, rc.y, rc.dx - railDx, rc.dy);

    EnterLib();
    if (!gModel.loaded) {
        SelectObject(hdc, fontBody);
        Str msg = len(gModel.error) > 0 ? gModel.error : StrL("Opening the library...");
        Rect r(rc.x + DpiScale(hdc, 40), rc.y + rc.dy / 2 - DpiScale(hdc, 20), rc.dx - DpiScale(hdc, 80),
               DpiScale(hdc, 60));
        DrawTextIn(hdc, r, msg, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX, Mix(text, bg, 40));
        LeaveLib();
        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        DeleteObject(fontHead);
        DeleteObject(fontTitle);
        DeleteObject(fontSub);
        DeleteObject(fontBody);
        return;
    }

    DrawRail(hdc, win, rail, fontSub, fontHead);

    int barDx = BarWidth(hdc);
    Rect body(main.x, main.y, main.dx - barDx, main.dy);
    int saved = SaveDC(hdc);
    HRGN clip = CreateRectRgn(body.x, body.y, body.x + body.dx, body.y + body.dy);
    SelectClipRgn(hdc, clip);
    DeleteObject(clip);

    if (gDetailOpen) {
        DrawDetail(hdc, win, body, win->homePageScrollY, fontTitle, fontSub, fontHead, fontBody);
    } else {
        DrawGrid(hdc, win, body, win->homePageScrollY, fontTitle, fontSub, fontHead);
    }
    RestoreDC(hdc, saved);
    LeaveLib();

    int mostY = gContentDy - main.dy;
    if (mostY < 0) {
        mostY = 0;
    }
    if (win->homePageScrollY > mostY) {
        win->homePageScrollY = mostY;
    }
    LayoutBar(hdc, &gMainBar, Rect(main.x + main.dx - barDx, main.y, barDx, main.dy), gContentDy, main.dy,
              win->homePageScrollY);
    DrawBar(hdc, gMainBar, gBarHot == &gMainBar || gBarDrag == &gMainBar);

    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    DeleteObject(fontHead);
    DeleteObject(fontTitle);
    DeleteObject(fontSub);
    DeleteObject(fontBody);
}

static int VisibleDy(MainWindow* win) {
    Rect rc = ClientRect(win->hwndCanvas);
    return rc.dy;
}

static void ScrollTo(MainWindow* win, int y) {
    int maxY = gContentDy - VisibleDy(win);
    if (maxY < 0) {
        maxY = 0;
    }
    if (y > maxY) {
        y = maxY;
    }
    if (y < 0) {
        y = 0;
    }
    if (y != win->homePageScrollY) {
        win->homePageScrollY = y;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
}

static void RailScrollTo(MainWindow* win, int y) {
    int most = gRailDy - gRailBand.dy;
    if (most < 0) {
        most = 0;
    }
    y = limitValue(y, 0, most);
    if (y != gRailScrollY) {
        gRailScrollY = y;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
}

static LibScrollBar* BarAt(int x, int y) {
    Point pt(x, y);
    if (gMainBar.live && gMainBar.track.Contains(pt)) {
        return &gMainBar;
    }
    if (gRailBar.live && gRailBar.track.Contains(pt)) {
        return &gRailBar;
    }
    return nullptr;
}

static void BarScrollTo(MainWindow* win, LibScrollBar* bar, int pos) {
    if (bar == &gRailBar) {
        RailScrollTo(win, pos);
    } else {
        ScrollTo(win, pos);
    }
}

bool LibraryOnLeftButtonDown(MainWindow* win, int x, int y) {
    LibScrollBar* bar = BarAt(x, y);
    if (!bar) {
        return false;
    }
    if (bar->thumb.Contains(Point(x, y))) {
        gBarGrabDy = y - bar->thumb.y;
    } else {
        gBarGrabDy = bar->thumb.dy / 2;
        BarScrollTo(win, bar, PosFromBar(*bar, y - gBarGrabDy));
    }
    gBarDrag = bar;
    gBarHot = bar;
    SetCapture(win->hwndCanvas);
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    return true;
}

bool LibraryOnMouseMove(MainWindow* win, int x, int y) {
    if (gBarDrag) {
        BarScrollTo(win, gBarDrag, PosFromBar(*gBarDrag, y - gBarGrabDy));
        return true;
    }
    LibScrollBar* hot = BarAt(x, y);
    if (hot != gBarHot) {
        gBarHot = hot;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    }
    return hot != nullptr;
}

bool LibraryOnLeftButtonUp(MainWindow* win) {
    if (!gBarDrag) {
        return false;
    }
    gBarDrag = nullptr;
    ReleaseCapture();
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);
    return true;
}

void LibraryOnCaptureLost(MainWindow* win) {
    if (!gBarDrag) {
        return;
    }
    gBarDrag = nullptr;
    InvalidateRect(win->hwndCanvas, nullptr, FALSE);
}

static bool CursorOnRail(MainWindow* win, int screenX, int screenY) {
    if (gRailBand.dy <= 0 || gRailDy <= gRailBand.dy) {
        return false;
    }
    POINT pt{screenX, screenY};
    ScreenToClient(win->hwndCanvas, &pt);
    return gRailBand.Contains(Point(pt.x, pt.y));
}

void LibraryOnMouseWheel(MainWindow* win, int delta, int screenX, int screenY) {
    int step = DpiScale(win->hwndCanvas, 90);
    if (CursorOnRail(win, screenX, screenY)) {
        RailScrollTo(win, gRailScrollY + (delta > 0 ? -step : step));
        return;
    }
    ScrollTo(win, win->homePageScrollY + (delta > 0 ? -step : step));
}

void LibraryOnVScroll(MainWindow* win, WPARAM wp) {
    int line = DpiScale(win->hwndCanvas, 90);
    int page = VisibleDy(win) - line;
    int y = win->homePageScrollY;
    switch (LOWORD(wp)) {
        case SB_LINEUP:
            y -= line;
            break;
        case SB_LINEDOWN:
            y += line;
            break;
        case SB_PAGEUP:
            y -= page;
            break;
        case SB_PAGEDOWN:
            y += page;
            break;
        case SB_TOP:
            y = 0;
            break;
        case SB_BOTTOM:
            y = INT_MAX;
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            y = (int)(short)HIWORD(wp);
            break;
    }
    ScrollTo(win, y);
}

static Str AfterPrefix(Str url, const char* prefix) {
    int n = (int)strlen(prefix);
    return Str(url.s + n, url.len - n);
}

struct LibOpenJob {
    MainWindow* win = nullptr;
    int page = 0;
    bool audiobook = false;
};

static void OnLibraryBookLoaded(LibOpenJob* job, bool ok) {
    AutoDelete del(job);
    MainWindow* win = job->win;
    if (!ok || !IsMainWindowValid(win) || win->isBeingClosed || !win->ctrl) {
        return;
    }
    if (job->page > 0 && win->ctrl->ValidPageNo(job->page)) {
        win->ctrl->GoToPage(job->page, false);
        UpdateToolbarPageText(win, win->ctrl->PageCount());
    }
    if (job->audiobook) {
        HwndSendCommand(win->hwndFrame, CmdReadAloudFromTopPage);
    }
}

static void LibraryOpenBook(MainWindow* win, Str path, int pageNo, bool audiobook = false) {
    if (len(path) == 0) {
        return;
    }
    if (pageNo == 0) {
        FileState* fs = gFileHistory.FindByPath(path);
        if (fs && fs->pageNo > 1) {
            pageNo = fs->pageNo;
        }
    }
    auto job = new LibOpenJob;
    job->win = win;
    job->page = pageNo;
    job->audiobook = audiobook;
    LoadArgs args(path, win);
    args.activateExisting = true;
    args.activateExistingInWindow = true;
    args.onFinished = MkFunc1<LibOpenJob, bool>(OnLibraryBookLoaded, job);
    StartLoadDocument(&args);
}

bool LibraryOnLinkClicked(MainWindow* win, Str url) {
    if (!str::StartsWith(url, Str(kLinkLibraryPrefix))) {
        return false;
    }
    if (str::StartsWith(url, Str(kLinkAllBooks))) {
        EnterLib();
        str::FreePtr(&gModel.filter);
        str::FreePtr(&gModel.filterName);
        LeaveLib();
        gDetailOpen = false;
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkSort))) {
        Str how = AfterPrefix(url, kLinkSort);
        if (gGlobalPrefs && !str::Eq(LibrarySortOrder(), how)) {
            str::ReplaceWithCopy(&gGlobalPrefs->audiobook.librarySort, how);
            SaveSettings();
            gRailScrollY = 0;
            EnterLib();
            gModel.loaded = false;
            LeaveLib();
            EnsureModel();
        }
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkSeries))) {
        Str key = AfterPrefix(url, kLinkSeries);
        EnterLib();
        str::ReplaceWithCopy(&gModel.filter, key);
        str::FreePtr(&gModel.filterName);
        for (int i = 0; i < gModel.nSeries; i++) {
            if (str::EqI(gModel.series[i].key, key)) {
                str::ReplaceWithCopy(&gModel.filterName, gModel.series[i].name);
                break;
            }
        }
        LeaveLib();
        gDetailOpen = false;
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkBook))) {
        OpenDetail(AfterPrefix(url, kLinkBook));
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkBack))) {
        gDetailOpen = false;
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkRead))) {
        Str path = AfterPrefix(url, kLinkRead);
        LoadArgs args(path, win);
        args.activateExisting = true;
        args.activateExistingInWindow = true;
        StartLoadDocument(&args);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkOpen))) {
        LibraryOpenBook(win, AfterPrefix(url, kLinkOpen), 0);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkPage))) {
        Str rest = AfterPrefix(url, kLinkPage);
        const char* bar = strchr(rest.s, '|');
        if (bar) {
            int pageNo = atoi(rest.s);
            Str path(bar + 1, (int)(rest.s + rest.len - bar - 1));
            LibraryOpenBook(win, path, pageNo);
        }
        return true;
    }
    if (str::StartsWith(url, Str(kLinkChapter))) {
        int i = atoi(AfterPrefix(url, kLinkChapter).s);
        EnterLib();
        if (i >= 0 && i < gDetail.nChapters) {
            gDetail.chapters[i].open = !gDetail.chapters[i].open;
        }
        LeaveLib();
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkTopicList))) {
        EnterLib();
        str::FreePtr(&gDetail.topic);
        LeaveLib();
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkTopic))) {
        OpenTopic(AfterPrefix(url, kLinkTopic));
        gDetail.tab = LibTab::Knows;
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkScreenTitle))) {
        Str id = AfterPrefix(url, kLinkScreenTitle);
        SumatraLaunchBrowser(fmt("https://www.imdb.com/title/%s/", id));
        return true;
    }
    if (str::StartsWith(url, Str(kLinkTab))) {
        Str which = AfterPrefix(url, kLinkTab);
        gDetail.tab = (LibTab)atoi(which.s);
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkPerson))) {
        OpenPerson(AfterPrefix(url, kLinkPerson));
        gDetail.tab = LibTab::People;
        win->homePageScrollY = 0;
        InvalidateRect(win->hwndCanvas, nullptr, FALSE);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkRescan))) {
        LibraryRefresh(win, true);
        return true;
    }
    if (str::StartsWith(url, Str(kLinkClassic))) {
        SetLibraryHomeEnabled(false);
        SaveSettings();
        win->homePageScrollY = 0;
        win->RedrawAll(true);
        return true;
    }
    return true;
}

static TempStr BookPathAtTemp(MainWindow* win, int x, int y) {
    TempStr url = GetStaticLinkAtTemp(win->staticLinks, x, y, nullptr);
    if (len(url) == 0) {
        return {};
    }
    if (str::StartsWith(url, Str(kLinkOpen))) {
        return str::DupTemp(AfterPrefix(url, kLinkOpen));
    }
    if (str::StartsWith(url, Str(kLinkBook))) {
        Str id = AfterPrefix(url, kLinkBook);
        for (int i = 0; i < gModel.nBooks; i++) {
            if (str::Eq(gModel.books[i].id, id)) {
                return str::DupTemp(gModel.books[i].path);
            }
        }
    }
    return {};
}

static TempStr RowKeyAtTemp(MainWindow* win, int x, int y) {
    TempStr url = GetStaticLinkAtTemp(win->staticLinks, x, y, nullptr);
    if (len(url) == 0) {
        return {};
    }
    if (str::StartsWith(url, Str(kLinkSeries))) {
        return str::DupTemp(AfterPrefix(url, kLinkSeries));
    }
    Str id;
    if (str::StartsWith(url, Str(kLinkBook))) {
        id = AfterPrefix(url, kLinkBook);
    } else if (str::StartsWith(url, Str(kLinkOpen))) {
        Str path = AfterPrefix(url, kLinkOpen);
        for (int i = 0; i < gModel.nBooks; i++) {
            if (str::Eq(gModel.books[i].path, path)) {
                id = gModel.books[i].id;
                break;
            }
        }
    }
    if (len(id) == 0) {
        return {};
    }
    for (int i = 0; i < gModel.nBooks; i++) {
        if (!str::Eq(gModel.books[i].id, id)) {
            continue;
        }
        Str keys = gModel.books[i].keys;
        if (len(keys) < 3) {
            return {};
        }
        const char* end = keys.s + len(keys) - 1;
        const char* start = end - 1;
        while (start > keys.s && *start != '|') {
            start--;
        }
        return str::DupTemp(Str(start + 1, (int)(end - start - 1)));
    }
    return {};
}

static LibSeries* RowByKey(Str key) {
    for (int i = 0; i < gModel.nSeries; i++) {
        if (str::Eq(gModel.series[i].key, key)) {
            return &gModel.series[i];
        }
    }
    return nullptr;
}

static void AddPartitionMenu(HMENU popup, Str rowKey) {
    LibSeries* row = RowByKey(rowKey);
    LibPartition* home = row ? PartitionByKey(row->parent) : nullptr;
    HMENU move = CreatePopupMenu();
    for (int i = 0; i < gNPartitions; i++) {
        LibPartition& p = gPartitions[i];
        if (row && str::Eq(row->key, p.key)) {
            continue;
        }
        str::Builder label;
        for (int step = 0; step < p.depth; step++) {
            label.Append("    ");
        }
        label.Append(fmt("%s  (%d)", p.name, p.books));
        uint flags = MF_STRING;
        if (home && str::Eq(home->key, p.key)) {
            flags |= MF_CHECKED;
        }
        AppendMenuW(move, flags, kMenuPartitionFirst + i, ToWStrTemp(ToStr(label)).s);
    }
    if (gNPartitions > 0) {
        AppendMenuW(move, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(move, MF_STRING, kMenuNewPartition, ToWStrTemp(StrL("New partition...")).s);
    AppendMenuW(popup, MF_POPUP, (UINT_PTR)move, ToWStrTemp(StrL("Move to partition")).s);
    if (home) {
        AppendMenuW(popup, MF_STRING, kMenuTakeOutOfPartition, ToWStrTemp(fmt("Take out of %s", home->name)).s);
    }
    if (row && str::Eq(row->kind, StrL("partition"))) {
        AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(popup, MF_STRING, kMenuRenamePartition, ToWStrTemp(StrL("Rename partition...")).s);
        AppendMenuW(popup, MF_STRING, kMenuDeletePartition, ToWStrTemp(StrL("Delete partition")).s);
    }
}

static void RunPartitionCommand(MainWindow* win, int cmd, Str rowKey) {
    LibSeries* row = RowByKey(rowKey);
    bool isPartition = row && str::Eq(row->kind, StrL("partition"));
    if (cmd == kMenuNewPartition) {
        Str name{};
        if (Dialog_PartitionName(win->hwndFrame, StrL("New partition"), StrL("&Name this partition:"), name)) {
            TempStr body = fmt("{\"name\":%s,\"row\":%s}", JsonStrTemp(name),
                               isPartition ? StrL("null") : Str(JsonStrTemp(rowKey)));
            PostPartition("/partition/new", Str(body));
        }
        str::Free(name);
        return;
    }
    if (cmd == kMenuTakeOutOfPartition && row) {
        TempStr body = fmt("{\"key\":\"\",\"row\":%s,\"out_of\":%s}", JsonStrTemp(rowKey), JsonStrTemp(row->parent));
        PostPartition("/partition/assign", Str(body));
        return;
    }
    if (cmd == kMenuRenamePartition && isPartition) {
        Str name = str::Dup(row->name);
        if (Dialog_PartitionName(win->hwndFrame, StrL("Rename partition"), StrL("&Name this partition:"), name)) {
            TempStr body = fmt("{\"key\":%s,\"name\":%s}", JsonStrTemp(rowKey), JsonStrTemp(name));
            PostPartition("/partition/rename", Str(body));
        }
        str::Free(name);
        return;
    }
    if (cmd == kMenuDeletePartition && isPartition) {
        TempStr body = fmt("{\"key\":%s}", JsonStrTemp(rowKey));
        PostPartition("/partition/delete", Str(body));
        return;
    }
    int pick = cmd - kMenuPartitionFirst;
    if (pick < 0 || pick >= gNPartitions) {
        return;
    }
    Str key = gPartitions[pick].key;
    if (isPartition) {
        TempStr body = fmt("{\"key\":%s,\"parent\":%s}", JsonStrTemp(rowKey), JsonStrTemp(key));
        PostPartition("/partition/nest", Str(body));
        return;
    }
    TempStr body = fmt("{\"key\":%s,\"row\":%s}", JsonStrTemp(key), JsonStrTemp(rowKey));
    PostPartition("/partition/assign", Str(body));
}

bool LibraryOnRightClick(MainWindow* win, int x, int y) {
    if (!LibraryHomeEnabled()) {
        return false;
    }
    TempStr path = BookPathAtTemp(win, x, y);
    TempStr rowKey = RowKeyAtTemp(win, x, y);
    if (len(path) == 0 && len(rowKey) == 0) {
        return false;
    }
    HMENU popup = CreatePopupMenu();
    if (len(path) > 0) {
        AppendMenuW(popup, MF_STRING, kMenuOpenResume, ToWStrTemp(_TRA("Open book from last page read")).s);
        AppendMenuW(popup, MF_STRING, kMenuOpenStart, ToWStrTemp(_TRA("Open book from beginning")).s);
        AppendMenuW(popup, MF_STRING, kMenuPlayAudiobook, ToWStrTemp(_TRA("Play as Audio Book")).s);
    }
    if (len(rowKey) > 0) {
        if (len(path) > 0) {
            AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        }
        AddPartitionMenu(popup, rowKey);
    }
    POINT pt = {x, y};
    MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    int cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    DestroyMenu(popup);

    if (cmd == kMenuOpenResume) {
        LibraryOpenBook(win, path, 0);
    } else if (cmd == kMenuOpenStart) {
        LibraryOpenBook(win, path, 1);
    } else if (cmd == kMenuPlayAudiobook) {
        LibraryOpenBook(win, path, 0, true);
    } else if (cmd > 0 && len(rowKey) > 0) {
        RunPartitionCommand(win, cmd, rowKey);
    }
    return true;
}
