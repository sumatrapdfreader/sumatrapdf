// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Copyright (c) 2024-2025 ozone10
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


// Based on Notepad++ dark mode code, original by adzm / Adam D. Walling
// with modification from Notepad++ team.
// Heavily modified by ozone10 (contributor of Notepad++)


#include "StdAfx.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DarkModeSubclass.h"

#if !defined(_DARKMODELIB_NOT_USED)

#include <dwmapi.h>
#include <richedit.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <windowsx.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>

#include "DarkMode.h"
#include "UAHMenuBar.h"

#include "Version.h"

#if defined(__GNUC__)
#include <cstdint>
//static constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
static constexpr int CP_DROPDOWNITEM = 9; // for some reason mingw use only enum up to 8
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

//#ifndef WM_DPICHANGED_BEFOREPARENT
//#define WM_DPICHANGED_BEFOREPARENT 0x02E2
//#endif

#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

//#ifndef WM_GETDPISCALEDSIZE
//#define WM_GETDPISCALEDSIZE 0x02E4
//#endif

/// Converts 0xRRGGBB to COLORREF (0xBBGGRR) for GDI usage.
static constexpr COLORREF HEXRGB(DWORD rrggbb)
{
	return
		((rrggbb & 0xFF0000) >> 16) |
		((rrggbb & 0x00FF00)) |
		((rrggbb & 0x0000FF) << 16);
}

/**
 * @brief Retrieves the class name of a given window.
 *
 * This function wraps the Win32 API `GetClassNameW` to return the class name
 * of a window as a wide string (`std::wstring`).
 *
 * @param hWnd Handle to the target window.
 * @return The class name of the window as a `std::wstring`.
 *
 * @note The maximum length is capped at 32 characters (including the null terminator),
 *       which suffices for standard Windows window classes.
 */
static std::wstring GetWndClassName(HWND hWnd)
{
	static constexpr int strLen = 32;
	std::wstring className(strLen, L'\0');
	className.resize(static_cast<size_t>(::GetClassNameW(hWnd, className.data(), strLen)));
	return className;
}

/**
 * @brief Compares the class name of a window with a specified string.
 *
 * This function retrieves the class name of the given window handle
 * and compares it to the provided class name.
 *
 * @param hWnd Handle to the window whose class name is to be checked.
 * @param classNameToCmp Pointer to a null-terminated wide string representing the class name to compare against.
 * @return `true` if the window's class name matches the specified string; otherwise `false`.
 *
 * @see GetWndClassName()
 */
static bool CmpWndClassName(HWND hWnd, const wchar_t* classNameToCmp)
{
	return (GetWndClassName(hWnd) == classNameToCmp);
}

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
/**
 * @brief Constructs a full path to an `.ini` file located next to the executable.
 *
 * Retrieves the directory of the current module (executable or DLL) and appends
 * the specified `.ini` filename to it.
 *
 * @param iniFilename The base name of the `.ini` file (without path or extension).
 * @return Full path to the `.ini` file as a wide string, or an empty string on failure.
 *
 * @note Returns a path like: `C:\\Path\\To\\Executable\\YourFile.ini`
 */
static std::wstring GetIniPath(const std::wstring& iniFilename)
{
	std::array<wchar_t, MAX_PATH> buffer{};
	const auto strLen = static_cast<size_t>(::GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH));
	if (strLen == 0)
	{
		return L"";
	}

	wchar_t* lastSlash = std::wcsrchr(buffer.data(), L'\\');
	if (lastSlash == nullptr)
	{
		return L"";
	}

	*lastSlash = L'\0';
	std::wstring iniPath(buffer.data());
	iniPath += L"\\" + iniFilename + L".ini";
	return iniPath;
}

/**
 * @brief Checks whether a file exists at the specified path.
 *
 * Determines if the given file path exists and refers to a regular file.
 *
 * @param filePath Path to the file to check.
 * @return `true` if the file exists and is not a directory, otherwise `false`.
 */
static bool FileExists(const std::wstring& filePath)
{
	const DWORD dwAttrib = ::GetFileAttributesW(filePath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Reads a color value from an `.ini` file and converts it to a `COLORREF`.
 *
 * Reads a 6-digit hex color string from the specified section and key, then parses
 * it as a Windows GDI `COLORREF` value.
 *
 * @param sectionName Section within the `.ini` file.
 * @param keyName Key name containing the hex RGB value (e.g., "E0E2E4").
 * @param iniFilePath Full path to the `.ini` file.
 * @param clr Pointer to a `COLORREF` where the parsed color will be stored. **Must not be `nullptr`.**
 * @return `true` if a valid 6-digit hex color was read and parsed, otherwise `false`.
 *
 * @note The value must be exactly 6 hexadecimal digits and represent an RGB color.
 */
static bool SetClrFromIni(const std::wstring& sectionName, const std::wstring& keyName, const std::wstring& iniFilePath, COLORREF* clr)
{
	static constexpr size_t maxStrLen = 6;
	std::wstring buffer(maxStrLen + 1, L'\0');

	const auto len = static_cast<size_t>(::GetPrivateProfileStringW(
		sectionName.c_str()
		, keyName.c_str()
		, L""
		, buffer.data()
		, static_cast<DWORD>(buffer.size())
		, iniFilePath.c_str()));

	if (len != maxStrLen)
	{
		return false;
	}

	buffer.resize(len); // remove extra '\0'

	for (const auto& wch : buffer)
	{
		if (iswxdigit(wch) == 0)
		{
			return false;
		}
	}

	try
	{
		static constexpr int baseHex = 16;
		*clr = HEXRGB(std::stoul(buffer, nullptr, baseHex));
	}
	catch (const std::exception&)
	{
		return false;
	}

	return true;
}
#endif // !defined(_DARKMODELIB_NO_INI_CONFIG)

namespace DarkMode
{
	/**
	 * @brief Returns library version information or compile-time feature flags.
	 *
	 * Responds to the specified query by returning either:
	 * - Version numbers (`verMajor`, `verMinor`, `verRevision`)
	 * - Build configuration flags (returns `TRUE` or `FALSE`)
	 * - A constant value (`featureCheck`, `maxValue`) used for validation
	 *
	 * @param libInfoType Enum value specifying which piece of information to retrieve.
	 * @return Integer value:
	 * - Version: as defined by `DM_VERSION_MAJOR`, etc.
	 * - Boolean flags: `TRUE` (1) if the feature is enabled, `FALSE` (0) otherwise.
	 * - `featureCheck`, `maxValue`: returns the numeric max enum value.
	 * - `-1`: for invalid or unhandled enum cases (should not occur in correct usage).
	 *
	 * @see LibInfo
	 */
	int getLibInfo(LibInfo libInfoType)
	{
		switch (libInfoType)
		{
			case LibInfo::maxValue:
			case LibInfo::featureCheck:
			{
				return static_cast<int>(LibInfo::maxValue);
			}

			case LibInfo::verMajor:
			{
				return DM_VERSION_MAJOR;
			}

			case LibInfo::verMinor:
			{
				return DM_VERSION_MINOR;
			}

			case LibInfo::verRevision:
			{
				return DM_VERSION_REVISION;
			}

			case LibInfo::iathookExternal:
			{
#if defined(_DARKMODELIB_EXTERNAL_IATHOOK)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::iniConfigUsed:
			{
#if !defined(_DARKMODELIB_NO_INI_CONFIG)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::allowOldOS:
			{
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::useDlgProcCtl:
			{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
				return TRUE;
#else
				return FALSE;
#endif
			}

			case LibInfo::preferTheme:
			{
#if defined(_DARKMODELIB_PREFER_THEME)
				return TRUE;
#else
				return FALSE;
#endif
			}
		}
		return -1; // should never happen
	}

	/**
	 * @brief Defines the available dark mode types.
	 *
	 * Used internally to distinguish between light, dark, and classic modes.
	 */
	enum class DarkModeType : std::uint8_t
	{
		light   = 0,  ///< Light mode appearance.
		dark    = 1,  ///< Dark mode appearance.
		//windows = 2, // never used
		classic = 3   ///< Classic (non-themed or system) appearance.
	};

	/**
	 * @brief Describes how the application responds to the system theme.
	 *
	 * Used to determine behavior when following the system's light/dark mode setting.
	 * - `disabled`: Do not follow system; use manually selected appearance.
	 * - `light`: Follow system mode; apply light theme when system is in light mode.
	 * - `classic`: Follow system mode; apply classic style when system is in light mode.
	 */
	enum class WinMode : std::uint8_t
	{
		disabled,  ///< Manual — system mode is ignored.
		light,     ///< Use light theme if system is in light mode.
		classic    ///< Use classic style if system is in light mode.
	};

	static constexpr UINT_PTR kButtonSubclassID                 = 42;
	static constexpr UINT_PTR kGroupboxSubclassID               = 1;
	static constexpr UINT_PTR kUpDownSubclassID                 = 2;
	static constexpr UINT_PTR kTabPaintSubclassID               = 3;
	static constexpr UINT_PTR kTabUpDownSubclassID              = 4;
	static constexpr UINT_PTR kCustomBorderSubclassID           = 5;
	static constexpr UINT_PTR kComboBoxSubclassID               = 6;
	static constexpr UINT_PTR kComboBoxExSubclassID             = 7;
	static constexpr UINT_PTR kListViewSubclassID               = 8;
	static constexpr UINT_PTR kHeaderSubclassID                 = 9;
	static constexpr UINT_PTR kStatusBarSubclassID              = 10;
	static constexpr UINT_PTR kProgressBarSubclassID            = 11;
	static constexpr UINT_PTR kStaticTextSubclassID             = 12;
	static constexpr UINT_PTR kWindowEraseBgSubclassID          = 13;
	static constexpr UINT_PTR kWindowCtlColorSubclassID         = 14;
	static constexpr UINT_PTR kWindowNotifySubclassID           = 15;
	static constexpr UINT_PTR kWindowMenuBarSubclassID          = 16;
	static constexpr UINT_PTR kWindowSettingChangeSubclassID    = 17;

	struct DarkModeParams
	{
		const wchar_t* _themeClassName = nullptr;
		bool _subclass = false;
		bool _theme = false;
	};

	static constexpr int kWin11CornerRoundness = 4;

	/// Threshold range around 50.0 where TreeView uses classic style instead of light/dark.
	static constexpr double kMiddleGrayRange = 2.0;

	namespace // anonymous
	{
		struct
		{
			DWM_WINDOW_CORNER_PREFERENCE _roundCorner = DWMWCP_DEFAULT;
			COLORREF _borderColor = DWMWA_COLOR_DEFAULT;
			DWM_SYSTEMBACKDROP_TYPE _mica = DWMSBT_AUTO;
			COLORREF _tvBackground = RGB(41, 49, 52);
			double _lightness = 50.0;
			TreeViewStyle _tvStylePrev = TreeViewStyle::classic;
			TreeViewStyle _tvStyle = TreeViewStyle::classic;
			bool _micaExtend = false;
			DarkModeType _dmType = DarkModeType::dark;
			WinMode _windowsMode = WinMode::disabled;
			bool _isInit = false;
			bool _isInitExperimental = false;

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
			bool _isIniNameSet = false;
			std::wstring _iniName;
#endif
		} g_dmCfg;
	} // anonymous namespace

	struct Brushes
	{
		HBRUSH _background = nullptr;
		HBRUSH _ctrlBackground = nullptr;
		HBRUSH _hotBackground = nullptr;
		HBRUSH _dlgBackground = nullptr;
		HBRUSH _errorBackground = nullptr;

		HBRUSH _edge = nullptr;
		HBRUSH _hotEdge = nullptr;
		HBRUSH _disabledEdge = nullptr;

		Brushes() = delete;

		explicit Brushes(const Colors& colors) noexcept
			: _background(::CreateSolidBrush(colors.background))
			, _ctrlBackground(::CreateSolidBrush(colors.ctrlBackground))
			, _hotBackground(::CreateSolidBrush(colors.hotBackground))
			, _dlgBackground(::CreateSolidBrush(colors.dlgBackground))
			, _errorBackground(::CreateSolidBrush(colors.errorBackground))

			, _edge(::CreateSolidBrush(colors.edge))
			, _hotEdge(::CreateSolidBrush(colors.hotEdge))
			, _disabledEdge(::CreateSolidBrush(colors.disabledEdge))
		{}

		Brushes(const Brushes&) = delete;
		Brushes& operator=(const Brushes&) = delete;

		Brushes(Brushes&&) = delete;
		Brushes& operator=(Brushes&&) = delete;

		~Brushes()
		{
			::DeleteObject(_background);         _background = nullptr;
			::DeleteObject(_ctrlBackground);     _ctrlBackground = nullptr;
			::DeleteObject(_hotBackground);      _hotBackground = nullptr;
			::DeleteObject(_dlgBackground);      _dlgBackground = nullptr;
			::DeleteObject(_errorBackground);    _errorBackground = nullptr;

			::DeleteObject(_edge);               _edge = nullptr;
			::DeleteObject(_hotEdge);            _hotEdge = nullptr;
			::DeleteObject(_disabledEdge);       _disabledEdge = nullptr;
		}

		void updateBrushes(const Colors& colors) noexcept
		{
			::DeleteObject(_background);
			::DeleteObject(_ctrlBackground);
			::DeleteObject(_hotBackground);
			::DeleteObject(_dlgBackground);
			::DeleteObject(_errorBackground);

			::DeleteObject(_edge);
			::DeleteObject(_hotEdge);
			::DeleteObject(_disabledEdge);

			_background = ::CreateSolidBrush(colors.background);
			_ctrlBackground = ::CreateSolidBrush(colors.ctrlBackground);
			_hotBackground = ::CreateSolidBrush(colors.hotBackground);
			_dlgBackground = ::CreateSolidBrush(colors.dlgBackground);
			_errorBackground = ::CreateSolidBrush(colors.errorBackground);

			_edge = ::CreateSolidBrush(colors.edge);
			_hotEdge = ::CreateSolidBrush(colors.hotEdge);
			_disabledEdge = ::CreateSolidBrush(colors.disabledEdge);
		}
	};

	struct Pens
	{
		HPEN _darkerText = nullptr;
		HPEN _edge = nullptr;
		HPEN _hotEdge = nullptr;
		HPEN _disabledEdge = nullptr;

		Pens() = delete;

		explicit Pens(const Colors& colors) noexcept
			: _darkerText(::CreatePen(PS_SOLID, 1, colors.darkerText))
			, _edge(::CreatePen(PS_SOLID, 1, colors.edge))
			, _hotEdge(::CreatePen(PS_SOLID, 1, colors.hotEdge))
			, _disabledEdge(::CreatePen(PS_SOLID, 1, colors.disabledEdge))
		{}

		Pens(const Pens&) = delete;
		Pens& operator=(const Pens&) = delete;

		Pens(Pens&&) = delete;
		Pens& operator=(Pens&&) = delete;

		~Pens()
		{
			::DeleteObject(_darkerText);    _darkerText = nullptr;
			::DeleteObject(_edge);          _edge = nullptr;
			::DeleteObject(_hotEdge);       _hotEdge = nullptr;
			::DeleteObject(_disabledEdge);  _disabledEdge = nullptr;
		}

		void updatePens(const Colors& colors) noexcept
		{
			::DeleteObject(_darkerText);
			::DeleteObject(_edge);
			::DeleteObject(_hotEdge);
			::DeleteObject(_disabledEdge);

			_darkerText = ::CreatePen(PS_SOLID, 1, colors.darkerText);
			_edge = ::CreatePen(PS_SOLID, 1, colors.edge);
			_hotEdge = ::CreatePen(PS_SOLID, 1, colors.hotEdge);
			_disabledEdge = ::CreatePen(PS_SOLID, 1, colors.disabledEdge);
		}
	};

	/// Black tone (default)
	static constexpr Colors darkColors{
		HEXRGB(0x202020),   // background
		HEXRGB(0x383838),   // ctrlBackground
		HEXRGB(0x454545),   // hotBackground
		HEXRGB(0x202020),   // dlgBackground
		HEXRGB(0xB00000),   // errorBackground
		HEXRGB(0xE0E0E0),   // textColor
		HEXRGB(0xC0C0C0),   // darkerTextColor
		HEXRGB(0x808080),   // disabledTextColor
		HEXRGB(0xFFFF00),   // linkTextColor
		HEXRGB(0x646464),   // edgeColor
		HEXRGB(0x9B9B9B),   // hotEdgeColor
		HEXRGB(0x484848)    // disabledEdgeColor
	};

	static constexpr DWORD offsetEdge = HEXRGB(0x1C1C1C);

	/// Red tone
	static constexpr DWORD offsetRed = HEXRGB(0x100000);
	static constexpr Colors darkRedColors{
		darkColors.background + offsetRed,
		darkColors.ctrlBackground + offsetRed,
		darkColors.hotBackground + offsetRed,
		darkColors.dlgBackground + offsetRed,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetRed,
		darkColors.hotEdge + offsetRed,
		darkColors.disabledEdge + offsetRed
	};

	/// Green tone
	static constexpr DWORD offsetGreen = HEXRGB(0x001000);
	static constexpr Colors darkGreenColors{
		darkColors.background + offsetGreen,
		darkColors.ctrlBackground + offsetGreen,
		darkColors.hotBackground + offsetGreen,
		darkColors.dlgBackground + offsetGreen,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetGreen,
		darkColors.hotEdge + offsetGreen,
		darkColors.disabledEdge + offsetGreen
	};

	/// Blue tone
	static constexpr DWORD offsetBlue = HEXRGB(0x000020);
	static constexpr Colors darkBlueColors{
		darkColors.background + offsetBlue,
		darkColors.ctrlBackground + offsetBlue,
		darkColors.hotBackground + offsetBlue,
		darkColors.dlgBackground + offsetBlue,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetBlue,
		darkColors.hotEdge + offsetBlue,
		darkColors.disabledEdge + offsetBlue
	};

	/// Purple tone
	static constexpr DWORD offsetPurple = HEXRGB(0x100020);
	static constexpr Colors darkPurpleColors{
		darkColors.background + offsetPurple,
		darkColors.ctrlBackground + offsetPurple,
		darkColors.hotBackground + offsetPurple,
		darkColors.dlgBackground + offsetPurple,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetPurple,
		darkColors.hotEdge + offsetPurple,
		darkColors.disabledEdge + offsetPurple
	};

	/// Cyan tone
	static constexpr DWORD offsetCyan = HEXRGB(0x001020);
	static constexpr Colors darkCyanColors{
		darkColors.background + offsetCyan,
		darkColors.ctrlBackground + offsetCyan,
		darkColors.hotBackground + offsetCyan,
		darkColors.dlgBackground + offsetCyan,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetCyan,
		darkColors.hotEdge + offsetCyan,
		darkColors.disabledEdge + offsetCyan
	};

	/// Olive tone
	static constexpr DWORD offsetOlive = HEXRGB(0x101000);
	static constexpr Colors darkOliveColors{
		darkColors.background + offsetOlive,
		darkColors.ctrlBackground + offsetOlive,
		darkColors.hotBackground + offsetOlive,
		darkColors.dlgBackground + offsetOlive,
		darkColors.errorBackground,
		darkColors.text,
		darkColors.darkerText,
		darkColors.disabledText,
		darkColors.linkText,
		darkColors.edge + offsetEdge + offsetOlive,
		darkColors.hotEdge + offsetOlive,
		darkColors.disabledEdge + offsetOlive
	};

	/// Light tone
	static Colors getLightColors()
	{
		const Colors lightColors{
		::GetSysColor(COLOR_3DFACE),        // background
		::GetSysColor(COLOR_WINDOW),        // ctrlBackground
		HEXRGB(0xC0DCF3),                   // hotBackground
		::GetSysColor(COLOR_3DFACE),        // dlgBackground
		HEXRGB(0xA01000),                   // errorBackground
		::GetSysColor(COLOR_WINDOWTEXT),    // textColor
		::GetSysColor(COLOR_BTNTEXT),       // darkerTextColor
		::GetSysColor(COLOR_GRAYTEXT),      // disabledTextColor
		::GetSysColor(COLOR_HOTLIGHT),      // linkTextColor
		HEXRGB(0x8D8D8D),                   // edgeColor
		::GetSysColor(COLOR_HIGHLIGHT),     // hotEdgeColor
		::GetSysColor(COLOR_GRAYTEXT)       // disabledEdgeColor
		};

		return lightColors;
	}

	class Theme
	{
	public:
		Theme() noexcept
			: _colors(darkColors)
			, _brushes(darkColors)
			, _pens(darkColors)
		{}

		explicit Theme(const Colors& colors) noexcept
			: _colors(colors)
			, _brushes(colors)
			, _pens(colors)
		{}

		void updateTheme()
		{
			_brushes.updateBrushes(_colors);
			_pens.updatePens(_colors);
		}

		void updateTheme(Colors colors)
		{
			_colors = colors;
			Theme::updateTheme();
		}

		[[nodiscard]] Colors getToneColors() const
		{
			switch (_tone)
			{
				case ColorTone::red:
				{
					return darkRedColors;
				}

				case ColorTone::green:
				{
					return darkGreenColors;
				}

				case ColorTone::blue:
				{
					return darkBlueColors;
				}

				case ColorTone::purple:
				{
					return darkPurpleColors;
				}

				case ColorTone::cyan:
				{
					return darkCyanColors;
				}

				case ColorTone::olive:
				{
					return darkOliveColors;
				}

				case ColorTone::black:
				case ColorTone::max:
				{
					break;
				}
			}
			return darkColors;
		}

		void setToneColors(ColorTone colorTone)
		{
			_tone = colorTone;

			switch (_tone)
			{
				case ColorTone::red:
				{
					_colors = darkRedColors;
					break;
				}

				case ColorTone::green:
				{
					_colors = darkGreenColors;
					break;
				}

				case ColorTone::blue:
				{
					_colors = darkBlueColors;
					break;
				}

				case ColorTone::purple:
				{
					_colors = darkPurpleColors;
					break;
				}

				case ColorTone::cyan:
				{
					_colors = darkCyanColors;
					break;
				}

				case ColorTone::olive:
				{
					_colors = darkOliveColors;
					break;
				}

				case ColorTone::black:
				case ColorTone::max:
				{
					_colors = darkColors;
					break;
				}
			}

			Theme::updateTheme();
		}

		void setToneColors()
		{
			_colors = Theme::getToneColors();
			Theme::updateTheme();
		}

		[[nodiscard]] const Brushes& getBrushes() const
		{
			return _brushes;
		}

		[[nodiscard]] const Pens& getPens() const
		{
			return _pens;
		}

		[[nodiscard]] const ColorTone& getColorTone() const
		{
			return _tone;
		}

		Colors _colors;

	private:
		Brushes _brushes;
		Pens _pens;
		ColorTone _tone = DarkMode::ColorTone::black;
	};

	static Theme tMain;

	static Theme& getTheme()
	{
		return tMain;
	}

	ColorTone getColorTone()
	{
		return DarkMode::getTheme().getColorTone();
	}

	static constexpr ColorsView darkColorsView{
		HEXRGB(0x293134),   // background
		HEXRGB(0xE0E2E4),   // text
		HEXRGB(0x646464),   // gridlines
		HEXRGB(0x202020),   // Header background
		HEXRGB(0x454545),   // Header hot background
		HEXRGB(0xC0C0C0),   // header text
		HEXRGB(0x646464)    // header divider
	};

	static constexpr ColorsView lightColorsView{
		HEXRGB(0xFFFFFF),   // background
		HEXRGB(0x000000),   // text
		HEXRGB(0xF0F0F0),   // gridlines
		HEXRGB(0xFFFFFF),   // header background
		HEXRGB(0xD9EBF9),   // header hot background
		HEXRGB(0x000000),   // header text
		HEXRGB(0xE5E5E5)    // header divider
	};

	struct BrushesAndPensView
	{
		HBRUSH _background = nullptr;
		HBRUSH _gridlines = nullptr;
		HBRUSH _headerBackground = nullptr;
		HBRUSH _headerHotBackground = nullptr;

		HPEN _headerEdge = nullptr;

		BrushesAndPensView() = delete;

		explicit BrushesAndPensView(const ColorsView& colors) noexcept
			: _background(::CreateSolidBrush(colors.background))
			, _gridlines(::CreateSolidBrush(colors.gridlines))
			, _headerBackground(::CreateSolidBrush(colors.headerBackground))
			, _headerHotBackground(::CreateSolidBrush(colors.headerHotBackground))

			, _headerEdge(::CreatePen(PS_SOLID, 1, colors.headerEdge))
		{}

		BrushesAndPensView(const BrushesAndPensView&) = delete;
		BrushesAndPensView& operator=(const BrushesAndPensView&) = delete;

		BrushesAndPensView(BrushesAndPensView&&) = delete;
		BrushesAndPensView& operator=(BrushesAndPensView&&) = delete;

		~BrushesAndPensView()
		{
			::DeleteObject(_background);             _background = nullptr;
			::DeleteObject(_gridlines);              _gridlines = nullptr;
			::DeleteObject(_headerBackground);       _headerBackground = nullptr;
			::DeleteObject(_headerHotBackground);    _headerHotBackground = nullptr;

			::DeleteObject(_headerEdge);             _headerEdge = nullptr;
		}

		void update(const ColorsView& colors)
		{
			::DeleteObject(_background);
			::DeleteObject(_gridlines);
			::DeleteObject(_headerBackground);
			::DeleteObject(_headerHotBackground);

			_background = ::CreateSolidBrush(colors.background);
			_gridlines = ::CreateSolidBrush(colors.gridlines);
			_headerBackground = ::CreateSolidBrush(colors.headerBackground);
			_headerHotBackground = ::CreateSolidBrush(colors.headerHotBackground);

			::DeleteObject(_headerEdge);

			_headerEdge = ::CreatePen(PS_SOLID, 1, colors.headerEdge);
		}
	};

	class ThemeView
	{
	public:
		ThemeView() noexcept
			: _clrView(darkColorsView)
			, _hbrPnView(darkColorsView)
		{}

		explicit ThemeView(const ColorsView& colorsView) noexcept
			: _clrView(colorsView)
			, _hbrPnView(colorsView)
		{}

		void updateView()
		{
			_hbrPnView.update(_clrView);
		}

		void updateView(ColorsView colors)
		{
			_clrView = colors;
			ThemeView::updateView();
		}

		[[nodiscard]] const BrushesAndPensView& getViewBrushesAndPens() const
		{
			return _hbrPnView;
		}

		ColorsView _clrView;

	private:
		BrushesAndPensView _hbrPnView;
	};

	static ThemeView tView;

	static ThemeView& getThemeView()
	{
		return tView;
	}

	static COLORREF setNewColor(COLORREF* clrOld, COLORREF clrNew)
	{
		const auto clrTmp = *clrOld;
		*clrOld = clrNew;
		return clrTmp;
	}

	COLORREF setBackgroundColor(COLORREF clrNew)        { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.background, clrNew); }
	COLORREF setCtrlBackgroundColor(COLORREF clrNew)    { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.ctrlBackground, clrNew); }
	COLORREF setHotBackgroundColor(COLORREF clrNew)     { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.hotBackground, clrNew); }
	COLORREF setDlgBackgroundColor(COLORREF clrNew)     { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.dlgBackground, clrNew); }
	COLORREF setErrorBackgroundColor(COLORREF clrNew)   { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.errorBackground, clrNew); }
	COLORREF setTextColor(COLORREF clrNew)              { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.text, clrNew); }
	COLORREF setDarkerTextColor(COLORREF clrNew)        { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.darkerText, clrNew); }
	COLORREF setDisabledTextColor(COLORREF clrNew)      { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.disabledText, clrNew); }
	COLORREF setLinkTextColor(COLORREF clrNew)          { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.linkText, clrNew); }
	COLORREF setEdgeColor(COLORREF clrNew)              { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.edge, clrNew); }
	COLORREF setHotEdgeColor(COLORREF clrNew)           { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.hotEdge, clrNew); }
	COLORREF setDisabledEdgeColor(COLORREF clrNew)      { return DarkMode::setNewColor(&DarkMode::getTheme()._colors.disabledEdge, clrNew); }

	void setThemeColors(Colors colors)
	{
		DarkMode::getTheme().updateTheme(colors);
	}

	void updateThemeBrushesAndPens()
	{
		DarkMode::getTheme().updateTheme();
	}

	COLORREF getBackgroundColor()         { return getTheme()._colors.background; }
	COLORREF getCtrlBackgroundColor()     { return getTheme()._colors.ctrlBackground; }
	COLORREF getHotBackgroundColor()      { return getTheme()._colors.hotBackground; }
	COLORREF getDlgBackgroundColor()      { return getTheme()._colors.dlgBackground; }
	COLORREF getErrorBackgroundColor()    { return getTheme()._colors.errorBackground; }
	COLORREF getTextColor()               { return getTheme()._colors.text; }
	COLORREF getDarkerTextColor()         { return getTheme()._colors.darkerText; }
	COLORREF getDisabledTextColor()       { return getTheme()._colors.disabledText; }
	COLORREF getLinkTextColor()           { return getTheme()._colors.linkText; }
	COLORREF getEdgeColor()               { return getTheme()._colors.edge; }
	COLORREF getHotEdgeColor()            { return getTheme()._colors.hotEdge; }
	COLORREF getDisabledEdgeColor()       { return getTheme()._colors.disabledEdge; }

	HBRUSH getBackgroundBrush()           { return getTheme().getBrushes()._background; }
	HBRUSH getCtrlBackgroundBrush()       { return getTheme().getBrushes()._ctrlBackground; }
	HBRUSH getHotBackgroundBrush()        { return getTheme().getBrushes()._hotBackground; }
	HBRUSH getDlgBackgroundBrush()        { return getTheme().getBrushes()._dlgBackground; }
	HBRUSH getErrorBackgroundBrush()      { return getTheme().getBrushes()._errorBackground; }

	HBRUSH getEdgeBrush()                 { return getTheme().getBrushes()._edge; }
	HBRUSH getHotEdgeBrush()              { return getTheme().getBrushes()._hotEdge; }
	HBRUSH getDisabledEdgeBrush()         { return getTheme().getBrushes()._disabledEdge; }

	HPEN getDarkerTextPen()               { return getTheme().getPens()._darkerText; }
	HPEN getEdgePen()                     { return getTheme().getPens()._edge; }
	HPEN getHotEdgePen()                  { return getTheme().getPens()._hotEdge; }
	HPEN getDisabledEdgePen()             { return getTheme().getPens()._disabledEdge; }

	COLORREF setViewBackgroundColor(COLORREF clrNew)        { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.background, clrNew); }
	COLORREF setViewTextColor(COLORREF clrNew)              { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.text, clrNew); }
	COLORREF setViewGridlinesColor(COLORREF clrNew)         { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.gridlines, clrNew); }

	COLORREF setHeaderBackgroundColor(COLORREF clrNew)      { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.headerBackground, clrNew); }
	COLORREF setHeaderHotBackgroundColor(COLORREF clrNew)   { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.headerHotBackground, clrNew); }
	COLORREF setHeaderTextColor(COLORREF clrNew)            { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.headerText, clrNew); }
	COLORREF setHeaderEdgeColor(COLORREF clrNew)            { return DarkMode::setNewColor(&DarkMode::getThemeView()._clrView.headerEdge, clrNew); }

	void setViewColors(ColorsView colors)
	{
		DarkMode::getThemeView().updateView(colors);
	}

	void updateViewBrushesAndPens()
	{
		DarkMode::getThemeView().updateView();
	}

	COLORREF getViewBackgroundColor()       { return DarkMode::getThemeView()._clrView.background; }
	COLORREF getViewTextColor()             { return DarkMode::getThemeView()._clrView.text; }
	COLORREF getViewGridlinesColor()        { return DarkMode::getThemeView()._clrView.gridlines; }

	COLORREF getHeaderBackgroundColor()     { return DarkMode::getThemeView()._clrView.headerBackground; }
	COLORREF getHeaderHotBackgroundColor()  { return DarkMode::getThemeView()._clrView.headerHotBackground; }
	COLORREF getHeaderTextColor()           { return DarkMode::getThemeView()._clrView.headerText; }
	COLORREF getHeaderEdgeColor()           { return DarkMode::getThemeView()._clrView.headerEdge; }

	HBRUSH getViewBackgroundBrush()         { return DarkMode::getThemeView().getViewBrushesAndPens()._background; }
	HBRUSH getViewGridlinesBrush()          { return DarkMode::getThemeView().getViewBrushesAndPens()._gridlines; }

	HBRUSH getHeaderBackgroundBrush()       { return DarkMode::getThemeView().getViewBrushesAndPens()._headerBackground; }
	HBRUSH getHeaderHotBackgroundBrush()    { return DarkMode::getThemeView().getViewBrushesAndPens()._headerHotBackground; }

	HPEN getHeaderEdgePen()                 { return DarkMode::getThemeView().getViewBrushesAndPens()._headerEdge; }

	/**
	 * @brief Initializes the dark mode configuration based on the selected mode.
	 *
	 * Sets the active dark mode rendering and system-following behavior according to the specified `dmType`:
	 * - `0`: Light mode, do not follow system.
	 * - `1` or default: Dark mode, do not follow system.
	 * - `2`: *[Internal]* Follow system — light or dark depending on registry (see `DarkMode::isDarkModeReg()`).
	 * - `3`: Classic mode, do not follow system.
	 * - `4`: *[Internal]* Follow system — classic or dark depending on registry.
	 *
	 * @param dmType Integer representing the desired mode.
	 *
	 * @see DarkModeType
	 * @see WinMode
	 * @see DarkMode::isDarkModeReg()
	 */
	void initDarkModeConfig(UINT dmType)
	{
		switch (dmType)
		{
			case 0:
			{
				g_dmCfg._dmType = DarkModeType::light;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}

			case 2:
			{
				g_dmCfg._dmType = DarkMode::isDarkModeReg() ? DarkModeType::dark : DarkModeType::light;
				g_dmCfg._windowsMode = WinMode::light;
				break;
			}

			case 3:
			{
				g_dmCfg._dmType = DarkModeType::classic;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}

			case 4:
			{
				g_dmCfg._dmType = DarkMode::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic;
				g_dmCfg._windowsMode = WinMode::classic;
				break;
			}

			case 1:
			default:
			{
				g_dmCfg._dmType = DarkModeType::dark;
				g_dmCfg._windowsMode = WinMode::disabled;
				break;
			}
		}
	}

	/**
	 * @brief Sets the preferred window corner style on Windows 11.
	 *
	 * Assigns a valid `DWM_WINDOW_CORNER_PREFERENCE` value to the config,
	 * falling back to `DWMWCP_DEFAULT` if the input is out of range.
	 *
	 * @param roundCornerStyle Integer value representing a `DWM_WINDOW_CORNER_PREFERENCE`.
	 *
	 * @see https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference
	 */
	void setRoundCornerConfig(UINT roundCornerStyle)
	{
		const auto cornerStyle = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(roundCornerStyle);
		if (cornerStyle > DWMWCP_ROUNDSMALL) // || cornerStyle < DWMWCP_DEFAULT) // should never be < 0
		{
			g_dmCfg._roundCorner = DWMWCP_DEFAULT;
		}
		else
		{
			g_dmCfg._roundCorner = cornerStyle;
		}
	}

	static constexpr DWORD kDwmwaClrDefaultRGBCheck = 0x00FFFFFF;

	/**
	 * @brief Sets the preferred border color for window edge on Windows 11.
	 *
	 * Assigns the given `COLORREF` to the configuration. If the value matches
	 * `kDwmwaClrDefaultRGBCheck`, the color is reset to `DWMWA_COLOR_DEFAULT`.
	 *
	 * @param clr Border color value, or sentinel to reset to system default.
	 *
	 * @see DWMWA_BORDER_COLOR
	 */
	void setBorderColorConfig(COLORREF clr)
	{
		if (clr == kDwmwaClrDefaultRGBCheck)
		{
			g_dmCfg._borderColor = DWMWA_COLOR_DEFAULT;
		}
		else
		{
			g_dmCfg._borderColor = clr;
		}
	}

	/**
	 * @brief Sets the Mica effects on Windows 11.
	 *
	 * Assigns a valid `DWM_SYSTEMBACKDROP_TYPE` to the configuration. If the value exceeds
	 * `DWMSBT_TABBEDWINDOW`, it falls back to `DWMSBT_AUTO`.
	 *
	 * @param mica Integer value representing a `DWM_SYSTEMBACKDROP_TYPE`.
	 *
	 * @see DWM_SYSTEMBACKDROP_TYPE
	 */
	void setMicaConfig(UINT mica)
	{
		const auto micaType = static_cast<DWM_SYSTEMBACKDROP_TYPE>(mica);
		if (micaType > DWMSBT_TABBEDWINDOW) // || micaType < DWMSBT_AUTO)  // should never be < 0
		{
			g_dmCfg._mica = DWMSBT_AUTO;
		}
		else
		{
			g_dmCfg._mica = micaType;
		}
	}

	/**
	 * @brief Applies Mica effects on the full window.
	 *
	 * Controls whether Mica should be applied to the entire window
	 * or limited to the title bar only.
	 *
	 * @param extendMica `true` to apply Mica to the full window, `false` for title bar only.
	 */
	void setMicaExtendedConfig(bool extendMica)
	{
		g_dmCfg._micaExtend = extendMica;
	}

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
	 /**
	  * @brief Initializes dark mode configuration and colors from an INI file.
	  *
	  * Loads configuration values from the specified INI file path and applies them to the
	  * current dark mode settings. This includes:
	  * - Base appearance (`DarkModeType`) and system-following mode (`WinMode`)
	  * - Optional Mica and rounded corner styling
	  * - Custom colors for background, text, borders, and headers (if present)
	  * - Tone settings for dark theme (`ColorTone`)
	  *
	  * If the INI file does not exist, default dark mode behavior is applied via
	  * @ref setDarkModeConfig.
	  *
	  * @param iniName Name of INI file (resolved via @ref GetIniPath).
	  *
	  * @note When `DarkModeType::classic` is set, system colors are used instead of themed ones.
	  */
	static void initOptions(const std::wstring& iniName)
	{
		if (iniName.empty())
		{
			return;
		}

		const std::wstring iniPath = GetIniPath(iniName);
		if (FileExists(iniPath))
		{
			DarkMode::initDarkModeConfig(::GetPrivateProfileIntW(L"main", L"mode", 1, iniPath.c_str()));
			if (g_dmCfg._dmType == DarkModeType::classic)
			{
				DarkMode::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
				DarkMode::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
				return;
			}

			const bool useDark = g_dmCfg._dmType == DarkModeType::dark;

			const std::wstring sectionBase = useDark ? L"dark" : L"light";
			const std::wstring sectionColorsView = sectionBase + L".colors.view";
			const std::wstring sectionColors = sectionBase + L".colors";

			DarkMode::setMicaConfig(::GetPrivateProfileIntW(sectionBase.c_str(), L"mica", 0, iniPath.c_str()));
			DarkMode::setRoundCornerConfig(::GetPrivateProfileIntW(sectionBase.c_str(), L"roundCorner", 0, iniPath.c_str()));
			SetClrFromIni(sectionBase, L"borderColor", iniPath, &g_dmCfg._borderColor);
			if (g_dmCfg._borderColor == kDwmwaClrDefaultRGBCheck)
			{
				g_dmCfg._borderColor = DWMWA_COLOR_DEFAULT;
			}

			if (useDark)
			{
				UINT tone = ::GetPrivateProfileIntW(sectionBase.c_str(), L"tone", 0, iniPath.c_str());
				if (tone >= static_cast<UINT>(ColorTone::max))
				{
					tone = 0;
				}

				DarkMode::getTheme().setToneColors(static_cast<ColorTone>(tone));
				DarkMode::getThemeView()._clrView = DarkMode::darkColorsView;
				DarkMode::getThemeView()._clrView.headerBackground = DarkMode::getTheme()._colors.background;
				DarkMode::getThemeView()._clrView.headerHotBackground = DarkMode::getTheme()._colors.hotBackground;
				DarkMode::getThemeView()._clrView.headerText = DarkMode::getTheme()._colors.darkerText;

				if (!DarkMode::isWindowsModeEnabled())
				{
					g_dmCfg._micaExtend = (::GetPrivateProfileIntW(sectionBase.c_str(), L"micaExtend", 0, iniPath.c_str()) == 1);
				}
			}
			else
			{
				DarkMode::getTheme()._colors = DarkMode::getLightColors();
				DarkMode::getThemeView()._clrView = DarkMode::lightColorsView;
			}

			SetClrFromIni(sectionColorsView, L"backgroundView", iniPath, &DarkMode::getThemeView()._clrView.background);
			SetClrFromIni(sectionColorsView, L"textView", iniPath, &DarkMode::getThemeView()._clrView.text);
			SetClrFromIni(sectionColorsView, L"gridlines", iniPath, &DarkMode::getThemeView()._clrView.gridlines);
			SetClrFromIni(sectionColorsView, L"backgroundHeader", iniPath, &DarkMode::getThemeView()._clrView.headerBackground);
			SetClrFromIni(sectionColorsView, L"backgroundHotHeader", iniPath, &DarkMode::getThemeView()._clrView.headerHotBackground);
			SetClrFromIni(sectionColorsView, L"textHeader", iniPath, &DarkMode::getThemeView()._clrView.headerText);
			SetClrFromIni(sectionColorsView, L"edgeHeader", iniPath, &DarkMode::getThemeView()._clrView.headerEdge);

			SetClrFromIni(sectionColors, L"background", iniPath, &DarkMode::getTheme()._colors.background);
			SetClrFromIni(sectionColors, L"backgroundCtrl", iniPath, &DarkMode::getTheme()._colors.ctrlBackground);
			SetClrFromIni(sectionColors, L"backgroundHot", iniPath, &DarkMode::getTheme()._colors.hotBackground);
			SetClrFromIni(sectionColors, L"backgroundDlg", iniPath, &DarkMode::getTheme()._colors.dlgBackground);
			SetClrFromIni(sectionColors, L"backgroundError", iniPath, &DarkMode::getTheme()._colors.errorBackground);

			SetClrFromIni(sectionColors, L"text", iniPath, &DarkMode::getTheme()._colors.text);
			SetClrFromIni(sectionColors, L"textItem", iniPath, &DarkMode::getTheme()._colors.darkerText);
			SetClrFromIni(sectionColors, L"textDisabled", iniPath, &DarkMode::getTheme()._colors.disabledText);
			SetClrFromIni(sectionColors, L"textLink", iniPath, &DarkMode::getTheme()._colors.linkText);

			SetClrFromIni(sectionColors, L"edge", iniPath, &DarkMode::getTheme()._colors.edge);
			SetClrFromIni(sectionColors, L"edgeHot", iniPath, &DarkMode::getTheme()._colors.hotEdge);
			SetClrFromIni(sectionColors, L"edgeDisabled", iniPath, &DarkMode::getTheme()._colors.disabledEdge);

			DarkMode::updateThemeBrushesAndPens();
			DarkMode::updateViewBrushesAndPens();
		}
		else
		{
			DarkMode::setDarkModeConfig();
			if (g_dmCfg._dmType == DarkModeType::classic)
			{
				DarkMode::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
				DarkMode::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
			}
		}
	}
#endif // !defined(_DARKMODELIB_NO_INI_CONFIG)

	static void initExperimentalDarkMode()
	{
		::InitDarkMode();
	}

	static void setDarkMode(bool useDark, bool fixDarkScrollbar)
	{
		::SetDarkMode(useDark, fixDarkScrollbar);
	}

	static bool allowDarkModeForWindow(HWND hWnd, bool allow)
	{
		return ::AllowDarkModeForWindow(hWnd, allow);
	}

#if defined(_DARKMODELIB_ALLOW_OLD_OS)
	static void setTitleBarThemeColor(HWND hWnd)
	{
		::RefreshTitleBarThemeColor(hWnd);
	}
#endif

	[[nodiscard]] static bool isColorSchemeChangeMessage(LPARAM lParam)
	{
		return ::IsColorSchemeChangeMessage(lParam);
	}

	static bool isHighContrast()
	{
		return ::IsHighContrast();
	}

	static bool isThemePrefered()
	{
		return (DarkMode::getLibInfo(LibInfo::preferTheme) == TRUE)
			&& DarkMode::isAtLeastWindows10()
			&& DarkMode::isExperimentalSupported();
	}

	/**
	 * @brief Applies dark mode settings based on the given configuration type.
	 *
	 * Initializes the dark mode type settings and system-following behavior.
	 * Enables or disables dark mode depending on whether `DarkModeType::dark` is selected.
	 * Sets colors according to mode type.
	 *
	 * @param dmType Dark mode configuration type; see @ref DarkMode::initDarkModeConfig for values.
	 *
	 * @see DarkMode::initDarkModeConfig()
	 */
	void setDarkModeConfig(UINT dmType)
	{
		DarkMode::initDarkModeConfig(dmType);

		const bool useDark = g_dmCfg._dmType == DarkModeType::dark;
		DarkMode::setDarkMode(useDark, true);

		if (useDark)
		{
			DarkMode::getTheme().setToneColors();
			DarkMode::getThemeView()._clrView = DarkMode::darkColorsView;
		}
		else if (g_dmCfg._dmType == DarkModeType::light)
		{
			DarkMode::getTheme()._colors = DarkMode::getLightColors();
			DarkMode::getThemeView()._clrView = DarkMode::lightColorsView;
		}
		else
		{
			DarkMode::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
			DarkMode::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
		}

		if (g_dmCfg._dmType != DarkModeType::classic)
		{
			DarkMode::updateThemeBrushesAndPens();
			DarkMode::updateViewBrushesAndPens();
		}

		DarkMode::calculateTreeViewStyle();
	}

	/**
	 * @brief Applies dark mode settings based on system mode preference.
	 *
	 * Determines the appropriate mode using @ref DarkMode::isDarkModeReg and forwards
	 * the result to @ref DarkMode::setDarkModeConfig.
	 *
	 * Uses:
	 * - `DarkModeType::dark` if registry prefers dark mode.
	 * - `DarkModeType::classic` otherwise.
	 */
	void setDarkModeConfig()
	{
		const auto dmType = static_cast<UINT>(DarkMode::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic);
		DarkMode::setDarkModeConfig(dmType);
	}

	/**
	 * @brief Initializes dark mode experimental features, colors, and other settings.
	 *
	 * Performs one-time setup for dark mode, including:
	 * - Initializing experimental features if not yet done.
	 * - Optionally loading settings from an INI file (if INI config is enabled).
	 * - Initializing TreeView style and applying dark mode settings.
	 * - Preparing system colors (e.g. `COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_BTNFACE`)
	 *   for hooking.
	 *
	 * @param iniName Optional path to an INI file for dark mode settings (ignored if already set).
	 *
	 * @note This function is only run once per session;
	 *       subsequent calls have no effect, unless follow system mode is used,
	 *       then only colors are updated each time system changes mode.
	 *
	 * @see DarkMode::calculateTreeViewStyle()
	 */
	void initDarkMode([[maybe_unused]] const wchar_t* iniName)
	{
		if (!g_dmCfg._isInit)
		{
			if (!g_dmCfg._isInitExperimental)
			{
				DarkMode::initExperimentalDarkMode();
				g_dmCfg._isInitExperimental = true;
			}

			const bool useDark = g_dmCfg._dmType == DarkModeType::dark;
			if (useDark)
			{
				DarkMode::getTheme()._colors = DarkMode::darkColors;
				DarkMode::getThemeView()._clrView = DarkMode::darkColorsView;
			}
			else
			{
				DarkMode::getTheme()._colors = DarkMode::getLightColors();
				DarkMode::getThemeView()._clrView = DarkMode::lightColorsView;
			}

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
			if (!g_dmCfg._isIniNameSet)
			{
				g_dmCfg._iniName = iniName;
				g_dmCfg._isIniNameSet = true;
			}
			DarkMode::initOptions(g_dmCfg._iniName);
			DarkMode::setDarkMode(g_dmCfg._dmType == DarkModeType::dark, true);
			DarkMode::calculateTreeViewStyle();
#else
			DarkMode::setDarkModeConfig();
#endif

			DarkMode::setSysColor(COLOR_WINDOW, DarkMode::getBackgroundColor());
			DarkMode::setSysColor(COLOR_WINDOWTEXT, DarkMode::getTextColor());
			DarkMode::setSysColor(COLOR_BTNFACE, DarkMode::getViewGridlinesColor());

			g_dmCfg._isInit = true;
		}
	}

	/**
	 * @brief Initializes dark mode without INI settings.
	 *
	 * Forwards to @ref DarkMode::initDarkMode with an empty INI path, effectively disabling INI settings.
	 */
	void initDarkMode()
	{
		DarkMode::initDarkMode(L"");
	}

	/**
	 * @brief Checks if non-classic mode is enabled.
	 *
	 * If `_DARKMODELIB_ALLOW_OLD_OS` is defined, this skips Windows version checks.
	 * Otherwise, dark mode is only enabled on Windows 10 or newer.
	 *
	 * @return `true` if a supported dark mode type is active, otherwise `false`.
	 */
	bool isEnabled()
	{
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
		return g_dmCfg._dmType != DarkModeType::classic;
#else
		return DarkMode::isAtLeastWindows10() && g_dmCfg._dmType != DarkModeType::classic;
#endif
	}

	/**
	 * @brief Checks if experimental dark mode features are currently active.
	 *
	 * @return `true` if experimental dark mode is enabled.
	 */
	bool isExperimentalActive()
	{
		return g_darkModeEnabled;
	}

	/**
	 * @brief Checks if experimental dark mode features are supported by the system.
	 *
	 * @return `true` if dark mode experimental APIs are available.
	 */
	bool isExperimentalSupported()
	{
		return g_darkModeSupported;
	}

	/**
	 * @brief Checks if follow the system mode behavior is enabled.
	 *
	 * @return `true` if "mode" is not `WinMode::disabled`, i.e. system mode is followed.
	 */
	bool isWindowsModeEnabled()
	{
		return g_dmCfg._windowsMode != WinMode::disabled;
	}

	/**
	 * @brief Checks if the host OS is at least Windows 10.
	 *
	 * @return `true` if running on Windows 10 or newer.
	 */
	bool isAtLeastWindows10()
	{
		return ::IsWindows10();
	}
	/**
	 * @brief Checks if the host OS is at least Windows 11.
	 *
	 * @return `true` if running on Windows 11 or newer.
	 */
	bool isAtLeastWindows11()
	{
		return ::IsWindows11();
	}

	/**
	 * @brief Retrieves the current Windows build number.
	 *
	 * @return Windows build number reported by the system.
	 */
	DWORD getWindowsBuildNumber()
	{
		return GetWindowsBuildNumber();
	}

	/**
	 * @brief Handles system setting changes related to dark mode.
	 *
	 * Responds to system messages indicating a color scheme change. If the current
	 * dark mode state no longer matches the system registry preference, dark mode is
	 * re-initialized.
	 *
	 * - Skips processing if experimental dark mode is unsupported.
	 * - Relies on @ref DarkMode::isDarkModeReg for theme preference and skips during high contrast.
	 *
	 * @param lParam Message parameter (typically from `WM_SETTINGCHANGE`).
	 * @return `true` if a dark mode change was handled; otherwise `false`.
	 *
	 * @see DarkMode::isDarkModeReg()
	 * @see DarkMode::initDarkMode()
	 */
	bool handleSettingChange(LPARAM lParam)
	{
		if (DarkMode::isExperimentalSupported() && DarkMode::isColorSchemeChangeMessage(lParam))
		{
			// ShouldAppsUseDarkMode() is not reliable from 1903+, use DarkMode::isDarkModeReg() instead
			const bool isDarkModeUsed = DarkMode::isDarkModeReg() && !DarkMode::isHighContrast();
			if (DarkMode::isExperimentalActive() != isDarkModeUsed)
			{
				if (g_dmCfg._isInit)
				{
					g_dmCfg._isInit = false;
					DarkMode::initDarkMode();
				}
			}
			return true;
		}
		return false;
	}

	/**
	 * @brief Checks if dark mode is enabled in the Windows registry.
	 *
	 * Queries `HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\AppsUseLightTheme`.
	 *
	 * @return `true` if dark mode is preferred (value is `0`); otherwise `false`.
	 */
	bool isDarkModeReg()
	{
		DWORD data{};
		DWORD dwBufSize = sizeof(data);
		LPCWSTR lpSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
		LPCWSTR lpValue = L"AppsUseLightTheme";

		const auto result = ::RegGetValueW(HKEY_CURRENT_USER, lpSubKey, lpValue, RRF_RT_REG_DWORD, nullptr, &data, &dwBufSize);
		if (result != ERROR_SUCCESS)
		{
			return false;
		}

		// dark mode is 0, light mode is 1
		return data == 0UL;
	}

	// from DarkMode.h

	/**
	 * @brief Overrides a specific system color with a custom color.
	 *
	 * Currently supports:
	 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
	 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
	 * - `COLOR_BTNFACE`: Gridline color in ListView (when applicable).
	 *
	 * @param nIndex One of the supported system color indices.
	 * @param color Custom `COLORREF` value to apply.
	 */
	void setSysColor(int nIndex, COLORREF color)
	{
		::SetMySysColor(nIndex, color);
	}

	/**
	 * @brief Hooks system color to support runtime customization.
	 *
	 * @return `true` if the hook was installed successfully.
	 */
	static bool hookSysColor()
	{
		return ::HookSysColor();
	}

	/**
	 * @brief Unhooks system color overrides and restores default color behavior.
	 *
	 * This function is safe to call even if no color hook is currently installed.
	 * It ensures that system colors return to normal without requiring
	 * prior state checks.
	 */
	static void unhookSysColor()
	{
		::UnhookSysColor();
	}

	/**
	 * @brief Makes scroll bars on the specified window and all its children consistent.
	 *
	 * Currently not widely used by default.
	 *
	 * @param hWnd Handle to the parent window.
	 */
	void enableDarkScrollBarForWindowAndChildren(HWND hWnd)
	{
		::EnableDarkScrollBarForWindowAndChildren(hWnd);
	}

	/**
	 * @brief Paints a rounded rectangle using the specified pen and brush.
	 *
	 * Draws a rounded rectangle defined by `rect`, using the provided pen (`hpen`) and brush (`hBrush`)
	 * for the edge and fill, respectively. Preserves previous GDI object selections.
	 *
	 * @param hdc Handle to the device context.
	 * @param rect Rectangle bounds for the shape.
	 * @param hpen Pen used to draw the edge.
	 * @param hBrush Brush used to inner fill.
	 * @param width Horizontal corner radius.
	 * @param height Vertical corner radius.
	 */
	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width, int height)
	{
		auto holdBrush = ::SelectObject(hdc, hBrush);
		auto holdPen = ::SelectObject(hdc, hpen);
		::RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, width, height);
		::SelectObject(hdc, holdBrush);
		::SelectObject(hdc, holdPen);
	}

	/**
	 * @brief Paints an unfilled rounded rectangle (frame only).
	 *
	 * Uses a `NULL_BRUSH` to omit the inner fill, drawing only the rounded frame.
	 *
	 * @param hdc Handle to the device context.
	 * @param rect Rectangle bounds for the frame.
	 * @param hpen Pen used to draw the edge.
	 * @param width Horizontal corner radius.
	 * @param height Vertical corner radius.
	 */
	void paintRoundFrameRect(HDC hdc, const RECT& rect, HPEN hpen, int width, int height)
	{
		DarkMode::paintRoundRect(hdc, rect, hpen, static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), width, height);
	}

	class ThemeData
	{
	public:
		ThemeData() = delete;

		explicit ThemeData(const wchar_t* themeClass)
			: _themeClass(themeClass)
		{}

		ThemeData(const ThemeData&) = delete;
		ThemeData& operator=(const ThemeData&) = delete;

		ThemeData(ThemeData&&) = delete;
		ThemeData& operator=(ThemeData&&) = delete;

		~ThemeData()
		{
			closeTheme();
		}

		bool ensureTheme(HWND hWnd)
		{
			if (_hTheme == nullptr && _themeClass != nullptr)
			{
				_hTheme = ::OpenThemeData(hWnd, _themeClass);
			}
			return _hTheme != nullptr;
		}

		void closeTheme() noexcept
		{
			if (_hTheme != nullptr)
			{
				::CloseThemeData(_hTheme);
				_hTheme = nullptr;
			}
		}

		[[nodiscard]] const HTHEME& getHTheme() const
		{
			return _hTheme;
		}

	private:
		const wchar_t* _themeClass = nullptr;
		HTHEME _hTheme = nullptr;
	};

	class BufferData
	{
	public:
		BufferData() = default;

		BufferData(const BufferData&) = delete;
		BufferData& operator=(const BufferData&) = delete;

		BufferData(BufferData&&) = delete;
		BufferData& operator=(BufferData&&) = delete;

		~BufferData()
		{
			releaseBuffer();
		}

		bool ensureBuffer(HDC hdc, const RECT& rcClient)
		{
			const int width = rcClient.right - rcClient.left;
			const int height = rcClient.bottom - rcClient.top;

			if (_szBuffer.cx != width || _szBuffer.cy != height)
			{
				releaseBuffer();
				_hMemDC = ::CreateCompatibleDC(hdc);
				_hMemBmp = ::CreateCompatibleBitmap(hdc, width, height);
				_holdBmp = static_cast<HBITMAP>(::SelectObject(_hMemDC, _hMemBmp));
				_szBuffer = { width, height };
			}

			return _hMemDC != nullptr && _hMemBmp != nullptr;
		}

		void releaseBuffer() noexcept
		{
			if (_hMemDC != nullptr)
			{
				::SelectObject(_hMemDC, _holdBmp);
				::DeleteObject(_hMemBmp);
				::DeleteDC(_hMemDC);

				_hMemDC = nullptr;
				_hMemBmp = nullptr;
				_holdBmp = nullptr;
				_szBuffer = { 0, 0 };
			}
		}

		[[nodiscard]] const HDC& getHMemDC() const
		{
			return _hMemDC;
		}

	private:
		HDC _hMemDC = nullptr;
		HBITMAP _hMemBmp = nullptr;
		HBITMAP _holdBmp = nullptr;
		SIZE _szBuffer{};
	};

	class FontData
	{
	public:
		FontData() = default;

		explicit FontData(HFONT hFont) noexcept
			: _hFont(hFont)
		{}

		FontData(const FontData&) = delete;
		FontData& operator=(const FontData&) = delete;

		FontData(FontData&&) = delete;
		FontData& operator=(FontData&&) = delete;

		~FontData()
		{
			FontData::destroyFont();
		}

		void setFont(HFONT newFont) noexcept
		{
			FontData::destroyFont();
			_hFont = newFont;
		}

		[[nodiscard]] const HFONT& getFont() const noexcept
		{
			return _hFont;
		}

		[[nodiscard]] bool hasFont() const noexcept
		{
			return _hFont != nullptr;
		}

		void destroyFont() noexcept
		{
			if (FontData::hasFont())
			{
				::DeleteObject(_hFont);
				_hFont = nullptr;
			}
		}

	private:
		HFONT _hFont = nullptr;
	};

	template <typename T, typename Param>
	static auto setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID, const Param& param) -> int
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			auto pData = std::make_unique<T>(param);
			if (::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	template <typename T>
	static auto setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID) -> int
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			auto pData = std::make_unique<T>();
			if (::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	static int setSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID)
	{
		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			return ::SetWindowSubclass(hWnd, subclassProc, subclassID, 0);
		}
		return -1;
	}

	template <typename T = void>
	static auto removeSubclass(HWND hWnd, SUBCLASSPROC subclassProc, UINT_PTR subclassID) -> int
	{
		T* pData = nullptr;

		if (::GetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR*>(&pData)) == TRUE)
		{
			if constexpr (!std::is_void_v<T>)
			{
				if (pData != nullptr)
				{
					delete pData;
					pData = nullptr;
				}
			}
			return ::RemoveWindowSubclass(hWnd, subclassProc, subclassID);
		}
		return -1;
	}

	struct ButtonData
	{
		ThemeData _themeData{ VSCLASS_BUTTON };
		SIZE _szBtn{};

		int _iStateID = 0;
		bool _isSizeSet = false;

		ButtonData() = default;

		// Saves width and height from the resource file for use as restrictions.
		explicit ButtonData(HWND hWnd)
		{
			const auto nBtnStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			switch (nBtnStyle & BS_TYPEMASK)
			{
				case BS_CHECKBOX:
				case BS_AUTOCHECKBOX:
				case BS_3STATE:
				case BS_AUTO3STATE:
				case BS_RADIOBUTTON:
				case BS_AUTORADIOBUTTON:
				{
					if ((nBtnStyle & BS_MULTILINE) != BS_MULTILINE)
					{
						RECT rcBtn{};
						::GetClientRect(hWnd, &rcBtn);
						_szBtn.cx = rcBtn.right - rcBtn.left;
						_szBtn.cy = rcBtn.bottom - rcBtn.top;
						_isSizeSet = (_szBtn.cx != 0 && _szBtn.cy != 0);
					}
					break;
				}

				default:
				{
					break;
				}
			}
		}
	};

	/**
	 * @brief Draws a themed checkbox or radio button (excluding push-like buttons).
	 *
	 * Internally used by @ref DarkMode::paintButton to draw visual elements such as checkbox glyphs
	 * or radio indicators alongside styled text. Not used for buttons with `BS_PUSHLIKE`,
	 * which require different handling and theming logic.
	 *
	 * - Retrieves themed or fallback font for consistent appearance.
	 * - Handles alignment, word wrapping, and prefix visibility per style flags.
	 * - Draws themed background and glyph using `DrawThemeBackground`.
	 * - Uses dark mode-aware text rendering and applies focus cue when needed.
	 *
	 * @param hWnd Handle to the button control.
	 * @param hdc Device context for drawing.
	 * @param hTheme Active visual style theme handle.
	 * @param iPartID Part ID (`BP_CHECKBOX`, `BP_RADIOBUTTON`, etc.).
	 * @param iStateID State ID (`CBS_CHECKEDHOT`, `RBS_UNCHECKEDNORMAL`, etc.).
	 */
	static void renderButton(HWND hWnd, HDC hdc, HTHEME hTheme, int iPartID, int iStateID)
	{
		HFONT hFont = nullptr;
		bool isFontCreated = false;
		LOGFONT lf{};
		if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
		{
			hFont = ::CreateFontIndirect(&lf);
			isFontCreated = true;
		}

		if (hFont == nullptr)
		{
			hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		}

		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isMultiline = (nStyle & BS_MULTILINE) == BS_MULTILINE;
		const bool isTop = (nStyle & BS_TOP) == BS_TOP;
		const bool isBottom = (nStyle & BS_BOTTOM) == BS_BOTTOM;
		const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;
		const bool isRight = (nStyle & BS_RIGHT) == BS_RIGHT;
		const bool isVCenter = (nStyle & BS_VCENTER) == BS_VCENTER;

		DWORD dtFlags = DT_LEFT; // DT_LEFT is 0
		if (isMultiline)
		{
			dtFlags |= DT_WORDBREAK;
		}
		else
		{
			dtFlags |= DT_SINGLELINE;
		}

		if (isCenter)
		{
			dtFlags |= DT_CENTER;
		}
		else if (isRight)
		{
			dtFlags |= DT_RIGHT;
		}

		if (isVCenter || (!isMultiline && !isBottom && !isTop))
		{
			dtFlags |= DT_VCENTER;
		}
		else if (isBottom)
		{
			dtFlags |= DT_BOTTOM;
		}

		const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));
		const bool hidePrefix = (uiState & UISF_HIDEACCEL) == UISF_HIDEACCEL;
		if (hidePrefix)
		{
			dtFlags |= DT_HIDEPREFIX;
		}


		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		std::wstring buffer;
		const auto bufferLen = static_cast<size_t>(::GetWindowTextLength(hWnd));
		buffer.resize(bufferLen + 1, L'\0');
		::GetWindowText(hWnd, buffer.data(), static_cast<int>(buffer.length()));

		SIZE szBox{};
		::GetThemePartSize(hTheme, hdc, iPartID, iStateID, nullptr, TS_DRAW, &szBox);

		RECT rcText{};
		::GetThemeBackgroundContentRect(hTheme, hdc, iPartID, iStateID, &rcClient, &rcText);

		RECT rcBackground{ rcClient };
		if (!isMultiline)
		{
			rcBackground.top += (rcText.bottom - rcText.top - szBox.cy) / 2;
		}
		rcBackground.bottom = rcBackground.top + szBox.cy;
		rcBackground.right = rcBackground.left + szBox.cx;
		rcText.left = rcBackground.right + 3;

		::DrawThemeParentBackground(hWnd, hdc, &rcClient);
		::DrawThemeBackground(hTheme, hdc, iPartID, iStateID, &rcBackground, nullptr);

		DTTOPTS dtto{};
		dtto.dwSize = sizeof(DTTOPTS);
		dtto.dwFlags = DTT_TEXTCOLOR;
		dtto.crText = (::IsWindowEnabled(hWnd) == FALSE) ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

		::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags, &rcText, &dtto);

		const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
		if (((nState & BST_FOCUS) == BST_FOCUS) && ((uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS))
		{
			dtto.dwFlags |= DTT_CALCRECT;
			::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags | DT_CALCRECT, &rcText, &dtto);
			const RECT rcFocus{ rcText.left - 1, rcText.top, rcText.right + 1, rcText.bottom + 1 };
			::DrawFocusRect(hdc, &rcFocus);
		}

		::SelectObject(hdc, holdFont);
		if (isFontCreated)
		{
			::DeleteObject(hFont);
		}
	}

	/**
	 * @brief Paints a themed checkbox or radio button with state-based visuals.
	 *
	 * Determines the appropriate themed part and state ID based on the control’s
	 * style (e.g. `BS_CHECKBOX`, `BS_RADIOBUTTON`) and current button state flags
	 * such as `BST_CHECKED`, `BST_PUSHED`, or `BST_HOT`.
	 *
	 * - Uses buffered animation (if available) to smoothly transition between states.
	 * - Falls back to direct drawing via @ref renderButton if animation is not used.
	 * - Internally updates the `buttonData._iStateID` to preserve the last rendered state.
	 * - Not used for `BS_PUSHLIKE` buttons.
	 *
	 * @param hWnd Handle to the checkbox or radio button control.
	 * @param hdc Device context used for rendering.
	 * @param buttonData Theming and state info, including current theme and last state.
	 *
	 * @see DarkMode::renderButton()
	 */
	static void paintButton(HWND hWnd, HDC hdc, ButtonData& buttonData)
	{
		const auto& hTheme = buttonData._themeData.getHTheme();

		const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const auto nBtnStyle = nStyle & BS_TYPEMASK;

		int iPartID = 0;
		int iStateID = 0;

		switch (nBtnStyle)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
			{
				iPartID = BP_CHECKBOX;

				if (::IsWindowEnabled(hWnd) == FALSE)           { iStateID = CBS_UNCHECKEDDISABLED; }
				else if ((nState & BST_PUSHED) == BST_PUSHED)   { iStateID = CBS_UNCHECKEDPRESSED; }
				else if ((nState & BST_HOT) == BST_HOT)         { iStateID = CBS_UNCHECKEDHOT; }
				else                                            { iStateID = CBS_UNCHECKEDNORMAL; }

				static constexpr int checkedOffset = 4;
				static constexpr int mixedOffset = 8;
				if ((nState & BST_CHECKED) == BST_CHECKED)      { iStateID += checkedOffset; }
				else if ((nState & BST_INDETERMINATE) == BST_INDETERMINATE) { iStateID += mixedOffset; }

				break;
			}

			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			{
				iPartID = BP_RADIOBUTTON;

				if (::IsWindowEnabled(hWnd) == FALSE)           { iStateID = RBS_UNCHECKEDDISABLED; }
				else if ((nState & BST_PUSHED) == BST_PUSHED)   { iStateID = RBS_UNCHECKEDPRESSED; }
				else if ((nState & BST_HOT) == BST_HOT)         { iStateID = RBS_UNCHECKEDHOT; }
				else                                            { iStateID = RBS_UNCHECKEDNORMAL; }

				if ((nState & BST_CHECKED) == BST_CHECKED)      { iStateID += 4; }

				break;
			}

			default: // should never happen
			{
				iPartID = BP_CHECKBOX;
				iStateID = CBS_UNCHECKEDDISABLED;
				break;
			}
		}

		if (::BufferedPaintRenderAnimation(hWnd, hdc) == TRUE)
		{
			return;
		}

		BP_ANIMATIONPARAMS animParams{};
		animParams.cbSize = sizeof(BP_ANIMATIONPARAMS);
		animParams.style = BPAS_LINEAR;
		if (iStateID != buttonData._iStateID)
		{
			::GetThemeTransitionDuration(hTheme, iPartID, buttonData._iStateID, iStateID, TMT_TRANSITIONDURATIONS, &animParams.dwDuration);
		}

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		HDC hdcFrom = nullptr;
		HDC hdcTo = nullptr;
		HANIMATIONBUFFER hbpAnimation = ::BeginBufferedAnimation(hWnd, hdc, &rcClient, BPBF_COMPATIBLEBITMAP, nullptr, &animParams, &hdcFrom, &hdcTo);
		if (hbpAnimation != nullptr)
		{
			if (hdcFrom != nullptr)
			{
				DarkMode::renderButton(hWnd, hdcFrom, hTheme, iPartID, buttonData._iStateID);
			}
			if (hdcTo != nullptr)
			{
				DarkMode::renderButton(hWnd, hdcTo, hTheme, iPartID, iStateID);
			}

			buttonData._iStateID = iStateID;

			::EndBufferedAnimation(hbpAnimation, TRUE);
		}
		else
		{
			DarkMode::renderButton(hWnd, hdc, hTheme, iPartID, iStateID);

			buttonData._iStateID = iStateID;
		}
	}

	/**
	 * @brief Window subclass procedure for themed checkbox and radio buttons.
	 */
	static LRESULT CALLBACK ButtonSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
		auto& themeData = pButtonData->_themeData;

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ButtonSubclass, uIdSubclass);
				delete pButtonData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}
				return TRUE;
			}

			case WM_PRINTCLIENT:
			case WM_PAINT:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				PAINTSTRUCT ps{};
				auto hdc = reinterpret_cast<HDC>(wParam);
				if (hdc == nullptr)
				{
					hdc = ::BeginPaint(hWnd, &ps);
				}

				DarkMode::paintButton(hWnd, hdc, *pButtonData);

				if (ps.hdc != nullptr)
				{
					::EndPaint(hWnd, &ps);
				}

				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_SIZE:
			case WM_DESTROY:
			{
				::BufferedPaintStopAllAnimations(hWnd);
				break;
			}

			case WM_ENABLE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				// skip the button's normal wndproc so it won't redraw out of wm_paint
				const LRESULT retVal = ::DefWindowProc(hWnd, uMsg, wParam, lParam);
				::InvalidateRect(hWnd, nullptr, FALSE);
				return retVal;
			}

			case WM_UPDATEUISTATE:
			{
				if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
				{
					::InvalidateRect(hWnd, nullptr, FALSE);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ButtonData>(hWnd, ButtonSubclass, kButtonSubclassID);
	}

	void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ButtonData>(hWnd, ButtonSubclass, kButtonSubclassID);
	}

	static void paintGroupbox(HWND hWnd, HDC hdc, const ButtonData& buttonData)
	{
		const auto& hTheme = buttonData._themeData.getHTheme();

		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		static constexpr int iPartID = BP_GROUPBOX;
		const int iStateID = isDisabled ? GBS_DISABLED : GBS_NORMAL;

		bool isFontCreated = false;
		HFONT hFont = nullptr;
		LOGFONT lf{};
		if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
		{
			hFont = ::CreateFontIndirect(&lf);
			isFontCreated = true;
		}

		if (hFont == nullptr)
		{
			hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
			isFontCreated = false;
		}

		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		std::wstring buffer;
		const auto bufferLen = static_cast<size_t>(::GetWindowTextLength(hWnd));
		if (bufferLen > 0)
		{
			buffer.resize(bufferLen + 1, L'\0');
			::GetWindowText(hWnd, buffer.data(), static_cast<int>(buffer.length()));
		}

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		rcClient.bottom -= 1;

		RECT rcText{ rcClient };
		RECT rcBackground{ rcClient };
		if (!buffer.empty())
		{
			SIZE szText{};
			::GetTextExtentPoint32(hdc, buffer.c_str(), static_cast<int>(bufferLen), &szText);

			const int centerPosX = isCenter ? ((rcClient.right - rcClient.left - szText.cx) / 2) : 7;

			rcBackground.top += szText.cy / 2;
			rcText.left += centerPosX;
			rcText.bottom = rcText.top + szText.cy;
			rcText.right = rcText.left + szText.cx + 4;

			::ExcludeClipRect(hdc, rcText.left, rcText.top, rcText.right, rcText.bottom);
		}
		else
		{
			SIZE szText{};
			::GetTextExtentPoint32(hdc, L"M", 1, &szText);
			rcBackground.top += szText.cy / 2;
		}

		RECT rcContent = rcBackground;
		::GetThemeBackgroundContentRect(hTheme, hdc, BP_GROUPBOX, iStateID, &rcBackground, &rcContent);
		::ExcludeClipRect(hdc, rcContent.left, rcContent.top, rcContent.right, rcContent.bottom);

		DarkMode::paintRoundFrameRect(hdc, rcBackground, DarkMode::getEdgePen());

		::SelectClipRgn(hdc, nullptr);

		if (!buffer.empty())
		{
			::InflateRect(&rcText, -2, 0);

			DTTOPTS dtto{};
			dtto.dwSize = sizeof(DTTOPTS);
			dtto.dwFlags = DTT_TEXTCOLOR;
			dtto.crText = isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

			DWORD dtFlags = isCenter ? DT_CENTER : DT_LEFT;

			if (::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0) != 0) // NULL
			{
				dtFlags |= DT_HIDEPREFIX;
			}

			::DrawThemeTextEx(hTheme, hdc, BP_GROUPBOX, iStateID, buffer.c_str(), -1, dtFlags | DT_SINGLELINE, &rcText, &dtto);
		}

		::SelectObject(hdc, holdFont);
		if (isFontCreated)
		{
			::DeleteObject(hFont);
		}
	}

	static LRESULT CALLBACK GroupboxSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
		auto& themeData = pButtonData->_themeData;

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, GroupboxSubclass, uIdSubclass);
				delete pButtonData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}
				return TRUE;
			}

			case WM_PRINTCLIENT:
			case WM_PAINT:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				PAINTSTRUCT ps{};
				auto hdc = reinterpret_cast<HDC>(wParam);
				if (hdc == nullptr)
				{
					hdc = ::BeginPaint(hWnd, &ps);
				}

				DarkMode::paintGroupbox(hWnd, hdc, *pButtonData);

				if (ps.hdc != nullptr)
				{
					::EndPaint(hWnd, &ps);
				}

				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_ENABLE:
			{
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setGroupboxCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ButtonData>(hWnd, GroupboxSubclass, kGroupboxSubclassID);
	}

	void removeGroupboxCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ButtonData>(hWnd, GroupboxSubclass, kGroupboxSubclassID);
	}

	static void setBtnCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		const auto nBtnStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		switch (nBtnStyle & BS_TYPEMASK)
		{
			case BS_CHECKBOX:
			case BS_AUTOCHECKBOX:
			case BS_3STATE:
			case BS_AUTO3STATE:
			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			{
				if ((nBtnStyle & BS_PUSHLIKE) == BS_PUSHLIKE)
				{
					if (p._theme)
					{
						::SetWindowTheme(hWnd, p._themeClassName, nullptr);
					}
					break;
				}

				if (DarkMode::isAtLeastWindows11() && p._theme)
				{
					::SetWindowTheme(hWnd, p._themeClassName, nullptr);
				}

				if (p._subclass)
				{
					DarkMode::setCheckboxOrRadioBtnCtrlSubclass(hWnd);
				}
				break;
			}

			case BS_GROUPBOX:
			{
				if (p._subclass)
				{
					DarkMode::setGroupboxCtrlSubclass(hWnd);
				}
				break;
			}

			case BS_PUSHBUTTON:
			case BS_DEFPUSHBUTTON:
			case BS_SPLITBUTTON:
			case BS_DEFSPLITBUTTON:
			{
				if (p._theme)
				{
					::SetWindowTheme(hWnd, p._themeClassName, nullptr);
				}
				break;
			}

			default:
			{
				break;
			}
		}
	}

	struct UpDownData
	{
		BufferData _bufferData;

		RECT _rcClient{};
		RECT _rcPrev{};
		RECT _rcNext{};
		int _cornerRoundness = 0;
		bool _isHorizontal = false;
		bool _wasHotNext = false;

		UpDownData() = delete;

		explicit UpDownData(HWND hWnd)
			: _cornerRoundness((DarkMode::isAtLeastWindows11() && CmpWndClassName(::GetParent(hWnd), WC_TABCONTROL)) ? (kWin11CornerRoundness + 1) : 0)
			, _isHorizontal((::GetWindowLongPtr(hWnd, GWL_STYLE) & UDS_HORZ) == UDS_HORZ)
		{
			updateRect(hWnd);
		}

		void updateRectUpDown()
		{
			if (_isHorizontal)
			{
				const RECT rcArrowLeft{
					_rcClient.left, _rcClient.top,
					_rcClient.right - ((_rcClient.right - _rcClient.left) / 2) - 1, _rcClient.bottom
				};

				const RECT rcArrowRight{
					rcArrowLeft.right + 1, _rcClient.top,
					_rcClient.right, _rcClient.bottom
				};

				_rcPrev = rcArrowLeft;
				_rcNext = rcArrowRight;
			}
			else
			{
				static constexpr LONG offset = 2;

				const RECT rcArrowTop{
					_rcClient.left + offset, _rcClient.top,
					_rcClient.right, _rcClient.bottom - ((_rcClient.bottom - _rcClient.top) / 2)
				};

				const RECT rcArrowBottom{
					_rcClient.left + offset, rcArrowTop.bottom,
					_rcClient.right, _rcClient.bottom
				};

				_rcPrev = rcArrowTop;
				_rcNext = rcArrowBottom;
			}
		}

		void updateRect(HWND hWnd)
		{
			::GetClientRect(hWnd, &_rcClient);
			updateRectUpDown();
		}

		bool updateRect(RECT rcClientNew)
		{
			if (::EqualRect(&_rcClient, &rcClientNew) == FALSE)
			{
				_rcClient = rcClientNew;
				updateRectUpDown();
				return true;
			}
			return false;
		}
	};

	static void paintUpDown(HWND hWnd, HDC hdc, UpDownData& upDownData)
	{
		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		const int roundness = upDownData._cornerRoundness;

		::FillRect(hdc, &upDownData._rcClient, DarkMode::getDlgBackgroundBrush());
		::SetBkMode(hdc, TRANSPARENT);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isHotPrev = ::PtInRect(&upDownData._rcPrev, ptCursor) == TRUE;
		const bool isHotNext = ::PtInRect(&upDownData._rcNext, ptCursor) == TRUE;

		upDownData._wasHotNext = !isHotPrev && (::PtInRect(&upDownData._rcClient, ptCursor) == TRUE);

		auto paintUpDownBtn = [&](const RECT& rect, bool isHot) -> void {
			HBRUSH hBrush = nullptr;
			HPEN hPen = nullptr;
			if (isDisabled)
			{
				hBrush = DarkMode::getDlgBackgroundBrush();
				hPen = DarkMode::getDisabledEdgePen();
			}
			else if (isHot)
			{
				hBrush = DarkMode::getHotBackgroundBrush();
				hPen = DarkMode::getHotEdgePen();
			}
			else
			{
				hBrush = DarkMode::getCtrlBackgroundBrush();
				hPen = DarkMode::getEdgePen();
			}

			DarkMode::paintRoundRect(hdc, rect, hPen, hBrush, roundness, roundness);
		};

		paintUpDownBtn(upDownData._rcPrev, isHotPrev);
		paintUpDownBtn(upDownData._rcNext, isHotNext);

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
		const COLORREF clrText = isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getDarkerTextColor();

		const LONG offset = upDownData._isHorizontal ? 1 : 0;
		RECT rcTectPrev{ upDownData._rcPrev.left, upDownData._rcPrev.top, upDownData._rcPrev.right, upDownData._rcPrev.bottom - offset };
		::SetTextColor(hdc, isHotPrev ? DarkMode::getTextColor() : clrText);
		::DrawText(hdc, upDownData._isHorizontal ? L"<" : L"˄", -1, &rcTectPrev, dtFlags);

		RECT rcTectNext{ upDownData._rcNext.left + offset, upDownData._rcNext.top, upDownData._rcNext.right, upDownData._rcNext.bottom - offset };
		::SetTextColor(hdc, isHotNext ? DarkMode::getTextColor() : clrText);
		::DrawText(hdc, upDownData._isHorizontal ? L">" : L"˅", -1, &rcTectNext, dtFlags);

		::SelectObject(hdc, holdFont);
	}

	static LRESULT CALLBACK UpDownSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pUpDownData = reinterpret_cast<UpDownData*>(dwRefData);
		auto& bufferData = pUpDownData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, UpDownSubclass, uIdSubclass);
				delete pUpDownData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				if (!pUpDownData->_isHorizontal)
				{
					::OffsetRect(&ps.rcPaint, 2, 0);
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				pUpDownData->updateRect(rcClient);
				if (!pUpDownData->_isHorizontal)
				{
					::OffsetRect(&rcClient, 2, 0);
				}

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					DarkMode::paintUpDown(hWnd, hMemDC, *pUpDownData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				pUpDownData->updateRect(hWnd);
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				if (pUpDownData->_wasHotNext)
				{
					pUpDownData->_wasHotNext = false;
					::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				}

				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				pUpDownData->_wasHotNext = false;
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);

				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setUpDownCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<UpDownData>(hWnd, UpDownSubclass, kUpDownSubclassID, hWnd);
		DarkMode::setDarkExplorerTheme(hWnd);
	}

	void removeUpDownCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<UpDownData>(hWnd, UpDownSubclass, kUpDownSubclassID);
	}

	static void setUpDownCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			DarkMode::setUpDownCtrlSubclass(hWnd);
		}
		else if (p._theme)
		{
			::SetWindowTheme(hWnd, p._themeClassName, nullptr);
		}
	}

	static void paintTab(HWND hWnd, HDC hdc, const RECT& rect)
	{
		::FillRect(hdc, &rect, DarkMode::getDlgBackgroundBrush());

		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, DarkMode::getEdgePen()));

		auto holdClip = ::CreateRectRgn(0, 0, 0, 0);
		if (::GetClipRgn(hdc, holdClip) != 1)
		{
			::DeleteObject(holdClip);
			holdClip = nullptr;
		}

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = ::SelectObject(hdc, hFont);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		bool hasFocusRect = false;
		if (::GetFocus() == hWnd)
		{
			const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));
			hasFocusRect = ((uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS);
		}

		const int iSelTab = TabCtrl_GetCurSel(hWnd);
		const int nTabs = TabCtrl_GetItemCount(hWnd);
		for (int i = 0; i < nTabs; ++i)
		{
			RECT rcItem{};
			TabCtrl_GetItemRect(hWnd, i, &rcItem);
			RECT rcFrame{ rcItem };

			RECT rcIntersect{};
			if (::IntersectRect(&rcIntersect, &rect, &rcItem) == TRUE)
			{
				const bool isHot = ::PtInRect(&rcItem, ptCursor) == TRUE;
				const bool isSelectedTab = (i == iSelTab);

				::SetBkMode(hdc, TRANSPARENT);

				HRGN hClip = ::CreateRectRgnIndirect(&rcItem);
				::SelectClipRgn(hdc, hClip);

				::InflateRect(&rcItem, -1, -1);
				rcItem.right += 1;

				std::wstring label(MAX_PATH, L'\0');
				TCITEM tci{};
				tci.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
				tci.dwStateMask = TCIS_HIGHLIGHTED;
				tci.pszText = label.data();
				tci.cchTextMax = MAX_PATH - 1;

				TabCtrl_GetItem(hWnd, i, &tci);

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				const bool isBtn = (nStyle & TCS_BUTTONS) == TCS_BUTTONS;
				if (isBtn)
				{
					const bool isHighlighted = (tci.dwState & TCIS_HIGHLIGHTED) == TCIS_HIGHLIGHTED;
					::FillRect(hdc, &rcItem, isHighlighted ? DarkMode::getHotBackgroundBrush() : DarkMode::getDlgBackgroundBrush());
					::SetTextColor(hdc, isHighlighted ? DarkMode::getLinkTextColor() : DarkMode::getDarkerTextColor());
				}
				else
				{
					// for consistency getBackgroundBrush()
					// would be better, than getCtrlBackgroundBrush(),
					// however default getBackgroundBrush() has same color
					// as getDlgBackgroundBrush()
					auto getBrush = [&]() -> HBRUSH {
						if (isSelectedTab)
						{
							return DarkMode::getDlgBackgroundBrush();
						}

						if (isHot)
						{
							return DarkMode::getHotBackgroundBrush();
						}
						return DarkMode::getCtrlBackgroundBrush();
					};

					::FillRect(hdc, &rcItem, getBrush());
					::SetTextColor(hdc, (isHot || isSelectedTab) ? DarkMode::getTextColor() : DarkMode::getDarkerTextColor());
				}

				RECT rcText{ rcItem };
				if (!isBtn)
				{
					if (isSelectedTab)
					{
						::OffsetRect(&rcText, 0, -1);
						::InflateRect(&rcFrame, 0, 1);
					}

					if (i != nTabs - 1)
					{
						rcFrame.right += 1;
					}
				}

				if (tci.iImage != -1)
				{
					int cx = 0;
					int cy = 0;
					auto hImagelist = TabCtrl_GetImageList(hWnd);
					static constexpr int offset = 2;
					::ImageList_GetIconSize(hImagelist, &cx, &cy);
					::ImageList_Draw(hImagelist, tci.iImage, hdc, rcText.left + offset, rcText.top + (((rcText.bottom - rcText.top) - cy) / 2), ILD_NORMAL);
					rcText.left += cx;
				}

				::DrawText(hdc, label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				::FrameRect(hdc, &rcFrame, DarkMode::getEdgeBrush());

				if (isSelectedTab && hasFocusRect)
				{
					::InflateRect(&rcFrame, -2, -1);
					::DrawFocusRect(hdc, &rcFrame);
				}

				::SelectClipRgn(hdc, holdClip);
				::DeleteObject(hClip);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectClipRgn(hdc, holdClip);
		if (holdClip != nullptr)
		{
			::DeleteObject(holdClip);
			holdClip = nullptr;
		}
		::SelectObject(hdc, holdPen);
	}

	static LRESULT CALLBACK TabPaintSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pTabBufferData = reinterpret_cast<BufferData*>(dwRefData);
		const auto& hMemDC = pTabBufferData->getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, TabPaintSubclass, uIdSubclass);
				delete pTabBufferData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				if ((nStyle & TCS_VERTICAL) == TCS_VERTICAL)
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (pTabBufferData->ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					DarkMode::paintTab(hWnd, hMemDC, rcClient);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_UPDATEUISTATE:
			{
				if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
				{
					::InvalidateRect(hWnd, nullptr, FALSE);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static void setTabCtrlPaintSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<BufferData>(hWnd, TabPaintSubclass, kTabPaintSubclassID);
	}

	static void removeTabCtrlPaintSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<BufferData>(hWnd, TabPaintSubclass, kTabPaintSubclassID);
	}

	static LRESULT CALLBACK TabUpDownSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, TabUpDownSubclass, uIdSubclass);
				break;
			}

			case WM_PARENTNOTIFY:
			{
				if (LOWORD(wParam) == WM_CREATE)
				{
					auto hUpDown = reinterpret_cast<HWND>(lParam);
					if (CmpWndClassName(hUpDown, UPDOWN_CLASS))
					{
						DarkMode::setUpDownCtrlSubclass(hUpDown);
						return 0;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setTabCtrlUpDownSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, TabUpDownSubclass, kTabUpDownSubclassID);
	}

	void removeTabCtrlUpDownSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, TabUpDownSubclass, kTabUpDownSubclassID);
	}

	void setTabCtrlSubclass(HWND hWnd)
	{
		DarkMode::setTabCtrlPaintSubclass(hWnd);
		DarkMode::setTabCtrlUpDownSubclass(hWnd);
	}

	void removeTabCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeTabCtrlPaintSubclass(hWnd);
		DarkMode::removeTabCtrlUpDownSubclass(hWnd);
	}

	static void setTabCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::setDarkTooltips(hWnd, ToolTipsType::tabbar);
		}

		if (p._subclass)
		{
			DarkMode::setTabCtrlSubclass(hWnd);
		}
	}

	struct BorderMetricsData
	{
		UINT _dpi = USER_DEFAULT_SCREEN_DPI;
		LONG _xEdge = ::GetSystemMetrics(SM_CXEDGE);
		LONG _yEdge = ::GetSystemMetrics(SM_CYEDGE);
		LONG _xScroll = ::GetSystemMetrics(SM_CXVSCROLL);
		LONG _yScroll = ::GetSystemMetrics(SM_CYVSCROLL);
		bool _isHot = false;
	};

	static void ncPaintCustomBorder(HWND hWnd, const BorderMetricsData& borderMetricsData)
	{
		HDC hdc = ::GetWindowDC(hWnd);
		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);
		rcClient.right += (2 * borderMetricsData._xEdge);

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasVerScrollbar = (nStyle & WS_VSCROLL) == WS_VSCROLL;
		if (hasVerScrollbar)
		{
			rcClient.right += borderMetricsData._xScroll;
		}

		rcClient.bottom += (2 * borderMetricsData._yEdge);

		const bool hasHorScrollbar = (nStyle & WS_HSCROLL) == WS_HSCROLL;
		if (hasHorScrollbar)
		{
			rcClient.bottom += borderMetricsData._yScroll;
		}

		HPEN hPen = ::CreatePen(PS_SOLID, 1, (::IsWindowEnabled(hWnd) == TRUE) ? DarkMode::getBackgroundColor() : DarkMode::getDlgBackgroundColor());
		RECT rcInner{ rcClient };
		::InflateRect(&rcInner, -1, -1);
		DarkMode::paintRoundFrameRect(hdc, rcInner, hPen);
		::DeleteObject(hPen);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE;
		const bool hasFocus = ::GetFocus() == hWnd;

		HPEN hEnabledPen = ((borderMetricsData._isHot && isHot) || hasFocus ? DarkMode::getHotEdgePen() : DarkMode::getEdgePen());

		DarkMode::paintRoundFrameRect(hdc, rcClient, (::IsWindowEnabled(hWnd) == TRUE) ? hEnabledPen : DarkMode::getDisabledEdgePen());

		::ReleaseDC(hWnd, hdc);
	}

	static LRESULT CALLBACK CustomBorderSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pBorderMetricsData = reinterpret_cast<BorderMetricsData*>(dwRefData);

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, CustomBorderSubclass, uIdSubclass);
				delete pBorderMetricsData;
				break;
			}

			case WM_NCPAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				::DefSubclassProc(hWnd, uMsg, wParam, lParam);

				DarkMode::ncPaintCustomBorder(hWnd, *pBorderMetricsData);

				return 0;
			}

			case WM_NCCALCSIZE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				auto* lpRect = reinterpret_cast<LPRECT>(lParam);
				::InflateRect(lpRect, -(pBorderMetricsData->_xEdge), -(pBorderMetricsData->_yEdge));

				break;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				DarkMode::redrawWindowFrame(hWnd);
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				if (::GetFocus() == hWnd)
				{
					break;
				}

				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				::TrackMouseEvent(&tme);

				if (!pBorderMetricsData->_isHot)
				{
					pBorderMetricsData->_isHot = true;
					DarkMode::redrawWindowFrame(hWnd);
				}
				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				if (pBorderMetricsData->_isHot)
				{
					pBorderMetricsData->_isHot = false;
					DarkMode::redrawWindowFrame(hWnd);
				}

				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE | TME_CANCEL;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				::TrackMouseEvent(&tme);
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, kCustomBorderSubclassID);
	}

	void removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, kCustomBorderSubclassID);
	}

	static void setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p, bool isListBox)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasScrollBar = ((nStyle & WS_HSCROLL) == WS_HSCROLL) || ((nStyle & WS_VSCROLL) == WS_VSCROLL);

		// edit control without scroll bars
		if (DarkMode::isThemePrefered()
			&& p._theme
			&& !isListBox
			&& !hasScrollBar)
		{
			DarkMode::setDarkThemeExperimental(hWnd, L"CFD");
		}
		else
		{
			if (p._theme && (isListBox || hasScrollBar))
			{
				// dark scroll bars for list box or edit control
				::SetWindowTheme(hWnd, p._themeClassName, nullptr);
			}

			const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
			const bool hasClientEdge = (nExStyle & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE;
			const bool isCBoxListBox = isListBox && (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;

			if (p._subclass && hasClientEdge && !isCBoxListBox)
			{
				DarkMode::setCustomBorderForListBoxOrEditCtrlSubclass(hWnd);
			}

			if (::GetWindowSubclass(hWnd, CustomBorderSubclass, kCustomBorderSubclassID, nullptr) == TRUE)
			{
				const bool enableClientEdge = !DarkMode::isEnabled();
				DarkMode::setWindowExStyle(hWnd, enableClientEdge, WS_EX_CLIENTEDGE);
			}
		}
	}

	struct ComboBoxData
	{
		ThemeData _themeData{ VSCLASS_COMBOBOX };
		BufferData _bufferData;

		LONG_PTR _cbStyle = CBS_SIMPLE;

		ComboBoxData() = delete;

		explicit ComboBoxData(LONG_PTR cbStyle)
			: _cbStyle(cbStyle)
		{}
	};

	static void paintCombobox(HWND hWnd, HDC hdc, ComboBoxData& comboBoxData)
	{
		auto& themeData = comboBoxData._themeData;
		const auto& hTheme = themeData.getHTheme();

		const bool hasTheme = themeData.ensureTheme(hWnd);

		COMBOBOXINFO cbi{};
		cbi.cbSize = sizeof(COMBOBOXINFO);
		::GetComboBoxInfo(hWnd, &cbi);

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		POINT ptCursor{};
		::GetCursorPos(&ptCursor);
		::ScreenToClient(hWnd, &ptCursor);

		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE && !isDisabled;

		bool hasFocus = false;

		::SelectObject(hdc, reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0)));
		::SetBkMode(hdc, TRANSPARENT); // for non-theme DrawText

		RECT rcArrow{ cbi.rcButton };
		rcArrow.left -= 1;

		auto getBrush = [&]() -> HBRUSH {
			if (isDisabled)
			{
				return DarkMode::getDlgBackgroundBrush();
			}

			if (isHot)
			{
				return DarkMode::getHotBackgroundBrush();
			}
			return DarkMode::getCtrlBackgroundBrush();
		};

		HBRUSH hBrush = getBrush();

		// CBS_DROPDOWN and CBS_SIMPLE text is handled by parent by WM_CTLCOLOREDIT
		if (comboBoxData._cbStyle == CBS_DROPDOWNLIST)
		{
			// erase background on item change
			::FillRect(hdc, &rcClient, hBrush);

			const auto index = static_cast<int>(::SendMessage(hWnd, CB_GETCURSEL, 0, 0));
			if (index != CB_ERR)
			{
				const auto bufferLen = static_cast<size_t>(::SendMessage(hWnd, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0));
				std::wstring buffer(bufferLen + 1, L'\0');
				::SendMessage(hWnd, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer.data()));

				RECT rcText{ cbi.rcItem };
				::InflateRect(&rcText, -2, 0);

				static constexpr DWORD dtFlags = DT_NOPREFIX | DT_LEFT | DT_VCENTER | DT_SINGLELINE;
				if (hasTheme)
				{
					DTTOPTS dtto{};
					dtto.dwSize = sizeof(DTTOPTS);
					dtto.dwFlags = DTT_TEXTCOLOR;
					dtto.crText = isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

					::DrawThemeTextEx(hTheme, hdc, CP_DROPDOWNITEM, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, buffer.c_str(), -1, dtFlags, &rcText, &dtto);
				}
				else
				{
					::SetTextColor(hdc, isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor());
					::DrawText(hdc, buffer.c_str(), -1, &rcText, dtFlags);
				}
			}

			hasFocus = ::GetFocus() == hWnd;
			if (!isDisabled && hasFocus && ::SendMessage(hWnd, CB_GETDROPPEDSTATE, 0, 0) == FALSE)
			{
				::DrawFocusRect(hdc, &cbi.rcItem);
			}
		}
		else if (cbi.hwndItem != nullptr)
		{
			hasFocus = ::GetFocus() == cbi.hwndItem;

			::FillRect(hdc, &rcArrow, hBrush);
		}

		HPEN hPen = nullptr;
		if (isDisabled)
		{
			hPen = DarkMode::getDisabledEdgePen();
		}
		else if ((isHot || hasFocus || comboBoxData._cbStyle == CBS_SIMPLE))
		{
			hPen = DarkMode::getHotEdgePen();
		}
		else
		{
			hPen = DarkMode::getEdgePen();
		}
		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, hPen));

		if (comboBoxData._cbStyle != CBS_SIMPLE)
		{
			if (hasTheme)
			{
				const RECT rcThemedArrow{ rcArrow.left, rcArrow.top - 1, rcArrow.right, rcArrow.bottom - 1 };
				::DrawThemeBackground(hTheme, hdc, CP_DROPDOWNBUTTONRIGHT, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, &rcThemedArrow, nullptr);
			}
			else
			{
				auto getTextClr = [&]() -> COLORREF {
					if (isDisabled)
					{
						return DarkMode::getDisabledTextColor();
					}

					if (isHot)
					{
						return DarkMode::getTextColor();
					}
					return DarkMode::getDarkerTextColor();
				};

				::SetTextColor(hdc, getTextClr());
				::DrawText(hdc, L"˅", -1, &rcArrow, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
			}
		}

		if (comboBoxData._cbStyle == CBS_DROPDOWNLIST)
		{
			::ExcludeClipRect(hdc, rcClient.left + 1, rcClient.top + 1, rcClient.right - 1, rcClient.bottom - 1);
		}
		else
		{
			::ExcludeClipRect(hdc, cbi.rcItem.left, cbi.rcItem.top, cbi.rcItem.right, cbi.rcItem.bottom);

			if (comboBoxData._cbStyle == CBS_SIMPLE && cbi.hwndList != nullptr)
			{
				RECT rcItem{ cbi.rcItem };
				::MapWindowPoints(cbi.hwndItem, hWnd, reinterpret_cast<LPPOINT>(&rcItem), 2);
				rcClient.bottom = rcItem.bottom;
			}

			RECT rcInner{ rcClient };
			::InflateRect(&rcInner, -1, -1);

			if (comboBoxData._cbStyle == CBS_DROPDOWN)
			{
				const std::array<POINT, 2> edge{ {
					{ rcArrow.left - 1, rcArrow.top },
					{ rcArrow.left - 1, rcArrow.bottom }
				} };
				::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

				::ExcludeClipRect(hdc, rcArrow.left - 1, rcArrow.top, rcArrow.right, rcArrow.bottom);

				rcInner.right = rcArrow.left - 1;
			}

			HPEN hInnerPen = ::CreatePen(PS_SOLID, 1, isDisabled ? DarkMode::getDlgBackgroundColor() : DarkMode::getBackgroundColor());
			DarkMode::paintRoundFrameRect(hdc, rcInner, hInnerPen);
			::DeleteObject(hInnerPen);
			::InflateRect(&rcInner, -1, -1);
			::FillRect(hdc, &rcInner, isDisabled ? DarkMode::getDlgBackgroundBrush() : DarkMode::getCtrlBackgroundBrush());
		}

		static const int roundness = DarkMode::isAtLeastWindows11() ? kWin11CornerRoundness : 0;
		DarkMode::paintRoundFrameRect(hdc, rcClient, hPen, roundness, roundness);

		::SelectObject(hdc, holdPen);
	}

	static LRESULT CALLBACK ComboBoxSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pComboboxData = reinterpret_cast<ComboBoxData*>(dwRefData);
		auto& themeData = pComboboxData->_themeData;
		auto& bufferData = pComboboxData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ComboBoxSubclass, uIdSubclass);
				delete pComboboxData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (pComboboxData->_cbStyle != CBS_DROPDOWN && hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (pComboboxData->_cbStyle != CBS_DROPDOWN)
				{
					if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
					{
						::EndPaint(hWnd, &ps);
						return 0;
					}

					RECT rcClient{};
					::GetClientRect(hWnd, &rcClient);

					if (bufferData.ensureBuffer(hdc, rcClient))
					{
						const int savedState = ::SaveDC(hMemDC);
						::IntersectClipRect(
							hMemDC,
							ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
						);

						DarkMode::paintCombobox(hWnd, hMemDC, *pComboboxData);

						::RestoreDC(hMemDC, savedState);

						::BitBlt(
							hdc,
							ps.rcPaint.left, ps.rcPaint.top,
							ps.rcPaint.right - ps.rcPaint.left,
							ps.rcPaint.bottom - ps.rcPaint.top,
							hMemDC,
							ps.rcPaint.left, ps.rcPaint.top,
							SRCCOPY
						);
					}
				}
				else
				{
					DarkMode::paintCombobox(hWnd, hdc, *pComboboxData);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_ENABLE:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
				return retVal;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxCtrlSubclass(HWND hWnd)
	{
		const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
		DarkMode::setSubclass<ComboBoxData>(hWnd, ComboBoxSubclass, kComboBoxSubclassID, cbStyle);
	}

	void removeComboBoxCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ComboBoxData>(hWnd, ComboBoxSubclass, kComboBoxSubclassID);
	}

	static void setComboBoxCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
		const bool isCbList = cbStyle == CBS_DROPDOWNLIST;
		const bool isCbSimple = cbStyle == CBS_SIMPLE;

		if (isCbList
			|| cbStyle == CBS_DROPDOWN
			|| isCbSimple)
		{
			COMBOBOXINFO cbi{};
			cbi.cbSize = sizeof(COMBOBOXINFO);
			if (::GetComboBoxInfo(hWnd, &cbi) == TRUE)
			{
				if (p._theme && cbi.hwndList != nullptr)
				{
					if (isCbSimple)
					{
						DarkMode::replaceClientEdgeWithBorderSafe(cbi.hwndList);
					}

					// dark scroll bar for list box of combo box
					::SetWindowTheme(cbi.hwndList, p._themeClassName, nullptr);
				}
			}

			if (!DarkMode::isThemePrefered() && p._subclass)
			{
				HWND hParent = ::GetParent(hWnd);
				if ((hParent == nullptr || GetWndClassName(hParent) != WC_COMBOBOXEX))
				{
					DarkMode::setComboBoxCtrlSubclass(hWnd);
				}
			}

			if (p._theme) // for light dropdown arrow in dark mode
			{
				DarkMode::setDarkThemeExperimental(hWnd, L"CFD");

				if (!isCbList)
				{
					::SendMessage(hWnd, CB_SETEDITSEL, 0, 0); // clear selection
				}
			}
		}
	}

	static LRESULT CALLBACK ComboboxExSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ComboboxExSubclass, uIdSubclass);
				DarkMode::unhookSysColor();
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, DarkMode::getDlgBackgroundBrush());
				return TRUE;
			}

			case WM_CTLCOLOREDIT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORLISTBOX:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorListbox(wParam, lParam);
			}

			case WM_COMMAND:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				// ComboboxEx has only one child combo box, so only control-defined notification code is checked.
				// Hooking is done only when list box is about to show. And unhook when list box is closed.
				// This process is used to avoid visual glitches in other GUI.
				switch (HIWORD(wParam))
				{
					case CBN_DROPDOWN:
					{
						DarkMode::hookSysColor();
						break;
					}

					case CBN_CLOSEUP:
					{
						DarkMode::unhookSysColor();
						break;
					}

					default:
					{
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxExCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, ComboboxExSubclass, kComboBoxExSubclassID);
	}

	void removeComboBoxExCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, ComboboxExSubclass, kComboBoxExSubclassID);
		DarkMode::unhookSysColor();
	}

	static void setComboBoxExCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			DarkMode::setComboBoxExCtrlSubclass(hWnd);
		}
	}

	static LRESULT CALLBACK ListViewSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ListViewSubclass, uIdSubclass);
				DarkMode::unhookSysColor();
				break;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				const auto lvStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & LVS_TYPEMASK;
				const bool isReport = (lvStyle == LVS_REPORT);
				bool hasGridlines = false;
				if (isReport)
				{
					const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
					hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
				}

				if (hasGridlines)
				{
					DarkMode::hookSysColor();
					const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
					DarkMode::unhookSysColor();
					return retVal;
				}
				break;
			}

			// For edit control, which is created when renaming/editing items
			case WM_CTLCOLOREDIT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_NOTIFY:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW)
				{
					auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
					switch (lpnmcd->dwDrawStage)
					{
						case CDDS_PREPAINT:
						{
							if (DarkMode::isExperimentalActive())
							{
								return CDRF_NOTIFYITEMDRAW;
							}
							return CDRF_DODEFAULT;
						}

						case CDDS_ITEMPREPAINT:
						{
							::SetTextColor(lpnmcd->hdc, DarkMode::getDarkerTextColor());

							return CDRF_NEWFONT;
						}

						default:
						{
							return CDRF_DODEFAULT;
						}
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setListViewCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, ListViewSubclass, kListViewSubclassID);
	}

	void removeListViewCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, ListViewSubclass, kListViewSubclassID);
	}

	static void setListViewCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		HWND hHeader = ListView_GetHeader(hWnd);

		if (p._theme)
		{
			ListView_SetTextColor(hWnd, DarkMode::getViewTextColor());
			ListView_SetTextBkColor(hWnd, DarkMode::getViewBackgroundColor());
			ListView_SetBkColor(hWnd, DarkMode::getViewBackgroundColor());

			DarkMode::setDarkListView(hWnd);
			DarkMode::setDarkListViewCheckboxes(hWnd);
			DarkMode::setDarkTooltips(hWnd, DarkMode::ToolTipsType::listview);

			if (DarkMode::isThemePrefered())
			{
				DarkMode::setDarkThemeExperimental(hHeader, L"ItemsView");
			}
		}

		if (p._subclass)
		{
			if (!DarkMode::isThemePrefered())
			{
				DarkMode::setHeaderCtrlSubclass(hHeader);
			}

			const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
			ListView_SetExtendedListViewStyle(hWnd, lvExStyle | LVS_EX_DOUBLEBUFFER);
			DarkMode::setListViewCtrlSubclass(hWnd);
		}
	}

	struct HeaderData
	{
		ThemeData _themeData{ VSCLASS_HEADER };
		BufferData _bufferData;
		FontData _fontData{ nullptr };

		POINT _pt{ LONG_MIN, LONG_MIN };
		bool _isHot = false;
		bool _hasBtnStyle = true;
		bool _isPressed = false;

		HeaderData() = delete;

		explicit HeaderData(bool hasBtnStyle)
			: _hasBtnStyle(hasBtnStyle)
		{}
	};

	static void paintHeader(HWND hWnd, HDC hdc, HeaderData& headerData)
	{
		auto& themeData = headerData._themeData;
		const auto& hTheme = themeData.getHTheme();
		const bool hasTheme = themeData.ensureTheme(hWnd);
		auto& fontData = headerData._fontData;

		::SetBkMode(hdc, TRANSPARENT);
		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, DarkMode::getHeaderEdgePen()));

		RECT rcHeader{};
		::GetClientRect(hWnd, &rcHeader);
		::FillRect(hdc, &rcHeader, DarkMode::getHeaderBackgroundBrush());

		LOGFONT lf{};
		if (!fontData.hasFont()
			&& hasTheme
			&& SUCCEEDED(::GetThemeFont(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, TMT_FONT, &lf)))
		{
			fontData.setFont(::CreateFontIndirect(&lf));
		}

		HFONT hFont = (fontData.hasFont()) ? fontData.getFont() : reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		DTTOPTS dtto{};
		if (hasTheme)
		{
			dtto.dwSize = sizeof(DTTOPTS);
			dtto.dwFlags = DTT_TEXTCOLOR;
			dtto.crText = DarkMode::getHeaderTextColor();
		}
		else
		{
			::SetTextColor(hdc, DarkMode::getHeaderTextColor());
		}

		HWND hList = ::GetParent(hWnd);
		const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
		bool hasGridlines = false;
		if (lvStyle == LVS_REPORT)
		{
			const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
			hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
		}

		const int count = Header_GetItemCount(hWnd);
		RECT rcItem{};
		for (int i = 0; i < count; i++)
		{
			Header_GetItemRect(hWnd, i, &rcItem);
			const bool isOnItem = ::PtInRect(&rcItem, headerData._pt) == TRUE;

			if (headerData._hasBtnStyle && isOnItem)
			{
				RECT rcTmp{ rcItem };
				if (hasGridlines)
				{
					::OffsetRect(&rcTmp, 1, 0);
				}
				else if (DarkMode::isExperimentalActive())
				{
					::OffsetRect(&rcTmp, -1, 0);
				}
				::FillRect(hdc, &rcTmp, DarkMode::getHeaderHotBackgroundBrush());
			}

			std::wstring buffer(MAX_PATH, L'\0');
			HDITEM hdi{};
			hdi.mask = HDI_TEXT | HDI_FORMAT;
			hdi.pszText = buffer.data();
			hdi.cchTextMax = MAX_PATH - 1;

			Header_GetItem(hWnd, i, &hdi);

			if (hasTheme
				&& ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP
					|| (hdi.fmt & HDF_SORTDOWN) == HDF_SORTDOWN))
			{
				const int iStateID = ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP) ? HSAS_SORTEDUP : HSAS_SORTEDDOWN;
				RECT rcArrow{ rcItem };
				SIZE szArrow{};
				if (SUCCEEDED(::GetThemePartSize(hTheme, hdc, HP_HEADERSORTARROW, iStateID, nullptr, TS_DRAW, &szArrow)))
				{
					rcArrow.bottom = szArrow.cy;
				}

				::DrawThemeBackground(hTheme, hdc, HP_HEADERSORTARROW, iStateID, &rcArrow, nullptr);
			}

			LONG edgeX = rcItem.right;
			if (!hasGridlines)
			{
				--edgeX;
				if (DarkMode::isExperimentalActive())
				{
					--edgeX;
				}
			}

			const std::array<POINT, 2> edge{ {
				{ edgeX, rcItem.top },
				{ edgeX, rcItem.bottom }
			} };
			::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

			DWORD dtFlags = DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_HIDEPREFIX;
			if ((hdi.fmt & HDF_RIGHT) == HDF_RIGHT)
			{
				dtFlags |= DT_RIGHT;
			}
			else if ((hdi.fmt & HDF_CENTER) == HDF_CENTER)
			{
				dtFlags |= DT_CENTER;
			}

			static constexpr LONG lOffset = 6;
			static constexpr LONG rOffset = 8;

			rcItem.left += lOffset;
			rcItem.right -= rOffset;

			if (headerData._isPressed && isOnItem)
			{
				::OffsetRect(&rcItem, 1, 1);
			}

			if (hasTheme)
			{
				::DrawThemeTextEx(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, hdi.pszText, -1, dtFlags, &rcItem, &dtto);
			}
			else
			{
				::DrawText(hdc, hdi.pszText, -1, &rcItem, dtFlags);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectObject(hdc, holdPen);
	}

	static LRESULT CALLBACK HeaderSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pHeaderData = reinterpret_cast<HeaderData*>(dwRefData);
		auto& themeData = pHeaderData->_themeData;
		auto& bufferData = pHeaderData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, HeaderSubclass, uIdSubclass);
				delete pHeaderData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					DarkMode::paintHeader(hWnd, hMemDC, *pHeaderData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case WM_LBUTTONDOWN:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				pHeaderData->_isPressed = true;
				break;
			}

			case WM_LBUTTONUP:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				pHeaderData->_isPressed = false;
				break;
			}

			case WM_MOUSEMOVE:
			{
				if (!pHeaderData->_hasBtnStyle || pHeaderData->_isPressed)
				{
					break;
				}

				TRACKMOUSEEVENT tme{};

				if (!pHeaderData->_isHot)
				{
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_LEAVE;
					tme.hwndTrack = hWnd;

					::TrackMouseEvent(&tme);

					pHeaderData->_isHot = true;
				}

				pHeaderData->_pt.x = GET_X_LPARAM(lParam);
				pHeaderData->_pt.y = GET_Y_LPARAM(lParam);

				::InvalidateRect(hWnd, nullptr, FALSE);
				break;
			}

			case WM_MOUSELEAVE:
			{
				if (!pHeaderData->_hasBtnStyle)
				{
					break;
				}

				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

				pHeaderData->_isHot = false;
				pHeaderData->_pt.x = LONG_MIN;
				pHeaderData->_pt.y = LONG_MIN;

				::InvalidateRect(hWnd, nullptr, TRUE);

				return retVal;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setHeaderCtrlSubclass(HWND hWnd)
	{
		const bool hasBtnStyle = (::GetWindowLongPtr(hWnd, GWL_STYLE) & HDS_BUTTONS) == HDS_BUTTONS;
		DarkMode::setSubclass<HeaderData>(hWnd, HeaderSubclass, kHeaderSubclassID, hasBtnStyle);
	}

	void removeHeaderCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<HeaderData>(hWnd, HeaderSubclass, kHeaderSubclassID);
	}

	struct StatusBarData
	{
		ThemeData _themeData{ VSCLASS_STATUS };
		BufferData _bufferData;
		FontData _fontData;

		StatusBarData() = delete;

		explicit StatusBarData(const HFONT& hFont)
			: _fontData(hFont)
		{}
	};

	static void paintStatusBar(HWND hWnd, HDC hdc, StatusBarData& statusBarData)
	{
		const auto& hFont = statusBarData._fontData.getFont();

		struct {
			int horizontal = 0;
			int vertical = 0;
			int between = 0;
		} borders{};

		::SendMessage(hWnd, SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(&borders));

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasSizeGrip = (nStyle & SBARS_SIZEGRIP) == SBARS_SIZEGRIP;

		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, DarkMode::getEdgePen()));
		auto holdFont = static_cast<HFONT>(::SelectObject(hdc, hFont));

		::SetBkMode(hdc, TRANSPARENT);
		::SetTextColor(hdc, DarkMode::getTextColor());

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		::FillRect(hdc, &rcClient, DarkMode::getBackgroundBrush());

		const auto nParts = static_cast<int>(::SendMessage(hWnd, SB_GETPARTS, 0, 0));
		std::wstring str;
		RECT rcPart{};
		RECT rcIntersect{};
		const int iLastDiv = nParts - (hasSizeGrip ? 1 : 0);
		const bool drawEdge = (nParts >= 2 || !hasSizeGrip);
		for (int i = 0; i < nParts; ++i)
		{
			::SendMessage(hWnd, SB_GETRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rcPart));
			if (::IntersectRect(&rcIntersect, &rcPart, &rcClient) == FALSE)
			{
				continue;
			}

			if (drawEdge && (i < iLastDiv))
			{
				const std::array<POINT, 2> edges{ {
					{ rcPart.right - borders.between, rcPart.top + 1 },
					{ rcPart.right - borders.between, rcPart.bottom - 3 }
				} };
				::Polyline(hdc, edges.data(), static_cast<int>(edges.size()));
			}

			rcPart.left += borders.between;
			rcPart.right -= borders.vertical;

			const LRESULT retValLen = ::SendMessage(hWnd, SB_GETTEXTLENGTH, static_cast<WPARAM>(i), 0);
			const DWORD cchText = LOWORD(retValLen);

			str.resize(static_cast<size_t>(cchText) + 1);
			const LRESULT retValText = ::SendMessage(hWnd, SB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(str.data()));

			if (cchText == 0 && (HIWORD(retValLen) & SBT_OWNERDRAW) != 0)
			{
				const auto id = static_cast<UINT>(::GetDlgCtrlID(hWnd));
				DRAWITEMSTRUCT dis{
					0
					, 0
					, static_cast<UINT>(i)
					, ODA_DRAWENTIRE
					, id
					, hWnd
					, hdc
					, rcPart
					, static_cast<ULONG_PTR>(retValText)
				};

				::SendMessage(::GetParent(hWnd), WM_DRAWITEM, id, reinterpret_cast<LPARAM>(&dis));
			}
			else
			{
				::DrawText(hdc, str.c_str(), -1, &rcPart, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
			}
		}

		/*POINT edgeHor[]{
			{rcClient.left, rcClient.top},
			{rcClient.right, rcClient.top}
		};
		Polyline(hdc, edgeHor, _countof(edgeHor));*/

		if (hasSizeGrip)
		{
			auto& themeData = statusBarData._themeData;
			const auto& hTheme = themeData.getHTheme();
			const bool hasTheme = themeData.ensureTheme(hWnd);
			if (hasTheme)
			{
				SIZE szGrip{};
				::GetThemePartSize(hTheme, hdc, SP_GRIPPER, 0, &rcClient, TS_DRAW, &szGrip);
				RECT rcGrip{ rcClient };
				rcGrip.left = rcGrip.right - szGrip.cx;
				rcGrip.top = rcGrip.bottom - szGrip.cy;
				::DrawThemeBackground(hTheme, hdc, SP_GRIPPER, 0, &rcGrip, nullptr);
			}
		}

		::SelectObject(hdc, holdFont);
		::SelectObject(hdc, holdPen);
	}

	static LRESULT CALLBACK StatusBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData)
	{
		auto* pStatusBarData = reinterpret_cast<StatusBarData*>(dwRefData);
		auto& themeData = pStatusBarData->_themeData;
		auto& bufferData = pStatusBarData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, StatusBarSubclass, uIdSubclass);
				delete pStatusBarData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					DarkMode::paintStatusBar(hWnd, hMemDC, *pStatusBarData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			case WM_THEMECHANGED:
			{
				themeData.closeTheme();

				LOGFONT lf{};
				NONCLIENTMETRICS ncm{};
				ncm.cbSize = sizeof(NONCLIENTMETRICS);
				if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0) != FALSE)
				{
					lf = ncm.lfStatusFont;
					pStatusBarData->_fontData.setFont(::CreateFontIndirect(&lf));
				}

				if (uMsg != WM_THEMECHANGED)
				{
					return 0;
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setStatusBarCtrlSubclass(HWND hWnd)
	{
		LOGFONT lf{};
		NONCLIENTMETRICS ncm{};
		ncm.cbSize = sizeof(NONCLIENTMETRICS);
		if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0) != FALSE)
		{
			lf = ncm.lfStatusFont;
		}
		DarkMode::setSubclass<StatusBarData>(hWnd, StatusBarSubclass, kStatusBarSubclassID, ::CreateFontIndirect(&lf));
	}

	void removeStatusBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<StatusBarData>(hWnd, StatusBarSubclass, kStatusBarSubclassID);
	}

	static void setStatusBarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			DarkMode::setStatusBarCtrlSubclass(hWnd);
		}
	}

	struct ProgressBarData
	{
		ThemeData _themeData{ VSCLASS_PROGRESS };
		BufferData _bufferData;

		int _iStateID = PBFS_PARTIAL;

		explicit ProgressBarData(HWND hWnd)
			: _iStateID(static_cast<int>(::SendMessage(hWnd, PBM_GETSTATE, 0, 0)))
		{}
	};

	static void getProgressBarRects(HWND hWnd, RECT* rcEmpty, RECT* rcFilled)
	{
		const auto pos = static_cast<int>(::SendMessage(hWnd, PBM_GETPOS, 0, 0));

		PBRANGE range{};
		::SendMessage(hWnd, PBM_GETRANGE, TRUE, reinterpret_cast<LPARAM>(&range));
		const int iMin = range.iLow;

		const int currPos = pos - iMin;
		if (currPos != 0)
		{
			const int totalWidth = rcEmpty->right - rcEmpty->left;
			rcFilled->left = rcEmpty->left;
			rcFilled->top = rcEmpty->top;
			rcFilled->bottom = rcEmpty->bottom;
			rcFilled->right = rcEmpty->left + static_cast<int>(static_cast<double>(currPos) / (range.iHigh - iMin) * totalWidth);

			rcEmpty->left = rcFilled->right; // to avoid painting under filled part
		}
	}

	static void paintProgressBar(HWND hWnd, HDC hdc, const ProgressBarData& progressBarData)
	{
		const auto& hTheme = progressBarData._themeData.getHTheme();

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		DarkMode::paintRoundFrameRect(hdc, rcClient, DarkMode::getEdgePen(), 0, 0);

		::InflateRect(&rcClient, -1, -1);
		rcClient.left = 1;

		RECT rcFill{};
		DarkMode::getProgressBarRects(hWnd, &rcClient, &rcFill);
		::DrawThemeBackground(hTheme, hdc, PP_FILL, progressBarData._iStateID, &rcFill, nullptr);
		::FillRect(hdc, &rcClient, DarkMode::getCtrlBackgroundBrush());
	}

	static LRESULT CALLBACK ProgressBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pProgressBarData = reinterpret_cast<ProgressBarData*>(dwRefData);
		auto& themeData = pProgressBarData->_themeData;
		auto& bufferData = pProgressBarData->_bufferData;
		const auto& hMemDC = bufferData.getHMemDC();

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, ProgressBarSubclass, uIdSubclass);
				delete pProgressBarData;
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
				{
					break;
				}

				const auto* hdc = reinterpret_cast<HDC>(wParam);
				if (hdc != hMemDC)
				{
					return FALSE;
				}
				return TRUE;
			}

			case WM_PAINT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				PAINTSTRUCT ps{};
				HDC hdc = ::BeginPaint(hWnd, &ps);

				if (ps.rcPaint.right <= ps.rcPaint.left || ps.rcPaint.bottom <= ps.rcPaint.top)
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				if (bufferData.ensureBuffer(hdc, rcClient))
				{
					const int savedState = ::SaveDC(hMemDC);
					::IntersectClipRect(
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom
					);

					DarkMode::paintProgressBar(hWnd, hMemDC, *pProgressBarData);

					::RestoreDC(hMemDC, savedState);

					::BitBlt(
						hdc,
						ps.rcPaint.left, ps.rcPaint.top,
						ps.rcPaint.right - ps.rcPaint.left,
						ps.rcPaint.bottom - ps.rcPaint.top,
						hMemDC,
						ps.rcPaint.left, ps.rcPaint.top,
						SRCCOPY
					);
				}

				::EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			{
				themeData.closeTheme();
				return 0;
			}

			case WM_THEMECHANGED:
			{
				themeData.closeTheme();
				break;
			}

			case PBM_SETSTATE:
			{
				switch (wParam)
				{
					case PBST_NORMAL:
					{
						pProgressBarData->_iStateID = PBFS_NORMAL; // green
						break;
					}

					case PBST_ERROR:
					{
						pProgressBarData->_iStateID = PBFS_ERROR; // red
						break;
					}

					case PBST_PAUSED:
					{
						pProgressBarData->_iStateID = PBFS_PAUSED; // yellow
						break;
					}

					default:
					{
						pProgressBarData->_iStateID = PBFS_PARTIAL; // cyan
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setProgressBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, kProgressBarSubclassID, hWnd);
	}

	void removeProgressBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, kProgressBarSubclassID);
	}

	static void setProgressBarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		if (p._theme && (nStyle & PBS_MARQUEE) == PBS_MARQUEE)
		{
			DarkMode::setProgressBarClassicTheme(hWnd);
		}
		else if (p._subclass)
		{
			DarkMode::setProgressBarCtrlSubclass(hWnd);
		}
	}

	struct StaticTextData
	{
		bool _isEnabled = true;

		StaticTextData() = default;

		explicit StaticTextData(HWND hWnd)
			: _isEnabled(::IsWindowEnabled(hWnd) == TRUE)
		{}
	};

	static LRESULT CALLBACK StaticTextSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pStaticTextData = reinterpret_cast<StaticTextData*>(dwRefData);

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, StaticTextSubclass, uIdSubclass);
				delete pStaticTextData;
				break;
			}

			case WM_ENABLE:
			{
				pStaticTextData->_isEnabled = (wParam == TRUE);

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				if (!pStaticTextData->_isEnabled)
				{
					::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle & ~WS_DISABLED);
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::MapWindowPoints(hWnd, ::GetParent(hWnd), reinterpret_cast<LPPOINT>(&rcClient), 2);
				::RedrawWindow(::GetParent(hWnd), &rcClient, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

				if (!pStaticTextData->_isEnabled)
				{
					::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle | WS_DISABLED);
				}

				return 0;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setStaticTextCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<StaticTextData>(hWnd, StaticTextSubclass, kStaticTextSubclassID, hWnd);
	}

	void removeStaticTextCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<StaticTextData>(hWnd, StaticTextSubclass, kStaticTextSubclassID);
	}

	static void setStaticTextCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			DarkMode::setStaticTextCtrlSubclass(hWnd);
		}
	}

	static void setTreeViewCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			TreeView_SetTextColor(hWnd, DarkMode::getViewTextColor());
			TreeView_SetBkColor(hWnd, DarkMode::getViewBackgroundColor());

			DarkMode::setTreeViewWindowTheme(hWnd, p._theme);
			DarkMode::setDarkTooltips(hWnd, DarkMode::ToolTipsType::treeview);
		}
	}

	static void setToolbarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::setDarkLineAbovePanelToolbar(hWnd);
			DarkMode::setDarkTooltips(hWnd, DarkMode::ToolTipsType::toolbar);
		}
	}

	static void setScrollBarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::setDarkScrollBar(hWnd);
		}
	}

	static void enableSysLinkCtrlCtlColor(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::enableSysLinkCtrlCtlColor(hWnd);
		}
	}

	static void setRichEditCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::setDarkRichEdit(hWnd);
		}
	}

	static void setTrackbarCtrlTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			DarkMode::setDarkTooltips(hWnd, ToolTipsType::trackbar);
			DarkMode::setWindowStyle(hWnd, DarkMode::isEnabled(), TBS_TRANSPARENTBKGND);
		}
	}

	static BOOL CALLBACK DarkEnumChildProc(HWND hWnd, LPARAM lParam)
	{
		const auto& p = *reinterpret_cast<DarkModeParams*>(lParam);
		const std::wstring className = GetWndClassName(hWnd);

		if (className == WC_BUTTON)
		{
			DarkMode::setBtnCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_STATIC)
		{
			DarkMode::setStaticTextCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_COMBOBOX)
		{
			DarkMode::setComboBoxCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_EDIT)
		{
			DarkMode::setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, false);
			return TRUE;
		}

		if (className == WC_LISTBOX)
		{
			DarkMode::setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, true);
			return TRUE;
		}

		if (className == WC_LISTVIEW)
		{
			DarkMode::setListViewCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_TREEVIEW)
		{
			DarkMode::setTreeViewCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == TOOLBARCLASSNAME)
		{
			DarkMode::setToolbarCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == UPDOWN_CLASS)
		{
			DarkMode::setUpDownCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_TABCONTROL)
		{
			DarkMode::setTabCtrlSubclassAndTheme(hWnd, p);
			return TRUE;
		}

		if (className == STATUSCLASSNAME)
		{
			DarkMode::setStatusBarCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_SCROLLBAR)
		{
			DarkMode::setScrollBarCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == WC_COMBOBOXEX)
		{
			DarkMode::setComboBoxExCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == PROGRESS_CLASS)
		{
			DarkMode::setProgressBarCtrlSubclass(hWnd, p);
			return TRUE;
		}

		if (className == WC_LINK)
		{
			DarkMode::enableSysLinkCtrlCtlColor(hWnd, p);
			return TRUE;
		}

		if (className == RICHEDIT_CLASS || className == MSFTEDIT_CLASS)
		{
			DarkMode::setRichEditCtrlTheme(hWnd, p);
			return TRUE;
		}

		if (className == TRACKBAR_CLASS)
		{
			DarkMode::setTrackbarCtrlTheme(hWnd, p);
			return TRUE;
		}

		/*
		// for debugging
		if (className == L"#32770")
		{
			return TRUE;
		}
		*/

		return TRUE;
	}

	void setChildCtrlsSubclassAndTheme(HWND hParent, bool subclass, bool theme)
	{
		DarkModeParams p{
			DarkMode::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr
			, subclass
			, theme
		};

		::EnumChildWindows(hParent, DarkMode::DarkEnumChildProc, reinterpret_cast<LPARAM>(&p));
	}

	void setChildCtrlsTheme(HWND hParent)
	{
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
		DarkMode::setChildCtrlsSubclassAndTheme(hParent, false, true);
#else
		DarkMode::setChildCtrlsSubclassAndTheme(hParent, false, DarkMode::isAtLeastWindows10());
#endif
	}

	static LRESULT CALLBACK WindowEraseBgSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowEraseBgSubclass, uIdSubclass);
				break;
			}

			case WM_ERASEBKGND:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, DarkMode::getDlgBackgroundBrush());
				return TRUE;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowEraseBgSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowEraseBgSubclass, kWindowEraseBgSubclassID);
	}

	void removeWindowEraseBgSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowEraseBgSubclass, kWindowEraseBgSubclassID);
	}

	/**
	 * @brief Window subclass procedure for handling `WM_CTLCOLOR*` messages.
	 *
	 * Handles control drawing messages to apply foreground and background
	 * styling based on control type and class.
	 *
	 * Handles:
	 * - `WM_CTLCOLOREDIT`, `WM_CTLCOLORLISTBOX`, `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
	 * - `WM_PRINTCLIENT` for removing light border for push buttons in dark mode
	 * - Cleans up subclass on `WM_NCDESTROY`
	 *
	 * Uses `DarkMode::onCtlColor*` utilities.
	 *
	 * @param hWnd Window handle being subclassed.
	 * @param uMsg Message identifier.
	 * @param wParam Message-specific data.
	 * @param lParam Message-specific data.
	 * @param uIdSubclass Subclass identifier.
	 * @param dwRefData Reserved data (unused).
	 * @return LRESULT Result of message processing.
	 *
	 * @see DarkMode::onCtlColor()
	 * @see DarkMode::onCtlColorDlg()
	 * @see DarkMode::onCtlColorDlgStaticText()
	 * @see DarkMode::onCtlColorDlgLinkText()
	 * @see DarkMode::onCtlColorListbox()
	 */
	static LRESULT CALLBACK WindowCtlColorSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowCtlColorSubclass, uIdSubclass);
				break;
			}

			case WM_CTLCOLOREDIT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORLISTBOX:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorListbox(wParam, lParam);
			}

			case WM_CTLCOLORDLG:
			{

				if (!DarkMode::isEnabled())
				{
					break;
				}
				return DarkMode::onCtlColorDlg(reinterpret_cast<HDC>(wParam));
			}

			case WM_CTLCOLORSTATIC:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				auto hChild = reinterpret_cast<HWND>(lParam);
				const bool isChildEnabled = ::IsWindowEnabled(hChild) == TRUE;
				const std::wstring className = GetWndClassName(hChild);

				auto hdc = reinterpret_cast<HDC>(wParam);

				if (className == WC_EDIT)
				{
					if (isChildEnabled)
					{
						return DarkMode::onCtlColor(hdc);
					}
					return DarkMode::onCtlColorDlg(hdc);
				}

				if (className == WC_LINK)
				{
					return DarkMode::onCtlColorDlgLinkText(hdc, isChildEnabled);
				}

				DWORD_PTR dwRefData = 0;
				if (::GetWindowSubclass(hChild, StaticTextSubclass, kStaticTextSubclassID, &dwRefData) == TRUE)
				{
					const bool isTextEnabled = (reinterpret_cast<StaticTextData*>(dwRefData))->_isEnabled;
					return DarkMode::onCtlColorDlgStaticText(hdc, isTextEnabled);
				}
				return DarkMode::onCtlColorDlg(hdc);
			}

			case WM_PRINTCLIENT:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}
				return TRUE;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowCtlColorSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowCtlColorSubclass, kWindowCtlColorSubclassID);
	}

	void removeWindowCtlColorSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowCtlColorSubclass, kWindowCtlColorSubclassID);
	}

	[[nodiscard]] static LRESULT prepaintToolbarItem(LPNMTBCUSTOMDRAW& lptbcd)
	{
		lptbcd->hbrMonoDither = DarkMode::getBackgroundBrush();
		lptbcd->hbrLines = DarkMode::getEdgeBrush();
		lptbcd->hpenLines = DarkMode::getEdgePen();
		lptbcd->clrText = DarkMode::getDarkerTextColor();
		lptbcd->clrTextHighlight = DarkMode::getTextColor();
		lptbcd->clrBtnFace = DarkMode::getBackgroundColor();
		lptbcd->clrBtnHighlight = DarkMode::getCtrlBackgroundColor();
		lptbcd->clrHighlightHotTrack = DarkMode::getHotBackgroundColor();
		lptbcd->nStringBkMode = TRANSPARENT;
		lptbcd->nHLStringBkMode = TRANSPARENT;

		const bool isHot = (lptbcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;
		const bool isChecked = (lptbcd->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED;

		RECT rcItem{ lptbcd->nmcd.rc };
		RECT rcDrop{};

		TBBUTTONINFO tbi{};
		tbi.cbSize = sizeof(TBBUTTONINFO);
		tbi.dwMask = TBIF_IMAGE | TBIF_STYLE;
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));
		const bool isIcon = tbi.iImage != I_IMAGENONE;
		const bool isDropDown = ((tbi.fsStyle & BTNS_DROPDOWN) == BTNS_DROPDOWN) && isIcon;
		if (isDropDown)
		{
			const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
			::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcDrop));

			rcItem.right = rcDrop.left;
		}

		static const int roundness = DarkMode::isAtLeastWindows11() ? kWin11CornerRoundness + 1 : 0;

		if (isHot)
		{
			if (!isIcon)
			{
				::FillRect(lptbcd->nmcd.hdc, &rcItem, DarkMode::getHotBackgroundBrush());
			}
			else
			{
				DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
				if (isDropDown)
				{
					DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
				}
			}

			lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED | CDIS_HOT);
		}
		else if (isChecked)
		{
			if (!isIcon)
			{
				::FillRect(lptbcd->nmcd.hdc, &rcItem, DarkMode::getCtrlBackgroundBrush());
			}
			else
			{
				DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
				if (isDropDown)
				{
					DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
				}
			}

			lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED);
		}

		LRESULT retVal = TBCDRF_USECDCOLORS;
		if ((lptbcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			retVal |= TBCDRF_NOBACKGROUND;
		}

		if (isDropDown)
		{
			retVal |= CDRF_NOTIFYPOSTPAINT;
		}

		return retVal;
	}

	[[nodiscard]] static LRESULT postpaintToolbarItem(LPNMTBCUSTOMDRAW& lptbcd)
	{
		TBBUTTONINFO tbi{};
		tbi.cbSize = sizeof(TBBUTTONINFO);
		tbi.dwMask = TBIF_IMAGE;
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));
		const bool isIcon = tbi.iImage != I_IMAGENONE;
		if (!isIcon)
		{
			return CDRF_DODEFAULT;
		}

		auto hFont = reinterpret_cast<HFONT>(::SendMessage(lptbcd->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0));
		auto holdFont = static_cast<HFONT>(::SelectObject(lptbcd->nmcd.hdc, hFont));

		RECT rcArrow{};
		const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcArrow));
		rcArrow.left += 1;
		rcArrow.bottom -= 3;

		::SetBkMode(lptbcd->nmcd.hdc, TRANSPARENT);
		::SetTextColor(lptbcd->nmcd.hdc, DarkMode::getTextColor());
		::DrawText(lptbcd->nmcd.hdc, L"⏷", -1, &rcArrow, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		::SelectObject(lptbcd->nmcd.hdc, holdFont);

		return CDRF_DODEFAULT;
	}

	[[nodiscard]] static LRESULT darkToolbarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lptbcd = reinterpret_cast<LPNMTBCUSTOMDRAW>(lParam);

		switch (lptbcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				LRESULT retVal = CDRF_DODEFAULT;
				if (DarkMode::isEnabled())
				{
					::FillRect(lptbcd->nmcd.hdc, &lptbcd->nmcd.rc, DarkMode::getDlgBackgroundBrush());
					retVal |= CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
				}

				return retVal;
			}

			case CDDS_ITEMPREPAINT:
			{
				return DarkMode::prepaintToolbarItem(lptbcd);
			}

			case CDDS_ITEMPOSTPAINT:
			{
				return DarkMode::postpaintToolbarItem(lptbcd);
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static void prepaintListViewItem(LPNMLVCUSTOMDRAW& lplvcd, bool isReport, bool hasGridlines)
	{
		const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
		const bool isSelected = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED;
		const bool isFocused = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_FOCUSED) == LVIS_FOCUSED;
		const bool isHot = (lplvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;

		HBRUSH hBrush = nullptr;

		if (isSelected)
		{
			lplvcd->clrText = DarkMode::getTextColor();
			lplvcd->clrTextBk = DarkMode::getCtrlBackgroundColor();
			hBrush = DarkMode::getCtrlBackgroundBrush();
		}
		else if (isHot)
		{
			lplvcd->clrText = DarkMode::getTextColor();
			lplvcd->clrTextBk = DarkMode::getHotBackgroundColor();
			hBrush = DarkMode::getHotBackgroundBrush();
		}

		if (hBrush != nullptr)
		{
			if (!isReport || hasGridlines)
			{
				::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, hBrush);
			}
			else
			{
				HWND hHeader = ListView_GetHeader(hList);
				const int nCol = Header_GetItemCount(hHeader);
				const LONG paddingLeft = DarkMode::isThemeDark() ? 1 : 0;
				const LONG paddingRight = DarkMode::isThemeDark() ? 2 : 1;

				LVITEMINDEX lvii{ static_cast<int>(lplvcd->nmcd.dwItemSpec), 0 };
				RECT rcSubitem{
					lplvcd->nmcd.rc.left
					, lplvcd->nmcd.rc.top
					, lplvcd->nmcd.rc.left + ListView_GetColumnWidth(hList, 0) - paddingRight
					, lplvcd->nmcd.rc.bottom
				};
				::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);

				for (int i = 1; i < nCol; ++i)
				{
					ListView_GetItemIndexRect(hList, &lvii, i, LVIR_BOUNDS, &rcSubitem);
					rcSubitem.left -= paddingLeft;
					rcSubitem.right -= paddingRight;
					::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);
				}
			}
		}
		else if (hasGridlines)
		{
			::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getViewBackgroundBrush());
		}

		if (isFocused)
		{
			//::DrawFocusRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc);
		}
		else if (!isSelected && isHot && !hasGridlines)
		{
			::FrameRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getHotEdgeBrush());
		}
	}

	[[nodiscard]] static LRESULT darkListViewNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
		const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
		const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
		const bool isReport = (lvStyle == LVS_REPORT);
		bool hasGridlines = false;
		if (isReport)
		{
			const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
			hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
		}

		switch (lplvcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				if (!DarkMode::isEnabled())
				{
					return CDRF_DODEFAULT;
				}

				if (isReport && hasGridlines)
				{
					::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getViewBackgroundBrush());
				}

				return CDRF_NOTIFYITEMDRAW;
			}

			case CDDS_ITEMPREPAINT:
			{
				DarkMode::prepaintListViewItem(lplvcd, isReport, hasGridlines);
				return CDRF_NEWFONT;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	[[nodiscard]] static LRESULT prepaintTreeViewItem(LPNMTVCUSTOMDRAW& lptvcd)
	{
		LRESULT retVal = CDRF_DODEFAULT;

		if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			lptvcd->clrText = DarkMode::getTextColor();
			lptvcd->clrTextBk = DarkMode::getCtrlBackgroundColor();
			::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, DarkMode::getCtrlBackgroundBrush());

			retVal |= CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
		}
		else if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
		{
			lptvcd->clrText = DarkMode::getTextColor();
			lptvcd->clrTextBk = DarkMode::getHotBackgroundColor();

			if (DarkMode::isAtLeastWindows10() || DarkMode::getTreeViewStyle() == TreeViewStyle::light)
			{
				::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, DarkMode::getHotBackgroundBrush());
				retVal |= CDRF_NOTIFYPOSTPAINT;
			}
			retVal |= CDRF_NEWFONT;
		}

		return retVal;
	}

	static void postpaintTreeViewItem(LPNMTVCUSTOMDRAW& lptvcd)
	{
		RECT rcFrame{ lptvcd->nmcd.rc };
		::InflateRect(&rcFrame, 1, 0);

		if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
		{
			DarkMode::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, DarkMode::getHotEdgePen(), 0, 0);
		}
		else if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
		{
			DarkMode::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, DarkMode::getEdgePen(), 0, 0);
		}
	}

	[[nodiscard]] static LRESULT darkTreeViewNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lptvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);

		switch (lptvcd->nmcd.dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				return DarkMode::isEnabled() ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
			}

			case CDDS_ITEMPREPAINT:
			{
				return DarkMode::prepaintTreeViewItem(lptvcd);
			}

			case CDDS_ITEMPOSTPAINT:
			{
				DarkMode::postpaintTreeViewItem(lptvcd);
				return CDRF_DODEFAULT;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	[[nodiscard]] static LRESULT darkTrackbarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);

		switch (lpnmcd->dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				return DarkMode::isEnabled() ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
			}

			case CDDS_ITEMPREPAINT:
			{
				switch (lpnmcd->dwItemSpec)
				{
					case TBCD_THUMB:
					{
						if ((lpnmcd->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
						{
							::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getCtrlBackgroundBrush());
							return CDRF_SKIPDEFAULT;
						}
						break;
					}

					case TBCD_CHANNEL:
					{
						if (::IsWindowEnabled(lpnmcd->hdr.hwndFrom) == FALSE)
						{
							::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getDlgBackgroundBrush());
							DarkMode::paintRoundFrameRect(lpnmcd->hdc, lpnmcd->rc, DarkMode::getEdgePen(), 0, 0);
						}
						else
						{
							::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getCtrlBackgroundBrush());
						}

						return CDRF_SKIPDEFAULT;
					}

					default:
					{
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	[[nodiscard]] static LRESULT prepaintRebar(LPNMCUSTOMDRAW& lpnmcd)
	{
		if (!DarkMode::isEnabled())
		{
			return CDRF_DODEFAULT;
		}

		::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getDlgBackgroundBrush());
		REBARBANDINFO rbBand{};
		rbBand.cbSize = sizeof(REBARBANDINFO);
		rbBand.fMask = RBBIM_STYLE | RBBIM_CHEVRONLOCATION | RBBIM_CHEVRONSTATE;
		::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETBANDINFO, 0, reinterpret_cast<LPARAM>(&rbBand));

		LRESULT retVal = CDRF_DODEFAULT;

		if ((rbBand.fStyle & RBBS_USECHEVRON) == RBBS_USECHEVRON
			&& (rbBand.rcChevronLocation.right - rbBand.rcChevronLocation.left) > 0)
		{
			static const int roundness = DarkMode::isAtLeastWindows11() ? kWin11CornerRoundness + 1 : 0;

			const bool isHot = (rbBand.uChevronState & STATE_SYSTEM_HOTTRACKED) == STATE_SYSTEM_HOTTRACKED;
			const bool isPressed = (rbBand.uChevronState & STATE_SYSTEM_PRESSED) == STATE_SYSTEM_PRESSED;

			if (isHot)
			{
				DarkMode::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
			}
			else if (isPressed)
			{
				DarkMode::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
			}

			::SetTextColor(lpnmcd->hdc, isHot ? DarkMode::getTextColor() : DarkMode::getDarkerTextColor());
			::SetBkMode(lpnmcd->hdc, TRANSPARENT);

			static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOCLIP;
			::DrawText(lpnmcd->hdc, L"»", -1, &rbBand.rcChevronLocation, dtFlags);

			retVal = CDRF_SKIPDEFAULT;
		}
		return retVal;
	}

	[[nodiscard]] static LRESULT darkRebarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
		if (lpnmcd->dwDrawStage == CDDS_PREPAINT)
		{
			return DarkMode::prepaintRebar(lpnmcd);
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static LRESULT CALLBACK WindowNotifySubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowNotifySubclass, uIdSubclass);
				break;
			}

			case WM_NOTIFY:
			{
				if (!DarkMode::isEnabled())
				{
					break;
				}

				auto* lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
				if (lpnmhdr->code == NM_CUSTOMDRAW)
				{
					const std::wstring className = GetWndClassName(lpnmhdr->hwndFrom);

					if (className == TOOLBARCLASSNAME)
					{
						return DarkMode::darkToolbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == WC_LISTVIEW)
					{
						return DarkMode::darkListViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == WC_TREEVIEW)
					{
						return DarkMode::darkTreeViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == TRACKBAR_CLASS)
					{
						return DarkMode::darkTrackbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}

					if (className == REBARCLASSNAME)
					{
						return DarkMode::darkRebarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowNotifyCustomDrawSubclass(HWND hWnd, bool subclassChildren)
	{
		if (DarkMode::setSubclass(hWnd, WindowNotifySubclass, kWindowNotifySubclassID) == TRUE)
		{
			if (subclassChildren)
			{
				DarkMode::setChildCtrlsSubclassAndTheme(hWnd);
				if (DarkMode::isWindowsModeEnabled())
				{
					DarkMode::setWindowSettingChangeSubclass(hWnd);
				}
			}
		}
	}

	void removeWindowNotifyCustomDrawSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowNotifySubclass, kWindowNotifySubclassID);
	}

	static void drawUAHMenuNCBottomLine(HWND hWnd)
	{
		MENUBARINFO mbi{};
		mbi.cbSize = sizeof(MENUBARINFO);
		if (::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi) == FALSE)
		{
			return;
		}

		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);
		::MapWindowPoints(hWnd, nullptr, reinterpret_cast<POINT*>(&rcClient), 2);

		RECT rcWindow{};
		::GetWindowRect(hWnd, &rcWindow);

		::OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

		// the rcBar is offset by the window rect
		RECT rcAnnoyingLine{ rcClient };
		rcAnnoyingLine.bottom = rcAnnoyingLine.top;
		rcAnnoyingLine.top--;


		HDC hdc = ::GetWindowDC(hWnd);
		::FillRect(hdc, &rcAnnoyingLine, DarkMode::getDlgBackgroundBrush());
		::ReleaseDC(hWnd, hdc);
	}

	static LRESULT CALLBACK WindowMenuBarSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pMenuThemeData = reinterpret_cast<ThemeData*>(dwRefData);

		if (uMsg != WM_NCDESTROY && (!DarkMode::isEnabled() || !pMenuThemeData->ensureTheme(hWnd)))
		{
			return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}

		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowMenuBarSubclass, uIdSubclass);
				delete pMenuThemeData;
				break;
			}

			case WM_UAHDRAWMENU:
			{
				auto* pUDM = reinterpret_cast<UAHMENU*>(lParam);

				// get the menubar rect
				MENUBARINFO mbi{};
				mbi.cbSize = sizeof(MENUBARINFO);
				::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

				RECT rcWindow{};
				::GetWindowRect(hWnd, &rcWindow);

				// the rcBar is offset by the window rect
				RECT rcBar{ mbi.rcBar };
				::OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);

				rcBar.top -= 1;

				::FillRect(pUDM->hdc, &rcBar, DarkMode::getDlgBackgroundBrush());

				return 0;
			}

			case WM_UAHDRAWMENUITEM:
			{
				const auto& hTheme = pMenuThemeData->getHTheme();

				auto* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);

				// get the menu item string
				std::wstring buffer(MAX_PATH, L'\0');
				MENUITEMINFO mii{};
				mii.cbSize = sizeof(MENUITEMINFO);
				mii.fMask = MIIM_STRING;
				mii.dwTypeData = buffer.data();
				mii.cch = MAX_PATH - 1;

				::GetMenuItemInfo(pUDMI->um.hmenu, static_cast<UINT>(pUDMI->umi.iPosition), TRUE, &mii);

				// get the item state for drawing

				DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;

				int iTextStateID = MBI_NORMAL;
				int iBackgroundStateID = MBI_NORMAL;
				if ((pUDMI->dis.itemState & ODS_SELECTED) == ODS_SELECTED)
				{
					// clicked
					iTextStateID = MBI_PUSHED;
					iBackgroundStateID = MBI_PUSHED;
				}
				else if ((pUDMI->dis.itemState & ODS_HOTLIGHT) == ODS_HOTLIGHT)
				{
					// hot tracking
					iTextStateID = ((pUDMI->dis.itemState & ODS_INACTIVE) == ODS_INACTIVE) ? MBI_DISABLEDHOT : MBI_HOT;
					iBackgroundStateID = MBI_HOT;
				}
				else if (((pUDMI->dis.itemState & ODS_GRAYED) == ODS_GRAYED)
					|| ((pUDMI->dis.itemState & ODS_DISABLED) == ODS_DISABLED)
					|| ((pUDMI->dis.itemState & ODS_INACTIVE) == ODS_INACTIVE))
				{
					// disabled / grey text / inactive
					iTextStateID = MBI_DISABLED;
					iBackgroundStateID = MBI_DISABLED;
				}
				else if ((pUDMI->dis.itemState & ODS_DEFAULT) == ODS_DEFAULT)
				{
					// normal display
					iTextStateID = MBI_NORMAL;
					iBackgroundStateID = MBI_NORMAL;
				}

				if ((pUDMI->dis.itemState & ODS_NOACCEL) == ODS_NOACCEL)
				{
					dwFlags |= DT_HIDEPREFIX;
				}

				switch (iBackgroundStateID)
				{
					case MBI_NORMAL:
					case MBI_DISABLED:
					{
						::FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, DarkMode::getDlgBackgroundBrush());
						break;
					}

					case MBI_HOT:
					case MBI_DISABLEDHOT:
					{
						::FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, DarkMode::getHotBackgroundBrush());
						break;
					}

					case MBI_PUSHED:
					case MBI_DISABLEDPUSHED:
					{
						::FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, DarkMode::getCtrlBackgroundBrush());
						break;
					}

					default:
					{
						::DrawThemeBackground(hTheme, pUDMI->um.hdc, MENU_BARITEM, iBackgroundStateID, &pUDMI->dis.rcItem, nullptr);
						break;
					}
				}

				DTTOPTS dttopts{};
				dttopts.dwSize = sizeof(DTTOPTS);
				dttopts.dwFlags = DTT_TEXTCOLOR;
				switch (iTextStateID)
				{
					case MBI_NORMAL:
					case MBI_HOT:
					case MBI_PUSHED:
					{
						dttopts.crText = DarkMode::getTextColor();
						break;
					}

					case MBI_DISABLED:
					case MBI_DISABLEDHOT:
					case MBI_DISABLEDPUSHED:
					{
						dttopts.crText = DarkMode::getDisabledTextColor();
						break;
					}

					default:
					{
						break;
					}
				}

				::DrawThemeTextEx(hTheme, pUDMI->um.hdc, MENU_BARITEM, iTextStateID, buffer.c_str(), static_cast<int>(mii.cch), dwFlags, &pUDMI->dis.rcItem, &dttopts);

				return 0;
			}

			case WM_UAHMEASUREMENUITEM:
			{
				//auto pMMI = reinterpret_cast<UAHMEASUREMENUITEM*>(lParam);
				return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			}

			case WM_DPICHANGED:
			case WM_DPICHANGED_AFTERPARENT:
			case WM_THEMECHANGED:
			{
				pMenuThemeData->closeTheme();
				break;
			}

			case WM_NCACTIVATE:
			case WM_NCPAINT:
			{
				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				DarkMode::drawUAHMenuNCBottomLine(hWnd);
				return retVal;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowMenuBarSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, kWindowMenuBarSubclassID, VSCLASS_MENU);
	}

	void removeWindowMenuBarSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, kWindowMenuBarSubclassID);
	}

	static LRESULT CALLBACK WindowSettingChangeSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR /*dwRefData*/
	)
	{
		switch (uMsg)
		{
			case WM_NCDESTROY:
			{
				::RemoveWindowSubclass(hWnd, WindowSettingChangeSubclass, uIdSubclass);
				break;
			}

			case WM_SETTINGCHANGE:
			{
				if (DarkMode::handleSettingChange(lParam))
				{
					DarkMode::setDarkTitleBarEx(hWnd, true);
					DarkMode::setChildCtrlsTheme(hWnd);
					::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
				}
				break;
			}

			default:
			{
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowSettingChangeSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowSettingChangeSubclass, kWindowSettingChangeSubclassID);
	}

	void removeWindowSettingChangeSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowSettingChangeSubclass, kWindowSettingChangeSubclassID);
	}

	/**
	 * @brief Configures the SysLink control to be affected by `WM_CTLCOLORSTATIC` message.
	 *
	 * Configures the first hyperlink item (index 0)
	 * to either use default system link colors if in classic mode, 
	 * or to be affected by `WM_CTLCOLORSTATIC` message from its parent.
	 *
	 * @param hWnd Handle to the SysLink control.
	 *
	 * @note Currently affects only the first link (index 0).
	 */
	void enableSysLinkCtrlCtlColor(HWND hWnd)
	{
		LITEM item{};
		item.iLink = 0; // for now colorize only 1st item
		item.mask = LIF_ITEMINDEX | LIF_STATE;
		item.state = DarkMode::isEnabled() ? LIS_DEFAULTCOLORS : 0;
		item.stateMask = LIS_DEFAULTCOLORS;
		::SendMessage(hWnd, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
	}

	/**
	 * @brief Sets dark title bar and optional Windows 11 features.
	 *
	 * For Windows 10 (2004+) and newer, this function configures the dark title bar using
	 * `DWMWA_USE_IMMERSIVE_DARK_MODE`. On Windows 11, if `useWin11Features` is `true`, it
	 * additionally applies:
	 * - Rounded corners (`DWMWA_WINDOW_CORNER_PREFERENCE`)
	 * - Border color (`DWMWA_BORDER_COLOR`)
	 * - Mica backdrop (`DWMWA_SYSTEMBACKDROP_TYPE`) if allowed and compatible
	 *
	 * If `_DARKMODELIB_ALLOW_OLD_OS` is defined and running on pre-2004 builds,
	 * fallback behavior will enable dark title bars via undocumented APIs.
	 *
	 * @param hWnd Handle to the top-level window.
	 * @param useWin11Features `true` to enable Windows 11 specific features such as Mica and rounded corners.
	 *
	 * @note Requires Windows 10 version 2004 (build 19041) or later.
	 * @see DwmSetWindowAttribute
	 * @see DwmExtendFrameIntoClientArea
	 */
	void setDarkTitleBarEx(HWND hWnd, bool useWin11Features)
	{
		static constexpr DWORD win10Build2004 = 19041;
		static constexpr DWORD win11Mica = 22621;
		if (DarkMode::getWindowsBuildNumber() >= win10Build2004)
		{
			const BOOL useDark = DarkMode::isExperimentalActive() ? TRUE : FALSE;
			::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

			if (useWin11Features && DarkMode::isAtLeastWindows11())
			{
				::DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &g_dmCfg._roundCorner, sizeof(g_dmCfg._roundCorner));
				::DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &g_dmCfg._borderColor, sizeof(g_dmCfg._borderColor));

				if (DarkMode::getWindowsBuildNumber() >= win11Mica)
				{
					if (g_dmCfg._micaExtend && g_dmCfg._mica != DWMSBT_AUTO && !DarkMode::isWindowsModeEnabled() && (g_dmCfg._dmType == DarkModeType::dark))
					{
						static constexpr MARGINS margins{ -1, 0, 0, 0 };
						::DwmExtendFrameIntoClientArea(hWnd, &margins);
					}

					::DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &g_dmCfg._mica, sizeof(g_dmCfg._mica));
				}
			}
		}
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
		else
		{
			DarkMode::allowDarkModeForWindow(hWnd, DarkMode::isExperimentalActive());
			DarkMode::setTitleBarThemeColor(hWnd);
		}
#endif
	}

	/**
	 * @brief Sets dark mode title bar on supported Windows versions.
	 *
	 * Delegates to @ref setDarkTitleBarEx with `useWin11Features = false`.
	 *
	 * @param hWnd Handle to the top-level window.
	 *
	 * @see DarkMode::setDarkTitleBarEx()
	 */
	void setDarkTitleBar(HWND hWnd)
	{
		DarkMode::setDarkTitleBarEx(hWnd, false);
	}

	/**
	 * @brief Applies an experimental visual style to the specified window, if supported.
	 *
	 * When experimental features are supported and active,
	 * this function enables dark experimental visual style on the window.
	 *
	 * @param hWnd Handle to the target window or control.
	 * @param themeClassName Name of the theme class to apply (e.g. L"Explorer", "ItemsView").
	 *
	 * @note This function is a no-op if experimental theming is not supported on the current OS.
	 *
	 * @see DarkMode::isExperimentalSupported()
	 * @see DarkMode::isExperimentalActive()
	 * @see DarkMode::allowDarkModeForWindow()
	 */
	void setDarkThemeExperimental(HWND hWnd, const wchar_t* themeClassName)
	{
		if (DarkMode::isExperimentalSupported())
		{
			DarkMode::allowDarkModeForWindow(hWnd, DarkMode::isExperimentalActive());
			::SetWindowTheme(hWnd, themeClassName, nullptr);
		}
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style if experimental mode is active.
	 *
	 * Useful for controls like list views or tree views to use dark scroll bars
	 * and explorer style theme in supported environments.
	 *
	 * @param hWnd Handle to the control or window to theme.
	 */
	void setDarkExplorerTheme(HWND hWnd)
	{
		::SetWindowTheme(hWnd, DarkMode::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr, nullptr);
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style to scroll bars.
	 *
	 * Convenience wrapper that calls @ref setDarkExplorerTheme to apply dark scroll bar
	 * for compatible controls (e.g. list views, tree views).
	 *
	 * @param hWnd Handle to the control with scroll bars.
	 *
	 * @see DarkMode::setDarkExplorerTheme()
	 */
	void setDarkScrollBar(HWND hWnd)
	{
		DarkMode::setDarkExplorerTheme(hWnd);
	}

	/**
	 * @brief Applies "DarkMode_Explorer" visual style to tooltip controls based on context.
	 *
	 * Selects the appropriate `GETTOOLTIPS` message depending on the control type
	 * (e.g. toolbar, list view, tree view, tab bar) to retrieve the tooltip handle.
	 * If `ToolTipsType::tooltip` is specified, applies theming directly to `hWnd`.
	 *
	 * Internally calls @ref setDarkExplorerTheme to set dark tooltip.
	 *
	 * @param hWnd Handle to the parent control or tooltip.
	 * @param type The tooltip context type (toolbar, list view, etc.).
	 *
	 * @see DarkMode::setDarkExplorerTheme()
	 * @see ToolTipsType
	 */
	void setDarkTooltips(HWND hWnd, ToolTipsType type)
	{
		UINT msg = 0;
		switch (type)
		{
			case DarkMode::ToolTipsType::toolbar:
			{
				msg = TB_GETTOOLTIPS;
				break;
			}

			case DarkMode::ToolTipsType::listview:
			{
				msg = LVM_GETTOOLTIPS;
				break;
			}

			case DarkMode::ToolTipsType::treeview:
			{
				msg = TVM_GETTOOLTIPS;
				break;
			}

			case DarkMode::ToolTipsType::tabbar:
			{
				msg = TCM_GETTOOLTIPS;
				break;
			}

			case DarkMode::ToolTipsType::trackbar:
			{
				msg = TBM_GETTOOLTIPS;
				break;
			}

			case DarkMode::ToolTipsType::tooltip:
			{
				msg = 0;
				break;
			}
		}

		if (msg == 0)
		{
			DarkMode::setDarkExplorerTheme(hWnd);
		}
		else
		{
			auto hTips = reinterpret_cast<HWND>(::SendMessage(hWnd, msg, 0, 0));
			if (hTips != nullptr)
			{
				DarkMode::setDarkExplorerTheme(hTips);
			}
		}
	}

	/**
	 * @brief Sets the color of line above a toolbar control for non-classic mode.
	 *
	 * Sends `TB_SETCOLORSCHEME` to customize the line drawn above the toolbar.
	 * When non-classic mode is enabled, sets both `clrBtnHighlight` and `clrBtnShadow`
	 * to the dialog background color, otherwise uses system defaults.
	 *
	 * @param hWnd Handle to the toolbar control.
	 */
	void setDarkLineAbovePanelToolbar(HWND hWnd)
	{
		COLORSCHEME scheme{};
		scheme.dwSize = sizeof(COLORSCHEME);

		if (DarkMode::isEnabled())
		{
			scheme.clrBtnHighlight = DarkMode::getDlgBackgroundColor();
			scheme.clrBtnShadow = DarkMode::getDlgBackgroundColor();
		}
		else
		{
			scheme.clrBtnHighlight = CLR_DEFAULT;
			scheme.clrBtnShadow = CLR_DEFAULT;
		}

		::SendMessage(hWnd, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
	}

	/**
	 * @brief Applies an experimental Explorer visual style to a list view.
	 *
	 * Uses @ref setDarkThemeExperimental with the `"Explorer"` theme class to adapt
	 * list view visuals (e.g. scroll bars, selection color) for dark mode, if supported.
	 *
	 * @param hWnd Handle to the list view control.
	 *
	 * @see DarkMode::setDarkThemeExperimental()
	 */
	void setDarkListView(HWND hWnd)
	{
		DarkMode::setDarkThemeExperimental(hWnd, L"Explorer");
	}

	/**
	 * @brief Replaces default list view checkboxes with themed dark-mode versions on Windows 11.
	 *
	 * If the list view uses `LVS_EX_CHECKBOXES` and is running on Windows 11 or later,
	 * this function manually renders the unchecked and checked checkbox visuals using
	 * themed drawing APIs, then inserts the resulting icons into the state image list.
	 *
	 * Uses `"DarkMode_Explorer::Button"` as the theme class if experimental dark mode is active;
	 * otherwise falls back to `VSCLASS_BUTTON`.
	 *
	 * @param hWnd Handle to the list view control with extended checkbox style.
	 *
	 * @note Does nothing on pre-Windows 11 systems or if checkboxes are not enabled.
	 */
	void setDarkListViewCheckboxes(HWND hWnd)
	{
		if (!DarkMode::isAtLeastWindows11())
		{
			return;
		}

		const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
		if ((lvExStyle & LVS_EX_CHECKBOXES) != LVS_EX_CHECKBOXES)
		{
			return;
		}

		HDC hdc = ::GetDC(nullptr);

		const bool useDark = DarkMode::isExperimentalActive() && DarkMode::isThemeDark();
		HTHEME hTheme = ::OpenThemeData(nullptr, useDark ? L"DarkMode_Explorer::Button" : VSCLASS_BUTTON);

		SIZE szBox{};
		::GetThemePartSize(hTheme, hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_DRAW, &szBox);

		const RECT rcBox{ 0, 0, szBox.cx, szBox.cy };

		auto hImgList = ListView_GetImageList(hWnd, LVSIL_STATE);
		if (hImgList == nullptr)
		{
			::CloseThemeData(hTheme);
			::ReleaseDC(nullptr, hdc);
			return;
		}
		::ImageList_RemoveAll(hImgList);

		HDC hBoxDC = ::CreateCompatibleDC(hdc);
		HBITMAP hBoxBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);
		HBITMAP hMaskBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);

		auto holdBmp = static_cast<HBITMAP>(::SelectObject(hBoxDC, hBoxBmp));
		::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &rcBox, nullptr);

		ICONINFO ii{};
		ii.fIcon = TRUE;
		ii.hbmColor = hBoxBmp;
		ii.hbmMask = hMaskBmp;

		HICON hIcon = ::CreateIconIndirect(&ii);
		if (hIcon != nullptr)
		{
			::ImageList_AddIcon(hImgList, hIcon);
			::DestroyIcon(hIcon);
			hIcon = nullptr;
		}

		::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, CBS_CHECKEDNORMAL, &rcBox, nullptr);
		ii.hbmColor = hBoxBmp;

		hIcon = ::CreateIconIndirect(&ii);
		if (hIcon != nullptr)
		{
			::ImageList_AddIcon(hImgList, hIcon);
			::DestroyIcon(hIcon);
			hIcon = nullptr;
		}

		::SelectObject(hBoxDC, holdBmp);
		::DeleteObject(hMaskBmp);
		::DeleteObject(hBoxBmp);
		::DeleteDC(hBoxDC);
		::CloseThemeData(hTheme);
		::ReleaseDC(nullptr, hdc);
	}

	/**
	 * @brief Sets colors and edges for a RichEdit control.
	 *
	 * Determines if the control has `WS_BORDER` or `WS_EX_STATICEDGE`, and sets the background
	 * accordingly: uses control background color when edged, otherwise dialog background.
	 *
	 * In dark mode:
	 * - Sets background color via `EM_SETBKGNDCOLOR`
	 * - Updates default text color via `EM_SETCHARFORMAT`
	 * - Applies themed scroll bars using `DarkMode_Explorer::ScrollBar`
	 *
	 * When not in dark mode, restores default visual styles and coloring.
	 * Also conditionally swaps `WS_BORDER` and `WS_EX_STATICEDGE`.
	 *
	 * @param hWnd Handle to the RichEdit control.
	 *
	 * @see DarkMode::setWindowStyle()
	 * @see DarkMode::setWindowExStyle()
	 */
	void setDarkRichEdit(HWND hWnd)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasBorder = (nStyle & WS_BORDER) == WS_BORDER;

		const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		const bool hasStaticEdge = (nExStyle & WS_EX_STATICEDGE) == WS_EX_STATICEDGE;

		if (DarkMode::isEnabled())
		{
			const COLORREF clrBg = (hasStaticEdge || hasBorder ? DarkMode::getCtrlBackgroundColor() : DarkMode::getDlgBackgroundColor());
			::SendMessage(hWnd, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(clrBg));

			CHARFORMATW cf{};
			cf.cbSize = sizeof(CHARFORMATW);
			cf.dwMask = CFM_COLOR;
			cf.crTextColor = DarkMode::getTextColor();
			::SendMessage(hWnd, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

			::SetWindowTheme(hWnd, nullptr, L"DarkMode_Explorer::ScrollBar");
		}
		else
		{
			::SendMessage(hWnd, EM_SETBKGNDCOLOR, TRUE, 0);
			::SendMessage(hWnd, EM_SETCHARFORMAT, 0, 0);

			::SetWindowTheme(hWnd, nullptr, nullptr);
		}

		DarkMode::setWindowStyle(hWnd, DarkMode::isEnabled() && hasStaticEdge, WS_BORDER);
		DarkMode::setWindowExStyle(hWnd, !DarkMode::isEnabled() && hasBorder, WS_EX_STATICEDGE);
	}

	/**
	 * @brief Applies visual styles; ctl color message and child controls subclassings to a dialog safely.
	 *
	 * Ensures the specified window is not `nullptr` and then:
	 * - Enables the dark title bar
	 * - Subclasses the dialog for control ctl coloring
	 * - Applies theming and subclassing to child controls
	 *
	 * Should not be used in combination with @ref setDarkDlgNotifySafe
	 * to avoid overlapping styling logic.
	 * 
	 * @param hWnd Handle to the dialog window. No action taken if `nullptr`.
	 * @param useWin11Features `true` to enable Windows 11 specific styling like Mica or rounded corners.
	 *
	 * @see DarkMode::setDarkDlgNotifySafe()
	 * @see DarkMode::setDarkTitleBarEx()
	 * @see DarkMode::setWindowCtlColorSubclass()
	 * @see DarkMode::setChildCtrlsSubclassAndTheme()
	 */
	void setDarkDlgSafe(HWND hWnd, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		DarkMode::setDarkTitleBarEx(hWnd, useWin11Features);
		DarkMode::setWindowCtlColorSubclass(hWnd);
		DarkMode::setChildCtrlsSubclassAndTheme(hWnd);
	}

	/**
	 * @brief Applies visual styles; ctl color message, child controls, and custom drawing subclassings to a dialog safely.
	 *
	 * Ensures the specified window is not `nullptr` and then:
	 * - Enables the dark title bar
	 * - Subclasses the dialog for control coloring
	 * - Applies theming and subclassing to child controls
	 * - Enables custom draw-based theming via notification subclassing
	 *
	 * Should not be used in combination with @ref DarkMode::setDarkDlgSafe
	 * to avoid overlapping styling logic.
	 * 
	 * @param hWnd Handle to the dialog window. No action taken if `nullptr`.
	 * @param useWin11Features `true` to enable Windows 11 specific styling like Mica or rounded corners.
	 * 
	 * @see DarkMode::setDarkDlgSafe()
	 * @see DarkMode::setDarkTitleBarEx()
	 * @see DarkMode::setWindowCtlColorSubclass()
	 * @see DarkMode::setChildCtrlsSubclassAndTheme()
	 */
	void setDarkDlgNotifySafe(HWND hWnd, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		DarkMode::setDarkTitleBarEx(hWnd, useWin11Features);
		DarkMode::setWindowCtlColorSubclass(hWnd);
		DarkMode::setWindowNotifyCustomDrawSubclass(hWnd, true);
	}

	/**
	 * @brief Enables or disables theme-based dialog background textures in classic mode.
	 *
	 * Applies `ETDT_ENABLETAB` only when `theme` is `true` and the current mode is classic.
	 * This replaces the default classic gray background with a lighter themed texture.
	 * Otherwise disables themed dialog textures with `ETDT_DISABLE`.
	 *
	 * @param hWnd Handle to the target dialog window.
	 * @param theme `true` to enable themed tab textures in classic mode.
	 *
	 * @see EnableThemeDialogTexture
	 */
	void enableThemeDialogTexture(HWND hWnd, bool theme)
	{
		::EnableThemeDialogTexture(hWnd, theme && (g_dmCfg._dmType == DarkModeType::classic) ? ETDT_ENABLETAB : ETDT_DISABLE);
	}

	/**
	 * @brief Enables or disables visual styles for a window.
	 *
	 * Applies `SetWindowTheme(hWnd, L"", L"")` when `doDisable` is `true`, effectively removing
	 * the current theme. Restores default theming when `doDisable` is `false`.
	 *
	 * @param hWnd Handle to the window.
	 * @param doDisable `true` to strip visual styles, `false` to re-enable them.
	 *
	 * @see SetWindowTheme
	 */
	void disableVisualStyle(HWND hWnd, bool doDisable)
	{
		if (doDisable)
		{
			::SetWindowTheme(hWnd, L"", L"");
		}
		else
		{
			::SetWindowTheme(hWnd, nullptr, nullptr);
		}
	}

	/**
	 * @brief Calculates perceptual lightness of a COLORREF color.
	 *
	 * Converts the RGB color to linear space and calculates perceived lightness.
	 *
	 * @param clr COLORREF in 0xBBGGRR format.
	 * @return Lightness value as a double.
	 *
	 * @note Based on: https://stackoverflow.com/a/56678483
	 */
	double calculatePerceivedLightness(COLORREF clr)
	{
		auto linearValue = [](double colorChannel) -> double {
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

	/**
	 * @brief Retrieves the current TreeView style configuration.
	 *
	 * @return Reference to the current `TreeViewStyle`.
	 */
	const TreeViewStyle& getTreeViewStyle()
	{
		return g_dmCfg._tvStyle;
	}

	/// Set TreeView style
	static void setTreeViewStyle(TreeViewStyle tvStyle)
	{
		g_dmCfg._tvStyle = tvStyle;
	}

	/**
	 * @brief Determines appropriate TreeView style based on background perceived lightness.
	 *
	 * Checks the perceived lightness of the current view background and
	 * selects a corresponding style: dark, light, or classic. Style selection
	 * is based on how far the lightness deviates from the middle gray threshold range
	 * around the midpoint value (50.0).
	 *
	 * @see DarkMode::calculatePerceivedLightness()
	 */
	void calculateTreeViewStyle()
	{
		static constexpr double middle = 50.0;
		const COLORREF bgColor = DarkMode::getViewBackgroundColor();

		if (g_dmCfg._tvBackground != bgColor || g_dmCfg._lightness == middle)
		{
			g_dmCfg._lightness = DarkMode::calculatePerceivedLightness(bgColor);
			g_dmCfg._tvBackground = bgColor;
		}

		if (g_dmCfg._lightness < (middle - kMiddleGrayRange))
		{
			DarkMode::setTreeViewStyle(TreeViewStyle::dark);
		}
		else if (g_dmCfg._lightness > (middle + kMiddleGrayRange))
		{
			DarkMode::setTreeViewStyle(TreeViewStyle::light);
		}
		else
		{
			DarkMode::setTreeViewStyle(TreeViewStyle::classic);
		}
	}

	/**
	 * @brief Applies the appropriate window theme style to the specified TreeView.
	 *
	 * Updates the TreeView's visual behavior and theme based on the currently selected
	 * style @ref getTreeViewStyle(). It conditionally adjusts the `TVS_TRACKSELECT`
	 * style flag and applies a matching visual theme using `SetWindowTheme()`.
	 *
	 * If `force` is `true`, the style is applied regardless of previous state.
	 * Otherwise, the update occurs only if the style has changed since the last update.
	 *
	 * - `light`: Enables `TVS_TRACKSELECT`, applies "Explorer" theme.
	 * - `dark`: If supported, enables `TVS_TRACKSELECT`, applies "DarkMode_Explorer" theme.
	 * - `classic`: Disables `TVS_TRACKSELECT`, clears the theme.
	 *
	 * @param hWnd Handle to the TreeView control.
	 * @param force Whether to forcibly reapply the style even if unchanged.
	 *
	 * @see TreeViewStyle
	 * @see DarkMode::getTreeViewStyle()
	 * @see DarkMode::getPrevTreeViewStyle()
	 */
	void setTreeViewWindowTheme(HWND hWnd, bool force)
	{
		if (force || DarkMode::getPrevTreeViewStyle() != DarkMode::getTreeViewStyle())
		{
			auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			const bool hasHotStyle = (nStyle & TVS_TRACKSELECT) == TVS_TRACKSELECT;
			bool change = false;
			std::wstring strSubAppName;

			switch (DarkMode::getTreeViewStyle())
			{
				case TreeViewStyle::light:
				{
					if (!hasHotStyle)
					{
						nStyle |= TVS_TRACKSELECT;
						change = true;
					}
					strSubAppName = L"Explorer";
					break;
				}

				case TreeViewStyle::dark:
				{
					if (DarkMode::isExperimentalSupported())
					{
						if (!hasHotStyle)
						{
							nStyle |= TVS_TRACKSELECT;
							change = true;
						}
						strSubAppName = L"DarkMode_Explorer";
						break;
					}
					[[fallthrough]];
				}

				case TreeViewStyle::classic:
				{
					if (hasHotStyle)
					{
						nStyle &= ~TVS_TRACKSELECT;
						change = true;
					}
					strSubAppName = L"";
					break;
				}
			}

			if (change)
			{
				::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle);
			}

			::SetWindowTheme(hWnd, strSubAppName.empty() ? nullptr : strSubAppName.c_str(), nullptr);
		}
	}

	/**
	 * @brief Retrieves the previous TreeView style configuration.
	 *
	 * @return Reference to the previous `TreeViewStyle`.
	 */
	const TreeViewStyle& getPrevTreeViewStyle()
	{
		return g_dmCfg._tvStylePrev;
	}

	/**
	 * @brief Stores the current TreeView style as the previous style for later comparison.
	 */
	void setPrevTreeViewStyle()
	{
		g_dmCfg._tvStylePrev = DarkMode::getTreeViewStyle();
	}

	/**
	 * @brief Checks whether the current theme is dark.
	 *
	 * Internally it use TreeView style to determine if dark theme is used.
	 *
	 * @return `true` if the active style is `TreeViewStyle::dark`, otherwise `false`.
	 *
	 * @see DarkMode::getTreeViewStyle()
	 */
	bool isThemeDark()
	{
		return DarkMode::getTreeViewStyle() == TreeViewStyle::dark;
	}

	/**
	 * @brief Checks whether the color is dark.
	 *
	 * @param clr Color to check.
	 * 
	 * @return `true` if the perceived lightness of the color
	 *         is less than (50.0 - kMiddleGrayRange), otherwise `false`.
	 *
	 * @see DarkMode::calculatePerceivedLightness()
	 */
	bool isColorDark(COLORREF clr)
	{
		static constexpr double middle = 50.0;
		return DarkMode::calculatePerceivedLightness(clr) < (middle - kMiddleGrayRange);
	}

	/**
	 * @brief Forces a window to redraw its non-client frame.
	 *
	 * Triggers a non-client area update by using `SWP_FRAMECHANGED` without changing
	 * size, position, or Z-order.
	 *
	 * @param hWnd Handle to the target window.
	 */
	void redrawWindowFrame(HWND hWnd)
	{
		::SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

	/**
	 * @brief Sets or clears a specific window style or extended style.
	 *
	 * Checks if the specified `dwFlag` is already set and toggles it if needed.
	 * Only valid for `GWL_STYLE` or `GWL_EXSTYLE`.
	 *
	 * @param hWnd Handle to the window.
	 * @param setFlag `true` to set the flag, `false` to clear it.
	 * @param dwFlag Style bitmask to apply.
	 * @param gwlIdx Either `GWL_STYLE` or `GWL_EXSTYLE`.
	 * @return `TRUE` if modified, `FALSE` if unchanged, `-1` if invalid index.
	 */
	static int setWindowLongPtrStyle(HWND hWnd, bool setFlag, LONG_PTR dwFlag, int gwlIdx)
	{
		if ((gwlIdx != GWL_STYLE) && (gwlIdx != GWL_EXSTYLE))
		{
			return -1;
		}

		auto nStyle = ::GetWindowLongPtr(hWnd, gwlIdx);
		const bool hasFlag = (nStyle & dwFlag) == dwFlag;

		if (setFlag != hasFlag)
		{
			nStyle ^= dwFlag;
			::SetWindowLongPtr(hWnd, gwlIdx, nStyle);
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * @brief Sets a window's standard style flags and redraws window if needed.
	 *
	 * Wraps @ref DarkMode::setWindowLongPtrStyle with `GWL_STYLE`
	 * and calls @ref DarkMode::redrawWindowFrame if a change occurs.
	 *
	 * @param hWnd Handle to the target window.
	 * @param setStyle `true` to set the flag, `false` to remove it.
	 * @param styleFlag Style bit to modify.
	 */
	void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag)
	{
		if (DarkMode::setWindowLongPtrStyle(hWnd, setStyle, styleFlag, GWL_STYLE) == TRUE)
		{
			DarkMode::redrawWindowFrame(hWnd);
		}
	}

	/**
	 * @brief Sets a window's extended style flags and redraws window if needed.
	 *
	 * Wraps @ref DarkMode::setWindowLongPtrStyle with `GWL_EXSTYLE`
	 * and calls @ref DarkMode::redrawWindowFrame if a change occurs.
	 *
	 * @param hWnd Handle to the target window.
	 * @param setExStyle `true` to set the flag, `false` to remove it.
	 * @param exStyleFlag Extended style bit to modify.
	 */
	void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag)
	{
		if (DarkMode::setWindowLongPtrStyle(hWnd, setExStyle, exStyleFlag, GWL_EXSTYLE) == TRUE)
		{
			DarkMode::redrawWindowFrame(hWnd);
		}
	}

	/**
	 * @brief Replaces an extended edge (e.g. client edge) with a standard window border.
	 *
	 * The given `exStyleFlag` must be a valid edge-related extended window style:
	 * - `WS_EX_CLIENTEDGE`
	 * - `WS_EX_DLGMODALFRAME`
	 * - `WS_EX_STATICEDGE`
	 * - `WS_EX_WINDOWEDGE`
	 * ...or any combination of these.
	 *
	 * If `replace` is `true`, the specified extended edge style(s) are removed and
	 * `WS_BORDER` is applied. If `false`, the edge style(s) are restored and `WS_BORDER` is cleared.
	 *
	 * @param hWnd Handle to the target window.
	 * @param replace `true` to apply standard border; `false` to restore extended edge(s).
	 * @param exStyleFlag One or more valid edge-related extended styles.
	 *
	 * @see DarkMode::setWindowExStyle()
	 * @see DarkMode::setWindowStyle()
	 */
	void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag)
	{
		DarkMode::setWindowExStyle(hWnd, !replace, exStyleFlag);
		DarkMode::setWindowStyle(hWnd, replace, WS_BORDER);
	}

	/**
	 * @brief Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER` based on dark mode state.
	 *
	 * If dark mode is enabled, removes `WS_EX_CLIENTEDGE` and applies `WS_BORDER`.
	 * Otherwise restores the extended edge style.
	 *
	 * @param hWnd Handle to the target window. No action is taken if `hWnd` is `nullptr`.
	 *
	 * @see DarkMode::replaceExEdgeWithBorder()
	 */
	void replaceClientEdgeWithBorderSafe(HWND hWnd)
	{
		if (hWnd != nullptr)
		{
			DarkMode::replaceExEdgeWithBorder(hWnd, DarkMode::isEnabled(), WS_EX_CLIENTEDGE);
		}
	}

	/**
	 * @brief Applies classic-themed styling to a progress bar in non-classic mode.
	 *
	 * When dark mode is enabled, applies `WS_DLGFRAME`, removes visual styles
	 * to allow to set custom background and fill colors using:
	 * - Background: `DarkMode::getBackgroundColor()`
	 * - Fill: Hardcoded green `0x06B025` via `PBM_SETBARCOLOR`
	 *
	 * Typically used for marquee style progress bar.
	 * 
	 * @param hWnd Handle to the progress bar control.
	 *
	 * @see DarkMode::setWindowStyle()
	 * @see DarkMode::disableVisualStyle()
	 */
	void setProgressBarClassicTheme(HWND hWnd)
	{
		DarkMode::setWindowStyle(hWnd, DarkMode::isEnabled(), WS_DLGFRAME);
		DarkMode::disableVisualStyle(hWnd, DarkMode::isEnabled());
		if (DarkMode::isEnabled())
		{
			::SendMessage(hWnd, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(DarkMode::getCtrlBackgroundColor()));
			static constexpr COLORREF greenLight = HEXRGB(0x06B025);
			static constexpr COLORREF greenDark = HEXRGB(0x0F7B0F);
			::SendMessage(hWnd, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(DarkMode::isExperimentalActive() ? greenDark : greenLight));
		}
	}

	/**
	 * @brief Handles text and background colorizing for read-only controls.
	 *
	 * Sets the text color and background color on the provided HDC.
	 * Returns the corresponding background brush for painting.
	 * Typically used for read-only controls (e.g. edit control and combo box' list box).
	 * Typically used in response to `WM_CTLCOLORSTATIC` or in `WM_CTLCOLORLISTBOX`
	 * via @ref DarkMode::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context (HDC) receiving the drawing instructions.
	 * @return Background brush to use for painting, or `FALSE` (0) if classic mode is enabled
	 *         and `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 * @see DarkMode::onCtlColorListbox()
	 */
	LRESULT onCtlColor(HDC hdc)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			return FALSE;
		}
#endif
		::SetTextColor(hdc, DarkMode::getTextColor());
		::SetBkColor(hdc, DarkMode::getBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for interactive controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLOREDIT` and `WM_CTLCOLORLISTBOX`
	 * via @ref DarkMode::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 * @see DarkMode::onCtlColorListbox()
	 */
	LRESULT onCtlColorCtrl(HDC hdc)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, DarkMode::getTextColor());
		::SetBkColor(hdc, DarkMode::getCtrlBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getCtrlBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for window and disabled non-text controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
	 * and `WM_CTLCOLORLISTBOX` via @ref DarkMode::onCtlColorListbox
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 * @see DarkMode::onCtlColorListbox()
	 */
	LRESULT onCtlColorDlg(HDC hdc)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, DarkMode::getTextColor());
		::SetBkColor(hdc, DarkMode::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for error state (for specific usage).
	 *
	 * Sets the text and background colors on the provided HDC.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorError(HDC hdc)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			return FALSE;
		}
#endif

		::SetTextColor(hdc, DarkMode::getTextColor());
		::SetBkColor(hdc, DarkMode::getErrorBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getErrorBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for static text controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Colors depend on if control is enabled.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORSTATIC`.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
			return FALSE;
		}
#endif
		::SetTextColor(hdc, isTextEnabled ? DarkMode::getTextColor() : DarkMode::getDisabledTextColor());
		::SetBkColor(hdc, DarkMode::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for syslink controls.
	 *
	 * Sets the text and background colors on the provided HDC.
	 * Colors depend on if control is enabled.
	 * Returns the corresponding brush used to paint the background.
	 * Typically used in response to `WM_CTLCOLORSTATIC`.
	 *
	 * @param hdc Handle to the device context for the target control.
	 * @return The background brush, or `FALSE` if dark mode is disabled and
	 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 */
	LRESULT onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled)
	{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS)
		if (!DarkMode::_isEnabled())
		{
			::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_HOTLIGHT : COLOR_GRAYTEXT));
			return FALSE;
		}
#endif
		::SetTextColor(hdc, isTextEnabled ? DarkMode::getLinkTextColor() : DarkMode::getDisabledTextColor());
		::SetBkColor(hdc, DarkMode::getDlgBackgroundColor());
		return reinterpret_cast<LRESULT>(DarkMode::getDlgBackgroundBrush());
	}

	/**
	 * @brief Handles text and background colorizing for list box controls.
	 *
	 * Inspects the list box style flags to detect if it's part of a combo box (via `LBS_COMBOBOX`)
	 * and whether experimental feature is active. Based on the context, delegates to:
	 * - @ref DarkMode::onCtlColorCtrl for standard enabled listboxes
	 * - @ref DarkMode::onCtlColorDlg for disabled ones or when dark mode is disabled
	 * - @ref DarkMode::onCtlColor for combo box' listbox
	 *
	 * @param wParam WPARAM from `WM_CTLCOLORLISTBOX`, representing the HDC.
	 * @param lParam LPARAM from `WM_CTLCOLORLISTBOX`, representing the HWND of the listbox.
	 * @return The brush handle as LRESULT for background painting, or `FALSE` if not themed.
	 *
	 * @see DarkMode::WindowCtlColorSubclass()
	 * @see DarkMode::onCtlColor()
	 * @see DarkMode::onCtlColorCtrl()
	 * @see DarkMode::onCtlColorDlg()
	 */
	LRESULT onCtlColorListbox(WPARAM wParam, LPARAM lParam)
	{
		auto hdc = reinterpret_cast<HDC>(wParam);
		auto hWnd = reinterpret_cast<HWND>(lParam);

		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool isComboBox = (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;
		if ((!isComboBox || !DarkMode::isExperimentalActive()))
		{
			if (::IsWindowEnabled(hWnd) == TRUE)
			{
				return DarkMode::onCtlColorCtrl(hdc);
			}
			return DarkMode::onCtlColorDlg(hdc);
		}
		return DarkMode::onCtlColor(hdc);
	}

	/**
	 * @brief Hook procedure for customizing common dialogs with dark mode.
	 */
	UINT_PTR CALLBACK HookDlgProc(HWND hWnd, UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/)
	{
		if (uMsg == WM_INITDIALOG)
		{
			DarkMode::setDarkDlgSafe(hWnd);
			return TRUE;
		}
		return FALSE;
	}
} // namespace DarkMode

#endif // !defined(_DARKMODELIB_NOT_USED)
