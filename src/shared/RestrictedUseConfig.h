/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";
constexpr const char* kRestrictedUseDocsUrl = "https://www.sumatrapdfreader.org/docs/Configure-for-restricted-use";

// policies in sumatrapdfrestrict.ini; the editor's tables and the app's Perm
// list (SumatraPDFCommon.cpp) must each have exactly this many, in this order
constexpr int kRestrictedUseFeatureCount = 8;

struct RestrictedUseItem {
    bool enabled = false;
    bool initialEnabled = false;
    bool changed = false;
};

struct RestrictedUseConfig {
    RestrictedUseItem items[kRestrictedUseFeatureCount];
    Str linkProtocols;
    Str safeFileTypes;
    bool hasLinkProtocols = false;
    bool hasSafeFileTypes = false;
    bool fileExists = false;

    ~RestrictedUseConfig();
};

enum class RestrictedUseFile {
    Missing,
    Exists,
};

int RestrictedUseFeatureCount();
Str RestrictedUseFeatureName(int idx);
Str RestrictedUseFeatureDescription(int idx);
void LoadRestrictedUseConfig(RestrictedUseConfig& cfg, Str data, RestrictedUseFile file);
void UpdateRestrictedUseChanged(RestrictedUseConfig& cfg);
bool RestrictedUseConfigChanged(const RestrictedUseConfig& cfg);
TempStr SerializeRestrictedUseConfigTemp(const RestrictedUseConfig& cfg);
bool CanWriteRestrictedUseConfig();
bool SaveRestrictedUseConfig(const RestrictedUseConfig& cfg);
bool DeleteRestrictedUseConfig();
