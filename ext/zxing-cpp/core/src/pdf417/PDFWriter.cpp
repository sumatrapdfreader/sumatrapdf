/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFWriter.h"
#include "PDFEncoder.h"
#include "BitMatrix.h"
#include "Utf.h"

#include <utility>

namespace ZXing {
namespace Pdf417 {

/**
* default white space (margin) around the code
*/
static const int WHITE_SPACE = 30;

/**
* default error correction level
*/
static const int DEFAULT_ERROR_CORRECTION_LEVEL = 2;

/**
* Takes and rotates the it 90 degrees
*/
static void RotateArray(const std::vector<std::vector<bool>>& input, std::vector<std::vector<bool>>& output)
{
	size_t height = input.size();
	size_t width = input[0].size();
	output.resize(width);
	for (size_t i = 0; i < width; ++i) {
		output[i].resize(height);
	}
	for (size_t ii = 0; ii < height; ++ii) {
		// This makes the direction consistent on screen when rotating the screen
		size_t inverseii = height - ii - 1;
		for (size_t jj = 0; jj < width; ++jj) {
			output[jj][inverseii] = input[ii][jj];
		}
	}
}

/**
* This takes an array holding the values of the PDF 417
*
* @param input a byte array of information with 0 is black, and 1 is white
* @param margin border around the barcode
* @return BitMatrix of the input
*/
static BitMatrix BitMatrixFromBitArray(const std::vector<std::vector<bool>>& input, int margin)
{
	// Creates the bitmatrix with extra space for whitespace
	int width = Size(input[0]);
	int height = Size(input);
	BitMatrix result(width + 2 * margin, height + 2 * margin);
	for (int y = 0, yOutput = static_cast<int>(result.height()) - margin - 1; y < height; y++, yOutput--) {
		for (int x = 0; x < width; ++x) {
			// Zero is white in the bytematrix
			if (input[y][x]) {
				result.set(x + margin, yOutput);
			}
		}
	}
	return result;
}

BitMatrix
Writer::encode(const std::wstring& contents, int width, int height) const
{
	int margin = _margin >= 0 ? _margin : WHITE_SPACE;
	int ecLevel = _ecLevel >= 0 ? _ecLevel : DEFAULT_ERROR_CORRECTION_LEVEL;

	BarcodeMatrix resultMatrix = _encoder->generateBarcodeLogic(contents, ecLevel);
	int aspectRatio = 4; // keep in sync with MODULE_RATIO in PDFEncoder.cpp
	std::vector<std::vector<bool>> originalScale;
	resultMatrix.getScaledMatrix(1, aspectRatio, originalScale);
	bool rotated = false;
	if ((height > width) != (originalScale[0].size() < originalScale.size())) {
		std::vector<std::vector<bool>> temp;
		RotateArray(originalScale, temp);
		originalScale = temp;
		rotated = true;
	}

	int scaleX = width / Size(originalScale[0]);
	int scaleY = height / Size(originalScale);

	int scale;
	if (scaleX < scaleY) {
		scale = scaleX;
	}
	else {
		scale = scaleY;
	}

	if (scale > 1) {
		std::vector<std::vector<bool>> scaledMatrix;
		resultMatrix.getScaledMatrix(scale, scale * aspectRatio, scaledMatrix);
		if (rotated) {
			std::vector<std::vector<bool>> temp;
			RotateArray(scaledMatrix, temp);
			scaledMatrix = temp;
		}
		return BitMatrixFromBitArray(scaledMatrix, margin);
	}
	else {
		return BitMatrixFromBitArray(originalScale, margin);
	}
}

BitMatrix Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

Writer::Writer()
{
	_encoder.reset(new Encoder);
}

Writer::Writer(Writer &&other) noexcept:
	_margin(other._margin),
	_ecLevel(other._ecLevel),
	_encoder(std::move(other._encoder))
{
}

Writer::~Writer()
{
}

Writer&
Writer::setDimensions(int minCols, int maxCols, int minRows, int maxRows)
{
	_encoder->setDimensions(minCols, maxCols, minRows, maxRows);
	return *this;
}

Writer&
Writer::setCompaction(Compaction compaction)
{
	_encoder->setCompaction(compaction);
	return *this;
}

/**
* @param compact if true, enables compaction
*/
Writer&
Writer::setCompact(bool compact)
{
	_encoder->setCompact(compact);
	return *this;
}

/**
* @param encoding sets character encoding to use
*/
Writer&
Writer::setEncoding(CharacterSet encoding)
{
	_encoder->setEncoding(encoding);
	return *this;
}


} // Pdf417
} // ZXing
