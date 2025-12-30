/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

/**
 * Utilities for building Huffman decoding tables.
 */
final class Huffman {

  private static final int MAX_LENGTH = 15;

  /**
   * Returns reverse(reverse(key, len) + 1, len).
   *
   * <p> reverse(key, len) is the bit-wise reversal of the len least significant bits of key.
   */
  private static int getNextKey(int key, int len) {
    int step = 1 << (len - 1);
    while ((key & step) != 0) {
      step = step >> 1;
    }
    return (key & (step - 1)) + step;
  }

  /**
   * Stores {@code item} in {@code table[0], table[step], table[2 * step] .., table[end]}.
   *
   * <p> Assumes that end is an integer multiple of step.
   */
  private static void replicateValue(int[] table, int offset, int step, int end, int item) {
    int pos = end;
    do {
      pos -= step;
      table[offset + pos] = item;
    } while (pos > 0);
  }

  /**
   * @param count histogram of bit lengths for the remaining symbols,
   * @param len code length of the next processed symbol.
   * @return table width of the next 2nd level table.
   */
  private static int nextTableBitSize(int[] count, int len, int rootBits) {
    int bits = len;
    int left = 1 << (bits - rootBits);
    while (bits < MAX_LENGTH) {
      left -= count[bits];
      if (left <= 0) {
        break;
      }
      bits++;
      left = left << 1;
    }
    return bits - rootBits;
  }

  /**
   * Builds Huffman lookup table assuming code lengths are in symbol order.
   *
   * @return number of slots used by resulting Huffman table
   */
  static int buildHuffmanTable(int[] tableGroup, int tableIdx, int rootBits, int[] codeLengths,
      int codeLengthsSize) {
    final int tableOffset = tableGroup[tableIdx];
    final int[] sorted = new int[codeLengthsSize]; // Symbols sorted by code length.
    // TODO(eustas): fill with zeroes?
    final int[] count = new int[MAX_LENGTH + 1]; // Number of codes of each length.
    final int[] offset = new int[MAX_LENGTH + 1]; // Offsets in sorted table for each length.

    // Build histogram of code lengths.
    for (int sym = 0; sym < codeLengthsSize; ++sym) {
      count[codeLengths[sym]]++;
    }

    // Generate offsets into sorted symbol table by code length.
    offset[1] = 0;
    for (int len = 1; len < MAX_LENGTH; ++len) {
      offset[len + 1] = offset[len] + count[len];
    }

    // Sort symbols by length, by symbol order within each length.
    for (int sym = 0; sym < codeLengthsSize; ++sym) {
      if (codeLengths[sym] != 0) {
        sorted[offset[codeLengths[sym]]++] = sym;
      }
    }

    int tableBits = rootBits;
    int tableSize = 1 << tableBits;
    int totalSize = tableSize;

    // Special case code with only one value.
    if (offset[MAX_LENGTH] == 1) {
      for (int k = 0; k < totalSize; ++k) {
        tableGroup[tableOffset + k] = sorted[0];
      }
      return totalSize;
    }

    // Fill in root table.
    int key = 0;  // Reversed prefix code.
    int symbol = 0;
    int step = 1;
    for (int len = 1; len <= rootBits; ++len) {
      step = step << 1;
      while (count[len] > 0) {
        replicateValue(tableGroup, tableOffset + key, step, tableSize,
            len << 16 | sorted[symbol++]);
        key = getNextKey(key, len);
        count[len]--;
      }
    }

    // Fill in 2nd level tables and add pointers to root table.
    final int mask = totalSize - 1;
    int low = -1;
    int currentOffset = tableOffset;
    step = 1;
    for (int len = rootBits + 1; len <= MAX_LENGTH; ++len) {
      step = step << 1;
      while (count[len] > 0) {
        if ((key & mask) != low) {
          currentOffset += tableSize;
          tableBits = nextTableBitSize(count, len, rootBits);
          tableSize = 1 << tableBits;
          totalSize += tableSize;
          low = key & mask;
          tableGroup[tableOffset + low] =
              (tableBits + rootBits) << 16 | (currentOffset - tableOffset - low);
        }
        replicateValue(tableGroup, currentOffset + (key >> rootBits), step, tableSize,
            (len - rootBits) << 16 | sorted[symbol++]);
        key = getNextKey(key, len);
        count[len]--;
      }
    }
    return totalSize;
  }
}
