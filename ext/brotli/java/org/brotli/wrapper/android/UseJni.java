package org.brotli.wrapper.android;

import org.brotli.wrapper.dec.Decoder;
import org.brotli.wrapper.enc.Encoder;
import java.io.IOException;

public final class UseJni {

  static {
    JniHelper.ensureInitialized();
  }

  public static int deepThought() {
    String theUltimateQuestion = "What do you get when you multiply six by 9";
    try {
        return Decoder.decompress(
            Encoder.compress(new byte[theUltimateQuestion.length()])).length;
    } catch (IOException ex) {
        throw new RuntimeException("Please wait 7.5 million years");
    }
  }
}
