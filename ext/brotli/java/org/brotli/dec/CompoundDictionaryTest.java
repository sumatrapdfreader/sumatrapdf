/* Copyright 2016 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Tests for {@link Dictionary}.
 */
@RunWith(JUnit4.class)
public class CompoundDictionaryTest {

  /** See {@link SynthTest} */
  private static final byte[] ONE_COPY = {
      (byte) 0xa1, (byte) 0xa8, (byte) 0x00, (byte) 0xc0, (byte) 0x2f, (byte) 0x01, (byte) 0x10,
      (byte) 0xc4, (byte) 0x44, (byte) 0x09, (byte) 0x00
    };

  private static final String TEXT = "Kot lomom kolol slona!";

  @Test
  public void testNoDictionary() throws IOException {
    BrotliInputStream decoder = new BrotliInputStream(new ByteArrayInputStream(ONE_COPY));
    byte[] buffer = new byte[32];
    int length = decoder.read(buffer, 0, buffer.length);
    assertEquals(TEXT.length(), length);
    assertEquals("alternate\" type=\"appli", new String(buffer, 0, length, "US-ASCII"));
    decoder.close();
  }

  @Test
  public void testOnePieceDictionary() throws IOException {
    BrotliInputStream decoder = new BrotliInputStream(new ByteArrayInputStream(ONE_COPY));
    decoder.attachDictionaryChunk(TEXT.getBytes("US-ASCII"));
    byte[] buffer = new byte[32];
    int length = decoder.read(buffer, 0, buffer.length);
    assertEquals(TEXT.length(), length);
    assertEquals(TEXT, new String(buffer, 0, length, "US-ASCII"));
    decoder.close();
  }

  @Test
  public void testTwoPieceDictionary() throws IOException {
    BrotliInputStream decoder = new BrotliInputStream(new ByteArrayInputStream(ONE_COPY));
    decoder.attachDictionaryChunk(TEXT.substring(0, 13).getBytes("US-ASCII"));
    decoder.attachDictionaryChunk(TEXT.substring(13).getBytes("US-ASCII"));
    byte[] buffer = new byte[32];
    int length = decoder.read(buffer, 0, buffer.length);
    assertEquals(TEXT.length(), length);
    assertEquals(TEXT, new String(buffer, 0, length, "US-ASCII"));
    decoder.close();
  }
}
