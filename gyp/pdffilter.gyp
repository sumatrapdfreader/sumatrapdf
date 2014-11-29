{
	# TODO: this should link with libmupdf.dll
	'targets': [
		{
			'target_name': 'PdfFilter',
			'type': 'shared_library',
			'msvs_disabled_warnings': [4244],
			'dependencies': [
				"utils.gyp:utils",
				'mupdf.gyp:mupdf',
			],
			'include_dirs': [
				"../src",
				"../ext/synctex",
			],
			'sources': [
				"../src/previewer/PdfPreview.cpp",
				"../src/previewer/PdfPreview.h",
				"../src/previewer/PdfPreview.rc",
				"../src/previewer/PdfPreviewBase.h",
				"../src/previewer/PdfPreviewDll.cpp",
				"../src/PdfEngine.cpp",
			],
			'defines': [
			],
			'link_settings': {
				'libraries': [
					'gdiplus.lib',
					'comctl32.lib',
					'shlwapi.lib',
					'Version.lib',
					'user32.lib',
					'kernel32.lib',
					'gdi32.lib',
					'ole32.lib',
					'advapi32.lib',
					'shell32.lib',
					'oleaut32.lib',
					'winspool.lib',
					'comdlg32.lib',
					'urlmon.lib',
					'windowscodecs',
					'wininet',
					'msimg32',
				],
			},
			'msvs_settings': {
			  'VCLinkerTool': {
			  },
			},
		},
	],
}
