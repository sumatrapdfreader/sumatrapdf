{
	'targets': [
		{
			'target_name': 'makelzsa',
			'type': 'executable',
			'include_dirs': [
				"../src",
			],
			'dependencies': [
				'utils.gyp:utils',
				'zlib.gyp:zlib',
				'lzma.gyp:lzma',
			],
			'msvs_disabled_warnings': [4996],
			'sources': [
				"../src/MakeLzsa.cpp",
			],
			'link_settings': {
				'libraries': [
					'shlwapi.lib',
				],
			},
			'msvs_settings': {
			  'VCLinkerTool': {
				'SubSystem': '1', # console
			  },
			},
		}
	],
}
