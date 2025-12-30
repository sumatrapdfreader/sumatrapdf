/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

import java.io.InputStream;

final class State {
  byte[] ringBuffer;
  byte[] contextModes;
  byte[] contextMap;
  byte[] distContextMap;
  byte[] distExtraBits;
  byte[] output;
  byte[] byteBuffer;  // BitReader

  short[] shortBuffer; // BitReader

  int[] intBuffer;  // BitReader
  int[] rings;
  int[] blockTrees;
  int[] literalTreeGroup;
  int[] commandTreeGroup;
  int[] distanceTreeGroup;
  int[] distOffset;

  long accumulator64;  // BitReader: pre-fetched bits.

  int runningState;  // Default value is 0 == Decode.UNINITIALIZED
  int nextRunningState;
  int accumulator32;  // BitReader: pre-fetched bits.
  int bitOffset;  // BitReader: bit-reading position in accumulator.
  int halfOffset;  // BitReader: offset of next item in intBuffer/shortBuffer.
  int tailBytes;  // BitReader: number of bytes in unfinished half.
  int endOfStreamReached;  // BitReader: input stream is finished.
  int metaBlockLength;
  int inputEnd;
  int isUncompressed;
  int isMetadata;
  int literalBlockLength;
  int numLiteralBlockTypes;
  int commandBlockLength;
  int numCommandBlockTypes;
  int distanceBlockLength;
  int numDistanceBlockTypes;
  int pos;
  int maxDistance;
  int distRbIdx;
  int trivialLiteralContext;
  int literalTreeIdx;
  int commandTreeIdx;
  int j;
  int insertLength;
  int contextMapSlice;
  int distContextMapSlice;
  int contextLookupOffset1;
  int contextLookupOffset2;
  int distanceCode;
  int numDirectDistanceCodes;
  int distancePostfixBits;
  int distance;
  int copyLength;
  int maxBackwardDistance;
  int maxRingBufferSize;
  int ringBufferSize;
  int expectedTotalSize;
  int outputOffset;
  int outputLength;
  int outputUsed;
  int ringBufferBytesWritten;
  int ringBufferBytesReady;
  int isEager;
  int isLargeWindow;

  // Compound dictionary
  int cdNumChunks;
  int cdTotalSize;
  int cdBrIndex;
  int cdBrOffset;
  int cdBrLength;
  int cdBrCopied;
  byte[][] cdChunks;
  int[] cdChunkOffsets;
  int cdBlockBits;
  byte[] cdBlockMap;

  InputStream input = Utils.makeEmptyInput();  // BitReader

  State() {
    this.ringBuffer = new byte[0];
    this.rings = new int[10];
    this.rings[0] = 16;
    this.rings[1] = 15;
    this.rings[2] = 11;
    this.rings[3] = 4;
  }
}
