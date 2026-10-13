/*
* Copyright 2017 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Error.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <string>
#include <utility>

namespace ZXing {

template <class T, class U>
constexpr T narrow_cast(U&& u) noexcept {
	return static_cast<T>(std::forward<U>(u));
}

template <typename Container, typename Value>
auto Find(Container& c, const Value& v) -> decltype(std::begin(c)) {
	return std::find(std::begin(c), std::end(c), v);
}

template <typename Container, typename Predicate>
auto FindIf(Container& c, Predicate p) -> decltype(std::begin(c)) {
	return std::find_if(std::begin(c), std::end(c), p);
}

template <typename Container, typename Value>
auto Contains(const Container& c, const Value& v) -> decltype(std::begin(c), bool()){
	return Find(c, v) != std::end(c);
}

template <typename ListType, typename Value>
auto Contains(const std::initializer_list<ListType>& c, const Value& v) -> decltype(std::begin(c), bool()){
	return Find(c, v) != std::end(c);
}

inline bool Contains(const char* str, char c) {
	return strchr(str, c) != nullptr;
}

template <template <typename...> typename C, typename... Ts>
auto FirstOrDefault(C<Ts...>&& container)
{
	return container.empty() ? typename C<Ts...>::value_type() : std::move(container.front());
}

template <typename Iterator, typename Value = typename std::iterator_traits<Iterator>::value_type, typename Op = std::plus<Value>>
Value Reduce(Iterator b, Iterator e, Value v = Value{}, Op op = {}) {
	// std::reduce() first sounded like a better implementation because it is not implemented as a strict left-fold
	// operation, meaning the order of the op-application is not specified. This sounded like an optimization opportunity
	// but it turns out that for this use case it actually does not make a difference (falsepositives runtime). And
	// when tested with a large std::vector<uint16_t> and proper autovectorization (e.g. clang++ -O2) it turns out that
	// std::accumulate can be twice as fast as std::reduce.
	return std::accumulate(b, e, v, op);
}

template <typename Container, typename Value = typename Container::value_type, typename Op = std::plus<Value>>
Value Reduce(const Container& c, Value v = Value{}, Op op = {}) {
	return Reduce(std::begin(c), std::end(c), v, op);
}

// see C++20 ssize
template <class Container>
constexpr auto Size(const Container& c) -> decltype(c.size(), int()) {
	return narrow_cast<int>(c.size());
}

template <class T, std::size_t N>
constexpr int Size(const T (&)[N]) noexcept {
	return narrow_cast<int>(N);
}

template <typename Container, typename Value>
int IndexOf(const Container& c, const Value& v) {
	auto i = Find(c, v);
	return i == std::end(c) ? -1 : narrow_cast<int>(std::distance(std::begin(c), i));
}

inline int IndexOf(const char* str, char c) {
	auto s = strchr(str, c);
	return s != nullptr ? narrow_cast<int>(s - str) : -1;
}

template <typename Container, typename Value, class UnaryOp>
Value TransformReduce(const Container& c, Value s, UnaryOp op) {
	for (const auto& v : c)
		s += op(v);
	return s;
}

template <typename T = char>
T ToDigit(int i)
{
	if (i < 0 || i > 9)
		throw FormatError("Invalid digit value");
	return static_cast<T>('0' + i);
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
std::string ToString(T val, int len)
{
	std::string result(len--, '0');
	if (val < 0)
		throw FormatError("Invalid value");
	for (; len >= 0 && val != 0; --len, val /= 10)
		result[len] = '0' + val % 10;
	if (val)
		throw FormatError("Invalid value");
	return result;
}

template <typename T>
void UpdateMin(T& min, T val)
{
	min = std::min(min, val);
}

template <typename T>
void UpdateMax(T& max, T val)
{
	max = std::max(max, val);
}

template <typename T>
void UpdateMinMax(T& min, T& max, T val)
{
	min = std::min(min, val);
	max = std::max(max, val);

	// Note: the above code is not equivalent to
	//    if (val < min)        min = val;
	//    else if (val > max)   max = val;
	// It is basically the same but without the 'else'. For the 'else'-variant to work,
	// both min and max have to be initialized with a value that is part of the sequence.
	// Also it turns out clang and gcc can vectorize the code above but not the code below.
}

} // ZXing
