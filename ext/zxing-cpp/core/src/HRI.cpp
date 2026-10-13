/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "HRI.h"

#include "ZXAlgorithms.h"

#include <cmath>
#include <cstring>
#include <string_view>

namespace ZXing {

struct AiInfo
{
	const char aiPrefix[5];
	int8_t _fieldSize;	// if negative, the length is variable and abs(length) give the max size

	bool isVariableLength() const noexcept { return _fieldSize < 0; }
	int fieldSize() const noexcept { return std::abs(_fieldSize); }
	int aiSize() const
	{
		using namespace std::literals;
		if ((aiPrefix[0] == '3' && Contains("1234569", aiPrefix[1])) || aiPrefix == "703"sv || aiPrefix == "723"sv)
			return 4;
		else
			return strlen(aiPrefix);
	}
};

// https://github.com/gs1/gs1-syntax-dictionary 2024-06-10
static const AiInfo aiInfos[] = {
//TWO_DIGIT_DATA_LENGTH
	{ "00", 18 },
	{ "01", 14 },
	{ "02", 14 },

	{ "10", -20 },
	{ "11", 6 },
	{ "12", 6 },
	{ "13", 6 },
	{ "15", 6 },
	{ "16", 6 },
	{ "17", 6 },

	{ "20", 2 },
	{ "21", -20 },
	{ "22", -20 },

	{ "30", -8 },
	{ "37", -8 },

	{ "90", -30 },
	{ "91", -90 },
	{ "92", -90 },
	{ "93", -90 },
	{ "94", -90 },
	{ "95", -90 },
	{ "96", -90 },
	{ "97", -90 },
	{ "98", -90 },
	{ "99", -90 },

//THREE_DIGIT_DATA_LENGTH
	{ "235", -28 },
	{ "240", -30 },
	{ "241", -30 },
	{ "242", -6 },
	{ "243", -20 },
	{ "250", -30 },
	{ "251", -30 },
	{ "253", -30 },
	{ "254", -20 },
	{ "255", -25 },

	{ "400", -30 },
	{ "401", -30 },
	{ "402", 17 },
	{ "403", -30 },
	{ "410", 13 },
	{ "411", 13 },
	{ "412", 13 },
	{ "413", 13 },
	{ "414", 13 },
	{ "415", 13 },
	{ "416", 13 },
	{ "417", 13 },
	{ "420", -20 },
	{ "421", -12 },
	{ "422", 3 },
	{ "423", -15 },
	{ "424", 3 },
	{ "425", -15 },
	{ "426", 3 },
	{ "427", -3 },

	{ "710", -20 },
	{ "711", -20 },
	{ "712", -20 },
	{ "713", -20 },
	{ "714", -20 },
	{ "715", -20 },

//THREE_DIGIT_PLUS_DIGIT_DATA_LENGTH
	{ "310", 6 },
	{ "311", 6 },
	{ "312", 6 },
	{ "313", 6 },
	{ "314", 6 },
	{ "315", 6 },
	{ "316", 6 },
	{ "320", 6 },
	{ "321", 6 },
	{ "322", 6 },
	{ "323", 6 },
	{ "324", 6 },
	{ "325", 6 },
	{ "326", 6 },
	{ "327", 6 },
	{ "328", 6 },
	{ "329", 6 },
	{ "330", 6 },
	{ "331", 6 },
	{ "332", 6 },
	{ "333", 6 },
	{ "334", 6 },
	{ "335", 6 },
	{ "336", 6 },
	{ "337", 6 },
	{ "340", 6 },
	{ "341", 6 },
	{ "342", 6 },
	{ "343", 6 },
	{ "344", 6 },
	{ "345", 6 },
	{ "346", 6 },
	{ "347", 6 },
	{ "348", 6 },
	{ "349", 6 },
	{ "350", 6 },
	{ "351", 6 },
	{ "352", 6 },
	{ "353", 6 },
	{ "354", 6 },
	{ "355", 6 },
	{ "356", 6 },
	{ "357", 6 },
	{ "360", 6 },
	{ "361", 6 },
	{ "362", 6 },
	{ "363", 6 },
	{ "364", 6 },
	{ "365", 6 },
	{ "366", 6 },
	{ "367", 6 },
	{ "368", 6 },
	{ "369", 6 },
	{ "390", -15 },
	{ "391", -18 },
	{ "392", -15 },
	{ "393", -18 },
	{ "394", 4 },
	{ "395", 6 },
	{ "703", -30 },
	{ "723", -30 },

//FOUR_DIGIT_DATA_LENGTH
	{ "4300", -35 },
	{ "4301", -35 },
	{ "4302", -70 },
	{ "4303", -70 },
	{ "4304", -70 },
	{ "4305", -70 },
	{ "4306", -70 },
	{ "4307", 2 },
	{ "4308", -30 },
	{ "4309", 20 },
	{ "4310", -35 },
	{ "4311", -35 },
	{ "4312", -70 },
	{ "4313", -70 },
	{ "4314", -70 },
	{ "4315", -70 },
	{ "4316", -70 },
	{ "4317", 2 },
	{ "4318", -20 },
	{ "4319", -30 },
	{ "4320", -35 },
	{ "4321", 1 },
	{ "4322", 1 },
	{ "4323", 1 },
	{ "4324", 10 },
	{ "4325", 10 },
	{ "4326", 6 },
	{ "4330", -7 },
	{ "4331", -7 },
	{ "4332", -7 },
	{ "4333", -7 },

	{ "7001", 13 },
	{ "7002", -30 },
	{ "7003", 10 },
	{ "7004", -4 },
	{ "7005", -12 },
	{ "7006", 6 },
	{ "7007", -12 },
	{ "7008", -3 },
	{ "7009", -10 },
	{ "7010", -2 },
	{ "7011", -10 },
	{ "7020", -20 },
	{ "7021", -20 },
	{ "7022", -20 },
	{ "7023", -30 },
	{ "7040", 4 },
	{ "7240", -20 },
	{ "7241", 2 },
	{ "7242", -25 },
	{ "7250", 8 },
	{ "7251", 12 },
	{ "7252", 1 },
	{ "7253", -40 },
	{ "7254", -40 },
	{ "7255", -10 },
	{ "7256", -90 },
	{ "7257", -70 },
	{ "7258", 3 },
	{ "7259", -40 },

	{ "8001", 14 },
	{ "8002", -20 },
	{ "8003", -30 },
	{ "8004", -30 },
	{ "8005", 6 },
	{ "8006", 18 },
	{ "8007", -34 },
	{ "8008", -12 },
	{ "8009", -50 },
	{ "8010", -30 },
	{ "8011", -12 },
	{ "8012", -20 },
	{ "8013", -25 },
	{ "8017", 18 },
	{ "8018", 18 },
	{ "8019", -10 },
	{ "8020", -25 },
	{ "8026", 18 },
	{ "8030", -90 },
	{ "8110", -70 },
	{ "8111", 4 },
	{ "8112", -70 },
	{ "8200", -70 },
};

std::string HRIFromGS1(std::string_view gs1)
{
	//TODO: c++20
	auto starts_with = [](std::string_view str, std::string_view pre) { return str.substr(0, pre.size()) == pre; };
	constexpr char GS = 29; // GS character (29 / 0x1D)

	std::string_view rem = gs1;
	std::string res;

	while (rem.size()) {
		const AiInfo* i = FindIf(aiInfos, [&](const AiInfo& i) { return starts_with(rem, i.aiPrefix); });
		if (i == std::end(aiInfos))
			return {};

		int aiSize = i->aiSize();
		if (Size(rem) < aiSize)
			return {};

		res += '(';
		res += rem.substr(0, aiSize);
		res += ')';
		rem.remove_prefix(aiSize);

		int fieldSize = i->fieldSize();
		if (i->isVariableLength()) {
			auto gsPos = rem.find(GS);
#if 1
			fieldSize = std::min(gsPos == std::string_view::npos ? Size(rem) : narrow_cast<int>(gsPos), fieldSize);
#else
			// TODO: ignore the 'max field size' part for now as it breaks rssexpanded-3/13.png?
			fieldSize = gsPos == std::string_view::npos ? Size(rem) : narrow_cast<int>(gsPos);
#endif
		}
		if (fieldSize == 0 || Size(rem) < fieldSize)
			return {};

		res += rem.substr(0, fieldSize);
		rem.remove_prefix(fieldSize);

		// See General Specification v22.0 Section 7.8.6.3: "...the processing routine SHALL tolerate a single separator character
		// immediately following any element string, whether necessary or not..."
		if (Size(rem) && rem.front() == GS)
			rem.remove_prefix(1);
	}

	return res;
}

std::string HRIFromISO15434(std::string_view str)
{
	// Use available unicode symbols to simulate sub- and superscript letters as specified in
	// ISO/IEC 15434:2019(E) 6. Human readable representation

	std::string res;
	res.reserve(str.size());

	for (char c : str) {
#if 1
		if (0 <= c && c <= 0x20)
			(res += "\xe2\x90") += char(0x80 + c); // Unicode Block “Control Pictures”: 0x2400
		else
			res += c;
#else
		switch (c) {
		case 4: oss << u8"\u1d31\u1d52\u209c"; break; // EOT
		case 28: oss << u8"\ua7f3\u209b"; break; // FS
		case 29: oss << u8"\u1d33\u209b"; break; // GS
		case 30: oss << u8"\u1d3f\u209b"; break; // RS
		case 31: oss << u8"\u1d41\u209b"; break; // US
		default: oss << c;
		}
#endif
	}

	return res;
}

} // namespace ZXing
