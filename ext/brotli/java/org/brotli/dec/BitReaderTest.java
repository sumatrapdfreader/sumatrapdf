/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

import static org.junit.Assert.fail;

import java.io.ByteArrayInputStream;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Tests for {@link BitReader}.
 */
@RunWith(JUnit4.class)
public class BitReaderTest {

  @Test
  public void testReadAfterEos() {
    State reader = new State();
    reader.input = new ByteArrayInputStream(new byte[1]);
    Decode.initState(reader);
    BitReader.readBits(reader, 9);
    try {
      BitReader.checkHealth(reader, 0);
    } catch (BrotliRuntimeException ex) {
      // This exception is expected.
      return;
    }
    fail("BrotliRuntimeException should have been thrown by BitReader.checkHealth");
  }

  @Test
  public void testAccumulatorUnderflowDetected() {
    State reader = new State();
    reader.input = new ByteArrayInputStream(new byte[8]);
    Decode.initState(reader);
    // 65 bits is enough for both 32 and 64 bit systems.
    BitReader.readBits(reader, 13);
    BitReader.readBits(reader, 13);
    BitReader.readBits(reader, 13);
    BitReader.readBits(reader, 13);
    BitReader.readBits(reader, 13);
    try {
      BitReader.fillBitWindow(reader);
    } catch (IllegalStateException ex) {
      // This exception is expected.
      return;
    }
    fail("IllegalStateException should have been thrown by 'broken' BitReader");
  }
}
