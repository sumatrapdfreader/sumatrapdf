{
	'includes': [
		#'makelzsa.gyp',
	],
	'targets': [
		{
			'target_name': 'all',
			'type': 'none',
			'dependencies': [
				#'makelzsa.gyp:*',
				'sumatra.gyp:*',
				#'zlib.gyp:zlib',
				#'lzma.gyp:*',
				#'utils.gyp:*',
				#'test_util.gyp:test_util',
			],
		},
	]
}
