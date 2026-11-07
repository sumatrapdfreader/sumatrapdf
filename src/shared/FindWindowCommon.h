/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- constants shared by orig's and ng's FindWindow.cpp and FindBar.cpp ---

constexpr int kFindWinPadding = 8;

constexpr int kFindWinGap = 6;

// command ids for the bar's and the window's toolbar buttons; must not collide
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;
constexpr int kFindWinPinCmdId = (int)CmdLast + 51;
constexpr int kFindBarPinCmdId = (int)CmdLast + 52;
constexpr int kFindWinCloseCmdId = (int)CmdLast + 53;
