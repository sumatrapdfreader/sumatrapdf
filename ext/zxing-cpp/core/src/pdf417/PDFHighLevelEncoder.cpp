/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
* Copyright 2006 Jeremias Maerki in part, and ZXing Authors in part
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFHighLevelEncoder.h"
#include "PDFCompaction.h"
#include "CharacterSet.h"
#include "ECI.h"
#include "TextEncoder.h"
#include "ZXBigInteger.h"
#include "ZXAlgorithms.h"

#include <cstdint>
#include <algorithm>
#include <string>
#include <stdexcept>

namespace ZXing {
namespace Pdf417 {

/**
* code for Text compaction
*/
static const int TEXT_COMPACTION = 0;

/**
* code for Byte compaction
*/
static const int BYTE_COMPACTION = 1;

/**
* code for Numeric compaction
*/
static const int NUMERIC_COMPACTION = 2;

/**
* Text compaction submode Alpha
*/
static const int SUBMODE_ALPHA = 0;

/**
* Text compaction submode Lower
*/
static const int SUBMODE_LOWER = 1;

/**
* Text compaction submode Mixed
*/
static const int SUBMODE_MIXED = 2;

/**
* Text compaction submode Punctuation
*/
static const int SUBMODE_PUNCTUATION = 3;

/**
* mode latch to Text Compaction mode
*/
static const int LATCH_TO_TEXT = 900;

/**
* mode latch to Byte Compaction mode (number of characters NOT a multiple of 6)
*/
static const int LATCH_TO_BYTE_PADDED = 901;

/**
* mode latch to Numeric Compaction mode
*/
static const int LATCH_TO_NUMERIC = 902;

/**
* mode shift to Byte Compaction mode
*/
static const int SHIFT_TO_BYTE = 913;

/**
* mode latch to Byte Compaction mode (number of characters a multiple of 6)
*/
static const int LATCH_TO_BYTE = 924;

/**
* identifier for a user defined Extended Channel Interpretation (ECI)
*/
static const int ECI_USER_DEFINED = 925;

/**
* identifier for a general purpose ECO format
*/
static const int ECI_GENERAL_PURPOSE = 926;

/**
* identifier for an ECI of a character set of code page
*/
static const int ECI_CHARSET = 927;

/**
* Raw code table for text compaction Mixed sub-mode
*/
//static const uint8_t TEXT_MIXED_RAW[] = {
//	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 38, 13, 9, 44, 58,
//	35, 45, 46, 36, 47, 43, 37, 42, 61, 94, 0, 32, 0, 0, 0 };

/**
* Raw code table for text compaction: Punctuation sub-mode
*/
//static const uint8_t TEXT_PUNCTUATION_RAW[] = {
//	59, 60, 62, 64, 91, 92, 93, 95, 96, 126, 33, 13, 9, 44, 58,
//	10, 45, 46, 36, 47, 34, 124, 42, 40, 41, 63, 123, 125, 39, 0 };

//static {
//	//Construct inverse lookups
//	Arrays.fill(MIXED, (byte)-1);
//	for (byte i = 0; i < TEXT_MIXED_RAW.length; i++) {
//		byte b = TEXT_MIXED_RAW[i];
//		if (b > 0) {
//			MIXED[b] = i;
//		}
//	}
//	Arrays.fill(PUNCTUATION, (byte)-1);
//	for (byte i = 0; i < TEXT_PUNCTUATION_RAW.length; i++) {
//		byte b = TEXT_PUNCTUATION_RAW[i];
//		if (b > 0) {
//			PUNCTUATION[b] = i;
//		}
//	}
//}

static const int8_t MIXED[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, 12, -1, -1, -1, 11, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	26, -1, -1, 15, 18, 21, 10, -1, -1, -1, 22, 20, 13, 16, 17, 19,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 14, -1, -1, 23, -1, -1,
	-1,	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1,	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1,
	-1,	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1,	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static const int8_t PUNCTUATION[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, 12, 15, -1, -1, 11, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, 10, 20, -1, 18, -1, -1, 28, 23, 24, 22, -1, 13, 16, 17, 19,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14,  0,  1, -1,  2, 25,
	 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  4,  5,  6, -1,  7,
	 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 21, 27,  9, -1,
};

static void EncodingECI(int eci, std::vector<int>& buffer)
{
	if (eci >= 0 && eci < 900) {
		buffer.push_back(ECI_CHARSET);
		buffer.push_back(eci);
	}
	else if (eci >= 900 && eci < 810900) {
		buffer.push_back(ECI_GENERAL_PURPOSE);
		buffer.push_back(eci / 900 - 1);
		buffer.push_back(eci % 900);
	}
	else if (eci >= 810900 && eci < 811800) {
		buffer.push_back(ECI_USER_DEFINED);
		buffer.push_back(eci - 810900);
	}
	else {
		throw std::invalid_argument("ECI number not in valid range from 0..811799");
	}
}

static bool IsDigit(int ch)
{
	return ch >= '0' && ch <= '9';
}

static bool IsAlphaUpper(int ch)
{
	return ch == ' ' || (ch >= 'A' && ch <= 'Z');
}

static bool IsAlphaLower(int ch)
{
	return ch == ' ' || (ch >= 'a' && ch <= 'z');
}

static bool IsMixed(int ch)
{
	return (ch & 0x7f) == ch && MIXED[ch] != -1;
}

static bool IsPunctuation(int ch)
{
	return (ch & 0x7f) == ch && PUNCTUATION[ch] != -1;
}

static bool IsText(int ch)
{
	return ch == '\t' || ch == '\n' || ch == '\r' || (ch >= 32 && ch <= 126);
}


/**
* Encode parts of the message using Text Compaction as described in ISO/IEC 15438:2001(E),
* chapter 4.4.2.
*
* @param msg      the message
* @param startpos the start position within the message
* @param count    the number of characters to encode
* @param submode  should normally be SUBMODE_ALPHA
* @param output   receives the encoded codewords
* @return the text submode in which this method ends
*/
static int EncodeText(const std::wstring& msg, int startpos, int count, int submode, std::vector<int>& output)
{
	std::vector<int> tmp;
	tmp.reserve(count);
	int idx = 0;
	while (true) {
		int ch = msg[startpos + idx];
		switch (submode) {
		case SUBMODE_ALPHA:
			if (IsAlphaUpper(ch)) {
				tmp.push_back(ch == ' ' ? 26 : (ch - 65)); // space
			} else if (IsAlphaLower(ch)) {
				submode = SUBMODE_LOWER;
				tmp.push_back(27); // ll
				continue;
			} else if (IsMixed(ch)) {
				submode = SUBMODE_MIXED;
				tmp.push_back(28); // ml
				continue;
			} else {
				tmp.push_back(29); // ps
				tmp.push_back(PUNCTUATION[ch]);
			}
			break;
		case SUBMODE_LOWER:
			if (IsAlphaLower(ch)) {
				tmp.push_back(ch == ' ' ? 26 : (ch - 97)); // space
			} else if (IsAlphaUpper(ch)) {
				tmp.push_back(27); // as
				tmp.push_back(ch - 65);
				// space cannot happen here, it is also in "Lower"
			} else if (IsMixed(ch)) {
				submode = SUBMODE_MIXED;
				tmp.push_back(28); // ml
				continue;
			} else {
				tmp.push_back(29); // ps
				tmp.push_back(PUNCTUATION[ch]);
			}
			break;
		case SUBMODE_MIXED:
			if (IsMixed(ch)) {
				tmp.push_back(MIXED[ch]);
			} else if (IsAlphaUpper(ch)) {
				submode = SUBMODE_ALPHA;
				tmp.push_back(28); // al
				continue;
			} else if (IsAlphaLower(ch)) {
				submode = SUBMODE_LOWER;
				tmp.push_back(27); // ll
				continue;
			} else {
				if (startpos + idx + 1 < count) {
					int next = msg[startpos + idx + 1];
					if (IsPunctuation(next)) {
						submode = SUBMODE_PUNCTUATION;
						tmp.push_back(25); // pl
						continue;
					}
				}
				tmp.push_back(29); // ps
				tmp.push_back(PUNCTUATION[ch]);
			}
			break;
		default: // SUBMODE_PUNCTUATION
			if (IsPunctuation(ch)) {
				tmp.push_back(PUNCTUATION[ch]);
			} else {
				submode = SUBMODE_ALPHA;
				tmp.push_back(29); // al
				continue;
			}
		}
		idx++;
		if (idx >= count) {
			break;
		}
	}
	int h = 0;
	size_t len = tmp.size();
	for (size_t i = 0; i < len; i++) {
		bool odd = (i % 2) != 0;
		if (odd) {
			h = (h * 30) + tmp[i];
			output.push_back(h);
		}
		else {
			h = tmp[i];
		}
	}
	if ((len % 2) != 0) {
		output.push_back((h * 30) + 29); //ps
	}
	return submode;
}


/**
* Encode parts of the message using Byte Compaction as described in ISO/IEC 15438:2001(E),
* chapter 4.4.3. The Unicode characters will be converted to binary using the cp437
* codepage.
*
* @param bytes     the message converted to a byte array
* @param startpos  the start position within the message
* @param count     the number of bytes to encode
* @param startmode the mode from which this method starts
* @param output    receives the encoded codewords
*/
static void EncodeBinary(const std::string& bytes, int startpos, int count, int startmode, std::vector<int>& output)
{
	if (count == 1 && startmode == TEXT_COMPACTION) {
		output.push_back(SHIFT_TO_BYTE);
	}
	else {
		if ((count % 6) == 0) {
			output.push_back(LATCH_TO_BYTE);
		}
		else {
			output.push_back(LATCH_TO_BYTE_PADDED);
		}
	}

	int idx = startpos;
	// Encode sixpacks
	if (count >= 6) {
		int chars[5];
		while ((startpos + count - idx) >= 6) {
			long t = 0;
			for (int i = 0; i < 6; i++) {
				t <<= 8;
				t += bytes[idx + i] & 0xff;
			}
			for (int i = 0; i < 5; i++) {
				chars[i] = t % 900;
				t /= 900;
			}
			for (int i = 4; i >= 0; i--) {
				output.push_back(chars[i]);
			}
			idx += 6;
		}
	}
	//Encode rest (remaining n<5 bytes if any)
	for (int i = idx; i < startpos + count; i++) {
		int ch = bytes[i] & 0xff;
		output.push_back(ch);
	}
}


static void EncodeNumeric(const std::wstring& msg, int startpos, int count, std::vector<int>& output)
{
	int idx = 0;
	std::vector<int> tmp;
	tmp.reserve(count / 3 + 1);
	BigInteger num900(900);
	while (idx < count) {
		tmp.clear();
		int len = std::min(44, count - idx);
		auto part = L"1" + msg.substr(startpos + idx, len);

		BigInteger bigint, r;
		BigInteger::TryParse(part, bigint);
		do {
			BigInteger::Divide(bigint, num900, bigint, r);
			tmp.push_back(r.toInt());
		} while (!bigint.isZero());

		//Reverse temporary string
		output.insert(output.end(), tmp.rbegin(), tmp.rend());
		idx += len;
	}
}


/**
* Determines the number of consecutive characters that are encodable using numeric compaction.
*
* @param msg      the message
* @param startpos the start position within the message
* @return the requested character count
*/
static int DetermineConsecutiveDigitCount(const std::wstring& msg, int startpos)
{
	int count = 0;
	size_t len = msg.length();
	size_t idx = startpos;
	if (idx < len) {
		int ch = msg[idx];
		while (IsDigit(ch) && idx < len) {
			count++;
			idx++;
			if (idx < len) {
				ch = msg[idx];
			}
		}
	}
	return count;
}

/**
* Determines the number of consecutive characters that are encodable using text compaction.
*
* @param msg      the message
* @param startpos the start position within the message
* @return the requested character count
*/
static int DetermineConsecutiveTextCount(const std::wstring& msg, int startpos)
{
	size_t len = msg.length();
	size_t idx = startpos;
	while (idx < len) {
		int ch = msg[idx];
		int numericCount = 0;
		while (numericCount < 13 && IsDigit(ch) && idx < len) {
			numericCount++;
			idx++;
			if (idx < len) {
				ch = msg[idx];
			}
		}
		if (numericCount >= 13) {
			return static_cast<int>(idx - startpos - numericCount);
		}
		if (numericCount > 0) {
			//Heuristic: All text-encodable chars or digits are binary encodable
			continue;
		}
		ch = msg[idx];

		//Check if character is encodable
		if (!IsText(ch)) {
			break;
		}
		idx++;
	}
	return static_cast<int>(idx - startpos);
}

/**
* Determines the number of consecutive characters that are encodable using binary compaction.
*
* @param msg      the message
* @param startpos the start position within the message
* @return the requested character count
*/
static int DetermineConsecutiveBinaryCount(const std::wstring& msg, int startpos)
{
	size_t len = msg.length();
	size_t idx = startpos;
	while (idx < len) {
		int ch = msg[idx];
		int numericCount = 0;

		while (numericCount < 13 && IsDigit(ch)) {
			numericCount++;
			//textCount++;
			size_t i = idx + numericCount;
			if (i >= len) {
				break;
			}
			ch = msg[i];
		}
		if (numericCount >= 13) {
			return static_cast<int>(idx - startpos);
		}
		idx++;
	}
	return static_cast<int>(idx - startpos);
}

/**
* Performs high-level encoding of a PDF417 message using the algorithm described in annex P
* of ISO/IEC 15438:2001(E). If byte compaction has been selected, then only byte compaction
* is used.
*
* @param msg the message
* @param compaction compaction mode to use
* @param encoding character encoding used to encode in default or byte compaction
*  or {@code null} for default / not applicable
* @return the encoded message (the char values range from 0 to 928)
*/
std::vector<int>
HighLevelEncoder::EncodeHighLevel(const std::wstring& msg, Compaction compaction, CharacterSet encoding)
{
	std::vector<int> highLevel;
	highLevel.reserve(highLevel.size() + msg.length());

	//the codewords 0..928 are encoded as Unicode characters
	if (encoding != CharacterSet::ISO8859_1) {
		EncodingECI(ToInt(ToECI(encoding)), highLevel);
	}

	int len = Size(msg);
	int p = 0;
	int textSubMode = SUBMODE_ALPHA;

	// User selected encoding mode
	if (compaction == Compaction::TEXT) {
		EncodeText(msg, p, len, textSubMode, highLevel);

	}
	else if (compaction == Compaction::BYTE) {
		std::string bytes = TextEncoder::FromUnicode(msg, encoding);
		EncodeBinary(bytes, p, Size(bytes), BYTE_COMPACTION, highLevel);
	}
	else if (compaction == Compaction::NUMERIC) {
		highLevel.push_back(LATCH_TO_NUMERIC);
		EncodeNumeric(msg, p, len, highLevel);

	}
	else {
		int encodingMode = TEXT_COMPACTION; //Default mode, see 4.4.2.1
		while (p < len) {
			int n = DetermineConsecutiveDigitCount(msg, p);
			if (n >= 13) {
				highLevel.push_back(LATCH_TO_NUMERIC);
				encodingMode = NUMERIC_COMPACTION;
				textSubMode = SUBMODE_ALPHA; //Reset after latch
				EncodeNumeric(msg, p, n, highLevel);
				p += n;
			}
			else {
				int t = DetermineConsecutiveTextCount(msg, p);
				if (t >= 5 || n == len) {
					if (encodingMode != TEXT_COMPACTION) {
						highLevel.push_back(LATCH_TO_TEXT);
						encodingMode = TEXT_COMPACTION;
						textSubMode = SUBMODE_ALPHA; //start with submode alpha after latch
					}
					textSubMode = EncodeText(msg, p, t, textSubMode, highLevel);
					p += t;
				}
				else {
					int b = DetermineConsecutiveBinaryCount(msg, p);
					if (b == 0) {
						b = 1;
					}
					std::string bytes = TextEncoder::FromUnicode(msg.substr(p, b), encoding);
					if (bytes.length() == 1 && encodingMode == TEXT_COMPACTION) {
						//Switch for one byte (instead of latch)
						EncodeBinary(bytes, 0, 1, TEXT_COMPACTION, highLevel);
					}
					else {
						//Mode latch performed by encodeBinary()
						EncodeBinary(bytes, 0, Size(bytes), encodingMode, highLevel);
						encodingMode = BYTE_COMPACTION;
						textSubMode = SUBMODE_ALPHA; //Reset after latch
					}
					p += b;
				}
			}
		}
	}
	return highLevel;
}



} // Pdf417
} // ZXing
