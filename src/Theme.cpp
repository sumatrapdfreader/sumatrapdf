/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

/* Adding themes instructions:
Add one to THEME_COUNT (Theme.h)
If THEME_COUNT > 20, you will have to update IDM_CHANGE_THEME_LAST (resource.h)
Copy one of the theme declarations below
Rename it to whatever and change all of the properties as desired
Add a pointer to your new struct to the g_themes array below

Try not to enter a color code twice. If you use it more than once in a theme,
reference it through the theme struct the second time. See g_themeDark.document for example.
You can also use methods like AdjustLightness2 to modify existing colors. If you use a
color in multiple themes, you may want to define it in the color definitions section.This
makes themes easier to modify and update.

Note: Colors are in format 0xBBGGRR, recommended to use RgbToCOLORREF
*/

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"

// Color definitions
#define COL_BLACK 0x000000
#define COL_WHITE 0xFFFFFF
#define COL_WHITEISH 0xEBEBF9
#define COL_DARK_GRAY 0x424242

// Theme definition helper functions
static COLORREF RgbToCOLORREF(COLORREF rgb) {
    return ((rgb & 0x0000FF) << 16) | (rgb & 0x00FF00) | ((rgb & 0xFF0000) >> 16);
}

// clang-format off
// Themes
Theme g_themeLight = {
    // Theme Name
    _TRN("Light"),
    // Main window theme
    {
        // Main Background Color
        // Background color comparison:
        // Adobe Reader X   0x565656 without any frame border
        // Foxit Reader 5   0x9C9C9C with a pronounced frame shadow
        // PDF-XChange      0xACA899 with a 1px frame and a gradient shadow
        // Google Chrome    0xCCCCCC with a symmetric gradient shadow
        // Evince           0xD7D1CB with a pronounced frame shadow
        // SumatraPDF (old) 0xCCCCCC with a pronounced frame shadow

        // it's very light gray but not white so that there's contrast between
        // background and thumbnail, which often have white background because
        // most PDFs have white background.
        RgbToCOLORREF(0xF2F2F2),
        // Control background Color
        COL_WHITE,
        // Main Text Color
        COL_BLACK,
        // Main Link Color
        RgbToCOLORREF(0x0020A0)
    },
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x999999),
        // Background Color
        COL_WHITE,
        // Text color
        COL_BLACK
    },
    // Tabs
    {
        // Height
        24,
        // Selected style
        {
            // Background color
            COL_WHITE,
            // Text color
            COL_DARK_GRAY,
            // Default close style
            {
                // X color
                AdjustLightness2(g_themeLight.tab.selected.backgroundColor, -60),
                // Circle color
                RgbToCOLORREF(0xC13535)
            }
        },
        // Background style
        {
            // Background color
            //AdjustLightness2(g_themeLight.tab.selected.backgroundColor, -25),
            RgbToCOLORREF(0xCECECE),
            // Text color
            COL_DARK_GRAY,
            // Default close style
            g_themeLight.tab.selected.close
        },
        // Highlighted style
        {
            // Background color
            // AdjustLightness2(g_themeLight.tab.selected.backgroundColor, 15),
            RgbToCOLORREF(0xBBBBBB),
            // Text color
            COL_BLACK,
            // Default close style
            g_themeLight.tab.selected.close
        },
        // Tab Close Circle Enabled
        true,
        // Tab Close Pen Width
        1.0f,
        // Hovered close style
        {
            // X color
            COL_WHITEISH,
            // Circle color
            g_themeLight.tab.selected.close.circleColor
        },
        // Clicked close style
        {
            // X color
            g_themeLight.tab.hoveredClose.xColor,
            // Circle color
            AdjustLightness2(g_themeLight.tab.selected.close.circleColor, -10)
        }
    },
    // Notifications
    {
        // Background color
        COL_WHITE,
        // Text color
        g_themeLight.mainWindow.textColor,
        // Highlight color
        RgbToCOLORREF(0xFFEE70),
        // Highlight text color
        RgbToCOLORREF(0x8d0801),
        // Progress bar color
        g_themeLight.mainWindow.linkColor
    },
    // Colorize standard controls
    false
};

Theme g_themeDark = {
    // Theme Name
    _TRN("Dark"),
    // Main window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x263238),
         // Control background Color
        RgbToCOLORREF(0x263238),
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        RgbToCOLORREF(0x80CBAD)
       
    },
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x1E272C),
        // Background Color
        g_themeDark.mainWindow.backgroundColor,
        // Text color
        g_themeDark.mainWindow.textColor
    },
    // Tabs
    {
        // Height
        24,
        // Selected style
        {
            // Background color
            RgbToCOLORREF(0x009688),
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                RgbToCOLORREF(0x99D5CF)
            }
        },
        // Background style
        {
            // Background color
            AdjustLightness2(g_themeDark.tab.selected.backgroundColor, -10),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDark.tab.selected.close
        },
        // Highlighted style
        {
            // Background color
            AdjustLightness2(g_themeDark.tab.selected.backgroundColor, 10),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDark.tab.selected.close
        },
        // Tab Close Circle Enabled
        false,
        // Tab Close Pen Width
        1.0f,
        // Hovered close style
        {
            // X color
            COL_WHITEISH
        },
        // Clicked close style
        g_themeDark.tab.hoveredClose
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(g_themeDark.mainWindow.backgroundColor, 10),
        // Text color
        g_themeDark.mainWindow.textColor,
        // Highlight color
        AdjustLightness2(RgbToCOLORREF(0x33434B), 10),
        // Highlight text color
        g_themeDark.mainWindow.textColor,
        // Progress bar color
        g_themeDark.mainWindow.linkColor
    },
    // Colorize standard controls
    true
};

Theme g_themeDarker = {
    // Theme Name
    _TRN("Darker"),
    // Main window theme
    {
        // Main Background Color
        RgbToCOLORREF(0x2D2D30),
         // Control background Color
        RgbToCOLORREF(0x2D2D30),
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        RgbToCOLORREF(0x3081D4)
    },
    // Document style
    {
        // Canvas Color
        RgbToCOLORREF(0x1E1E1E),
        // Background Color
        g_themeDarker.mainWindow.backgroundColor,
        // Text color
        g_themeDarker.mainWindow.textColor
    },
    // Tabs
    {
        // Height
        24,
        // Selected style
        {
            // Background color
            RgbToCOLORREF(0x007ACC),
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                RgbToCOLORREF(0xD0E6F5),
                // Circle color
                COL_BLACK
            }
        },
        // Background style
        {
            // Background color
            RgbToCOLORREF(0xEAEAEA),
            // Text color
            COL_BLACK,
            // Default close style
            {
                // X color
                COL_BLACK,
                // Circle color
                COL_BLACK
            }
        },
        // Highlighted style
        {
            // Background color
            RgbToCOLORREF(0x1C97EA),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDarker.tab.selected.close
        },
        // Tab Close Circle Enabled
        false,
        // Tab Close Pen Width
        2.0f,
        // Hovered close style
        {
            // X color
            COL_WHITE,
            // Circle color
            COL_BLACK
        },
        // Clicked close style
        g_themeDarker.tab.hoveredClose
    },
    // Notifications
    {
        // Background color
        AdjustLightness2(g_themeDarker.mainWindow.backgroundColor, 10),
        // Text color
        g_themeDarker.mainWindow.textColor,
        // Highlight color
        AdjustLightness2(RgbToCOLORREF(0x3E3E42), 10),
        // Highlight text color
        g_themeDarker.mainWindow.textColor,
        // Progress bar color
        g_themeDarker.mainWindow.linkColor
    },
    // Colorize standard controls
    true
};
// clang-format on

// Master themes list
Theme* g_themes[THEME_COUNT] = {
    &g_themeLight,
    &g_themeDark,
    &g_themeDarker,
};

// Current theme caching
Theme* currentTheme = &g_themeLight;
int currentThemeIndex = 0;

void SwitchTheme(int index) {
    CrashIf(index < 0 || index >= THEME_COUNT);
    currentThemeIndex = index;
    currentTheme = g_themes[index];
}

void CycleNextTheme() {
    ++currentThemeIndex;
    if (currentThemeIndex >= THEME_COUNT) {
        currentThemeIndex = 0;
    }
    SwitchTheme(currentThemeIndex);
}

Theme* GetThemeByName(char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (str::Eq(g_themes[i]->name, name)) {
            return g_themes[i];
        }
    }
    return NULL;
}

Theme* GetThemeByIndex(int index) {
    CrashIf(index < 0 || index >= THEME_COUNT);
    return g_themes[index];
}

void GetDocumentColors(COLORREF& text, COLORREF& bg) {
    // Special behavior for light theme.
    // TODO: migrate from prefs to theme.
    if (currentThemeIndex == 0) {
        ParsedColor* parsedCol;
        if (gGlobalPrefs->fixedPageUI.invertColors) {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
        } else {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
        }
        bg = parsedCol->col;
        if (gGlobalPrefs->fixedPageUI.invertColors) {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
        } else {
            parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
        }
        text = parsedCol->col;
    } else {
        bg = currentTheme->document.backgroundColor;
        text = currentTheme->document.textColor;
    }
}

COLORREF GetMainWindowBackgroundColor() {
    COLORREF bgColor = currentTheme->mainWindow.backgroundColor;
    // Special behavior for light theme.
    // TODO: migrate from prefs to theme.
    if (currentThemeIndex == 0) {
// for backward compatibility use a value that older versions will render as yellow
#define MAIN_WINDOW_BG_COLOR_DEFAULT (RGB(0xff, 0xf2, 0) - 0x80000000)

        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (MAIN_WINDOW_BG_COLOR_DEFAULT != bgParsed->col) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}

#if 0

Theme* GetCurrentTheme() {
    if (currentTheme == NULL || !str::Eq(currentTheme->name, gGlobalPrefs->themeName)) {
        currentTheme = GetThemeByName(gGlobalPrefs->themeName);
        if (currentTheme == NULL) {
            str::ReplaceWithCopy(&gGlobalPrefs->themeName, g_themeLight.name);
            currentTheme = &g_themeLight;
        }
        currentThemeIndex = GetThemeIndex(currentTheme);
    }
    return currentTheme;
}

int GetThemeIndex(Theme* theme) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (g_themes[i] == theme) {
            return i;
        }
    }
    return -1;
}

int GetCurrentThemeIndex() {
    if (currentTheme == NULL || !str::Eq(currentTheme->name, gGlobalPrefs->themeName)) {
        GetCurrentTheme();
    }
    return currentThemeIndex;
}

#endif
