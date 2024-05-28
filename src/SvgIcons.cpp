/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "SvgIcons.h"

// https://github.com/tabler/tabler-icons/blob/master/icons/folder.svg
static const char* gIconFileOpen =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M5 4h4l3 3h7a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-14a2 2 0 0 1 -2 -2v-11a2 2 0 0 1 2 -2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/printer.svg
static const char* gIconPrint =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M17 17h2a2 2 0 0 0 2 -2v-4a2 2 0 0 0 -2 -2h-14a2 2 0 0 0 -2 2v4a2 2 0 0 0 2 2h2" />
  <path d="M17 9v-4a2 2 0 0 0 -2 -2h-6a2 2 0 0 0 -2 2v4" />
  <rect x="7" y="13" width="10" height="8" rx="2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/arrow-left.svg
static const char* gIconPagePrev =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="5" y1="12" x2="19" y2="12" />
  <line x1="5" y1="12" x2="11" y2="18" />
  <line x1="5" y1="12" x2="11" y2="6" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/arrow-right.svg
static const char* gIconPageNext =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="5" y1="12" x2="19" y2="12" />
  <line x1="13" y1="18" x2="19" y2="12" />
  <line x1="13" y1="6" x2="19" y2="12" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/layout-rows.svg
static const char* gIconLayoutContinuous =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <rect x="3" y="3" width="18" height="18" rx="2" />
  <line x1="3" y1="12" x2="21" y2="12" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/square.svg
static const char* gIconLayoutSinglePage =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <rect x="4" y="4" width="16" height="16" rx="2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/chevron-left.svg
static const char* gIconSearchPrev =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="15 6 9 12 15 18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/chevron-right.svg
static const char* gIconSearchNext =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="9 6 15 12 9 18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/letter-case.svg
static const char* gIconMatchCase =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <circle cx="18" cy="16" r="3" />
  <line x1="21" y1="13" x2="21" y2="19" />
  <path d="M3 19l5 -13l5 13" />
  <line x1="5" y1="14" x2="11" y2="14" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/zoom-in.svg
static const char* gIconZoomIn =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="10" cy="10" r="7" />
  <line x1="7" y1="10" x2="13" y2="10" />
  <line x1="10" y1="7" x2="10" y2="13" />
  <line x1="21" y1="21" x2="15" y2="15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/zoom-out.svg
static const char* gIconZoomOut =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="10" cy="10" r="7" />
  <line x1="7" y1="10" x2="13" y2="10" />
  <line x1="21" y1="21" x2="15" y2="15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/floppy-disk.svg
static const char* gIconSave =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M18 20h-12a2 2 0 0 1 -2 -2v-12a2 2 0 0 1 2 -2h9l5 5v9a2 2 0 0 1 -2 2" />
  <circle cx="12" cy="13" r="2" />
  <polyline points="4 8 10 8 10 4" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/rotate-2.svg - modified
static const char* gIconRotateLeft =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M15 4.55a8 8 0 0 0 -6 14.9m0 -5.45v6h-6"/>
  <circle cx="18.37" cy="7.16" r="0.15"/>
  <circle cx="13" cy="19.94" r="0.15"/>
  <circle cx="16.84" cy="18.37" r="0.15"/>
  <circle cx="19.37" cy="15.1" r="0.15"/>
  <circle cx="19.94" cy="11" r="0.15"/>
</svg>)";

// https://github.com/tabler/tabler-icons/blob/master/icons/rotate-clockwise-2.svg - modified
static const char* gIconRotateRight =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M9 4.55a8 8 0 0 1 6 14.9m0 -5.45v6h6"/>
  <circle cx="5.63" cy="7.16" r="0.15"/>
  <circle cx="4.06" cy="11" r="0.15"/>
  <circle cx="4.63" cy="15.1" r="0.15"/>
  <circle cx="7.16" cy="18.37" r="0.15"/>
  <circle cx="11" cy="19.94" r="0.15"/>
</svg>)";

// must match order in enum class TbIcon
// clang-format off
static const char* gIcons[] = {
    gIconFileOpen,
    gIconPrint,
    gIconPagePrev,
    gIconPageNext,
    gIconLayoutContinuous,
    gIconLayoutSinglePage,
    gIconZoomOut,
    gIconZoomIn,
    gIconSearchPrev,
    gIconSearchNext,
    gIconMatchCase,
    gIconMatchCase,  // TODO: remove this, is for compatiblity with bitmap icons
    gIconSave,
    gIconRotateLeft,
    gIconRotateRight,
};
// clang-format on

const char* GetSvgIcon(TbIcon idx) {
    int n = (int)idx;
    ReportIf(n < 0 || n >= dimofi(gIcons));
    if (n >= dimofi(gIcons)) {
        return nullptr;
    }
    return gIcons[n];
}
