/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "AZHighLevelEncoder.h"

#include "AZEncodingState.h"
#include "AZToken.h"
#include "BitArray.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <list>
#include <vector>

namespace ZXing::Aztec {

// Do not change these constants
static const int MODE_UPPER = 0; // 5 bits
static const int MODE_LOWER = 1; // 5 bits
static const int MODE_DIGIT = 2; // 4 bits
static const int MODE_MIXED = 3; // 5 bits
static const int MODE_PUNCT = 4; // 5 bits

// The Latch Table shows, for each pair of Modes, the optimal method for
// getting from one mode to another.  In the worst possible case, this can
// be up to 14 bits.  In the best possible case, we are already there!
// The high half-word of each entry gives the number of bits.
// The low half-word of each entry are the actual bits necessary to change
static const std::array<std::array<int, 5>, 5> LATCH_TABLE = {
		0,
		(5 << 16) + 28,              // UPPER -> LOWER
		(5 << 16) + 30,              // UPPER -> DIGIT
		(5 << 16) + 29,              // UPPER -> MIXED
		(10 << 16) + (29 << 5) + 30, // UPPER -> MIXED -> PUNCT

		(9 << 16) + (30 << 4) + 14,  // LOWER -> DIGIT -> UPPER
		0,
		(5 << 16) + 30,              // LOWER -> DIGIT
		(5 << 16) + 29,              // LOWER -> MIXED
		(10 << 16) + (29 << 5) + 30, // LOWER -> MIXED -> PUNCT

		(4 << 16) + 14,              // DIGIT -> UPPER
		(9 << 16) + (14 << 5) + 28,  // DIGIT -> UPPER -> LOWER
		0,
		(9 << 16) + (14 << 5) + 29,  // DIGIT -> UPPER -> MIXED
		(14 << 16) + (14 << 10) + (29 << 5) + 30,
		// DIGIT -> UPPER -> MIXED -> PUNCT

		(5 << 16) + 29,              // MIXED -> UPPER
		(5 << 16) + 28,              // MIXED -> LOWER
		(10 << 16) + (29 << 5) + 30, // MIXED -> UPPER -> DIGIT
		0,
		(5 << 16) + 30,              // MIXED -> PUNCT

		(5 << 16) + 31,              // PUNCT -> UPPER
		(10 << 16) + (31 << 5) + 28, // PUNCT -> UPPER -> LOWER
		(10 << 16) + (31 << 5) + 30, // PUNCT -> UPPER -> DIGIT
		(10 << 16) + (31 << 5) + 29, // PUNCT -> UPPER -> MIXED
		0,
};

// A reverse mapping from [mode][char] to the encoding for that character
// in that mode.  An entry of 0 indicates no mapping exists.
static const std::array<std::array<int8_t, 256>, 5>& InitCharMap()
{
	static std::array<std::array<int8_t, 256>, 5> charmap = {};
	charmap[MODE_UPPER][' '] = 1;
	for (int c = 'A'; c <= 'Z'; c++) {
		charmap[MODE_UPPER][c] = c - 'A' + 2;
	}
	charmap[MODE_LOWER][' '] = 1;
	for (int c = 'a'; c <= 'z'; c++) {
		charmap[MODE_LOWER][c] = c - 'a' + 2;
	}
	charmap[MODE_DIGIT][' '] = 1;
	for (int c = '0'; c <= '9'; c++) {
		charmap[MODE_DIGIT][c] = c - '0' + 2;
	}
	charmap[MODE_DIGIT][','] = 12;
	charmap[MODE_DIGIT]['.'] = 13;
	const int8_t mixedTable[] = {
		0x00, 0x20, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
		0x0d, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x40, 0x5c, 0x5e, 0x5f, 0x60, 0x7c, 0x7d, 0x7f,
	};
	for (uint8_t i = 0; i < Size(mixedTable); i++) {
		charmap[MODE_MIXED][mixedTable[i]] = i;
	}
	const char punctTable[] = {'\0', '\r', '\0', '\0', '\0', '\0', '!', '\'', '#', '$', '%', '&', '\'', '(', ')', '*',
							   '+',  ',',  '-',  '.',  '/',  ':',  ';', '<',  '=', '>', '?', '[', ']',  '{', '}'};
	for (uint8_t i = 0; i < Size(punctTable); i++) {
		if (punctTable[i] > 0) {
			charmap[MODE_PUNCT][punctTable[i]] = i;
		}
	}
	return charmap;
}
const std::array<std::array<int8_t, 256>, 5>& CHAR_MAP = InitCharMap();

// A map showing the available shift codes.  (The shifts to BINARY are not shown
static const std::array<std::array<int8_t, 6>, 6>& InitShiftTable()
{
	static std::array<std::array<int8_t, 6>, 6> table;

	for (auto& row : table) {
		std::fill(row.begin(), row.end(), -1);
	}
	table[MODE_UPPER][MODE_PUNCT] = 0;
	table[MODE_LOWER][MODE_PUNCT] = 0;
	table[MODE_LOWER][MODE_UPPER] = 28;
	table[MODE_MIXED][MODE_PUNCT] = 0;
	table[MODE_DIGIT][MODE_PUNCT] = 0;
	table[MODE_DIGIT][MODE_UPPER] = 15;
	return table;
}

const std::array<std::array<int8_t, 6>, 6>& SHIFT_TABLE = InitShiftTable();

// Create a new state representing this state with a latch to a (not
// necessary different) mode, and then a code.
static EncodingState LatchAndAppend(const EncodingState& state, int mode, int value)
{
	//assert binaryShiftByteCount == 0;
	int bitCount = state.bitCount;
	auto tokens = state.tokens;
	if (mode != state.mode) {
		int latch = LATCH_TABLE[state.mode][mode];
		tokens.push_back(Token::CreateSimple(latch & 0xFFFF, latch >> 16));
		bitCount += latch >> 16;
	}
	int latchModeBitCount = mode == MODE_DIGIT ? 4 : 5;
	tokens.push_back(Token::CreateSimple(value, latchModeBitCount));
	return EncodingState{ tokens, mode, 0, bitCount + latchModeBitCount };
}

// Create a new state representing this state, with a temporary shift
// to a different mode to output a single value.
static EncodingState ShiftAndAppend(const EncodingState& state, int mode, int value)
{
	//assert binaryShiftByteCount == 0 && this.mode != mode;
	int thisModeBitCount = state.mode == MODE_DIGIT ? 4 : 5;
	// Shifts exist only to UPPER and PUNCT, both with tokens size 5.
	auto tokens = state.tokens;
	tokens.push_back(Token::CreateSimple(SHIFT_TABLE[state.mode][mode], thisModeBitCount));
	tokens.push_back(Token::CreateSimple(value, 5));
	return EncodingState{ tokens, state.mode, 0, state.bitCount + thisModeBitCount + 5 };
}

// Create the state identical to this one, but we are no longer in
// Binary Shift mode.
static EncodingState EndBinaryShift(const EncodingState& state, int index)
{
	if (state.binaryShiftByteCount == 0) {
		return state;
	}
	auto tokens = state.tokens;
	tokens.push_back(Token::CreateBinaryShift(index - state.binaryShiftByteCount, state.binaryShiftByteCount));
	//assert token.getTotalBitCount() == this.bitCount;
	return EncodingState{ tokens, state.mode, 0, state.bitCount };
}

// Create a new state representing this state, but an additional character
// output in Binary Shift mode.
static EncodingState AddBinaryShiftChar(const EncodingState& state, int index)
{
	auto tokens = state.tokens;
	int mode = state.mode;
	int bitCount = state.bitCount;
	if (state.mode == MODE_PUNCT || state.mode == MODE_DIGIT) {
		//assert binaryShiftByteCount == 0;
		int latch = LATCH_TABLE[mode][MODE_UPPER];
		tokens.push_back(Token::CreateSimple(latch & 0xFFFF, latch >> 16));
		bitCount += latch >> 16;
		mode = MODE_UPPER;
	}
	int deltaBitCount = (state.binaryShiftByteCount == 0 || state.binaryShiftByteCount == 31) ? 18 : (state.binaryShiftByteCount == 62) ? 9 : 8;
	EncodingState result{ tokens, mode, state.binaryShiftByteCount + 1, bitCount + deltaBitCount };
	if (result.binaryShiftByteCount == 2047 + 31) {
		// The string is as long as it's allowed to be.  We should end it.
		result = EndBinaryShift(result, index + 1);
	}
	return result;
}

static int CalculateBinaryShiftCost(const EncodingState& state)
{
	if (state.binaryShiftByteCount > 62) {
		return 21; // B/S with extended length
	}
	if (state.binaryShiftByteCount > 31) {
		return 20; // two B/S
	}
	if (state.binaryShiftByteCount > 0) {
		return 10; // one B/S
	}
	return 0;
}

// Returns true if "this" state is better (or equal) to be in than "that"
// state under all possible circumstances.
static bool IsBetterThanOrEqualTo(const EncodingState& state, const EncodingState& other)
{
	int newModeBitCount = state.bitCount + (LATCH_TABLE[state.mode][other.mode] >> 16);
	if (state.binaryShiftByteCount < other.binaryShiftByteCount) {
		// add additional B/S encoding cost of other, if any
		newModeBitCount += CalculateBinaryShiftCost(other) - CalculateBinaryShiftCost(state);
	}
	else if (state.binaryShiftByteCount > other.binaryShiftByteCount && other.binaryShiftByteCount > 0) {
		// maximum possible additional cost (we end up exceeding the 31 byte boundary and other state can stay beneath it)
		newModeBitCount += 10;
	}
	return newModeBitCount <= other.bitCount;
}

static BitArray ToBitArray(const EncodingState& state, const std::string& text)
{
	auto endState = EndBinaryShift(state, Size(text));
	BitArray bits;
	// Add each token to the result.
	for (const Token& symbol : endState.tokens) {
		symbol.appendTo(bits, text);
	}
	//assert bitArray.getSize() == this.bitCount;
	return bits;
}

static void UpdateStateForPair(const EncodingState& state, int index, int pairCode, std::list<EncodingState>& result)
{
	EncodingState stateNoBinary = EndBinaryShift(state, index);
	// Possibility 1.  Latch to MODE_PUNCT, and then append this code
	result.push_back(LatchAndAppend(stateNoBinary, MODE_PUNCT, pairCode));
	if (state.mode != MODE_PUNCT) {
		// Possibility 2.  Shift to MODE_PUNCT, and then append this code.
		// Every state except MODE_PUNCT (handled above) can shift
		result.push_back(ShiftAndAppend(stateNoBinary, MODE_PUNCT, pairCode));
	}
	if (pairCode == 3 || pairCode == 4) {
		// both characters are in DIGITS.  Sometimes better to just add two digits
		auto digitState = LatchAndAppend(stateNoBinary, MODE_DIGIT, 16 - pairCode);	// period or comma in DIGIT
		result.push_back(LatchAndAppend(digitState, MODE_DIGIT, 1));				// space in DIGIT
	}
	if (state.binaryShiftByteCount > 0) {
		// It only makes sense to do the characters as binary if we're already
		// in binary mode.
		result.push_back(AddBinaryShiftChar(AddBinaryShiftChar(state, index), index + 1));
	}
}


static std::list<EncodingState> SimplifyStates(const std::list<EncodingState>& states)
{
	std::list<EncodingState> result;
	for (auto& newState : states) {
		bool add = true;
		for (auto iterator = result.begin(); iterator != result.end();) {
			auto& oldState = *iterator;
			if (IsBetterThanOrEqualTo(oldState, newState)) {
				add = false;
				break;
			}
			if (IsBetterThanOrEqualTo(newState, oldState)) {
				iterator = result.erase(iterator);
			}
			else {
				++iterator;
			}
		}
		if (add) {
			result.push_back(newState);
		}
	}
	return result;
}

static std::list<EncodingState> UpdateStateListForPair(const std::list<EncodingState>& states, int index, int pairCode)
{
	std::list<EncodingState> result;
	for (auto& state : states) {
		UpdateStateForPair(state, index, pairCode, result);
	}
	return SimplifyStates(result);
}



// Return a set of states that represent the possible ways of updating this
// state for the next character.  The resulting set of states are added to
// the "result" list.
static void UpdateStateForChar(const EncodingState& state, const std::string& text, int index, std::list<EncodingState>& result)
{
	int ch = text[index] & 0xff;
	bool charInCurrentTable = CHAR_MAP[state.mode][ch] > 0;
	EncodingState stateNoBinary;
	bool firstTime = true;
	for (int mode = 0; mode <= MODE_PUNCT; mode++) {
		int charInMode = CHAR_MAP[mode][ch];
		if (charInMode > 0) {
			if (firstTime) {
				// Only create stateNoBinary the first time it's required.
				stateNoBinary = EndBinaryShift(state, index);
				firstTime = false;
			}
			// Try generating the character by latching to its mode
			if (!charInCurrentTable || mode == state.mode || mode == MODE_DIGIT) {
				// If the character is in the current table, we don't want to latch to
				// any other mode except possibly digit (which uses only 4 bits).  Any
				// other latch would be equally successful *after* this character, and
				// so wouldn't save any bits.
				result.push_back(LatchAndAppend(stateNoBinary, mode, charInMode));
			}
			// Try generating the character by switching to its mode.
			if (!charInCurrentTable && SHIFT_TABLE[state.mode][mode] >= 0) {
				// It never makes sense to temporarily shift to another mode if the
				// character exists in the current mode.  That can never save bits.
				result.push_back(ShiftAndAppend(stateNoBinary, mode, charInMode));
			}
		}
	}
	if (state.binaryShiftByteCount > 0 || CHAR_MAP[state.mode][ch] == 0) {
		// It's never worthwhile to go into binary shift mode if you're not already
		// in binary shift mode, and the character exists in your current mode.
		// That can never save bits over just outputting the char in the current mode.
		result.push_back(AddBinaryShiftChar(state, index));
	}
}

// We update a set of states for a new character by updating each state
// for the new character, merging the results, and then removing the
// non-optimal states.
static std::list<EncodingState> UpdateStateListForChar(const std::list<EncodingState>& states, const std::string& text, int index)
{
	std::list<EncodingState> result;
	for (auto& state : states) {
		UpdateStateForChar(state, text, index, result);
	}
	return result.size() > 1 ? SimplifyStates(result) : result;
}

/**
* @return text represented by this encoder encoded as a {@link BitArray}
*/
BitArray
HighLevelEncoder::Encode(const std::string& text)
{
	std::list<EncodingState> states;
	states.push_back(EncodingState{ std::vector<Token>(), MODE_UPPER, 0, 0 });
	for (int index = 0; index < Size(text); index++) {
		int pairCode;
		int nextChar = index + 1 < Size(text) ? text[index + 1] : 0;
		switch (text[index]) {
		case '\r': pairCode = nextChar == '\n' ? 2 : 0; break;
		case '.': pairCode = nextChar == ' ' ? 3 : 0; break;
		case ',': pairCode = nextChar == ' ' ? 4 : 0; break;
		case ':': pairCode = nextChar == ' ' ? 5 : 0; break;
		default: pairCode = 0;
		}
		if (pairCode > 0) {
			// We have one of the four special PUNCT pairs.  Treat them specially.
			// Get a new set of states for the two new characters.
			states = UpdateStateListForPair(states, index, pairCode);
			index++;
		} else {
			// Get a new set of states for the new character.
			states = UpdateStateListForChar(states, text, index);
		}
	}
	// We are left with a set of states.  Find the shortest one.
	EncodingState minState = *std::min_element(states.begin(), states.end(), [](const EncodingState& a, const EncodingState& b) { return a.bitCount < b.bitCount; });
	// Convert it to a bit array, and return.
	return ToBitArray(minState, text);
}

} // namespace ZXing::Aztec
