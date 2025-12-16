#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <windowsx.h>
#include "include/cpuinfopage.h"
#include "include/cpuinfo.h"

// Helper: compute row height for given font (DPI-aware)
static int ComputeRowHeight(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    HFONT hf = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
    HFONT oldf = NULL;
    if (hf) oldf = (HFONT)SelectObject(hdc, hf);
    TEXTMETRIC tm = {};
    GetTextMetrics(hdc, &tm);
    if (oldf) SelectObject(hdc, oldf);
    ReleaseDC(hwnd, hdc);
    return tm.tmHeight + tm.tmExternalLeading + 6;
}

static LRESULT CALLBACK CpuInfoProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static int g_scrollY = 0; // vertical scroll offset in pixels
    static bool g_dragging = false;
    static int g_dragStartY = 0;
    static int g_thumbStart = 0; // thumb top position when drag started

    switch (msg) {
    case WM_CREATE: {
        SetTimer(hWnd, 1, 1000, NULL);
        g_scrollY = 0;
        // initialize vertical scrollbar (range updated on size/paint)
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = 0; si.nPage = 0; si.nPos = 0;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hWnd, SB_VERT, FALSE); // we'll draw our own thin scrollbar
        break;
    }
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        break;
    case WM_TIMER:
        if (wParam == 1) {
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    case WM_SIZE: {
        // update scrollbar range based on content and client height
        RECT rc; GetClientRect(hWnd, &rc);
        std::vector<CpuInfoRow> rows = getCpuInfoRows();
        int rowH = ComputeRowHeight(hWnd);
        int margin = 12;
        int totalH = (int)rows.size() * rowH + margin*2 + 28;
        int clientH = rc.bottom - rc.top;
        int maxScroll = std::max(0, totalH - clientH);
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = maxScroll; si.nPage = clientH; si.nPos = g_scrollY;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hWnd, SB_VERT, FALSE); // hide system scrollbar
        if (g_scrollY > maxScroll) { g_scrollY = maxScroll; si.fMask = SIF_POS; si.nPos = g_scrollY; SetScrollInfo(hWnd, SB_VERT, &si, TRUE); }
        return 0;
    }
    case WM_VSCROLL: {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        int cur = si.nPos;
        int code = LOWORD(wParam);
        RECT rc; GetClientRect(hWnd, &rc);
        int clientH = rc.bottom - rc.top;
        int rowH = ComputeRowHeight(hWnd);
        switch (code) {
        case SB_LINEUP: cur = std::max(0, cur - rowH); break;
        case SB_LINEDOWN: cur = std::min(si.nMax, cur + rowH); break;
        case SB_PAGEUP: cur = std::max(0, cur - clientH); break;
        case SB_PAGEDOWN: cur = std::min(si.nMax, cur + clientH); break;
        case SB_THUMBTRACK: cur = si.nPos; break;
        }
        si.fMask = SIF_POS;
        si.nPos = cur;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        g_scrollY = si.nPos;
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int lines = delta / WHEEL_DELTA; // typically +/-1
        int step = lines * 3 * ComputeRowHeight(hWnd); // 3 rows per wheel
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        int cur = si.nPos - step; // invert so wheel up scrolls up
        cur = std::max(0, std::min(si.nMax, cur));
        si.fMask = SIF_POS;
        si.nPos = cur;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        g_scrollY = si.nPos;
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        RECT rc; GetClientRect(hWnd, &rc);
        int sbWidth = 8; // thinner custom scrollbar
        RECT sbArea = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
        int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
        if (mx >= sbArea.left && mx <= sbArea.right) {
            // clicked inside custom scrollbar area
            std::vector<CpuInfoRow> rows = getCpuInfoRows();
            int rowH = ComputeRowHeight(hWnd);
            int margin = 12;
            int totalH = (int)rows.size() * rowH + margin*2 + 28;
            int clientH = rc.bottom - rc.top;
            int maxScroll = std::max(0, totalH - clientH);
            if (maxScroll <= 0) break;
            float pageRatio = (float)clientH / (float)totalH;
            int thumbH = std::max(20, (int)(pageRatio * (sbArea.bottom - sbArea.top)));
            int thumbRange = (sbArea.bottom - sbArea.top) - thumbH;
            int thumbTop = sbArea.top + (int)((float)g_scrollY / maxScroll * thumbRange);
            RECT thumb = { sbArea.left + 1, thumbTop, sbArea.right - 1, thumbTop + thumbH };
            if (my >= thumb.top && my <= thumb.bottom) {
                g_dragging = true;
                g_dragStartY = my;
                g_thumbStart = thumbTop;
                SetCapture(hWnd);
            } else {
                // clicked on track -> page
                if (my < thumb.top) {
                    int newPos = std::max(0, g_scrollY - clientH);
                    g_scrollY = newPos;
                } else {
                    int newPos = std::min(maxScroll, g_scrollY + clientH);
                    g_scrollY = newPos;
                }
                SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_POS; si.nPos = g_scrollY; SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (g_dragging) {
            int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
            RECT rc; GetClientRect(hWnd, &rc);
            int sbWidth = 8; int margin = 12;
            RECT sbArea = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
            std::vector<CpuInfoRow> rows = getCpuInfoRows();
            int rowH = ComputeRowHeight(hWnd);
            int totalH = (int)rows.size() * rowH + margin*2 + 28;
            int clientH = rc.bottom - rc.top; int maxScroll = std::max(0, totalH - clientH);
            int thumbH = std::max(20, (int)(((float)clientH / totalH) * (sbArea.bottom - sbArea.top)));
            int thumbRange = (sbArea.bottom - sbArea.top) - thumbH;
            int dy = my - g_dragStartY;
            int newThumbTop = g_thumbStart + dy;
            int sbTop = sbArea.top;
            newThumbTop = std::max(sbTop, std::min(sbTop + thumbRange, newThumbTop));
            if (thumbRange > 0) {
                float t = (float)(newThumbTop - sbArea.top) / (float)thumbRange;
                g_scrollY = (int)(t * maxScroll);
                SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_POS; si.nPos = g_scrollY; SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
                InvalidateRect(hWnd, NULL, TRUE);
            }
            return 0;
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (g_dragging) {
            g_dragging = false;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        // Background
        static HBRUSH bg = CreateSolidBrush(RGB(44,62,80));
        FillRect(hdc, &rc, bg);

        // Draw header
        int margin = 12;
        int x = margin, y = margin - g_scrollY;
        int col1 = 220, col2 = rc.right - (col1 + margin*2);

        HFONT hFontHeader = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
        if (!hFontHeader) hFontHeader = CreateFont(-MulDiv(10, GetDpiForWindow(hWnd), 72), 0,0,0,FW_BOLD, FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        SelectObject(hdc, hFontHeader);
        SetTextColor(hdc, RGB(236,240,241));
        SetBkMode(hdc, TRANSPARENT);
        RECT hdr1 = { x, y, x + col1, y + ComputeRowHeight(hWnd) };
        DrawTextW(hdc, L"Property", -1, &hdr1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT hdr2 = { x + col1, y, x + col1 + col2, y + ComputeRowHeight(hWnd) };
        DrawTextW(hdc, L"Value", -1, &hdr2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        y += ComputeRowHeight(hWnd) + 6;

        // Draw rows
        std::vector<CpuInfoRow> rows = getCpuInfoRows();
        int rowH = ComputeRowHeight(hWnd);
        for (size_t i = 0; i < rows.size(); ++i) {
            // skip rows above the visible area
            if (y + rowH < 0) { y += rowH; continue; }
            if (y > rc.bottom) break; // no more visible rows
            RECT rowRc = { x, y, rc.right - margin, y + rowH };
            // alternating background
            if (i % 2 == 0) {
                static HBRUSH rb = CreateSolidBrush(RGB(38,52,68));
                FillRect(hdc, &rowRc, rb);
            }

            // Draw label using parent font (bold if possible)
            HFONT hFontLabel = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            if (!hFontLabel) hFontLabel = CreateFont(-MulDiv(9, GetDpiForWindow(hWnd), 72),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
            SelectObject(hdc, hFontLabel);
            SetTextColor(hdc, RGB(236,240,241));
            RECT rLabel = { x + 6, y, x + col1, y + rowH };
            DrawTextW(hdc, rows[i].label.c_str(), -1, &rLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Draw value (muted)
            HFONT hFontValue = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
            if (!hFontValue) hFontValue = CreateFont(-MulDiv(9, GetDpiForWindow(hWnd), 72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
            SelectObject(hdc, hFontValue);
            SetTextColor(hdc, RGB(160,170,180));
            RECT rValue = { x + col1 + 6, y, x + col1 + col2, y + rowH };
            DrawTextW(hdc, rows[i].value.c_str(), -1, &rValue, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // grid line
            HPEN p = CreatePen(PS_SOLID, 1, RGB(28,40,52));
            HPEN old = (HPEN)SelectObject(hdc, p);
            MoveToEx(hdc, x, y + rowH - 1, NULL);
            LineTo(hdc, rc.right - margin, y + rowH - 1);
            SelectObject(hdc, old);
            DeleteObject(p);

            y += rowH;
        }

        // Update scroll info (pixel-based)
        int totalH = (int)rows.size() * rowH + margin*2 + 28;
        int clientH = rc.bottom - rc.top;
        int maxScroll = std::max(0, totalH - clientH);
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = maxScroll; si.nPage = clientH; si.nPos = g_scrollY;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        ShowScrollBar(hWnd, SB_VERT, FALSE); // ensure system scrollbar hidden

        // Draw custom narrow scrollbar with background-colored edges
        {
            int sbWidth = 8;
            RECT track = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
            RECT inner = track; InflateRect(&inner, -1, -1);
            static HBRUSH hTrackBrush = CreateSolidBrush(RGB(38,52,68));
            static HBRUSH hThumbBrush = CreateSolidBrush(RGB(110,170,90));
            // draw inner track
            FillRect(hdc, &inner, hTrackBrush);
            if (maxScroll > 0) {
                int trackH = inner.bottom - inner.top;
                float pageRatio = (float)clientH / (float)totalH;
                int thumbH = std::max(20, (int)(pageRatio * trackH));
                int thumbRange = trackH - thumbH;
                int thumbTop = inner.top + (int)((float)g_scrollY / maxScroll * thumbRange);
                RECT thumb = { inner.left + 1, thumbTop, inner.right - 1, thumbTop + thumbH };
                FillRect(hdc, &thumb, hThumbBrush);
            }
        }

        EndPaint(hWnd, &ps);
        break;
    }
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

BOOL RegisterCpuInfoPageClass(HINSTANCE hInstance) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = CpuInfoProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CpuInfoPageClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    ATOM a = RegisterClassW(&wc);
    return a != 0;
}

HWND CreateCpuInfoPage(HWND hParent, HINSTANCE hInstance) {
    // Ensure class registered
    RegisterCpuInfoPageClass(hInstance);
    HWND h = CreateWindowExW(0, L"CpuInfoPageClass", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 100, 100, hParent, NULL, hInstance, NULL);
    return h;
}

void ShowCpuInfoWindow(HWND hParent) {
    RegisterCpuInfoPageClass(GetModuleHandle(NULL));
    RECT rcParent = {0,0,0,0};
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (hParent) {
        GetWindowRect(hParent, &rcParent);
        x = rcParent.left + 100;
        y = rcParent.top + 100;
    }
    HWND hWnd = CreateWindowExW(0, L"CpuInfoPageClass", L"CPU Info",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, 640, 360, hParent, NULL, GetModuleHandle(NULL), NULL);
    if (hWnd) UpdateWindow(hWnd);
}

int GetCpuInfoPagePreferredHeight(HWND hwnd) {
    // compute height based on number of rows and font metrics
    std::vector<CpuInfoRow> rows = getCpuInfoRows();
    int rowH = ComputeRowHeight(hwnd ? hwnd : GetDesktopWindow());
    int margin = 12;
    int totalH = (int)rows.size() * rowH + margin*2 + 28;
    return totalH;
}

