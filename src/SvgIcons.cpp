/* Tabler icon source links updated from master/icons to main/icons/outline
   after https://github.com/tabler/tabler-icons/commit/143d38918613a3e13998713fe7aaa1f246df3e6e
 */

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

extern "C" {
#include <mupdf/fitz.h>
}

#include "ImageReader.h"
#include "Theme.h"
#include "SvgIcons.h"

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/folder.svg
const char* gIconFileOpen =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M5 4h4l3 3h7a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-14a2 2 0 0 1 -2 -2v-11a2 2 0 0 1 2 -2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/printer.svg
const char* gIconPrint =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M17 17h2a2 2 0 0 0 2 -2v-4a2 2 0 0 0 -2 -2h-14a2 2 0 0 0 -2 2v4a2 2 0 0 0 2 2h2" />
  <path d="M17 9v-4a2 2 0 0 0 -2 -2h-6a2 2 0 0 0 -2 2v4" />
  <rect x="7" y="13" width="10" height="8" rx="2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/arrow-left.svg
const char* gIconPagePrev =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="5" y1="12" x2="19" y2="12" />
  <line x1="5" y1="12" x2="11" y2="18" />
  <line x1="5" y1="12" x2="11" y2="6" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/arrow-right.svg
const char* gIconPageNext =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="5" y1="12" x2="19" y2="12" />
  <line x1="13" y1="18" x2="19" y2="12" />
  <line x1="13" y1="6" x2="19" y2="12" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/layout-rows.svg
const char* gIconLayoutContinuous =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <rect x="3" y="3" width="18" height="18" rx="2" />
  <line x1="3" y1="12" x2="21" y2="12" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/square.svg
const char* gIconLayoutSinglePage =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <rect x="4" y="4" width="16" height="16" rx="2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/chevron-left.svg
const char* gIconSearchPrev =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="15 6 9 12 15 18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/chevron-right.svg
const char* gIconSearchNext =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="9 6 15 12 9 18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/letter-case.svg
const char* gIconMatchCase =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <circle cx="18" cy="16" r="3" />
  <line x1="21" y1="13" x2="21" y2="19" />
  <path d="M3 19l5 -13l5 13" />
  <line x1="5" y1="14" x2="11" y2="14" />
</svg>)";

// "match whole word": lowercase "ab" over an underline bracketed at both ends,
// suggesting a complete word delimited by word boundaries (like VS Code's
// whole-word toggle). Custom icon drawn in the tabler stroke style.
const char* gIconMatchWholeWord =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <circle cx="7" cy="11" r="2.5" />
  <line x1="9.5" y1="8.5" x2="9.5" y2="13.5" />
  <line x1="14.5" y1="6" x2="14.5" y2="13.5" />
  <circle cx="17" cy="11" r="2.5" />
  <path d="M3 16v3h18v-3" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/zoom-in.svg
const char* gIconZoomIn =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="10" cy="10" r="7" />
  <line x1="7" y1="10" x2="13" y2="10" />
  <line x1="10" y1="7" x2="10" y2="13" />
  <line x1="21" y1="21" x2="15" y2="15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/zoom-out.svg
const char* gIconZoomOut =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="10" cy="10" r="7" />
  <line x1="7" y1="10" x2="13" y2="10" />
  <line x1="21" y1="21" x2="15" y2="15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/device-floppy.svg
const char* gIconSave =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M18 20h-12a2 2 0 0 1 -2 -2v-12a2 2 0 0 1 2 -2h9l5 5v9a2 2 0 0 1 -2 2" />
  <circle cx="12" cy="13" r="2" />
  <polyline points="4 8 10 8 10 4" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/file-plus.svg
// a document with a plus: saving the changes into a new PDF
const char* gIconSaveToNewFile =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M14 3v4a1 1 0 0 0 1 1h4" />
  <path d="M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2z" />
  <line x1="12" y1="11" x2="12" y2="17" />
  <line x1="9" y1="14" x2="15" y2="14" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/rotate-2.svg - modified
const char* gIconRotateLeft =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M15 4.55a8 8 0 0 0 -6 14.9m0 -5.45v6h-6"/>
  <circle cx="18.37" cy="7.16" r="0.15"/>
  <circle cx="13" cy="19.94" r="0.15"/>
  <circle cx="16.84" cy="18.37" r="0.15"/>
  <circle cx="19.37" cy="15.1" r="0.15"/>
  <circle cx="19.94" cy="11" r="0.15"/>
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/rotate-clockwise-2.svg - modified
const char* gIconRotateRight =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M9 4.55a8 8 0 0 1 6 14.9m0 -5.45v6h6"/>
  <circle cx="5.63" cy="7.16" r="0.15"/>
  <circle cx="4.06" cy="11" r="0.15"/>
  <circle cx="4.63" cy="15.1" r="0.15"/>
  <circle cx="7.16" cy="18.37" r="0.15"/>
  <circle cx="11" cy="19.94" r="0.15"/>
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/copy.svg
const char* gIconCopy =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M16 8v-2a2 2 0 0 0 -2 -2h-8a2 2 0 0 0 -2 2v8a2 2 0 0 0 2 2h2" />
  <rect x="8" y="8" width="12" height="12" rx="2" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/language.svg
const char* gIconTranslate =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M4 5h7" />
  <path d="M9 3v2c0 4.418 -2.239 8 -5 8" />
  <path d="M5 9c0 2.144 2.952 3.908 6.7 4" />
  <path d="M12 20l4 -9l4 9" />
  <path d="M19.1 18h-6.2" />
</svg>)";

const char* gIconSpeak =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M15 8a5 5 0 0 1 0 8" />
  <path d="M17.7 5a9 9 0 0 1 0 14" />
  <path d="M6 15h-2a1 1 0 0 1 -1 -1v-4a1 1 0 0 1 1 -1h2l3.5 -4.5a.8 .8 0 0 1 1.5 .5v14a.8 .8 0 0 1 -1.5 .5l-3.5 -4.5" />
</svg>)";

// tabler player-pause
const char* gIconPauseSpeaking =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M6 5v14" />
  <path d="M18 5v14" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/arrow-back-up.svg
const char* gIconNavigateBack =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M9 14l-4 -4l4 -4" />
  <path d="M5 10h11a4 4 0 1 1 0 8h-1" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/arrow-forward-up.svg
const char* gIconNavigateForward =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M15 14l4 -4l-4 -4" />
  <path d="M19 10h-11a4 4 0 1 0 0 8h1" />
</svg>)";

// undo / redo: like tabler arrow-back-up / arrow-forward-up, but with a wider
// loop and a longer tail so they don't read as the toolbar's Back / Forward
const char* gIconUndo =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M9 13l-5 -4l5 -4" />
  <path d="M4 9h10a5 5 0 0 1 0 10h-6" />
</svg>)";

const char* gIconRedo =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M15 13l5 -4l-5 -4" />
  <path d="M20 9h-10a5 5 0 0 0 0 10h6" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/search.svg
const char* gIconSearch =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="10" cy="10" r="7" />
  <line x1="21" y1="21" x2="15" y2="15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/list-search.svg
const char* gIconFindAnnotation =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <circle cx="15" cy="14" r="4" />
  <line x1="18" y1="17" x2="21" y2="20" />
  <line x1="3" y1="6" x2="20" y2="6" />
  <line x1="3" y1="12" x2="8" y2="12" />
  <line x1="3" y1="18" x2="8" y2="18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/command.svg
const char* gIconCommandPalette =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M7 9a2 2 0 1 1 2 -2v10a2 2 0 1 1 -2 -2h10a2 2 0 1 1 -2 2v-10a2 2 0 1 1 2 2h-10" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/chevron-up.svg
const char* gIconChevronUp =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="6 15 12 9 18 15" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/chevron-down.svg
const char* gIconChevronDown =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <polyline points="6 9 12 15 18 9" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/x.svg
const char* gIconClose =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="18" y1="6" x2="6" y2="18" />
  <line x1="6" y1="6" x2="18" y2="18" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/pin.svg
// tabler arrows-diagonal: expand the compact find bar into a floating window
const char* gIconArrowsDiagonal =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M16 4l4 0l0 4" />
  <path d="M14 10l6 -6" />
  <path d="M8 20l-4 0l0 -4" />
  <path d="M4 20l6 -6" />
</svg>)";

// tabler arrows-diagonal-minimize-2: dock the floating window back to the bar
const char* gIconArrowsDiagonalMinimize =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M18 10l-4 0l0 -4" />
  <path d="M20 4l-6 6" />
  <path d="M6 14l4 0l0 4" />
  <path d="M10 14l-6 6" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/list.svg
const char* gIconHomeList =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <line x1="9" y1="6" x2="20" y2="6" />
  <line x1="9" y1="12" x2="20" y2="12" />
  <line x1="9" y1="18" x2="20" y2="18" />
  <line x1="5" y1="6" x2="5" y2="6.01" />
  <line x1="5" y1="12" x2="5" y2="12.01" />
  <line x1="5" y1="18" x2="5" y2="18.01" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/layout-grid.svg
const char* gIconHomeThumbnails =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <rect x="4" y="4" width="6" height="6" rx="1" />
  <rect x="14" y="4" width="6" height="6" rx="1" />
  <rect x="4" y="14" width="6" height="6" rx="1" />
  <rect x="14" y="14" width="6" height="6" rx="1" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/pin.svg
const char* gIconPin =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <rect x="0" y="0" width="24" height="24" stroke="none"></rect>
  <path d="M15 4.5l4.5 4.5" />
  <path d="M14.5 9.5l-5 5" />
  <path d="M9 15l-4 4" />
  <path d="M9.5 4l10.5 10.5l-5.5 0.5l-4 4l-1 -4.5l-4.5 -1l4 -4z" />
</svg>)";

// PDF annotation tools, drawn in the same 24px stroke style as the toolbar's
// Tabler icons.
const char* gIconEditAnnotations =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M4 20h4l11 -11a2.8 2.8 0 0 0 -4 -4l-11 11v4" />
  <path d="M13.5 6.5l4 4" />
</svg>)";

const char* gIconAnnotHighlight =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M4 17l4 3l10 -12a2.8 2.8 0 0 0 -4 -4l-10 13" />
  <path d="M12.5 6l4 3.5" />
  <path d="M3 21h18" />
</svg>)";

const char* gIconAnnotHighlightBrush =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M15 3l6 6l-9 9h-6l-3 -3z" />
  <path d="M12 6l6 6" />
  <rect x="3" y="20" width="18" height="3" rx="1" fill="currentColor" stroke="none" />
</svg>)";

const char* gIconAnnotUnderline =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M6 4v7a6 6 0 0 0 12 0v-7" />
  <path d="M4 21h16" />
</svg>)";

const char* gIconAnnotSquiggly =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M6 3v7a6 6 0 0 0 12 0v-7" />
  <path d="M3 20c1.5 -3 3 3 4.5 0s3 3 4.5 0s3 3 4.5 0s3 3 4.5 0" />
</svg>)";

const char* gIconAnnotStrikeOut =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M16 6.5a5 3 0 0 0 -4 -1.5c-2.2 0 -4 1.1 -4 2.7c0 1.2 .9 2 2.8 2.6" />
  <path d="M13 13.2c2 .5 3 1.3 3 2.7c0 1.8 -1.8 3.1 -4.2 3.1a6 4 0 0 1 -4.8 -2" />
  <path d="M4 12h16" />
</svg>)";

const char* gIconAnnotText =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M5 4h14v12l-4 4h-10z" />
  <path d="M15 20v-4h4" />
  <path d="M8 8h8" />
  <path d="M8 12h6" />
</svg>)";

const char* gIconAnnotFreeText =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M5 8v-3h14v3" />
  <path d="M5 16v3h14v-3" />
  <path d="M9 9h6" />
  <path d="M12 9v7" />
</svg>)";

const char* gIconAnnotLine =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M5 19l14 -14" />
  <circle cx="5" cy="19" r="1.5" />
  <circle cx="19" cy="5" r="1.5" />
</svg>)";

const char* gIconAnnotSquare =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <rect x="4" y="4" width="16" height="16" rx="1" />
</svg>)";

const char* gIconAnnotCircle =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <circle cx="12" cy="12" r="8" />
</svg>)";

const char* gIconAnnotPolygon =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M12 3l8 6l-3 10h-10l-3 -10z" />
  <circle cx="12" cy="3" r="1" />
  <circle cx="20" cy="9" r="1" />
  <circle cx="17" cy="19" r="1" />
  <circle cx="7" cy="19" r="1" />
  <circle cx="4" cy="9" r="1" />
</svg>)";

const char* gIconAnnotPolyLine =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M4 18l5 -11l5 9l6 -10" />
  <circle cx="4" cy="18" r="1.2" />
  <circle cx="9" cy="7" r="1.2" />
  <circle cx="14" cy="16" r="1.2" />
  <circle cx="20" cy="6" r="1.2" />
</svg>)";

const char* gIconAnnotInk =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M4 17c4 -8 5 -11 7 -11c3 0 -2 10 0 10c1.5 0 3 -6 4.5 -6c2.5 0 -1 6 1 6c1 0 2 -2 3.5 -3" />
  <path d="M4 20h16" />
</svg>)";

const char* gIconAnnotRedact =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <rect x="4" y="5" width="16" height="14" rx="1" />
  <rect x="7" y="9" width="10" height="6" fill="currentColor" stroke="none" />
</svg>)";

const char* gIconApplyRedactions =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <rect x="4" y="5" width="16" height="14" rx="1" />
  <path d="M8 12l3 3l5 -6" />
</svg>)";

const char* gIconAnnotStamp =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M8 14c1 -2 1.5 -3.5 1.5 -6a2.5 2.5 0 0 1 5 0c0 2.5 .5 4 1.5 6" />
  <path d="M6 14h12a2 2 0 0 1 2 2v2h-16v-2a2 2 0 0 1 2 -2" />
  <path d="M5 21h14" />
</svg>)";

const char* gIconAnnotCaret =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <path d="M4 17l8 -10l8 10" />
  <path d="M8 17l4 -5l4 5" />
</svg>)";

// https://github.com/tabler/tabler-icons/blob/main/icons/outline/paperclip.svg
const char* gIconAnnotFileAttachment =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z" fill="none"/>
  <path d="M15 7l-6.5 6.5a1.5 1.5 0 0 0 3 3l6.5 -6.5a3 3 0 0 0 -6 -6l-6.5 6.5a4.5 4.5 0 0 0 9 9l6.5 -6.5" />
</svg>)";

const char* gIconTrash =
    R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" stroke-width="1" stroke="currentColor" fill="none" stroke-linecap="round" stroke-linejoin="round">
  <path stroke="none" d="M0 0h24v24H0z"/>
  <line x1="4" y1="7" x2="20" y2="7" />
  <line x1="10" y1="11" x2="10" y2="17" />
  <line x1="14" y1="11" x2="14" y2="17" />
  <path d="M5 7l1 12a2 2 0 0 0 2 2h8a2 2 0 0 0 2 -2l1 -12" />
  <path d="M9 7v-3a1 1 0 0 1 1 -1h4a1 1 0 0 1 1 1v3" />
</svg>)";

// A custom ToolbarSvgIcon comes from the settings file, so it can be malformed:
// a typo, or the file caught half-written by the settings watcher while the user
// is editing it. mupdf signals that by throwing, and an uncaught mupdf exception
// aborts the whole process, so everything here has to be inside fz_try.
static fz_pixmap* RenderSvgToFzPixmap(fz_context* ctx, Str svgData, int dx, int dy, Color fgCol) {
    TempStr strokeCol = SerializeColorTemp(fgCol);
    TempStr svg = str::ReplaceTemp(svgData, StrL("currentColor"), strokeCol);

    fz_buffer* buf = nullptr;
    fz_display_list* list = nullptr;
    fz_device* dev = nullptr;
    fz_pixmap* pixmap = nullptr;
    fz_var(buf);
    fz_var(list);
    fz_var(dev);
    fz_var(pixmap);
    fz_try(ctx) {
        buf = fz_new_buffer_from_copied_data(ctx, (u8*)svg.s, svg.len);
        float svgWidth = 0;
        float svgHeight = 0;
        list = fz_new_display_list_from_svg(ctx, buf, nullptr, nullptr, &svgWidth, &svgHeight);
        pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), fz_make_irect(0, 0, dx, dy), nullptr, 1);
        fz_clear_pixmap(ctx, pixmap);
        dev = fz_new_draw_device(ctx, fz_scale(dx / svgWidth, dy / svgHeight), pixmap);
        fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, nullptr);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        dev = nullptr;
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_display_list(ctx, list);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_drop_pixmap(ctx, pixmap);
        fz_report_error(ctx);
        logf("GetCachedPixmapForSvg: rendering svg icon failed with: '%s'\n", Str(fz_caught_message(ctx)));
        return nullptr;
    }
    return pixmap;
}

static void BlitFzPixmapBgra(u8* dstSamples, ptrdiff_t dstStride, fz_pixmap* src) {
    int dx = src->w;
    int dy = src->h;
    int srcN = src->n;
    int srcAlpha = src->alpha;
    auto srcStride = src->stride;
    for (size_t y = 0; y < (size_t)dy; y++) {
        u8* s = src->samples + (srcStride * y);
        u8* d = dstSamples + (dstStride * y);
        for (int x = 0; x < dx; x++) {
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d[3] = srcAlpha ? s[srcN - 1] : 0xff;
            d += 4;
            s += srcN;
        }
    }
}

// BGRA DIB, alpha-premultiplied, transparent where the SVG left the background.
static Pixmap* RenderSvgToPixmap(Str svgData, int dx, int dy, Color fgCol) {
    Pixmap* px = AllocPixmapDIB(dx, dy);
    if (!px) {
        return nullptr;
    }
    memset(px->data, 0, (size_t)px->stride * (size_t)dy);
    px->premultiplied = true;

    fz_context* ctx = fz_new_context_windows();
    fz_pixmap* pixmap = RenderSvgToFzPixmap(ctx, svgData, dx, dy, fgCol);
    if (pixmap) {
        BlitFzPixmapBgra(px->data, px->stride, pixmap);
        u8* row = px->data;
        for (int y = 0; y < dy; y++) {
            u8* d = row;
            for (int x = 0; x < dx; x++) {
                if (d[3] == 0) {
                    d[0] = d[1] = d[2] = 0;
                }
                d += 4;
            }
            row += px->stride;
        }
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_drop_context_windows(ctx);
    return px;
}

// Super-set of the old GetPixmapForIcon / SelToolbarIcon caches: keyed by
// SVG bytes (built-in gIcon* or a user-provided string), size, and colors.
struct SvgPixmapCacheEntry {
    SvgPixmapCacheEntry* next = nullptr;
    Str svg; // owned
    int dx = 0;
    int dy = 0;
    Color fg = 0;
    Color bg = 0;
    Pixmap* pixmap = nullptr; // owned

    ~SvgPixmapCacheEntry() {
        str::Free(svg);
        FreePixmap(pixmap);
    }
};

static SvgPixmapCacheEntry* gSvgPixmapCache = nullptr;

// Render `svg` at dx×dy in fg/bg (theme text/control colors if unset).
// The Pixmap belongs to the cache until DestroySvgPixmapIconsCache().
Pixmap* GetCachedPixmapForSvg(Str svg, int dx, int dy, Color fg, Color bg) {
    if (str::IsEmptyOrWhiteSpace(svg) || dx <= 0 || dy <= 0) {
        return nullptr;
    }
    if (fg == kColorUnset) {
        fg = ThemeWindowTextColor();
    }
    if (bg == kColorUnset) {
        bg = ThemeControlBackgroundColor();
    }
    for (SvgPixmapCacheEntry* e = gSvgPixmapCache; e; e = e->next) {
        if (e->dx == dx && e->dy == dy && e->fg == fg && e->bg == bg && str::Eq(e->svg, svg)) {
            return e->pixmap;
        }
    }
    Pixmap* px = RenderSvgToPixmap(svg, dx, dy, fg);
    if (!px) {
        return nullptr;
    }
    auto* e = new SvgPixmapCacheEntry();
    e->svg = str::Dup(svg);
    e->dx = dx;
    e->dy = dy;
    e->fg = fg;
    e->bg = bg;
    e->pixmap = px;
    ListInsertFront(&gSvgPixmapCache, e);
    return px;
}

// Theme, DPI, and shutdown: every cached pixmap is in the current colors/size.
void DestroySvgPixmapIconsCache() {
    ListDelete(gSvgPixmapCache);
    gSvgPixmapCache = nullptr;
}
