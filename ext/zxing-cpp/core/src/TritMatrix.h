/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Matrix.h"

#include <cstdint>

namespace ZXing {

/**
 * @brief Represent a tri-state value false/true/empty
 */
class Trit
{
public:
	enum value_t : uint8_t {false_v, true_v, empty_v} value = empty_v;
	Trit() = default;
	Trit(bool v) : value(static_cast<value_t>(v)) {}
	operator bool() const { return value == true_v; }
	bool isEmpty() const { return value == empty_v; }
};

using TritMatrix = Matrix<Trit>;

} // ZXing
