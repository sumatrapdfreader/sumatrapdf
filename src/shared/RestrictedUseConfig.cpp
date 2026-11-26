/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/SquareTreeParser.h"
#include "Permissions.h"
#include "RestrictedUseConfig.h"

static const struct {
    Perm permission;
    Str name;
    Str description;
} kRestrictedUseFeatures[] = {
    {Perm::InternetAccess, StrL("InternetAccess"),
     StrL("Allow update checks, crash reports, and other Internet access.")},
    {Perm::DiskAccess, StrL("DiskAccess"),
     StrL("Allow opening and saving files, recent files, and external applications.")},
    {Perm::SavePreferences, StrL("SavePreferences"),
     StrL("Allow preferences, favorites, and file history to be saved.")},
    {Perm::RegistryAccess, StrL("RegistryAccess"),
     StrL("Allow registry changes, including default PDF viewer registration.")},
    {Perm::PrinterAccess, StrL("PrinterAccess"), StrL("Allow documents to be printed.")},
    {Perm::CopySelection, StrL("CopySelection"), StrL("Allow document content to be selected and copied.")},
    {Perm::FullscreenAccess, StrL("FullscreenAccess"), StrL("Allow fullscreen and presentation modes.")},
    {Perm::AIChat, StrL("AIChat"),
     StrL("Allow AI chat and AI translation features. Internet and disk access are also required.")},
    {Perm::CrashReports, StrL("CrashReports"),
     StrL("Allow automatic crash and diagnostic report uploads. Internet access is also required.")},
};

static_assert(dimof(kRestrictedUseFeatures) == kRestrictedUseFeatureCount);

RestrictedUseConfig::~RestrictedUseConfig() {
    str::Free(linkProtocols);
    str::Free(safeFileTypes);
}

int RestrictedUseFeatureCount() {
    return dimofi(kRestrictedUseFeatures);
}

Perm RestrictedUseFeaturePerm(int idx) {
    if (ReportIf(idx < 0 || idx >= RestrictedUseFeatureCount())) {
        return (Perm)0;
    }
    return kRestrictedUseFeatures[idx].permission;
}

Str RestrictedUseFeatureName(int idx) {
    if (ReportIf(idx < 0 || idx >= RestrictedUseFeatureCount())) {
        return {};
    }
    return kRestrictedUseFeatures[idx].name;
}

Str RestrictedUseFeatureDescription(int idx) {
    if (ReportIf(idx < 0 || idx >= RestrictedUseFeatureCount())) {
        return {};
    }
    return kRestrictedUseFeatures[idx].description;
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
