/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODCode93Writer.h"

#include "ODWriterHelper.h"
#include "Utf.h"
#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <stdexcept>

namespace ZXing::OneD {

static const char ALPHABET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%abcd*";

/**
* These represent the encodings of characters, as patterns of wide and narrow bars.
* The 9 least-significant bits of each int correspond to the pattern of wide and narrow.
*/
static const int CHARACTER_ENCODINGS[] = {
	0x114, 0x148, 0x144, 0x142, 0x128, 0x124, 0x122, 0x150, 0x112, 0x10A, // 0-9
	0x1A8, 0x1A4, 0x1A2, 0x194, 0x192, 0x18A, 0x168, 0x164, 0x162, 0x134, // A-J
	0x11A, 0x158, 0x14C, 0x146, 0x12C, 0x116, 0x1B4, 0x1B2, 0x1AC, 0x1A6, // K-T
	0x196, 0x19A, 0x16C, 0x166, 0x136, 0x13A, // U-Z
	0x12E, 0x1D4, 0x1D2, 0x1CA, 0x16E, 0x176, 0x1AE, // - - %
	0x126, 0x1DA, 0x1D6, 0x132, 0x15E, // Control chars? $-*
};

static_assert(Size(ALPHABET) - 1 == Size(CHARACTER_ENCODINGS), "table size mismatch");

static const int ASTERISK_ENCODING = CHARACTER_ENCODINGS[47];

static int AppendPattern(std::vector<bool>& target, int pos, int a)
{
	for (int i = 0; i < 9; i++) {
		int temp = a & (1 << (8 - i));
		target[pos + i] = temp != 0;
	}
	return 9;
}


static int ComputeChecksumIndex(const std::string& contents, int maxWeight)
{
	int weight = 1;
	int total = 0;

	for (int i = Size(contents) - 1; i >= 0; i--) {
		int indexInString = IndexOf(ALPHABET, contents[i]);
		total += indexInString * weight;
		if (++weight > maxWeight) {
			weight = 1;
		}
	}
	return total % 47;
}


ZXING_EXPORT_TEST_ONLY
std::string Code93ConvertToExtended(const std::wstring& contents)
{
	size_t length = contents.length();
	std::string extendedContent;
	extendedContent.reserve(length * 2);

	for (size_t i = 0; i < length; i++) {
		int character = contents[i];
		// ($)=a, (%)=b, (/)=c, (+)=d. see Code93Reader.ALPHABET
		if (character == 0) {
			// NUL: (%)U
			extendedContent.append("bU");
		}
		else if (character <= 26) {
			// SOH - SUB: ($)A - ($)Z
			extendedContent.push_back('a');
			extendedContent.push_back((char)('A' + character - 1));
		}
		else if (character <= 31) {
			// ESC - US: (%)A - (%)E
			extendedContent.push_back('b');
			extendedContent.push_back((char)('A' + character - 27));
		}
		else if (character == ' ' || character == '$' || character == '%' || character == '+') {
			// space $ % +
			extendedContent.push_back(character);
		}
		else if (character <= ',') {
			// ! " # & ' ( ) * ,: (/)A - (/)L
			extendedContent.push_back('c');
			extendedContent.push_back((char)('A' + character - '!'));
		}
		else if (character <= '9') {
			extendedContent.push_back(character);
		}
		else if (character == ':') {
			// :: (/)Z
			extendedContent.append("cZ");
		}
		else if (character <= '?') {
			// ; - ?: (%)F - (%)J
			extendedContent.push_back('b');
			extendedContent.push_back((char)('F' + character - ';'));
		}
		else if (character == '@') {
			// @: (%)V
			extendedContent.append("bV");
		}
		else if (character <= 'Z') {
			// A - Z
			extendedContent.push_back(character);
		}
		else if (character <= '_') {
			// [ - _: (%)K - (%)O
			extendedContent.push_back('b');
			extendedContent.push_back((char)('K' + character - '['));
		}
		else if (character == '`') {
			// `: (%)W
			extendedContent.append("bW");
		}
		else if (character <= 'z') {
			// a - z: (*)A - (*)Z
			extendedContent.push_back('d');
			extendedContent.push_back((char)('A' + character - 'a'));
		}
		else if (character <= 127) {
			// { - DEL: (%)P - (%)T
			extendedContent.push_back('b');
			extendedContent.push_back((char)('P' + character - '{'));
		}
		else {
			throw std::invalid_argument(std::string("Requested content contains a non-encodable character: '") + (char)character + "'");
		}
	}
	return extendedContent;
}

BitMatrix
Code93Writer::encode(const std::wstring& contents_, int width, int height) const
{
	std::string contents = Code93ConvertToExtended(contents_);

	size_t length = contents.length();
	if (length == 0) {
		throw std::invalid_argument("Found empty contents");
	}
	if (length > 80) {
		throw std::invalid_argument("Requested contents should be less than 80 digits long after converting to extended encoding");
	}

	//length of code + 2 start/stop characters + 2 checksums, each of 9 bits, plus a termination bar
	size_t codeWidth = (contents.length() + 2 + 2) * 9 + 1;

	std::vector<bool> result(codeWidth, false);

	//start character (*)
	int pos = AppendPattern(result, 0, ASTERISK_ENCODING);

	for (size_t i = 0; i < length; i++) {
		int indexInString = IndexOf(ALPHABET, contents[i]);
		pos += AppendPattern(result, pos, CHARACTER_ENCODINGS[indexInString]);
	}

	//add two checksums
	int check1 = ComputeChecksumIndex(contents, 20);
	pos += AppendPattern(result, pos, CHARACTER_ENCODINGS[check1]);

	//append the contents to reflect the first checksum added
	contents += static_cast<wchar_t>(ALPHABET[check1]);

	int check2 = ComputeChecksumIndex(contents, 15);
	pos += AppendPattern(result, pos, CHARACTER_ENCODINGS[check2]);

	//end character (*)
	pos += AppendPattern(result, pos, ASTERISK_ENCODING);

	//termination bar (single black bar)
	result[pos] = true;

	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 10);
}

BitMatrix Code93Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
