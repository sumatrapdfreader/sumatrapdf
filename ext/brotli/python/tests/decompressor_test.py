# Copyright 2016 The Brotli Authors. All rights reserved.
#
# Distributed under MIT license.
# See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

import functools
import os
import unittest

from . import _test_utils
import brotli


def _get_original_name(test_data):
    return test_data.split('.compressed')[0]


class TestDecompressor(_test_utils.TestCase):

    CHUNK_SIZE = 1

    def setUp(self):
        self.decompressor = brotli.Decompressor()

    def tearDown(self):
        self.decompressor = None

    def _check_decompression(self, test_data):
        # Verify decompression matches the original.
        temp_uncompressed = _test_utils.get_temp_uncompressed_name(test_data)
        original = _get_original_name(test_data)
        self.assertFilesMatch(temp_uncompressed, original)

    def _decompress(self, test_data):
        temp_uncompressed = _test_utils.get_temp_uncompressed_name(test_data)
        with open(temp_uncompressed, 'wb') as out_file:
            with open(test_data, 'rb') as in_file:
                read_chunk = functools.partial(in_file.read, self.CHUNK_SIZE)
                for data in iter(read_chunk, b''):
                    out_file.write(self.decompressor.process(data))
        self.assertTrue(self.decompressor.is_finished())

    def _decompress_with_limit(self, test_data, max_output_length):
        temp_uncompressed = _test_utils.get_temp_uncompressed_name(test_data)
        with open(temp_uncompressed, 'wb') as out_file:
            with open(test_data, 'rb') as in_file:
                chunk_iter = iter(functools.partial(in_file.read, 10 * 1024), b'')
                while not self.decompressor.is_finished():
                    data = b''
                    if self.decompressor.can_accept_more_data():
                        data = next(chunk_iter, b'')
                    decompressed_data = self.decompressor.process(data, max_output_length=max_output_length)
                    self.assertTrue(len(decompressed_data) <= max_output_length)
                    out_file.write(decompressed_data)
                self.assertTrue(next(chunk_iter, None) == None)

    def _test_decompress(self, test_data):
        self._decompress(test_data)
        self._check_decompression(test_data)

    def _test_decompress_with_limit(self, test_data):
        self._decompress_with_limit(test_data, max_output_length=20)
        self._check_decompression(test_data)

    def test_too_much_input(self):
        with open(os.path.join(_test_utils.TESTDATA_DIR, "zerosukkanooa.compressed"), 'rb') as in_file:
            compressed = in_file.read()
            self.decompressor.process(compressed[:-1], max_output_length=1)
            # the following assertion checks whether the test setup is correct
            self.assertTrue(not self.decompressor.can_accept_more_data())
            with self.assertRaises(brotli.error):
                self.decompressor.process(compressed[-1:])

    def test_changing_limit(self):
        test_data = os.path.join(_test_utils.TESTDATA_DIR, "zerosukkanooa.compressed")
        temp_uncompressed = _test_utils.get_temp_uncompressed_name(test_data)
        with open(temp_uncompressed, 'wb') as out_file:
            with open(test_data, 'rb') as in_file:
                compressed = in_file.read()
                uncompressed = self.decompressor.process(compressed[:-1], max_output_length=1)
                self.assertTrue(len(uncompressed) <= 1)
                out_file.write(uncompressed)
                while not self.decompressor.can_accept_more_data():
                    out_file.write(self.decompressor.process(b''))
                out_file.write(self.decompressor.process(compressed[-1:]))
        self._check_decompression(test_data)

    def test_garbage_appended(self):
        with self.assertRaises(brotli.error):
            self.decompressor.process(brotli.compress(b'a') + b'a')

    def test_already_finished(self):
        self.decompressor.process(brotli.compress(b'a'))
        with self.assertRaises(brotli.error):
            self.decompressor.process(b'a')


_test_utils.generate_test_methods(TestDecompressor, for_decompression=True)

if __name__ == '__main__':
    unittest.main()
