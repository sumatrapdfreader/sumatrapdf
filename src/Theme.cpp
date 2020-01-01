/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

Note: Colors are in format 0xBBGGRR, recommended to use rgb_to_bgr
*/

// temporarily (?) disabled
#if defined(ENABLE_THEME)

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "SettingsStructs.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"

// Color definitions
#define COL_BLACK 0x000000
#define COL_WHITE 0xFFFFFF
#define COL_WHITEISH 0xEBEBF9

// Theme definition helper functions
static COLORREF rgb_to_bgr(COLORREF rgb) {
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
        rgb_to_bgr(0xF2F2F2),
        // Main Text Color
        COL_BLACK,
        // Main Link Color
        rgb_to_bgr(0x0020A0)
    },
    // Document style
    {
        // Canvas Color
        rgb_to_bgr(0x999999),
        // Background Color
        COL_WHITE,
        // Text color
        COL_BLACK
    },
    // Tabs
    {
        // Height
        24,
        // Current style
        {
            // Background color
            rgb_to_bgr(0xFfFfFf),
            // Text color
            COL_BLACK,
            // Default close style
            {
                // X color
                AdjustLightness2(g_themeLight.tab.current.backgroundColor, -60),
                // Circle color
                rgb_to_bgr(0xC13535)
            }
        },
        // Background style
        {
            // Background color
            AdjustLightness2(g_themeLight.tab.current.backgroundColor, -25),
            // Text color
            COL_BLACK,
            // Default close style
            g_themeLight.tab.current.close
        },
        // Highlighted style
        {
            // Background color
            AdjustLightness2(g_themeLight.tab.current.backgroundColor, 15),
            // Text color
            COL_BLACK,
            // Default close style
            g_themeLight.tab.current.close
        },
        // Tab Close Circle Enabled
        true,
        // Tab Close Pen Width
        2.0f,
        // Hovered close style
        {
            // X color
            COL_WHITEISH,
            // Circle color
            g_themeLight.tab.current.close.circleColor
        },
        // Clicked close style
        {
            // X color
            g_themeLight.tab.hoveredClose.xColor,
            // Circle color
            AdjustLightness2(g_themeLight.tab.current.close.circleColor, -10)
        }
    },
    // Notifications
    {
        // Background color
        g_themeLight.mainWindow.backgroundColor,
        // Text color
        g_themeLight.mainWindow.textColor,
        // Highlight color
        rgb_to_bgr(0x3399FF),
        // Highlight text color
        COL_WHITE,
        // Progress bar color
        g_themeLight.mainWindow.linkColor
    }
};

Theme g_themeDark = {
    // Theme Name
    _TRN("Dark"),
    // Main window theme
    {
        // Main Background Color
        rgb_to_bgr(0x263238),
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        rgb_to_bgr(0x80CBAD)
    },
    // Document style
    {
        // Canvas Color
        rgb_to_bgr(0x1E272C),
        // Background Color
        g_themeDark.mainWindow.backgroundColor,
        // Text color
        g_themeDark.mainWindow.textColor
    },
    // Tabs
    {
        // Height
        24,
        // Current style
        {
            // Background color
            rgb_to_bgr(0x009688),
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                rgb_to_bgr(0x99D5CF)
            }
        },
        // Background style
        {
            // Background color
            AdjustLightness2(g_themeDark.tab.current.backgroundColor, -10),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDark.tab.current.close
        },
        // Highlighted style
        {
            // Background color
            AdjustLightness2(g_themeDark.tab.current.backgroundColor, 10),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDark.tab.current.close
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
        AdjustLightness2(rgb_to_bgr(0x33434B), 10),
        // Highlight text color
        g_themeDark.mainWindow.textColor,
        // Progress bar color
        g_themeDark.mainWindow.linkColor
    }
};

Theme g_themeDarker = {
    // Theme Name
    _TRN("Darker"),
    // Main window theme
    {
        // Main Background Color
        rgb_to_bgr(0x2D2D30),
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        rgb_to_bgr(0x3081D4)
    },
    // Document style
    {
        // Canvas Color
        rgb_to_bgr(0x1E1E1E),
        // Background Color
        g_themeDarker.mainWindow.backgroundColor,
        // Text color
        g_themeDarker.mainWindow.textColor
    },
    // Tabs
    {
        // Height
        24,
        // Current style
        {
            // Background color
            rgb_to_bgr(0x007ACC),
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                rgb_to_bgr(0xD0E6F5),
                // Circle color
                COL_BLACK
            }
        },
        // Background style
        {
            // Background color
            rgb_to_bgr(0xEAEAEA),
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
            rgb_to_bgr(0x1C97EA),
            // Text color
            COL_WHITE,
            // Default close style
            g_themeDarker.tab.current.close
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
        AdjustLightness2(rgb_to_bgr(0x3E3E42), 10),
        // Highlight text color
        g_themeDarker.mainWindow.textColor,
        // Progress bar color
        g_themeDarker.mainWindow.linkColor
    }
};
// clang-format on

// Master themes list
Theme* g_themes[THEME_COUNT] = {
    &g_themeLight,
    &g_themeDark,
    &g_themeDarker,
};

// Current theme caching
Theme* currentTheme = NULL;
int currentThemeIndex;

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

Theme* GetCurrentTheme() {
    if (currentTheme == NULL || !str::Eq(currentTheme->name, gGlobalPrefs->themeName)) {
        currentTheme = GetThemeByName(gGlobalPrefs->themeName);
        if (currentTheme == NULL) {
            str::ReplacePtr(&gGlobalPrefs->themeName, g_themeLight.name);
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
