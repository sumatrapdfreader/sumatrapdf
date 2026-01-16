// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibSubclass.h"

#if defined(_DARKMODELIB_PREFER_THEME)
namespace dmlib_win32api
{
	[[nodiscard]] bool IsWindows10() noexcept;
	[[nodiscard]] bool IsDarkModeSupported() noexcept;
}
#endif

/**
 * @brief Determines if themed styling should be preferred over subclassing.
 *
 * Requires support for experimental theming and Windows 10 or later.
 *
 * @return `true` if themed appearance is preferred and supported.
 */
bool dmlib_subclass::isThemePrefered() noexcept
{
#if defined(_DARKMODELIB_PREFER_THEME)
	return dmlib_win32api::IsWindows10() && dmlib_win32api::IsDarkModeSupported();
#else
	return false;
#endif
}
