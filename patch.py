import re
with open('src/Minimap.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Include AppSettings.h
content = content.replace('#include "Settings.h"\n#include "DocController.h"', '#include "Settings.h"\n#include "AppSettings.h"\n#include "DocController.h"')

# 2. WM_MOUSEMOVE
content = content.replace('''            int y = GET_Y_LPARAM(lp);
            int clickY = y + minimap->scrollY;
            
            int currentY = 15;''', '''            int y = GET_Y_LPARAM(lp);
            if (y <= 28) return 0;
            int clickY = y - 28 + minimap->scrollY;
            
            int currentY = 15;''')

# 3. WM_LBUTTONDOWN
content = content.replace('''        case WM_LBUTTONDOWN: {
            if (!minimap->win || !minimap->win->AsFixed()) break;
            DisplayModel* dm = minimap->win->AsFixed();
            
            int y = GET_Y_LPARAM(lp);
            int clickY = y + minimap->scrollY;
            
            int currentY = 15;''', '''        case WM_LBUTTONDOWN: {
            if (!minimap->win || !minimap->win->AsFixed()) break;
            DisplayModel* dm = minimap->win->AsFixed();
            
            int y = GET_Y_LPARAM(lp);
            int x = GET_X_LPARAM(lp);
            
            if (y <= 28) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (x >= rc.right - 28) {
                    MinimapToggleVisible(minimap->win);
                }
                return 0;
            }
            
            int clickY = y - 28 + minimap->scrollY;
            
            int currentY = 15;''')

# 4. WM_PAINT currentY offset
content = content.replace('''            int padX = (w - minimap->thumbW) / 2;
            int currentY = 15 - minimap->scrollY;
            
            HDC hdcMem = CreateCompatibleDC(hdcMemDouble);''', '''            int padX = (w - minimap->thumbW) / 2;
            int currentY = 28 + 15 - minimap->scrollY;
            
            HDC hdcMem = CreateCompatibleDC(hdcMemDouble);''')

# 5. WM_PAINT drawing header
content = content.replace('''                currentY += itemHeight;
                if (currentY > rc.bottom) break;
            }
            
            SelectObject(hdcMemDouble, hOldFont);
            DeleteObject(hFont);
            DeleteDC(hdcMem);
            
            // Draw double buffer to screen''', '''                currentY += itemHeight;
                if (currentY > rc.bottom) break;
            }
            
            // Draw Header
            RECT headerRc = { 0, 0, w, 28 };
            Color bgColHeader = ThemeControlBackgroundColor();
            HBRUSH hBgHeader = CreateSolidBrush(bgColHeader);
            FillRect(hdcMemDouble, &headerRc, hBgHeader);
            DeleteObject(hBgHeader);
            
            SetTextColor(hdcMemDouble, ThemeControlForegroundColor());
            SetBkMode(hdcMemDouble, TRANSPARENT);
            HFONT hFontOldHdr = (HFONT)SelectObject(hdcMemDouble, GetAppSidebarLabelFont()->GetHFont());
            
            RECT titleRc = { 10, 0, w - 28, 28 };
            DrawTextW(hdcMemDouble, L"Minimap", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            RECT closeRc = { w - 28, 0, w, 28 };
            DrawTextW(hdcMemDouble, L"\u2715", -1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdcMemDouble, hFontOldHdr);
            
            SelectObject(hdcMemDouble, hOldFont);
            DeleteObject(hFont);
            DeleteDC(hdcMem);
            
            // Draw double buffer to screen''')

with open('src/Minimap.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
