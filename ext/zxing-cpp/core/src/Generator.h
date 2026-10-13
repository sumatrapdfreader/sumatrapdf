/*
 * Copyright 2022 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef __ANDROID__
#include <android/ndk-version.h>
#endif

#ifdef __cpp_impl_coroutine
#if defined __ANDROID__ && __NDK_MAJOR__ < 26
// NDK 25.1.8937393 can compile this code with c++20 but needs a few tweaks:
#include <experimental/coroutine>
namespace std {
	using experimental::suspend_always;
	using experimental::coroutine_handle;
	struct default_sentinel_t {};
}
#else
#include <concepts>
#include <coroutine>
#endif

#include <optional>
#include <iterator>

// this code is based on https://en.cppreference.com/w/cpp/coroutine/coroutine_handle#Example
// but modified trying to prevent accidental copying of generated objects

#if defined __ANDROID__ && __NDK_MAJOR__ < 26
template <class T>
#else
template <std::movable T>
#endif
class Generator
{
public:
	struct promise_type
	{
		Generator<T> get_return_object() { return Generator{Handle::from_promise(*this)}; }
		static std::suspend_always initial_suspend() noexcept { return {}; }
		static std::suspend_always final_suspend() noexcept { return {}; }
		std::suspend_always yield_value(T&& value) noexcept
		{
			current_value = std::move(value);
			return {};
		}
//		void return_value(T&& value) noexcept { current_value = std::move(value); }
		static void return_void() {} // required to compile in VisualStudio, no idea why clang/gcc are happy without
		// Disallow co_await in generator coroutines.
		void await_transform() = delete;
		[[noreturn]] static void unhandled_exception() { throw; }

		std::optional<T> current_value;
	};

	using Handle = std::coroutine_handle<promise_type>;

	explicit Generator(const Handle coroutine) : _coroutine{coroutine} {}

	Generator() = default;
	~Generator()
	{
		if (_coroutine)
			_coroutine.destroy();
	}

	Generator(const Generator&) = delete;
	Generator& operator=(const Generator&) = delete;

	Generator(Generator&& other) noexcept : _coroutine{other._coroutine} { other._coroutine = {}; }
//	Generator& operator=(Generator&& other) noexcept
//	{
//		if (this != &other) {
//			if (_coroutine)
//				_coroutine.destroy();

//			_coroutine = other._coroutine;
//			other._coroutine = {};
//		}
//		return *this;
//	}

	// Range-based for loop support.
	class Iter
	{
	public:
		void operator++() { _coroutine.resume(); }
		T&& operator*() const { return std::move(*_coroutine.promise().current_value); }
		bool operator==(std::default_sentinel_t) const { return !_coroutine || _coroutine.done(); }

		explicit Iter(const Handle coroutine) : _coroutine{coroutine} {}

	private:
		Handle _coroutine;
	};

	Iter begin()
	{
		if (_coroutine)
			_coroutine.resume();

		return Iter{_coroutine};
	}
	std::default_sentinel_t end() { return {}; }

private:
	Handle _coroutine;
};

#endif

/*
usage example:

template <std::integral T>
Generator<T> range(T first, const T last)
{
	while (first < last)
		co_yield first++;
}

int main()
{
	for (const char i : range(65, 91))
		std::cout << i << ' ';
	std::cout << '\n';
}
*/
