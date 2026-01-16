// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibColor.h"

#include <windows.h>

#include <dwmapi.h>
#include <shlwapi.h>

#include <cmath>

#include "DarkModeSubclass.h"

namespace dmlib_win32api
{
	[[nodiscard]] bool IsDarkModeActive() noexcept;
}

DarkMode::Colors dmlib_color::getLightColors() noexcept
{
	return DarkMode::Colors{
		::GetSysColor(COLOR_3DFACE),        // background
		::GetSysColor(COLOR_WINDOW),        // ctrlBackground
		dmlib_color::HEXRGB(0xC0DCF3),      // hotBackground
		::GetSysColor(COLOR_3DFACE),        // dlgBackground
		dmlib_color::HEXRGB(0xA01000),      // errorBackground
		::GetSysColor(COLOR_WINDOWTEXT),    // textColor
		::GetSysColor(COLOR_BTNTEXT),       // darkerTextColor
		::GetSysColor(COLOR_GRAYTEXT),      // disabledTextColor
		::GetSysColor(COLOR_HOTLIGHT),      // linkTextColor
		dmlib_color::HEXRGB(0x8D8D8D),      // edgeColor
		::GetSysColor(COLOR_HIGHLIGHT),     // hotEdgeColor
		::GetSysColor(COLOR_GRAYTEXT)       // disabledEdgeColor
	};
}

/**
 * @brief Calculates perceptual lightness of a COLORREF color.
 *
 * Converts the RGB color to linear space and calculates perceived lightness.
 *
 * @param[in] clr COLORREF in 0xBBGGRR format.
 * @return Lightness value as a double.
 *
 * @note Based on: https://stackoverflow.com/a/56678483
 */
double dmlib_color::calculatePerceivedLightness(COLORREF clr) noexcept
{
	auto linearValue = [](double colorChannel) noexcept
	{
		colorChannel /= 255.0;

		static constexpr double treshhold = 0.04045;
		static constexpr double lowScalingFactor = 12.92;
		static constexpr double gammaOffset = 0.055;
		static constexpr double gammaScalingFactor = 1.055;
		static constexpr double gammaExp = 2.4;

		if (colorChannel <= treshhold)
		{
			return colorChannel / lowScalingFactor;
		}
		return std::pow(((colorChannel + gammaOffset) / gammaScalingFactor), gammaExp);
	};

	const double r = linearValue(static_cast<double>(GetRValue(clr)));
	const double g = linearValue(static_cast<double>(GetGValue(clr)));
	const double b = linearValue(static_cast<double>(GetBValue(clr)));

	static constexpr double rWeight = 0.2126;
	static constexpr double gWeight = 0.7152;
	static constexpr double bWeight = 0.0722;

	const double luminance = (rWeight * r) + (gWeight * g) + (bWeight * b);

	static constexpr double cieEpsilon = 216.0 / 24389.0;
	static constexpr double cieKappa = 24389.0 / 27.0;
	static constexpr double oneThird = 1.0 / 3.0;
	static constexpr double scalingFactor = 116.0;
	static constexpr double offset = 16.0;

	// calculate lightness

	if (luminance <= cieEpsilon)
	{
		return (luminance * cieKappa);
	}
	return ((std::pow(luminance, oneThird) * scalingFactor) - offset);
}

static COLORREF adjustClrLightness(COLORREF clr, bool useDark) noexcept
{
	WORD h = 0;
	WORD s = 0;
	WORD l = 0;
	::ColorRGBToHLS(clr, &h, &l, &s);

	static constexpr double lightnessThreshold = 50.0 - 3.0;
	static constexpr WORD saturationAdjustment = 20;
	static constexpr WORD luminanceAdjustment = 50;

	if (dmlib_color::calculatePerceivedLightness(clr) < lightnessThreshold)
	{
		s -= saturationAdjustment;
		l += luminanceAdjustment;
		return useDark ? ::ColorHLSToRGB(h, l, s) : clr;
	}
	else
	{
		s += saturationAdjustment;
		l -= luminanceAdjustment;
		return useDark ? clr : ::ColorHLSToRGB(h, l, s);
	}
}

COLORREF dmlib_color::getAccentColor(bool adjust) noexcept
{
	BOOL opaque = TRUE;
	COLORREF clrAccent = 0;

	if (FAILED(::DwmGetColorizationColor(&clrAccent, &opaque)))
	{
		return CLR_INVALID;
	}

	// DwmGetColorizationColor use 0xAARRGGBB format
	clrAccent = RGB(GetBValue(clrAccent), GetGValue(clrAccent), GetRValue(clrAccent));

	if (adjust)
	{
		clrAccent = adjustClrLightness(clrAccent, dmlib_win32api::IsDarkModeActive());
	}

	return clrAccent;
}
