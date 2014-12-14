{
	'targets': [
		{
			'target_name': 'all',
			'type': 'none',
			'dependencies': [
				'sumatrapdf.gyp:*',
				'makelzsa.gyp:*',
				'zlib.gyp:*',
				'lzma.gyp:*',
				'utils.gyp:*',
				'test_util.gyp:*',
				'PdfFilter.gyp:*',
				'PdfPreview.gyp:*',
				'libmupdf.gyp:*',
			],
		},
	]
}
