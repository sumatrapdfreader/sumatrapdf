/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.enc;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;

/**
 * Java prepared (raw) dictionary producer.
 */
public class PreparedDictionaryGenerator {

  private static final int MAGIC = 0xDEBCEDE0;
  private static final long HASH_MULTIPLIER = 0x1fe35a7bd3579bd3L;

  private static class PreparedDictionaryImpl implements PreparedDictionary {
    private final ByteBuffer data;

    private PreparedDictionaryImpl(ByteBuffer data) {
      this.data = data;
    }

    @Override
    public ByteBuffer getData() {
      return data;
    }
  }

  // Disallow instantiation.
  private PreparedDictionaryGenerator() { }

  public static PreparedDictionary generate(ByteBuffer src) {
    return generate(src, 17, 3, 40, 5);
  }

  public static PreparedDictionary generate(ByteBuffer src,
      int bucketBits, int slotBits, int hashBits, int blockBits) {
    ((Buffer) src).clear();  // Just in case...
    if (blockBits > 12) {
      throw new IllegalArgumentException("blockBits is too big");
    }
    if (bucketBits >= 24) {
      throw new IllegalArgumentException("bucketBits is too big");
    }
    if (bucketBits - slotBits >= 16) {
      throw new IllegalArgumentException("slotBits is too small");
    }
    int bucketLimit = 1 << blockBits;
    int numBuckets = 1 << bucketBits;
    int numSlots = 1 << slotBits;
    int slotMask = numSlots - 1;
    int hashShift = 64 - bucketBits;
    long hashMask = (~0L) >>> (64 - hashBits);
    int sourceSize = src.capacity();
    if (sourceSize < 8) {
      throw new IllegalArgumentException("src is too short");
    }

    /* Step 1: create "bloated" hasher. */
    short[] num = new short[numBuckets];
    int[] bucketHeads = new int[numBuckets];
    int[] nextBucket = new int[sourceSize];

    long accumulator = 0;
    for (int i = 0; i < 7; ++i) {
      accumulator |= (src.get(i) & 0xFFL) << (8 * i);
    }
    accumulator <<= 8;
    /* TODO(eustas): apply custom "store" order. */
    for (int i = 0; i + 7 < sourceSize; ++i) {
      accumulator = (accumulator >>> 8) | ((src.get(i + 7) & 0xFFL) << 56);
      long h = (accumulator & hashMask) * HASH_MULTIPLIER;
      int key = (int) (h >>> hashShift);
      int count = num[key];
      nextBucket[i] = (count == 0) ? -1 : bucketHeads[key];
      bucketHeads[key] = i;
      count++;
      if (count > bucketLimit) {
        count = bucketLimit;
      }
      num[key] = (short) count;
    }

    /* Step 2: find slot limits. */
    int[] slotLimit = new int[numSlots];
    int[] slotSize = new int[numSlots];
    int totalItems = 0;
    for (int i = 0; i < numSlots; ++i) {
      boolean overflow = false;
      slotLimit[i] = bucketLimit;
      while (true) {
        overflow = false;
        int limit = slotLimit[i];
        int count = 0;
        for (int j = i; j < numBuckets; j += numSlots) {
          int size = num[j];
          /* Last chain may span behind 64K limit; overflow happens only if
             we are about to use 0xFFFF+ as item offset. */
          if (count >= 0xFFFF) {
            overflow = true;
            break;
          }
          if (size > limit) {
            size = limit;
          }
          count += size;
        }
        if (!overflow) {
          slotSize[i] = count;
          totalItems += count;
          break;
        }
        slotLimit[i]--;
      }
    }

    /* Step 3: transfer data to "slim" hasher. */
    int part0 = 6 * 4;
    int part1 = numSlots * 4;
    int part2 = numBuckets * 2;
    int part3 = totalItems * 4;
    int allocSize = part0 + part1 + part2 + part3 + sourceSize;
    ByteBuffer flat = ByteBuffer.allocateDirect(allocSize);
    ByteBuffer pointer = flat.slice();
    pointer.order(ByteOrder.nativeOrder());

    IntBuffer struct = pointer.asIntBuffer();
    pointer.position(pointer.position() + part0);
    IntBuffer slotOffsets = pointer.asIntBuffer();
    pointer.position(pointer.position() + part1);
    ShortBuffer heads = pointer.asShortBuffer();
    pointer.position(pointer.position() + part2);
    IntBuffer items = pointer.asIntBuffer();
    pointer.position(pointer.position() + part3);
    ByteBuffer sourceCopy = pointer.slice();

    /* magic         */ struct.put(0, MAGIC);
    /* source_offset */ struct.put(1, totalItems);
    /* source_size   */ struct.put(2, sourceSize);
    /* hash_bits     */ struct.put(3, hashBits);
    /* bucket_bits   */ struct.put(4, bucketBits);
    /* slot_bits     */ struct.put(5, slotBits);

    totalItems = 0;
    for (int i = 0; i < numSlots; ++i) {
      slotOffsets.put(i, totalItems);
      totalItems += slotSize[i];
      slotSize[i] = 0;
    }

    for (int i = 0; i < numBuckets; ++i) {
      int slot = i & slotMask;
      int count = num[i];
      if (count > slotLimit[slot]) {
        count = slotLimit[slot];
      }
      if (count == 0) {
        heads.put(i, (short) 0xFFFF);
        continue;
      }
      int cursor = slotSize[slot];
      heads.put(i, (short) cursor);
      cursor += slotOffsets.get(slot);
      slotSize[slot] += count;
      int pos = bucketHeads[i];
      for (int j = 0; j < count; j++) {
        items.put(cursor++, pos);
        pos = nextBucket[pos];
      }
      cursor--;
      items.put(cursor, items.get(cursor) | 0x80000000);
    }

    sourceCopy.put(src);

    return new PreparedDictionaryImpl(flat);
  }
}
