/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: a file that needs both gpui and our base library includes this instead
// of "base/Base.h", and includes it first. gpui.h brings its own `base`
// namespace and redefines the StrL / dimof macros, so the order matters:
//   - NOMINMAX before the first <windows.h> (winsock2.h pulls it in, and
//     gpui.h's own min/max-free code is parsed after that)
//   - winsock2.h before <windows.h>, which our Base.h also requires
//   - StrL / dimof are undefined after gpui.h so Base.h can define ours
// After this header, `Str` / `Vec` / `Rect` / `Point` / `Size` are ours and
// gpui's are `gp::Str` &c. Never `using namespace gpui` in such a file.

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#endif

#include "gpui.h"

#undef StrL
#undef dimof

#include "base/Base.h"

// shlwapi.h (pulled in by Base.h) makes StrDup / StrCat / StrCmp macros for
// their W variants, which mangles the gpui functions of those names
#undef StrDup
#undef StrCat
#undef StrCmp
#undef StrCmpI
#undef StrCmpN
#undef StrCmpNI
#undef StrStr

namespace gp = gpui;
namespace gpc = gpui::component;

// StrL for a gpui string literal
#define GStrL(lit)                           \
    gp::Str {                                \
        (char*)(lit), (int)(sizeof(lit) - 1) \
    }

inline gp::Str ToGpui(Str s) {
    return gp::Str{s.s, s.len};
}

inline Str FromGpui(gp::Str s) {
    return Str{s.s, s.len};
}

// a temp string copied into the frame arena, which is what an element may hold
inline gp::Str GpuiDup(gp::Arena* a, Str s) {
    return gp::StrDup(a, ToGpui(s));
}

// ng: a Color made with MkRgb() has a zero alpha byte and means opaque - orig
// stores win32 COLORREFs, which have no alpha, and asks for translucency with
// a separate parameter. gpui's Rgba really does use the alpha, so 0 becomes
// 255 here; a translucent color is built with MkRgba() and survives.
inline gp::Rgba ToGpui(Color c) {
    u8 a = GetAlpha(c);
    return gp::Rgba{GetRed(c), GetGreen(c), GetBlue(c), a == 0 ? (u8)255 : a};
}

inline Color FromGpui(gp::Rgba c) {
    return MkRgba(c.r, c.g, c.b, c.a);
}

inline gp::Point ToGpui(Point p) {
    return gp::Point{(float)p.x, (float)p.y};
}

inline Point FromGpui(gp::Point p) {
    return Point{(int)p.x, (int)p.y};
}

inline gp::Size ToGpui(Size s) {
    return gp::Size{(float)s.dx, (float)s.dy};
}

inline Size FromGpui(gp::Size s) {
    return Size{(int)s.w, (int)s.h};
}

inline gp::Bounds ToGpui(Rect r) {
    return gp::Bounds{(float)r.x, (float)r.y, (float)r.dx, (float)r.dy};
}

inline Rect FromGpui(gp::Bounds b) {
    return Rect{(int)b.x, (int)b.y, (int)b.w, (int)b.h};
}

// ng: gpui's Listen() needs the listener's owner entity type, so it can't be
// wrapped generically. This is the one shape the shell repeats: a handler on
// the root view entity.
#define GpuiListen(cx, fn) gp::Listen(cx, fn)
#define GpuiListenArg(cx, fn, arg) gp::Listen(cx, fn, arg)
