/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <synctex_parser.h>
#include "base/Win.h"
#include "base/File.h"
#include "base/Zip.h"

#include "gui/UIModels.h"

#include "DocController.h"
#include "EngineBase.h"
#include "PdfSync.h"

// size of the mark highlighting the location calculated by forward-search
constexpr int kMarkSize = 10;
// maximum error in the source file line number when doing forward-search
constexpr int kEpsilonLine = 5;
// Minimal error distance^2 between a point clicked by the user and a PDF mark
constexpr int kPdfsyncEpsilonSquare = 800;
// Minimal vertical distance
constexpr int kPdfsyncEpsilonY = 20;

struct PdfsyncFileIndex {
    int start, end; // first and one-after-last index of lines associated with a file
};

struct PdfsyncLine {
    UINT record = 0; // index for mapping line(s) to point(s)
    int file = 0;    // index into srcfiles
    UINT line = 0;
    UINT column = 0;
};

struct PdfsyncPoint {
    UINT record; // index for mapping point(s) to line(s)
    UINT page, x, y;
};

// Synchronizer based on .pdfsync file generated with the pdfsync tex package
struct Pdfsync : Synchronizer {
    Pdfsync(Str syncfilename, Str pdffilename, EngineBase* engine)
        : Synchronizer(syncfilename, pdffilename), engine(engine) {
        ReportIf(!str::EndsWithI(syncfilename, StrL(".pdfsync")));
    }

    int DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) override;
    int SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) override;

    int RebuildIndexIfNeeded();
    UINT SourceToRecord(Str srcfilename, int line, int col, Vec<int>& records);

    EngineBase* engine;              // needed for converting between coordinate systems
    StrVec srcfiles;                 // source file names
    Vec<PdfsyncLine> lines;          // record-to-line mapping
    Vec<PdfsyncPoint> points;        // record-to-point mapping
    Vec<PdfsyncFileIndex> fileIndex; // start and end of entries for a file in <lines>
    Vec<int> sheetIndex;             // start of entries for a sheet in <points>
};

// Synchronizer based on .synctex file generated with SyncTex
struct SyncTex : Synchronizer {
    SyncTex(Str syncfilename, Str pdffilename, EngineBase* engineIn) : Synchronizer(syncfilename, pdffilename) {
        engine = engineIn;
        scanner = nullptr;
        ReportIf(!str::EndsWithI(syncfilename, StrL(".synctex")));
    }

    ~SyncTex() override { synctex_scanner_free(scanner); }

    int DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) override;
    int SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) override;

    int RebuildIndexIfNeeded();

    EngineBase* engine; // needed for converting between coordinate systems
    synctex_scanner_p scanner;
};

static i64 GetSyncFileTimestamp(Str path) {
    FILETIME ft = file::GetModificationTime(path);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (i64)uli.QuadPart;
}

// Modification time of whichever of the two files the index can be built from is
// newer. SyncTex::RebuildIndexIfNeeded() reads either <base>.synctex or, when only the
// compressed form exists, <base>.synctex.gz -- but Create() stores the .synctex
// path either way (synctex_parser.c insists on it). Stat'ing only the stored
// path meant that with a gzipped synctex -- what -synctex=1 produces, the
// MiKTeX/TeX Live default -- the timestamp was always 0 for a file that never
// exists, so "has it changed?" was never true and a recompile's new synctex was
// never picked up: forward search kept answering from the first compile's
// mapping for the rest of the session (issue #5040). Only a reload of the PDF
// itself, which builds a new Synchronizer, escaped it.
i64 Synchronizer::SyncFileTimestamp() const {
    i64 stamp = GetSyncFileTimestamp(syncFilePath);
    if (str::EndsWithI(syncFilePath, StrL(".synctex"))) {
        i64 gzStamp = GetSyncFileTimestamp(str::JoinTemp(syncFilePath, StrL(".gz")));
        stamp = std::max(stamp, gzStamp);
    }
    return stamp;
}

Synchronizer::Synchronizer(Str syncFilePathIn, Str pdffilename) {
    syncFilePath = str::Dup(syncFilePathIn);
    pdfPath = str::Dup(pdffilename);
    syncfileTimestamp = SyncFileTimestamp();
}

Synchronizer::~Synchronizer() {
    str::Free(syncFilePath);
    str::Free(pdfPath);
}

bool Synchronizer::NeedsToRebuildIndex() {
    // was the index manually discarded?
    if (needsToRebuildIndex) {
        return true;
    }

    // has the synchronization file been changed on disk? != rather than >: a
    // rewrite can also move the timestamp backwards (a toolchain restoring an
    // older file, a copy that preserves mtime), and that's a change too
    i64 newstamp = SyncFileTimestamp();
    if (newstamp != syncfileTimestamp) {
        // update time stamp
        syncfileTimestamp = newstamp;
        return true; // the file has changed!
    }

    return false;
}

int Synchronizer::MarkIndexWasRebuilt() {
    needsToRebuildIndex = false;
    syncfileTimestamp = SyncFileTimestamp();
    return PDFSYNCERR_SUCCESS;
}

Str Synchronizer::PrependDir(Str filename) const {
    TempStr dir = path::GetDirTemp(syncFilePath);
    return path::Join(dir, filename);
}

TempStr Synchronizer::PrependDirTemp(Str filename) const {
    TempStr dir = path::GetDirTemp(syncFilePath);
    return path::JoinTemp(dir, filename);
}

// Create a Synchronizer object for a PDF file.
// It creates either a SyncTex or PdfSync object
// based on the synchronization file found in the folder containing the PDF file.
int Synchronizer::Create(Str pdffilename, EngineBase* engine, Synchronizer** sync) {
    if (!sync || !engine) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    if (!str::EndsWithI(pdffilename, StrL(".pdf"))) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    TempStr basePath = path::GetPathNoExtTemp(pdffilename);

    // Check if a PDFSYNC file is present
    TempStr syncFile = str::JoinTemp(basePath, StrL(".pdfsync"));
    if (file::Exists(syncFile)) {
        *sync = new Pdfsync(syncFile, pdffilename, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    // check if SYNCTEX or compressed SYNCTEX file is present
    TempStr texGzFile = str::JoinTemp(basePath, StrL(".synctex.gz"));
    TempStr texFile = str::JoinTemp(basePath, StrL(".synctex"));

    if (file::Exists(texGzFile) || file::Exists(texFile)) {
        // due to a bug with synctex_parser.c, this must always be
        // the pdffilename to the .synctex file (even if a .synctex.gz file is used instead)
        *sync = new SyncTex(texFile, pdffilename, engine);
        return *sync ? PDFSYNCERR_SUCCESS : PDFSYNCERR_OUTOFMEMORY;
    }

    return PDFSYNCERR_SYNCFILE_NOTFOUND;
}

// PDFSYNC synchronizer

static int SyncLineLen(Str data, int off) {
    int end = off;
    int n = data.len;
    while (end < n && ((u8*)data.s)[end]) {
        end++;
    }
    return end - off;
}

static Str SyncLineAt(Str data, int off) {
    return Str((char*)((u8*)data.s + off), SyncLineLen(data, off));
}

// move to the next line in a list of zero-terminated lines
static int SyncAdvanceLine(Str data, int off) {
    off += SyncLineLen(data, off);
    int n = data.len;
    while (off < n && !((u8*)data.s)[off]) {
        off++;
    }
    return off < n ? off : -1;
}

// see http://itexmac.sourceforge.net/pdfsync.html for the specification
int Pdfsync::RebuildIndexIfNeeded() {
    if (!NeedsToRebuildIndex()) {
        return PDFSYNCERR_SUCCESS;
    }

    Str data = file::ReadFile(syncFilePath);
    if (len(data) == 0) {
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }

    // convert the file data into a list of zero-terminated strings
    Str blob = data;
    str::TransCharsInPlace(blob, StrL("\r\n"), StrL("\0\0"));

    // parse preamble (jobname and version marker)
    // replace star by spaces (TeX uses stars instead of spaces in filenames)
    str::TransCharsInPlace(blob, StrL("*/"), StrL(" \\"));
    TempStr jobName = strconv::AnsiToUtf8Temp(SyncLineAt(data, 0));
    jobName = str::JoinTemp(jobName, StrL(".tex"));
    jobName = PrependDirTemp(jobName);

    int lineOff = SyncAdvanceLine(data, 0);
    UINT versionNumber = 0;
    if (lineOff < 0 || str::IsNull(str::Parse(SyncLineAt(data, lineOff), "version %u", &versionNumber)) ||
        versionNumber != 1) {
        return PDFSYNCERR_SYNCFILE_CANNOT_BE_OPENED;
    }

    // reset synchronizer database
    srcfiles.Reset();
    VecReset(lines);
    VecReset(points);
    VecReset(fileIndex);
    VecReset(sheetIndex);

    Vec<int> filestack;
    int page = 1;
    VecAppend(sheetIndex, 0);

    // add the initial tex file to the source file stack
    VecAppend(filestack, len(srcfiles));
    srcfiles.Append(jobName);
    PdfsyncFileIndex findex{};
    VecAppend(fileIndex, findex);

    PdfsyncLine psline;
    PdfsyncPoint pspoint;

    // parse data
    int maxPageNo = engine->PageCount();
    while (true) {
        lineOff = SyncAdvanceLine(data, lineOff);
        if (lineOff < 0) {
            break;
        }
        Str line = SyncLineAt(data, lineOff);
        switch (line.s[0]) {
            case 'l':
                psline.file = VecLast(filestack);
                if (!str::IsNull(str::Parse(line, "l %u %u %u", &psline.record, &psline.line, &psline.column))) {
                    VecAppend(lines, psline);
                } else if (!str::IsNull(str::Parse(line, "l %u %u", &psline.record, &psline.line))) {
                    psline.column = 0;
                    VecAppend(lines, psline);
                }
                // else dbg("Bad 'l' line in the pdfsync file");
                break;

            case 's':
                if (!str::IsNull(str::Parse(line, "s %u", &page))) {
                    VecAppend(sheetIndex, len(points));
                }
                // else dbg("Bad 's' line in the pdfsync file");
                // if (0 == page || page > maxPageNo)
                //     dbg("'s' line with invalid page number in the pdfsync file");
                break;

            case 'p':
                pspoint.page = page;
                if (0 == page || page > maxPageNo) {
                    /* ignore point for invalid page number */;
                } else if (!str::IsNull(str::Parse(line, "p %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y)) ||
                           !str::IsNull(str::Parse(line, "p* %u %u %u", &pspoint.record, &pspoint.x, &pspoint.y))) {
                    VecAppend(points, pspoint);
                }
                // else dbg("Bad 'p' line in the pdfsync file");
                break;

            case '(': {
                TempStr filename = strconv::AnsiToUtf8Temp(Str(line.s + 1, line.len - 1));
                // if the filename contains quotes then remove them
                // TODO: this should never happen!?
                Str fn = filename;
                if (len(fn) > 0 && fn.s[0] == '"' && fn.s[fn.len - 1] == '"') {
                    filename = str::DupTemp(Str(fn.s + 1, fn.len - 2));
                }
                // undecorate the filepath: replace * by space and / by \ (backslash)
                str::TransCharsInPlace(filename, StrL("*/"), StrL(" \\"));
                // if the file name extension is not specified then add the suffix '.tex'
                if (len(path::GetExtTemp(filename)) == 0) {
                    filename = str::JoinTemp(filename, StrL(".tex"));
                }
                // ensure that the path is absolute
                if (!path::IsAbsolute(filename)) {
                    filename = PrependDirTemp(filename);
                }

                VecAppend(filestack, len(srcfiles));
                srcfiles.Append(filename);
                findex.start = findex.end = len(lines);
                VecAppend(fileIndex, findex);
            } break;

            case ')':
                if (len(filestack) > 1) {
                    fileIndex[VecPop(filestack)].end = len(lines);
                }
                // else dbg("Unbalanced ')' line in the pdfsync file");
                break;

            default:
                // dbg("Ignoring invalid pdfsync line starting with '%c'", *line);
                break;
        }
    }

    fileIndex[0].end = len(lines);
    ReportIf(len(filestack) != 1);

    return MarkIndexWasRebuilt();
}

// convert a coordinate from the sync file into a PDF coordinate
constexpr double kSyncToPdfCoordinateDiv = 65781.76;

static int cmpLineRecords(const void* a, const void* b) {
    return (int)((PdfsyncLine*)a)->record - (int)((PdfsyncLine*)b)->record;
}

// If `srcfilepath` doesn't exist on disk, checks whether it's been moved to sit
// next to the PDF document (which happens if all files are moved together)
static void TryRecoverMovedSourceFile(Str& srcfilepath, Str pdfPath) {
    if (file::Exists(srcfilepath)) {
        return;
    }
    TempStr altsrcpath = path::GetDirTemp(pdfPath);
    altsrcpath = path::JoinTemp(altsrcpath, path::GetBaseNameTemp(srcfilepath));
    if (!str::Eq(altsrcpath, srcfilepath) && file::Exists(altsrcpath)) {
        str::ReplaceWithCopy(&srcfilepath, altsrcpath);
    }
}

int Pdfsync::DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) {
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        return res;
    }

    // find the entry in the index corresponding to this page
    int nPages = engine->PageCount();
    if (pageNo == 0 || pageNo >= len(sheetIndex) || pageNo > nPages) {
        return PDFSYNCERR_INVALID_PAGE_NUMBER;
    }

    // PdfSync coordinates are y-inversed
    Rect mbox = engine->PageMediabox(pageNo).Round();
    pt.y = mbox.dy - pt.y;

    // distance to the closest pdf location (in the range <kPdfsyncEpsilonSquare)
    UINT closest_xydist = UINT_MAX;
    UINT selected_record = UINT_MAX;
    // If no record is found within a distance^2 of kPdfsyncEpsilonSquare
    // (selected_record == -1) then we pick up the record that is closest
    // vertically to the hit-point.
    UINT closest_ydist = UINT_MAX;        // vertical distance between the hit point and the vertically-closest record
    UINT closest_xdist = UINT_MAX;        // horizontal distance between the hit point and the vertically-closest record
    UINT closest_ydist_record = UINT_MAX; // vertically-closest record

    // read all the sections of 'p' declarations for this pdf sheet
    for (int i = sheetIndex[pageNo]; i < len(points) && points[i].page == (uint)pageNo; i++) {
        // check whether it is closer than the closest point found so far
        UINT dx = abs(pt.x - (int)(points[i].x / kSyncToPdfCoordinateDiv));
        UINT dy = abs(pt.y - (int)(points[i].y / kSyncToPdfCoordinateDiv));
        UINT dist = (dx * dx) + (dy * dy);
        if (dist < kPdfsyncEpsilonSquare && dist < closest_xydist) {
            selected_record = points[i].record;
            closest_xydist = dist;
        } else if ((closest_xydist == UINT_MAX) && dy < kPdfsyncEpsilonY &&
                   (dy < closest_ydist || (dy == closest_ydist && dx < closest_xdist))) {
            closest_ydist_record = points[i].record;
            closest_ydist = dy;
            closest_xdist = dx;
        }
    }

    if (selected_record == UINT_MAX) {
        selected_record = closest_ydist_record;
    }
    if (selected_record == UINT_MAX) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION; // no record was found close enough to the hit point
    }

    // We have a record number, we need to find its declaration ('l ...') in the syncfile
    PdfsyncLine cmp;
    cmp.record = selected_record;
    PdfsyncLine* found = (PdfsyncLine*)bsearch(&cmp, VecData(lines), len(lines), sizeof(PdfsyncLine), cmpLineRecords);
    ReportIf(!found);
    if (!found) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    Str path = srcfiles[found->file];
    str::ReplaceWithCopy(&filename, path::NormalizeTemp(path));
    TryRecoverMovedSourceFile(filename, pdfPath);

    *line = (int)found->line;
    *col = (int)found->column;
    *col = std::max(*col, 0);

    return PDFSYNCERR_SUCCESS;
}

// Find a record corresponding to the given source file, line number and optionally column number.
// (at the moment the column parameter is ignored)
//
// If there are several *consecutively declared* records for the same line then they are all returned.
// The list of records is added to the vector 'records'
//
// If there is no record for that line, the record corresponding to the nearest line is selected
// (within a range of kEpsilonLine)
//
// The function returns PDFSYNCERR_SUCCESS if a matching record was found.
UINT Pdfsync::SourceToRecord(Str srcfilename, int line, int /*col*/, Vec<int>& records) {
    if (len(srcfilename) == 0) {
        return PDFSYNCERR_INVALID_ARGUMENT;
    }

    // convert the source file to an absolute path
    TempStr srcfilepath = path::IsAbsolute(srcfilename) ? srcfilename : PrependDirTemp(srcfilename);
    if (len(srcfilepath) == 0) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    // find the source file entry
    int isrc;
    for (isrc = 0; isrc < len(srcfiles); isrc++) {
        Str path = srcfiles[isrc];
        if (path::IsSame(srcfilepath, path)) {
            break;
        }
    }
    if (isrc == len(srcfiles)) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }

    if (fileIndex[isrc].start == fileIndex[isrc].end) {
        return PDFSYNCERR_NORECORD_IN_SOURCEFILE; // there is not any record declaration for that particular source file
    }

    // look for sections belonging to the specified file
    // starting with the first section that is declared within the scope of the file.
    UINT min_distance = kEpsilonLine; // distance to the closest record
    int lineIx = -1;                  // closest record-line index

    for (int isec = fileIndex[isrc].start; isec < fileIndex[isrc].end; isec++) {
        // does this section belong to the desired file?
        if (lines[isec].file != isrc) {
            continue;
        }

        UINT d = abs((int)lines[isec].line - line);
        if (d < min_distance) {
            min_distance = d;
            lineIx = isec;
            if (0 == d) {
                break; // We have found a record for the requested line!
            }
        }
    }
    if (lineIx < 0) {
        return PDFSYNCERR_NORECORD_FOR_THATLINE;
    }

    // we read all the consecutive records until we reach a record belonging to another line
    for (int i = lineIx; i < len(lines) && lines[i].line == lines[lineIx].line; i++) {
        VecAppend(records, (int)lines[i].record);
    }

    return PDFSYNCERR_SUCCESS;
}

int Pdfsync::SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) {
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        return res;
    }

    Vec<int> found_records;
    UINT ret = SourceToRecord(srcfilename, line, col, found_records);
    if (ret != PDFSYNCERR_SUCCESS || len(found_records) == 0) {
        return (int)ret;
    }

    VecReset(rects);

    // records have been found for the desired source position:
    // we now find the page and positions in the PDF corresponding to these found records
    int firstPage = UINT_MAX;
    for (PdfsyncPoint& p : points) {
        if (!VecContains(found_records, (int)p.record)) {
            continue;
        }
        if (firstPage != UINT_MAX && firstPage != (int)p.page) {
            continue;
        }
        firstPage = *page = (int)p.page;
        RectF rc((float)(p.x / kSyncToPdfCoordinateDiv), (float)(p.y / kSyncToPdfCoordinateDiv), kMarkSize, kMarkSize);
        // PdfSync coordinates are y-inversed
        RectF mbox = engine->PageMediabox(firstPage);
        rc.y = mbox.dy - (rc.y + rc.dy);
        VecAppend(rects, rc.Round());
    }

    if (len(rects) > 0) {
        return PDFSYNCERR_SUCCESS;
    }
    // the record does not correspond to any point in the PDF: this is possible...
    return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
}

static bool PathHasNonAscii(Str s) {
    if (len(s) == 0) {
        return false;
    }
    for (int i = 0; i < s.len; i++) {
        if ((u8)s.s[i] > 127) {
            return true;
        }
    }
    return false;
}

/**
 * Convert a string encoded in the local character page (system ANSI code page) to UTF-8 encoding.
 *
 * @param localStr  A null-terminated string encoded in the local character page
 * @return          A heap-allocated UTF-8 string (caller must free via str::Free), or empty on failure
 */
static Str ConvertLocalToUTF8(Str localStr) {
    if (len(localStr) == 0) {
        return {};
    }
    UINT acp = GetACP();
    int wLen = MultiByteToWideChar(acp, MB_ERR_INVALID_CHARS, localStr.s, -1, nullptr, 0);
    if (wLen == 0) {
        return {};
    }
    WCHAR* wBuf = AllocArrayTemp<WCHAR>(wLen);
    if (!wBuf) {
        return {};
    }
    if (MultiByteToWideChar(acp, MB_ERR_INVALID_CHARS, localStr.s, -1, wBuf, wLen) == 0) {
        return {};
    }
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wBuf, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len == 0) {
        return {};
    }
    char* utf8Buf = (char*)malloc(utf8Len);
    if (!utf8Buf) {
        return {};
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wBuf, -1, utf8Buf, utf8Len, nullptr, nullptr) == 0) {
        free(utf8Buf);
        return {};
    }
    return Str(utf8Buf, utf8Len - 1);
}

static TempStr CopyPlainSyncToTempFile(TempStr pathSync) {
    if (len(pathSync) == 0) {
        return {};
    }
    // use file::ReadFile which uses CreateFileW (handles Unicode)
    Str data = file::ReadFile(pathSync);
    if (len(data) == 0) {
        logf("CopyPlainSyncToTempFile: source file '.synctex' '%s' is empty.\n", pathSync);
        // return {};
    }
    TempStr tempPath = GetTempFilePathTemp(StrL("stx")); // stxabcdef.tmp
    if (len(tempPath) == 0) {
        str::Free(data);
        logf("CopyPlainSyncToTempFile: unable to get temp file path. error: %d.\n", errno);
        return {};
    }
    bool ok = file::WriteFile(tempPath, data);
    str::Free(data);
    if (!ok) {
        logf("CopyPlainSyncToTempFile: unable to write temp file '%s'. error: %d.\n", tempPath, errno);
        return {};
    }

    TempStr tempPathNoExt = path::GetPathNoExtTemp(tempPath);              // stxabcdef
    TempStr tempPathSync = str::JoinTemp(tempPathNoExt, StrL(".synctex")); // stxabcdef.synctex
    int ret = rename(tempPath.s, tempPathSync.s);
    if (ret) {
        logf("CopyPlainSyncToTempFile: unable rename from '%s' to '%s'. error: %d.\n", tempPath, tempPathSync, errno);
        return {};
    }

    logf("CopyPlainSyncToTempFile: copied '%s' to '%s'\n", pathSync, tempPathSync);
    return tempPathSync;
}

static TempStr DealPlainSync(TempStr pathSync) {
    if (len(pathSync) == 0) {
        return {};
    }
    Str src = file::ReadFile(pathSync);
    if (len(src) == 0) {
        logf("DealPlainSync: '%s' failed\n", pathSync);
        return {};
    }
    TempStr srcZ = str::DupTemp(src);
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, srcZ.s, -1, nullptr, 0);
    if (wlen != 0) {
        logf("DealPlainSync: '%s' is utf-8 (created by lualatex)\n", pathSync);
        return pathSync;
    }
    logf("DealPlainSync: '%s' NOT utf-8, decode by local ansi and write utf-8 to temp file\n", pathSync);
    Str converted = ConvertLocalToUTF8(srcZ);
    if (len(converted) == 0) {
        logf("DealPlainSync: unable to convert '%s' from local ansi to utf-8.\n", pathSync);
        return {};
    }
    Str dst = converted;

    if (len(dst) == 0) {
        logf("DealPlainSync: decoded content is empty.\n", pathSync);
        return {};
    }
    TempStr tempPath = GetTempFilePathTemp(StrL("stx")); // stxabcdef.tmp
    if (len(tempPath) == 0) {
        str::Free(dst);
        logf("DealPlainSync: unable to get temp file path. error: %d.\n", errno);
        return {};
    }
    bool ok = file::WriteFile(tempPath, dst);
    str::Free(dst);
    if (!ok) {
        logf("DealPlainSync: unable to write temp file '%s'. error: %d.\n", tempPath, errno);
        return {};
    }
    logf("DealPlainSync: utf-8 written to temp file '%s'.\n", tempPath);

    TempStr tempPathNoExt = path::GetPathNoExtTemp(tempPath);              // stxabcdef
    TempStr tempPathSync = str::JoinTemp(tempPathNoExt, StrL(".synctex")); // stxabcdef.synctex
    int ret = rename(tempPath.s, tempPathSync.s);
    if (ret) {
        logf("DealPlainSync: unable rename from '%s' to '%s'. error: %d.\n", tempPath, tempPathSync, errno);
        return {};
    }
    logf("DealPlainSync: copied '%s' to '%s'\n", pathSync, tempPathSync);
    return tempPathSync;
}

static bool IsGzipFile(Str path) {
    // gzip files start with magic bytes 0x1f 0x8b; only need to read the header
    u8 buf[2] = {0};
    int nRead = file::ReadN(path, buf, sizeof(buf));
    return nRead == 2 && buf[0] == 0x1f && buf[1] == 0x8b;
}

// returns path of ungzipped file
static TempStr ungzipToTempSync(Str gzPath) {
    if (len(gzPath) == 0) {
        return {};
    }
    Str compr = file::ReadFile(gzPath);
    if (len(compr) == 0) {
        logf("ungzipToTempSync: file::ReadFile() '%s' failed\n", gzPath);
        return {};
    }
    logf("ungzipToTempSync: file::ReadFile() did read '%s'\n", gzPath);
    constexpr int kMaxSyncTexSize = 256 * 1024 * 1024;
    int expansionLimit = (int)std::min((i64)kMaxSyncTexSize, (i64)len(compr) * 1000);
    Str uncompr = Ungzip(compr, expansionLimit);
    if (len(uncompr) == 0) {
        str::Free(compr);
        return {};
    }

    TempStr tempPath = GetTempFilePathTemp(StrL("stx")); // stxabcdef.tmp
    if (len(tempPath) == 0) {
        str::Free(uncompr);
        logf("ungzipToTempSync: unable to get temp file path. error: %d.\n", errno);
        return {};
    }
    bool ok = file::WriteFile(tempPath, uncompr);
    str::Free(uncompr);
    if (!ok) {
        logf("ungzipToTempSync: unable to write temp file '%s'. error: %d.\n", tempPath, errno);
        return {};
    }

    TempStr tempPathNoExt = path::GetPathNoExtTemp(tempPath);              // stxabcdef
    TempStr tempPathSync = str::JoinTemp(tempPathNoExt, StrL(".synctex")); // stxabcdef.synctex
    int ret = rename(tempPath.s, tempPathSync.s);
    if (ret) {
        logf("ungzipToTempSync: unable rename from '%s' to '%s'. error: %d.\n", tempPath, tempPathSync, errno);
        return {};
    }

    logf("ungzipToTempSync: ungzip '%s' to '%s'\n", gzPath, tempPathSync);
    return tempPathSync;
}

// SYNCTEX synchronizer
int SyncTex::RebuildIndexIfNeeded() {
    if (!NeedsToRebuildIndex()) {
        logf("SyncTex::RebuildIndexIfNeeded: no need to rebuild\n");
        return PDFSYNCERR_SUCCESS;
    }
    synctex_scanner_free(scanner);
    scanner = nullptr;
    TempStr pathBase;   //  abc
    TempStr pathSync;   //  abc.synctex
    TempStr pathSyncGz; //  abc.synctex.gz
    pathSync = str::DupTemp(syncFilePath);
    pathBase = path::GetPathNoExtTemp(syncFilePath);
    pathSyncGz = str::JoinTemp(pathBase, StrL(".synctex.gz"));

    i64 fsize;
    bool path_nonascii = PathHasNonAscii(pathSync);

    TempStr tempsync1;
    TempStr tempsync2;
    if (!path_nonascii) {
        // Only ASCII
        if (file::Exists(pathSync)) {
            if (IsGzipFile(pathSync)) {
                // --synctex=NUMBER with NUMBER&2: gzip data in a .synctex file
                tempsync1 = ungzipToTempSync(pathSync);
                tempsync2 = DealPlainSync(tempsync1);
            } else {
                tempsync2 = DealPlainSync(pathSync);
            }
        } else if (file::Exists(pathSyncGz)) {
            tempsync1 = ungzipToTempSync(pathSyncGz);
            tempsync2 = DealPlainSync(tempsync1);
        } else {
            return PDFSYNCERR_SYNCFILE_NOTFOUND;
        }
    } else {
        // ANSI in file path
        if (file::Exists(pathSync)) {
            if (IsGzipFile(pathSync)) {
                tempsync1 = ungzipToTempSync(pathSync);
                tempsync2 = DealPlainSync(tempsync1);
            } else {
                tempsync1 = CopyPlainSyncToTempFile(pathSync);
                tempsync2 = DealPlainSync(tempsync1);
            }
        } else if (file::Exists(pathSyncGz)) {
            tempsync1 = ungzipToTempSync(pathSyncGz);
            tempsync2 = DealPlainSync(tempsync1);
        } else {
            return PDFSYNCERR_SYNCFILE_NOTFOUND;
        }
    }
    logf("[dbg]: tempsync1: %s\n", tempsync1 ? tempsync1 : StrL("[NULL]"));
    logf("[dbg]: tempsync2: %s\n", tempsync2 ? tempsync2 : StrL("[NULL]"));
    if (len(tempsync2) == 0) {
        logf("SyncTex::RebuildIndexIfNeeded: temp file for origin file '%s' not found\n", pathSync);
        return PDFSYNCERR_SYNCFILE_NOTFOUND;
    }
    fsize = file::GetSize(tempsync2);
    logf("SyncTex::RebuildIndexIfNeeded: org path: %s\n; final file path: %s, final file size: %lld.\n", pathSync,
         tempsync2, fsize);

    scanner = synctex_scanner_new_with_output_file(CStrTemp(tempsync2), nullptr, 1);
    if (scanner) {
        logf("SyncTex::RebuildIndexIfNeeded: file '%s' is ok.\n", pathSync);
    } else {
        return PDFSYNCERR_SYNCFILE_NOTFOUND;
    }
    return MarkIndexWasRebuilt();
}

// Decides whether `resolvedSrcPath` should be treated as a Unix path rather
// than a Windows path.
//
// True if the sync file lives on WSL AND the resolved path is not itself a
// WSL UNC path -- i.e. the project was compiled by a Linux/WSL toolchain
// (which records plain Unix paths).
//
// Also true if the resolved path is a WSL mount path (e.g. /mnt/c/...),
// which happens when the PDF lives on a Windows drive but was compiled from
// inside WSL
static bool IsUnixSourcePath(Str syncFilePath, Str resolvedSrcPath) {
    if (len(syncFilePath) == 0 || len(resolvedSrcPath) == 0) {
        return false;
    }

    if (path::IsWslUnc(syncFilePath) && !path::IsWslUnc(resolvedSrcPath)) {
        return true;
    }

    return path::IsWslMount(resolvedSrcPath);
}

int SyncTex::DocToSource(int pageNo, Point pt, Str& filename, int* line, int* col) {
    logf("SyncTex::DocToSource: '%s', pageNo: %d\n", syncFilePath, pageNo);
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        ReportDebugIf(true);
        return res;
    }
    ReportIf(!scanner);

    // Coverity: at this point, this->scanner->flags.has_parsed == 1 and thus
    // synctex_scanner_parse never gets the chance to freeing the scanner
    if (synctex_edit_query(scanner, pageNo, (float)pt.x, (float)pt.y) <= 0) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    synctex_node_p node = synctex_scanner_next_result(this->scanner);
    if (!node) {
        return PDFSYNCERR_NO_SYNC_AT_LOCATION;
    }

    Str name = Str(synctex_scanner_get_name(this->scanner, synctex_node_tag(node)));
    if (len(name) == 0) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }

    // The name comes verbatim from the .synctex file, which travels with the PDF
    // and is entirely attacker-controlled. It ends up on the command line of the
    // configured inverse-search editor (BuildOpenFileCmdTemp), so a name like
    //   foo.tex" --install-extension evil.vsix --
    // breaks out of the editor template's quotes and injects arguments
    // (GHSA-jf4v-rw66-j4w2). None of these characters can occur in a real
    // Windows path, so a name that has one is not a source file we should act
    // on. A '*' is legitimate here - SyncTeX encodes spaces as '*'.
    if (str::ContainsCharAny(name, StrL("\"<>|\r\n\t"))) {
        logf("SyncTex::DocToSource: rejecting source name with an illegal char: '%s'\n", name);
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }

    filename = str::Dup(name);
    if (len(filename) == 0) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    // Unescape SyncTeX's space encoding: * represents a space in filenames
    str::TransCharsInPlace(filename, StrL("*"), StrL(" "));

    if (IsUnixSourcePath(syncFilePath, filename)) {
        // Treat filename as unix path

        // Resolve relative Unix paths relative to the sync file's directory
        if (filename.s[0] != '/') {
            TempStr unixSyncFilePath = path::WslUncToUnixTemp(syncFilePath);
            TempStr dir = path::GetDirTemp(unixSyncFilePath);
            Str joined = path::Join(dir, filename);
            str::Free(filename);
            filename = joined;
        }
    } else {
        // Treat filename as Windows path

        str::TransCharsInPlace(filename, StrL("/"), StrL("\\"));
        // Convert the source filepath to an absolute path
        if (!path::IsAbsolute(filename)) {
            Str abs = PrependDir(filename);
            str::Free(filename);
            filename = abs;
        }
        str::ReplaceWithCopy(&filename, path::NormalizeTemp(filename));
        TryRecoverMovedSourceFile(filename, pdfPath);
    }

    *line = synctex_node_line(node);
    *col = synctex_node_column(node);
    *col = std::max(*col, 0);

    return PDFSYNCERR_SUCCESS;
}

static int SynctexDisplayQueryWithVariants(synctex_scanner_p scanner, Str srcPath, int line, int col) {
    int ret = synctex_display_query(scanner, CStrTemp(srcPath), line, col, 0);
    if (ret > 0) {
        return ret;
    }

    TempStr variants[] = {
        path::WslUncToUnixTemp(srcPath),
        path::WindowsToWslMountTemp(srcPath),
    };
    for (TempStr variant : variants) {
        if (len(variant) == 0) {
            continue;
        }
        logf("SynctexDisplayQueryWithVariants: '%s' failed, retrying with '%s'\n", srcPath, variant);
        int ret2 = synctex_display_query(scanner, CStrTemp(variant), line, col, 0);
        if (ret2 > 0) {
            return ret2;
        }
    }
    return ret;
}

int SyncTex::SourceToDoc(Str srcfilename, int line, int col, int* page, Vec<Rect>& rects) {
    logf("SyncTex::SourceToDoc: '%s', line: %d, col: %d\n", srcfilename, line, col);
    int res = RebuildIndexIfNeeded();
    if (res != PDFSYNCERR_SUCCESS) {
        return res;
    }
    ReportIf(!scanner);

    TempStr srcfilepath = srcfilename;
    // convert the source file to an absolute path
    if (!path::IsAbsolute(srcfilename)) {
        srcfilepath = PrependDir(srcfilename);
    }
    if (len(srcfilepath) == 0) {
        return PDFSYNCERR_OUTOFMEMORY;
    }

    int ret = SynctexDisplayQueryWithVariants(this->scanner, srcfilepath, line, col);

    if (-1 == ret) {
        return PDFSYNCERR_UNKNOWN_SOURCEFILE;
    }
    if (0 == ret) {
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }

    synctex_node_p node;
    int firstpage = -1;
    VecReset(rects);

    while ((node = synctex_scanner_next_result(this->scanner)) != nullptr) {
        if (firstpage == -1) {
            firstpage = synctex_node_page(node);
            if (firstpage <= 0 || firstpage > engine->PageCount()) {
                continue;
            }
            *page = (int)(UINT)firstpage;
        }
        if (synctex_node_page(node) != firstpage) {
            continue;
        }

        RectF rc;
        rc.x = synctex_node_box_visible_h(node);
        rc.y = (float)((double)synctex_node_box_visible_v(node) - (double)synctex_node_box_visible_height(node));
        rc.dx = synctex_node_box_visible_width(node),
        rc.dy = (float)((double)synctex_node_box_visible_height(node) + (double)synctex_node_box_visible_depth(node));
        VecAppend(rects, rc.Round());
    }

    if (firstpage <= 0) {
        return PDFSYNCERR_NOSYNCPOINT_FOR_LINERECORD;
    }
    return PDFSYNCERR_SUCCESS;
}
