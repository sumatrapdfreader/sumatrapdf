// SPDX-License-Identifier: MIT

// Copyright (c) 2025 ozone10
// Licensed under the MIT license.

// This file is part of darkmodelib library.

// This file is a modified version of IatHook.h from the win32-darkmode project
// https://github.com/ysc3839/win32-darkmode

// This file contains code from
// https://github.com/stevemk14ebr/PolyHook_2_0/blob/master/sources/IatHook.cpp
// which is licensed under the MIT license.
// See LICENSE-PolyHook_2_0 for more information.

#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
	# pragma warning(push)
	# pragma warning(disable: 26429) // Symbol is never tested for nullness.
	# pragma warning(disable: 26472) // Don't use a static_cast for arithmetic conversions.
	# pragma warning(disable: 26481) // Don't use pointer arithmetic.
	# pragma warning(disable: 26485) // No array to pointer decay.
#elif defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif

namespace iat_hook
{
template <typename T, typename T1, typename T2>
	constexpr auto RVA2VA(T1 base, T2 rva) noexcept -> T
{
	return reinterpret_cast<T>(reinterpret_cast<ULONG_PTR>(base) + rva);
}

template <typename T>
	constexpr auto DataDirectoryFromModuleBase(void* moduleBase, size_t entryID) noexcept -> T
{
	const auto* dosHdr = static_cast<PIMAGE_DOS_HEADER>(moduleBase);
	const auto* ntHdr = RVA2VA<PIMAGE_NT_HEADERS>(moduleBase, static_cast<DWORD>(dosHdr->e_lfanew));
	const auto* dataDir = ntHdr->OptionalHeader.DataDirectory;
	return RVA2VA<T>(moduleBase, dataDir[entryID].VirtualAddress);
}

	inline PIMAGE_THUNK_DATA FindAddressByName(void* moduleBase, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, const char* funcName) noexcept
{
	for (; impName->u1.Ordinal != 0; ++impName, ++impAddr)
	{
		if (IMAGE_SNAP_BY_ORDINAL(impName->u1.Ordinal))
		{
			continue;
		}

			if (const auto* imgImport = RVA2VA<PIMAGE_IMPORT_BY_NAME>(moduleBase, impName->u1.AddressOfData);
				std::strcmp(reinterpret_cast<const char*>(imgImport->Name), funcName) != 0)
		{
			continue;
		}
		return impAddr;
	}
	return nullptr;
}

	inline PIMAGE_THUNK_DATA FindAddressByOrdinal([[maybe_unused]] void* /*moduleBase*/, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, uint16_t ordinal) noexcept
{
	for (; impName->u1.Ordinal != 0; ++impName, ++impAddr)
	{
		if (IMAGE_SNAP_BY_ORDINAL(impName->u1.Ordinal) && IMAGE_ORDINAL(impName->u1.Ordinal) == ordinal)
		{
			return impAddr;
		}
	}
	return nullptr;
}

	inline PIMAGE_THUNK_DATA FindIatThunkInModule(void* moduleBase, const char* dllName, const char* funcName) noexcept
{
		const auto* imports = DataDirectoryFromModuleBase<PIMAGE_IMPORT_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_IMPORT);
	for (; imports->Name != 0; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->Name), dllName) != 0)
		{
			continue;
		}

		auto* origThunk = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->OriginalFirstThunk);
		auto* thunk = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->FirstThunk);
		return FindAddressByName(moduleBase, origThunk, thunk, funcName);
	}
	return nullptr;
}

	inline PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void* moduleBase, const char* dllName, const char* funcName) noexcept
{
		const auto* imports = DataDirectoryFromModuleBase<PIMAGE_DELAYLOAD_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
	for (; imports->DllNameRVA != 0; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->DllNameRVA), dllName) != 0)
		{
			continue;
		}

		auto* impName = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportNameTableRVA);
		auto* impAddr = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportAddressTableRVA);
		return FindAddressByName(moduleBase, impName, impAddr, funcName);
	}
	return nullptr;
}

	inline PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void* moduleBase, const char* dllName, uint16_t ordinal) noexcept
{
		const auto* imports = DataDirectoryFromModuleBase<PIMAGE_DELAYLOAD_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
	for (; imports->DllNameRVA != 0; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->DllNameRVA), dllName) != 0)
		{
			continue;
		}

		auto* impName = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportNameTableRVA);
		auto* impAddr = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportAddressTableRVA);
		return FindAddressByOrdinal(moduleBase, impName, impAddr, ordinal);
	}
	return nullptr;
}
} // namespace iat_hook

#if defined(_MSC_VER)
# pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
