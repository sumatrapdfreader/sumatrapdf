/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <utility>

namespace ZXing {

/**
 * ScopeExit is a trivial helper meant to be later replaced by std::scope_exit from library fundamentals TS v3.
 */

template <typename EF>
class ScopeExit
{
	EF fn;
public:
	ScopeExit(EF&& fn) : fn(std::move(fn)) {}
	~ScopeExit() { fn(); }
};

template <class EF>
ScopeExit(EF) -> ScopeExit<EF>;

/**
 * The SCOPE_EXIT macro is eliminating the need to give the object a name.
 * Example usage:
 *   SCOPE_EXIT([]{ printf("exiting scope"); });
 */

#define SCOPE_EXIT_CAT2(x, y) x##y
#define SCOPE_EXIT_CAT(x, y) SCOPE_EXIT_CAT2(x, y)
#define SCOPE_EXIT const auto SCOPE_EXIT_CAT(scopeExit_, __COUNTER__) = ScopeExit

} // ZXing

