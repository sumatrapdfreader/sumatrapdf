/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMSymbolInfo.h"

#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <cstddef>
#include <stdexcept>

namespace ZXing::DataMatrix {

static constexpr const SymbolInfo PROD_SYMBOLS[] = {
	{ false, 3, 5, 8, 8, 1 },
	{ false, 5, 7, 10, 10, 1 },
	{ true, 5, 7, 16, 6, 1 },
	{ false, 8, 10, 12, 12, 1 },
	{ true, 10, 11, 14, 6, 2 },
	{ false, 12, 12, 14, 14, 1 },
	{ true, 16, 14, 24, 10, 1 },

	{ false, 18, 14, 16, 16, 1 },
	{ false, 22, 18, 18, 18, 1 },
	{ true, 22, 18, 16, 10, 2 },
	{ false, 30, 20, 20, 20, 1 },
	{ true, 32, 24, 16, 14, 2 },
	{ false, 36, 24, 22, 22, 1 },
	{ false, 44, 28, 24, 24, 1 },
	{ true, 49, 28, 22, 14, 2 },

	{ false, 62, 36, 14, 14, 4 },
	{ false, 86, 42, 16, 16, 4 },
	{ false, 114, 48, 18, 18, 4 },
	{ false, 144, 56, 20, 20, 4 },
	{ false, 174, 68, 22, 22, 4 },

	{ false, 204, 84, 24, 24, 4, 102, 42 },
	{ false, 280, 112, 14, 14, 16, 140, 56 },
	{ false, 368, 144, 16, 16, 16, 92, 36 },
	{ false, 456, 192, 18, 18, 16, 114, 48 },
	{ false, 576, 224, 20, 20, 16, 144, 56 },
	{ false, 696, 272, 22, 22, 16, 174, 68 },
	{ false, 816, 336, 24, 24, 16, 136, 56 },
	{ false, 1050, 408, 18, 18, 36, 175, 68 },
	{ false, 1304, 496, 20, 20, 36, 163, 62 },
	{ false, 1558, 620, 22, 22, 36, -1, 62 },
};

static const SymbolInfo* s_symbols = PROD_SYMBOLS;
static ZXING_IF_NOT_TEST(const) size_t s_symbolCount = Size(PROD_SYMBOLS);

#ifdef ZXING_BUILD_FOR_TEST

ZXING_EXPORT_TEST_ONLY
void OverrideSymbolSet(const SymbolInfo* symbols, size_t count)
{
	s_symbols = symbols;
	s_symbolCount = count;
}

ZXING_EXPORT_TEST_ONLY
void UseDefaultSymbolSet()
{
	s_symbols = PROD_SYMBOLS;
	s_symbolCount = Size(PROD_SYMBOLS);
}

#endif // ZXING_BUILD_FOR_TEST

const SymbolInfo *
SymbolInfo::Lookup(int dataCodewords)
{
	return Lookup(dataCodewords, SymbolShape::NONE);
}

const SymbolInfo *
SymbolInfo::Lookup(int dataCodewords, SymbolShape shape)
{
	return Lookup(dataCodewords, shape, -1, -1, -1, -1);
}

const SymbolInfo *
SymbolInfo::Lookup(int dataCodewords, bool allowRectangular)
{
	return Lookup(dataCodewords, allowRectangular ? SymbolShape::NONE : SymbolShape::SQUARE, -1, -1, -1, -1);
}

const SymbolInfo *
SymbolInfo::Lookup(int dataCodewords, SymbolShape shape, int minWidth, int minHeight, int maxWidth, int maxHeight)
{
	for (size_t i = 0; i < s_symbolCount; ++i) {
		auto& symbol = s_symbols[i];
		if (shape == SymbolShape::SQUARE && symbol._rectangular) {
			continue;
		}
		if (shape == SymbolShape::RECTANGLE && !symbol._rectangular) {
			continue;
		}
		if (minWidth >= 0 && minHeight >= 0 && (symbol.symbolWidth() < minWidth || symbol.symbolHeight() < minHeight)) {
			continue;
		}
		if (maxWidth >= 0 && maxHeight >= 0 && (symbol.symbolWidth() > maxWidth || symbol.symbolHeight() > maxHeight)) {
			continue;
		}
		if (dataCodewords <= symbol._dataCapacity) {
			return &symbol;
		}
	}
	return nullptr;
}

int
SymbolInfo::horizontalDataRegions() const
{
	switch (_dataRegions) {
	case 1:  return 1;
	case 2:  return 2;
	case 4:  return 2;
	case 16: return 4;
	case 36: return 6;
	default: throw std::out_of_range("Cannot handle this number of data regions");
	}
}

int
SymbolInfo::verticalDataRegions() const {
	switch (_dataRegions) {
	case 1:  return 1;
	case 2:  return 1;
	case 4:  return 2;
	case 16: return 4;
	case 36: return 6;
	default: throw std::out_of_range("Cannot handle this number of data regions");
	}
}

} // namespace ZXing::DataMatrix
