/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "CharacterSet.h"
#include "PDFCompaction.h"
#include "ZXAlgorithms.h"

#include <string>
#include <vector>

namespace ZXing {

namespace Pdf417 {

/**
* @author Jacob Haynes
*/
class BarcodeRow
{
	std::vector<bool> _row;
	int _currentLocation = 0; // A tacker for position in the bar

public:
	explicit BarcodeRow(int width = 0) : _row(width, false) {}

	void init(int width) {
		_row.resize(width, false);
		_currentLocation = 0;
	}

	void set(int x, bool black) {
		_row.at(x) = black;
	}

	/**
	* @param black A boolean which is true if the bar black false if it is white
	* @param width How many spots wide the bar is.
	*/
	void addBar(bool black, int width) {
		for (int ii = 0; ii < width; ii++) {
			_row.at(_currentLocation++) = black;
		}
	}

	/**
	* This function scales the row
	*
	* @param scale How much you want the image to be scaled, must be greater than or equal to 1.
	* @param output the scaled row
	*/
	void getScaledRow(int scale, std::vector<bool>& output) const {
		output.resize(_row.size() * scale);
		for (size_t i = 0; i < output.size(); ++i) {
			output[i] = _row[i / scale];
		}
	}
};

/**
* Holds all of the information for a barcode in a format where it can be easily accessible
*
* @author Jacob Haynes
*/
class BarcodeMatrix
{
	std::vector<BarcodeRow> _matrix;
	int _width = 0;
	int _currentRow = -1;

public:
	BarcodeMatrix() {}

	/**
	* @param height the height of the matrix (Rows)
	* @param width  the width of the matrix (Cols)
	*/
	BarcodeMatrix(int height, int width) {
		init(height, width);
	}

	void init(int height, int width) {
		_matrix.resize(height);
		for (int i = 0; i < height; ++i) {
			_matrix[i].init((width + 4) * 17 + 1);
		}
		_width = width * 17;
		_currentRow = -1;
	}

	void set(int x, int y, bool value) {
		_matrix[y].set(x, value);
	}

	void startRow() {
		++_currentRow;
	}

	const BarcodeRow& currentRow() const {
		return _matrix[_currentRow];
	}

	BarcodeRow& currentRow() {
		return _matrix[_currentRow];
	}

	void getScaledMatrix(int xScale, int yScale, std::vector<std::vector<bool>>& output)
	{
		output.resize(_matrix.size() * yScale);
		int yMax = Size(output);
		for (int i = 0; i < yMax; i++) {
			_matrix[i / yScale].getScaledRow(xScale, output[yMax - i - 1]);
		}
	}
};

/**
* Top-level class for the logic part of the PDF417 implementation.
* C++ port: this class was named PDF417 in Java code. Since that name
* does say much in the context of PDF417 writer, it's renamed here Encoder
* to follow the same naming convention with other modules.
*/
class Encoder
{
public:
	explicit Encoder(bool compact = false) : _compact(compact)  {}
	
	BarcodeMatrix generateBarcodeLogic(const std::wstring& msg, int errorCorrectionLevel) const;

	/**
	* Sets max/min row/col values
	*
	* @param maxCols maximum allowed columns
	* @param minCols minimum allowed columns
	* @param maxRows maximum allowed rows
	* @param minRows minimum allowed rows
	*/
	void setDimensions(int minCols, int maxCols, int minRows, int maxRows) {
		_minCols = minCols;
		_maxCols = maxCols;
		_minRows = minRows;
		_maxRows = maxRows;
	}

	/**
	* @param compaction compaction mode to use
	*/
	void setCompaction(Compaction compaction) {
		_compaction = compaction;
	}

	/**
	* @param compact if true, enables compaction
	*/
	void setCompact(bool compact) {
		_compact = compact;
	}

	/**
	* @param encoding sets character encoding to use
	*/
	void setEncoding(CharacterSet encoding) {
		_encoding = encoding;
	}

	static int GetRecommendedMinimumErrorCorrectionLevel(int n);

private:
	bool _compact;
	Compaction _compaction = Compaction::AUTO;
	CharacterSet _encoding = CharacterSet::ISO8859_1;
	int _minCols = 2;
	int _maxCols = 30;
	int _minRows = 2;
	int _maxRows = 30;
};

} // Pdf417
} // ZXing
