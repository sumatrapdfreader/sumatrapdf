/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Annotation.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Tabs.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "ChangeColorDialogCommon.h"

void HsvToRgb(float h, float s, float v, u8& r, u8& g, u8& b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf, gf, bf;
    if (h < 60) {
        rf = c;
        gf = x;
        bf = 0;
    } else if (h < 120) {
        rf = x;
        gf = c;
        bf = 0;
    } else if (h < 180) {
        rf = 0;
        gf = c;
        bf = x;
    } else if (h < 240) {
        rf = 0;
        gf = x;
        bf = c;
    } else if (h < 300) {
        rf = x;
        gf = 0;
        bf = c;
    } else {
        rf = c;
        gf = 0;
        bf = x;
    }
    r = (u8)((rf + m) * 255.0f);
    g = (u8)((gf + m) * 255.0f);
    b = (u8)((bf + m) * 255.0f);
}

Color WithAlpha(Color c, u8 a) {
    return (c & 0xffffff) | ((Color)a << 24);
}

// an alpha of 0 means "no alpha given" everywhere else, so it reads as opaque
u8 OpacityOf(Color c) {
    u8 a = GetAlpha(c);
    return a == 0 ? 0xff : a;
}

void SaveCustomColors(const Vec<Color>& colors) {
    if (!gSettings) {
        return;
    }
    str::ReplaceWithCopy(&gSettings->customColors, SerializeColorList(colors));
    ScheduleSaveSettings();
}

void ShowSetTabColorDialog(MainWindow* win, WindowTab* tab) {
    if (!IsMainWindowValidAndNotClosing(win) || !tab || !tab->ctrl) {
        return;
    }
    auto* target = new TabColorTarget();
    target->win = win;
    str::ReplaceWithCopy(&target->filePath, tab->filePath);

    auto* args = new ChangeColorsArgs();
    args->win = win;
    args->title = Tr("Change Tab Color");
    args->color = tab->tabColor;
    if (gSettings) {
        ParseColorList(gSettings->customColors, args->colors, kMaxCustomColors);
    }
    args->onClose = MkFunc1(TabColorPicked, target);
    ShowChangeColorsDialog(args);
}
