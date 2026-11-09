/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

// ng: objc's BOOL (pulled in by CoreFoundation in Objective-C++) is bool; ours is int
#include "base/MacTypesHide.h"
#define BOOL MacObjcBool
#include <CoreFoundation/CoreFoundation.h>
#undef BOOL
#include "base/MacTypesShow.h"

#include "Translations.h"

// System Settings > Language & Region, as BCP 47 tags ("zh-Hans-CN")
void GetSystemPreferredLocales(StrVec& out) {
    CFArrayRef langs = CFLocaleCopyPreferredLanguages();
    if (!langs) {
        return;
    }
    CFIndex n = CFArrayGetCount(langs);
    for (CFIndex i = 0; i < n; i++) {
        auto s = (CFStringRef)CFArrayGetValueAtIndex(langs, i);
        char buf[64];
        if (s && CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) {
            out.Append(Str(buf));
        }
    }
    CFRelease(langs);
}
