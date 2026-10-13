/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2006 Jeremias Maerki
* Copyright 2020 Axel Waggersauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMBitLayout.h"

#include "BitArray.h"
#include "BitMatrix.h"
#include "ByteArray.h"
#include "DMVersion.h"

#include <array>
#include <cstddef>

namespace ZXing::DataMatrix {

struct BitPos
{
	int row, col;
};

using BitPosArray = std::array<BitPos, 8>;

/**
 * VisitMatrix gets a functor/callback that is responsible for processing/visiting the bits in the matrix.
 * The return value contains a BitMatrix where each bit that has been visited is set.
 */
template <typename VisitFunc>
BitMatrix VisitMatrix(int numRows, int numCols, VisitFunc visit)
{
	// <p>See ISO 16022:2006, Figure F.3 to F.6</p>
	const BitPosArray CORNER1 = {{{-1, 0}, {-1, 1}, {-1, 2}, {0, -2}, {0, -1}, {1, -1}, {2, -1}, {3, -1}}};
	const BitPosArray CORNER2 = {{{-3, 0}, {-2, 0}, {-1, 0}, {0, -4}, {0, -3}, {0, -2}, {0, -1}, {1, -1}}};
	const BitPosArray CORNER3 = {{{-1, 0}, {-1,-1}, {0, -3}, {0, -2}, {0, -1}, {1, -3}, {1, -2}, {1, -1}}};
	const BitPosArray CORNER4 = {{{-3, 0}, {-2, 0}, {-1, 0}, {0, -2}, {0, -1}, {1, -1}, {2, -1}, {3, -1}}};

	BitMatrix visited(numCols, numRows);
	auto logAccess = [&visited](BitPos p){ visited.set(p.col, p.row); };

	int row = 4;
	int col = 0;

	auto corner = [&numRows, &numCols, logAccess](const BitPosArray& corner) {
		auto clamp = [](int i, int max) { return i < 0 ? i + max : i; };
		BitPosArray result;
		for (size_t bit = 0; bit < 8; ++bit) {
			result[bit] = {clamp(corner[bit].row, numRows), clamp(corner[bit].col, numCols)};
			logAccess(result[bit]);
		}
		return result;
	};

	auto utah = [&numRows, &numCols, logAccess](int row, int col) {
		const BitPosArray delta = {{{-2, -2}, {-2, -1}, {-1, -2}, {-1, -1}, {-1, 0}, {0, -2}, {0, -1}, {0, 0}}};

		BitPosArray result;
		for (size_t bit = 0; bit < 8; ++bit) {
			int r = row + delta[bit].row;
			int c = col + delta[bit].col;
			if (r < 0) {
				r += numRows;
				c += 4 - ((numRows + 4) % 8);
			}
			if (c < 0) {
				c += numCols;
				r += 4 - ((numCols + 4) % 8);
			}
			if (r >= numRows) {
				r -= numRows;
			}
			result[bit] = {r, c};
			logAccess(result[bit]);
		}
		return result;
	};

	do {
		// Check the four corner cases
		if ((row == numRows) && (col == 0))
			visit(corner(CORNER1));
		else if ((row == numRows - 2) && (col == 0) && (numCols % 4 != 0))
			visit(corner(CORNER2));
		else if ((row == numRows + 4) && (col == 2) && (numCols % 8 == 0))
			visit(corner(CORNER3));
		else if ((row == numRows - 2) && (col == 0) && (numCols % 8 == 4))
			visit(corner(CORNER4));

		// Sweep upward diagonally to the right
		do {
			if ((row < numRows) && (col >= 0) && !visited.get(col, row))
				visit(utah(row, col));

			row -= 2;
			col += 2;
		} while (row >= 0 && col < numCols);

		row += 1;
		col += 3;

		// Sweep downward diagonally to the left
		do {
			if ((row >= 0) && (col < numCols) && !visited.get(col, row))
				visit(utah(row, col));

			row += 2;
			col -= 2;
		} while ((row < numRows) && (col >= 0));

		row += 3;
		col += 1;
	} while ((row < numRows) || (col < numCols));

	return visited;
}

/**
* Symbol Character Placement Program. Adapted from Annex M.1 in ISO/IEC 16022:2000(E).
*/
BitMatrix BitMatrixFromCodewords(const ByteArray& codewords, int width, int height)
{
	BitMatrix result(width, height);

	auto codeword = codewords.begin();

	auto visited = VisitMatrix(height, width, [&codeword, &result](const BitPosArray& bitPos) {
		// Places the 8 bits of a corner or the utah-shaped symbol character in the result matrix
		uint8_t mask = 0x80;
		for (auto& p : bitPos) {
			if (*codeword & mask)
				result.set(p.col, p.row);
			mask >>= 1;
		}
		++codeword;
	});

	if (codeword != codewords.end())
		return {};

	// Lastly, if the lower righthand corner is untouched, fill in fixed pattern
	if (!visited.get(width - 1, height - 1)) {
		result.set(width - 1, height - 1);
		result.set(width - 2, height - 2);
	}

	return result;
}

// Extracts the data bits from a BitMatrix that contains alignment patterns.
static BitMatrix ExtractDataBits(const Version& version, const BitMatrix& bits)
{
	BitMatrix res(version.dataWidth(), version.dataHeight());

	for (int y = 0; y < res.height(); ++y)
		for (int x = 0; x < res.width(); ++x) {
			int ix = x + 1 + (x / version.dataBlockWidth) * 2;
			int iy = y + 1 + (y / version.dataBlockHeight) * 2;
			res.set(x, y, bits.get(ix, iy));
		}

	return res;
}

/**
* <p>Reads the bits in the {@link BitMatrix} representing the mapping matrix (No alignment patterns)
* in the correct order in order to reconstitute the codewords bytes contained within the
* Data Matrix Code.</p>
*
* @return bytes encoded within the Data Matrix Code
*/
ByteArray CodewordsFromBitMatrix(const BitMatrix& bits, const Version& version)
{
	BitMatrix dataBits = ExtractDataBits(version, bits);

	ByteArray result(version.totalCodewords());
	auto codeword = result.begin();

	VisitMatrix(dataBits.height(), dataBits.width(), [&codeword, &dataBits](const BitPosArray& bitPos) {
		// Read the 8 bits of one of the special corner/utah symbols into the current codeword
		*codeword = 0;
		for (auto& p : bitPos)
			AppendBit(*codeword, dataBits.get(p.col, p.row));
		++codeword;
	});

	if (codeword != result.end())
		return {};

	return result;
}

} // namespace ZXing::DataMatrix
