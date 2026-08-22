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

#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>
#include <type_traits>

#if defined(_DARKMODELIB_CUSTOM_MEM) && (_DARKMODELIB_CUSTOM_MEM >= 0x001)
#include "MemoryHelper.h"
#endif
#include "MemoryHelperDef.h"

namespace dmlib_subclass
{
	/**
	 * @brief Maximum Length of buffer for class name.
	 */
	inline constexpr int kMaxClassNameLen = 32;

	/**
	 * @brief Defines control subclass ID values.
	 */
	enum class SubclassID : unsigned char
	{
		button = 42,
		groupbox,
		upDown,
		tabPaint,
		tabUpDown,
		customBorder,
		comboBox,
		comboBoxEx,
		listView,
		header,
		statusBar,
		progressBar,
		staticText,
		ipAddress,
		hotKey,
		dtp,
		windowEraseBg,
		windowCtlColor,
		windowNotify,
		windowMenuBar,
		windowSettingChange,
		taskDlg,
		comDlg,
		comboLBox
	};

	/**
	 * @brief Attaches a typed subclass procedure with custom data to a window.
	 *
	 * If the subclass ID is not already attached, allocates a `T` instance using the given
	 * `param` and stores it as subclass reference data. Ownership is transferred to the system.
	 *
	 * @tparam      T               The user-defined data type associated with the subclass.
	 * @tparam      Param           Type used to initialize `T`.
	 * @param[in]   hWnd            Window handle.
	 * @param[in]   subclassProc    Subclass procedure.
	 * @param[in]   subID           Identifier for the subclass instance.
	 * @param[in]   param           Constructor argument forwarded to `T`.
	 * @return TRUE on success, FALSE on failure, -1 if subclass already set.
	 */
	template <typename T, typename Param>
	inline auto SetSubclass(HWND hWnd, SUBCLASSPROC subclassProc, SubclassID subID, const Param& param) DMLIB_MEM_NOEXCEPT -> int
	{
		if (const auto subclassID = static_cast<UINT_PTR>(subID);
			::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			if (auto pData = DMLIB_MEM_MAKE_UNIQUE<T>(param);
				pData != nullptr && ::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	/**
	 * @brief Attaches a typed subclass procedure with default-constructed data.
	 *
	 * Same logic as the other overload, but constructs `T` using its default constructor.
	 *
	 * @tparam      T               The user-defined data type associated with the subclass.
	 * @param[in]   hWnd            Window handle.
	 * @param[in]   subclassProc    Subclass procedure.
	 * @param[in]   subID           Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if already subclassed.
	 */
	template <typename T>
	inline auto SetSubclass(HWND hWnd, SUBCLASSPROC subclassProc, SubclassID subID) DMLIB_MEM_NOEXCEPT -> int
	{
		if (const auto subclassID = static_cast<UINT_PTR>(subID);
			::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			if (auto pData = DMLIB_MEM_MAKE_UNIQUE<T>();
				pData != nullptr && ::SetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR>(pData.get())) == TRUE)
			{
				pData.release();
				return TRUE;
			}
			return FALSE;
		}
		return -1;
	}

	/**
	 * @brief Attaches an untyped subclass (no reference data).
	 *
	 * Sets a subclass with no associated custom data.
	 *
	 * @param[in]   hWnd            Window handle.
	 * @param[in]   subclassProc    Subclass procedure.
	 * @param[in]   subID           Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if already subclassed.
	 */
	inline int SetSubclass(HWND hWnd, SUBCLASSPROC subclassProc, SubclassID subID) noexcept
	{
		if (const auto subclassID = static_cast<UINT_PTR>(subID);
			::GetWindowSubclass(hWnd, subclassProc, subclassID, nullptr) == FALSE)
		{
			return ::SetWindowSubclass(hWnd, subclassProc, subclassID, 0);
		}
		return -1;
	}

	/**
	 * @brief Removes a subclass and deletes associated user data (if provided).
	 *
	 * Retrieves and deletes user-defined `T` data stored in subclass reference
	 * (unless `T = void`, in which case no delete is performed). Then removes the subclass.
	 *
	 * @tparam      T               Optional type of reference data to delete.
	 * @param[in]   hWnd            Window handle.
	 * @param[in]   subclassProc    Subclass procedure.
	 * @param[in]   subID           Identifier for the subclass instance.
	 * @return TRUE on success, FALSE on failure, -1 if not present.
	 */
	template <typename T = void>
	inline auto RemoveSubclass(HWND hWnd, SUBCLASSPROC subclassProc, SubclassID subID) noexcept -> int
	{
		T* pData = nullptr;
		if (const auto subclassID = static_cast<UINT_PTR>(subID);
			::GetWindowSubclass(hWnd, subclassProc, subclassID, reinterpret_cast<DWORD_PTR*>(&pData)) == TRUE)
		{
			if constexpr (!std::is_void_v<T>)
			{
				if (pData != nullptr)
				{
					std::unique_ptr<T> u_ptrData(pData);
					u_ptrData.reset(nullptr);
				}
			}
			return ::RemoveWindowSubclass(hWnd, subclassProc, subclassID);
		}
		return -1;
	}

	/**
	 * @class ThemeData
	 * @brief RAII-style wrapper for `HTHEME` handle tied to a specific theme class.
	 *
	 * Prevents leaks by managing the lifecycle of a theme handle opened via `OpenThemeData()`.
	 * Ensures handles are released properly in the destructor via `CloseThemeData()`.
	 *
	 * Usage:
	 * - Construct with a valid theme class name (e.g. `L"Button"`).
	 * - Call `ensureTheme(HWND)` before drawing to open the theme handle.
	 * - Access the active handle via `getHTheme()`.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
	class ThemeData
	{
	public:
		ThemeData() = delete;

		explicit ThemeData(std::wstring_view themeClass) noexcept
		{
			const size_t copyLength = std::min(themeClass.length(), m_themeClass.size() - 1);
			std::copy_n(themeClass.begin(), copyLength, m_themeClass.begin());
		}

		ThemeData(const ThemeData&) = delete;
		ThemeData& operator=(const ThemeData&) = delete;

		ThemeData(ThemeData&&) = delete;
		ThemeData& operator=(ThemeData&&) = delete;

		~ThemeData()
		{
			closeTheme();
		}

		bool ensureTheme(HWND hWnd) noexcept
		{
			if (m_hTheme == nullptr && !m_themeClass.empty())
			{
				m_hTheme = ::OpenThemeData(hWnd, m_themeClass.data());
			}
			return m_hTheme != nullptr;
		}

		void closeTheme() noexcept
		{
			if (m_hTheme != nullptr)
			{
				::CloseThemeData(m_hTheme);
				m_hTheme = nullptr;
			}
		}

		[[nodiscard]] const HTHEME& getHTheme() const noexcept
		{
			return m_hTheme;
		}

	private:
		std::array<wchar_t, kMaxClassNameLen> m_themeClass{};
		HTHEME m_hTheme = nullptr;
	};

	/**
	 * @class BufferData
	 * @brief RAII-style utility for double buffer technique.
	 *
	 * Allocates and resizes an offscreen buffer for flicker-free GDI drawing. When
	 * `ensureBuffer()` is called with a target HDC and client rect, it creates or resizes
	 * a memory device context and bitmap accordingly. Automatically releases resources
	 * via `releaseBuffer()` and destructor.
	 *
	 * Usage:
	 * - Call `ensureBuffer()` before painting.
	 * - Draw to `getHMemDC()`.
	 * - BitBlt back to screen in WM_PAINT.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
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

		bool ensureBuffer(HDC hdc, const RECT& rcClient) noexcept
		{
			const int width = rcClient.right - rcClient.left;
			const int height = rcClient.bottom - rcClient.top;
			if (m_szBuffer.cx != width || m_szBuffer.cy != height)
			{
				releaseBuffer();
				m_hMemDC = ::CreateCompatibleDC(hdc);
				m_hMemBmp = ::CreateCompatibleBitmap(hdc, width, height);
				m_holdBmp = static_cast<HBITMAP>(::SelectObject(m_hMemDC, m_hMemBmp));
				m_szBuffer = { width, height };
			}

			return m_hMemDC != nullptr && m_hMemBmp != nullptr;
		}

		void releaseBuffer() noexcept
		{
			if (m_hMemDC != nullptr)
			{
				::SelectObject(m_hMemDC, m_holdBmp);
				::DeleteObject(m_hMemBmp);
				::DeleteDC(m_hMemDC);

				m_hMemDC = nullptr;
				m_hMemBmp = nullptr;
				m_holdBmp = nullptr;
				m_szBuffer = { 0, 0 };
			}
		}

		[[nodiscard]] const HDC& getHMemDC() const noexcept
		{
			return m_hMemDC;
		}

	private:
		HDC m_hMemDC = nullptr;
		HBITMAP m_hMemBmp = nullptr;
		HBITMAP m_holdBmp = nullptr;
		SIZE m_szBuffer{};
	};

	/**
	 * @class FontData
	 * @brief RAII-style wrapper for managing a GDI font (`HFONT`) resource.
	 *
	 * Ensures safe creation, assignment, and destruction of fonts in GDI-based UI code.
	 * Automatically deletes the font in the destructor or when replaced via `setFont()`.
	 *
	 * Usage:
	 * - Use `setFont()` to assign a new font, deleting any previous one.
	 * - `getFont()` provides access to the current `HFONT`.
	 * - `hasFont()` checks if a valid font is currently held.
	 *
	 * Copying and moving are explicitly disabled to preserve exclusive ownership.
	 */
	class FontData
	{
	public:
		FontData() = default;

		explicit FontData(HFONT hFont) noexcept
			: m_hFont(hFont)
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
			m_hFont = newFont;
		}

		[[nodiscard]] const HFONT& getFont() const noexcept
		{
			return m_hFont;
		}

		[[nodiscard]] bool hasFont() const noexcept
		{
			return m_hFont != nullptr;
		}

		void destroyFont() noexcept
		{
			if (FontData::hasFont())
			{
				::DeleteObject(m_hFont);
				m_hFont = nullptr;
			}
		}

	private:
		HFONT m_hFont = nullptr;
	};

	/**
	 * @class WndClassName
	 * @brief Helper class for checking window class name.
	 */
	class WndClassName
	{
	public:
		explicit WndClassName(HWND hWnd) noexcept
			: m_length(static_cast<size_t>(::GetClassNameW(hWnd, m_className.data(), static_cast<int>(m_className.size()))))
			, m_view(m_className.data(), m_length)
		{}

		[[nodiscard]] bool operator==(const wchar_t* classNameToCmp) const noexcept
		{
			return m_view == classNameToCmp;
		}

		/**
		 * @brief Compares the class name of a window with a specified string.
		 *
		 * This function retrieves the class name of the given window handle
		 * and compares it to the provided class name.
		 *
		 * @param[in]   hWnd            Handle to the window whose class name is to be checked.
		 * @param[in]   classNameToCmp  Pointer to a null-terminated wide string representing the class name to compare against.
		 * @return `true` if the window's class name matches the specified string.
		 * @return `false` otherwise.
		 */
		[[nodiscard]] static bool cmpWndClassName(HWND hWnd, const wchar_t* classNameToCmp) noexcept
		{
			if (hWnd == nullptr)
			{
				return false;
			}

			const auto wndClassName = WndClassName(hWnd);
			if (wndClassName.m_length == 0)
			{
				return false;
			}
			return (wndClassName.m_view == classNameToCmp);
		}

	private:
		std::array<wchar_t, kMaxClassNameLen> m_className{};
		size_t m_length = 0;
		std::wstring_view m_view;
	};

	/// Determines if themed styling should be preferred over subclassing.
	[[nodiscard]] bool isThemePrefered() noexcept;
} // namespace dmlib_subclass
