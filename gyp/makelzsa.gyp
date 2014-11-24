{
	'targets': [
		{
			'target_name': 'makelzsa',
			'type': 'executable',
			'include_dirs': [
				"../src",
			],
			'dependencies': [
				'utils',
				'zlib',
				'lzma',
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
