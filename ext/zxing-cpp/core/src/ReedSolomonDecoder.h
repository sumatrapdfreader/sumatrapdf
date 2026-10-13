/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

namespace ZXing {

class GenericGF;

/**
* <p>Implements Reed-Solomon decoding, as the name implies.</p>
*
* <p>The algorithm will not be explained here, but the following references were helpful
* in creating this implementation:</p>
*
* <ul>
* <li>Bruce Maggs.
* <a href="http://www.cs.cmu.edu/afs/cs.cmu.edu/project/pscico-guyb/realworld/www/rs_decode.ps">
* "Decoding Reed-Solomon Codes"</a> (see discussion of Forney's Formula)</li>
* <li>J.I. Hall. <a href="www.mth.msu.edu/~jhall/classes/codenotes/GRS.pdf">
* "Chapter 5. Generalized Reed-Solomon Codes"</a>
* (see discussion of Euclidean algorithm)</li>
* </ul>
*
* <p>Much credit is due to William Rucklidge since portions of this code are an indirect
* port of his C++ Reed-Solomon implementation.</p>
*
* @author Sean Owen
* @author William Rucklidge
* @author sanfordsquires
*/

/**
 * @brief ReedSolomonDecode fixes errors in a message containing both data and parity codewords.
 *
 * @param message data and error-correction/parity codewords
 * @param numECCodeWords number of error-correction code words
 * @return true iff message errors could successfully be fixed (or there have not been any)
 */
bool ReedSolomonDecode(const GenericGF& field, std::vector<int>& message, int numECCodeWords);

} // ZXing
