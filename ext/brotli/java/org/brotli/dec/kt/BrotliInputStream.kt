/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.dec.kt

import java.io.IOException
import java.io.InputStream

/**
 * [InputStream] decorator that decompresses brotli data.
 *
 * Not thread-safe.
 */
class BrotliInputStream
@JvmOverloads
constructor(source: InputStream, byteReadBufferSize: Int = DEFAULT_INTERNAL_BUFFER_SIZE) :
  InputStream() {

  /** Internal buffer used for efficient byte-by-byte reading. */
  private val buffer: ByteArray

  /** Number of decoded but still unused bytes in internal buffer. */
  private var remainingBufferBytes: Int

  /** Next unused byte offset. */
  private var bufferOffset: Int

  /** Decoder state. */
  private val state: State = State()

  /**
   * Creates a [InputStream] wrapper that decompresses brotli data.
   *
   * For byte-by-byte reading ([.read]) internal buffer with [.DEFAULT_INTERNAL_BUFFER_SIZE] size is
   * allocated and used.
   *
   * Will block the thread until first [BitReader.CAPACITY] bytes of data of source are available.
   *
   * @param source underlying data source
   * @throws IOException in case of corrupted data or source stream problems
   */
  init {
    require(byteReadBufferSize > 0) { "Bad buffer size:$byteReadBufferSize" }
    buffer = ByteArray(byteReadBufferSize)
    remainingBufferBytes = 0
    bufferOffset = 0
    try {
      state.input = source
      initState(state)
    } catch (ex: BrotliRuntimeException) {
      throw IOException("Brotli decoder initialization failed", ex)
    }
  }

  fun attachDictionaryChunk(data: ByteArray) {
    attachDictionaryChunk(state, data)
  }

  fun enableEagerOutput() {
    enableEagerOutput(state)
  }

  fun enableLargeWindow() {
    enableLargeWindow(state)
  }

  /** {@inheritDoc} */
  @Throws(IOException::class)
  override fun close() {
    close(state)
  }

  /** {@inheritDoc} */
  @Throws(IOException::class)
  override fun read(): Int {
    if (bufferOffset >= remainingBufferBytes) {
      remainingBufferBytes = read(buffer, 0, buffer.size)
      bufferOffset = 0
      if (remainingBufferBytes == END_OF_STREAM_MARKER) {
        // Both Java and C# return the same value for EOF on single-byte read.
        return -1
      }
    }
    return buffer[bufferOffset++].toInt() and 0xFF
  }

  /** {@inheritDoc} */
  @Throws(IOException::class)
  override fun read(destBuffer: ByteArray, destOffset: Int, destLen: Int): Int {
    require(destOffset >= 0) { "Bad offset: $destOffset" }
    require(destLen >= 0) { "Bad length: $destLen" }
    require(destOffset + destLen <= destBuffer.size) {
      "Buffer overflow: " + (destOffset + destLen) + " > " + destBuffer.size
    }
    if (destLen == 0) {
      return 0
    }
    var copyLen = Math.max(remainingBufferBytes - bufferOffset, 0)
    var offset = destOffset
    var len = destLen
    if (copyLen != 0) {
      copyLen = Math.min(copyLen, len)
      System.arraycopy(buffer, bufferOffset, destBuffer, offset, copyLen)
      bufferOffset += copyLen
      offset += copyLen
      len -= copyLen
      if (len == 0) {
        return copyLen
      }
    }
    return try {
      state.output = destBuffer
      state.outputOffset = offset
      state.outputLength = len
      state.outputUsed = 0
      decompress(state)
      copyLen += state.outputUsed
      copyLen = if (copyLen > 0) copyLen else END_OF_STREAM_MARKER
      copyLen
    } catch (ex: BrotliRuntimeException) {
      throw IOException("Brotli stream decoding failed", ex)
    }
  }

  companion object {
    const val DEFAULT_INTERNAL_BUFFER_SIZE = 256

    /**
     * Value expected by InputStream contract when stream is over.
     *
     * In Java it is -1. In C# it is 0 (should be patched during transpilation).
     */
    private const val END_OF_STREAM_MARKER = -1
  }
}
