/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {

class BitMatrix;
class ByteArray;

namespace DataMatrix {

class Version;

BitMatrix BitMatrixFromCodewords(const ByteArray& codewords, int width, int height);
ByteArray CodewordsFromBitMatrix(const BitMatrix& bits, const Version& version);

} // namespace DataMatrix
} // namespace ZXing
