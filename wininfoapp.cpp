// --- WinInfoApp Main Window, all menus and separator are #FFFFFF, About page table layout ---

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <windows.h>

#include "include/cpuinfo.h"
#include "include/resource.h"

using namespace Gdiplus;


// Language IDs
#define ID_LANG_EN 4001
#define ID_LANG_NO 4002

// Current language (0=en,1=no)
static int g_currentLanguage = 0;

// --- Utility: INI path & persistence ---
static std::wstring GetIniPath() {
    wchar_t buf[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", buf, MAX_PATH) > 0) {
        std::wstring p(buf);
        if (!p.empty() && p.back() != L'\\' && p.back() != L'/') p += L"\\";
        p += L"WinInfoApp.ini";
        return p;
    }
    wchar_t cwd[MAX_PATH] = {0};
    if (GetCurrentDirectoryW(MAX_PATH, cwd) > 0) {
        std::wstring p(cwd);
        if (!p.empty() && p.back() != L'\\' && p.back() != L'/') p += L"\\";
        p += L"WinInfoApp.ini";
        return p;
    }
    return std::wstring(L"WinInfoApp.ini");
}

static int ReadLanguageSetting() {
    std::wstring ini = GetIniPath();
    wchar_t buf[64] = {};
    GetPrivateProfileStringW(L"Settings", L"Language", L"en-GB", buf, (DWORD)std::size(buf), ini.c_str());
    if (wcscmp(buf, L"nb-NO") == 0 || wcscmp(buf, L"nb") == 0) return 1;
    return 0;
}

static void WriteLanguageSetting(int lang) {
    std::wstring ini = GetIniPath();
    const wchar_t* val = (lang == 1) ? L"nb-NO" : L"en-GB";
    WritePrivateProfileStringW(L"Settings", L"Language", val, ini.c_str());
}

// --- Global variables ---
static HFONT hFontLabel = nullptr, hFontValue = nullptr;
static HACCEL hAccel = nullptr;
static HBRUSH hBgBrush = nullptr;
static HFONT hFont9 = nullptr;
static HFONT hFontBold = nullptr;
static HFONT hFontH2 = nullptr;
static HFONT hFontHeadline = nullptr;
static HBITMAP hAboutBitmap = nullptr;
static UINT_PTR cpuInfoTimer = 0;
static int currentPage = 0;

#ifndef IDI_ICON1
#define IDI_ICON1 101
#endif
#ifndef IDB_PNG1
#define IDB_PNG1 201
#endif

const wchar_t* versionnumber = L"0.1.0";
const wchar_t* APP_CLASS = L"WinInfoAppMainClass2025_UNIQUE";
const wchar_t* APP_BASE_TITLE = L"WinInfoApp";
const COLORREF APP_BG_COLOR = RGB(240,240,240);

const wchar_t* APP_WINDOW_TITLE = L"WinInfoApp - V. 0.1.0";


// --- Utility for DPI scaling ---
int ScaleByDPI(int value, UINT dpi) {
    return MulDiv(value, dpi, 96);
}

// --- Font creation ---
HFONT CreateUIFont(int pt, UINT dpi, int weight = FW_NORMAL, bool h2 = false) {
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(pt, dpi, 72);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    const wchar_t* fontName = L"Segoe UI";
    size_t i = 0;
    for (; i < LF_FACESIZE - 1 && fontName[i]; ++i)
        lf.lfFaceName[i] = fontName[i];
    lf.lfFaceName[i] = L'\0';
    if (h2) lf.lfHeight = -MulDiv(pt + 5, dpi, 72);
    return CreateFontIndirect(&lf);
}

RECT GetBoundingRectOfControls(const HWND* controls, int count) {
    RECT bounds = { INT_MAX, INT_MAX, INT_MIN, INT_MIN };
    for (int i = 0; i < count; ++i) {
        RECT rc;
        if (controls[i] && IsWindowVisible(controls[i])) {
            GetWindowRect(controls[i], &rc);
            // Convert screen to client coordinates
            POINT tl = { rc.left, rc.top };
            POINT br = { rc.right, rc.bottom };
            ScreenToClient(GetParent(controls[i]), &tl);
            ScreenToClient(GetParent(controls[i]), &br);
            if (tl.x < bounds.left) bounds.left = tl.x;
            if (tl.y < bounds.top) bounds.top = tl.y;
            if (br.x > bounds.right) bounds.right = br.x;
            if (br.y > bounds.bottom) bounds.bottom = br.y;
        }
    }
    // If no controls, return a default rect
    if (bounds.left == INT_MAX) {
        bounds.left = bounds.top = 0;
        bounds.right = bounds.bottom = 100;
    }
    return bounds;
}


// --- CPU Info Page Window Procedure ---
LRESULT CALLBACK CpuInfoPageProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HFONT hFontLabel = NULL, hFontValue = NULL;
    switch (msg) {
    case WM_CREATE: {
      

        LOGFONT lf = {};
        lf.lfHeight = 20;
        lf.lfWeight = FW_BOLD;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        hFontLabel = CreateFontIndirect(&lf);
        lf.lfWeight = FW_NORMAL;
        hFontValue = CreateFontIndirect(&lf);

        SetTimer(hWnd, 2, 250, NULL); // Timer ID must be 2 for updates
        break;
    }
    


    case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    if (currentPage == 1) { // CPU Info page
        auto rows = getCpuInfoRows();
        int x = 20, y = 20;
        int labelWidth = 220, valueWidth = 220, rowHeight = 32;
        SetBkMode(hdc, TRANSPARENT);

        for (size_t i = 0; i < rows.size(); ++i) {
            // Draw label (bold)
            SelectObject(hdc, hFontLabel);
            SetTextColor(hdc, RGB(0,0,0));
            RECT rLabel = { x, y, x + labelWidth, y + rowHeight };
            DrawTextW(hdc, rows[i].label.c_str(), -1, &rLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Draw value (dark blue)
            SelectObject(hdc, hFontValue);
            SetTextColor(hdc, RGB(0,0,139));
            RECT rValue = { x + labelWidth, y, x + labelWidth + valueWidth, y + rowHeight };
            DrawTextW(hdc, rows[i].value.c_str(), -1, &rValue, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            y += rowHeight;
        }
    }
    // ...existing code for other pages...
    EndPaint(hWnd, &ps);
    break;
    }
}
return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- Show the CPU Info Page as a child window or dialog ---
void ShowCpuInfoPage(HWND hParent) {
    const wchar_t* className = L"CpuInfoPageClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = CpuInfoPageProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, className, L"CPU Info",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 150, 650, 350, hParent, NULL, GetModuleHandle(NULL), NULL);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
}


void ResizeWindowToFitContent(HWND hwnd, const HWND* controls, int count) {
    UINT dpi = GetDpiForWindow(hwnd);
    int minW = ScaleByDPI(350, dpi), minH = ScaleByDPI(200, dpi);
    int maxW = ScaleByDPI(1300, dpi), maxH = ScaleByDPI(900, dpi);

    RECT bounds = GetBoundingRectOfControls(controls, count);
    int contentW = bounds.right - bounds.left + ScaleByDPI(32, dpi); // add margin
    int contentH = bounds.bottom - bounds.top + ScaleByDPI(32, dpi);

    int winW = std::min(std::max(contentW, minW), maxW);
    int winH = std::min(std::max(contentH, minH), maxH);

    // Add scrollbars if needed
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    if (contentH > maxH) style |= WS_VSCROLL;
    else style &= ~WS_VSCROLL;
    if (contentW > maxW) style |= WS_HSCROLL;
    else style &= ~WS_HSCROLL;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    SetWindowPos(hwnd, NULL, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
}




// --- GDI+ startup/shutdown ---
ULONG_PTR gdiplusToken;
void InitGDIPlus() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}
void ShutdownGDIPlus() {
    GdiplusShutdown(gdiplusToken);
}

// --- PNG resource loader ---
HBITMAP LoadPngFromResource(int resId) {
    HMODULE hMod = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), L"RCDATA");
    if (!hRes) return NULL;
    HGLOBAL hMem = LoadResource(hMod, hRes);
    if (!hMem) return NULL;
    DWORD size = SizeofResource(hMod, hRes);
    void* pData = LockResource(hMem);
    if (!pData) return NULL;

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hBuffer) return NULL;
    void* pBuffer = GlobalLock(hBuffer);
    memcpy(pBuffer, pData, size);
    GlobalUnlock(hBuffer);

    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) != S_OK) {
        GlobalFree(hBuffer);
        return NULL;
    }

    Bitmap* bmp = Bitmap::FromStream(pStream);
    HBITMAP hBmp = NULL;
    if (bmp && bmp->GetLastStatus() == Ok) {
        bmp->GetHBITMAP(Color::White, &hBmp);
    }
    delete bmp;
    pStream->Release();
    return hBmp;
}

// --- Menu text ---
const wchar_t* MENU_TEXT_SUMMARY = L"Summary";
const wchar_t* MENU_TEXT_CPUINFO = L"CPU Info";
const wchar_t* MENU_TEXT_EXIT    = L"Exit";
const wchar_t* MENU_TEXT_HELP    = L"Help";
const wchar_t* MENU_TEXT_ABOUT   = L"About";

// --- Icon loader ---
HICON LoadAppIcon(int size = 32) {
    HICON hIcon = (HICON)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(NULL, IDI_APPLICATION, IMAGE_ICON, size, size, LR_SHARED);
    }
    return hIcon;
}

HBITMAP LoadAboutImage() {
    return LoadPngFromResource(IDB_PNG1);
}

// --- URL opener ---
void OpenURL(const wchar_t* url) {
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

// --- Error message helper ---
std::wstring GetLastErrorMessage(DWORD err) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&buf, 0, NULL);
    std::wstring msg = buf ? buf : L"Unknown error";
    if (buf) LocalFree(buf);
    return msg;
}

// --- Quit dialog ---
struct QuitDialogData {
    HWND hDlg, hYes, hNo, hEdit;
    HFONT hFont9;
    int result;
};

LRESULT CALLBACK QuitDialogWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    QuitDialogData* data = (QuitDialogData*)GetWindowLongPtrW(hDlg, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        UINT dpi = GetDpiForWindow(hDlg);
        data = new QuitDialogData();
        data->hDlg = hDlg;
        data->hFont9 = CreateUIFont(9, dpi);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);

        data->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Quit?",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_TABSTOP,
            ScaleByDPI(20, dpi), ScaleByDPI(28, dpi), ScaleByDPI(280, dpi), ScaleByDPI(32, dpi), hDlg, NULL, GetModuleHandle(NULL), NULL);

        data->hYes = CreateWindowExW(0, L"BUTTON", L"Yes",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            ScaleByDPI(40, dpi), ScaleByDPI(80, dpi), ScaleByDPI(90, dpi), ScaleByDPI(36, dpi), hDlg, (HMENU)IDYES, GetModuleHandle(NULL), NULL);

        data->hNo = CreateWindowExW(0, L"BUTTON", L"No",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            ScaleByDPI(170, dpi), ScaleByDPI(80, dpi), ScaleByDPI(90, dpi), ScaleByDPI(36, dpi), hDlg, (HMENU)IDNO, GetModuleHandle(NULL), NULL);

        SendMessageW(data->hYes, WM_SETFONT, (WPARAM)data->hFont9, TRUE);
        SendMessageW(data->hNo, WM_SETFONT, (WPARAM)data->hFont9, TRUE);
        SendMessageW(data->hEdit, WM_SETFONT, (WPARAM)data->hFont9, TRUE);

        SetFocus(data->hYes);
        data->result = IDNO;
    } 


    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, APP_BG_COLOR);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        static HBRUSH hBrush = CreateSolidBrush(APP_BG_COLOR);
        return (LRESULT)hBrush;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDYES:
            data->result = IDYES;
            EndDialog(hDlg, IDYES);
            return TRUE;
        case IDNO:
            data->result = IDNO;
            EndDialog(hDlg, IDNO);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDNO);
        return TRUE;
    
    
    case WM_DESTROY:
    // Only clean up dialog-specific resources
    if (data && data->hFont9) DeleteObject(data->hFont9);
    PostQuitMessage(0);
    break;

    // Clean up other fonts and resources
    ShutdownGDIPlus();
    if (hAccel) DestroyAcceleratorTable(hAccel);
    if (hBgBrush) DeleteObject(hBgBrush);
    if (hFontH2) DeleteObject(hFontH2);
    if (hFontBold) DeleteObject(hFontBold);
    if (hFontHeadline) DeleteObject(hFontHeadline);
    if (hFont9) DeleteObject(hFont9);
    if (hAboutBitmap) DeleteObject(hAboutBitmap);

    PostQuitMessage(0);
    break;



    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            EndDialog(hDlg, IDNO);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int ShowStyledQuitDialog(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    const int dlgWidth = ScaleByDPI(320, dpi), dlgHeight = ScaleByDPI(160, dpi);
    HINSTANCE hInst = GetModuleHandle(NULL);

    RECT rcParent;
    GetWindowRect(hwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - dlgWidth) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - dlgHeight) / 2;

    std::wstring title = std::wstring(APP_BASE_TITLE) + L" - V. " + versionnumber;
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"QuitDialogClass", title.c_str(),
        WS_POPUP | WS_CAPTION | DS_MODALFRAME | WS_SYSMENU,
        x, y, dlgWidth, dlgHeight, hwnd, NULL, hInst, NULL);

    if (!hDlg) {
        std::wstring msg = L"Quit?";
        return MessageBoxW(hwnd, msg.c_str(), title.c_str(), MB_ICONQUESTION | MB_YESNO);
    }

    ShowWindow(hDlg, SW_SHOW);
    SetForegroundWindow(hDlg);

    MSG msg;
    int result = IDNO;
    BOOL done = FALSE;
    while (!done && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            if (msg.hwnd == hDlg && (LOWORD(msg.wParam) == IDYES || LOWORD(msg.wParam) == IDNO)) {
                result = LOWORD(msg.wParam);
                done = TRUE;
            }
        }
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyWindow(hDlg);
    return result;
}

// --- Owner-draw menu ---
void DrawMenuItemWithIcon(HMENU hMenu, LPDRAWITEMSTRUCT dis, HICON hIcon) {
    if (dis->itemAction == ODA_DRAWENTIRE && (dis->itemState & ODS_SELECTED) == 0 && dis->itemID == -1) {
        RECT rc = dis->rcItem;
        HDC hdc = dis->hDC;
        HBRUSH bgBrush = CreateSolidBrush(RGB(255,255,255));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
        int y = (rc.top + rc.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(220,220,220));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, rc.left + 8, y, NULL);
        LineTo(hdc, rc.right - 8, y);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        return;
    }

    COLORREF bgColor = RGB(255,255,255);
    if (dis->itemState & ODS_SELECTED)
        bgColor = GetSysColor(COLOR_HIGHLIGHT);
    COLORREF textColor = (dis->itemState & ODS_SELECTED) ? GetSysColor(COLOR_HIGHLIGHTTEXT) : GetSysColor(COLOR_MENUTEXT);

    RECT rc = dis->rcItem;
    HDC hdc = dis->hDC;

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    const wchar_t* text = nullptr;
    switch (dis->itemID) {
        case 1001: text = MENU_TEXT_SUMMARY; break;
        case 1002: text = MENU_TEXT_CPUINFO; break;
        case 1003: text = MENU_TEXT_EXIT; break;
        case 2001: text = MENU_TEXT_ABOUT; break;
        default: text = L""; break;
    }

    int iconMargin = 6;
    int iconSize = 20;
    int textOffset = iconMargin + iconSize + 6;
    if (hIcon) {
        DrawIconEx(hdc, rc.left + iconMargin, rc.top + (rc.bottom - rc.top - iconSize) / 2, hIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
    } else {
        textOffset = iconMargin;
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    RECT textRc = rc;
    textRc.left += textOffset;
    DrawTextW(hdc, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
}

// --- Accelerator ---
HACCEL CreateAppAccelerators() {
    ACCEL accels[1];
    accels[0].fVirt = FCONTROL | FVIRTKEY;
    accels[0].key = 'W';
    accels[0].cmd = 1003;
    return CreateAcceleratorTableW(accels, 1);
}

// --- Main window proc ---
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hSummaryEdit = nullptr;
    static HWND hCpuInfoEdit = nullptr;
    static HWND hAboutIcon = nullptr, hAboutStatic1 = nullptr, hAboutStatic2 = nullptr, hAboutStatic3 = nullptr, hVersionValue = nullptr, hContactLabel = nullptr;
    static HWND hLicenseBtn = nullptr, hSourceBtn = nullptr;
    static HWND hContactBtn = nullptr;
    static HWND hAboutStatic4 = nullptr, hAboutStatic5 = nullptr;
    static HWND hAboutImage = nullptr;
    static HWND hAboutInfoLine = nullptr;
    static HMENU hMenu = nullptr;
    static HICON hSummaryIcon = nullptr;
    static HICON hCpuIcon = nullptr;
    static HICON hExitIcon = nullptr;
    static HICON hAboutIconImg = nullptr;
    static bool sizingAboutManually = false;

    

    switch (msg) {
    case WM_CREATE: {
        InitGDIPlus();
        UINT dpi = GetDpiForWindow(hwnd);
        hFont9 = CreateUIFont(9, dpi);
        hFontBold = CreateUIFont(9, dpi, FW_BOLD);
        hFontH2 = CreateUIFont(9, dpi, FW_NORMAL, true);
        hFontHeadline = CreateUIFont(12, dpi, FW_BOLD);

        LOGFONT lf = {};
        lf.lfHeight = 20;
        lf.lfWeight = FW_BOLD;
        wcscpy_s(lf.lfFaceName, L"Segoe UI");
        hFontLabel = CreateFontIndirect(&lf);
        lf.lfWeight = FW_NORMAL;
        hFontValue = CreateFontIndirect(&lf);

        SetWindowTextW(hwnd, APP_WINDOW_TITLE);

        hMenu = CreateMenu();
        HMENU hMainMenu = CreateMenu();
        HMENU hHelpMenu = CreateMenu();

        hSummaryIcon = (HICON)LoadImageW(NULL, IDI_INFORMATION, IMAGE_ICON, 32, 32, LR_SHARED);
        hCpuIcon = (HICON)LoadImageW(NULL, IDI_SHIELD, IMAGE_ICON, 32, 32, LR_SHARED);
        hExitIcon = (HICON)LoadImageW(NULL, IDI_ERROR, IMAGE_ICON, 32, 32, LR_SHARED);
        hAboutIconImg = LoadAppIcon(70);
        hAboutBitmap = LoadAboutImage();

        // Read persisted language early so labels are localized
        g_currentLanguage = ReadLanguageSetting();

        const wchar_t* lblSummary = g_currentLanguage ? L"Oppsummering" : L"Summary";
        const wchar_t* lblCpu = g_currentLanguage ? L"CPU-informasjon" : L"CPU Info";
        const wchar_t* lblStorage = g_currentLanguage ? L"Lagring" : L"Storage";
        const wchar_t* lblExit = g_currentLanguage ? L"Avslutt" : L"Exit";
        const wchar_t* lblHelp = g_currentLanguage ? L"Hjelp" : L"Help";
        const wchar_t* lblMenuTop = g_currentLanguage ? L"Meny" : L"Menu";
        const wchar_t* lblAbout = g_currentLanguage ? L"Om" : L"About";

        MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
        mii.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
        mii.fType = MFT_OWNERDRAW;

        mii.wID = 1001; mii.dwTypeData = (LPWSTR)lblSummary;
        InsertMenuItemW(hMainMenu, 0, TRUE, &mii);

        mii.wID = 1002; mii.dwTypeData = (LPWSTR)lblCpu;
        InsertMenuItemW(hMainMenu, 1, TRUE, &mii);

        // Language submenu (insert after CPU item)
        HMENU hLangMenu = CreateMenu();
        AppendMenuW(hLangMenu, MF_STRING, ID_LANG_EN, g_currentLanguage ? L"Engelsk" : L"English");
        AppendMenuW(hLangMenu, MF_STRING, ID_LANG_NO, L"Norsk");
        InsertMenuW(hMainMenu, 2, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hLangMenu, g_currentLanguage ? L"Språk" : L"Language");
        CheckMenuRadioItem(hLangMenu, ID_LANG_EN, ID_LANG_NO, (g_currentLanguage==1)?ID_LANG_NO:ID_LANG_EN, MF_BYCOMMAND);

        MENUITEMINFOW sep = { sizeof(MENUITEMINFOW) };
        sep.fMask = MIIM_FTYPE;
        sep.fType = MFT_SEPARATOR;
        sep.wID = -1;
        InsertMenuItemW(hMainMenu, 2, TRUE, &sep);

        mii.wID = 1003; mii.dwTypeData = (LPWSTR)MENU_TEXT_EXIT;
        mii.fType = MFT_OWNERDRAW;
        InsertMenuItemW(hMainMenu, 3, TRUE, &mii);

        MENUITEMINFOW miiHelp = { sizeof(MENUITEMINFOW) };
        miiHelp.fMask = MIIM_ID | MIIM_STRING | MIIM_FTYPE;
        miiHelp.fType = MFT_OWNERDRAW;
        miiHelp.wID = 2001; miiHelp.dwTypeData = (LPWSTR)lblAbout;
        InsertMenuItemW(hHelpMenu, 0, TRUE, &miiHelp);

        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hMainMenu, (LPWSTR)lblMenuTop);
        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, (LPWSTR)lblHelp);
        SetMenu(hwnd, hMenu);

        const wchar_t* summaryText = L"Summary:\r\nThis is the summary page. Add your summary content here.";
        hSummaryEdit = CreateWindowExW(
            0, L"EDIT", summaryText,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            16, 16, 380, 180, hwnd, NULL, GetModuleHandle(NULL), NULL
        );
        std::wstring cpuInfoText = GetCpuInfoText();
        hCpuInfoEdit = CreateWindowExW(
            0, L"EDIT", cpuInfoText.c_str(),
            WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            16, 16, 380, 180, hwnd, NULL, GetModuleHandle(NULL), NULL
        );
        hAboutIcon = CreateWindowExW(0, L"STATIC", NULL,
            WS_CHILD | SS_ICON, 0, 0, 84, 84, hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessageW(hAboutIcon, STM_SETICON, (WPARAM)hAboutIconImg, 0);

        hAboutImage = CreateWindowExW(0, L"STATIC", NULL,
            WS_CHILD | SS_BITMAP, 0, 0, 128, 128, hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessageW(hAboutImage, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hAboutBitmap);

        hAboutStatic1 = CreateWindowExW(0, L"STATIC", (std::wstring(APP_BASE_TITLE) + L" - About").c_str(),
            WS_CHILD, 0, 0, 300, 32, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hAboutStatic2 = CreateWindowExW(0, L"STATIC", L"Copyleft 2025 Nalle Berg",
            WS_CHILD, 0, 0, 300, 24, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hAboutInfoLine = CreateWindowExW(0, L"STATIC", L"Information about your PC system and Operating system.",
            WS_CHILD, 0, 0, 400, 24, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hAboutStatic3 = CreateWindowExW(0, L"STATIC", L"Version:",
            WS_CHILD, 0, 0, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hVersionValue = CreateWindowExW(0, L"STATIC", versionnumber,
            WS_CHILD, 0, 0, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hContactLabel = CreateWindowExW(0, L"STATIC", L"Contact:",
            WS_CHILD, 0, 0, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hContactBtn = CreateWindowExW(0, L"BUTTON", L"prog@nalle.no",
            WS_CHILD | BS_FLAT, 0, 0, 120, 20, hwnd, (HMENU)3003, GetModuleHandle(NULL), NULL);
        hAboutStatic4 = CreateWindowExW(0, L"STATIC", L"License:",
            WS_CHILD | SS_LEFTNOWORDWRAP | SS_NOPREFIX, 0, 0, 70, 24, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hAboutStatic5 = CreateWindowExW(0, L"STATIC", L"Source:",
            WS_CHILD | SS_LEFTNOWORDWRAP | SS_NOPREFIX, 0, 0, 70, 24, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hLicenseBtn = CreateWindowExW(0, L"BUTTON", L"GPL V2",
            WS_CHILD, 0, 0, 70, 24, hwnd, (HMENU)3001, GetModuleHandle(NULL), NULL);
        hSourceBtn = CreateWindowExW(0, L"BUTTON", L"GitHub",
            WS_CHILD, 0, 0, 70, 24, hwnd, (HMENU)3002, GetModuleHandle(NULL), NULL);

        SendMessageW(hSummaryEdit, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hCpuInfoEdit, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hAboutStatic1, WM_SETFONT, (WPARAM)hFontHeadline, TRUE);
        SendMessageW(hAboutStatic2, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessageW(hAboutInfoLine, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hAboutStatic3, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessageW(hVersionValue, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hContactLabel, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessageW(hContactBtn, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hAboutStatic4, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessageW(hAboutStatic5, WM_SETFONT, (WPARAM)hFontBold, TRUE);
        SendMessageW(hLicenseBtn, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hSourceBtn, WM_SETFONT, (WPARAM)hFont9, TRUE);
        SendMessageW(hAboutImage, WM_SETFONT, (WPARAM)hFont9, TRUE);

        ShowWindow(hSummaryEdit, SW_SHOW);
        ShowWindow(hCpuInfoEdit, SW_HIDE);
        ShowWindow(hAboutIcon, SW_HIDE);
        ShowWindow(hAboutImage, SW_HIDE);
        ShowWindow(hAboutStatic1, SW_HIDE);
        ShowWindow(hAboutStatic2, SW_HIDE);
        ShowWindow(hAboutInfoLine, SW_HIDE);
        ShowWindow(hAboutStatic3, SW_HIDE);
        ShowWindow(hVersionValue, SW_HIDE);
        ShowWindow(hContactLabel, SW_HIDE);
        ShowWindow(hContactBtn, SW_HIDE);
        ShowWindow(hAboutStatic4, SW_HIDE);
        ShowWindow(hAboutStatic5, SW_HIDE);
        ShowWindow(hLicenseBtn, SW_HIDE);
        ShowWindow(hSourceBtn, SW_HIDE);

        hAccel = CreateAppAccelerators();
        hBgBrush = CreateSolidBrush(APP_BG_COLOR);
        break;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, APP_BG_COLOR);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        static HBRUSH hBrush = CreateSolidBrush(APP_BG_COLOR);
        return (LRESULT)hBrush;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, APP_BG_COLOR);
        if ((HWND)lParam == hAboutInfoLine) {
            SetTextColor(hdc, RGB(0, 102, 204));
        } else if ((HWND)lParam == hAboutStatic1) {
            SetTextColor(hdc, RGB(10, 30, 120));
        } else {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }
        static HBRUSH hBrush = CreateSolidBrush(APP_BG_COLOR);
        return (LRESULT)hBrush;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hBgBrush ? hBgBrush : CreateSolidBrush(APP_BG_COLOR));
        return 1;
    }
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        if ((UINT)mis->itemID == (UINT)-1) {
            mis->itemHeight = 8;
            mis->itemWidth = 220;
        } else {
            mis->itemHeight = 28;
            mis->itemWidth = 220;
        }
        return TRUE;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlType == ODT_MENU) {
            HICON icon = nullptr;
            switch ((UINT)dis->itemID) {
                case 1001: icon = hSummaryIcon; break;
                case 1002: icon = hCpuIcon; break;
                case 1003: icon = hExitIcon; break;
                case 2001: icon = hAboutIconImg; break;
                case (UINT)-1: icon = nullptr; break;
                default: icon = nullptr; break;
            }
            DrawMenuItemWithIcon((HMENU)wParam, dis, icon);
            return TRUE;
        }
        break;
    }

    case WM_TIMER: {
    if (wParam == 2 && currentPage == 1 && hCpuInfoEdit) {
        SetWindowTextW(hCpuInfoEdit, GetCpuInfoText().c_str());
    }
    break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case 1001: // Summary
            currentPage = 0;
            ShowWindow(hSummaryEdit, SW_SHOW);
            ShowWindow(hCpuInfoEdit, SW_HIDE);
            ShowWindow(hAboutIcon, SW_HIDE);
            ShowWindow(hAboutImage, SW_HIDE);
            ShowWindow(hAboutStatic1, SW_HIDE);
            ShowWindow(hAboutStatic2, SW_HIDE);
            ShowWindow(hAboutInfoLine, SW_HIDE);
            ShowWindow(hAboutStatic3, SW_HIDE);
            ShowWindow(hVersionValue, SW_HIDE);
            ShowWindow(hContactLabel, SW_HIDE);
            ShowWindow(hContactBtn, SW_HIDE);
            ShowWindow(hAboutStatic4, SW_HIDE);
            ShowWindow(hAboutStatic5, SW_HIDE);
            ShowWindow(hLicenseBtn, SW_HIDE);
            ShowWindow(hSourceBtn, SW_HIDE);
            {
                HWND summaryControls[] = { hSummaryEdit };
                ResizeWindowToFitContent(hwnd, summaryControls, 1);
            }
            KillTimer(hwnd, 2);
            break;
       
        case 1002: // CPU Info
        currentPage = 1;
        ShowWindow(hSummaryEdit, SW_HIDE);
        ShowWindow(hCpuInfoEdit, SW_SHOW);
        ShowWindow(hAboutIcon, SW_HIDE);
        ShowWindow(hAboutImage, SW_HIDE);
        ShowWindow(hAboutStatic1, SW_HIDE);
        ShowWindow(hAboutStatic2, SW_HIDE);
        ShowWindow(hAboutInfoLine, SW_HIDE);
        ShowWindow(hAboutStatic3, SW_HIDE);
        ShowWindow(hVersionValue, SW_HIDE);
        ShowWindow(hContactLabel, SW_HIDE);
        ShowWindow(hContactBtn, SW_HIDE);
        ShowWindow(hAboutStatic4, SW_HIDE);
        ShowWindow(hAboutStatic5, SW_HIDE);
        ShowWindow(hLicenseBtn, SW_HIDE);
        ShowWindow(hSourceBtn, SW_HIDE);
        {
            HWND cpuControls[] = { hCpuInfoEdit };
            ResizeWindowToFitContent(hwnd, cpuControls, 1);
        }
        // Start live update timer
        SetTimer(hwnd, 2, 1000, NULL);
        // Immediately update the text
        SetWindowTextW(hCpuInfoEdit, GetCpuInfoText().c_str());
        break;

        case ID_LANG_EN:
            WriteLanguageSetting(0);
            MessageBoxW(hwnd, L"Language set to English. Restart the app to apply the change.", APP_WINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
            break;
        case ID_LANG_NO:
            WriteLanguageSetting(1);
            MessageBoxW(hwnd, L"Language set to Norsk. Restart the app to apply the change.", APP_WINDOW_TITLE, MB_OK | MB_ICONINFORMATION);
            break;


        case 1003: // Exit
            SendMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case 2001: // About
            currentPage = 2;
            sizingAboutManually = true;
            ShowWindow(hSummaryEdit, SW_HIDE);
            ShowWindow(hCpuInfoEdit, SW_HIDE);
            ShowWindow(hAboutIcon, SW_SHOW);
            ShowWindow(hAboutImage, SW_SHOW);
            ShowWindow(hAboutStatic1, SW_SHOW);
            ShowWindow(hAboutStatic2, SW_SHOW);
            ShowWindow(hAboutInfoLine, SW_SHOW);
            ShowWindow(hAboutStatic3, SW_SHOW);
            ShowWindow(hVersionValue, SW_SHOW);
            ShowWindow(hContactLabel, SW_SHOW);
            ShowWindow(hContactBtn, SW_SHOW);
            ShowWindow(hAboutStatic4, SW_SHOW);
            ShowWindow(hAboutStatic5, SW_SHOW);
            ShowWindow(hLicenseBtn, SW_SHOW);
            ShowWindow(hSourceBtn, SW_SHOW);
            {
                UINT dpi = GetDpiForWindow(hwnd);
                int desiredW = ScaleByDPI(400, dpi);
                int desiredH = ScaleByDPI(400, dpi);
                SetWindowPos(hwnd, NULL, 0, 0, desiredW, desiredH, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
                RECT rc;
                GetClientRect(hwnd, &rc);
                int winW = rc.right - rc.left;
                int margin = ScaleByDPI(12, dpi);
                int y = margin;
                HDC hdc = GetDC(hwnd);
                HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeadline);
                std::wstring headlineText = std::wstring(APP_BASE_TITLE) + L" - About";
                SIZE szHeadline;
                GetTextExtentPoint32W(hdc, headlineText.c_str(), (int)headlineText.length(), &szHeadline);
                SelectObject(hdc, hFontBold);
                SIZE szCopyright;
                GetTextExtentPoint32W(hdc, L"Copyleft 2025 Nalle Berg", 24, &szCopyright);
                SelectObject(hdc, oldFont);
                ReleaseDC(hwnd, hdc);
                int headlineW = szHeadline.cx + ScaleByDPI(40, dpi);
                int headlineH = ScaleByDPI(32, dpi);
                int xHeadline = (winW - headlineW) / 2;
                MoveWindow(hAboutStatic1, xHeadline, y, headlineW, headlineH, TRUE);
                y += headlineH + margin;
                int logoW = ScaleByDPI(84, dpi), logoH = ScaleByDPI(84, dpi);
                int xLogo = (winW - logoW) / 2;
                MoveWindow(hAboutIcon, xLogo, y, logoW, logoH, TRUE);
                y += logoH + margin;
                int copyrightW = szCopyright.cx + ScaleByDPI(40, dpi);
                int copyrightH = ScaleByDPI(24, dpi);
                int xCopyright = (winW - copyrightW) / 2;
                MoveWindow(hAboutStatic2, xCopyright, y, copyrightW, copyrightH, TRUE);
                y += copyrightH + margin;
                int infoW = ScaleByDPI(340, dpi), infoH = ScaleByDPI(24, dpi);
                int xInfo = (winW - infoW) / 2;
                MoveWindow(hAboutInfoLine, xInfo, y, infoW, infoH, TRUE);
                y += infoH + margin;
                int boxW = ScaleByDPI(340, dpi);
                int boxH = ScaleByDPI(120, dpi);
                int xBox = (winW - boxW) / 2;
                int yBox = y + margin;
                int labelW = ScaleByDPI(80, dpi), labelH = ScaleByDPI(20, dpi);
                int valueW = ScaleByDPI(80, dpi), valueH = ScaleByDPI(20, dpi);
                int xLabel = xBox;
                int yLabel = yBox;
                MoveWindow(hAboutStatic3, xLabel, yLabel, labelW, labelH, TRUE);
                MoveWindow(hVersionValue, xLabel + labelW, yLabel, valueW, valueH, TRUE);
                int contactLabelW = ScaleByDPI(80, dpi), contactLabelH = ScaleByDPI(20, dpi);
                int contactBtnW = ScaleByDPI(120, dpi), contactBtnH = ScaleByDPI(20, dpi);
                int yContact = yLabel + labelH + margin;
                MoveWindow(hContactLabel, xLabel, yContact, contactLabelW, contactLabelH, TRUE);
                MoveWindow(hContactBtn, xLabel + contactLabelW, yContact, contactBtnW, contactBtnH, TRUE);
                int btnW = ScaleByDPI(70, dpi), btnH = ScaleByDPI(24, dpi);
                int labelBtnSpacing = ScaleByDPI(8, dpi);
                int yLicenseSource = yContact + contactBtnH + margin;
                MoveWindow(hAboutStatic4, xLabel, yLicenseSource, btnW, btnH, TRUE);
                MoveWindow(hLicenseBtn, xLabel + btnW + labelBtnSpacing, yLicenseSource, btnW, btnH, TRUE);
                MoveWindow(hAboutStatic5, xLabel + (btnW + labelBtnSpacing) * 2, yLicenseSource, btnW, btnH, TRUE);
                MoveWindow(hSourceBtn, xLabel + (btnW + labelBtnSpacing) * 3, yLicenseSource, btnW, btnH, TRUE);
                int imgW = ScaleByDPI(128, dpi), imgH = ScaleByDPI(128, dpi);
                int xImg = winW - imgW - margin;
                int yImg = yBox + boxH + margin;
                MoveWindow(hAboutImage, xImg, yImg, imgW, imgH, TRUE);
                sizingAboutManually = false;
            }
             KillTimer(hwnd, 2);
            break;
        case 3001:
            OpenURL(L"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html");
            break;
        case 3002:
            OpenURL(L"https://github.com/NalleBerg/WinInfoApp");
            break;
        case 3003:
            OpenURL(L"mailto:prog@nalle.no");
            break;
        }
        break;
    }
    case WM_SIZE: {
        if (currentPage == 2 && sizingAboutManually) {
            return 0;
        }
        UINT dpi = 96;
        if (hwnd) {
            dpi = GetDpiForWindow(hwnd);
        }
        int margin = ScaleByDPI(8, dpi);
        int logoW = ScaleByDPI(84, dpi), logoH = ScaleByDPI(84, dpi);
        int btnW = ScaleByDPI(70, dpi), btnH = ScaleByDPI(24, dpi);
        int labelBtnSpacing = ScaleByDPI(8, dpi);
        HDC hdc = GetDC(hwnd);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFontHeadline);
        std::wstring headlineText = std::wstring(APP_BASE_TITLE) + L" - About";
        SIZE szHeadline;
        GetTextExtentPoint32W(hdc, headlineText.c_str(), (int)headlineText.length(), &szHeadline);
        SelectObject(hdc, hFontBold);
        SIZE szCopyright;
        GetTextExtentPoint32W(hdc, L"Copyleft 2025 Nalle Berg", 24, &szCopyright);
        SelectObject(hdc, hFont9);
        SIZE szInfo;
        GetTextExtentPoint32W(hdc, L"Information about your PC system and Operating system.", 52, &szInfo);
        SelectObject(hdc, oldFont);
        int containerW = ScaleByDPI(340, dpi);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int curW = rc.right - rc.left;
        int curH = rc.bottom - rc.top;
        SetWindowTextW(hwnd, APP_WINDOW_TITLE);
        MoveWindow(hSummaryEdit, ScaleByDPI(10, dpi), ScaleByDPI(10, dpi), curW - ScaleByDPI(20, dpi), curH - ScaleByDPI(20, dpi), TRUE);
        MoveWindow(hCpuInfoEdit, ScaleByDPI(10, dpi), ScaleByDPI(10, dpi), curW - ScaleByDPI(20, dpi), curH - ScaleByDPI(20, dpi), TRUE);
        int winW = curW, winH = curH;
        int headlineW = szHeadline.cx + ScaleByDPI(40, dpi);
        int headlineH = ScaleByDPI(32, dpi);
        int xHeadline = (winW - headlineW) / 2;
        int yHeadline = margin;
        MoveWindow(hAboutStatic1, xHeadline, yHeadline, headlineW, headlineH, TRUE);
        int xLogo = (winW - logoW) / 2;
        int yLogo = yHeadline + headlineH + margin;
        MoveWindow(hAboutIcon, xLogo, yLogo, logoW, logoH, TRUE);
        int copyrightW = szCopyright.cx + ScaleByDPI(40, dpi);
        int copyrightH = ScaleByDPI(24, dpi);
        int xCopyright = (winW - copyrightW) / 2;
        int yCopyright = yLogo + logoH + margin;
        MoveWindow(hAboutStatic2, xCopyright, yCopyright, copyrightW, copyrightH, TRUE);
        int infoW = szInfo.cx + ScaleByDPI(40, dpi);
        int infoH = ScaleByDPI(24, dpi);
        int xInfo = (winW - infoW) / 2;
        int yInfo = yCopyright + copyrightH + margin;
        MoveWindow(hAboutInfoLine, xInfo, yInfo, infoW, infoH, TRUE);
        int xContainer = (winW - containerW) / 2;
        int yContainer = yInfo + infoH + margin * 2;
        int labelW = ScaleByDPI(80, dpi), labelH = ScaleByDPI(20, dpi);
        int valueW = ScaleByDPI(80, dpi), valueH = ScaleByDPI(20, dpi);
        int xLabel = xContainer;
        int yLabel = yContainer;
        MoveWindow(hAboutStatic3, xLabel, yLabel, labelW, labelH, TRUE);
        MoveWindow(hVersionValue, xLabel + labelW, yLabel, valueW, valueH, TRUE);
        int contactLabelW = ScaleByDPI(80, dpi), contactLabelH = ScaleByDPI(20, dpi);
        int contactBtnW = ScaleByDPI(120, dpi), contactBtnH = ScaleByDPI(20, dpi);
        int yContact = yLabel + labelH + margin;
        MoveWindow(hContactLabel, xLabel, yContact, contactLabelW, contactLabelH, TRUE);
        MoveWindow(hContactBtn, xLabel + contactLabelW, yContact, contactBtnW, contactBtnH, TRUE);
        int yLicenseSource = yContact + contactBtnH + margin;
        MoveWindow(hAboutStatic4, xLabel, yLicenseSource, btnW, btnH, TRUE);
        MoveWindow(hLicenseBtn, xLabel + btnW + labelBtnSpacing, yLicenseSource, btnW, btnH, TRUE);
        MoveWindow(hAboutStatic5, xLabel + btnW*2 + labelBtnSpacing*2, yLicenseSource, btnW, btnH, TRUE);
        MoveWindow(hSourceBtn, xLabel + btnW*3 + labelBtnSpacing*3, yLicenseSource, btnW, btnH, TRUE);
        int imgW = ScaleByDPI(128, dpi), imgH = ScaleByDPI(128, dpi);
        int xImg = winW - imgW - margin;
        int yImg = yLogo + logoH + margin;
        MoveWindow(hAboutImage, xImg, yImg, imgW, imgH, TRUE);
        ReleaseDC(hwnd, hdc);
        if (currentPage == 2) {
            HWND aboutControls[] = {
                hAboutIcon, hAboutImage, hAboutStatic1, hAboutStatic2, hAboutInfoLine,
                hAboutStatic3, hVersionValue, hContactLabel, hContactBtn,
                hAboutStatic4, hLicenseBtn, hAboutStatic5, hSourceBtn
            };
            ResizeWindowToFitContent(hwnd, aboutControls, sizeof(aboutControls)/sizeof(HWND));
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi == 0) dpi = 96;
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = ScaleByDPI(200, dpi);
        mmi->ptMinTrackSize.y = ScaleByDPI(100, dpi);
        mmi->ptMaxTrackSize.x = ScaleByDPI(1300, dpi);
        mmi->ptMaxTrackSize.y = ScaleByDPI(900, dpi);
        break;
    }
    case WM_CLOSE: {
        int ret = ShowStyledQuitDialog(hwnd);
        if (ret == IDYES) {
            DestroyWindow(hwnd);
        }
        break;
    }
   case WM_DESTROY:
    ShutdownGDIPlus();
    if (hAccel) DestroyAcceleratorTable(hAccel);
    if (hBgBrush) DeleteObject(hBgBrush);
    if (hFontH2) DeleteObject(hFontH2);
    if (hFontBold) DeleteObject(hFontBold);
    if (hFontHeadline) DeleteObject(hFontHeadline);
    if (hFont9) DeleteObject(hFont9);
    if (hAboutBitmap) DeleteObject(hAboutBitmap);
    if (hFontLabel) DeleteObject(hFontLabel);
    if (hFontValue) DeleteObject(hFontValue);
    if (cpuInfoTimer) KillTimer(hwnd, 2);
    PostQuitMessage(0);
    break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

// --- Main entry point ---
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow){

     // Elevate the whole app if not already elevated
    if (!IsProcessElevated()) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = lpCmdLine; // Pass along any command line args
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            ExitProcess(0); // Exit current process, elevated instance will continue
        } else {
            MessageBoxW(NULL, L"Elevation failed!", L"Error", MB_ICONERROR);
            return 1;
        }
    }

    // SetProcessDPIAware();
    UINT dpi = GetDpiForSystem();
    int winW = ScaleByDPI(350, dpi); // Initial window size set to min
    int winH = ScaleByDPI(200, dpi); // Initial window size set to min

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadAppIcon();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(APP_BG_COLOR); // Use f0f0f0
    wc.lpszMenuName = NULL;
    wc.lpszClassName = APP_CLASS;
    wc.hIconSm = wc.hIcon;

    WNDCLASSW wcQuit = {};
    wcQuit.lpfnWndProc = QuitDialogWndProc;
    wcQuit.hInstance = hInstance;
    wcQuit.lpszClassName = L"QuitDialogClass";
    wcQuit.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wcQuit.style = CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW;
    RegisterClassW(&wcQuit);

    ATOM classAtom = RegisterClassExW(&wc);
    if (!classAtom) {
        DWORD err = GetLastError();
        wchar_t buf[256];
        swprintf(buf, 256, L"RegisterClassExW failed with error %lu", err);
        MessageBoxW(NULL, buf, L"Error", MB_ICONERROR);
        return 1;
    }

    DWORD winStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

        HWND hwnd = CreateWindowExW(
        0,
        APP_CLASS,
        APP_WINDOW_TITLE,
        winStyle,
        100, 100, winW, winH,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create main window.", APP_BASE_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetWindowTextW(hwnd, APP_WINDOW_TITLE);

    // Check for argument to auto-open CPU Info page
    if (wcsstr(lpCmdLine, L"--cpuinfo")) {
        ShowCpuInfoPage(hwnd);
    }

    HACCEL hAccel = CreateAppAccelerators();
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(hAccel);
    return 0;
}
// Provide WinMain wrapper for linkers expecting ANSI entrypoint
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LPWSTR cmd = GetCommandLineW();
    return wWinMain(hInstance, hPrevInstance, cmd, nCmdShow);
}