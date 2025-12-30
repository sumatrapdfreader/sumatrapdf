/* Copyright 2021 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.wrapper.dec;

import static org.junit.Assert.assertArrayEquals;

import java.io.IOException;
import org.brotli.integration.BrotliJniTestBase;
import org.brotli.wrapper.enc.Encoder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Tests for {@link org.brotli.wrapper.enc.Encoder}. */
@RunWith(JUnit4.class)
public class CornerCasesTest extends BrotliJniTestBase {

  private static byte[] makeInput(int len) {
    byte[] dst = new byte[len];
    for (int i = 0; i < len; ++i) {
      dst[i] = (byte) Integer.bitCount(i);
    }
    return dst;
  }

  private static byte[] embedChunk(byte[] src, int offset, int padding) {
    int len = src.length;
    byte[] dst = new byte[offset + len + padding];
    // TODO(eustas): fill with garbage?
    System.arraycopy(src, 0, dst, offset, len);
    return dst;
  }

  @Test
  public void testPowerOfTwoInput() throws IOException {
    // 24 == max window bits to ensure ring-buffer size is not greater than input.
    int len = 1 << 24;
    byte[] data = makeInput(len);
    byte[] encoded = Encoder.compress(data);
    byte[] decoded = Decoder.decompress(encoded);
    assertArrayEquals(data, decoded);
  }

  @Test
  public void testInputOffset() throws IOException {
    int inputLength = 19;
    int inputOffset = 4;
    int inputPadding = 7;
    byte[] data = makeInput(inputLength);
    byte[] input = embedChunk(data, inputOffset, inputPadding);
    byte[] encoded = Encoder.compress(input, inputOffset, inputLength);

    int outputLength = encoded.length;
    int outputOffset = 9;
    int outputPadding = 5;
    byte[] output = embedChunk(encoded, outputOffset, outputPadding);
    byte[] decoded = Decoder.decompress(output, outputOffset, outputLength);

    assertArrayEquals(data, decoded);
  }
}
