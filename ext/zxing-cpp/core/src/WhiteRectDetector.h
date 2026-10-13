/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {

class BitMatrix;
class ResultPoint;

/**
 * <p>
 * Detects a candidate barcode-like rectangular region within an image. It
 * starts around the center of the image, increases the size of the candidate
 * region until it finds a white rectangular region.
 * </p>
 *
 * @param image barcode image to find a rectangle in
 * @param initSize initial size of search area around center
 * @param x x position of search center
 * @param y y position of search center
 * @return {@link ResultPoint}[] describing the corners of the rectangular
 *         region. The first and last points are opposed on the diagonal, as
 *         are the second and third. The first point will be the topmost
 *         point and the last, the bottommost. The second point will be
 *         leftmost and the third, the rightmost
 * @return true iff white rect was found
 */
bool DetectWhiteRect(const BitMatrix& image, int initSize, int x, int y, ResultPoint& p0, ResultPoint& p1,
					 ResultPoint& p2, ResultPoint& p3);
bool DetectWhiteRect(const BitMatrix& image, ResultPoint& p0, ResultPoint& p1, ResultPoint& p2, ResultPoint& p3);

} // ZXing
