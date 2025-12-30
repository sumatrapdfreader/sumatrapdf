/* Copyright 2018 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/
package org.brotli.common;

/** POJO enum that mirrors C BrotliSharedDictionaryType. */
public class SharedDictionaryType {
  // Disallow instantiation.
  private SharedDictionaryType() {}

  public static final int RAW = 0;
  public static final int SERIALIZED = 1;
}
