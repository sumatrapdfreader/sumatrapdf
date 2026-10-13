/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2017 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

// MSVC has the <bit> header but then warns about including it.
// We check for _MSVC_LANG here as well, so client code is depending on /Zc:__cplusplus
#if __has_include(<bit>) && (__cplusplus > 201703L || (defined(_MSVC_LANG) && _MSVC_LANG > 201703L))
#include <bit>
#if __cplusplus > 201703L && defined(__ANDROID__) // NDK 25.1.8937393 has the implementation but fails to advertise it
#define __cpp_lib_bitops 201907L
#endif
#elif defined(_MSC_VER)
// accoring to #863 MSVC defines __cpp_lib_bitops even when <bit> it not included and bitops are not available
#undef __cpp_lib_bitops
#endif

#if defined(__clang__) || defined(__GNUC__)
#define ZX_HAS_GCC_BUILTINS
#elif defined(_MSC_VER) && !defined(_M_ARM) && !defined(_M_ARM64)
#include <intrin.h>
#define ZX_HAS_MSC_BUILTINS
#endif

namespace ZXing::BitHacks {

/**
* The code below is taken from https://graphics.stanford.edu/~seander/bithacks.html
* All credits go to Sean Eron Anderson and other authors mentioned in that page.
*/

/// <summary>
/// Compute the number of zero bits on the left.
/// </summary>
template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline int NumberOfLeadingZeros(T x)
{
#ifdef __cpp_lib_bitops
	return std::countl_zero(static_cast<std::make_unsigned_t<T>>(x));
#else
	if constexpr (sizeof(x) <= 4) {
		static_assert(sizeof(x) == 4, "NumberOfLeadingZeros not implemented for 8 and 16 bit ints.");
		if (x == 0)
			return 32;
#ifdef ZX_HAS_GCC_BUILTINS
		return __builtin_clz(x);
#elif defined(ZX_HAS_MSC_BUILTINS)
		unsigned long where;
		if (_BitScanReverse(&where, x))
			return 31 - static_cast<int>(where);
		return 32;
#else
		int n = 0;
		if ((x & 0xFFFF0000) == 0) { n = n + 16; x = x << 16; }
		if ((x & 0xFF000000) == 0) { n = n + 8; x = x << 8; }
		if ((x & 0xF0000000) == 0) { n = n + 4; x = x << 4; }
		if ((x & 0xC0000000) == 0) { n = n + 2; x = x << 2; }
		if ((x & 0x80000000) == 0) { n = n + 1; }
		return n;
#endif
	} else {
		if (x == 0)
			return 64;
#ifdef ZX_HAS_GCC_BUILTINS
		return __builtin_clzll(x);
#else // including ZX_HAS_MSC_BUILTINS
		int n = NumberOfLeadingZeros(static_cast<uint32_t>(x >> 32));
		if (n == 32)
			n += NumberOfLeadingZeros(static_cast<uint32_t>(x));
		return n;
#endif
	}
#endif
}

/// <summary>
/// Compute the number of zero bits on the right.
/// </summary>
template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline int NumberOfTrailingZeros(T v)
{
#ifdef __cpp_lib_bitops
	return std::countr_zero(static_cast<std::make_unsigned_t<T>>(v));
#else
	if constexpr (sizeof(v) <= 4) {
		static_assert(sizeof(v) == 4, "NumberOfTrailingZeros not implemented for 8 and 16 bit ints.");
#ifdef ZX_HAS_GCC_BUILTINS
		return v == 0 ? 32 : __builtin_ctz(v);
#elif defined(ZX_HAS_MSC_BUILTINS)
		unsigned long where;
		if (_BitScanForward(&where, v))
			return static_cast<int>(where);
		return 32;
#else
		int c = 32;
		v &= -int32_t(v);
		if (v) c--;
		if (v & 0x0000FFFF) c -= 16;
		if (v & 0x00FF00FF) c -= 8;
		if (v & 0x0F0F0F0F) c -= 4;
		if (v & 0x33333333) c -= 2;
		if (v & 0x55555555) c -= 1;
		return c;
#endif
	} else {
#ifdef ZX_HAS_GCC_BUILTINS
		return v == 0 ? 64 : __builtin_ctzll(v);
#else // including ZX_HAS_MSC_BUILTINS
		int n = NumberOfTrailingZeros(static_cast<uint32_t>(v));
		if (n == 32)
			n += NumberOfTrailingZeros(static_cast<uint32_t>(v >> 32));
		return n;
#endif
	}
#endif
}

inline uint32_t Reverse(uint32_t v)
{
#if 0
	return __builtin_bitreverse32(v);
#else
	v = ((v >> 1) & 0x55555555) | ((v & 0x55555555) << 1);
	// swap consecutive pairs
	v = ((v >> 2) & 0x33333333) | ((v & 0x33333333) << 2);
	// swap nibbles ...
	v = ((v >> 4) & 0x0F0F0F0F) | ((v & 0x0F0F0F0F) << 4);
	// swap bytes
	v = ((v >> 8) & 0x00FF00FF) | ((v & 0x00FF00FF) << 8);
	// swap 2-byte long pairs
	v = (v >> 16) | (v << 16);
	return v;
#endif
}

inline int CountBitsSet(uint32_t v)
{
#ifdef __cpp_lib_bitops
	return std::popcount(v);
#elif defined(ZX_HAS_GCC_BUILTINS)
	return __builtin_popcount(v);
#else
	v = v - ((v >> 1) & 0x55555555);							// reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);				// temp
	return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;	// count
#endif
}

// this is the same as log base 2 of v
inline int HighestBitSet(uint32_t v)
{
	return 31 - NumberOfLeadingZeros(v);
}

// shift a whole array of bits by offset bits to the right (thinking of the array as a contiguous stream of bits
// starting with the LSB of the first int and ending with the MSB of the last int, this is actually a left shift)
template <typename T>
void ShiftRight(std::vector<T>& bits, std::size_t offset)
{
	assert(offset < sizeof(T) * 8);

	if (offset == 0 || bits.empty())
		return;

	std::size_t leftOffset = sizeof(T) * 8 - offset;
	for (std::size_t i = 0; i < bits.size() - 1; ++i) {
		bits[i] = (bits[i] >> offset) | (bits[i + 1] << leftOffset);
	}
	bits.back() >>= offset;
}

// reverse a whole array of bits. padding is the number of 'dummy' bits at the end of the array
template <typename T>
void Reverse(std::vector<T>& bits, std::size_t padding)
{
	static_assert(sizeof(T) == sizeof(uint32_t), "Reverse only implemented for 32 bit types");

	// reverse all int's first (reversing the ints in the array and the bits in the ints at the same time)
	auto first = bits.begin(), last = bits.end();
	for (; first < --last; ++first) {
		auto t = *first;
		*first = BitHacks::Reverse(*last);
		*last = BitHacks::Reverse(t);
	}
	if (first == last)
		*last = BitHacks::Reverse(*last);

	// now correct the int's if the bit size isn't a multiple of 32
	ShiftRight(bits, padding);
}

// use to avoid "load of misaligned address" when using a simple type cast
template <typename T>
T LoadU(const void* ptr)
{
	static_assert(std::is_integral<T>::value, "T must be an integer");
	T res;
	memcpy(&res, ptr, sizeof(T));
	return res;
}

} // namespace ZXing::BitHacks
