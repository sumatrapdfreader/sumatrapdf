package org.brotli.wrapper.enc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.brotli.common.SharedDictionaryType;
import org.brotli.enc.PreparedDictionary;
import org.brotli.enc.PreparedDictionaryGenerator;
import org.brotli.integration.BrotliJniTestBase;
import org.brotli.integration.BundleHelper;
import org.brotli.wrapper.common.BrotliCommon;
import org.brotli.wrapper.dec.BrotliInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.List;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.junit.runner.RunWith;
import org.junit.runners.AllTests;

/** Tests for compression / decompression aided with LZ77 dictionary. */
@RunWith(AllTests.class)
public class UseCompoundDictionaryTest extends BrotliJniTestBase {

  static InputStream getBundle() throws IOException {
    return new FileInputStream(System.getProperty("TEST_BUNDLE"));
  }

  /** Creates a test suite. */
  public static TestSuite suite() throws IOException {
    TestSuite suite = new TestSuite();
    InputStream bundle = getBundle();
    try {
      List<String> entries = BundleHelper.listEntries(bundle);
      for (String entry : entries) {
        suite.addTest(new UseCompoundDictionaryTestCase(entry));
      }
    } finally {
      bundle.close();
    }
    return suite;
  }

  /** Test case with a unique name. */
  static class UseCompoundDictionaryTestCase extends TestCase {
    final String entryName;
    UseCompoundDictionaryTestCase(String entryName) {
      super("UseCompoundDictionaryTest." + entryName);
      this.entryName = entryName;
    }

    @Override
    protected void runTest() throws Throwable {
      UseCompoundDictionaryTest.run(entryName);
    }
  }

  private static PreparedDictionary prepareRawDictionary(String entryName, ByteBuffer data) {
    if (entryName.endsWith("E.coli.txt")) {
      // Default prepared dictionary parameters doesn't work well for DNA data.
      // Should work well with 8-byte hash.
      return PreparedDictionaryGenerator.generate(data, 17, 3, 64, 5);
    } else {
       return Encoder.prepareDictionary(data, SharedDictionaryType.RAW);
    }
  }

  private static void run(String entryName) throws Throwable {
    InputStream bundle = getBundle();
    byte[] original;
    try {
      original = BundleHelper.readEntry(bundle, entryName);
    } finally {
      bundle.close();
    }

    if (original == null) {
      throw new RuntimeException("Can't read bundle entry: " + entryName);
    }

    ByteBuffer dictionary = BrotliCommon.makeNative(original);
    PreparedDictionary preparedDictionary = prepareRawDictionary(entryName, dictionary);

    ByteArrayOutputStream dst = new ByteArrayOutputStream();
    BrotliOutputStream encoder =
        new BrotliOutputStream(dst, new Encoder.Parameters().setQuality(9).setWindow(23), 1 << 23);
    encoder.attachDictionary(preparedDictionary);
    try {
      encoder.write(original);
    } finally {
      encoder.close();
    }

    byte[] compressed = dst.toByteArray();

    // Just copy self from LZ77 dictionary -> ultimate compression ratio.
    assertTrue(compressed.length < 80 + original.length / 65536);

    BrotliInputStream decoder =
        new BrotliInputStream(new ByteArrayInputStream(compressed), 1 << 23);
    decoder.attachDictionary(dictionary);
    try {
      long originalCrc = BundleHelper.fingerprintStream(new ByteArrayInputStream(original));
      long crc = BundleHelper.fingerprintStream(decoder);
      assertEquals(originalCrc, crc);
    } finally {
      decoder.close();
    }
  }
}
