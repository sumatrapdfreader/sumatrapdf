/* Copyright 2017 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/
import {BrotliDecode} from "./decode.js";

/**
 * @param {!Int8Array} bytes
 * @return {string}
 */
function bytesToString(bytes) {
  return String.fromCharCode.apply(null, new Uint16Array(bytes));
}

/**
 * @param {string} str
 * @return {!Int8Array}
 */
function stringToBytes(str) {
  let out = new Int8Array(str.length);
  for (let i = 0; i < str.length; ++i) out[i] = str.charCodeAt(i);
  return out;
}

describe('DecodeTest', () => {
  it('testMetadata', () => {
    expect('').toEqual(
        bytesToString(BrotliDecode(Int8Array.from([1, 11, 0, 42, 3]))));
  });

  it('testCompoundDictionary', () => {
    const txt = 'kot lomom kolol slona\n';
    const dictionary = stringToBytes(txt);
    const compressed =
        [0xa1, 0xa8, 0x00, 0xc0, 0x2f, 0x01, 0x10, 0xc4, 0x44, 0x09, 0x00];
    expect(txt.length).toEqual(compressed.length * 2);
    const options = {'customDictionary': dictionary};
    expect(txt).toEqual(
        bytesToString(BrotliDecode(Int8Array.from(compressed), options)));
  });
});
