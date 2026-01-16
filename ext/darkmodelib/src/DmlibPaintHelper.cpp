// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibPaintHelper.h"

#include <windows.h>

/**
 * @brief Paints a rounded rectangle using the specified pen and brush.
 *
 * Draws a rounded rectangle defined by `rect`, using the provided pen (`hpen`) and brush (`hBrush`)
 * for the edge and fill, respectively. Preserves previous GDI object selections.
 *
 * @param[in]   hdc     Handle to the device context.
 * @param[in]   rect    Rectangle bounds for the shape.
 * @param[in]   hpen    Pen used to draw the edge.
 * @param[in]   hBrush  Brush used to inner fill.
 * @param[in]   width   Horizontal corner radius.
 * @param[in]   height  Vertical corner radius.
 */
void dmlib_paint::paintRoundRect(
	HDC hdc,
	const RECT& rect,
	HPEN hpen,
	HBRUSH hBrush,
	int width,
	int height
) noexcept
{
	auto holdBrush = ::SelectObject(hdc, hBrush);
	auto holdPen = ::SelectObject(hdc, hpen);
	::RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, width, height);
	::SelectObject(hdc, holdBrush);
	::SelectObject(hdc, holdPen);
}
