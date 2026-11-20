/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Http.h"
#include "gui/UIModels.h"

#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DropUrlCommon.h"

#include "SumatraLog.h"

// A link dropped on the canvas (a browser's image): downloaded to the
// Downloads folder and opened. Shared by orig's OLE drop target and ng's.

static void AdvanceUrlPathUntilSuffix(Str& p, Str& lastSlash) {
    while (len(p) > 0 && p.s[0] != '?' && p.s[0] != '#') {
        if (p.s[0] == '/') {
            lastSlash = p;
        }
        p.s++;
        p.len--;
    }
}

// Extract a file name from a URL (last path component, without query/fragment)
TempStr FileNameFromUrlTemp(Str url) {
    // skip past scheme
    Str path = url;
    Str slash = str::SliceFromChar(url, '/');
    if (slash) {
        path = slash;
        if (path.len >= 2 && path.s[0] == '/' && path.s[1] == '/') {
            path.s += 2;
            path.len -= 2;
        }
    }
    // find last '/' before any '?' or '#'
    Str lastSlash;
    Str p = path;
    AdvanceUrlPathUntilSuffix(p, lastSlash);
    if (len(lastSlash) == 0) {
        return {};
    }
    int nameLen = (int)(p.s - lastSlash.s - 1);
    if (nameLen <= 0) {
        return {};
    }
    return str::DupTemp(Str(lastSlash.s + 1, nameLen));
}

void DownloadAndOpenUrl(DownloadAndOpenUrlData* data) {
    Str url = data->url;

    TempStr downloadsDir = GetDownloadsDirTemp();
    if (len(downloadsDir) == 0) {
        logf("DownloadAndOpenUrl: failed to get Downloads folder\n");
        str::Free(data->url);
        delete data;
        return;
    }

    TempStr fileName = FileNameFromUrlTemp(url);
    if (len(fileName) == 0 || str::Eq(fileName, StrL(".")) || str::Eq(fileName, StrL("..")) ||
        str::Contains(fileName, StrL("/")) || str::Contains(fileName, StrL("\\")) ||
        str::Contains(fileName, StrL(":"))) {
        // generate a fallback name
        fileName = str::DupTemp(StrL("dropped_image.png"));
    }

    TempStr destPath = path::JoinTemp(downloadsDir, fileName);

    // avoid overwriting: if file exists, add a numeric suffix
    if (file::Exists(destPath)) {
        TempStr ext = path::GetExtTemp(destPath);
        TempStr base = str::DupTemp(Str(fileName.s, len(fileName) - len(ext)));
        for (int i = 1; i < 1000; i++) {
            TempStr newName = fmt("%s_%d%s", base, i, ext);
            destPath = path::JoinTemp(downloadsDir, newName);
            if (!file::Exists(destPath)) {
                break;
            }
        }
    }

    logf("DownloadAndOpenUrl: downloading '%s' to '%s'\n", url, destPath);

    Func1<HttpProgress*> emptyProgress;
    bool ok = HttpGetToFile(url, destPath, emptyProgress);
    if (!ok) {
        logf("DownloadAndOpenUrl: download failed for '%s'\n", url);
        str::Free(data->url);
        delete data;
        return;
    }

    // verify the downloaded file is a supported image type
    FileType kind = GuessFileTypeFromFile(destPath);
    if (!IsEngineImageSupportedFileType(kind)) {
        logf("DownloadAndOpenUrl: downloaded file is not a supported image type: '%s'\n", destPath);
        file::Delete(destPath);
        str::Free(data->url);
        delete data;
        return;
    }

    // ensure it has a good extension, some urls are like:
    // https://pbs.twimg.com/media/HEwit7bbQAAWiIO?format=jpg&name=large
    TempStr ext = GetExtForFileTypeTemp(kind);
    if (!str::EndsWithI(destPath, ext)) {
        TempStr newDest = str::JoinTemp(destPath, ext);
        ok = file::Rename(newDest, destPath);
        if (ok) {
            destPath = newDest;
        }
    }

    // open the file on the UI thread
    auto* pathDup = new Str(str::Dup(destPath));
    auto fn = MkFunc0<Str>(OpenDownloadedPath, pathDup);
    uitask::Post(fn, "DownloadAndOpenUrl");

    str::Free(data->url);
    delete data;
}
