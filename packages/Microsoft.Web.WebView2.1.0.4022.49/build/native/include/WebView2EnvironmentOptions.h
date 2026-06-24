// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __core_webview2_environment_options_h__
#define __core_webview2_environment_options_h__

// Adding the definition of this macro to enable unsafe buffer usage
#ifndef UNSAFE_BUFFERS
#if defined(__clang__)

// clang-format off
#define UNSAFE_BUFFERS(...)                  \
  _Pragma("clang unsafe_buffer_usage begin") \
  __VA_ARGS__                                \
  _Pragma("clang unsafe_buffer_usage end")
// clang-format on

#else
#define UNSAFE_BUFFERS(...) __VA_ARGS__
#endif
#endif

#include <objbase.h>

#include <wrl/implements.h>

#include <algorithm>
#include <limits>
#include <string>

#include "WebView2.h"
#define CORE_WEBVIEW_TARGET_PRODUCT_VERSION L"149.0.4022.49"

#define COREWEBVIEW2ENVIRONMENTOPTIONS_STRING_PROPERTY(p)     \
 public:                                                      \
  HRESULT STDMETHODCALLTYPE get_##p(LPWSTR* value) override { \
    if (!value)                                               \
      return E_POINTER;                                       \
    *value = m_##p.Copy();                                    \
    if ((*value == nullptr) && (m_##p.Get() != nullptr))      \
      return HRESULT_FROM_WIN32(GetLastError());              \
    return S_OK;                                              \
  }                                                           \
  HRESULT STDMETHODCALLTYPE put_##p(LPCWSTR value) override { \
    LPCWSTR result = m_##p.Set(value);                        \
    if ((result == nullptr) && (value != nullptr))            \
      return HRESULT_FROM_WIN32(GetLastError());              \
    return S_OK;                                              \
  }                                                           \
                                                              \
 protected:                                                   \
  AutoCoMemString m_##p;

#define COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(p, defPVal) \
 public:                                                         \
  HRESULT STDMETHODCALLTYPE get_##p(BOOL* value) override {      \
    if (!value)                                                  \
      return E_POINTER;                                          \
    *value = m_##p;                                              \
    return S_OK;                                                 \
  }                                                              \
  HRESULT STDMETHODCALLTYPE put_##p(BOOL value) override {       \
    m_##p = value;                                               \
    return S_OK;                                                 \
  }                                                              \
                                                                 \
 protected:                                                      \
  BOOL m_##p = defPVal ? TRUE : FALSE;

#define DEFINE_AUTO_COMEM_STRING()                                            \
 protected:                                                                   \
  class AutoCoMemString {                                                     \
   public:                                                                    \
    AutoCoMemString() {}                                                      \
    ~AutoCoMemString() {                                                      \
      Release();                                                              \
    }                                                                         \
    void Release() {                                                          \
      if (m_string) {                                                         \
        deallocate_fn(m_string);                                              \
        m_string = nullptr;                                                   \
      }                                                                       \
    }                                                                         \
                                                                              \
    LPCWSTR Set(LPCWSTR str) {                                                \
      Release();                                                              \
      if (str) {                                                              \
        m_string = MakeCoMemString(str);                                      \
      }                                                                       \
      return m_string;                                                        \
    }                                                                         \
    LPCWSTR Get() {                                                           \
      return m_string;                                                        \
    }                                                                         \
    LPWSTR Copy() {                                                           \
      if (m_string)                                                           \
        return MakeCoMemString(m_string);                                     \
      return nullptr;                                                         \
    }                                                                         \
                                                                              \
   protected:                                                                 \
    LPWSTR MakeCoMemString(LPCWSTR source) {                                  \
      const size_t length = wcslen(source);                                   \
      if (length >= (std::numeric_limits<size_t>::max)() / sizeof(*source)) { \
        return nullptr;                                                       \
      }                                                                       \
                                                                              \
      const size_t bytes = (length + 1) * sizeof(*source);                    \
      wchar_t* result = reinterpret_cast<wchar_t*>(allocate_fn(bytes));       \
                                                                              \
      if (result)                                                             \
        std::char_traits<wchar_t>::copy(result, source, length + 1);          \
      return result;                                                          \
    }                                                                         \
    LPWSTR m_string = nullptr;                                                \
  };

template <typename allocate_fn_t,
          allocate_fn_t allocate_fn,
          typename deallocate_fn_t,
          deallocate_fn_t deallocate_fn>
class CoreWebView2CustomSchemeRegistrationBase
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2CustomSchemeRegistration> {
 public:
  CoreWebView2CustomSchemeRegistrationBase(LPCWSTR schemeName) {
    m_schemeName.Set(schemeName);
  }

  CoreWebView2CustomSchemeRegistrationBase(
      CoreWebView2CustomSchemeRegistrationBase&&) = default;
  ~CoreWebView2CustomSchemeRegistrationBase() { ReleaseAllowedOrigins(); }

  HRESULT STDMETHODCALLTYPE get_SchemeName(LPWSTR* schemeName) override {
    if (!schemeName)
      return E_POINTER;
    *schemeName = m_schemeName.Copy();
    if ((*schemeName == nullptr) && (m_schemeName.Get() != nullptr))
      return HRESULT_FROM_WIN32(GetLastError());
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetAllowedOrigins(UINT32* allowedOriginsCount,
                    LPWSTR** allowedOrigins) override {
    if (!allowedOrigins || !allowedOriginsCount) {
      return E_POINTER;
    }
    *allowedOriginsCount = 0;
    if (m_allowedOriginsCount == 0) {
      *allowedOrigins = nullptr;
      return S_OK;
    } else {
      *allowedOrigins = reinterpret_cast<LPWSTR*>(
          allocate_fn(m_allowedOriginsCount * sizeof(LPWSTR)));
      if (!(*allowedOrigins)) {
        return HRESULT_FROM_WIN32(GetLastError());
      }
      std::fill_n((*allowedOrigins), m_allowedOriginsCount,
                  static_cast<LPWSTR>(nullptr));
      for (UINT32 i = 0; i < m_allowedOriginsCount; i++) {
        UNSAFE_BUFFERS((*allowedOrigins)[i] = m_allowedOrigins[i].Copy();
                       if (!(*allowedOrigins)[i]) {
                         HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
                         for (UINT32 j = 0; j < i; j++) {
                           deallocate_fn((*allowedOrigins)[j]);
                         }
                         deallocate_fn(*allowedOrigins);
                         return hr;
                       })
      }
      *allowedOriginsCount = m_allowedOriginsCount;
      return S_OK;
    }
  }

  HRESULT STDMETHODCALLTYPE
  SetAllowedOrigins(UINT32 allowedOriginsCount,
                    LPCWSTR* allowedOrigins) override {
    ReleaseAllowedOrigins();
    if (allowedOriginsCount == 0) {
      return S_OK;
    } else {
      m_allowedOrigins = new AutoCoMemString[allowedOriginsCount];
      if (!m_allowedOrigins) {
        return HRESULT_FROM_WIN32(GetLastError());
      }
      for (UINT32 i = 0; i < allowedOriginsCount; i++) {
        UNSAFE_BUFFERS(m_allowedOrigins[i].Set(allowedOrigins[i]);
                       if (!m_allowedOrigins[i].Get()) {
                         HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
                         for (UINT32 j = 0; j < i; j++) {
                           m_allowedOrigins[j].Release();
                         }
                         m_allowedOriginsCount = 0;
                         delete[] (m_allowedOrigins);
                         return hr;
                       })
      }
      m_allowedOriginsCount = allowedOriginsCount;
      return S_OK;
    }
  }

 protected:
  DEFINE_AUTO_COMEM_STRING()

  void ReleaseAllowedOrigins() {
    if (m_allowedOrigins) {
      delete[] (m_allowedOrigins);
      m_allowedOrigins = nullptr;
    }
  }

  AutoCoMemString m_schemeName;
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(TreatAsSecure, false)
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(HasAuthorityComponent, false)

  // WebView2EnvironmentOptions.h this is a publicly exposed header to clients
  // and it used by them to integrate the webview2 enviorment as published at
  // https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2environmentoptions?view=webview2-1.0.3179.45
  // This file is not included in msedge.dll/lib/exe and is limited to WebView2
  // SDK which does not use partition alloc. So need to use raw_ptr_exclusion
  // here.
#if defined(__has_attribute)
  __attribute__((annotate("raw_ptr_exclusion")))
#endif
  AutoCoMemString* m_allowedOrigins = nullptr;
  unsigned int m_allowedOriginsCount = 0;
};

// This is a base COM class that implements ICoreWebView2EnvironmentOptions.
template <typename allocate_fn_t,
          allocate_fn_t allocate_fn,
          typename deallocate_fn_t,
          deallocate_fn_t deallocate_fn>
class CoreWebView2EnvironmentOptionsBase
    : public Microsoft::WRL::Implements<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2EnvironmentOptions,
          ICoreWebView2EnvironmentOptions2,
          ICoreWebView2EnvironmentOptions3,
          ICoreWebView2EnvironmentOptions4,
          ICoreWebView2EnvironmentOptions5,
          ICoreWebView2EnvironmentOptions6,
          ICoreWebView2EnvironmentOptions7,
          ICoreWebView2EnvironmentOptions8> {
 public:
  static const COREWEBVIEW2_RELEASE_CHANNELS kAllChannels =
      COREWEBVIEW2_RELEASE_CHANNELS_STABLE |
      COREWEBVIEW2_RELEASE_CHANNELS_BETA | COREWEBVIEW2_RELEASE_CHANNELS_DEV |
      COREWEBVIEW2_RELEASE_CHANNELS_CANARY;

  CoreWebView2EnvironmentOptionsBase() {
    // Initialize the target compatible browser version value to the version
    // of the browser binaries corresponding to this version of the SDK.
    m_TargetCompatibleBrowserVersion.Set(CORE_WEBVIEW_TARGET_PRODUCT_VERSION);
  }

  // ICoreWebView2EnvironmentOptions7
  HRESULT STDMETHODCALLTYPE
  get_ReleaseChannels(COREWEBVIEW2_RELEASE_CHANNELS* channels) override {
    if (!channels) {
      return E_POINTER;
    }
    *channels = m_releaseChannels;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  put_ReleaseChannels(COREWEBVIEW2_RELEASE_CHANNELS channels) override {
    m_releaseChannels = channels;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  get_ChannelSearchKind(COREWEBVIEW2_CHANNEL_SEARCH_KIND* value) override {
    if (!value) {
      return E_POINTER;
    }
    *value = m_channelSearchKind;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  put_ChannelSearchKind(COREWEBVIEW2_CHANNEL_SEARCH_KIND value) override {
    m_channelSearchKind = value;
    return S_OK;
  }

  // ICoreWebView2EnvironmentOptions8
  HRESULT STDMETHODCALLTYPE
  get_ScrollBarStyle(COREWEBVIEW2_SCROLLBAR_STYLE* style) override {
    if (!style) {
      return E_POINTER;
    }
    *style = m_scrollbarStyle;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  put_ScrollBarStyle(COREWEBVIEW2_SCROLLBAR_STYLE style) override {
    m_scrollbarStyle = style;
    return S_OK;
  }

 protected:
  ~CoreWebView2EnvironmentOptionsBase() { ReleaseCustomSchemeRegistrations(); }

  void ReleaseCustomSchemeRegistrations() {
    if (m_customSchemeRegistrations) {
      for (UINT32 i = 0; i < m_customSchemeRegistrationsCount; i++) {
        // SAFETY: Since we can't convert the raw buffer to safe type we do a
        // bound check and use the macro to mitigate the error.
        UNSAFE_BUFFERS(m_customSchemeRegistrations[i])->Release();
      }
      deallocate_fn(m_customSchemeRegistrations);
      m_customSchemeRegistrations = nullptr;
      m_customSchemeRegistrationsCount = 0;
    }
  }

 private:
  // TODO(task.ms/56073082): Use raw_ptr.
#if defined(__has_attribute)
  __attribute__((annotate("raw_ptr_exclusion")))
#endif
  ICoreWebView2CustomSchemeRegistration** m_customSchemeRegistrations = nullptr;
  unsigned int m_customSchemeRegistrationsCount = 0;

  COREWEBVIEW2_RELEASE_CHANNELS m_releaseChannels = kAllChannels;
  COREWEBVIEW2_CHANNEL_SEARCH_KIND m_channelSearchKind =
      COREWEBVIEW2_CHANNEL_SEARCH_KIND_MOST_STABLE;

  // ICoreWebView2EnvironmentOptions8
  COREWEBVIEW2_SCROLLBAR_STYLE m_scrollbarStyle =
      COREWEBVIEW2_SCROLLBAR_STYLE_DEFAULT;

  DEFINE_AUTO_COMEM_STRING()

 public:
  // ICoreWebView2EnvironmentOptions
  COREWEBVIEW2ENVIRONMENTOPTIONS_STRING_PROPERTY(AdditionalBrowserArguments)
  COREWEBVIEW2ENVIRONMENTOPTIONS_STRING_PROPERTY(Language)
  COREWEBVIEW2ENVIRONMENTOPTIONS_STRING_PROPERTY(TargetCompatibleBrowserVersion)
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(
      AllowSingleSignOnUsingOSPrimaryAccount,
      false)

  // ICoreWebView2EnvironmentOptions2
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(ExclusiveUserDataFolderAccess,
                                               false)

  // ICoreWebView2EnvironmentOptions3
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(IsCustomCrashReportingEnabled,
                                               false)

  // ICoreWebView2EnvironmentOptions4
 public:
  HRESULT STDMETHODCALLTYPE GetCustomSchemeRegistrations(
      UINT32* count,
      ICoreWebView2CustomSchemeRegistration*** schemeRegistrations) override {
    if (!count || !schemeRegistrations) {
      return E_POINTER;
    }
    *count = 0;
    if (m_customSchemeRegistrationsCount == 0) {
      *schemeRegistrations = nullptr;
      return S_OK;
    } else {
      *schemeRegistrations =
          reinterpret_cast<ICoreWebView2CustomSchemeRegistration**>(
              allocate_fn(sizeof(ICoreWebView2CustomSchemeRegistration*) *
                          m_customSchemeRegistrationsCount));
      if (!*schemeRegistrations) {
        return HRESULT_FROM_WIN32(GetLastError());
      }
      for (UINT32 i = 0; i < m_customSchemeRegistrationsCount; i++) {
        // SAFETY: Since we can't convert the raw buffer to safe type we do a
        // bound check and use the macro to mitigate the error.
        UNSAFE_BUFFERS((*schemeRegistrations)[i] =
                           m_customSchemeRegistrations[i];
                       (*schemeRegistrations)[i]->AddRef();)
      }
      *count = m_customSchemeRegistrationsCount;
      return S_OK;
    }
  }

  HRESULT STDMETHODCALLTYPE SetCustomSchemeRegistrations(
      UINT32 count,
      ICoreWebView2CustomSchemeRegistration** schemeRegistrations) override {
    ReleaseCustomSchemeRegistrations();
    m_customSchemeRegistrations =
        reinterpret_cast<ICoreWebView2CustomSchemeRegistration**>(allocate_fn(
            sizeof(ICoreWebView2CustomSchemeRegistration*) * count));
    if (!m_customSchemeRegistrations) {
      return GetLastError();
    }
    for (UINT32 i = 0; i < count; i++) {
      // SAFETY: Since we can't convert the raw buffer to safe type we do a
      // bound check and use the macro to mitigate the error.
      UNSAFE_BUFFERS(m_customSchemeRegistrations[i] = schemeRegistrations[i];
                     m_customSchemeRegistrations[i]->AddRef());
    }
    m_customSchemeRegistrationsCount = count;
    return S_OK;
  }

  // ICoreWebView2EnvironmentOptions5
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(EnableTrackingPrevention, true)

  // ICoreWebView2EnvironmentOptions6
  COREWEBVIEW2ENVIRONMENTOPTIONS_BOOL_PROPERTY(AreBrowserExtensionsEnabled,
                                               false)
};

template <typename allocate_fn_t,
          allocate_fn_t allocate_fn,
          typename deallocate_fn_t,
          deallocate_fn_t deallocate_fn>
class CoreWebView2EnvironmentOptionsBaseClass
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          CoreWebView2EnvironmentOptionsBase<allocate_fn_t,
                                             allocate_fn,
                                             deallocate_fn_t,
                                             deallocate_fn>> {
 public:
  CoreWebView2EnvironmentOptionsBaseClass() {}

 protected:
  ~CoreWebView2EnvironmentOptionsBaseClass() override {}
};

typedef CoreWebView2CustomSchemeRegistrationBase<decltype(&::CoTaskMemAlloc),
                                                 ::CoTaskMemAlloc,
                                                 decltype(&::CoTaskMemFree),
                                                 ::CoTaskMemFree>
    CoreWebView2CustomSchemeRegistration;

typedef CoreWebView2EnvironmentOptionsBaseClass<decltype(&::CoTaskMemAlloc),
                                                ::CoTaskMemAlloc,
                                                decltype(&::CoTaskMemFree),
                                                ::CoTaskMemFree>
    CoreWebView2EnvironmentOptions;

#endif  // __core_webview2_environment_options_h__
