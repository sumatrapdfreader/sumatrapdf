/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/SquareTreeParser.h"
#include "RestrictedUseConfig.h"

static const char* kRestrictedUseNames[] = {
    "InternetAccess", "DiskAccess",    "SavePreferences",  "RegistryAccess",
    "PrinterAccess",  "CopySelection", "FullscreenAccess", "AIChat",
};

static const char* kRestrictedUseDescriptions[] = {
    "Allow update checks, crash reports, and other Internet access.",
    "Allow opening and saving files, recent files, and external applications.",
    "Allow preferences, favorites, and file history to be saved.",
    "Allow registry changes, including default PDF viewer registration.",
    "Allow documents to be printed.",
    "Allow document content to be selected and copied.",
    "Allow fullscreen and presentation modes.",
    "Allow AI chat and AI translation features. Internet and disk access are also required.",
};

RestrictedUseConfig::~RestrictedUseConfig() {
    str::Free(linkProtocols);
    str::Free(safeFileTypes);
}

int RestrictedUseFeatureCount() {
    return dimofi(kRestrictedUseNames);
}

Str RestrictedUseFeatureName(int idx) {
    ReportIf(idx < 0 || idx >= RestrictedUseFeatureCount());
    return Str(kRestrictedUseNames[idx]);
}

Str RestrictedUseFeatureDescription(int idx) {
    ReportIf(idx < 0 || idx >= RestrictedUseFeatureCount());
    return Str(kRestrictedUseDescriptions[idx]);
}

void LoadRestrictedUseConfig(RestrictedUseConfig& cfg, Str data, RestrictedUseFile file) {
    str::Free(cfg.linkProtocols);
    str::Free(cfg.safeFileTypes);
    cfg.linkProtocols = {};
    cfg.safeFileTypes = {};
    cfg.hasLinkProtocols = false;
    cfg.hasSafeFileTypes = false;
    cfg.fileExists = file == RestrictedUseFile::Exists;

    if (!cfg.fileExists) {
        for (RestrictedUseItem& item : cfg.items) {
            item.enabled = true;
            item.initialEnabled = true;
            item.changed = false;
        }
        cfg.linkProtocols = str::Dup(StrL("http,https,mailto,file"));
        cfg.safeFileTypes = str::Dup(StrL("audio,video,webpage"));
        cfg.hasLinkProtocols = true;
        cfg.hasSafeFileTypes = true;
        return;
    }

    SquareTreeNode* root = ParseSquareTree(data);
    AutoDelete delRoot(root);
    SquareTreeNode* policies = root ? root->GetChild(StrL("Policies")) : nullptr;
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        Str value = policies ? policies->GetValue(RestrictedUseFeatureName(i)) : Str();
        bool enabled = value && ParseInt(value) != 0;
        cfg.items[i].enabled = enabled;
        cfg.items[i].initialEnabled = enabled;
        cfg.items[i].changed = false;
    }
    if (!policies) {
        return;
    }

    Str value = policies->GetValue(StrL("LinkProtocols"));
    if (value) {
        cfg.linkProtocols = str::Dup(value);
        cfg.hasLinkProtocols = true;
    }
    value = policies->GetValue(StrL("SafeFileTypes"));
    if (value) {
        cfg.safeFileTypes = str::Dup(value);
        cfg.hasSafeFileTypes = true;
    }
}

void UpdateRestrictedUseChanged(RestrictedUseConfig& cfg) {
    for (RestrictedUseItem& item : cfg.items) {
        item.changed = item.enabled != item.initialEnabled;
    }
}

bool RestrictedUseConfigChanged(const RestrictedUseConfig& cfg) {
    for (const RestrictedUseItem& item : cfg.items) {
        if (item.changed) {
            return true;
        }
    }
    return false;
}

TempStr SerializeRestrictedUseConfigTemp(const RestrictedUseConfig& cfg) {
    str::Builder b;
    b.Append(StrL("; SumatraPDF restricted-use configuration.\n"));
    b.Append(StrL("; Missing policies default to 0 (disabled).\n\n[Policies]\n"));
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        b.Append(fmt("%s = %d\n", RestrictedUseFeatureName(i), cfg.items[i].enabled ? 1 : 0));
    }
    if (cfg.hasLinkProtocols) {
        b.Append(fmt("LinkProtocols = %s\n", cfg.linkProtocols));
    }
    if (cfg.hasSafeFileTypes) {
        b.Append(fmt("SafeFileTypes = %s\n", cfg.safeFileTypes));
    }
    return ToStrTemp(b);
}

bool CanWriteRestrictedUseConfig() {
    TempStr configPath = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    if (file::Exists(configPath)) {
        file::FileHandle h = file::OpenReadWrite(configPath, false);
        if (h == file::kInvalidFileHandle) {
            return false;
        }
        file::Close(h);
    }

    TempStr basePath = GetPathInExeDirTemp(StrL("sumatrapdfrestrict-write-test.tmp"));
    TempStr probePath = MakeUniqueFilePathTemp(basePath);
    if (!file::WriteFile(probePath, {})) {
        return false;
    }
    return file::Delete(probePath);
}

bool SaveRestrictedUseConfig(const RestrictedUseConfig& cfg) {
    TempStr path = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    return file::WriteFile(path, SerializeRestrictedUseConfigTemp(cfg));
}

bool DeleteRestrictedUseConfig() {
    TempStr path = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    return !file::Exists(path) || file::Delete(path);
}
