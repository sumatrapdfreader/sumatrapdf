package org.brotli.wrapper.android;

import android.content.Context;
import android.os.Build;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipFile;

public class JniHelper {

  // Should be set on application start.
  static Context context = null;

  private static final String LIB_NAME = "native";

  private static void tryInitialize() {
    try {
      System.loadLibrary(LIB_NAME);
    } catch (UnsatisfiedLinkError e) {
      if (context == null) {
        throw e;
      }
      int sdk = Build.VERSION.SDK_INT;
      boolean tryFallback =
          (sdk >= Build.VERSION_CODES.JELLY_BEAN) && (sdk <= Build.VERSION_CODES.KITKAT);
      if (!tryFallback) {
        throw e;
      }
      try {
        String libraryFileName = "lib" + LIB_NAME + ".so";
        String libraryFullPath = context.getFilesDir() + File.separator + libraryFileName;
        File file = new File(libraryFullPath);
        if (!file.exists()) {
          String apkPath = context.getApplicationInfo().sourceDir;
          String pathInApk = "lib/" + Build.CPU_ABI + "/lib" + LIB_NAME + ".so";
          unzip(apkPath, pathInApk, libraryFullPath);
        }
        System.load(libraryFullPath);
      } catch (UnsatisfiedLinkError unsatisfiedLinkError) {
        throw unsatisfiedLinkError;
      } catch (Throwable t) {
        UnsatisfiedLinkError unsatisfiedLinkError = new UnsatisfiedLinkError(
            "Exception while extracting native library: " + t.getMessage());
        unsatisfiedLinkError.initCause(t);
        throw unsatisfiedLinkError;
      }
    }
  }

  private static final Object mutex = new Object();
  private static volatile boolean alreadyInitialized;

  public static void ensureInitialized() {
    synchronized (mutex) {
      if (!alreadyInitialized) {
        // If failed, do not retry.
        alreadyInitialized = true;
        tryInitialize();
      }
    }
  }

  private static void unzip(String zipFileName, String entryName, String outputFileName)
      throws IOException {
    ZipFile zipFile = new ZipFile(zipFileName);
    try {
      InputStream input = zipFile.getInputStream(zipFile.getEntry(entryName));
      OutputStream output = new FileOutputStream(outputFileName);
      byte[] data = new byte[16384];
      int len;
      while ((len = input.read(data)) != -1) {
        output.write(data, 0, len);
      }
      output.close();
      input.close();
    } finally {
      zipFile.close();
    }
  }
}
