#! /usr/bin/env python

# this is the test script for jbig2dec

import os, re
import sys, time
import hashlib

class SelfTest:
  'generic class for self tests'
  def __init__(self):
    self.result = 'unrun'
    self.msg = ''
  def shortDescription(self):
    'returns a short name for the test'
    return "generic self test"
  def runTest(self):
    'call this to execute the test'
    pass
  def fail(self, msg=None):
    self.result = 'FAIL'
    self.msg = msg
  def failIf(self, check, msg=None):
    if check: self.fail(msg)
  def assertEqual(self, a, b, msg=None):
    if a != b: self.fail(msg)

class SelfTestSuite:
  'generic class for running a collection of SelfTest instances'
  def __init__(self, stream=sys.stderr):
    self.stream = stream
    self.tests = []
    self.fails = []
    self.xfails = []
    self.errors = []
  def addTest(self, test):
    self.tests.append(test)
  def run(self):
    starttime = time.time()
    for test in self.tests:
      self.stream.write("%s ... " % test.shortDescription())
      test.result = 'ok'
      test.runTest()
      if test.result != 'ok':
        self.fails.append(test)
      self.stream.write("%s\n" % test.result)
    stoptime = time.time()
    self.stream.write('-'*72 + '\n')
    self.stream.write('ran %d tests in %.3f seconds\n\n' %
        (len(self.tests), stoptime - starttime))
    if len(self.fails):
      self.stream.write('FAILED %d of %d tests\n' %
        (len(self.fails),len(self.tests)))
      return False
    else:
      self.stream.write('PASSED all %d tests\n' % len(self.tests))
      return True

class KnownFileHash(SelfTest):
  'self test to check for correct decode of known test files'

  # hashes of known test inputs
  known_NOTHING_DECODED = "da39a3ee5e6b4b0d3255bfef95601890afd80709"
  known_WHITE_PAGE_DECODED = "28a6bd83a8a3a36910fbc1f5ce06c962e4332911"
  known_042_DECODED = "ebfdf6e2fc5ff3ee2271c2fa19de0e52712046e8"
  known_amb_DECODED = "3d4b7992d506894662b53415bd3d0d2a2f8b7953"

  # these are known test files in the form
  # (filename, sha-1(file), sha-1(decoded document)
  known_hashes = (
                   ('tests/ubc/042_1.jb2', "673e1ee5c55ab241b171e476ba1168a42733ddaa", known_042_DECODED),
                   ('tests/ubc/042_2.jb2', "9aa2804e2d220952035c16fb3c907547884067c5", known_042_DECODED),
                   ('tests/ubc/042_3.jb2', "9663a5f35727f13e61a0a2f0a64207b1f79e7d67", known_042_DECODED),
                   ('tests/ubc/042_4.jb2', "014df658c8b99b600c2ceac3f1d53c7cc2b4917c", known_042_DECODED),
                   ('tests/ubc/042_5.jb2', "264720a6ccbbf72aa6a2cfb6343f43b8e6f2da4b", known_042_DECODED),
                   ('tests/ubc/042_6.jb2', "96f7dc9df4a1b305f9ac082dd136f85ef5b108fe", known_042_DECODED),
                   ('tests/ubc/042_7.jb2', "5526371ba9dc2b8743f20ae3e05a7e60b3dcba76", known_042_DECODED),
                   ('tests/ubc/042_8.jb2', "4bf0c87dfaf40d67c36f2a083579eeda26d54641", known_042_DECODED),
                   ('tests/ubc/042_9.jb2', "53e630e7fe2fe6e1d6164758e15fc93382e07f55", known_042_DECODED),
                   ('tests/ubc/042_10.jb2', "5ca1364367e25cb8f642e9dc677a94d5cfed0c8b", known_042_DECODED),
                   ('tests/ubc/042_11.jb2', "bc194caf022bc5345fc41259e05cea3c08245216", known_042_DECODED),
                   ('tests/ubc/042_12.jb2', "f354df8eb4849bc707f088739e322d1fe3a14ef3", known_042_DECODED),
                   ('tests/ubc/042_13.jb2', "7d428bd542f58591b254d9827f554b0552c950a7", known_WHITE_PAGE_DECODED),
                   ('tests/ubc/042_14.jb2', "c40fe3a02acb6359baf9b40fc9c49bc0800be589", known_WHITE_PAGE_DECODED),
                   ('tests/ubc/042_15.jb2', "a9e39fc1ecb178aec9f05039514d75ea3246246c", known_042_DECODED),
                   ('tests/ubc/042_16.jb2', "4008bbca43670f3c90eaee26516293ba95baaf3d", known_042_DECODED),
                   ('tests/ubc/042_17.jb2', "0ff95637b64c57d659a41c582da03e25321551fb", known_042_DECODED),
                   ('tests/ubc/042_18.jb2', "87381d044f00c4329200e44decbe91bebfa31595", known_042_DECODED),
                   ('tests/ubc/042_19.jb2', "387d95a140b456d4742622c788cf5b51cebbf438", known_042_DECODED),
                   ('tests/ubc/042_20.jb2', "85c19e9ec42b8ddd6b860a1bebea1c67610e7a59", known_042_DECODED),
                   ('tests/ubc/042_21.jb2', "ab535c7d7a61a7b9dc53d546e7419ca78ac7f447", known_042_DECODED),
                   ('tests/ubc/042_22.jb2', "a9e2b365be63716dbde74b0661c3c6efd2a6844d", known_042_DECODED),
                   ('tests/ubc/042_23.jb2', "8ffa40a05e93e10982b38a2233a8da58c1b5c343", known_042_DECODED),
                   ('tests/ubc/042_24.jb2', "2553fe65111c58f6412de51d8cdc71651e778ccf", known_042_DECODED),
                   ('tests/ubc/042_25.jb2', "52de4a3b86252d896a8d783ba71dd0699333dd69", known_042_DECODED),

                   ('tests/ubc/amb_1.jb2', "d6d6d1c981dc37a09108c1e3ed990aa5b345fa6a", known_amb_DECODED),
                   ('tests/ubc/amb_2.jb2', "9af6616a89eb03f8934de72626e301a716366c3c", known_amb_DECODED),

                   ('tests/ubc/200-10-0.jb2', "f6014b43775640ef0874497e0873f8deb291cc32", "49cddf903d3451ba23297a6b68502504093979cf"),
                   ('tests/ubc/200-10-0-stripe.jb2', "d19f58cd180afd1ae2afd11c96471e98c7c6f125", "ac89ae2046c4859348418830287982b6d60bf39b"),
                   ('tests/ubc/200-10-45.jb2', "504297b028810f812cbf075597f589a9fb82121b", "38aa99e40c6a746391c26c953223bcd4549cadd0"),
                   ('tests/ubc/200-10-45-stripe.jb2', "0d9f2a63c9fd224a6b60a9b7c0cd658f47551edd", "2921889fc5ffaafb348084761aa7c54831ec57ba"),
                   ('tests/ubc/200-20-0.jb2', "a40aaf33dd4c3225728ddfc0fad12167ceff1b17", "cc1732742d5d68c6d5c3f4eec9d5887e9ee24cd0"),
                   ('tests/ubc/200-20-0-stripe.jb2', "d499a89baf69a1b5f6fa450ec20b21136052b4cd", "743aa86e7abc9e238e23d02fbc993b048589282a"),
                   ('tests/ubc/200-20-45.jb2', "a39f1e2670f1c08dbd07d14a99965bf7253e6318", "7213fb351f65397c12accf662787aa3bc028c40f"),
                   ('tests/ubc/200-20-45-stripe.jb2', "3aa44cdef38fc8e34376480408ca99364ccbf0ee", "9021716b3eca4da549508db691655eddc4d51548"),
                   ('tests/ubc/200-2-0.jb2', "087f529ba6e3cc5fca3773c1d07e39fb642f5052", "534fceffada398444ce065088a37b6d6517a3406"),
                   ('tests/ubc/200-2-0-stripe.jb2', "dc227f7531ccecda08511bda9359864c66a8d230", "56f0e25ae5863a75d69a1825b820ba004e48d2c4"),
                   ('tests/ubc/200-3-0.jb2', "024a20b82e794eb469b4fae2b4f930c5c079fd6b", "57fe3645b028e6c7a68dcf707674f889038ee4b5"),
                   ('tests/ubc/200-3-0-stripe.jb2', "2322db7dc956863b7257d28a212431e304661998", "32ea498b28a46bf04e0b799c70114ab99ce7d15e"),
                   ('tests/ubc/200-3-45.jb2', "21ba06f8cfcc31b5bd7fa39ad98093180d3e05aa", "83e01d0a83d167fe00f7389e5fec0a660841aeef"),
                   ('tests/ubc/200-3-45-stripe.jb2', "6dfe3cbb019ef0c30ecae7d2196b1b3fd7634288", "20c2ade5766eeb3a70dca9963029c0a74171064b"),
                   ('tests/ubc/200-4-0.jb2', "c7d8d8b8a97388b0fcc6e5e3d8708fbce0881edf", "b85fe470db7542789b0632dc87dbdc721e07ddf5"),
                   ('tests/ubc/200-4-0-stripe.jb2', "840f076fd542b2ae8d0d1663ed7efd5683326bc7", "0acd5a6f24637dad4b948fa24563b1fae04996be"),
                   ('tests/ubc/200-4-45.jb2', "6ed49af06268d57137436ffeea2def6f93ea17eb", "5177abf7e9d641ca4f553bd4847134e51bb1159a"),
                   ('tests/ubc/200-4-45-stripe.jb2', "0dfc5b59a046ab05364298b1767334298fa03eeb", "944a399d8763007ae0477f69b80ec28d7fbe6edd"),
                   ('tests/ubc/200-5-0.jb2', "47770e4144b022790af00098ac830ac8665f62a0", "515eaf8e4537bbda841abf3b7ffbd1b4728c7597"),
                   ('tests/ubc/200-5-0-stripe.jb2', "23f784c297c204bc1bf7cd1559a7c38a95097266", "c69e97f9e1a7e45d6eb3975ecb8a4a7dd7f09e2e"),
                   ('tests/ubc/200-5-45.jb2', "193376e966e8bc22868e38791289e810953b5483", "77fff5286023b77316221d5c36a6d40f8b905ca9"),
                   ('tests/ubc/200-5-45-stripe.jb2', "d211863df684b5c113c2e29aec72c6a533681356", "efd7b9ae877bf3d71c0baa604a1014e1218ada90"),
                   ('tests/ubc/200-6-0.jb2', "e66a8cff6c00575018253a06f9309192cc796fb2", "7b5dae69e6f8953463dd29707f77225cd8a543ad"),
                   ('tests/ubc/200-6-0-stripe.jb2', "55ba1b94e73d96defbb7abbe35ccf13b4e1ac89f", "e8a1b55780dde4102f37ca5fafeff29bbd30e867"),
                   ('tests/ubc/200-6-45.jb2', "71d167f8af4e6c2a3202c26873aedf490e8da8f2", "9093ba8bfc65b87dddc310d437b8ca626ee2283c"),
                   ('tests/ubc/200-6-45-stripe.jb2', "abcf8f71f9ce0cb65c43942ecf0cfda7ece5d7ff", "397a48e4f3a3261928b2175699104117e36349e6"),
                   ('tests/ubc/200-8-0.jb2', "e7004846acb5529d5335c16315d11c188edea89d", "8cfa43f514911d35d9666e52ae51bbd93a9bddfc"),
                   ('tests/ubc/200-8-0-stripe.jb2', "0d96be49231e7e5a52c41bfa7303768465a9fa81", "021fbcfa12122999cded6beea3b7aa3c7018acbd"),
                   ('tests/ubc/200-8-45.jb2', "e28403c3bf1014a5b5e9c3c3e5e99cae47aa09ab", "669986963011b174d5352d38e6c77f459ff3bebd"),
                   ('tests/ubc/200-8-45-stripe.jb2', "c2e19b3e51d06c102a06643f3ea15f77d6df3788", "ceb0ef29cb68fe53d9abceb45ab182c1e6a39ff7"),
                   ('tests/ubc/200-lossless.jb2', "b9989aea1a3edd65e38e7fbeaa89a29d7a2aa342", "94d9324437bc27955e610ef4fbbd684ad3107fea"),
                   ('tests/ubc/600-10-0.jb2', "46c9af206382243d838f86ea45c63e7ca2900b68", "0ad323815315270f02f8220ed3b69133a1639f74"),
                   ('tests/ubc/600-10-45.jb2', "1f143e95bf57d8d2696525797e198efd785f7221", "16002bb4e4cefbb58da5dde531b1064b9e6ad1a7"),
                   ('tests/ubc/600-20-0.jb2', "8c874b1fb89e714ef8c64f33d292db2aea4fd05f", "a537aac28d9e0ea27d43a38024962f86aa1e403b"),
                   ('tests/ubc/600-20-45.jb2', "a9c94915dd140916bc14db7b4bc9fc5d7e73b5a9", "5af6ec6f2e8ae68cfb6df3f82bf47ee2f6c4f0b5"),
                   ('tests/ubc/600-30-0.jb2', "f0b9eea13b5c7a18742238778f1a3b7e1a4d3361", "6feaffc771381922a578bc54c4b50d18e7933ea1"),
                   ('tests/ubc/600-30-45.jb2', "65bb4202b575bba6063ef3597a5eefa356b5e660", "768788c5176d5ffb5d8d0855d8ab34312611f67d"),
                   ('tests/ubc/600-6-0.jb2', "c54abd4bdbb26b1f1209dc03ab10c05cdfd7a63a", "baba4bc5359c0fafc54efcba14da2bd5943222be"),
                   ('tests/ubc/600-6-45.jb2', "94f4f6ea60eda33e0cd8bb94a5a0f90dc05f96a7", "bc3afe7c37533ca43f3244e6877ce38b3e978e9f"),
                   ('tests/ubc/600-lossless.jb2', "60ecd5ddfb0984e3d2691bc385f425a50c753019", "f632d82b3c3d500098ad560e5ab91c69bd20827f")
                 )

  def __init__(self, file, file_hash, decode_hash):
    SelfTest.__init__(self)
    self.file = file
    self.file_hash = file_hash
    self.decode_hash = decode_hash

  def shortDescription(self):
    return "Checking '%s' for correct decoded document hash" % self.file

  def runTest(self):
    '''jbig2dec should return proper document hashes for known files'''
    # verify that the input file hash is correct
    sha1 = hashlib.sha1()
    with open(self.file, 'rb') as f:
      sha1.update(f.read())
    self.assertEqual(self.file_hash, sha1.hexdigest())

    # invoke jbig2dec on our file
    instance = os.popen('./jbig2dec -q -o /dev/null --hash ' + self.file)
    lines = instance.readlines()
    exit_code = instance.close()
    self.failIf(exit_code, 'jbig2dec should exit normally')

    # test here for correct hash
    hash_pattern = re.compile('[0-9a-f]{%d}' % len(decode_hash))
    for line in lines:
      m = hash_pattern.search(line.lower())
      if m:
        self.assertEqual(self.decode_hash, m.group(),
          'hash of known decoded document must be correct')
        return
    self.fail('document hash was not found in the output')

suite = SelfTestSuite()
for filename, file_hash, decode_hash in KnownFileHash.known_hashes:
  # only add tests for files we can find
  if not os.access(filename, os.R_OK): continue
  # todo: verify our file matches its encoded document hash
  suite.addTest(KnownFileHash(filename, file_hash, decode_hash))

# run the defined tests if we're called as a script
if __name__ == "__main__":
    result = suite.run()
    sys.exit(not result)
