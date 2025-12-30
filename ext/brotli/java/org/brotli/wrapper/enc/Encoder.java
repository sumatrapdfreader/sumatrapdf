/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.wrapper.enc;

import java.io.IOException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.channels.WritableByteChannel;
import java.util.ArrayList;
import java.util.List;
import org.brotli.enc.PreparedDictionary;

/**
 * Base class for OutputStream / Channel implementations.
 */
public class Encoder {
  private final WritableByteChannel destination;
  private final List<PreparedDictionary> dictionaries;
  private final EncoderJNI.Wrapper encoder;
  private ByteBuffer buffer;
  final ByteBuffer inputBuffer;
  boolean closed;

  /**
   * https://www.brotli.org/encode.html#aa6f
   * See encode.h, typedef enum BrotliEncoderMode
   *
   * <strong>Important</strong>: The ordinal value of the
   * modes should be the same as the constant values in encode.h
   */
  public enum Mode {
    /**
     * Default compression mode.
     * In this mode compressor does not know anything in advance about the properties of the input.
     */
    GENERIC,
    /**
     * Compression mode for UTF-8 formatted text input.
     */
    TEXT,
    /**
     * Compression mode used in WOFF 2.0.
     */
    FONT;

    // see: https://www.gamlor.info/wordpress/2017/08/javas-enum-values-hidden-allocations/
    private static final Mode[] ALL_VALUES = values();

    public static Mode of(int value) {
      return ALL_VALUES[value];
    }
  }

  /**
   * Brotli encoder settings.
   */
  public static final class Parameters {
    private int quality = -1;
    private int lgwin = -1;
    private Mode mode;

    public Parameters() { }

    private Parameters(Parameters other) {
      this.quality = other.quality;
      this.lgwin = other.lgwin;
      this.mode = other.mode;
    }

    /**
     * Setup encoder quality.
     *
     * @param quality compression quality, or -1 for default
     */
    public Parameters setQuality(int quality) {
      if (quality < -1 || quality > 11) {
        throw new IllegalArgumentException("quality should be in range [0, 11], or -1");
      }
      this.quality = quality;
      return this;
    }

    /**
     * Setup encoder window size.
     *
     * @param lgwin log2(LZ window size), or -1 for default
     */
    public Parameters setWindow(int lgwin) {
      if ((lgwin != -1) && ((lgwin < 10) || (lgwin > 24))) {
        throw new IllegalArgumentException("lgwin should be in range [10, 24], or -1");
      }
      this.lgwin = lgwin;
      return this;
    }

    /**
     * Setup encoder compression mode.
     *
     * @param mode compression mode, or {@code null} for default
     */
    public Parameters setMode(Mode mode) {
      this.mode = mode;
      return this;
    }
  }

  /**
   * Creates a Encoder wrapper.
   *
   * @param destination underlying destination
   * @param params encoding parameters
   * @param inputBufferSize read buffer size
   */
  Encoder(WritableByteChannel destination, Parameters params, int inputBufferSize)
      throws IOException {
    if (inputBufferSize <= 0) {
      throw new IllegalArgumentException("buffer size must be positive");
    }
    if (destination == null) {
      throw new NullPointerException("destination can not be null");
    }
    this.dictionaries = new ArrayList<PreparedDictionary>();
    this.destination = destination;
    this.encoder =
        new EncoderJNI.Wrapper(inputBufferSize, params.quality, params.lgwin, params.mode);
    this.inputBuffer = this.encoder.getInputBuffer();
  }

  private void fail(String message) throws IOException {
    try {
      close();
    } catch (IOException ex) {
      /* Ignore */
    }
    throw new IOException(message);
  }

  public void attachDictionary(PreparedDictionary dictionary) throws IOException {
    if (!encoder.attachDictionary(dictionary.getData())) {
      fail("failed to attach dictionary");
    }
    // Reference to native prepared dictionary wrapper should be held till the end of encoding.
    dictionaries.add(dictionary);
  }

  /**
   * @param force repeat pushing until all output is consumed
   * @return true if all encoder output is consumed
   */
  boolean pushOutput(boolean force) throws IOException {
    while (buffer != null) {
      if (buffer.hasRemaining()) {
        destination.write(buffer);
      }
      if (!buffer.hasRemaining()) {
        buffer = null;
      } else if (!force) {
        return false;
      }
    }
    return true;
  }

  /**
   * @return true if there is space in inputBuffer.
   */
  boolean encode(EncoderJNI.Operation op) throws IOException {
    boolean force = (op != EncoderJNI.Operation.PROCESS);
    if (force) {
      ((Buffer) inputBuffer).limit(inputBuffer.position());
    } else if (inputBuffer.hasRemaining()) {
      return true;
    }
    boolean hasInput = true;
    while (true) {
      if (!encoder.isSuccess()) {
        fail("encoding failed");
      } else if (!pushOutput(force)) {
        return false;
      } else if (encoder.hasMoreOutput()) {
        buffer = encoder.pull();
      } else if (encoder.hasRemainingInput()) {
        encoder.push(op, 0);
      } else if (hasInput) {
        encoder.push(op, inputBuffer.limit());
        hasInput = false;
      } else {
        ((Buffer) inputBuffer).clear();
        return true;
      }
    }
  }

  void flush() throws IOException {
    encode(EncoderJNI.Operation.FLUSH);
  }

  void close() throws IOException {
    if (closed) {
      return;
    }
    closed = true;
    try {
      encode(EncoderJNI.Operation.FINISH);
    } finally {
      encoder.destroy();
      destination.close();
    }
  }

  /** Encodes the given data buffer. */
  public static byte[] compress(byte[] data, int offset, int length, Parameters params)
      throws IOException {
    if (length == 0) {
      byte[] empty = new byte[1];
      empty[0] = 6;
      return empty;
    }
    /* data.length > 0 */
    EncoderJNI.Wrapper encoder =
        new EncoderJNI.Wrapper(length, params.quality, params.lgwin, params.mode);
    ArrayList<byte[]> output = new ArrayList<byte[]>();
    int totalOutputSize = 0;
    try {
      encoder.getInputBuffer().put(data, offset, length);
      encoder.push(EncoderJNI.Operation.FINISH, length);
      while (true) {
        if (!encoder.isSuccess()) {
          throw new IOException("encoding failed");
        } else if (encoder.hasMoreOutput()) {
          ByteBuffer buffer = encoder.pull();
          byte[] chunk = new byte[buffer.remaining()];
          buffer.get(chunk);
          output.add(chunk);
          totalOutputSize += chunk.length;
        } else if (!encoder.isFinished()) {
          encoder.push(EncoderJNI.Operation.FINISH, 0);
        } else {
          break;
        }
      }
    } finally {
      encoder.destroy();
    }
    if (output.size() == 1) {
      return output.get(0);
    }
    byte[] result = new byte[totalOutputSize];
    int resultOffset = 0;
    for (byte[] chunk : output) {
      System.arraycopy(chunk, 0, result, resultOffset, chunk.length);
      resultOffset += chunk.length;
    }
    return result;
  }

  /** Encodes the given data buffer. */
  public static byte[] compress(byte[] data, Parameters params) throws IOException {
    return compress(data, 0, data.length, params);
  }

  public static byte[] compress(byte[] data) throws IOException {
    return compress(data, new Parameters());
  }

  public static byte[] compress(byte[] data, int offset, int length) throws IOException {
    return compress(data, offset, length, new Parameters());
  }

  /**
   * Prepares raw or serialized dictionary for being used by encoder.
   *
   * @param dictionary raw / serialized dictionary data; MUST be direct
   * @param sharedDictionaryType dictionary data type
   */
  public static PreparedDictionary prepareDictionary(ByteBuffer dictionary,
      int sharedDictionaryType) {
    return EncoderJNI.prepareDictionary(dictionary, sharedDictionaryType);
  }
}
