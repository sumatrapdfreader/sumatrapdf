/* Copyright 2023 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/
import {brotliDecode} from "./decode";
import {makeTestData} from "./test_data";

const CRC_64_POLY = new Uint32Array([0xD7870F42, 0xC96C5795]);

/**
 * Calculates binary data footprint.
 */
function calculateCrc64(data: Int8Array): string {
  const crc = new Uint32Array([0xFFFFFFFF, 0xFFFFFFFF]);
  const c = new Uint32Array(2);
  for (let i = 0; i < data.length; ++i) {
    c[1] = 0;
    c[0] = (crc[0] ^ data[i]) & 0xFF;
    for (let k = 0; k < 8; ++k) {
      const isOdd = c[0] & 1;
      c[0] = (c[0] >>> 1) | ((c[1] & 1) << 31);
      c[1] = c[1] >>> 1;
      if (isOdd) {
        c[0] = c[0] ^ CRC_64_POLY[0];
        c[1] = c[1] ^ CRC_64_POLY[1];
      }
    }
    crc[0] = ((crc[0] >>> 8) | ((crc[1] & 0xFF) << 24)) ^ c[0];
    crc[1] = (crc[1] >>> 8) ^ c[1];
  }
  crc[0] = ~crc[0];
  crc[1] = ~crc[1];

  let lo = crc[0].toString(16);
  lo = "0".repeat(8 - lo.length) + lo;
  let hi = crc[1].toString(16);
  hi = "0".repeat(8 - hi.length) + hi;

  return hi + lo;
}

/**
 * Decompresses data and checks that output footprint is correct.
 */
function checkEntry(entry: string, data: Int8Array) {
  const expectedCrc = entry.substring(0, 16);
  const decompressed = brotliDecode(data);
  const crc = calculateCrc64(decompressed);
  expect(expectedCrc).toEqual(crc);
}

describe("BundleTest", () => {
  const testData = makeTestData();
  for (const entry in testData) {
    if (!testData.hasOwnProperty(entry)) {
      continue;
    }
    const name = entry.substring(17);
    const data = testData[entry];
    it('test_' + name, checkEntry.bind(null, entry, data));
  }
});
