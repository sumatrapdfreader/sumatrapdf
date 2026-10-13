/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "BarcodeFormat.h"

#include "ZXAlgorithms.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace ZXing {

struct BarcodeFormatName
{
	BarcodeFormat format;
	std::string_view name;
};

static BarcodeFormatName NAMES[] = {
	{BarcodeFormat::None, "None"},
	{BarcodeFormat::Aztec, "Aztec"},
	{BarcodeFormat::Codabar, "Codabar"},
	{BarcodeFormat::Code39, "Code39"},
	{BarcodeFormat::Code93, "Code93"},
	{BarcodeFormat::Code128, "Code128"},
	{BarcodeFormat::DataBar, "DataBar"},
	{BarcodeFormat::DataBarExpanded, "DataBarExpanded"},
	{BarcodeFormat::DataBarLimited, "DataBarLimited"},
	{BarcodeFormat::DataMatrix, "DataMatrix"},
	{BarcodeFormat::DXFilmEdge, "DXFilmEdge"},
	{BarcodeFormat::EAN8, "EAN-8"},
	{BarcodeFormat::EAN13, "EAN-13"},
	{BarcodeFormat::ITF, "ITF"},
	{BarcodeFormat::MaxiCode, "MaxiCode"},
	{BarcodeFormat::MicroQRCode, "MicroQRCode"},
	{BarcodeFormat::PDF417, "PDF417"},
	{BarcodeFormat::QRCode, "QRCode"},
	{BarcodeFormat::RMQRCode, "rMQRCode"},
	{BarcodeFormat::UPCA, "UPC-A"},
	{BarcodeFormat::UPCE, "UPC-E"},
	{BarcodeFormat::LinearCodes, "Linear-Codes"},
	{BarcodeFormat::MatrixCodes, "Matrix-Codes"},
};

std::string ToString(BarcodeFormat format)
{
	auto i = FindIf(NAMES, [format](auto& v) { return v.format == format; });
	return i == std::end(NAMES) ? std::string() : std::string(i->name);
}

std::string ToString(BarcodeFormats formats)
{
	if (formats.empty())
		return ToString(BarcodeFormat::None);
	std::string res;
	for (auto f : formats)
		res += ToString(f) + "|";
	return res.substr(0, res.size() - 1);
}

static std::string NormalizeFormatString(std::string_view sv)
{
	std::string str(sv);
	std::transform(str.begin(), str.end(), str.begin(), [](char c) { return (char)std::tolower(c); });
#ifdef __cpp_lib_erase_if
	std::erase_if(str, [](char c) { return Contains("_-[]", c); });
#else
	str.erase(std::remove_if(str.begin(), str.end(), [](char c) { return Contains("_-[]", c); }), str.end());
#endif
	return str;
}

static BarcodeFormat ParseFormatString(const std::string& str)
{
	auto i = FindIf(NAMES, [str](auto& v) { return NormalizeFormatString(v.name) == str; });
	return i == std::end(NAMES) ? BarcodeFormat::None : i->format;
}

BarcodeFormat BarcodeFormatFromString(std::string_view str)
{
	return ParseFormatString(NormalizeFormatString(str));
}

BarcodeFormats BarcodeFormatsFromString(std::string_view str)
{
	auto normalized = NormalizeFormatString(str);
	std::replace_if(
		normalized.begin(), normalized.end(), [](char c) { return Contains(" ,", c); }, '|');
	std::istringstream input(normalized);
	BarcodeFormats res;
	for (std::string token; std::getline(input, token, '|');) {
		if(!token.empty()) {
			auto bc = ParseFormatString(token);
			if (bc == BarcodeFormat::None)
				throw std::invalid_argument("This is not a valid barcode format: " + token);
			res |= bc;
		}
	}
	return res;
}

} // ZXing
