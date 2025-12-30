package org.brotli.dec;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class Decoder {
  private static long decodeBytes(InputStream input, OutputStream output, byte[] buffer)
      throws IOException {
    long totalOut = 0;
    int readBytes;
    BrotliInputStream in = new BrotliInputStream(input);
    in.enableLargeWindow();
    try {
      while ((readBytes = in.read(buffer)) >= 0) {
        output.write(buffer, 0, readBytes);
        totalOut += readBytes;
      }
    } finally {
      in.close();
    }
    return totalOut;
  }

  private static void decompress(String fromPath, String toPath, byte[] buffer) throws IOException {
    long start;
    long bytesDecoded;
    long end;
    InputStream in = null;
    OutputStream out = null;
    try {
      in = new FileInputStream(fromPath);
      out = new FileOutputStream(toPath);
      start = System.nanoTime();
      bytesDecoded = decodeBytes(in, out, buffer);
      end = System.nanoTime();
    } finally {
      if (in != null) {
        in.close();  // Hopefully, does not throw exception.
      }
      if (out != null) {
        out.close();
      }
    }

    double timeDelta = (end - start) / 1000000000.0;
    if (timeDelta <= 0) {
      return;
    }
    double mbDecoded = bytesDecoded / (1024.0 * 1024.0);
    System.out.println(mbDecoded / timeDelta + " MiB/s");
  }

  public static void main(String... args) throws IOException {
    if (args.length != 2 && args.length != 3) {
      System.out.println("Usage: decoder <compressed_in> <decompressed_out> [repeat]");
      return;
    }

    int repeat = 1;
    if (args.length == 3) {
      repeat = Integer.parseInt(args[2]);
    }

    byte[] buffer = new byte[1024 * 1024];
    for (int i = 0; i < repeat; ++i) {
      decompress(args[0], args[1], buffer);
    }
  }
}
