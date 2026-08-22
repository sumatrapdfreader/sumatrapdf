// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025-2026 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#include <windows.h>

#include "MemoryHelperDef.h"

namespace dmlib_subclass
{
	LRESULT CALLBACK WindowEraseBgSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) noexcept;
	LRESULT CALLBACK WindowCtlColorSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) noexcept;
	LRESULT CALLBACK WindowNotifySubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) noexcept;
	LRESULT CALLBACK WindowMenuBarSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) noexcept;
	LRESULT CALLBACK WindowSettingChangeSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) noexcept;

	/// Applies a subclass to task dialog and its children to handle dark mode.
	void setTaskDlgChildCtrlsSubclassAndTheme(HWND hWnd) DMLIB_MEM_NOEXCEPT;

} // namespace dmlib_subclass
