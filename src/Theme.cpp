/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

/* Adding themes instructions:
Add one to kThemeCount (Theme.h)
If kThemeCount > 20, you will have to update IDM_CHANGE_THEME_LAST (resource.h)
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
        // Hovered close style
        {
            // X color
            COL_WHITEISH,
        },
        // Clicked close style
        {
            // X color
            g_themeLight.tab.hoveredClose.xColor,
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
        // Hovered close style
        {
            // X color
            COL_WHITE,
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
Theme* g_themes[kThemeCount] = {
    &g_themeLight,
    &g_themeDark,
    &g_themeDarker,
};

// Current theme caching
Theme* currentTheme = &g_themeLight;
int currentThemeIndex = 0;

int GetCurrentThemeIndex() {
    return currentThemeIndex;
}

void SetThemeByIndex(int themeIdx) {
    CrashIf((themeIdx < 0) || (themeIdx >= kThemeCount));
    currentThemeIndex = themeIdx;
    currentTheme = g_themes[currentThemeIndex];
    str::ReplaceWithCopy(&gGlobalPrefs->theme, currentTheme->name);
};

void SelectNextTheme() {
    int newIdx = (currentThemeIndex + 1) % kThemeCount;
    SetThemeByIndex(newIdx);
}

// not case sensitive
static Theme* GetThemeByName(const char* name, int& idx) {
    for (int i = 0; i < kThemeCount; i++) {
        if (str::EqI(g_themes[i]->name, name)) {
            idx = i;
            return g_themes[i];
        }
    }
    return nullptr;
}

// call after loading settings
void SetCurrentThemeFromSettings() {
    const char* name = gGlobalPrefs->theme;
    if (str::IsEmpty(name)) {
        return;
    }
    int idx = 0;
    auto theme = GetThemeByName(name, idx);
    if (!theme) {
        // invalid name, reset to light theme
        str::ReplaceWithCopy(&gGlobalPrefs->theme, g_themeLight.name);
        return;
    }
    SetThemeByIndex(idx);
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
