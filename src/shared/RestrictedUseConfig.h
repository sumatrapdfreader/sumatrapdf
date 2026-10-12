/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";
constexpr const char* kRestrictedUseDocsUrl = "https://www.sumatrapdfreader.org/docs/Configure-for-restricted-use";

struct RestrictedUseItem {
    bool enabled = false;
    bool initialEnabled = false;
    bool changed = false;
};

struct RestrictedUseConfig {
    RestrictedUseItem items[8];
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
