/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

/* Adding themes instructions:
Add one to THEME_COUNT (Theme.h)
If THEME_COUNT > 20, you will have to update IDM_CHANGE_THEME_FIRST (resource.h)
Copy one of the theme declarations below
Rename it to whatever and change all of the properties as desired
Add a pointer to your new struct to the g_themes array below
Note: Colors are in format 0xBBGGRR
*/

// utils
#include "BaseUtil.h"
#include "WinUtil.h"
// layout controllers
#include "SettingsStructs.h"
#include "GlobalPrefs.h"

#define COL_BLACK 0x000000
#define COL_WHITE 0xFFFFFF

// Themes
Theme g_themeLight = {
    // Theme Name
    "Light",
    // Main window theme
    {
        // Main Background Color
        0xF2F2F2,
        // Main Text Color
        COL_BLACK,
        // Main Link Color
        0xA02000,
    },
    // Document style
    {
        // Canvas Color
        0x999999,
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
            0x3C68E3,
            // Text color
            COL_BLACK,
            // Default close style
            {
                // X color
                AdjustLightness2(g_themeLight.tab.current.backgroundColor, -60),
                // Circle color
                0x3535C1
            }
        },
        // Background style
        {
            // Background color
            AdjustLightness2(g_themeLight.tab.current.backgroundColor, -15),
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
            0xEBEBF9,
            // Circle color
            0x3535C1
        },
        // Clicked close style
        {
            // X color
            0xEBEBF9,
            // Circle color
            AdjustLightness2(g_themeLight.tab.current.close.circleColor, -10)
        }
    }
};
Theme g_themeDark = {
    // Theme Name
    "Dark",
    // Main window theme
    {
        // Main Background Color
        0x383226,
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        0xADCB80
    },
    // Document style
    {
        // Canvas Color
        0x2C271E,
        // Background Color
        g_themeDark.mainWindow.backgroundColor,
        // Text color
        g_themeDark.mainWindow.textColor
    },
    // Tabs
    {
        // Height
        32,
        // Current style
        {
            // Background color
            0x889600,
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                0xCFD599,
                // Circle color
                COL_BLACK
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
            0xEBEBF9,
            // Circle color
            COL_BLACK
        },
        // Clicked close style
        g_themeDark.tab.hoveredClose
    }
};
Theme g_themeDarker = {
    // Theme Name
    "Darker",
    // Main window theme
    {
        // Main Background Color
        0x302D2D,
        // Main Text Color
        COL_WHITE,
        // Main Link Color
        0xD48130
    },
    // Document style
    {
        // Canvas Color
        0x1E1E1E,
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
            0xCC7A00,
            // Text color
            COL_WHITE,
            // Default close style
            {
                // X color
                0xF5E6D0,
                // Circle color
                COL_BLACK
            }
        },
        // Background style
        {
            // Background color
            0xEAEAEA,
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
            0xEA971C,
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
    }
};

// Master themes list
Theme *g_themes[THEME_COUNT] = {
    &g_themeLight,
    &g_themeDark,
    &g_themeDarker,
};

// Current theme caching
Theme *currentTheme = NULL;
int currentThemeIndex;

Theme *GetThemeByName(char* name) {
    for (int i = 0; i < THEME_COUNT; i++) {
        if (str::Eq(g_themes[i]->name, name)) {
            return g_themes[i];
        }
    }
    return NULL;
}

Theme *GetThemeByIndex(int index) {
    return g_themes[index];
}

Theme *GetCurrentTheme() {
    if (currentTheme == NULL || !str::Eq(currentTheme->name, gGlobalPrefs->themeName)) {
        currentTheme = GetThemeByName(gGlobalPrefs->themeName);
        if (currentTheme == NULL) {
            gGlobalPrefs->themeName = g_themeLight.name;
            currentTheme = &g_themeLight;
        }
        currentThemeIndex = GetThemeIndex(currentTheme);
    }
    return currentTheme;
}

int GetThemeIndex(Theme *theme) {
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
