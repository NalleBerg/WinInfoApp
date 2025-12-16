#include "include/menupanel.h"
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <windowsx.h>

struct MenuItem { int id; std::wstring text; };

static const std::vector<MenuItem> g_items = {
    {1001, L"Summary"},
    {1002, L"CPU Info"},
    {1003, L"Exit"},
    {2001, L"About"}
};

static HWND g_panel = NULL;
static int g_scrollPos = 0;
static int g_itemHeight = 56;
static int g_hoverIndex = -1; // index of item under mouse (absolute index)

LRESULT CALLBACK MenuPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        // Setup vertical scroll
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = (int)g_items.size() - 1;
        si.nPage = 1;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        return 0;
    }
    case WM_VSCROLL: {
        int code = LOWORD(wParam);
        int pos = HIWORD(wParam);
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int cur = si.nPos;
        switch (code) {
        case SB_LINEUP: cur = std::max(0, cur - 1); break;
        case SB_LINEDOWN: cur = std::min(si.nMax, cur + 1); break;
        case SB_PAGEUP: cur = std::max(0, cur - (int)si.nPage); break;
        case SB_PAGEDOWN: cur = std::min(si.nMax, cur + (int)si.nPage); break;
        case SB_THUMBTRACK: cur = pos; break;
        }
        si.fMask = SIF_POS;
        si.nPos = cur;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        g_scrollPos = si.nPos;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int y = GET_Y_LPARAM(lParam);
        int idx = (y / g_itemHeight) + g_scrollPos;
        if (idx >= 0 && idx < (int)g_items.size()) {
            if (g_hoverIndex != idx) {
                g_hoverIndex = idx;
                InvalidateRect(hwnd, NULL, TRUE);
            }
        } else if (g_hoverIndex != -1) {
            g_hoverIndex = -1;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        // ensure mouse leave tracking
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (g_hoverIndex != -1) {
            g_hoverIndex = -1;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int lines = delta > 0 ? -1 : 1;
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int cur = si.nPos + lines;
        cur = std::max(0, std::min(si.nMax, cur));
        si.fMask = SIF_POS;
        si.nPos = cur;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        g_scrollPos = si.nPos;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int idx = (y / g_itemHeight) + g_scrollPos;
        if (idx >= 0 && idx < (int)g_items.size()) {
            // send command to parent
            HWND parent = (HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
            PostMessage(parent, WM_COMMAND, (WPARAM)g_items[idx].id, 0);
            // close panel
            DestroyWindow(hwnd);
            g_panel = NULL;
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) { DestroyWindow(hwnd); g_panel = NULL; return 0; }
        int count = (int)g_items.size();
        if (wParam == VK_DOWN) {
            int cur = g_scrollPos;
            cur = std::min(cur + 1, std::max(0, count - 1));
            g_scrollPos = cur;
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_POS; si.nPos = g_scrollPos; SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        } else if (wParam == VK_UP) {
            int cur = g_scrollPos;
            cur = std::max(0, cur - 1);
            g_scrollPos = cur;
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_POS; si.nPos = g_scrollPos; SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        } else if (wParam == VK_RETURN) {
            int idx = g_hoverIndex;
            if (idx < 0) idx = g_scrollPos;
            if (idx >= 0 && idx < count) {
                HWND parent = (HWND)GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
                PostMessage(parent, WM_COMMAND, (WPARAM)g_items[idx].id, 0);
                DestroyWindow(hwnd); g_panel = NULL;
            }
            return 0;
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        // rounded container background
        HBRUSH bg = CreateSolidBrush(RGB(34,48,60));
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(24,34,44));
        HBRUSH oldb = (HBRUSH)SelectObject(hdc, bg);
        HPEN oldp = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
        SelectObject(hdc, oldb);
        SelectObject(hdc, oldp);
        DeleteObject(bg);
        DeleteObject(pen);

        // draw items
        int y = 8;
        int visible = (rc.bottom - rc.top) / g_itemHeight + 1;
        for (int i = g_scrollPos; i < (int)g_items.size(); ++i) {
            if (y + g_itemHeight > rc.bottom) break;
            RECT itemRc = { 12, y, rc.right - 12, y + g_itemHeight - 8 };

            // pill background
            COLORREF pill = (g_hoverIndex == i) ? RGB(60,84,106) : RGB(44,62,80);
            HBRUSH br = CreateSolidBrush(pill);
            RoundRect(hdc, itemRc.left, itemRc.top, itemRc.right, itemRc.bottom, 10, 10);

            // icon circle
            int iconSize = g_itemHeight - 20;
            int iconX = itemRc.left + 10;
            int iconY = itemRc.top + 10;
            HBRUSH ibr = CreateSolidBrush((g_items[i].id==1001)? RGB(52,152,219) : (g_items[i].id==1002)? RGB(241,196,15) : (g_items[i].id==1003)? RGB(231,76,60) : RGB(149,165,166));
            Ellipse(hdc, iconX, iconY, iconX + iconSize, iconY + iconSize);
            DeleteObject(ibr);

            // text label
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(236,240,241));
            HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT oldf = (HFONT)SelectObject(hdc, hf);
            RECT txt = { iconX + iconSize + 12, itemRc.top, itemRc.right - 14, itemRc.bottom };
            DrawTextW(hdc, g_items[i].text.c_str(), -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldf);

            DeleteObject(br);
            y += g_itemHeight;
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_DESTROY:
        g_panel = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND ShowMenuPanel(HWND parent, int x, int y) {
    if (g_panel) {
        // already open
        return g_panel;
    }
    const wchar_t* cls = L"WinInfoAppMenuPanelClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = MenuPanelProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    int width = 300;
    int height = std::min((int)g_items.size() * g_itemHeight + 16, 520);
    // create popup window
    HWND h = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, cls, L"",
        WS_POPUP | WS_BORDER | WS_VSCROLL,
        x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
    if (!h) return NULL;
    g_panel = h;
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    return h;
}

void CloseMenuPanel() {
    if (g_panel) DestroyWindow(g_panel);
}
