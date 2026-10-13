/*
* Copyright 2016 Huy Cuong Nguyen
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace ZXing {

template <typename T>
class Nullable final
{
	bool m_hasValue = false;
	T m_value;

public:
	Nullable() = default;
	Nullable(const T &value) : m_hasValue(true), m_value(value) {}
	Nullable(T &&value) noexcept : m_hasValue(true), m_value(std::move(value)) {}
	Nullable(std::nullptr_t) {}
	
	Nullable<T> & operator=(const T &value) {
		m_hasValue = true;
		m_value = value;
		return *this;
	}

	Nullable<T> & operator=(T &&value) noexcept {
		m_hasValue = true;
		m_value = std::move(value);
		return *this;
	}

	Nullable<T> & operator=(std::nullptr_t) {
		m_hasValue = false;
		m_value = T();
		return *this;
	}

	operator T() const {
		if (!m_hasValue) {
			throw std::logic_error("Access empty value");
		}
		return m_value;
	}
	
	bool hasValue() const {
		return m_hasValue;
	}
	
	const T & value() const {
		return m_value;
	}
	
	T & value() {
		return m_value;
	}

	friend inline bool operator==(const Nullable &a, const Nullable &b) {
		return a.m_hasValue == b.m_hasValue && (!a.m_hasValue || a.m_value == b.m_value);
	}
	friend inline bool operator!=(const Nullable &a, const Nullable &b) {
		return !(a == b);
	}

	friend inline bool operator==(const Nullable &a, const T &v) {
		return a.m_hasValue && a.m_value == v;
	}
	friend inline bool operator!=(const Nullable &a, const T &v) {
		return !(a == v);
	}

	friend inline bool operator==(const T &v, const Nullable &a) {
		return a.m_hasValue && a.m_value == v;
	}
	friend inline bool operator!=(const T &v, const Nullable &a) {
		return !(v == a);
	}

	friend inline bool operator==(const Nullable &a, std::nullptr_t) {
		return !a.m_hasValue;
	}
	friend inline bool operator!=(const Nullable &a, std::nullptr_t) {
		return a.m_hasValue;
	}

	friend inline bool operator==(std::nullptr_t, const Nullable &a) {
		return !a.m_hasValue;
	}
	friend inline bool operator!=(std::nullptr_t, const Nullable &a) {
		return a.m_hasValue;
	}

	friend inline void swap(Nullable &a, Nullable &b) {
		using std::swap;
		swap(a.m_value, b.m_value);
		swap(a.m_hasValue, b.m_hasValue);
	}
};

} // ZXing
