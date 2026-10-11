/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- constants shared by orig's and ng's AnnotTextPopup.cpp ---

constexpr int kMargin = 8;

// between the header line and the rule under it
constexpr int kRuleGap = 4;

constexpr int kCornerRadius = 6;

// the card is as wide as the comment's longest line; lines longer than this
// wrap, so a comment with no line breaks doesn't span the canvas
constexpr int kMaxLineChars = 80;

constexpr int kMinWidth = 120;

// how much of the canvas the card may cover before the text starts scrolling
constexpr int kMaxHeightPercent = 60;
