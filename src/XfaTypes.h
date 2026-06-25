/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Include utils/BaseUtil.h before this header (provides RectF via GeomUtil.h).

enum class XfaFieldKind {
    Unknown = 0,
    Text = 1,
    Checkbox = 2,
    Radio = 3,
    Choice = 4,
};

struct XfaFieldHit {
    int pageNo = 0;
    char name[64] = {};
    RectF bounds;
    XfaFieldKind kind = XfaFieldKind::Unknown;

    bool IsValid() const { return name[0] != 0; }
};