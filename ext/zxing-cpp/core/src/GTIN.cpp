/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "GTIN.h"

#include "Barcode.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

namespace ZXing::GTIN {

struct CountryId
{
	uint16_t first;
	uint16_t last;
	const char id[3];
};

bool operator<(const CountryId& lhs, const CountryId& rhs)
{
	return lhs.last < rhs.last;
}

// https://www.gs1.org/standards/id-keys/company-prefix (as of 7 Feb 2022)
// and https://en.wikipedia.org/wiki/List_of_GS1_country_codes
static const CountryId COUNTRIES[] = {
	// clang-format off
	{1, 19, "US"},
	{30, 39, "US"},
	{60, 99, "US"}, // Note 99 coupon identification
	{100, 139, "US"},
	{300, 379, "FR"}, // France (and Monaco according to Wikipedia)
	{380, 380, "BG"}, // Bulgaria
	{383, 383, "SI"}, // Slovenia
	{385, 385, "HR"}, // Croatia
	{387, 387, "BA"}, // Bosnia and Herzegovina
	{389, 389, "ME"}, // Montenegro
	//{390, 390, "XK"}, // (Kosovo according to Wikipedia, unsourced)
	{400, 440, "DE"}, // Germany
	{450, 459, "JP"}, // Japan
	{460, 469, "RU"}, // Russia
	{470, 470, "KG"}, // Kyrgyzstan
	{471, 471, "TW"}, // Taiwan
	{474, 474, "EE"}, // Estonia
	{475, 475, "LV"}, // Latvia
	{476, 476, "AZ"}, // Azerbaijan
	{477, 477, "LT"}, // Lithuania
	{478, 478, "UZ"}, // Uzbekistan
	{479, 479, "LK"}, // Sri Lanka
	{480, 480, "PH"}, // Philippines
	{481, 481, "BY"}, // Belarus
	{482, 482, "UA"}, // Ukraine
	{483, 483, "TM"}, // Turkmenistan
	{484, 484, "MD"}, // Moldova
	{485, 485, "AM"}, // Armenia
	{486, 486, "GE"}, // Georgia
	{487, 487, "KZ"}, // Kazakhstan
	{488, 488, "TJ"}, // Tajikistan
	{489, 489, "HK"}, // Hong Kong
	{490, 499, "JP"}, // Japan
	{500, 509, "GB"}, // UK
	{520, 521, "GR"}, // Greece
	{528, 528, "LB"}, // Lebanon
	{529, 529, "CY"}, // Cyprus
	{530, 530, "AL"}, // Albania
	{531, 531, "MK"}, // North Macedonia
	{535, 535, "MT"}, // Malta
	{539, 539, "IE"}, // Ireland
	{540, 549, "BE"}, // Belgium & Luxembourg
	{560, 560, "PT"}, // Portugal
	{569, 569, "IS"}, // Iceland
	{570, 579, "DK"}, // Denmark (and Faroe Islands and Greenland according to Wikipedia)
	{590, 590, "PL"}, // Poland
	{594, 594, "RO"}, // Romania
	{599, 599, "HU"}, // Hungary
	{600, 601, "ZA"}, // South Africa
	{603, 603, "GH"}, // Ghana
	{604, 604, "SN"}, // Senegal
	{608, 608, "BH"}, // Bahrain
	{609, 609, "MU"}, // Mauritius
	{611, 611, "MA"}, // Morocco
	{613, 613, "DZ"}, // Algeria
	{615, 615, "NG"}, // Nigeria
	{616, 616, "KE"}, // Kenya
	{617, 617, "CM"}, // Cameroon
	{618, 618, "CI"}, // Côte d'Ivoire
	{619, 619, "TN"}, // Tunisia
	{620, 620, "TZ"}, // Tanzania
	{621, 621, "SY"}, // Syria
	{622, 622, "EG"}, // Egypt
	{623, 623, "BN"}, // Brunei
	{624, 624, "LY"}, // Libya
	{625, 625, "JO"}, // Jordan
	{626, 626, "IR"}, // Iran
	{627, 627, "KW"}, // Kuwait
	{628, 628, "SA"}, // Saudi Arabia
	{629, 629, "AE"}, // United Arab Emirates
	{630, 630, "QA"}, // Qatar
	{631, 631, "NA"}, // Namibia
	{640, 649, "FI"}, // Finland
	{690, 699, "CN"}, // China
	{700, 709, "NO"}, // Norway
	{729, 729, "IL"}, // Israel
	{730, 739, "SE"}, // Sweden
	{740, 740, "GT"}, // Guatemala
	{741, 741, "SV"}, // El Salvador
	{742, 742, "HN"}, // Honduras
	{743, 743, "NI"}, // Nicaragua
	{744, 744, "CR"}, // Costa Rica
	{745, 745, "PA"}, // Panama
	{746, 746, "DO"}, // Dominican Republic
	{750, 750, "MX"}, // Mexico
	{754, 755, "CA"}, // Canada
	{759, 759, "VE"}, // Venezuela
	{760, 769, "CH"}, // Switzerland (and Liechtenstein according to Wikipedia)
	{770, 771, "CO"}, // Colombia
	{773, 773, "UY"}, // Uruguay
	{775, 775, "PE"}, // Peru
	{777, 777, "BO"}, // Bolivia
	{778, 779, "AR"}, // Argentina
	{780, 780, "CL"}, // Chile
	{784, 784, "PY"}, // Paraguay
	{786, 786, "EC"}, // Ecuador
	{789, 790, "BR"}, // Brazil
	{800, 839, "IT"}, // Italy (and San Marino and Vatican City according to Wikipedia)
	{840, 849, "ES"}, // Spain (and Andorra according to Wikipedia)
	{850, 850, "CU"}, // Cuba
	{858, 858, "SK"}, // Slovakia
	{859, 859, "CZ"}, // Czechia
	{860, 860, "RS"}, // Serbia
	{865, 865, "MN"}, // Mongolia
	{867, 867, "KP"}, // North Korea
	{868, 869, "TR"}, // Turkey
	{870, 879, "NL"}, // Netherlands
	{880, 880, "KR"}, // South Korea
	{883, 883, "MM"}, // Myanmar
	{884, 884, "KH"}, // Cambodia
	{885, 885, "TH"}, // Thailand
	{888, 888, "SG"}, // Singapore
	{890, 890, "IN"}, // India
	{893, 893, "VN"}, // Vietnam
	{896, 896, "PK"}, // Pakistan
	{899, 899, "ID"}, // Indonesia
	{900, 919, "AT"}, // Austria
	{930, 939, "AU"}, // Australia
	{940, 949, "NZ"}, // New Zealand
	{955, 955, "MY"}, // Malaysia
	{958, 958, "MO"}, // Macao
	//{960, 961, "GB"}, // Global Office - assigned to GS1 UK for GTIN-8 allocations (also 9620-9624999)
	// clang-format on
};

std::string LookupCountryIdentifier(const std::string& GTIN, const BarcodeFormat format)
{
	// Ignore add-on if any
	const auto space = GTIN.find(' ');
	const std::string::size_type size = space != std::string::npos ? space : GTIN.size();

	if (size != 14 && size != 13 && size != 12 && size != 8)
		return {};

	// GTIN-14 leading packaging level indicator
	const int first = size == 14 ? 1 : 0;
	// UPC-A/E implicit leading 0
	const int implicitZero = size == 12 || (size == 8 && format != BarcodeFormat::EAN8) ? 1 : 0;

	if (size != 8 || format != BarcodeFormat::EAN8) { // Assuming following doesn't apply to EAN-8
		// 0000000 Restricted Circulation Numbers; 0000001-0000099 unused to avoid collision with GTIN-8
		int prefix = std::stoi(GTIN.substr(first, 7 - implicitZero));
		if (prefix >= 0 && prefix <= 99)
			return {};

		// 00001-00009 US
		prefix = std::stoi(GTIN.substr(first, 5 - implicitZero));
		if (prefix >= 1 && prefix <= 9)
			return "US";

		// 0001-0009 US
		prefix = std::stoi(GTIN.substr(first, 4 - implicitZero));
		if (prefix >= 1 && prefix <= 9)
			return "US";
	}

	const int prefix = std::stoi(GTIN.substr(first, 3 - implicitZero));

	// Special case EAN-8 for prefix < 100 (GS1 General Specifications Figure 1.4.3-1)
	if (size == 8 && format == BarcodeFormat::EAN8 && prefix <= 99) // Restricted Circulation Numbers
		return {};

	const auto it = std::lower_bound(std::begin(COUNTRIES), std::end(COUNTRIES), CountryId{0, narrow_cast<uint16_t>(prefix), ""});

	return it != std::end(COUNTRIES) && prefix >= it->first && prefix <= it->last ? it->id : std::string();
}

std::string EanAddOn(const Barcode& barcode)
{
	if (!(BarcodeFormat::EAN13 | BarcodeFormat::UPCA | BarcodeFormat::UPCE | BarcodeFormat::EAN8).testFlag(barcode.format()))
		return {};
	auto txt = barcode.bytes().asString();
	auto pos = txt.find(' ');
	return pos != std::string::npos ? std::string(txt.substr(pos + 1)) : std::string();
}

std::string IssueNr(const std::string& ean2AddOn)
{
	if (ean2AddOn.size() != 2)
		return {};

	return std::to_string(std::stoi(ean2AddOn));
}

std::string Price(const std::string& ean5AddOn)
{
	if (ean5AddOn.size() != 5)
		return {};

	std::string currency;
	switch (ean5AddOn.front()) {
	case '0': [[fallthrough]];
	case '1': currency = "GBP \xc2\xa3"; break; // UK
	case '3': currency = "AUD $"; break; // AUS
	case '4': currency = "NZD $"; break; // NZ
	case '5': currency = "USD $"; break; // US
	case '6': currency = "CAD $"; break; // CA
	case '9':
		// Reference: http://www.jollytech.com
		if (ean5AddOn == "90000") // No suggested retail price
			return {};
		if (ean5AddOn == "99991") // Complementary
			return "0.00";
		if (ean5AddOn == "99990")
			return "Used";

		// Otherwise... unknown currency?
		currency = "";
		break;
	default: currency = ""; break;
	}

	int rawAmount = std::stoi(ean5AddOn.substr(1));
	std::stringstream buf;
	buf << currency << std::fixed << std::setprecision(2) << (float(rawAmount) / 100);
	return buf.str();
}

} // namespace ZXing::GTIN
