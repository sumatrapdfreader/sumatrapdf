/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// must match order in gAllIcons
enum class TbIcon {
    Text = -2,
    None = -1,
    Open = 0,
    Print,
    PagePrev,
    PageNext,
    LayoutContinuous,
    LayoutSinglePage,
    ZoomOut,
    ZoomIn,
    SearchPrev,
    SearchNext,
    MatchCase,
    Save,
    RotateLeft,
    RotateRight,
    Speak,
    PauseSpeaking,
    NavigateBack,
    NavigateForward,
    Search,
    ChevronUp,
    ChevronDown,
    Close,
    ArrowsDiagonal,
    ArrowsDiagonalMinimize,
    MatchWholeWord,
    kMax
};

const char* GetSvgIcon(TbIcon);
