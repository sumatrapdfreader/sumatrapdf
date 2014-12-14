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
			],
			'sources': [
				"../src/ifilter/CPdfFilter.cpp",
				"../src/ifilter/CPdfFilter.h",
				"../src/ifilter/FilterBase.h",
				"../src/ifilter/PdfFilter.h",
				"../src/ifilter/PdfFilter.rc",
				"../src/ifilter/PdfFilterDll.cpp",
				"../src/PdfEngine.cpp",

				# if tex filter BUILD_TEX_IFILTER
				"../src/ifilter/CTeXFilter.cpp",
				"../src/ifilter/CTeXFilter.h",

				# if epub filter BUILD_EPUB_IFILTER
				"../src/ifilter/CEpubFilter.cpp",
				"../src/ifilter/CEpubFilter.h",
				"../src/EbookDoc.cpp",
				"../src/EbookDoc.h",
				"../src/MobiDoc.cpp",
				"../src/MobiDoc.h",
				"../src/utils/PalmDbReader.cpp",
				"../src/utils/PalmDbReader.h",
			],
			'defines': [
				# TODO: conditional, only in debug
				'BUILD_EPUB_IFILTER',
				'BUILD_TEX_IFILTER',
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
