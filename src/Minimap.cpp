#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "gui/PlatformFont.h"
#include "base/Pixmap.h"
#include "EngineBase.h"
#include "Settings.h"
#include "base/UITask.h"
#include "AppSettings.h"
#include "DocController.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Minimap.h"
#include "Theme.h"
#include "Translations.h"
#include "OverlayScrollbar.h"
#include "PdfDarkMode.h"
#include "RenderCache.h"
#include "base/Timer.h"
#include <math.h>
#include <mmsystem.h>

static const WCHAR kMinimapClassName[] = L"SumatraPDF_Minimap";
static const WCHAR kMinimapPreviewClassName[] = L"SumatraPDF_MinimapPreview";


static LRESULT CALLBACK MinimapPreviewWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Minimap* minimap = (Minimap*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!minimap && msg != WM_CREATE) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            minimap = (Minimap*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)minimap);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            HBRUSH hBorder = CreateSolidBrush(RGB(100, 100, 100));
            FrameRect(hdc, &rc, hBorder);
            DeleteObject(hBorder);
            
            InflateRect(&rc, -1, -1);
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
            
            if (minimap->previewBitmap && minimap->previewBitmap->GetBitmap()) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcMem, minimap->previewBitmap->GetBitmap());
                Size size = minimap->previewBitmap->GetSize();
                
                SetStretchBltMode(hdc, HALFTONE);
                StretchBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 
                           hdcMem, 0, 0, size.dx, size.dy, SRCCOPY);
                
                SelectObject(hdcMem, hbmpOld);
                DeleteDC(hdcMem);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void MinimapClearCache(Minimap* minimap) {
    if (!minimap) return;
    for (auto& c : minimap->cache) {
        delete c.bitmap;
    }
    minimap->cache.Reset();
}

Minimap::~Minimap() {
    MinimapClearCache(this);
    delete previewBitmap;
    if (hwndPreview) DestroyWindow(hwndPreview);
}

void MinimapScrollTo(Minimap* minimap, int y) {
    if (!minimap || minimap->maxScrollY <= 0) return;
    if (y < 0) y = 0;
    if (y > minimap->maxScrollY) y = minimap->maxScrollY;
    
    if (minimap->scrollY != y) {
        minimap->scrollY = y;
        
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = y;
        if (minimap->overlayScrollV) {
            OverlayScrollbarSetInfo(minimap->overlayScrollV, &si, TRUE);
        }
        
        // FALSE to avoid erasing background
        InvalidateRect(minimap->hwnd, nullptr, FALSE);
    }
}

void MinimapRecalculateScroll(Minimap* minimap) {
    if (!minimap || !minimap->hwnd) return;
    
    RECT rc;
    GetClientRect(minimap->hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    
    DisplayModel* dm = minimap->win ? minimap->win->AsFixed() : nullptr;
    if (!dm || !dm->GetEngine()) {
        minimap->maxScrollY = 0;
        minimap->scrollY = 0;
        minimap->pageHeights.Reset();
        return;
    }
    
    minimap->thumbW = (int)((float)w * 0.9f); // Use 90% of width
    if (minimap->thumbW < 10) minimap->thumbW = 10;
    
    minimap->pageHeights.Reset();
    int totalHeight = 15; // Initial top padding
    int pageCount = dm->PageCount();
    
    for (int p = 1; p <= pageCount; p++) {
        RectF prc = dm->GetEngine()->PageMediabox(p);
        int ph = 100;
        if (prc.dx > 0) {
            ph = (int)(prc.dy * (float)minimap->thumbW / prc.dx);
        }
        minimap->pageHeights.Append(ph);
        totalHeight += ph + 25 + 5; // text and padding
    }
    
    minimap->maxScrollY = totalHeight - h;
    if (minimap->maxScrollY < 0) minimap->maxScrollY = 0;
    if (minimap->scrollY > minimap->maxScrollY) minimap->scrollY = minimap->maxScrollY;
    
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;
    si.nMax = totalHeight;
    si.nPage = h;
    si.nPos = minimap->scrollY;
    
    if (minimap->overlayScrollV) {
        OverlayScrollbarSetInfo(minimap->overlayScrollV, &si, TRUE);
    }
    
    MinimapScrollTo(minimap, minimap->scrollY);
}

struct MinimapRenderTask {
    Minimap* minimap;
    DisplayModel* dm;
    int pageNo;
    float zoom;
    int rotation;
    RectF pageRect;
    Pixmap* bmp;
};

static void MinimapRenderFinished(MinimapRenderTask* task) {
    Minimap* minimap = task->minimap;
    Pixmap* bmp = task->bmp;
    if (minimap->hwnd) {
        minimap->pendingRequests.Remove(task->pageNo);
        if (bmp) {
            RenderedBitmap* rbmp = RenderedBitmapFromPixmap(bmp);
            if (rbmp && rbmp->GetBitmap()) {
                if (minimap->cache.len > 150) { // LRU eviction
                    int oldest = 0;
                    for (int j = 1; j < minimap->cache.len; j++) {
                        if (minimap->cache[j].lastUsed < minimap->cache[oldest].lastUsed) oldest = j;
                    }
                    delete minimap->cache[oldest].bitmap;
                    minimap->cache.RemoveAt(oldest);
                }
                CachedThumb ct;
                ct.pageNo = task->pageNo;
                ct.bitmap = rbmp;
                ct.lastUsed = GetTickCount();
                minimap->cache.Append(ct);
                InvalidateRect(minimap->hwnd, nullptr, FALSE);
            } else {
                delete rbmp;
            }
        }
    } else {
        if (bmp) {
            RenderedBitmap* rbmp = RenderedBitmapFromPixmap(bmp);
            delete rbmp;
        }
    }
    delete task;
}

static void MinimapRenderThread(MinimapRenderTask* task) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    EngineBase* clone = task->dm->GetEngine()->Clone();
    if (clone) {
        task->pageRect = clone->PageMediabox(task->pageNo);
        RenderPageArgs args(task->pageNo, task->zoom, task->rotation, &task->pageRect, RenderTarget::Export);
        task->bmp = clone->RenderPage(args);
        clone->Release();
    } else {
        task->bmp = nullptr;
    }
    
    if (task->bmp && task->dm->invertColors && !task->bmp->hasAlpha) {
        Color textCol = ThemeWindowTextColor();
        Color bgCol;
        ThemePageRenderColors(bgCol);
        RecolorPixmap(task->bmp, textCol, bgCol, 0, nullptr);
    }
    
    uitask::Post(MkFunc0<MinimapRenderTask>(MinimapRenderFinished, task));
}

static LRESULT CALLBACK MinimapWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Minimap* minimap = (Minimap*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!minimap && msg != WM_CREATE) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            minimap = (Minimap*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)minimap);
            minimap->overlayScrollV = OverlayScrollbarCreate(hwnd, OverlayScrollbar::Type::Vert, OverlayScrollbar::Mode::Smart);
            if (minimap->overlayScrollV) {
                minimap->overlayScrollV->topMargin = 28;
            }
            return 0;
        }
        case WM_DESTROY: {
            if (minimap->overlayScrollV) {
                OverlayScrollbarDestroy(minimap->overlayScrollV);
                minimap->overlayScrollV = nullptr;
            }
            if (minimap->hwndPreview) {
                DestroyWindow(minimap->hwndPreview);
                minimap->hwndPreview = nullptr;
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
            MinimapRecalculateScroll(minimap);
            if (minimap->overlayScrollV) {
                OverlayScrollbarUpdatePos(minimap->overlayScrollV);
            }
            return DefWindowProc(hwnd, msg, wp, lp);
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            int lines = 3;
            SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            
            double scrollAmount = (delta > 0 ? -1 : 1) * lines * 16.0;
            
            if (!minimap->scrollAnimActive) {
                minimap->scrollAnimTargetY = minimap->scrollY;
                minimap->scrollAnimCurrentY = minimap->scrollY;
                minimap->scrollAnimLastTime = TimeGet();
                minimap->scrollAnimActive = true;
                if (!minimap->scrollAnimHiResTimer) {
                    timeBeginPeriod(1);
                    minimap->scrollAnimHiResTimer = true;
                }
                SetTimer(hwnd, 2, 1, NULL); // 1ms high res timer
            }
            
            minimap->scrollAnimTargetY += scrollAmount;
            
            // Clamp target
            if (minimap->scrollAnimTargetY < 0) minimap->scrollAnimTargetY = 0;
            if (minimap->scrollAnimTargetY > minimap->maxScrollY) minimap->scrollAnimTargetY = minimap->maxScrollY;
            
            if (minimap->overlayScrollV) {
                OverlayScrollbarNotifyScroll(minimap->overlayScrollV);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!minimap->isMouseTracking) {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                minimap->isMouseTracking = true;
            }
            
            DisplayModel* dm = minimap->win ? minimap->win->AsFixed() : nullptr;
            if (!dm) break;
            
            int y = GET_Y_LPARAM(lp);
            if (y <= 28) return 0;
            int clickY = y - 28 + minimap->scrollY;
            
            int currentY = 15;
            int pageHovered = -1;
            for (int p = 1; p <= dm->PageCount() && p <= minimap->pageHeights.len; p++) {
                int itemHeight = minimap->pageHeights[p - 1] + 30;
                if (clickY >= currentY && clickY < currentY + itemHeight) {
                    pageHovered = p;
                    break;
                }
                currentY += itemHeight;
            }
            
            if (pageHovered != minimap->hoverPageNo) {
                minimap->hoverPageNo = pageHovered;
                if (minimap->hwndPreview) {
                    DestroyWindow(minimap->hwndPreview);
                    minimap->hwndPreview = nullptr;
                }
                if (pageHovered != -1) {
                    SetTimer(hwnd, 1, 1250, NULL); // Hover delay 1.25s
                } else {
                    KillTimer(hwnd, 1);
                }
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            minimap->isMouseTracking = false;
            minimap->hoverPageNo = -1;
            KillTimer(hwnd, 1);
            if (minimap->hwndPreview) {
                DestroyWindow(minimap->hwndPreview);
                minimap->hwndPreview = nullptr;
            }
            return 0;
        }
        case WM_TIMER: {
            if (wp == 1) { // Hover Timer
                KillTimer(hwnd, 1);
                if (minimap->hoverPageNo != -1) {
                    DisplayModel* dm = minimap->win ? minimap->win->AsFixed() : nullptr;
                    if (dm && dm->GetEngine()) {
                        if (minimap->previewBitmap) {
                            delete minimap->previewBitmap;
                            minimap->previewBitmap = nullptr;
                        }
                        minimap->previewPageNo = minimap->hoverPageNo;
                        
                        // Render at ~600px width
                        RectF pageRect = dm->GetEngine()->PageMediabox(minimap->previewPageNo);
                        float previewZoom = 600.0f / pageRect.dx;
                        RenderPageArgs args(minimap->previewPageNo, previewZoom, dm->GetRotation(), &pageRect, RenderTarget::View);
                        DarkModeProfile darkProfile;
                        BuildViewDarkModeProfile(dm->GetEngine(), &darkProfile);
                        if (darkProfile.mode != PageColorMode::Normal) {
                            args.darkProfile = &darkProfile;
                        }
                        Pixmap* px = dm->GetEngine()->RenderPage(args);
                        if (px) {
                            if (darkProfile.mode != PageColorMode::Normal && DarkModeProfileUsesLegacyPostProcess(&darkProfile)) {
                                RecolorPixmap(px, darkProfile.foreground, darkProfile.pageBackground, darkProfile.linkColor, nullptr);
                            }
                            minimap->previewBitmap = RenderedBitmapFromPixmap(px);
                        }
                        
                        if (minimap->previewBitmap) {
                            Size size = minimap->previewBitmap->GetSize();
                            
                            RECT rc;
                            GetWindowRect(hwnd, &rc);
                            
                            POINT pt;
                            GetCursorPos(&pt);
                            int pxPos = rc.left - size.dx - 10;
                            if (pxPos < 0) pxPos = rc.right + 10; // Fallback if no space on left
                            
                            int pyPos = pt.y - (size.dy / 2);
                            
                            // Keep vertically on screen
                            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                            MONITORINFO mi = { sizeof(mi) };
                            GetMonitorInfo(hMon, &mi);
                            if (pyPos < mi.rcWork.top) pyPos = mi.rcWork.top;
                            if (pyPos + size.dy > mi.rcWork.bottom) pyPos = mi.rcWork.bottom - size.dy - 2;
                            
                            WNDCLASSEX wcex;
                            ZeroMemory(&wcex, sizeof(wcex));
                            wcex.cbSize = sizeof(WNDCLASSEX);
                            if (!GetClassInfoEx(GetModuleHandle(nullptr), kMinimapPreviewClassName, &wcex)) {
                                wcex.style = CS_HREDRAW | CS_VREDRAW;
                                wcex.lpfnWndProc = MinimapPreviewWndProc;
                                wcex.hInstance = GetModuleHandle(nullptr);
                                wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
                                wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                                wcex.lpszClassName = kMinimapPreviewClassName;
                                RegisterClassEx(&wcex);
                            }
                            
                            minimap->hwndPreview = CreateWindowExW(
                                WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                kMinimapPreviewClassName, nullptr,
                                WS_POPUP | WS_VISIBLE | WS_BORDER,
                                pxPos, pyPos, size.dx + 2, size.dy + 2,
                                minimap->win->hwndFrame, nullptr, GetModuleHandle(nullptr), minimap);
                        }
                    }
                }
            } else if (wp == 2) { // Smooth Scroll Timer
                if (!minimap->scrollAnimActive) {
                    if (minimap->scrollAnimHiResTimer) {
                        timeEndPeriod(1);
                        minimap->scrollAnimHiResTimer = false;
                    }
                    KillTimer(hwnd, 2);
                    return 0;
                }
                
                double dtMs = TimeDiffMs(minimap->scrollAnimLastTime, TimeGet());
                minimap->scrollAnimLastTime = TimeGet();
                if (dtMs < 0.5) dtMs = 0.5;
                if (dtMs > 32.0) dtMs = 32.0;
                double dt = dtMs / 1000.0;
                
                double diff = minimap->scrollAnimTargetY - minimap->scrollAnimCurrentY;
                if (abs(diff) < 0.5) {
                    minimap->scrollAnimCurrentY = minimap->scrollAnimTargetY;
                    minimap->scrollAnimActive = false;
                    if (minimap->scrollAnimHiResTimer) {
                        timeEndPeriod(1);
                        minimap->scrollAnimHiResTimer = false;
                    }
                    KillTimer(hwnd, 2);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    double kSmoothScrollRate = 15.0;
                    double a = 1.0 - exp(-kSmoothScrollRate * dt);
                    a = std::min(a, 1.0);
                    minimap->scrollAnimCurrentY += diff * a;
                }
                
                int newY = (int)lround(minimap->scrollAnimCurrentY);
                MinimapScrollTo(minimap, newY);
            }
            return 0;
        }
        case WM_VSCROLL: {
            minimap->scrollAnimActive = false;
            KillTimer(hwnd, 2);
            
            int action = LOWORD(wp);
            int newY = minimap->scrollY;
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            if (minimap->overlayScrollV) {
                OverlayScrollbarGetInfo(minimap->overlayScrollV, &si);
            }
            
            switch (action) {
                case SB_TOP: newY = si.nMin; break;
                case SB_BOTTOM: newY = si.nMax; break;
                case SB_LINEUP: newY -= 20; break;
                case SB_LINEDOWN: newY += 20; break;
                case SB_PAGEUP: newY -= (int)si.nPage; break;
                case SB_PAGEDOWN: newY += (int)si.nPage; break;
                case SB_THUMBTRACK: 
                    newY = si.nTrackPos; 
                    minimap->isThumbTracking = true;
                    break;
                case SB_ENDSCROLL:
                    minimap->isThumbTracking = false;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    break;
            }
            MinimapScrollTo(minimap, newY);
            return 0;
        }
        case WM_PAINT: {
            DisplayModel* dm = minimap->win ? minimap->win->AsFixed() : nullptr;
            int currentEpoch = gRenderCache ? (int)gRenderCache->darkModeEpoch : 0;
            if (dm != minimap->lastDm || currentEpoch != minimap->lastDarkModeEpoch) {
                MinimapClearCache(minimap);
                minimap->lastDm = dm;
                minimap->lastDarkModeEpoch = currentEpoch;
                MinimapRecalculateScroll(minimap);
            }
            
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            
            // Double buffering to eliminate flickering completely
            HDC hdcMemDouble = CreateCompatibleDC(hdc);
            HBITMAP hbmpDouble = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP hbmpOldDouble = (HBITMAP)SelectObject(hdcMemDouble, hbmpDouble);
            
            dm = minimap->win ? minimap->win->AsFixed() : nullptr;
            Color bgCol = ThemeControlBackgroundColor();
            HBRUSH hBg = CreateSolidBrush(bgCol);
            
            FillRect(hdcMemDouble, &rc, hBg);
            DeleteObject(hBg);
            
            if (!dm || !dm->GetEngine() || minimap->pageHeights.len == 0) {
                BitBlt(hdc, 0, 0, w, h, hdcMemDouble, 0, 0, SRCCOPY);
                SelectObject(hdcMemDouble, hbmpOldDouble);
                DeleteObject(hbmpDouble);
                DeleteDC(hdcMemDouble);
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            int padX = (w - minimap->thumbW) / 2;
            int currentY = 28 + 15 - minimap->scrollY;
            
            HDC hdcMem = CreateCompatibleDC(hdcMemDouble);
            SetStretchBltMode(hdcMemDouble, HALFTONE);
            SetBkMode(hdcMemDouble, TRANSPARENT);
            
            HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                      DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hOldFont = (HFONT)SelectObject(hdcMemDouble, hFont);
            
            DWORD now = GetTickCount();
            int pageCount = dm->PageCount();
            int currentPage = dm->CurrentPageNo();
            
            // Pass 1: Find and enqueue missing pages (visible FIRST, then predictive)
            Vec<int> visibleToRender;
            Vec<int> predictiveToRender;
            int tempY = currentY;
            
            for (int p = 1; p <= minimap->pageHeights.len; p++) {
                int ph = minimap->pageHeights[p - 1];
                int itemHeight = ph + 30;
                bool isVisible = (tempY + itemHeight > rc.top && tempY < rc.bottom);
                bool isPredictive = (tempY + itemHeight > rc.top - 2000 && tempY < rc.bottom + 2000); // 2000px = approx 4-6 pages
                
                if (isPredictive) {
                    bool inCache = false;
                    for (auto& c : minimap->cache) {
                        if (c.pageNo == p) {
                            if (isVisible) c.lastUsed = now;
                            inCache = true;
                            break;
                        }
                    }
                    if (!inCache && !minimap->pendingRequests.Contains(p)) {
                        if (isVisible) visibleToRender.Append(p);
                        else predictiveToRender.Append(p);
                    }
                }
                
                tempY += itemHeight;
                if (tempY > rc.bottom + 2000) break; // Optimization: stop checking once past predictive range
            }
            
            int activeThreads = minimap->pendingRequests.len;
            int maxThreads = 2; // Limit concurrency to prevent PC freezing/crashing

            // Dispatch visible pages first
            for (int p : visibleToRender) {
                if (activeThreads >= maxThreads) break;
                minimap->pendingRequests.Append(p);
                MinimapRenderTask* task = new MinimapRenderTask{ minimap, dm, p, minimap->zoom, dm->GetRotation(), RectF(), nullptr };
                RunAsync(MkFunc0<MinimapRenderTask>(MinimapRenderThread, task), StrL("MinimapRender"));
                activeThreads++;
            }
            // Then dispatch predictive pages
            for (int p : predictiveToRender) {
                if (activeThreads >= maxThreads) break;
                minimap->pendingRequests.Append(p);
                MinimapRenderTask* task = new MinimapRenderTask{ minimap, dm, p, minimap->zoom, dm->GetRotation(), RectF(), nullptr };
                RunAsync(MkFunc0<MinimapRenderTask>(MinimapRenderThread, task), StrL("MinimapRender"));
                activeThreads++;
            }
            
            // Pass 2: Draw the UI
            for (int p = 1; p <= minimap->pageHeights.len; p++) {
                int ph = minimap->pageHeights[p - 1];
                int itemHeight = ph + 30;
                bool isVisible = (currentY + itemHeight > rc.top && currentY < rc.bottom);

                if (isVisible) {
                    // Get from cache
                    RenderedBitmap* bmp = nullptr;
                    for (auto& c : minimap->cache) {
                        if (c.pageNo == p) {
                            bmp = c.bitmap;
                            break;
                        }
                    }
                    
                    if (bmp && bmp->GetBitmap()) {
                        HBITMAP hbmpOld = (HBITMAP)SelectObject(hdcMem, bmp->GetBitmap());
                        Size size = bmp->GetSize();
                        if (size.dx > 0 && size.dy > 0) {
                            StretchBlt(hdcMemDouble, padX, currentY, minimap->thumbW, ph, hdcMem, 0, 0, size.dx, size.dy, SRCCOPY);
                        }
                        SelectObject(hdcMem, hbmpOld);
                    } else {
                        // Draw placeholder if still rendering or scrolling fast
                        RECT prc = { padX, currentY, padX + minimap->thumbW, currentY + ph };
                        Color pBg;
                        ThemePageRenderColors(pBg);
                        HBRUSH hPl = CreateSolidBrush(pBg);
                        FillRect(hdcMemDouble, &prc, hPl);
                        DeleteObject(hPl);
                    }
                    
                    // Draw active page blue box
                    if (p == currentPage) {
                        HPEN hPen = CreatePen(PS_SOLID, 3, RGB(0, 120, 215)); // Windows blue
                        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMemDouble, GetStockObject(NULL_BRUSH));
                        HPEN hOldPen = (HPEN)SelectObject(hdcMemDouble, hPen);
                        
                        Rectangle(hdcMemDouble, padX - 3, currentY - 3, padX + minimap->thumbW + 3, currentY + ph + 3);
                        
                        SelectObject(hdcMemDouble, hOldBrush);
                        SelectObject(hdcMemDouble, hOldPen);
                        DeleteObject(hPen);
                    }
                    
                    // Draw label
                    char label[32];
                    snprintf(label, sizeof(label), "%d", p);
                    RECT textRect = {0, currentY + ph + 5, w, currentY + ph + 25};
                    SetTextColor(hdcMemDouble, RGB(220, 220, 220));
                    DrawTextA(hdcMemDouble, label, -1, &textRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
                }
                
                currentY += itemHeight;
                if (currentY > rc.bottom) break;
            }
            
            // Draw Header
            RECT headerRc = { 0, 0, w, 28 };
            Color bgColHeader = ThemeControlBackgroundColor();
            HBRUSH hBgHeader = CreateSolidBrush(bgColHeader);
            FillRect(hdcMemDouble, &headerRc, hBgHeader);
            SetBkMode(hdcMemDouble, TRANSPARENT);
            SetTextColor(hdcMemDouble, (COLORREF)ThemeWindowTextColor());
            
            HFONT hFontOldHdr = (HFONT)SelectObject(hdcMemDouble, GetAppSidebarLabelFont()->GetHFont());
            
            RECT titleRc = { 10, 0, w - 28, 28 };
            DrawTextW(hdcMemDouble, L"Minimap", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            
            RECT closeRc = { w - 28, 0, w, 28 };
            DrawTextW(hdcMemDouble, L"✕", -1, &closeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdcMemDouble, hFontOldHdr);
            
            SelectObject(hdcMemDouble, hOldFont);
            DeleteObject(hFont);
            DeleteDC(hdcMem);
            
            // Draw double buffer to screen
            BitBlt(hdc, 0, 0, w, h, hdcMemDouble, 0, 0, SRCCOPY);
            
            SelectObject(hdcMemDouble, hbmpOldDouble);
            DeleteObject(hbmpDouble);
            DeleteDC(hdcMemDouble);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
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
            
            int currentY = 15;
            for (int p = 1; p <= dm->PageCount() && p <= minimap->pageHeights.len; p++) {
                int itemHeight = minimap->pageHeights[p - 1] + 30;
                if (clickY >= currentY && clickY < currentY + itemHeight) {
                    dm->GoToPage(p, 0, true);
                    break;
                }
                currentY += itemHeight;
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}


Minimap* MinimapCreate(MainWindow* win) {
    WNDCLASSEX wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEX);
    if (!GetClassInfoEx(GetModuleHandle(nullptr), kMinimapClassName, &wcex)) {
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = MinimapWndProc;
        wcex.hInstance = GetModuleHandle(nullptr);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszClassName = kMinimapClassName;
        if (!RegisterClassEx(&wcex)) return nullptr;
    }

    Minimap* minimap = new Minimap();
    minimap->win = win;

    minimap->hwnd = CreateWindowExW(
        0, kMinimapClassName, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
        0, 0, 250, 100,
        win->hwndFrame, nullptr, GetModuleHandle(nullptr), minimap);

    return minimap;
}

void MinimapDestroy(Minimap* minimap) {
    if (minimap) {
        if (minimap->hwnd) {
            DestroyWindow(minimap->hwnd);
        }
        delete minimap;
    }
}
void MinimapUpdateScrollProportional(Minimap* minimap) {
    if (!minimap || !minimap->isVisible || !minimap->hwnd) return;
    
    DisplayModel* dm = minimap->win ? minimap->win->AsFixed() : nullptr;
    if (!dm || !dm->GetEngine()) return;
    
    int currentPdfScroll = dm->yOffset();
    bool pdfScrolled = (currentPdfScroll != minimap->lastPdfScrollY);
    minimap->lastPdfScrollY = currentPdfScroll;
    
    // Update the blue box page
    int pageNo = dm->CurrentPageNo();
    if (pageNo != minimap->pageNo) {
        minimap->pageNo = pageNo;
        InvalidateRect(minimap->hwnd, nullptr, FALSE);
    }

    // If PDF actually scrolled, cancel any minimap animation so they sync
    if (pdfScrolled && minimap->scrollAnimActive) {
        minimap->scrollAnimActive = false;
        if (minimap->scrollAnimHiResTimer) {
            timeEndPeriod(1);
            minimap->scrollAnimHiResTimer = false;
        }
        KillTimer(minimap->hwnd, 2);
    }
    
    // Don't force minimap scroll if it is actively being scrolled by the user
    if (minimap->scrollAnimActive || minimap->isThumbTracking) return;
    
    Size canvas = dm->GetCanvasSize();
    Size viewPort = dm->GetViewPort().Size();
    int totalPdfScroll = canvas.dy - viewPort.dy;
    
    if (totalPdfScroll > 0) {
        float percentage = (float)currentPdfScroll / (float)totalPdfScroll;
        if (percentage < 0.0f) percentage = 0.0f;
        if (percentage > 1.0f) percentage = 1.0f;
        
        MinimapRecalculateScroll(minimap); // Make sure maxScrollY is up to date
        int targetY = (int)(percentage * minimap->maxScrollY);
        MinimapScrollTo(minimap, targetY);
    }
}

void MinimapUpdate(Minimap* minimap) {
    MinimapUpdateScrollProportional(minimap);
}

void MinimapToggleVisible(MainWindow* win) {
    if (!win || !win->minimap || !win->minimap->hwnd) return;

    Minimap* m = win->minimap;
    m->isVisible = !m->isVisible;

    ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty);

    if (!m->isVisible) {
        if (m->overlayScrollV) {
            OverlayScrollbarShow(m->overlayScrollV, false);
        }
    }
}
