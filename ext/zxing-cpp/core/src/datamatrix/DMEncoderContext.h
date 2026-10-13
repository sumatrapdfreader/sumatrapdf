/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2006-2007 Jeremias Maerki.
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ByteArray.h"
#include "DMSymbolInfo.h"
#include "ZXAlgorithms.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace ZXing::DataMatrix {

class EncoderContext
{
	std::string _msg;
	SymbolShape _shape = SymbolShape::NONE;
	int _minWidth = -1;
	int _minHeight = -1;
	int _maxWidth = -1;
	int _maxHeight = -1;
	ByteArray _codewords;
	int _pos = 0;
	int _newEncoding = -1;
	const SymbolInfo* _symbolInfo = nullptr;
	int _skipAtEnd = 0;

public:
	explicit EncoderContext(std::string&& msg) : _msg(std::move(msg)) { _codewords.reserve(_msg.length()); }
	
	EncoderContext(const EncoderContext &) = delete;	// avoid copy by mistake

	void setSymbolShape(SymbolShape shape) {
		_shape = shape;
	}

	void setSizeConstraints(int minWidth, int minHeight, int maxWidth, int maxHeight) {
		_minWidth = minWidth;
		_minHeight = minHeight;
		_maxWidth = maxWidth;
		_maxHeight = maxHeight;
	}

	const std::string& message() const {
		return _msg;
	}

	void setSkipAtEnd(int count) {
		_skipAtEnd = count;
	}

	int currentPos() const {
		return _pos;
	}

	void setCurrentPos(int pos) {
		_pos = pos;
	}

	int currentChar() const {
		return _msg.at(_pos) & 0xff;
	}

	int nextChar() const {
		return _msg.at(_pos + 1) & 0xff;
	}

	const ByteArray& codewords() const {
		return _codewords;
	}

	int codewordCount() const {
		return Size(_codewords);
	}

	void addCodeword(uint8_t codeword) {
		_codewords.push_back(codeword);
	}

	void setNewEncoding(int encoding) {
		_newEncoding = encoding;
	}

	void clearNewEncoding() {
		_newEncoding = -1;
	}

	int newEncoding() const {
		return _newEncoding;
	}

	bool hasMoreCharacters() const {
		return _pos < totalMessageCharCount();
	}

	int totalMessageCharCount() const {
		return narrow_cast<int>(_msg.length() - _skipAtEnd);
	}

	int remainingCharacters() const {
		return totalMessageCharCount() - _pos;
	}

	const SymbolInfo* updateSymbolInfo(int len) {
		if (_symbolInfo == nullptr || len > _symbolInfo->dataCapacity()) {
			_symbolInfo = SymbolInfo::Lookup(len, _shape, _minWidth, _minHeight, _maxWidth, _maxHeight);
			if (_symbolInfo == nullptr) {
				throw std::invalid_argument("Can't find a symbol arrangement that matches the message. Data codewords: " + std::to_string(len));
			}
		}
		return _symbolInfo;
	}

	void resetSymbolInfo() {
		_symbolInfo = nullptr;
	}
	
	const SymbolInfo* symbolInfo() const {
		return _symbolInfo;
	}
};

} // namespace ZXing::DataMatrix
