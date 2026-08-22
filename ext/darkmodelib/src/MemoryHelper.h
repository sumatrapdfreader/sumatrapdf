// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#if defined(_DARKMODELIB_CUSTOM_MEM) && (_DARKMODELIB_CUSTOM_MEM == 0x002)
#include <array>
#endif

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace dmlib_mem
{
	// NOLINTBEGIN(modernize-use-constraints) // keep c++17 compatibility for now

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26485) // Expression 'array-name': No array to pointer decay (bounds.3).
#endif

	/// Nothrow variant of std::make_unique
	template <class T, class... Args, std::enable_if_t<!std::is_array_v<T>, int> = 0>
	[[nodiscard]] inline std::unique_ptr<T> make_unique_nothrow(Args&&... args) noexcept(noexcept(std::remove_reference_t<T>(std::forward<Args>(args)...)))
	{
		return std::unique_ptr<T>(new (std::nothrow) T(std::forward<Args>(args)...)); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	/// Nothrow variant of std::make_unique
	template <class T, std::enable_if_t<std::is_array_v<T>&& std::extent_v<T> == 0, int> = 0>
	[[nodiscard]] inline std::unique_ptr<T> make_unique_nothrow(const size_t size) noexcept
	{
		using U = std::remove_extent_t<T>;
		return std::unique_ptr<T>(new (std::nothrow) U[size]());
	}

	/// Nothrow variant of std::make_unique
	template <class T, class... Args, std::enable_if_t<std::extent_v<T> != 0, int> = 0>
	void make_unique_nothrow(Args&&...) = delete;

	// NOLINTEND(modernize-use-constraints)

#if defined(_DARKMODELIB_CUSTOM_MEM) && (_DARKMODELIB_CUSTOM_MEM == 0x002)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26429) // Symbol is never tested for nullness, it can be marked as gsl::not_null.
#pragma warning(disable: 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable: 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable: 26482) // Only index into arrays using constant expressions.
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-unique-ptr-array-access"
#endif

	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-*,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

	/// Basic implementation of string buffer with no exeptions as replacement for std::wstring with only necessary methods
	class BufferWString
	{
	public:
		BufferWString() noexcept = default;
		explicit BufferWString(size_t count, wchar_t ch = L'\0') noexcept
		{
			if (count <= kStackCapacity)
			{
				m_size = count;
				for (size_t i = 0; i < m_size; ++i)
				{
					m_storage.stackBuf[i] = ch;
				}
				m_storage.stackBuf[m_size] = L'\0';

				return;
			}

			if (auto newPtr = make_unique_nothrow<wchar_t[]>(count + 1);
				newPtr != nullptr)
			{
				for (size_t i = 0; i < count; ++i)
				{
					newPtr[i] = ch;
				}
				newPtr[count] = L'\0';
				::new (static_cast<void*>(&m_storage.heapBuf)) std::unique_ptr<wchar_t[]>(std::move(newPtr));
				m_size = count;
				m_capacity = count;
			}
		}

		~BufferWString() noexcept
		{
			if (!isStack())
			{
				m_storage.heapBuf.reset();
			}
		}

		[[nodiscard]] bool isValid() const noexcept
		{
			return isStack() || m_storage.heapBuf != nullptr;
		}

		[[nodiscard]] bool empty() const noexcept
		{
			return m_size == 0;
		}

		BufferWString(const BufferWString&) = delete;
		BufferWString& operator=(const BufferWString&) = delete;
		BufferWString(BufferWString&& other) = delete;
		BufferWString& operator=(BufferWString&&) = delete;

		[[nodiscard]] wchar_t* data() noexcept
		{
			return (isStack() || m_storage.heapBuf == nullptr) ? m_storage.stackBuf.data() : m_storage.heapBuf.get();
		}

		[[nodiscard]] const wchar_t* data() const noexcept
		{
			return (isStack() || m_storage.heapBuf == nullptr) ? m_storage.stackBuf.data() : m_storage.heapBuf.get();
		}

		[[nodiscard]] const wchar_t* c_str() const noexcept
		{
			return (isStack() || m_storage.heapBuf == nullptr) ? m_storage.stackBuf.data() : m_storage.heapBuf.get();
		}

		[[nodiscard]] size_t size() const noexcept
		{
			return m_size;
		}

		[[nodiscard]] size_t length() const noexcept
		{
			return m_size;
		}

		[[nodiscard]] size_t capacity() const noexcept
		{
			return m_capacity;
		}

		bool resize(size_t count, wchar_t ch = L'\0') noexcept
		{
			if (count > m_capacity)
			{
				auto newPtr = make_unique_nothrow<wchar_t[]>(count + 1);
				if (newPtr == nullptr)
				{
					return false;
				}

				const wchar_t* currentBuffer = data();
				for (size_t i = 0; i < m_size; ++i)
				{
					newPtr[i] = currentBuffer[i];
				}

				if (isStack())
				{
					::new (static_cast<void*>(&m_storage.heapBuf)) std::unique_ptr<wchar_t[]>(std::move(newPtr));
				}
				else
				{
					m_storage.heapBuf = std::move(newPtr);
				}
				m_capacity = count;
			}

			wchar_t* currentBuffer = data();
			if (count > m_size)
			{
				for (size_t i = m_size; i < count; ++i)
				{
					currentBuffer[i] = ch;
				}
			}
			currentBuffer[count] = L'\0';

			m_size = count;
			return true;
		}

	private:
		static constexpr size_t kStackCapacity = 128;

		union Storage
		{
			std::array<wchar_t, kStackCapacity + 1> stackBuf{};
			std::unique_ptr<wchar_t[]> heapBuf;

			Storage() noexcept = default;

			Storage(const Storage&) = delete;
			Storage& operator=(const Storage&) = delete;
			Storage(Storage&& other) = delete;
			Storage& operator=(Storage&&) = delete;

			~Storage() noexcept // must destroy it in ~BufferWString()
			{}
		};

		Storage m_storage;
		size_t m_size = 0;
		size_t m_capacity = kStackCapacity;

		[[nodiscard]] bool isStack() const noexcept
		{
			return m_capacity == kStackCapacity;
		}
	};

	// NOLINTEND(cppcoreguidelines-pro-bounds-*,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif // defined(_DARKMODELIB_CUSTOM_MEM) && (_DARKMODELIB_CUSTOM_MEM == 0x002)

} // namespace dmlib_mem
