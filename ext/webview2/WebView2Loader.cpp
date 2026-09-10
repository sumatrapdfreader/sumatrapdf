// Replacement for WebView2LoaderStatic.lib from the Microsoft.Web.WebView2
// NuGet package. It locates the installed WebView2 runtime and calls into
// EmbeddedBrowserWebView.dll directly.
// Derived from the loader in wry (extras/wry in kjk/gpui-kit-cpp-dist).

#include <windows.h>
#include <objbase.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "WebView2.h"

namespace {

const uint32_t kMinRuntimeVersion[4] = {86, 0, 616, 0};

// bit masks match ICoreWebView2EnvironmentOptions7::get_ReleaseChannels
const int kAllReleaseChannels = 15;

// runtimeType passed to CreateWebViewEnvironmentWithOptionsInternal
const int kRuntimeTypeInstalled = 0;
const int kRuntimeTypeFixed = 1;

WCHAR* WStrDup(const WCHAR* s) {
    if (!s) {
        return nullptr;
    }
    size_t n = wcslen(s) + 1;
    WCHAR* res = (WCHAR*)malloc(n * sizeof(WCHAR));
    if (res) {
        memcpy(res, s, n * sizeof(WCHAR));
    }
    return res;
}

WCHAR* CoTaskWStrDup(const WCHAR* s) {
    size_t n = wcslen(s) + 1;
    WCHAR* res = (WCHAR*)CoTaskMemAlloc(n * sizeof(WCHAR));
    if (res) {
        memcpy(res, s, n * sizeof(WCHAR));
    }
    return res;
}

WCHAR* EnvironmentVariableDup(const WCHAR* name) {
    DWORD cap = GetEnvironmentVariableW(name, nullptr, 0);
    if (cap == 0) {
        return nullptr;
    }
    WCHAR* value = (WCHAR*)malloc((size_t)cap * sizeof(WCHAR));
    if (!value) {
        return nullptr;
    }
    DWORD len = GetEnvironmentVariableW(name, value, cap);
    if (len == 0 || len >= cap) {
        free(value);
        return nullptr;
    }
    return value;
}

// policy overrides are looked up per app user model id, then per exe name,
// then for all apps
int LoaderOverrideIds(WCHAR appId[256], WCHAR exeName[MAX_PATH], const WCHAR* ids[3]) {
    appId[0] = 0;
    exeName[0] = 0;

    typedef LONG(WINAPI * GetCurrentApplicationUserModelIdFn)(UINT32*, PWSTR);
    auto getAppUserModelId =
        (GetCurrentApplicationUserModelIdFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                                           "GetCurrentApplicationUserModelId");
    if (getAppUserModelId) {
        UINT32 len = 256;
        if (getAppUserModelId(&len, appId) != ERROR_SUCCESS) {
            appId[0] = 0;
        }
    }

    if (appId[0] == 0) {
        typedef HRESULT(WINAPI * GetExplicitAppUserModelIdFn)(PWSTR*);
        HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
        auto getExplicit =
            shell32 ? (GetExplicitAppUserModelIdFn)GetProcAddress(
                          shell32, "GetCurrentProcessExplicitAppUserModelID")
                    : nullptr;
        PWSTR explicitId = nullptr;
        if (getExplicit && SUCCEEDED(getExplicit(&explicitId)) && explicitId) {
            wcscpy_s(appId, 256, explicitId);
        }
        CoTaskMemFree(explicitId);
    }

    WCHAR module[MAX_PATH * 2];
    DWORD len = GetModuleFileNameW(nullptr, module, (DWORD)_countof(module));
    if (len > 0 && len < _countof(module)) {
        const WCHAR* slash = wcsrchr(module, L'\\');
        wcscpy_s(exeName, MAX_PATH, slash ? slash + 1 : module);
    }

    int count = 0;
    if (appId[0] != 0) {
        ids[count++] = appId;
    }
    if (exeName[0] != 0) {
        ids[count++] = exeName;
    }
    ids[count++] = L"*";
    return count;
}

WCHAR* RegistryValueDup(HKEY root, const WCHAR* keyPath, const WCHAR* valueName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, keyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return nullptr;
    }

    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes);
    WCHAR* result = nullptr;

    if (status == ERROR_SUCCESS && type == REG_SZ && bytes >= sizeof(WCHAR)) {
        result = (WCHAR*)malloc((size_t)bytes + sizeof(WCHAR));
        if (result &&
            RegQueryValueExW(key, valueName, nullptr, &type, (BYTE*)result, &bytes) == ERROR_SUCCESS) {
            result[bytes / sizeof(WCHAR)] = 0;
        } else {
            free(result);
            result = nullptr;
        }
    } else if (status == ERROR_SUCCESS && type == REG_DWORD && bytes == sizeof(DWORD)) {
        DWORD value = 0;
        if (RegQueryValueExW(key, valueName, nullptr, &type, (BYTE*)&value, &bytes) == ERROR_SUCCESS) {
            result = (WCHAR*)malloc(16 * sizeof(WCHAR));
            if (result) {
                swprintf_s(result, 16, L"%u", value);
            }
        }
    }

    RegCloseKey(key);
    return result;
}

WCHAR* PolicyOverrideDup(const WCHAR* property) {
    WCHAR appId[256];
    WCHAR exeName[MAX_PATH];
    const WCHAR* ids[3];
    int idCount = LoaderOverrideIds(appId, exeName, ids);
    const HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};

    for (int r = 0; r < (int)_countof(roots); r++) {
        WCHAR key[MAX_PATH];
        swprintf_s(key, L"Software\\Policies\\Microsoft\\Edge\\WebView2\\%s", property);
        for (int i = 0; i < idCount; i++) {
            WCHAR* result = RegistryValueDup(roots[r], key, ids[i]);
            if (result) {
                return result;
            }
        }
    }

    for (int r = 0; r < (int)_countof(roots); r++) {
        for (int i = 0; i < idCount; i++) {
            WCHAR key[MAX_PATH];
            swprintf_s(key, L"Software\\Policies\\Microsoft\\EmbeddedBrowserWebView\\LoaderOverride\\%s",
                       ids[i]);
            WCHAR* result = RegistryValueDup(roots[r], key, property);
            if (result) {
                return result;
            }
        }
    }
    return nullptr;
}

WCHAR* LoaderOverrideDup(const WCHAR* environment, const WCHAR* property) {
    WCHAR* result = EnvironmentVariableDup(environment);
    if (!result) {
        result = PolicyOverrideDup(property);
    }
    if (result && result[0] == 0) {
        free(result);
        result = nullptr;
    }
    return result;
}

struct RuntimeChannel {
    const WCHAR* id;
    const WCHAR* name;
    const WCHAR* packageFamily;
    int mask;
};

const RuntimeChannel kRuntimeChannels[] = {
    {L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", L"", L"Microsoft.WebView2Runtime.Stable_8wekyb3d8bbwe", 1},
    {L"{2CD8A007-E189-409D-A2C8-9AF4EF3C72AA}", L"beta", L"Microsoft.WebView2Runtime.Beta_8wekyb3d8bbwe", 2},
    {L"{0D50BFEC-CD6A-4F9A-964C-C7416E3ACB10}", L"dev", L"Microsoft.WebView2Runtime.Dev_8wekyb3d8bbwe", 4},
    {L"{65C35B14-6C1D-4122-AC46-7148CC9D6497}", L"canary", L"Microsoft.WebView2Runtime.Canary_8wekyb3d8bbwe", 8},
};

struct RuntimeInfo {
    WCHAR version[64];
    WCHAR clientDll[MAX_PATH * 2];
    int runtimeType;
};

bool RegReadStr(HKEY root, const WCHAR* subKey, const WCHAR* name, DWORD extraFlags, WCHAR* out,
                DWORD outChars) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_QUERY_VALUE | extraFlags, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD cb = outChars * sizeof(WCHAR);
    LSTATUS st = RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)out, &cb);
    RegCloseKey(key);

    if (st != ERROR_SUCCESS || type != REG_SZ || cb < sizeof(WCHAR)) {
        return false;
    }
    out[(cb / sizeof(WCHAR)) - 1] = 0;
    return out[0] != 0;
}

bool ParseVersion(const WCHAR* text, uint32_t version[4]) {
    if (!text) {
        return false;
    }
    for (int part = 0; part < 4; part++) {
        if (*text < L'0' || *text > L'9') {
            return false;
        }
        uint32_t value = 0;
        do {
            uint32_t digit = (uint32_t)(*text - L'0');
            if (value > (UINT32_MAX - digit) / 10) {
                return false;
            }
            value = value * 10 + digit;
            text++;
        } while (*text >= L'0' && *text <= L'9');
        version[part] = value;
        if (part < 3) {
            if (*text != L'.') {
                return false;
            }
            text++;
        }
    }
    return *text == 0;
}

bool IsCompatibleRuntime(const WCHAR* versionText) {
    uint32_t version[4];
    if (!ParseVersion(versionText, version)) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        if (version[i] != kMinRuntimeVersion[i]) {
            return version[i] > kMinRuntimeVersion[i];
        }
    }
    return true;
}

const WCHAR* ArchFolder() {
#if defined(_M_ARM64)
    return L"arm64";
#elif defined(_M_X64) || defined(__x86_64__)
    return L"x64";
#else
    return L"x86";
#endif
}

bool IsAbsolutePath(const WCHAR* path) {
    if (!path) {
        return false;
    }
    bool drive = path[0] != 0 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
    bool unc = (path[0] == L'\\' || path[0] == L'/') && (path[1] == L'\\' || path[1] == L'/');
    return drive || unc;
}

// a relative fixed-runtime folder is relative to the exe
WCHAR* FixedFolderDup(const WCHAR* folder) {
    if (IsAbsolutePath(folder)) {
        return WStrDup(folder);
    }

    WCHAR module[MAX_PATH * 2];
    DWORD len = GetModuleFileNameW(nullptr, module, (DWORD)_countof(module));
    if (len == 0 || len >= _countof(module)) {
        return nullptr;
    }
    WCHAR* slash = wcsrchr(module, L'\\');
    if (!slash) {
        return nullptr;
    }

    size_t prefixLen = (size_t)(slash - module) + 1;
    size_t folderLen = wcslen(folder);
    WCHAR* result = (WCHAR*)malloc((prefixLen + folderLen + 1) * sizeof(WCHAR));
    if (!result) {
        return nullptr;
    }
    memcpy(result, module, prefixLen * sizeof(WCHAR));
    memcpy(result + prefixLen, folder, (folderLen + 1) * sizeof(WCHAR));
    return result;
}

bool FileVersion(const WCHAR* path, WCHAR* out, int outChars) {
    typedef DWORD(WINAPI * GetFileVersionInfoSizeWFn)(LPCWSTR, LPDWORD);
    typedef BOOL(WINAPI * GetFileVersionInfoWFn)(LPCWSTR, DWORD, DWORD, LPVOID);
    typedef BOOL(WINAPI * VerQueryValueWFn)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

    HMODULE versionDll = LoadLibraryW(L"version.dll");
    if (!versionDll) {
        return false;
    }
    auto getSize = (GetFileVersionInfoSizeWFn)GetProcAddress(versionDll, "GetFileVersionInfoSizeW");
    auto getInfo = (GetFileVersionInfoWFn)GetProcAddress(versionDll, "GetFileVersionInfoW");
    auto query = (VerQueryValueWFn)GetProcAddress(versionDll, "VerQueryValueW");

    bool ok = false;
    if (getSize && getInfo && query) {
        DWORD ignored = 0;
        DWORD size = getSize(path, &ignored);
        uint8_t* data = size > 0 ? (uint8_t*)malloc(size) : nullptr;
        if (data && getInfo(path, 0, size, data)) {
            VS_FIXEDFILEINFO* info = nullptr;
            UINT infoSize = 0;
            if (query(data, L"\\", (void**)&info, &infoSize) && info && infoSize >= sizeof(*info) &&
                info->dwSignature == VS_FFI_SIGNATURE) {
                int n = swprintf_s(out, (size_t)outChars, L"%u.%u.%u.%u",
                                   HIWORD(info->dwProductVersionMS), LOWORD(info->dwProductVersionMS),
                                   HIWORD(info->dwProductVersionLS), LOWORD(info->dwProductVersionLS));
                ok = n > 0;
            }
        }
        free(data);
    }

    FreeLibrary(versionDll);
    return ok;
}

bool SetClientDll(RuntimeInfo* out, const WCHAR* folder) {
    int written = swprintf_s(out->clientDll, L"%s\\EBWebView\\%s\\EmbeddedBrowserWebView.dll", folder,
                             ArchFolder());
    if (written < 0) {
        return false;
    }
    return GetFileAttributesW(out->clientDll) != INVALID_FILE_ATTRIBUTES;
}

void AppendChannelName(RuntimeInfo* out, const RuntimeChannel* channel) {
    if (out->version[0] == 0 || channel->name[0] == 0) {
        return;
    }
    wcscat_s(out->version, L" ");
    wcscat_s(out->version, channel->name);
}

bool RuntimeVersionAndLocation(const WCHAR* id, WCHAR* version, DWORD versionChars, WCHAR* location,
                               DWORD locationChars) {
    struct Where {
        HKEY root;
        DWORD flags;
    };
    const Where places[] = {
        {HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY},
        {HKEY_CURRENT_USER, 0},
    };

    for (int i = 0; i < (int)_countof(places); i++) {
        WCHAR key[256];
        wcscpy_s(key, L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\");
        wcscat_s(key, id);

        if (!RegReadStr(places[i].root, key, L"pv", places[i].flags, version, versionChars)) {
            continue;
        }
        if (wcscmp(version, L"0.0.0.0") == 0) {
            continue;
        }
        if (RegReadStr(places[i].root, key, L"location", places[i].flags, location, locationChars)) {
            return true;
        }
    }
    return false;
}

// the runtime can also ship as an MSIX package the app takes a dependency on
bool FindPackagedRuntime(const RuntimeChannel* channel, RuntimeInfo* out) {
    typedef LONG(WINAPI * TryCreatePackageDependencyFn)(PSID, PCWSTR, uint64_t, int, int, PCWSTR, int,
                                                        PWSTR*);
    typedef LONG(WINAPI * AddPackageDependencyFn)(PCWSTR, int, int, void**, PWSTR*);
    typedef LONG(WINAPI * GetPackagePathByFullNameFn)(PCWSTR, UINT32*, PWSTR);

    HMODULE kernelBase = GetModuleHandleW(L"kernelbase.dll");
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE apiModule = kernelBase ? kernelBase : kernel32;

    auto tryCreate =
        apiModule ? (TryCreatePackageDependencyFn)GetProcAddress(apiModule, "TryCreatePackageDependency")
                  : nullptr;
    auto add = apiModule ? (AddPackageDependencyFn)GetProcAddress(apiModule, "AddPackageDependency")
                         : nullptr;
    auto getPath =
        kernel32 ? (GetPackagePathByFullNameFn)GetProcAddress(kernel32, "GetPackagePathByFullName")
                 : nullptr;
    if (!tryCreate || !add || !getPath) {
        return false;
    }

    PWSTR dependencyId = nullptr;
    LONG status = tryCreate(nullptr, channel->packageFamily, 0, 0, 0, nullptr, 0, &dependencyId);
    if (status != ERROR_SUCCESS || !dependencyId) {
        return false;
    }

    void* dependencyContext = nullptr;
    PWSTR packageFullName = nullptr;
    status = add(dependencyId, 0, 0, &dependencyContext, &packageFullName);
    HeapFree(GetProcessHeap(), 0, dependencyId);
    if (status != ERROR_SUCCESS || !packageFullName) {
        HeapFree(GetProcessHeap(), 0, packageFullName);
        return false;
    }

    UINT32 pathChars = 0;
    status = getPath(packageFullName, &pathChars, nullptr);
    WCHAR* packagePath = nullptr;
    if (status == ERROR_INSUFFICIENT_BUFFER && pathChars > 0) {
        packagePath = (WCHAR*)malloc((size_t)pathChars * sizeof(WCHAR));
        if (packagePath) {
            status = getPath(packageFullName, &pathChars, packagePath);
        }
    }
    HeapFree(GetProcessHeap(), 0, packageFullName);
    if (!packagePath || status != ERROR_SUCCESS) {
        free(packagePath);
        return false;
    }

    bool ok = SetClientDll(out, packagePath);
    free(packagePath);
    if (!ok) {
        return false;
    }
    if (!FileVersion(out->clientDll, out->version, (int)_countof(out->version)) ||
        !IsCompatibleRuntime(out->version)) {
        return false;
    }

    AppendChannelName(out, channel);
    return true;
}

bool FindInstalledRuntime(const RuntimeChannel* channel, RuntimeInfo* out) {
    const HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};

    WCHAR key[256];
    wcscpy_s(key, L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\");
    wcscat_s(key, channel->id);

    for (int i = 0; i < (int)_countof(roots); i++) {
        WCHAR folder[MAX_PATH * 2];
        if (!RegReadStr(roots[i], key, L"EBWebView", KEY_WOW64_32KEY, folder, (DWORD)_countof(folder))) {
            continue;
        }

        const WCHAR* version = wcsrchr(folder, L'\\');
        version = version ? version + 1 : folder;
        if (!IsCompatibleRuntime(version)) {
            continue;
        }
        if (!SetClientDll(out, folder)) {
            continue;
        }

        wcscpy_s(out->version, version);
        AppendChannelName(out, channel);
        return true;
    }

    WCHAR location[MAX_PATH * 2];
    WCHAR version[64];
    if (!RuntimeVersionAndLocation(channel->id, version, (DWORD)_countof(version), location,
                                   (DWORD)_countof(location)) ||
        !IsCompatibleRuntime(version)) {
        return FindPackagedRuntime(channel, out);
    }

    // an EdgeUpdate-registered runtime lives in <location>\<version>
    WCHAR versionedFolder[MAX_PATH * 2];
    int written = swprintf_s(versionedFolder, L"%s\\%s", location, version);
    if (written < 0 || !SetClientDll(out, versionedFolder)) {
        return FindPackagedRuntime(channel, out);
    }

    wcscpy_s(out->version, version);
    AppendChannelName(out, channel);
    return true;
}

int ReleaseChannelsFromEnv(int fallback) {
    WCHAR* value = LoaderOverrideDup(L"WEBVIEW2_RELEASE_CHANNELS", L"ReleaseChannels");
    if (!value) {
        return fallback;
    }

    int result = 0;
    WCHAR* at = value;
    for (;;) {
        WCHAR* end = nullptr;
        long channel = wcstol(at, &end, 10);
        if (end == at || channel < 0 || channel > 3) {
            channel = 0;
        }
        result |= 1 << channel;

        if (!end || *end == 0) {
            break;
        }
        WCHAR* comma = wcschr(end, L',');
        if (!comma) {
            break;
        }
        at = comma + 1;
    }

    free(value);
    return result;
}

bool LeastStableFromEnv(bool fallback) {
    WCHAR* legacy = LoaderOverrideDup(L"WEBVIEW2_RELEASE_CHANNEL_PREFERENCE", L"ReleaseChannelPreference");
    if (legacy) {
        fallback = wcstol(legacy, nullptr, 10) == 1;
        free(legacy);
    }

    WCHAR* value = LoaderOverrideDup(L"WEBVIEW2_CHANNEL_SEARCH_KIND", L"ChannelSearchKind");
    if (value) {
        fallback = wcstol(value, nullptr, 10) == 1;
        free(value);
    }
    return fallback;
}

bool FindFixedRuntime(const WCHAR* folder, RuntimeInfo* out) {
    WCHAR* resolved = FixedFolderDup(folder);
    if (!resolved) {
        return false;
    }

    out->runtimeType = kRuntimeTypeFixed;
    bool ok = SetClientDll(out, resolved);
    free(resolved);
    if (!ok) {
        return false;
    }

    FileVersion(out->clientDll, out->version, (int)_countof(out->version));
    return true;
}

bool FindRuntime(RuntimeInfo* out, const WCHAR* browserExecutableFolder = nullptr,
                 IUnknown* options = nullptr, bool useOverrides = true) {
    out->version[0] = 0;
    out->clientDll[0] = 0;
    out->runtimeType = kRuntimeTypeInstalled;

    if (browserExecutableFolder && browserExecutableFolder[0] != 0) {
        return FindFixedRuntime(browserExecutableFolder, out);
    }

    WCHAR* folder =
        useOverrides ? LoaderOverrideDup(L"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER", L"BrowserExecutableFolder")
                     : nullptr;
    if (folder) {
        bool ok = FindFixedRuntime(folder, out);
        free(folder);
        return ok;
    }

    int releaseChannels = kAllReleaseChannels;
    bool leastStable = false;

    ICoreWebView2EnvironmentOptions7* options7 = nullptr;
    if (options &&
        SUCCEEDED(options->QueryInterface(__uuidof(ICoreWebView2EnvironmentOptions7), (void**)&options7))) {
        COREWEBVIEW2_CHANNEL_SEARCH_KIND searchKind = COREWEBVIEW2_CHANNEL_SEARCH_KIND_MOST_STABLE;
        if (SUCCEEDED(options7->get_ChannelSearchKind(&searchKind))) {
            leastStable = searchKind == COREWEBVIEW2_CHANNEL_SEARCH_KIND_LEAST_STABLE;
        }
        COREWEBVIEW2_RELEASE_CHANNELS channels = (COREWEBVIEW2_RELEASE_CHANNELS)0;
        if (SUCCEEDED(options7->get_ReleaseChannels(&channels))) {
            releaseChannels = (int)channels;
        }
        options7->Release();
    }
    if (releaseChannels == 0) {
        releaseChannels = kAllReleaseChannels;
    }
    if (useOverrides) {
        releaseChannels = ReleaseChannelsFromEnv(releaseChannels);
        leastStable = LeastStableFromEnv(leastStable);
    }

    int count = (int)_countof(kRuntimeChannels);
    for (int step = 0; step < count; step++) {
        int i = leastStable ? count - step - 1 : step;
        if ((releaseChannels & kRuntimeChannels[i].mask) == 0) {
            continue;
        }
        if (FindInstalledRuntime(&kRuntimeChannels[i], out)) {
            return true;
        }
    }
    return false;
}

typedef HRESULT(STDMETHODCALLTYPE* CreateEnvironmentInternalFn)(
    BOOL fromClientDll, int runtimeType, PCWSTR userDataFolder, IUnknown* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler);

// the first creation attempt can fail while the runtime is being updated
// underneath us, so retry once through the same client dll
struct RetryHandler : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    LONG refs = 1;
    CreateEnvironmentInternalFn create = nullptr;
    int runtimeType = kRuntimeTypeInstalled;
    WCHAR* userDataFolder = nullptr;
    IUnknown* options = nullptr;
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* original = nullptr;
    int retries = 1;

    ~RetryHandler() {
        free(userDataFolder);
        if (options) {
            options->Release();
        }
        if (original) {
            original->Release();
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown ||
            riid == __uuidof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*)this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return (ULONG)InterlockedIncrement(&refs);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        LONG left = InterlockedDecrement(&refs);
        if (left == 0) {
            delete this;
        }
        return (ULONG)left;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* environment) override {
        if (SUCCEEDED(result) || retries <= 0) {
            return original->Invoke(result, environment);
        }
        retries--;

        HRESULT hr = create(TRUE, runtimeType, userDataFolder, options, this);
        if (FAILED(hr)) {
            return original->Invoke(hr, nullptr);
        }
        return S_OK;
    }
};

void DefaultUserDataFolder(WCHAR* out, int cap) {
    out[0] = 0;
    DWORD n = GetModuleFileNameW(nullptr, out, (DWORD)cap);
    if (n == 0 || n >= (DWORD)cap - 16) {
        out[0] = 0;
        return;
    }
    wcscat_s(out, (size_t)cap, L".WebView2");
}

} // namespace

STDAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR browserExecutableFolder, PCWSTR userDataFolder,
                                                ICoreWebView2EnvironmentOptions* environmentOptions,
                                                ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*
                                                    environmentCreatedHandler) {
    if (!environmentCreatedHandler) {
        return E_POINTER;
    }

    WCHAR* userDataOverride = LoaderOverrideDup(L"WEBVIEW2_USER_DATA_FOLDER", L"UserDataFolder");
    if (userDataOverride) {
        userDataFolder = userDataOverride;
    }
    WCHAR defaultFolder[MAX_PATH * 2];
    if (!userDataFolder || userDataFolder[0] == 0) {
        DefaultUserDataFolder(defaultFolder, (int)_countof(defaultFolder));
        userDataFolder = defaultFolder;
    }

    RuntimeInfo rt;
    if (!FindRuntime(&rt, browserExecutableFolder, environmentOptions)) {
        free(userDataOverride);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    HMODULE client = LoadLibraryW(rt.clientDll);
    if (!client) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        free(userDataOverride);
        return hr;
    }

    auto create =
        (CreateEnvironmentInternalFn)GetProcAddress(client, "CreateWebViewEnvironmentWithOptionsInternal");
    if (!create) {
        FreeLibrary(client);
        free(userDataOverride);
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    }

    RetryHandler* retry = new RetryHandler();
    retry->create = create;
    retry->runtimeType = rt.runtimeType;
    retry->userDataFolder = WStrDup(userDataFolder);
    retry->options = environmentOptions;
    retry->original = environmentCreatedHandler;
    if (environmentOptions) {
        environmentOptions->AddRef();
    }
    environmentCreatedHandler->AddRef();

    HRESULT hr = retry->userDataFolder
                     ? create(TRUE, rt.runtimeType, retry->userDataFolder, environmentOptions, retry)
                     : E_OUTOFMEMORY;
    retry->Release();

    // the client dll only supports unloading when it says so
    if (GetProcAddress(client, "DllCanUnloadNow")) {
        FreeLibrary(client);
    }
    free(userDataOverride);
    return hr;
}

STDAPI CreateCoreWebView2Environment(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler) {
    return CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, environmentCreatedHandler);
}

STDAPI GetAvailableCoreWebView2BrowserVersionStringWithOptions(
    PCWSTR browserExecutableFolder, ICoreWebView2EnvironmentOptions* environmentOptions,
    LPWSTR* versionInfo) {
    if (!versionInfo) {
        return E_POINTER;
    }
    *versionInfo = nullptr;

    RuntimeInfo rt;
    if (!FindRuntime(&rt, browserExecutableFolder, environmentOptions) || rt.version[0] == 0) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    *versionInfo = CoTaskWStrDup(rt.version);
    return *versionInfo ? S_OK : E_OUTOFMEMORY;
}

STDAPI GetAvailableCoreWebView2BrowserVersionString(PCWSTR browserExecutableFolder, LPWSTR* versionInfo) {
    return GetAvailableCoreWebView2BrowserVersionStringWithOptions(browserExecutableFolder, nullptr,
                                                                   versionInfo);
}

// versions can carry a channel suffix ("88.0.705.50 beta"); a less stable
// channel sorts before a more stable one at the same version
STDAPI CompareBrowserVersions(PCWSTR version1, PCWSTR version2, int* result) {
    if (!version1 || !version2 || !result) {
        return E_POINTER;
    }

    auto split = [](PCWSTR text, uint32_t version[4], int* channel) -> bool {
        WCHAR buf[64];
        if (wcslen(text) >= _countof(buf)) {
            return false;
        }
        wcscpy_s(buf, text);
        WCHAR* space = wcschr(buf, L' ');
        *channel = 0;
        if (space) {
            *space = 0;
            const WCHAR* name = space + 1;
            for (int i = 0; i < (int)_countof(kRuntimeChannels); i++) {
                if (wcscmp(name, kRuntimeChannels[i].name) == 0) {
                    *channel = i;
                    break;
                }
            }
        }
        return ParseVersion(buf, version);
    };

    uint32_t v1[4];
    uint32_t v2[4];
    int channel1 = 0;
    int channel2 = 0;
    if (!split(version1, v1, &channel1) || !split(version2, v2, &channel2)) {
        return E_INVALIDARG;
    }

    for (int i = 0; i < 4; i++) {
        if (v1[i] != v2[i]) {
            *result = v1[i] < v2[i] ? -1 : 1;
            return S_OK;
        }
    }
    *result = channel1 == channel2 ? 0 : (channel1 > channel2 ? -1 : 1);
    return S_OK;
}
