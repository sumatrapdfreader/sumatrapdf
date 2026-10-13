/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"
#include "QRECB.h"
#include "QRErrorCorrectionLevel.h"
#include "ZXAlgorithms.h"

#include <array>
#include <initializer_list>
#include <vector>

namespace ZXing {

class BitMatrix;

namespace QRCode {

// clang-format off
constexpr std::array<PointI, 32> RMQR_SIZES {
			PointI{43,  7}, {59,  7}, {77,  7}, {99,  7}, {139,  7},
				  {43,  9}, {59,  9}, {77,  9}, {99,  9}, {139,  9},
		{27, 11}, {43, 11}, {59, 11}, {77, 11}, {99, 11}, {139, 11},
		{27, 13}, {43, 13}, {59, 13}, {77, 13}, {99, 13}, {139, 13},
				  {43, 15}, {59, 15}, {77, 15}, {99, 15}, {139, 15},
				  {43, 17}, {59, 17}, {77, 17}, {99, 17}, {139, 17},
};
// clang-format on

/**
* See ISO 18004:2006 Annex D
*/
class Version
{
public:
	Type type() const { return _type; }
	bool isMicro() const { return type() == Type::Micro; }
	bool isRMQR() const { return type() == Type::rMQR; }
	bool isModel1() const { return type() == Type::Model1; }
	bool isModel2() const { return type() == Type::Model2; }

	int versionNumber() const { return _versionNumber; }

	const std::vector<int>& alignmentPatternCenters() const { return _alignmentPatternCenters; }

	int totalCodewords() const { return _totalCodewords; }

	int dimension() const { return SymbolSize(versionNumber(), isMicro() ? Type::Micro : Type::Model2).x; }

	const ECBlocks& ecBlocksForLevel(ErrorCorrectionLevel ecLevel) const { return _ecBlocks[(int)ecLevel]; }

	BitMatrix buildFunctionPattern() const;

	static constexpr PointI SymbolSize(int version, Type type)
	{
		auto square = [](int s) { return PointI(s, s); };
		auto valid = [](int v, int max) { return v >= 1 && v <= max; };

		switch (type) {
		case Type::Model1: return valid(version, 32) ? square(17 + 4 * version) : PointI{};
		case Type::Model2: return valid(version, 40) ? square(17 + 4 * version) : PointI{};
		case Type::Micro: return valid(version, 4) ? square(9 + 2 * version) : PointI{};
		case Type::rMQR: return valid(version, 32) ? RMQR_SIZES[version - 1] : PointI{};
		}

		return {}; // silence warning
	}

	static constexpr bool IsValidSize(PointI size, Type type)
	{
		switch (type) {
		case Type::Model1: return size.x == size.y && size.x >= 21 && size.x <= 145 && (size.x % 4 == 1);
		case Type::Model2: return size.x == size.y && size.x >= 21 && size.x <= 177 && (size.x % 4 == 1);
		case Type::Micro: return size.x == size.y && size.x >= 11 && size.x <= 17 && (size.x % 2 == 1);
		case Type::rMQR:
			return size.x != size.y && size.x & 1 && size.y & 1 && size.x >= 27 && size.x <= 139 && size.y >= 7 && size.y <= 17
				   && IndexOf(RMQR_SIZES, size) != -1;
		}
		return {}; // silence warning
	}
	static bool HasValidSize(const BitMatrix& bitMatrix, Type type);

	static bool HasValidSize(const BitMatrix& matrix)
	{
		return HasValidSize(matrix, Type::Model1) || HasValidSize(matrix, Type::Model2) || HasValidSize(matrix, Type::Micro)
			   || HasValidSize(matrix, Type::rMQR);
	}

	static constexpr int Number(PointI size)
	{
		if (size.x != size.y)
			return IndexOf(RMQR_SIZES, size) + 1;
		if (IsValidSize(size, Type::Model2))
			return (size.x - 17) / 4;
		if (IsValidSize(size, Type::Micro))
			return (size.x - 9) / 2;
		return 0;
	}

	static int Number(const BitMatrix& bitMatrix);

	static const Version* DecodeVersionInformation(int versionBitsA, int versionBitsB = 0);

	static const Version* Model1(int number);
	static const Version* Model2(int number);
	static const Version* Micro(int number);
	static const Version* rMQR(int number);

private:
	int _versionNumber;
	std::vector<int> _alignmentPatternCenters;
	std::array<ECBlocks, 4> _ecBlocks;
	int _totalCodewords;
	Type _type;

	Version(int versionNumber, std::initializer_list<int> alignmentPatternCenters, const std::array<ECBlocks, 4> &ecBlocks);
	Version(int versionNumber, const std::array<ECBlocks, 4>& ecBlocks);
};

} // QRCode
} // ZXing
