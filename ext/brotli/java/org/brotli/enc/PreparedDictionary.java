/* Copyright 2018 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

package org.brotli.enc;

import java.nio.ByteBuffer;

/**
 * Prepared dictionary data provider.
 */
public interface PreparedDictionary {
  ByteBuffer getData();
}
