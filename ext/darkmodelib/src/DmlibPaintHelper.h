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

#include <utility>

namespace dmlib_paint
{
	/// Base roundness value for various controls, such as toolbar iconic buttons and combo boxes
	inline constexpr int kWin11CornerRoundness = 4;

	/**
	 * @class GdiObject
	 * @brief RAII wrapper for managing GDI objects in a device context.
	 *
	 * Automatically selects a GDI object (e.g., brush, pen, font) into a device context (DC),
	 * stores the previous object, and restores it upon destruction. Optionally deletes the
	 * selected object unless marked as shared.
	 *
	 * Logic:
	 * - Prevents resource leaks and ensures proper cleanup of GDI objects.
	 * - Supports shared objects (e.g., system fonts or brushes) via the `isShared` flag.
	 * - Uses `SelectObject()` to apply and restore the DC state.
	 * - Deletes the object via `DeleteObject()` unless shared.
	 *
	 * Constructors:
	 * - `GdiObject(HDC hdc, HGDIOBJ obj, bool isShared)`
	 *   Selects the object into the DC and marks it as shared or owned.
	 * - `GdiObject(HDC hdc, HGDIOBJ obj)`
	 *   Convenience constructor for non-shared objects.
	 * - `GdiObject(HDC hdc, HWND hWnd)`
	 *   Convenience constructor for shared HFONT object acquired from WM_GETFONT.
	 *
	 * Destructor:
	 * - Automatically restores the previous object and deletes the selected one if owned.
	 *
	 * Methods:
	 * - `void deleteObj()`
	 *   Manually restores and deletes the object (if not shared).
	 *
	 * @note The default constructor is deleted to enforce explicit initialization.
	 */
	class GdiObject
	{
	public:
		GdiObject() = delete;
		explicit GdiObject(HDC hdc, HGDIOBJ obj, bool isShared) noexcept
			: m_hdc(hdc)
			, m_hObj(obj)
			, m_isShared(isShared)
		{
			if (m_hObj != nullptr)
			{
				m_holdObj = ::SelectObject(hdc, obj);
			}
		}

		explicit GdiObject(HDC hdc, HGDIOBJ obj) noexcept
			: GdiObject(hdc, obj, false)
		{}

		explicit GdiObject(HDC hdc, HWND hWnd) noexcept
			: GdiObject(hdc, reinterpret_cast<HFONT>(::SendMessageW(hWnd, WM_GETFONT, 0, 0)), true)
		{}

		~GdiObject()
		{
			deleteObj();
		}

		void deleteObj() noexcept
		{
			if (m_hObj != nullptr)
			{
				::SelectObject(m_hdc, m_holdObj);
				if (!m_isShared)
				{
					::DeleteObject(m_hObj);
					m_hObj = nullptr;
				}
			}
		}

		GdiObject(const GdiObject&) = delete;
		GdiObject& operator=(const GdiObject&) = delete;

		GdiObject(GdiObject&&) = delete;
		GdiObject& operator=(GdiObject&&) = delete;

		explicit operator HGDIOBJ() const noexcept
		{
			return m_hObj;
		}

	private:
		HDC m_hdc = nullptr;
		HGDIOBJ m_hObj = nullptr;
		HGDIOBJ m_holdObj = nullptr;
		bool m_isShared = false;
	};

	/**
	 * @brief Performs double-buffered painting using a memory DC and a custom paint function.
	 *
	 * Allocates and manages an off-screen buffer via `BufferData`, clips to the paint region,
	 * executes the provided paint function, and blits the result to the target DC.
	 *
	 * @tparam      T           Control data type containing a `m_bufferData` member.
	 * @tparam      PaintFunc   Callable object (lambda or function) that performs painting.
	 * @param[in]   ctrlData    Reference to control-specific data (must contain `m_bufferData`).
	 * @param[in]   hdc         Target device context.
	 * @param[in]   ps          Paint structure from `BeginPaint`.
	 * @param[in]   paintFunc   Custom paint routine.
	 * @param[in]   rcClient    Client rectangle of the control.
	 *
	 * @see BufferData
	 */
	template<typename T, typename PaintFunc>
	inline void PaintWithBuffer(
		T& ctrlData,
		HDC hdc,
		const PAINTSTRUCT& ps,
		PaintFunc&& paintFunc,
		const RECT& rcClient
	)
	{
		auto& bufferData = ctrlData.m_bufferData;

		if (bufferData.ensureBuffer(hdc, rcClient))
		{
			const auto& hMemDC = bufferData.getHMemDC();
			const int savedState = ::SaveDC(hMemDC);

			::IntersectClipRect(
				hMemDC,
				ps.rcPaint.left, ps.rcPaint.top,
				ps.rcPaint.right, ps.rcPaint.bottom
			);

			std::forward<PaintFunc>(paintFunc)();

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

	/**
	 * @brief Overload of `paintWithBuffer` that automatically retrieves the client rect.
	 *
	 * Extracts the client rectangle from the window handle,
	 * then forwards it to the main `paintWithBuffer` implementation.
	 *
	 * @tparam      T           Control data type containing a `m_bufferData` member.
	 * @tparam      PaintFunc   Callable object (lambda or function) that performs painting.
	 * @param[in]   ctrlData    Reference to control-specific data (must contain `m_bufferData`).
	 * @param[in]   hdc         Target device context.
	 * @param[in]   ps          Paint structure from `BeginPaint`.
	 * @param[in]   paintFunc   Custom paint routine.
	 * @param[in]   hWnd        Handle to the control window.
	 *
	 * @see dmlib_paint::PaintWithBuffer(const T&, HDC, const PAINTSTRUCT&, PaintFunc&&, const RECT&)
	 */
	template<typename T, typename PaintFunc>
	inline void PaintWithBuffer(
		T& ctrlData,
		HDC hdc,
		const PAINTSTRUCT& ps,
		PaintFunc&& paintFunc,
		HWND hWnd
	)
	{
		RECT rcClient{};
		::GetClientRect(hWnd, &rcClient);

		dmlib_paint::PaintWithBuffer(ctrlData, hdc, ps, std::forward<PaintFunc>(paintFunc), rcClient);
	}

	/// Paints a rounded rectangle using the specified pen and brush.
	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width, int height) noexcept;

	/**
	 * @brief Paints a rectangle using the specified pen and brush.
	 *
	 * Draws a rectangle defined by `rect`, using the provided pen (`hpen`) and brush (`hBrush`)
	 * for the edge and fill, respectively. Preserves previous GDI object selections.
	 * Forwards to `dmlib_paint::paintRoundRect` with `width` and `height` parameters with `0` value.
	 *
	 * @param[in]   hdc     Handle to the device context.
	 * @param[in]   rect    Rectangle bounds for the shape.
	 * @param[in]   hpen    Pen used to draw the edge.
	 * @param[in]   hBrush  Brush used to inner fill.
	 *
	 * @see dmlib_paint::paintRoundRect()
	 */
	inline void paintRect(
		HDC hdc,
		const RECT& rect,
		HPEN hpen,
		HBRUSH hBrush
	) noexcept
	{
		dmlib_paint::paintRoundRect(hdc, rect, hpen, hBrush, 0, 0);
	}

	/**
	 * @brief Paints an unfilled rounded rectangle (frame only).
	 *
	 * Forwards to `dmlib_paint::paintRoundRect` and uses a `NULL_BRUSH`
	 * to omit the inner fill, drawing only the rounded frame.
	 *
	 * @param[in]   hdc     Handle to the device context.
	 * @param[in]   rect    Rectangle bounds for the frame.
	 * @param[in]   hpen    Pen used to draw the edge.
	 * @param[in]   width   Horizontal corner radius.
	 * @param[in]   height  Vertical corner radius.
	 *
	 * @see dmlib_paint::paintRoundRect()
	 */
	inline void paintRoundFrameRect(
		HDC hdc,
		const RECT& rect,
		HPEN hpen,
		int width,
		int height
	) noexcept
	{
		dmlib_paint::paintRoundRect(hdc, rect, hpen, static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), width, height);
	}

	/**
	 * @brief Paints an unfilled rectangle (frame only).
	 *
	 * Forwards to `dmlib_paint::paintRoundRect` and uses a `NULL_BRUSH`
	 * to omit the inner fill with `width` and `height` parameters with `0` value
	 * to draw only the frame.
	 *
	 * @param[in]   hdc     Handle to the device context.
	 * @param[in]   rect    Rectangle bounds for the frame.
	 * @param[in]   hpen    Pen used to draw the edge.
	 *
	 * @see dmlib_paint::paintRoundRect()
	 */
	inline void paintFrameRect(HDC hdc, const RECT& rect, HPEN hpen) noexcept
	{
		dmlib_paint::paintRoundRect(hdc, rect, hpen, static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH)), 0, 0);
	}

	/**
	 * @brief Checks whether a RECT defines a non-empty, valid area.
	 *
	 * @param[in] rc The rectangle to validate.
	 * @return `true`  If rc has positive width and height (right > left and bottom > top).
	 * @return `false` Otherwise.
	 */
	[[nodiscard]] inline bool isRectValid(const RECT& rc) noexcept
	{
		return (rc.right > rc.left && rc.bottom > rc.top);
	}
} // namespace dmlib_paint
