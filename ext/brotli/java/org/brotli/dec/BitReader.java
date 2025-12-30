/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

/**
 * Bit reading helpers.
 */
final class BitReader {

  // Possible values: {5, 6}.  5 corresponds to 32-bit build, 6 to 64-bit. This value is used for
  // JIT conditional compilation.
  private static final int LOG_BITNESS = Utils.getLogBintness();

  // Not only Java compiler prunes "if (const false)" code, but JVM as well.
  // Code under "if (DEBUG != 0)" have zero performance impact (outside unit tests).
  private static final int DEBUG = Utils.isDebugMode();

  static final int BITNESS = 1 << LOG_BITNESS;

  private static final int BYTENESS = BITNESS / 8;
  private static final int CAPACITY = 4096;
  // After encountering the end of the input stream, this amount of zero bytes will be appended.
  private static final int SLACK = 64;
  private static final int BUFFER_SIZE = CAPACITY + SLACK;
  // Don't bother to replenish the buffer while this number of bytes is available.
  private static final int SAFEGUARD = 36;
  private static final int WATERLINE = CAPACITY - SAFEGUARD;

  // "Half" refers to "half of native integer type", i.e. on 64-bit machines it is 32-bit type,
  // on 32-bit machines it is 16-bit.
  private static final int HALF_BITNESS = BITNESS / 2;
  private static final int HALF_SIZE = BYTENESS / 2;
  private static final int HALVES_CAPACITY = CAPACITY / HALF_SIZE;
  private static final int HALF_BUFFER_SIZE = BUFFER_SIZE / HALF_SIZE;
  private static final int HALF_WATERLINE = WATERLINE / HALF_SIZE;

  private static final int LOG_HALF_SIZE = LOG_BITNESS - 4;

  /**
   * Fills up the input buffer.
   *
   * <p> No-op if there are at least 36 bytes present after current position.
   *
   * <p> After encountering the end of the input stream, 64 additional zero bytes are copied to the
   * buffer.
   */
  static void readMoreInput(State s) {
    if (s.halfOffset > HALF_WATERLINE) {
      doReadMoreInput(s);
    }
  }

  static void doReadMoreInput(State s) {
    if (s.endOfStreamReached != 0) {
      if (halfAvailable(s) >= -2) {
        return;
      }
      throw new BrotliRuntimeException("No more input");
    }
    final int readOffset = s.halfOffset << LOG_HALF_SIZE;
    int bytesInBuffer = CAPACITY - readOffset;
    // Move unused bytes to the head of the buffer.
    Utils.copyBytesWithin(s.byteBuffer, 0, readOffset, CAPACITY);
    s.halfOffset = 0;
    while (bytesInBuffer < CAPACITY) {
      final int spaceLeft = CAPACITY - bytesInBuffer;
      final int len = Utils.readInput(s, s.byteBuffer, bytesInBuffer, spaceLeft);
      // EOF is -1 in Java, but 0 in C#.
      if (len <= 0) {
        s.endOfStreamReached = 1;
        s.tailBytes = bytesInBuffer;
        bytesInBuffer += HALF_SIZE - 1;
        break;
      }
      bytesInBuffer += len;
    }
    bytesToNibbles(s, bytesInBuffer);
  }

  static void checkHealth(State s, int endOfStream) {
    if (s.endOfStreamReached == 0) {
      return;
    }
    final int byteOffset = (s.halfOffset << LOG_HALF_SIZE) + ((s.bitOffset + 7) >> 3) - BYTENESS;
    if (byteOffset > s.tailBytes) {
      throw new BrotliRuntimeException("Read after end");
    }
    if ((endOfStream != 0) && (byteOffset != s.tailBytes)) {
      throw new BrotliRuntimeException("Unused bytes after end");
    }
  }

  static void assertAccumulatorHealthy(State s) {
    if (s.bitOffset > BITNESS) {
      throw new IllegalStateException("Accumulator underloaded: " + s.bitOffset);
    }
  }

  static void fillBitWindow(State s) {
    if (DEBUG != 0) {
      assertAccumulatorHealthy(s);
    }
    if (s.bitOffset >= HALF_BITNESS) {
      // Same as doFillBitWindow. JVM fails to inline it.
      if (BITNESS == 64) {
        s.accumulator64 = ((long) s.intBuffer[s.halfOffset++] << HALF_BITNESS)
            | (s.accumulator64 >>> HALF_BITNESS);
      } else {
        s.accumulator32 = ((int) s.shortBuffer[s.halfOffset++] << HALF_BITNESS)
            | (s.accumulator32 >>> HALF_BITNESS);
      }
      s.bitOffset -= HALF_BITNESS;
    }
  }

  static void doFillBitWindow(State s) {
    if (DEBUG != 0) {
      assertAccumulatorHealthy(s);
    }
    if (BITNESS == 64) {
      s.accumulator64 = ((long) s.intBuffer[s.halfOffset++] << HALF_BITNESS)
          | (s.accumulator64 >>> HALF_BITNESS);
    } else {
      s.accumulator32 = ((int) s.shortBuffer[s.halfOffset++] << HALF_BITNESS)
          | (s.accumulator32 >>> HALF_BITNESS);
    }
    s.bitOffset -= HALF_BITNESS;
  }

  static int peekBits(State s) {
    if (BITNESS == 64) {
      return (int) (s.accumulator64 >>> s.bitOffset);
    } else {
      return s.accumulator32 >>> s.bitOffset;
    }
  }

  /**
   * Fetches bits from accumulator.
   *
   * WARNING: accumulator MUST contain at least the specified amount of bits,
   * otherwise BitReader will become broken.
   */
  static int readFewBits(State s, int n) {
    final int v = peekBits(s) & ((1 << n) - 1);
    s.bitOffset += n;
    return v;
  }

  static int readBits(State s, int n) {
    if (HALF_BITNESS >= 24) {
      return readFewBits(s, n);
    } else {
      return (n <= 16) ? readFewBits(s, n) : readManyBits(s, n);
    }
  }

  private static int readManyBits(State s, int n) {
    final int low = readFewBits(s, 16);
    doFillBitWindow(s);
    return low | (readFewBits(s, n - 16) << 16);
  }

  static void initBitReader(State s) {
    s.byteBuffer = new byte[BUFFER_SIZE];
    if (BITNESS == 64) {
      s.accumulator64 = 0;
      s.intBuffer = new int[HALF_BUFFER_SIZE];
    } else {
      s.accumulator32 = 0;
      s.shortBuffer = new short[HALF_BUFFER_SIZE];
    }
    s.bitOffset = BITNESS;
    s.halfOffset = HALVES_CAPACITY;
    s.endOfStreamReached = 0;
    prepare(s);
  }

  private static void prepare(State s) {
    readMoreInput(s);
    checkHealth(s, 0);
    doFillBitWindow(s);
    doFillBitWindow(s);
  }

  static void reload(State s) {
    if (s.bitOffset == BITNESS) {
      prepare(s);
    }
  }

  static void jumpToByteBoundary(State s) {
    final int padding = (BITNESS - s.bitOffset) & 7;
    if (padding != 0) {
      final int paddingBits = readFewBits(s, padding);
      if (paddingBits != 0) {
        throw new BrotliRuntimeException("Corrupted padding bits");
      }
    }
  }

  static int halfAvailable(State s) {
    int limit = HALVES_CAPACITY;
    if (s.endOfStreamReached != 0) {
      limit = (s.tailBytes + (HALF_SIZE - 1)) >> LOG_HALF_SIZE;
    }
    return limit - s.halfOffset;
  }

  static void copyRawBytes(State s, byte[] data, int offset, int length) {
    int pos = offset;
    int len = length;
    if ((s.bitOffset & 7) != 0) {
      throw new BrotliRuntimeException("Unaligned copyBytes");
    }

    // Drain accumulator.
    while ((s.bitOffset != BITNESS) && (len != 0)) {
      data[pos++] = (byte) peekBits(s);
      s.bitOffset += 8;
      len--;
    }
    if (len == 0) {
      return;
    }

    // Get data from shadow buffer with "sizeof(int)" granularity.
    final int copyNibbles = Math.min(halfAvailable(s), len >> LOG_HALF_SIZE);
    if (copyNibbles > 0) {
      final int readOffset = s.halfOffset << LOG_HALF_SIZE;
      final int delta = copyNibbles << LOG_HALF_SIZE;
      System.arraycopy(s.byteBuffer, readOffset, data, pos, delta);
      pos += delta;
      len -= delta;
      s.halfOffset += copyNibbles;
    }
    if (len == 0) {
      return;
    }

    // Read tail bytes.
    if (halfAvailable(s) > 0) {
      // length = 1..3
      fillBitWindow(s);
      while (len != 0) {
        data[pos++] = (byte) peekBits(s);
        s.bitOffset += 8;
        len--;
      }
      checkHealth(s, 0);
      return;
    }

    // Now it is possible to copy bytes directly.
    while (len > 0) {
      final int chunkLen = Utils.readInput(s, data, pos, len);
      if (chunkLen == -1) {
        throw new BrotliRuntimeException("Unexpected end of input");
      }
      pos += chunkLen;
      len -= chunkLen;
    }
  }

  /**
   * Translates bytes to halves (int/short).
   */
  static void bytesToNibbles(State s, int byteLen) {
    final byte[] byteBuffer = s.byteBuffer;
    final int halfLen = byteLen >> LOG_HALF_SIZE;
    if (BITNESS == 64) {
      final int[] intBuffer = s.intBuffer;
      for (int i = 0; i < halfLen; ++i) {
        intBuffer[i] = ((int) byteBuffer[i * 4] & 0xFF)
            | (((int) byteBuffer[(i * 4) + 1] & 0xFF) << 8)
            | (((int) byteBuffer[(i * 4) + 2] & 0xFF) << 16)
            | (((int) byteBuffer[(i * 4) + 3] & 0xFF) << 24);
      }
    } else {
      final short[] shortBuffer = s.shortBuffer;
      for (int i = 0; i < halfLen; ++i) {
        shortBuffer[i] = (short) (((int) byteBuffer[i * 2] & 0xFF)
            | (((int) byteBuffer[(i * 2) + 1] & 0xFF) << 8));
      }
    }
  }
}
