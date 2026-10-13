/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFScanningDecoder.h"

#include "BitMatrix.h"
#include "DecoderResult.h"
#include "PDFBarcodeMetadata.h"
#include "PDFBarcodeValue.h"
#include "PDFCodewordDecoder.h"
#include "PDFDetectionResult.h"
#include "PDFDecoder.h"
#include "PDFDecoderResultExtra.h"
#include "PDFModulusGF.h"
#include "ZXAlgorithms.h"
#include "ZXTestSupport.h"

#include <cmath>

namespace ZXing {
namespace Pdf417 {

static const int CODEWORD_SKEW_SIZE = 2;
static const int MAX_ERRORS = 3;
static const int MAX_EC_CODEWORDS = 512;

using ModuleBitCountType = std::array<int, CodewordDecoder::BARS_IN_MODULE>;

static int AdjustCodewordStartColumn(const BitMatrix& image, int minColumn, int maxColumn, bool leftToRight, int codewordStartColumn, int imageRow)
{
	int correctedStartColumn = codewordStartColumn;
	int increment = leftToRight ? -1 : 1;
	// there should be no black pixels before the start column. If there are, then we need to start earlier.
	for (int i = 0; i < 2; i++) {
		while ((leftToRight ? correctedStartColumn >= minColumn : correctedStartColumn < maxColumn) &&
			leftToRight == image.get(correctedStartColumn, imageRow)) {
			if (std::abs(codewordStartColumn - correctedStartColumn) > CODEWORD_SKEW_SIZE) {
				return codewordStartColumn;
			}
			correctedStartColumn += increment;
		}
		increment = -increment;
		leftToRight = !leftToRight;
	}
	return correctedStartColumn;
}

static bool GetModuleBitCount(const BitMatrix& image, int minColumn, int maxColumn, bool leftToRight, int startColumn, int imageRow, ModuleBitCountType& moduleBitCount)
{
	int imageColumn = startColumn;
	size_t moduleNumber = 0;
	int increment = leftToRight ? 1 : -1;
	bool previousPixelValue = leftToRight;
	std::fill(moduleBitCount.begin(), moduleBitCount.end(), 0);
	while ((leftToRight ? (imageColumn < maxColumn) : (imageColumn >= minColumn)) && moduleNumber < moduleBitCount.size()) {
		if (image.get(imageColumn, imageRow) == previousPixelValue) {
			moduleBitCount[moduleNumber] += 1;
			imageColumn += increment;
		}
		else {
			moduleNumber += 1;
			previousPixelValue = !previousPixelValue;
		}
	}
	return moduleNumber == moduleBitCount.size() || (imageColumn == (leftToRight ? maxColumn : minColumn) && moduleNumber == moduleBitCount.size() - 1);
}

static bool CheckCodewordSkew(int codewordSize, int minCodewordWidth, int maxCodewordWidth)
{
	return minCodewordWidth - CODEWORD_SKEW_SIZE <= codewordSize &&
		codewordSize <= maxCodewordWidth + CODEWORD_SKEW_SIZE;
}


static ModuleBitCountType GetBitCountForCodeword(int codeword)
{
	ModuleBitCountType result;
	result.fill(0);
	int previousValue = 0;
	int i = Size(result) - 1;
	while (true) {
		if ((codeword & 0x1) != previousValue) {
			previousValue = codeword & 0x1;
			i--;
			if (i < 0) {
				break;
			}
		}
		result[i]++;
		codeword >>= 1;
	}
	return result;
}

static int GetCodewordBucketNumber(const ModuleBitCountType& moduleBitCount)
{
	return (moduleBitCount[0] - moduleBitCount[2] + moduleBitCount[4] - moduleBitCount[6] + 9) % 9;
}

static int GetCodewordBucketNumber(int codeword)
{
	return GetCodewordBucketNumber(GetBitCountForCodeword(codeword));
}

static Nullable<Codeword> DetectCodeword(const BitMatrix& image, int minColumn, int maxColumn, bool leftToRight, int startColumn, int imageRow, int minCodewordWidth, int maxCodewordWidth)
{
	startColumn = AdjustCodewordStartColumn(image, minColumn, maxColumn, leftToRight, startColumn, imageRow);
	// we usually know fairly exact now how long a codeword is. We should provide minimum and maximum expected length
	// and try to adjust the read pixels, e.g. remove single pixel errors or try to cut off exceeding pixels.
	// min and maxCodewordWidth should not be used as they are calculated for the whole barcode an can be inaccurate
	// for the current position
	ModuleBitCountType moduleBitCount;
	if (!GetModuleBitCount(image, minColumn, maxColumn, leftToRight, startColumn, imageRow, moduleBitCount)) {
		return nullptr;
	}
	int endColumn;
	int codewordBitCount = Reduce(moduleBitCount);
	if (leftToRight) {
		endColumn = startColumn + codewordBitCount;
	}
	else {
		std::reverse(moduleBitCount.begin(), moduleBitCount.end());
		endColumn = startColumn;
		startColumn = endColumn - codewordBitCount;
	}
	// TODO implement check for width and correction of black and white bars
	// use start (and maybe stop pattern) to determine if blackbars are wider than white bars. If so, adjust.
	// should probably done only for codewords with a lot more than 17 bits.
	// The following fixes 10-1.png, which has wide black bars and small white bars
	//    for (int i = 0; i < moduleBitCount.length; i++) {
	//      if (i % 2 == 0) {
	//        moduleBitCount[i]--;
	//      } else {
	//        moduleBitCount[i]++;
	//      }
	//    }

	// We could also use the width of surrounding codewords for more accurate results, but this seems
	// sufficient for now
	if (!CheckCodewordSkew(codewordBitCount, minCodewordWidth, maxCodewordWidth)) {
		// We could try to use the startX and endX position of the codeword in the same column in the previous row,
		// create the bit count from it and normalize it to 8. This would help with single pixel errors.
		return nullptr;
	}

	int decodedValue = CodewordDecoder::GetDecodedValue(moduleBitCount);
	if (decodedValue != -1) {
		int codeword = CodewordDecoder::GetCodeword(decodedValue);
		if (codeword != -1) {
			return Codeword(startColumn, endColumn, GetCodewordBucketNumber(decodedValue), codeword);
		}
	}
	return nullptr;
}

static DetectionResultColumn GetRowIndicatorColumn(const BitMatrix& image, const BoundingBox& boundingBox, const ResultPoint& startPoint, bool leftToRight, int minCodewordWidth, int maxCodewordWidth)
{
	DetectionResultColumn rowIndicatorColumn(boundingBox, leftToRight ? DetectionResultColumn::RowIndicator::Left : DetectionResultColumn::RowIndicator::Right);
	for (int i = 0; i < 2; i++) {
		int increment = i == 0 ? 1 : -1;
		int startColumn = (int)startPoint.x();
		for (int imageRow = (int)startPoint.y(); imageRow <= boundingBox.maxY() && imageRow >= boundingBox.minY(); imageRow += increment) {
			auto codeword = DetectCodeword(image, 0, image.width(), leftToRight, startColumn, imageRow, minCodewordWidth, maxCodewordWidth);
			if (codeword != nullptr) {
				rowIndicatorColumn.setCodeword(imageRow, codeword);
				if (leftToRight) {
					startColumn = codeword.value().startX();
				}
				else {
					startColumn = codeword.value().endX();
				}
			}
		}
	}
	return rowIndicatorColumn;
}

static bool GetBarcodeMetadata(Nullable<DetectionResultColumn>& leftRowIndicatorColumn, Nullable<DetectionResultColumn>& rightRowIndicatorColumn, BarcodeMetadata& result)
{
	BarcodeMetadata leftBarcodeMetadata;
	if (leftRowIndicatorColumn == nullptr || !leftRowIndicatorColumn.value().getBarcodeMetadata(leftBarcodeMetadata)) {
		return rightRowIndicatorColumn != nullptr && rightRowIndicatorColumn.value().getBarcodeMetadata(result);
	}

	BarcodeMetadata rightBarcodeMetadata;
	if (rightRowIndicatorColumn == nullptr || !rightRowIndicatorColumn.value().getBarcodeMetadata(rightBarcodeMetadata)) {
		result = leftBarcodeMetadata;
		return true;
	}

	if (leftBarcodeMetadata.columnCount() != rightBarcodeMetadata.columnCount() &&
		leftBarcodeMetadata.errorCorrectionLevel() != rightBarcodeMetadata.errorCorrectionLevel() &&
		leftBarcodeMetadata.rowCount() != rightBarcodeMetadata.rowCount()) {
		return false;
	}
	result = leftBarcodeMetadata;
	return true;
}

template <typename Iter>
static auto GetMax(Iter start, Iter end) -> typename std::remove_reference<decltype(*start)>::type
{
	auto it = std::max_element(start, end);
	return it != end ? *it : -1;
}

static bool AdjustBoundingBox(Nullable<DetectionResultColumn>& rowIndicatorColumn, Nullable<BoundingBox>& result)
{
	if (rowIndicatorColumn == nullptr) {
		result = nullptr;
		return true;
	}
	std::vector<int> rowHeights;
	if (!rowIndicatorColumn.value().getRowHeights(rowHeights)) {
		result = nullptr;
		return true;
	}
	int maxRowHeight = GetMax(rowHeights.begin(), rowHeights.end());
	int missingStartRows = 0;
	for (int rowHeight : rowHeights) {
		missingStartRows += maxRowHeight - rowHeight;
		if (rowHeight > 0) {
			break;
		}
	}
	auto& codewords = rowIndicatorColumn.value().allCodewords();
	for (int row = 0; missingStartRows > 0 && codewords[row] == nullptr; row++) {
		missingStartRows--;
	}
	int missingEndRows = 0;
	for (int row = Size(rowHeights) - 1; row >= 0; row--) {
		missingEndRows += maxRowHeight - rowHeights[row];
		if (rowHeights[row] > 0) {
			break;
		}
	}
	for (int row = Size(codewords) - 1; missingEndRows > 0 && codewords[row] == nullptr; row--) {
		missingEndRows--;
	}
	BoundingBox box;
	if (BoundingBox::AddMissingRows(rowIndicatorColumn.value().boundingBox(), missingStartRows, missingEndRows, rowIndicatorColumn.value().isLeftRowIndicator(), box)) {
		result = box;
		return true;
	}
	return false;
}

static bool Merge(Nullable<DetectionResultColumn>& leftRowIndicatorColumn, Nullable<DetectionResultColumn>& rightRowIndicatorColumn, DetectionResult& result)
{
	if (leftRowIndicatorColumn != nullptr || rightRowIndicatorColumn != nullptr) {
		BarcodeMetadata barcodeMetadata;
		if (GetBarcodeMetadata(leftRowIndicatorColumn, rightRowIndicatorColumn, barcodeMetadata)) {
			Nullable<BoundingBox> leftBox, rightBox, mergedBox;
			if (AdjustBoundingBox(leftRowIndicatorColumn, leftBox) && AdjustBoundingBox(rightRowIndicatorColumn, rightBox) && BoundingBox::Merge(leftBox, rightBox, mergedBox)) {
				result.init(barcodeMetadata, mergedBox);
				return true;
			}
		}
	}
	return false;
}

static bool IsValidBarcodeColumn(const DetectionResult& detectionResult, int barcodeColumn)
{
	return barcodeColumn >= 0 && barcodeColumn <= detectionResult.barcodeColumnCount() + 1;
}

static int GetStartColumn(const DetectionResult& detectionResult, int barcodeColumn, int imageRow, bool leftToRight)
{
	int offset = leftToRight ? 1 : -1;
	Nullable<Codeword> codeword;
	if (IsValidBarcodeColumn(detectionResult, barcodeColumn - offset)) {
		codeword = detectionResult.column(barcodeColumn - offset).value().codeword(imageRow);
	}
	if (codeword != nullptr) {
		return leftToRight ? codeword.value().endX() : codeword.value().startX();
	}
	codeword = detectionResult.column(barcodeColumn).value().codewordNearby(imageRow);
	if (codeword != nullptr) {
		return leftToRight ? codeword.value().startX() : codeword.value().endX();
	}
	if (IsValidBarcodeColumn(detectionResult, barcodeColumn - offset)) {
		codeword = detectionResult.column(barcodeColumn - offset).value().codewordNearby(imageRow);
	}
	if (codeword != nullptr) {
		return leftToRight ? codeword.value().endX() : codeword.value().startX();
	}
	int skippedColumns = 0;

	while (IsValidBarcodeColumn(detectionResult, barcodeColumn - offset)) {
		barcodeColumn -= offset;
		for (auto& previousRowCodeword : detectionResult.column(barcodeColumn).value().allCodewords()) {
			if (previousRowCodeword != nullptr) {
				return (leftToRight ? previousRowCodeword.value().endX() : previousRowCodeword.value().startX()) +
					offset *
					skippedColumns *
					(previousRowCodeword.value().endX() - previousRowCodeword.value().startX());
			}
		}
		skippedColumns++;
	}
	return leftToRight ? detectionResult.getBoundingBox().value().minX() : detectionResult.getBoundingBox().value().maxX();
}

static std::vector<std::vector<BarcodeValue>> CreateBarcodeMatrix(DetectionResult& detectionResult)
{
	std::vector<std::vector<BarcodeValue>> barcodeMatrix(detectionResult.barcodeRowCount());
	for (auto& row : barcodeMatrix) {
		row.resize(detectionResult.barcodeColumnCount() + 2);
	}

	int column = 0;
	for (auto& resultColumn : detectionResult.allColumns()) {
		if (resultColumn != nullptr) {
			for (auto& codeword : resultColumn.value().allCodewords()) {
				if (codeword != nullptr) {
					int rowNumber = codeword.value().rowNumber();
					if (rowNumber >= 0) {
						if (rowNumber >= Size(barcodeMatrix)) {
							// We have more rows than the barcode metadata allows for, ignore them.
							continue;
						}
						barcodeMatrix[rowNumber][column].setValue(codeword.value().value());
					}
				}
			}
		}
		column++;
	}
	return barcodeMatrix;
}

static int GetNumberOfECCodeWords(int barcodeECLevel)
{
	return 2 << barcodeECLevel;
}

static bool AdjustCodewordCount(const DetectionResult& detectionResult, std::vector<std::vector<BarcodeValue>>& barcodeMatrix)
{
	auto numberOfCodewords = barcodeMatrix[0][1].value();
	int calculatedNumberOfCodewords = detectionResult.barcodeColumnCount() * detectionResult.barcodeRowCount() - GetNumberOfECCodeWords(detectionResult.barcodeECLevel());
	if (calculatedNumberOfCodewords < 1 || calculatedNumberOfCodewords > CodewordDecoder::MAX_CODEWORDS_IN_BARCODE)
		calculatedNumberOfCodewords = 0;
	if (numberOfCodewords.empty()) {
		if (!calculatedNumberOfCodewords)
			return false;
		barcodeMatrix[0][1].setValue(calculatedNumberOfCodewords);
	}
	else if (calculatedNumberOfCodewords && numberOfCodewords[0] != calculatedNumberOfCodewords) {
		// The calculated one is more reliable as it is derived from the row indicator columns
		barcodeMatrix[0][1].setValue(calculatedNumberOfCodewords);
	}
	return true;
}
// +++++++++++++++++++++++++++++++++++ Error Correction

static const ModulusGF& GetModulusGF()
{
	static const ModulusGF field(CodewordDecoder::NUMBER_OF_CODEWORDS, 3);
	return field;
}

static bool RunEuclideanAlgorithm(ModulusPoly a, ModulusPoly b, int R, ModulusPoly& sigma, ModulusPoly& omega)
{
	const ModulusGF& field = GetModulusGF();

	// Assume a's degree is >= b's
	if (a.degree() < b.degree()) {
		swap(a, b);
	}

	ModulusPoly rLast = a;
	ModulusPoly r = b;
	ModulusPoly tLast = field.zero();
	ModulusPoly t = field.one();

	// Run Euclidean algorithm until r's degree is less than R/2
	while (r.degree() >= R / 2) {
		ModulusPoly rLastLast = rLast;
		ModulusPoly tLastLast = tLast;
		rLast = r;
		tLast = t;

		// Divide rLastLast by rLast, with quotient in q and remainder in r
		if (rLast.isZero()) {
			// Oops, Euclidean algorithm already terminated?
			return false;
		}
		r = rLastLast;
		ModulusPoly q = field.zero();
		int denominatorLeadingTerm = rLast.coefficient(rLast.degree());
		int dltInverse = field.inverse(denominatorLeadingTerm);
		while (r.degree() >= rLast.degree() && !r.isZero()) {
			int degreeDiff = r.degree() - rLast.degree();
			int scale = field.multiply(r.coefficient(r.degree()), dltInverse);
			q = q.add(field.buildMonomial(degreeDiff, scale));
			r = r.subtract(rLast.multiplyByMonomial(degreeDiff, scale));
		}

		t = q.multiply(tLast).subtract(tLastLast).negative();
	}

	int sigmaTildeAtZero = t.coefficient(0);
	if (sigmaTildeAtZero == 0) {
		return false;
	}

	int inverse = field.inverse(sigmaTildeAtZero);
	sigma = t.multiply(inverse);
	omega = r.multiply(inverse);
	return true;
}

static bool FindErrorLocations(const ModulusPoly& errorLocator, std::vector<int>& result)
{
	const ModulusGF& field = GetModulusGF();
	// This is a direct application of Chien's search
	int numErrors = errorLocator.degree();
	result.resize(numErrors);
	int e = 0;
	for (int i = 1; i < field.size() && e < numErrors; i++) {
		if (errorLocator.evaluateAt(i) == 0) {
			result[e] = field.inverse(i);
			e++;
		}
	}
	return e == numErrors;
}

static std::vector<int> FindErrorMagnitudes(const ModulusPoly& errorEvaluator, const ModulusPoly& errorLocator, const std::vector<int>& errorLocations)
{
	const ModulusGF& field = GetModulusGF();
	int errorLocatorDegree = errorLocator.degree();
	std::vector<int> formalDerivativeCoefficients(errorLocatorDegree);
	for (int i = 1; i <= errorLocatorDegree; i++) {
		formalDerivativeCoefficients[errorLocatorDegree - i] = field.multiply(i, errorLocator.coefficient(i));
	}

	ModulusPoly formalDerivative(field, formalDerivativeCoefficients);
	// This is directly applying Forney's Formula
	std::vector<int> result(errorLocations.size());
	for (size_t i = 0; i < result.size(); i++) {
		int xiInverse = field.inverse(errorLocations[i]);
		int numerator = field.subtract(0, errorEvaluator.evaluateAt(xiInverse));
		int denominator = field.inverse(formalDerivative.evaluateAt(xiInverse));
		result[i] = field.multiply(numerator, denominator);
	}
	return result;
}

/**
* @param received received codewords
* @param numECCodewords number of those codewords used for EC
* @param erasures location of erasures
* @return false if errors cannot be corrected, maybe because of too many errors
*/
ZXING_EXPORT_TEST_ONLY
bool DecodeErrorCorrection(std::vector<int>& received, int numECCodewords, const std::vector<int>& erasures [[maybe_unused]], int& nbErrors)
{
	const ModulusGF& field = GetModulusGF();
	ModulusPoly poly(field, received);
	std::vector<int> S(numECCodewords);
	bool error = false;
	for (int i = numECCodewords; i > 0; i--) {
		int eval = poly.evaluateAt(field.exp(i));
		S[numECCodewords - i] = eval;
		if (eval != 0) {
			error = true;
		}
	}

	if (!error) {
		nbErrors = 0;
		return true;
	}

//	ModulusPoly knownErrors = field.one();
//	for (int erasure : erasures) {
//		int b = field.exp(Size(received) - 1 - erasure);
//		// Add (1 - bx) term:
//		ModulusPoly term(field, { field.subtract(0, b), 1 });
//		knownErrors = knownErrors.multiply(term);
//	}

	ModulusPoly syndrome(field, S);
//	syndrome = syndrome.multiply(knownErrors);

	ModulusPoly sigma, omega;
	if (!RunEuclideanAlgorithm(field.buildMonomial(numECCodewords, 1), syndrome, numECCodewords, sigma, omega)) {
		return false;
	}

//	sigma = sigma.multiply(knownErrors);

	std::vector<int> errorLocations;
	if (!FindErrorLocations(sigma, errorLocations)) {
		return false;
	}

	std::vector<int> errorMagnitudes = FindErrorMagnitudes(omega, sigma, errorLocations);

	int receivedSize = Size(received);
	for (size_t i = 0; i < errorLocations.size(); i++) {
		int position = receivedSize - 1 - field.log(errorLocations[i]);
		if (position < 0) {
			return false;
		}
		received[position] = field.subtract(received[position], errorMagnitudes[i]);
	}
	nbErrors = Size(errorLocations);
	return true;
}

// --------------------------------------- Error Correction

/**
* <p>Given data and error-correction codewords received, possibly corrupted by errors, attempts to
* correct the errors in-place.</p>
*
* @param codewords   data and error correction codewords
* @param erasures positions of any known erasures
* @param numECCodewords number of error correction codewords that are available in codewords
* @return false if error correction fails
*/
static bool CorrectErrors(std::vector<int>& codewords, const std::vector<int>& erasures, int numECCodewords, int& errorCount)
{
	if (Size(erasures) > numECCodewords / 2 + MAX_ERRORS ||
		numECCodewords < 0 ||
		numECCodewords > MAX_EC_CODEWORDS) {
		// Too many errors or EC Codewords is corrupted
		return false;
	}
	return DecodeErrorCorrection(codewords, numECCodewords, erasures, errorCount);
}

/**
* Verify that all is OK with the codeword array.
*/
static bool VerifyCodewordCount(std::vector<int>& codewords, int numECCodewords)
{
	if (codewords.size() < 4) {
		// Codeword array size should be at least 4 allowing for
		// Count CW, At least one Data CW, Error Correction CW, Error Correction CW
		return false;
	}
	// The first codeword, the Symbol Length Descriptor, shall always encode the total number of data
	// codewords in the symbol, including the Symbol Length Descriptor itself, data codewords and pad
	// codewords, but excluding the number of error correction codewords.
	int numberOfCodewords = codewords[0];
	if (numberOfCodewords > Size(codewords)) {
		return false;
	}

	assert(numECCodewords >= 2);
	if (numberOfCodewords + numECCodewords != Size(codewords)) {
		// Reset to the length of the array less number of Error Codewords
		if (numECCodewords < Size(codewords)) {
			codewords[0] = Size(codewords) - numECCodewords;
		}
		else {
			return false;
		}
	}
	return true;
}

static DecoderResult DecodeCodewords(std::vector<int>& codewords, int numECCodewords, const std::vector<int>& erasures)
{
	if (codewords.empty())
		return FormatError();

	int correctedErrorsCount = 0;
	if (!CorrectErrors(codewords, erasures, numECCodewords, correctedErrorsCount))
		return ChecksumError();

	if (!VerifyCodewordCount(codewords, numECCodewords))
		return FormatError();

	// Decode the codewords
	return Decode(codewords).setEcLevel(std::to_string(numECCodewords * 100 / Size(codewords)) + "%");
}

DecoderResult DecodeCodewords(std::vector<int>& codewords, int numECCodeWords)
{
	for (auto& cw : codewords)
		cw = std::clamp(cw, 0, CodewordDecoder::MAX_CODEWORDS_IN_BARCODE);

	// erasures array has never been actually used inside the error correction code
	return DecodeCodewords(codewords, numECCodeWords, {});
}


/**
* This method deals with the fact, that the decoding process doesn't always yield a single most likely value. The
* current error correction implementation doesn't deal with erasures very well, so it's better to provide a value
* for these ambiguous codewords instead of treating it as an erasure. The problem is that we don't know which of
* the ambiguous values to choose. We try decode using the first value, and if that fails, we use another of the
* ambiguous values and try to decode again. This usually only happens on very hard to read and decode barcodes,
* so decoding the normal barcodes is not affected by this.
*
* @param erasureArray contains the indexes of erasures
* @param ambiguousIndexes array with the indexes that have more than one most likely value
* @param ambiguousIndexValues two dimensional array that contains the ambiguous values. The first dimension must
* be the same length as the ambiguousIndexes array
*/
static DecoderResult CreateDecoderResultFromAmbiguousValues(int ecLevel, std::vector<int>& codewords,
	const std::vector<int>& erasureArray, const std::vector<int>& ambiguousIndexes,
	const std::vector<std::vector<int>>& ambiguousIndexValues)
{
	std::vector<int> ambiguousIndexCount(ambiguousIndexes.size(), 0);

	int tries = 100;
	while (tries-- > 0) {
		for (size_t i = 0; i < ambiguousIndexCount.size(); i++) {
			codewords[ambiguousIndexes[i]] = ambiguousIndexValues[i][ambiguousIndexCount[i]];
		}
		auto result = DecodeCodewords(codewords, NumECCodeWords(ecLevel), erasureArray);
		if (result.error() != Error::Checksum) {
			return result;
		}

		if (ambiguousIndexCount.empty()) {
			return ChecksumError();
		}
		for (size_t i = 0; i < ambiguousIndexCount.size(); i++) {
			if (ambiguousIndexCount[i] < Size(ambiguousIndexValues[i]) - 1) {
				ambiguousIndexCount[i]++;
				break;
			}
			else {
				ambiguousIndexCount[i] = 0;
				if (i == ambiguousIndexCount.size() - 1) {
					return ChecksumError();
				}
			}
		}
	}
	return ChecksumError();
}


static DecoderResult CreateDecoderResult(DetectionResult& detectionResult)
{
	auto barcodeMatrix = CreateBarcodeMatrix(detectionResult);
	if (!AdjustCodewordCount(detectionResult, barcodeMatrix)) {
		return {};
	}
	std::vector<int> erasures;
	std::vector<int> codewords(detectionResult.barcodeRowCount() * detectionResult.barcodeColumnCount(), 0);
	std::vector<std::vector<int>> ambiguousIndexValues;
	std::vector<int> ambiguousIndexesList;
	for (int row = 0; row < detectionResult.barcodeRowCount(); row++) {
		for (int column = 0; column < detectionResult.barcodeColumnCount(); column++) {
			auto values = barcodeMatrix[row][column + 1].value();
			int codewordIndex = row * detectionResult.barcodeColumnCount() + column;
			if (values.empty()) {
				erasures.push_back(codewordIndex);
			}
			else if (values.size() == 1) {
				codewords[codewordIndex] = values[0];
			}
			else {
				ambiguousIndexesList.push_back(codewordIndex);
				ambiguousIndexValues.push_back(values);
			}
		}
	}
	return CreateDecoderResultFromAmbiguousValues(detectionResult.barcodeECLevel(), codewords, erasures,
												  ambiguousIndexesList, ambiguousIndexValues);
}


// TODO don't pass in minCodewordWidth and maxCodewordWidth, pass in barcode columns for start and stop pattern
// columns. That way width can be deducted from the pattern column.
// This approach also allows detecting more details about the barcode, e.g. if a bar type (white or black) is wider
// than it should be. This can happen if the scanner used a bad blackpoint.
DecoderResult
ScanningDecoder::Decode(const BitMatrix& image, const Nullable<ResultPoint>& imageTopLeft, const Nullable<ResultPoint>& imageBottomLeft,
	const Nullable<ResultPoint>& imageTopRight, const Nullable<ResultPoint>& imageBottomRight,
	int minCodewordWidth, int maxCodewordWidth)
{
	BoundingBox boundingBox;
	if (!BoundingBox::Create(image.width(), image.height(), imageTopLeft, imageBottomLeft, imageTopRight, imageBottomRight, boundingBox)) {
		return {};
	}

	Nullable<DetectionResultColumn> leftRowIndicatorColumn, rightRowIndicatorColumn;
	DetectionResult detectionResult;
	for (int i = 0; i < 2; i++) {
		if (imageTopLeft != nullptr) {
			leftRowIndicatorColumn = GetRowIndicatorColumn(image, boundingBox, imageTopLeft, true, minCodewordWidth, maxCodewordWidth);
		}
		if (imageTopRight != nullptr) {
			rightRowIndicatorColumn = GetRowIndicatorColumn(image, boundingBox, imageTopRight, false, minCodewordWidth, maxCodewordWidth);
		}
		if (!Merge(leftRowIndicatorColumn, rightRowIndicatorColumn, detectionResult)) {
			return {};
		}
		if (i == 0 && detectionResult.getBoundingBox() != nullptr && (detectionResult.getBoundingBox().value().minY() < boundingBox.minY() || detectionResult.getBoundingBox().value().maxY() > boundingBox.maxY())) {
			boundingBox = detectionResult.getBoundingBox();
		}
		else {
			detectionResult.setBoundingBox(boundingBox);
			break;
		}
	}

	int maxBarcodeColumn = detectionResult.barcodeColumnCount() + 1;
	detectionResult.setColumn(0, leftRowIndicatorColumn);
	detectionResult.setColumn(maxBarcodeColumn, rightRowIndicatorColumn);

	bool leftToRight = leftRowIndicatorColumn != nullptr;
	for (int barcodeColumnCount = 1; barcodeColumnCount <= maxBarcodeColumn; barcodeColumnCount++) {
		int barcodeColumn = leftToRight ? barcodeColumnCount : maxBarcodeColumn - barcodeColumnCount;
		if (detectionResult.column(barcodeColumn) != nullptr) {
			// This will be the case for the opposite row indicator column, which doesn't need to be decoded again.
			continue;
		}
		DetectionResultColumn::RowIndicator rowIndicator = barcodeColumn == 0 ? DetectionResultColumn::RowIndicator::Left : (barcodeColumn == maxBarcodeColumn ? DetectionResultColumn::RowIndicator::Right : DetectionResultColumn::RowIndicator::None);
		detectionResult.setColumn(barcodeColumn, DetectionResultColumn(boundingBox, rowIndicator));
		int startColumn = -1;
		int previousStartColumn = startColumn;
		// TODO start at a row for which we know the start position, then detect upwards and downwards from there.
		for (int imageRow = boundingBox.minY(); imageRow <= boundingBox.maxY(); imageRow++) {
			startColumn = GetStartColumn(detectionResult, barcodeColumn, imageRow, leftToRight);
			if (startColumn < 0 || startColumn > boundingBox.maxX()) {
				if (previousStartColumn == -1) {
					continue;
				}
				startColumn = previousStartColumn;
			}
			Nullable<Codeword> codeword = DetectCodeword(image, boundingBox.minX(), boundingBox.maxX(), leftToRight, startColumn, imageRow, minCodewordWidth, maxCodewordWidth);
			if (codeword != nullptr) {
				detectionResult.column(barcodeColumn).value().setCodeword(imageRow, codeword);
				previousStartColumn = startColumn;
				UpdateMinMax(minCodewordWidth, maxCodewordWidth, codeword.value().width());
			}
		}
	}
	auto res = CreateDecoderResult(detectionResult);
	auto meta = dynamic_cast<DecoderResultExtra*>(res.extra().get());
	if (meta)
		meta->approxSymbolWidth = (detectionResult.barcodeColumnCount() + 2) * (minCodewordWidth + maxCodewordWidth) / 2;
	return res;
}

} // Pdf417
} // ZXing
