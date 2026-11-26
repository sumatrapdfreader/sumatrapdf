/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr const char* kRestrictionsFileName = "sumatrapdfrestrict.ini";
constexpr const char* kRestrictedUseDocsUrl = "https://www.sumatrapdfreader.org/docs/Configure-for-restricted-use";

// Number of named policies in sumatrapdfrestrict.ini.
constexpr int kRestrictedUseFeatureCount = 9;

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
enum class Perm : uint;
Perm RestrictedUseFeaturePerm(int idx);
Str RestrictedUseFeatureName(int idx);
Str RestrictedUseFeatureDescription(int idx);
void LoadRestrictedUseConfig(RestrictedUseConfig& cfg, Str data, RestrictedUseFile file);
void UpdateRestrictedUseChanged(RestrictedUseConfig& cfg);
bool RestrictedUseConfigChanged(const RestrictedUseConfig& cfg);
TempStr SerializeRestrictedUseConfigTemp(const RestrictedUseConfig& cfg);
bool CanWriteRestrictedUseConfig();
bool SaveRestrictedUseConfig(const RestrictedUseConfig& cfg);
bool DeleteRestrictedUseConfig();
