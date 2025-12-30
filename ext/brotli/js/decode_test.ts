/* Copyright 2023 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

import 'jasmine';

import {brotliDecode} from './decode';

function bytesToString(bytes: Int8Array): string {
  const chars: number[] = new Uint16Array(bytes) as unknown as number[];
  return String.fromCharCode.apply(null, chars);
}

function stringToBytes(str: string): Int8Array {
  const out = new Int8Array(str.length);
  for (let i = 0; i < str.length; ++i) out[i] = str.charCodeAt(i);
  return out;
}

describe('DecodeTest', () => {
  it('testMetadata', () => {
    expect('').toEqual(
        bytesToString(brotliDecode(Int8Array.from([1, 11, 0, 42, 3]))));
  });

  it('testCompoundDictionary', () => {
    const txt = 'kot lomom kolol slona\n';
    const dictionary = stringToBytes(txt);
    const compressed =
        [0xa1, 0xa8, 0x00, 0xc0, 0x2f, 0x01, 0x10, 0xc4, 0x44, 0x09, 0x00];
    expect(txt.length).toEqual(compressed.length * 2);
    const options = {'customDictionary': dictionary};
    expect(txt).toEqual(
        bytesToString(brotliDecode(Int8Array.from(compressed), options)));
  });
});
