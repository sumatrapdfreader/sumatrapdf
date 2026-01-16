// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#include <windows.h>

namespace dmlib_hook
{
#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
	bool loadOpenNcThemeData(const HMODULE& hUxtheme) noexcept;
	/// Makes scroll bars on the specified window and all its children consistent.
	void enableDarkScrollBarForWindowAndChildren(HWND hWnd);
	void fixDarkScrollBar();
#endif

	/// Overrides a specific system color with a custom color.
	void setMySysColor(int nIndex, COLORREF clr) noexcept;
	/// Hooks system color to support runtime customization.
	bool hookSysColor() noexcept;
	/// Unhooks system color overrides and restores default color behavior.
	void unhookSysColor() noexcept;

	/// Hooks `GetThemeColor` and `DrawThemeBackgroundEx` to support dark colors.
	bool hookThemeColor() noexcept;
	/// Unhooks `GetThemeColor` and `DrawThemeBackgroundEx` overrides and restores default color behavior.
	void unhookThemeColor() noexcept;
} // namespace dmlib_hook
