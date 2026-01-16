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

namespace dmlib_module
{
	template <typename P>
	inline auto LoadFn(HMODULE handle, P& pointer, const char* name) noexcept -> bool
	{
		if (auto proc = ::GetProcAddress(handle, name); proc != nullptr)
		{
			pointer = reinterpret_cast<P>(reinterpret_cast<INT_PTR>(proc));
			return true;
		}
		return false;
	}

	template <typename P>
	inline auto LoadFn(HMODULE handle, P& pointer, WORD index) noexcept -> bool
	{
		return dmlib_module::LoadFn(handle, pointer, MAKEINTRESOURCEA(index));
	}

	template <typename P, typename D>
	inline auto LoadFn(HMODULE handle, P& pointer, const char* name, D& dummy) noexcept -> bool
	{
		const bool retVal = dmlib_module::LoadFn(handle, pointer, name);
		if (!retVal)
		{
			pointer = static_cast<P>(dummy);
		}
		return retVal;
	}

	class ModuleHandle
	{
	public:
		ModuleHandle() = delete;

		explicit ModuleHandle(const wchar_t* moduleName) noexcept
			: m_hModule(::LoadLibraryExW(moduleName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
		{}

		ModuleHandle(const ModuleHandle&) = delete;
		ModuleHandle& operator=(const ModuleHandle&) = delete;

		ModuleHandle(ModuleHandle&&) = delete;
		ModuleHandle& operator=(ModuleHandle&&) = delete;

		~ModuleHandle()
		{
			if (m_hModule != nullptr)
			{
				::FreeLibrary(m_hModule);
				m_hModule = nullptr;
			}
		}

		[[nodiscard]] HMODULE get() const noexcept
		{
			return m_hModule;
		}

		[[nodiscard]] bool isLoaded() const noexcept
		{
			return m_hModule != nullptr;
		}

	private:
		HMODULE m_hModule = nullptr;
	};
} // namespace dmlib_module
