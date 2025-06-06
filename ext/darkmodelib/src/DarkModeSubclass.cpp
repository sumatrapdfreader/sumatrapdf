// Copyright (C)2024-2025 ozone10

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.


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
#include <shlwapi.h>
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

#if defined(_MSC_VER)
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdi32.lib")
#elif defined(__GNUC__)
#include <cstdint>
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
constexpr int CP_DROPDOWNITEM = 9; // for some reason mingw use only enum up to 8
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

static constexpr COLORREF HEXRGB(DWORD rrggbb)
{
	// from 0xRRGGBB like natural #RRGGBB
	// to the little-endian 0xBBGGRR
	return
		((rrggbb & 0xFF0000) >> 16) |
		((rrggbb & 0x00FF00)) |
		((rrggbb & 0x0000FF) << 16);
}

static std::wstring getWndClassName(HWND hWnd)
{
	constexpr int strLen = 32;
	std::wstring className(strLen, L'\0');
	className.resize(static_cast<size_t>(::GetClassNameW(hWnd, className.data(), strLen)));
	return className;
}

static bool cmpWndClassName(HWND hWnd, const wchar_t* classNameToCmp)
{
	return (getWndClassName(hWnd) == classNameToCmp);
}

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
static std::wstring getIniPath(const std::wstring& iniFilename)
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

static bool fileExists(const std::wstring& filePath)
{
	const DWORD dwAttrib = ::GetFileAttributesW(filePath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY));
}

static bool setClrFromIni(const std::wstring& sectionName, const std::wstring& keyName, const std::wstring& iniFilePath, COLORREF* clr)
{
	constexpr size_t maxStrLen = 6;
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
		*clr = HEXRGB(std::stoul(buffer, nullptr, 16));
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
	int getLibInfo(LibInfo libInfoType)
	{
		switch (libInfoType)
		{
			case LibInfo::maxValue:
			case LibInfo::featureCheck:
				return static_cast<int>(LibInfo::maxValue);

			case LibInfo::verMajor:
				return DM_VERSION_MAJOR;

			case LibInfo::verMinor:
				return DM_VERSION_MINOR;

			case LibInfo::verRevision:
				return DM_VERSION_REVISION;

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
		}
		return -1; // should never happen
	}

	enum class DarkModeType : std::uint8_t
	{
		light   = 0,
		dark    = 1,
		//windows = 2, // never used

		classic = 3
	};

	enum class SubclassID : std::uint8_t
	{
		button          = 42,
		groupbox        = 1,
		upDown          = 2,
		tabPaint        = 3,
		tabUpDown       = 4,
		customBorder    = 5,
		comboBox        = 6,
		comboBoxEx      = 7,
		listView        = 8,
		header          = 9,
		statusBar       = 10,
		progress        = 11,
		eraseBg         = 12,
		ctlColor        = 13,
		staticText      = 14,
		notify          = 15,
		menuBar         = 16,
		settingChange   = 17
	};

	enum class WinMode : std::uint8_t
	{
		disabled,
		light,
		classic
	};

	struct DarkModeParams
	{
		const wchar_t* _themeClassName = nullptr;
		bool _subclass = false;
		bool _theme = false;
	};

	static struct
	{
		DWM_WINDOW_CORNER_PREFERENCE _roundCorner = DWMWCP_DEFAULT;
		COLORREF _borderColor = DWMWA_COLOR_DEFAULT;
		DWM_SYSTEMBACKDROP_TYPE _mica = DWMSBT_AUTO;
		TreeViewStyle _treeViewStyle = TreeViewStyle::classic;
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

	// range to determine when it should be better to use classic style for tree view
	constexpr double MiddleGrayRange = 2.0;

	static struct
	{
		double _lightness = 50.0;
		COLORREF _background = RGB(41, 49, 52);
		TreeViewStyle _stylePrev = TreeViewStyle::classic;
	} g_tvCfg;

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

		explicit Brushes(const Colors& colors)
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

		void updateBrushes(const Colors& colors)
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

		explicit Pens(const Colors& colors)
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

		void updatePens(const Colors& colors)
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

	// black (default)
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

	constexpr DWORD offsetEdge = HEXRGB(0x1C1C1C);

	// red tone
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

	// green tone
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

	// blue tone
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

	// purple tone
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

	// cyan tone
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

	// olive tone
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
		Theme()
			: _colors(darkColors)
			, _brushes(darkColors)
			, _pens(darkColors)
		{}

		explicit Theme(const Colors& colors)
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
				{
					_colors = darkColors;
					break;
				}
			}

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

		explicit BrushesAndPensView(const ColorsView& colors)
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
		ThemeView()
			: _clrView(darkColorsView)
			, _hbrPnView(darkColorsView)
		{}

		explicit ThemeView(const ColorsView& colorsView)
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

	inline static COLORREF setNewColor(COLORREF* clrOld, COLORREF clrNew)
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

	void setBorderColorConfig(COLORREF clr)
	{
		if (clr == 0xFFFFFF)
		{
			g_dmCfg._borderColor = DWMWA_COLOR_DEFAULT;
		}
		else
		{
			g_dmCfg._borderColor = clr;
		}
	}

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

	void setMicaExtendedConfig(bool extendMica)
	{
		g_dmCfg._micaExtend = extendMica;
	}

#if !defined(_DARKMODELIB_NO_INI_CONFIG)
	static void initOptions(const std::wstring& iniName)
	{
		if (iniName.empty())
		{
			return;
		}

		std::wstring iniPath = getIniPath(iniName);
		if (fileExists(iniPath))
		{
			DarkMode::initDarkModeConfig(::GetPrivateProfileInt(L"main", L"mode", 1, iniPath.c_str()));
			if (g_dmCfg._dmType == DarkModeType::classic)
			{
				DarkMode::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
				DarkMode::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
				return;
			}

			const bool useDark = g_dmCfg._dmType == DarkModeType::dark;

			std::wstring sectionBase = useDark ? L"dark" : L"light";
			std::wstring sectionColorsView = sectionBase + L".colors.view";
			std::wstring sectionColors = sectionBase + L".colors";

			DarkMode::setMicaConfig(::GetPrivateProfileInt(sectionBase.c_str(), L"mica", 0, iniPath.c_str()));
			DarkMode::setRoundCornerConfig(::GetPrivateProfileInt(sectionBase.c_str(), L"roundCorner", 0, iniPath.c_str()));
			setClrFromIni(sectionBase, L"borderColor", iniPath, &g_dmCfg._borderColor);
			if (g_dmCfg._borderColor == 0xFFFFFF)
			{
				g_dmCfg._borderColor = DWMWA_COLOR_DEFAULT;
			}

			if (useDark)
			{
				UINT tone = ::GetPrivateProfileInt(sectionBase.c_str(), L"tone", 0, iniPath.c_str());
				if (tone > 6)
				{
					tone = 0;
				}

				DarkMode::getTheme().setToneColors(static_cast<DarkMode::ColorTone>(tone));
				DarkMode::getThemeView()._clrView = DarkMode::darkColorsView;
				DarkMode::getThemeView()._clrView.headerBackground = DarkMode::getTheme()._colors.background;
				DarkMode::getThemeView()._clrView.headerHotBackground = DarkMode::getTheme()._colors.hotBackground;
				DarkMode::getThemeView()._clrView.headerText = DarkMode::getTheme()._colors.darkerText;

				if (!DarkMode::isWindowsModeEnabled())
				{
					g_dmCfg._micaExtend = (::GetPrivateProfileInt(sectionBase.c_str(), L"micaExtend", 0, iniPath.c_str()) == 1);
				}
			}
			else
			{
				DarkMode::getTheme()._colors = DarkMode::getLightColors();
				DarkMode::getThemeView()._clrView = DarkMode::lightColorsView;
			}

			setClrFromIni(sectionColorsView, L"backgroundView", iniPath, &DarkMode::getThemeView()._clrView.background);
			setClrFromIni(sectionColorsView, L"textView", iniPath, &DarkMode::getThemeView()._clrView.text);
			setClrFromIni(sectionColorsView, L"gridlines", iniPath, &DarkMode::getThemeView()._clrView.gridlines);
			setClrFromIni(sectionColorsView, L"backgroundHeader", iniPath, &DarkMode::getThemeView()._clrView.headerBackground);
			setClrFromIni(sectionColorsView, L"backgroundHotHeader", iniPath, &DarkMode::getThemeView()._clrView.headerHotBackground);
			setClrFromIni(sectionColorsView, L"textHeader", iniPath, &DarkMode::getThemeView()._clrView.headerText);
			setClrFromIni(sectionColorsView, L"edgeHeader", iniPath, &DarkMode::getThemeView()._clrView.headerEdge);

			setClrFromIni(sectionColors, L"background", iniPath, &DarkMode::getTheme()._colors.background);
			setClrFromIni(sectionColors, L"backgroundCtrl", iniPath, &DarkMode::getTheme()._colors.ctrlBackground);
			setClrFromIni(sectionColors, L"backgroundHot", iniPath, &DarkMode::getTheme()._colors.hotBackground);
			setClrFromIni(sectionColors, L"backgroundDlg", iniPath, &DarkMode::getTheme()._colors.dlgBackground);
			setClrFromIni(sectionColors, L"backgroundError", iniPath, &DarkMode::getTheme()._colors.errorBackground);

			setClrFromIni(sectionColors, L"text", iniPath, &DarkMode::getTheme()._colors.text);
			setClrFromIni(sectionColors, L"textItem", iniPath, &DarkMode::getTheme()._colors.darkerText);
			setClrFromIni(sectionColors, L"textDisabled", iniPath, &DarkMode::getTheme()._colors.disabledText);
			setClrFromIni(sectionColors, L"textLink", iniPath, &DarkMode::getTheme()._colors.linkText);

			setClrFromIni(sectionColors, L"edge", iniPath, &DarkMode::getTheme()._colors.edge);
			setClrFromIni(sectionColors, L"edgeHot", iniPath, &DarkMode::getTheme()._colors.hotEdge);
			setClrFromIni(sectionColors, L"edgeDisabled", iniPath, &DarkMode::getTheme()._colors.disabledEdge);

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

	//static void initOptions()
	//{
	//	initOptions(L"");
	//}
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

	void setDarkModeConfig(UINT dmType)
	{
		DarkMode::initDarkModeConfig(dmType);
		DarkMode::setDarkMode(g_dmCfg._dmType == DarkModeType::dark, true);
	}

	void setDarkModeConfig()
	{
		const auto dmType = static_cast<UINT>(DarkMode::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic);
		DarkMode::setDarkModeConfig(dmType);
	}

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
#else
			DarkMode::setDarkModeConfig();
#endif

			DarkMode::calculateTreeViewStyle();
			DarkMode::setDarkMode(g_dmCfg._dmType == DarkModeType::dark, true);

			DarkMode::setSysColor(COLOR_WINDOW, DarkMode::getBackgroundColor());
			DarkMode::setSysColor(COLOR_WINDOWTEXT, DarkMode::getTextColor());
			DarkMode::setSysColor(COLOR_BTNFACE, DarkMode::getViewGridlinesColor());

			g_dmCfg._isInit = true;
		}
	}

	void initDarkMode()
	{
		DarkMode::initDarkMode(L"");
	}

	bool isEnabled()
	{
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
		return g_dmCfg._dmType != DarkModeType::classic;
#else
		return DarkMode::isWindows10() && g_dmCfg._dmType != DarkModeType::classic;
#endif
	}

	bool isExperimentalActive()
	{
		return g_darkModeEnabled;
	}

	bool isExperimentalSupported()
	{
		return g_darkModeSupported;
	}

	bool isWindowsModeEnabled()
	{
		return g_dmCfg._windowsMode != WinMode::disabled;
	}

	bool isWindows10()
	{
		return ::IsWindows10();
	}

	bool isWindows11()
	{
		return ::IsWindows11();
	}

	DWORD getWindowsBuildNumber()
	{
		return GetWindowsBuildNumber();
	}

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

	void setSysColor(int nIndex, COLORREF color)
	{
		::SetMySysColor(nIndex, color);
	}

	bool hookSysColor()
	{
		return ::HookSysColor();
	}
	void unhookSysColor()
	{
		::UnhookSysColor();
	}

	void enableDarkScrollBarForWindowAndChildren(HWND hWnd)
	{
		::EnableDarkScrollBarForWindowAndChildren(hWnd);
	}

	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width, int height)
	{
		auto holdBrush = ::SelectObject(hdc, hBrush);
		auto holdPen = ::SelectObject(hdc, hpen);
		::RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, width, height);
		::SelectObject(hdc, holdBrush);
		::SelectObject(hdc, holdPen);
	}

	void paintRoundFrameRect(HDC hdc, const RECT& rect, HPEN hpen, int width, int height)
	{
		DarkMode::paintRoundRect(hdc, rect, hpen, static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), width, height);
	}

	constexpr int Win11CornerRoundness = 4;

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

		void closeTheme()
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

		void releaseBuffer()
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

		explicit FontData(HFONT hFont)
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

		void setFont(HFONT newFont)
		{
			FontData::destroyFont();
			_hFont = newFont;
		}

		[[nodiscard]] const HFONT& getFont() const
		{
			return _hFont;
		}

		[[nodiscard]] bool hasFont() const
		{
			return _hFont != nullptr;
		}

		void destroyFont()
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
					break;
			}
		}
	};

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
			RECT rcFocus{ rcText.left - 1, rcText.top, rcText.right + 1, rcText.bottom + 1 };
			::DrawFocusRect(hdc, &rcFocus);
		}

		::SelectObject(hdc, holdFont);
		if (isFontCreated)
		{
			::DeleteObject(hFont);
		}
	}

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

				if ((nState & BST_CHECKED) == BST_CHECKED)      { iStateID += 4; }
				else if ((nState & BST_INDETERMINATE) == BST_INDETERMINATE) { iStateID += 8; }

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
				// assert(false);
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

	constexpr auto ButtonSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::button);

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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ButtonData>(hWnd, ButtonSubclass, ButtonSubclassID);
	}

	void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ButtonData>(hWnd, ButtonSubclass, ButtonSubclassID);
	}

	static void paintGroupbox(HWND hWnd, HDC hdc, const ButtonData& buttonData)
	{
		const auto& hTheme = buttonData._themeData.getHTheme();

		const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
		constexpr int iPartID = BP_GROUPBOX;
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

		//DrawThemeParentBackground(hWnd, hdc, &rcClient);
		//DrawThemeBackground(hTheme, hdc, BP_GROUPBOX, iStateID, &rcBackground, nullptr);
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

	constexpr auto GroupboxSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::groupbox);

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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setGroupboxCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ButtonData>(hWnd, GroupboxSubclass, GroupboxSubclassID);
	}

	void removeGroupboxCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ButtonData>(hWnd, GroupboxSubclass, GroupboxSubclassID);
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

				if (DarkMode::isWindows11() && p._theme)
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
			: _cornerRoundness((DarkMode::isWindows11() && cmpWndClassName(::GetParent(hWnd), WC_TABCONTROL)) ? (Win11CornerRoundness + 1) : 0)
			, _isHorizontal((::GetWindowLongPtr(hWnd, GWL_STYLE)& UDS_HORZ) == UDS_HORZ)
		{
			updateRect(hWnd);
		}

		void updateRectUpDown()
		{
			if (_isHorizontal)
			{
				RECT rcArrowLeft{
					_rcClient.left, _rcClient.top,
					_rcClient.right - ((_rcClient.right - _rcClient.left) / 2) - 1, _rcClient.bottom
				};

				RECT rcArrowRight{
					rcArrowLeft.right + 1, _rcClient.top,
					_rcClient.right, _rcClient.bottom
				};

				_rcPrev = rcArrowLeft;
				_rcNext = rcArrowRight;
			}
			else
			{
				constexpr LONG offset = 2;

				RECT rcArrowTop{
					_rcClient.left + offset, _rcClient.top,
					_rcClient.right, _rcClient.bottom - ((_rcClient.bottom - _rcClient.top) / 2)
				};

				RECT rcArrowBottom{
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

		constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
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

	constexpr auto UpDownSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::upDown);

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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setUpDownCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<UpDownData>(hWnd, UpDownSubclass, UpDownSubclassID, hWnd);
		DarkMode::setDarkExplorerTheme(hWnd);
	}

	void removeUpDownCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<UpDownData>(hWnd, UpDownSubclass, UpDownSubclassID);
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

		const int nTabs = TabCtrl_GetItemCount(hWnd);

		const int iSelTab = TabCtrl_GetCurSel(hWnd);
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
					constexpr int offset = 2;
					::ImageList_GetIconSize(hImagelist, &cx, &cy);
					::ImageList_Draw(hImagelist, tci.iImage, hdc, rcText.left + offset, rcText.top + (((rcText.bottom - rcText.top) - cy) / 2), ILD_NORMAL);
					rcText.left += cx;
				}

				::DrawText(hdc, label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				::FrameRect(hdc, &rcFrame, DarkMode::getEdgeBrush());

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

	constexpr auto TabPaintSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::tabPaint);

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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static void setTabCtrlPaintSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<BufferData>(hWnd, TabPaintSubclass, TabPaintSubclassID);
	}

	static void removeTabCtrlPaintSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<BufferData>(hWnd, TabPaintSubclass, TabPaintSubclassID);
	}

	constexpr auto TabUpDownSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::tabUpDown);

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
				switch (LOWORD(wParam))
				{
					case WM_CREATE:
					{
						auto hUpDown = reinterpret_cast<HWND>(lParam);
						if (cmpWndClassName(hUpDown, UPDOWN_CLASS))
						{
							DarkMode::setUpDownCtrlSubclass(hUpDown);
							return 0;
						}
						break;
					}
				}
				return 0;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setTabCtrlUpDownSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, TabUpDownSubclass, TabUpDownSubclassID);
	}

	void removeTabCtrlUpDownSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, TabUpDownSubclass, TabUpDownSubclassID);
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
			DarkMode::setDarkTooltips(hWnd, DarkMode::ToolTipsType::tabbar);
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

	constexpr auto CustomBorderSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::customBorder);

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

				HDC hdc = ::GetWindowDC(hWnd);
				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				rcClient.right += (2 * pBorderMetricsData->_xEdge);

				const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				const bool hasVerScrollbar = (nStyle & WS_VSCROLL) == WS_VSCROLL;
				if (hasVerScrollbar)
				{
					rcClient.right += pBorderMetricsData->_xScroll;
				}

				rcClient.bottom += (2 * pBorderMetricsData->_yEdge);

				const bool hasHorScrollbar = (nStyle & WS_HSCROLL) == WS_HSCROLL;
				if (hasHorScrollbar)
				{
					rcClient.bottom += pBorderMetricsData->_yScroll;
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

				HPEN hEnabledPen = ((pBorderMetricsData->_isHot && isHot) || hasFocus ? DarkMode::getHotEdgePen() : DarkMode::getEdgePen());

				DarkMode::paintRoundFrameRect(hdc, rcClient, (::IsWindowEnabled(hWnd) == TRUE) ? hEnabledPen : DarkMode::getDisabledEdgePen());

				::ReleaseDC(hWnd, hdc);

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

				//const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
				//const bool hasVerScrollbar = (nStyle & WS_VSCROLL) == WS_VSCROLL;
				//if (hasVerScrollbar)
				//{
				//	lpRect->right -= pBorderMetricsData->_xScroll;
				//}

				//const bool hasHorScrollbar = (nStyle & WS_HSCROLL) == WS_HSCROLL;
				//if (hasHorScrollbar)
				//{
				//	lpRect->bottom -= pBorderMetricsData->_yScroll;
				//}

				//return 0;
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	static void setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, CustomBorderSubclassID);
	}

	//void removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
	//{
	//	DarkMode::removeCtrlSubclass<BorderMetricsData>(hWnd, CustomBorderSubclass, CustomBorderSubclassID);
	//}

	static void setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p, bool isListBox)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		const bool hasScrollBar = ((nStyle & WS_HSCROLL) == WS_HSCROLL) || ((nStyle & WS_VSCROLL) == WS_VSCROLL);
		if (p._theme && (isListBox || hasScrollBar))
		{
			//dark scrollbar for listbox or edit control
			::SetWindowTheme(hWnd, p._themeClassName, nullptr);
		}

		const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		const bool hasClientEdge = (nExStyle & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE;
		const bool isCBoxListBox = isListBox && (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;

		if (p._subclass && hasClientEdge && !isCBoxListBox)
		{
			DarkMode::setCustomBorderForListBoxOrEditCtrlSubclass(hWnd);
		}

		if (::GetWindowSubclass(hWnd, CustomBorderSubclass, CustomBorderSubclassID, nullptr) == TRUE)
		{
			const bool enableClientEdge = !DarkMode::isEnabled();
			DarkMode::setWindowExStyle(hWnd, enableClientEdge, WS_EX_CLIENTEDGE);
		}
	}

	struct ComboboxData
	{
		ThemeData _themeData{ VSCLASS_COMBOBOX };
		BufferData _bufferData;

		LONG_PTR _cbStyle = CBS_SIMPLE;

		ComboboxData() = default;

		explicit ComboboxData(LONG_PTR cbStyle)
			: _cbStyle(cbStyle)
		{}
	};

	static void paintCombobox(HWND hWnd, HDC hdc, ComboboxData& comboboxData)
	{
		auto& themeData = comboboxData._themeData;
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
		if (comboboxData._cbStyle == CBS_DROPDOWNLIST)
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

				constexpr DWORD dtFlags = DT_NOPREFIX | DT_LEFT | DT_VCENTER | DT_SINGLELINE;
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
		else if ((isHot || hasFocus || comboboxData._cbStyle == CBS_SIMPLE))
		{
			hPen = DarkMode::getHotEdgePen();
		}
		else
		{
			hPen = DarkMode::getEdgePen();
		}
		auto holdPen = static_cast<HPEN>(::SelectObject(hdc, hPen));

		if (comboboxData._cbStyle != CBS_SIMPLE)
		{
			if (hasTheme)
			{
				RECT rcThemedArrow{ rcArrow.left, rcArrow.top - 1, rcArrow.right, rcArrow.bottom - 1 };
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

		if (comboboxData._cbStyle == CBS_DROPDOWNLIST)
		{
			::ExcludeClipRect(hdc, rcClient.left + 1, rcClient.top + 1, rcClient.right - 1, rcClient.bottom - 1);
		}
		else
		{
			::ExcludeClipRect(hdc, cbi.rcItem.left, cbi.rcItem.top, cbi.rcItem.right, cbi.rcItem.bottom);

			if (comboboxData._cbStyle == CBS_SIMPLE && cbi.hwndList != nullptr)
			{
				RECT rcItem{ cbi.rcItem };
				::MapWindowPoints(cbi.hwndItem, hWnd, reinterpret_cast<LPPOINT>(&rcItem), 2);
				rcClient.bottom = rcItem.bottom;
			}

			RECT rcInner{ rcClient };
			::InflateRect(&rcInner, -1, -1);

			if (comboboxData._cbStyle == CBS_DROPDOWN)
			{
				const std::array<POINT, 2> edge{ {
					{rcArrow.left - 1, rcArrow.top},
					{rcArrow.left - 1, rcArrow.bottom}
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

		const int roundCornerValue = DarkMode::isWindows11() ? Win11CornerRoundness : 0;

		DarkMode::paintRoundFrameRect(hdc, rcClient, hPen, roundCornerValue, roundCornerValue);

		::SelectObject(hdc, holdPen);
	}

	constexpr auto ComboBoxSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::comboBox);

	static LRESULT CALLBACK ComboBoxSubclass(
		HWND hWnd,
		UINT uMsg,
		WPARAM wParam,
		LPARAM lParam,
		UINT_PTR uIdSubclass,
		DWORD_PTR dwRefData
	)
	{
		auto* pComboboxData = reinterpret_cast<ComboboxData*>(dwRefData);
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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxCtrlSubclass(HWND hWnd)
	{
		const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
		DarkMode::setSubclass<ComboboxData>(hWnd, ComboBoxSubclass, ComboBoxSubclassID, cbStyle);
	}

	void removeComboBoxCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ComboboxData>(hWnd, ComboBoxSubclass, ComboBoxSubclassID);
	}

	static void setComboBoxCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);

		if ((nStyle & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST
			|| (nStyle & CBS_DROPDOWN) == CBS_DROPDOWN
			|| (nStyle & CBS_SIMPLE) == CBS_SIMPLE)
		{
			COMBOBOXINFO cbi{};
			cbi.cbSize = sizeof(COMBOBOXINFO);
			if (::GetComboBoxInfo(hWnd, &cbi) == TRUE)
			{
				if (p._theme && cbi.hwndList != nullptr)
				{
					if ((nStyle & CBS_SIMPLE) == CBS_SIMPLE)
					{
						DarkMode::replaceClientEdgeWithBorderSafe(cbi.hwndList);
					}

					//dark scrollbar for listbox of combobox
					::SetWindowTheme(cbi.hwndList, p._themeClassName, nullptr);
				}
			}

			if (p._subclass)
			{
				HWND hParent = ::GetParent(hWnd);
				if ((hParent == nullptr || getWndClassName(hParent) != WC_COMBOBOXEX))
				{
					DarkMode::setComboBoxCtrlSubclass(hWnd);
				}
			}

			if (p._theme)
			{
				DarkMode::setDarkThemeExperimental(hWnd, L"CFD");
			}
		}
	}

	constexpr auto ComboBoxExSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::comboBoxEx);

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

				// ComboboxEx has only one child combobox, so only control-defined notification code is checked.
				// Hooking is done only when listbox is about to show. And unhook when listbox is closed.
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
						break;
				}
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setComboBoxExCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, ComboboxExSubclass, ComboBoxExSubclassID);
	}

	void removeComboBoxExCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, ComboboxExSubclass, ComboBoxExSubclassID);
		DarkMode::unhookSysColor();
	}

	static void setComboBoxExCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._subclass)
		{
			DarkMode::setComboBoxExCtrlSubclass(hWnd);
		}
	}

	constexpr auto ListViewSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::listView);

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

				switch (reinterpret_cast<LPNMHDR>(lParam)->code)
				{
					case NM_CUSTOMDRAW:
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
								return CDRF_DODEFAULT;
						}
					}
				}
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setListViewCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, ListViewSubclass, ListViewSubclassID);
	}

	void removeListViewCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, ListViewSubclass, ListViewSubclassID);
	}

	static void setListViewCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			ListView_SetTextColor(hWnd, DarkMode::getViewTextColor());
			ListView_SetTextBkColor(hWnd, DarkMode::getViewBackgroundColor());
			ListView_SetBkColor(hWnd, DarkMode::getViewBackgroundColor());

			DarkMode::setDarkListView(hWnd);
			DarkMode::setDarkListViewCheckboxes(hWnd);
			DarkMode::setDarkTooltips(hWnd, DarkMode::ToolTipsType::listview);
		}

		if (p._subclass)
		{
			HWND hHeader = ListView_GetHeader(hWnd);
			DarkMode::setDarkHeader(hHeader);

			const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
			ListView_SetExtendedListViewStyle(hWnd, lvExStyle | LVS_EX_DOUBLEBUFFER);
			DarkMode::setListViewCtrlSubclass(hWnd);
		}
	}

	struct HeaderData
	{
		ThemeData _themeData{ VSCLASS_HEADER };
		BufferData _bufferData;
		FontData _fontData;

		POINT _pt{ LONG_MIN, LONG_MIN };
		bool _isHot = false;
		bool _hasBtnStyle = true;
		bool _isPressed = false;

		HeaderData() = default;

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
				{edgeX, rcItem.top},
				{edgeX, rcItem.bottom}
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

			rcItem.left += 6;
			rcItem.right -= 8;

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

	constexpr auto HeaderSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::header);

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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setHeaderCtrlSubclass(HWND hWnd)
	{
		const bool hasBtnStyle = (::GetWindowLongPtr(hWnd, GWL_STYLE) & HDS_BUTTONS) == HDS_BUTTONS;
		DarkMode::setSubclass<HeaderData>(hWnd, HeaderSubclass, HeaderSubclassID, hasBtnStyle);
	}

	void removeHeaderCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<HeaderData>(hWnd, HeaderSubclass, HeaderSubclassID);
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
					{rcPart.right - borders.between, rcPart.top + 1},
					{rcPart.right - borders.between, rcPart.bottom - 3}
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

	constexpr auto StatusBarSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::statusBar);

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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
		DarkMode::setSubclass<StatusBarData>(hWnd, StatusBarSubclass, StatusBarSubclassID, ::CreateFontIndirect(&lf));
	}

	void removeStatusBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<StatusBarData>(hWnd, StatusBarSubclass, StatusBarSubclassID);
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

		int _iStateID = PBFS_PARTIAL; // PBFS_PARTIAL for cyan color

		ProgressBarData() = default;
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

	constexpr auto ProgressBarSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::progress);

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

				auto hdc = reinterpret_cast<HDC>(wParam);
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
				}
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setProgressBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, ProgressBarSubclassID);
	}

	void removeProgressBarCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ProgressBarData>(hWnd, ProgressBarSubclass, ProgressBarSubclassID);
	}

	static void setProgressBarCtrlSubclass(HWND hWnd, DarkModeParams p)
	{
		if (p._theme)
		{
			if (p._subclass)
			{
				DarkMode::setProgressBarCtrlSubclass(hWnd);
			}
		}
		else
		{
			DarkMode::setProgressBarClassicTheme(hWnd);
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

	constexpr auto StaticTextSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::staticText);

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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setStaticTextCtrlSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<StaticTextData>(hWnd, StaticTextSubclass, StaticTextSubclassID, hWnd);
	}

	void removeStaticTextCtrlSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<StaticTextData>(hWnd, StaticTextSubclass, StaticTextSubclassID);
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

			DarkMode::setTreeViewStyle(hWnd, p._theme);
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

	static BOOL CALLBACK DarkEnumChildProc(HWND hWnd, LPARAM lParam)
	{
		const auto& p = *reinterpret_cast<DarkModeParams*>(lParam);
		std::wstring className = getWndClassName(hWnd);

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
			if (p._theme)
			{
				DarkMode::setDarkScrollBar(hWnd);
			}
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

		/*
		// for debugging
		if (className == L"#32770")
		{
			return TRUE;
		}

		if (className == TRACKBAR_CLASS)
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
		DarkMode::setChildCtrlsSubclassAndTheme(hParent, false, DarkMode::isWindows10());
#endif
	}

	constexpr auto WindowEraseBgSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::eraseBg);

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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowEraseBgSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowEraseBgSubclass, WindowEraseBgSubclassID);
	}

	void removeWindowEraseBgSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowEraseBgSubclass, WindowEraseBgSubclassID);
	}

	constexpr auto WindowCtlColorSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::ctlColor);

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
				std::wstring className = getWndClassName(hChild);

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
				if (::GetWindowSubclass(hChild, StaticTextSubclass, StaticTextSubclassID, &dwRefData) == TRUE)
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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowCtlColorSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowCtlColorSubclass, WindowCtlColorSubclassID);
	}

	void removeWindowCtlColorSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowCtlColorSubclass, WindowCtlColorSubclassID);
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

		const int roundCornerValue = DarkMode::isWindows11() ? Win11CornerRoundness + 1 : 0;

		if (isHot)
		{
			if (!isIcon)
			{
				::FillRect(lptbcd->nmcd.hdc, &rcItem, DarkMode::getHotBackgroundBrush());
			}
			else
			{
				DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundCornerValue, roundCornerValue);
				if (isDropDown)
				{
					DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundCornerValue, roundCornerValue);
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
				DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundCornerValue, roundCornerValue);
				if (isDropDown)
				{
					DarkMode::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundCornerValue, roundCornerValue);
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
				break;
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

				const LVITEMINDEX lvii{ static_cast<int>(lplvcd->nmcd.dwItemSpec), 0 };
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
				break;
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

			if (DarkMode::isWindows10() || g_dmCfg._treeViewStyle == TreeViewStyle::light)
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
				break;
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	[[nodiscard]] static LRESULT darkTrackBarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
						break;
				}
				break;
			}

			default:
				break;
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
			const int roundCornerValue = DarkMode::isWindows11() ? Win11CornerRoundness + 1 : 0;

			const bool isHot = (rbBand.uChevronState & STATE_SYSTEM_HOTTRACKED) == STATE_SYSTEM_HOTTRACKED;
			const bool isPressed = (rbBand.uChevronState & STATE_SYSTEM_PRESSED) == STATE_SYSTEM_PRESSED;

			if (isHot)
			{
				DarkMode::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundCornerValue, roundCornerValue);
			}
			else if (isPressed)
			{
				DarkMode::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundCornerValue, roundCornerValue);
			}

			::SetTextColor(lpnmcd->hdc, isHot ? DarkMode::getTextColor() : DarkMode::getDarkerTextColor());
			::SetBkMode(lpnmcd->hdc, TRANSPARENT);

			constexpr auto dtFlags = DT_NOPREFIX | DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOCLIP;
			::DrawText(lpnmcd->hdc, L"»", -1, &rbBand.rcChevronLocation, dtFlags);

			retVal = CDRF_SKIPDEFAULT;
		}
		return retVal;
	}

	[[nodiscard]] static LRESULT darkRebarNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);

		switch (lpnmcd->dwDrawStage)
		{
			case CDDS_PREPAINT:
			{
				return DarkMode::prepaintRebar(lpnmcd);
			}

			default:
				break;
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	constexpr auto WindowNotifySubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::notify);

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
				std::wstring className = getWndClassName(lpnmhdr->hwndFrom);

				switch (lpnmhdr->code)
				{
					case NM_CUSTOMDRAW:
					{
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
							return DarkMode::darkTrackBarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
						}

						if (className == REBARCLASSNAME)
						{
							return DarkMode::darkRebarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
						}
						break;
					}

					default:
						break;
				}
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowNotifyCustomDrawSubclass(HWND hWnd, bool subclassChildren)
	{
		if (DarkMode::setSubclass(hWnd, WindowNotifySubclass, WindowNotifySubclassID) == TRUE)
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
		DarkMode::removeSubclass(hWnd, WindowNotifySubclass, WindowNotifySubclassID);
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

	constexpr auto WindowMenuBarSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::menuBar);

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
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowMenuBarSubclass(HWND hWnd)
	{
		DarkMode::setSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, WindowMenuBarSubclassID, VSCLASS_MENU);
	}

	void removeWindowMenuBarSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass<ThemeData>(hWnd, WindowMenuBarSubclass, WindowMenuBarSubclassID);
	}

	constexpr auto WindowSettingChangeSubclassID = static_cast<UINT_PTR>(DarkMode::SubclassID::settingChange);

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
					::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
				}
				break;
			}
		}
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	void setWindowSettingChangeSubclass(HWND hWnd)
	{
		DarkMode::setSubclass(hWnd, WindowSettingChangeSubclass, WindowSettingChangeSubclassID);
	}

	void removeWindowSettingChangeSubclass(HWND hWnd)
	{
		DarkMode::removeSubclass(hWnd, WindowSettingChangeSubclass, WindowSettingChangeSubclassID);
	}

	void enableSysLinkCtrlCtlColor(HWND hWnd)
	{
		LITEM item{};
		item.iLink = 0; // for now colorize only 1st item
		item.mask = LIF_ITEMINDEX | LIF_STATE;
		item.state = DarkMode::isEnabled() ? LIS_DEFAULTCOLORS : 0;
		item.stateMask = LIS_DEFAULTCOLORS;
		::SendMessage(hWnd, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
	}

	void setDarkTitleBarEx(HWND hWnd, bool useWin11Features)
	{
		constexpr DWORD win10Build2004 = 19041;
		constexpr DWORD win11Mica = 22621;
		if (DarkMode::getWindowsBuildNumber() >= win10Build2004)
		{
			const BOOL useDark = DarkMode::isExperimentalActive() ? TRUE : FALSE;
			::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));

			if (useWin11Features && DarkMode::isWindows11())
			{
				::DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &g_dmCfg._roundCorner, sizeof(g_dmCfg._roundCorner));
				::DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &g_dmCfg._borderColor, sizeof(g_dmCfg._borderColor));

				if (DarkMode::getWindowsBuildNumber() >= win11Mica)
				{
					if (g_dmCfg._micaExtend && g_dmCfg._mica != DWMSBT_AUTO && !DarkMode::isWindowsModeEnabled() && (g_dmCfg._dmType == DarkModeType::dark))
					{
						constexpr MARGINS margins{ -1, 0, 0, 0 };
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

	void setDarkTitleBar(HWND hWnd)
	{
		DarkMode::setDarkTitleBarEx(hWnd, false);
	}

	void setDarkExplorerTheme(HWND hWnd)
	{
		::SetWindowTheme(hWnd, DarkMode::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr, nullptr);
	}

	void setDarkScrollBar(HWND hWnd)
	{
		DarkMode::setDarkExplorerTheme(hWnd);
	}

	void setDarkTooltips(HWND hWnd, ToolTipsType type)
	{
		UINT msg = 0;
		switch (type)
		{
			case DarkMode::ToolTipsType::toolbar:
				msg = TB_GETTOOLTIPS;
				break;
			case DarkMode::ToolTipsType::listview:
				msg = LVM_GETTOOLTIPS;
				break;
			case DarkMode::ToolTipsType::treeview:
				msg = TVM_GETTOOLTIPS;
				break;
			case DarkMode::ToolTipsType::tabbar:
				msg = TCM_GETTOOLTIPS;
				break;
			case DarkMode::ToolTipsType::tooltip:
				msg = 0;
				break;
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

	void setDarkHeader(HWND hWnd)
	{
		//DarkMode::setDarkThemeExperimental(hWnd, L"ItemsView");
		DarkMode::setHeaderCtrlSubclass(hWnd);
	}

	void setDarkListView(HWND hWnd)
	{
		DarkMode::setDarkThemeExperimental(hWnd, L"Explorer");
	}

	void setDarkListViewCheckboxes(HWND hWnd)
	{
		if (!DarkMode::isWindows11())
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

		RECT rcBox{ 0, 0, szBox.cx, szBox.cy };

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

	void setDarkThemeExperimental(HWND hWnd, const wchar_t* themeClassName)
	{
		if (DarkMode::isExperimentalSupported())
		{
			DarkMode::allowDarkModeForWindow(hWnd, DarkMode::isExperimentalActive());
			::SetWindowTheme(hWnd, themeClassName, nullptr);
		}
	}

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

	void setDarkDlgSafe(HWND hWnd, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		DarkMode::setDarkTitleBarEx(hWnd, useWin11Features);
		//DarkMode::setWindowEraseBgSubclass(hWnd);
		DarkMode::setWindowCtlColorSubclass(hWnd);
		DarkMode::setChildCtrlsSubclassAndTheme(hWnd);
	}

	void setDarkDlgNotifySafe(HWND hWnd, bool useWin11Features)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		DarkMode::setDarkTitleBarEx(hWnd, useWin11Features);
		//DarkMode::setWindowEraseBgSubclass(hWnd);
		DarkMode::setWindowCtlColorSubclass(hWnd);
		DarkMode::setWindowNotifyCustomDrawSubclass(hWnd, true);
	}

	void enableThemeDialogTexture(HWND hWnd, bool theme)
	{
		::EnableThemeDialogTexture(hWnd, theme && (g_dmCfg._dmType == DarkModeType::classic) ? ETDT_ENABLETAB : ETDT_DISABLE);
	}

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

	// adapted from https://stackoverflow.com/a/56678483
	double calculatePerceivedLightness(COLORREF clr)
	{
		auto linearValue = [](double colorChannel) -> double {
			colorChannel /= 255.0;
			if (colorChannel <= 0.04045)
			{
				return colorChannel / 12.92;
			}
			return std::pow(((colorChannel + 0.055) / 1.055), 2.4);
		};

		const double r = linearValue(static_cast<double>(GetRValue(clr)));
		const double g = linearValue(static_cast<double>(GetGValue(clr)));
		const double b = linearValue(static_cast<double>(GetBValue(clr)));

		const double luminance = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);

		const double lightness = (luminance <= 216.0 / 24389.0) ? (luminance * 24389.0 / 27.0) : ((std::pow(luminance, (1.0 / 3.0)) * 116.0) - 16.0);
		return lightness;
	}

	void calculateTreeViewStyle()
	{
		constexpr double middle = 50.0;
		const COLORREF bgColor = DarkMode::getViewBackgroundColor();

		if (g_tvCfg._background != bgColor || g_tvCfg._lightness == middle)
		{
			g_tvCfg._lightness = calculatePerceivedLightness(bgColor);
			g_tvCfg._background = bgColor;
		}

		if (g_tvCfg._lightness < (middle - MiddleGrayRange))
		{
			g_dmCfg._treeViewStyle = TreeViewStyle::dark;
		}
		else if (g_tvCfg._lightness > (middle + MiddleGrayRange))
		{
			g_dmCfg._treeViewStyle = TreeViewStyle::light;
		}
		else
		{
			g_dmCfg._treeViewStyle = TreeViewStyle::classic;
		}
	}

	void updatePrevTreeViewStyle()
	{
		g_tvCfg._stylePrev = g_dmCfg._treeViewStyle;
	}

	TreeViewStyle getTreeViewStyle()
	{
		const auto style = g_dmCfg._treeViewStyle;
		return style;
	}

	void setTreeViewStyle(HWND hWnd, bool force)
	{
		if (force || g_tvCfg._stylePrev != g_dmCfg._treeViewStyle)
		{
			auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			const bool hasHotStyle = (nStyle & TVS_TRACKSELECT) == TVS_TRACKSELECT;
			bool change = false;
			std::wstring strSubAppName;

			switch (g_dmCfg._treeViewStyle)
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

	bool isThemeDark()
	{
		return g_dmCfg._treeViewStyle == TreeViewStyle::dark;
	}

	void redrawWindowFrame(HWND hWnd)
	{
		::SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	}

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

	void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag)
	{
		if (DarkMode::setWindowLongPtrStyle(hWnd, setStyle, styleFlag, GWL_STYLE) == TRUE)
		{
			DarkMode::redrawWindowFrame(hWnd);
		}
	}

	void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag)
	{
		if (DarkMode::setWindowLongPtrStyle(hWnd, setExStyle, exStyleFlag, GWL_EXSTYLE) == TRUE)
		{
			DarkMode::redrawWindowFrame(hWnd);
		}
	}

	void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag)
	{
		DarkMode::setWindowExStyle(hWnd, !replace, exStyleFlag);
		DarkMode::setWindowStyle(hWnd, replace, WS_BORDER);
	}

	void replaceClientEdgeWithBorderSafe(HWND hWnd)
	{
		if (hWnd != nullptr)
		{
			DarkMode::replaceExEdgeWithBorder(hWnd, DarkMode::isEnabled(), WS_EX_CLIENTEDGE);
		}
	}

	void setProgressBarClassicTheme(HWND hWnd)
	{
		DarkMode::setWindowStyle(hWnd, DarkMode::isEnabled(), WS_DLGFRAME);
		DarkMode::disableVisualStyle(hWnd, DarkMode::isEnabled());
		if (DarkMode::isEnabled())
		{
			::SendMessage(hWnd, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(DarkMode::getBackgroundColor()));
			::SendMessage(hWnd, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(HEXRGB(0x06B025)));
		}
	}

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
