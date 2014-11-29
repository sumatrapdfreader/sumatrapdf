{
    'targets': [
        {
            'target_name': 'mui',
            'type': 'static_library',
            'dependencies': [
                'utils.gyp:utils',
            ],
            'include_dirs': [
                "../src/mui",
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    "../src/mui",
                ],
            },
            'msvs_disabled_warnings': [4996],
            'sources': [
                "../src/mui/Mui.cpp",
                "../src/mui/Mui.h",
                "../src/mui/MuiBase.cpp",
                "../src/mui/MuiBase.h",
                "../src/mui/MuiButton.cpp",
                "../src/mui/MuiButton.h",
                "../src/mui/MuiControl.cpp",
                "../src/mui/MuiControl.h",
                "../src/mui/MuiCss.cpp",
                "../src/mui/MuiCss.h",
                "../src/mui/MuiDefs.cpp",
                "../src/mui/MuiDefs.h",
                "../src/mui/MuiEventMgr.cpp",
                "../src/mui/MuiEventMgr.h",
                "../src/mui/MuiFromText.cpp",
                "../src/mui/MuiFromText.h",
                "../src/mui/MuiGrid.cpp",
                "../src/mui/MuiGrid.h",
                "../src/mui/MuiHwndWrapper.cpp",
                "../src/mui/MuiHwndWrapper.h",
                "../src/mui/MuiLayout.cpp",
                "../src/mui/MuiLayout.h",
                "../src/mui/MuiPainter.cpp",
                "../src/mui/MuiPainter.h",
                "../src/mui/MuiScrollBar.cpp",
                "../src/mui/MuiScrollBar.h",
                "../src/mui/SvgPath.cpp",
                "../src/mui/SvgPath.h",
                "../src/mui/TextRender.cpp",
                "../src/mui/TextRender.h",
            ],
        }
    ],
}
