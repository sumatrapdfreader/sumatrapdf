/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Barcode.h"
#include "BarcodeFormat.h"
#include "ZXAlgorithms.h"

#include <string>

namespace ZXing::GTIN {

template <typename T>
T ComputeCheckDigit(const std::basic_string<T>& digits, bool skipTail = false)
{
	int sum = 0, N = Size(digits) - skipTail;
	for (int i = N - 1; i >= 0; i -= 2)
		sum += digits[i] - '0';
	sum *= 3;
	for (int i = N - 2; i >= 0; i -= 2)
		sum += digits[i] - '0';
	return ToDigit<T>((10 - (sum % 10)) % 10);
}

template <typename T>
bool IsCheckDigitValid(const std::basic_string<T>& s)
{
	return ComputeCheckDigit(s, true) == s.back();
}

/**
 * Evaluate the prefix of the GTIN to estimate the country of origin. See
 * <a href="https://www.gs1.org/standards/id-keys/company-prefix">
 * https://www.gs1.org/standards/id-keys/company-prefix</a> and
 * <a href="https://en.wikipedia.org/wiki/List_of_GS1_country_codes">
 * https://en.wikipedia.org/wiki/List_of_GS1_country_codes</a>.
 *
 * `format` required for EAN-8 (UPC-E assumed if not given)
 */
std::string LookupCountryIdentifier(const std::string& GTIN, const BarcodeFormat format = BarcodeFormat::None);

std::string EanAddOn(const Barcode& barcode);

std::string IssueNr(const std::string& ean2AddOn);
std::string Price(const std::string& ean5AddOn);

} // namespace ZXing::GTIN
