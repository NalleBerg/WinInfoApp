#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <winioctl.h>
#include <windowsx.h>
#include <thread>
#include "include/storagepage.h"

static const wchar_t* STORAGE_PAGE_CLASS = L"WinInfoAppStoragePageClass2025";

static std::wstring FormatBytes(unsigned long long bytes) {
    const unsigned long long KB = 1024ULL;
    const unsigned long long MB = KB * 1024ULL;
    const unsigned long long GB = MB * 1024ULL;
    std::wostringstream ss;
    ss.precision(1);
    ss.setf(std::ios::fixed);
    if (bytes == 0) {
        return std::wstring(L"N/A");
    } else if (bytes >= GB) ss << (double)bytes / (double)GB << L" GB";
    else if (bytes >= MB) ss << (double)bytes / (double)MB << L" MB";
    else if (bytes >= KB) ss << (double)bytes / (double)KB << L" KB";
    else ss << bytes << L" B";
    return ss.str();
}

struct DriveInfo {
    std::wstring name;
    std::wstring type;
    std::wstring fs;
    std::wstring label;
    unsigned long long totalBytes;
    unsigned long long freeBytes;
};

// Enumerate mounted volumes quickly using FindFirstVolume/FindNextVolume
static std::vector<DriveInfo> EnumerateVolumes() {
    std::vector<DriveInfo> res;
    wchar_t volName[MAX_PATH] = {};
    HANDLE h = FindFirstVolumeW(volName, (DWORD)std::size(volName));
    if (h == INVALID_HANDLE_VALUE) return res;
    do {
        // get filesystem and label and sizes
        wchar_t mountPoints[1024] = {};
        DWORD returnLen = 0;
        GetVolumePathNamesForVolumeNameW(volName, mountPoints, (DWORD)std::size(mountPoints), &returnLen);

        wchar_t fsName[64] = {};
        wchar_t volLabel[256] = {};
        DWORD serial = 0, maxCompLen = 0, flags = 0;
        ULARGE_INTEGER freeBytesAvailable = {}, totalNumberOfBytes = {}, totalNumberOfFreeBytes = {};
        // Try to get info for the first mount point if any
        wchar_t firstMount[MAX_PATH] = {};
        if (returnLen > 0) {
            // mountPoints contains null-separated list; copy first
            wcscpy_s(firstMount, mountPoints);
            if (GetVolumeInformationW(firstMount, volLabel, (DWORD)std::size(volLabel), &serial, &maxCompLen, &flags, fsName, (DWORD)std::size(fsName))) {
                GetDiskFreeSpaceExW(firstMount, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes);
            }
        } else {
            // attempt GetVolumeInformation directly on volume name
            GetVolumeInformationW(volName, volLabel, (DWORD)std::size(volLabel), &serial, &maxCompLen, &flags, fsName, (DWORD)std::size(fsName));
            GetDiskFreeSpaceExW(volName, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes);
        }

        DriveInfo di;
        // use mountpoint if available else volume GUID
        if (firstMount[0]) di.name = firstMount; else di.name = volName;
        di.fs = fsName[0] ? fsName : L"";
        di.label = volLabel[0] ? volLabel : L"";
        di.totalBytes = totalNumberOfBytes.QuadPart;
        di.freeBytes = totalNumberOfFreeBytes.QuadPart;
        di.type = L"Volume";
        res.push_back(di);
    } while (FindNextVolumeW(h, volName, (DWORD)std::size(volName)));
    FindVolumeClose(h);
    return res;
}

// Enumerate physical disks and partitions quickly via IOCTL (fast, non-blocking)
static std::vector<DriveInfo> EnumeratePhysicalPartitions() {
    std::vector<DriveInfo> res;
    for (int disk = 0; disk < 16; ++disk) {
        wchar_t path[64];
        swprintf(path, (size_t)_countof(path), L"\\\\.\\PhysicalDrive%d", disk);
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) continue;
        // get geometry
        DISK_GEOMETRY_EX geom = {};
        DWORD bytes = 0;
        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geom, sizeof(geom), &bytes, NULL)) {
            unsigned long long totalBytes = geom.DiskSize.QuadPart;
            // Add disk entry
            DriveInfo di;
            di.name = path;
            di.type = L"Physical Disk";
            di.fs = L"";
            di.label = L"";
            di.totalBytes = totalBytes;
            di.freeBytes = 0;
            res.push_back(di);
        }
        // attempt to get partition layout to detect linux partitions (MBR type 0x83 or GPT GUID)
        BYTE buf[4096] = {};
        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, buf, (DWORD)sizeof(buf), &bytes, NULL)) {
            DRIVE_LAYOUT_INFORMATION_EX* layout = (DRIVE_LAYOUT_INFORMATION_EX*)buf;
            if (layout->PartitionCount > 0) {
                for (DWORD i = 0; i < layout->PartitionCount; ++i) {
                    PARTITION_INFORMATION_EX &p = layout->PartitionEntry[i];
                    DriveInfo pd;
                    // name as diskN:partitionM
                    wchar_t nm[64];
                    swprintf(nm, (size_t)_countof(nm), L"%s\x20part%d", path, (int)i+1);
                    pd.name = nm;
                    pd.totalBytes = p.PartitionLength.QuadPart;
                    pd.freeBytes = 0;
                    pd.fs = L"";
                    // detect common linux indicators
                    if (p.PartitionStyle == PARTITION_STYLE_MBR) {
                        unsigned char t = p.Mbr.PartitionType;
                        if (t == 0x83) pd.type = L"Linux (MBR)"; else pd.type = L"MBR";
                    } else if (p.PartitionStyle == PARTITION_STYLE_GPT) {
                        GUID g = p.Gpt.PartitionType;
                        // Linux filesystem GUID: 0FC63DAF-8483-4772-8E79-3D69D8477DE4
                        GUID linuxGuid = {0x0FC63DAF,0x8483,0x4772,{0x8E,0x79,0x3D,0x69,0xD8,0x47,0x7D,0xE4}};
                        if (memcmp(&g, &linuxGuid, sizeof(GUID)) == 0) pd.type = L"Linux (GPT)"; else pd.type = L"GPT";
                    } else pd.type = L"Unknown";
                    res.push_back(pd);
                }
            }
        }
        CloseHandle(h);
    }
    return res;
}

LRESULT CALLBACK StoragePageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static std::vector<DriveInfo> drives;
    static std::vector<DriveInfo>* pending = nullptr;
    static int scrollPos = 0;
    static int itemHeight = 20;
    static int headerHeight = 28;
    static int totalHeight = 0;
    static bool g_dragging = false;
    static int g_dragStartY = 0;
    static int g_thumbStart = 0;
    switch (msg) {
        case WM_CREATE: {
            // Defer drive enumeration until the page is shown to avoid blocking app startup
            drives.clear();
            pending = nullptr;
            // initialize vertical scroll offset to 0
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            // hide system scrollbar (we draw our own)
            ShowScrollBar(hwnd, SB_VERT, FALSE);
            return 0;
        }
        case WM_SHOWWINDOW: {
            if (wParam) {
                // start a worker thread to enumerate volumes and physical partitions
                if (pending) { delete pending; pending = nullptr; }
                // spawn thread
                CreateThread(NULL, 0, [](LPVOID p)->DWORD {
                    HWND hwndLocal = (HWND)p;
                    std::vector<DriveInfo>* result = new std::vector<DriveInfo>();
                    // gather volumes first
                    std::vector<DriveInfo> v = EnumerateVolumes();
                    result->insert(result->end(), v.begin(), v.end());
                    // gather physical partitions
                    std::vector<DriveInfo> pvec = EnumeratePhysicalPartitions();
                    result->insert(result->end(), pvec.begin(), pvec.end());
                    PostMessageW(hwndLocal, WM_APP+1, 0, (LPARAM)result);
                    return 0;
                }, hwnd, 0, NULL);
            } else {
                // hide/stop
            }
            return 0;
        }
        
        case WM_APP+1: {
            // worker thread finished; lParam is pointer to vector
            std::vector<DriveInfo>* v = (std::vector<DriveInfo>*)lParam;
            if (v) {
                drives = *v;
                delete v;
                InvalidateRect(hwnd, NULL, TRUE);
                // update scroll ranges now that we have data
                SendMessageW(hwnd, WM_SIZE, 0, 0);
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1) {
                // refresh asynchronously
                CreateThread(NULL, 0, [](LPVOID p)->DWORD {
                    HWND hwndLocal = (HWND)p;
                    std::vector<DriveInfo>* result = new std::vector<DriveInfo>();
                    std::vector<DriveInfo> v = EnumerateVolumes();
                    result->insert(result->end(), v.begin(), v.end());
                    std::vector<DriveInfo> pvec = EnumeratePhysicalPartitions();
                    result->insert(result->end(), pvec.begin(), pvec.end());
                    PostMessageW(hwndLocal, WM_APP+1, 0, (LPARAM)result);
                    return 0;
                }, hwnd, 0, NULL);
            }
            break;
        }
        case WM_SIZE: {
            RECT rc; GetClientRect(hwnd, &rc);
            // determine row height from font
            HDC hdc = GetDC(hwnd);
            HFONT hf = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
            if (!hf) hf = (HFONT)SendMessageW(GetParent(hwnd), WM_GETFONT, 0, 0);
            HFONT oldf = (HFONT)SelectObject(hdc, hf);
            TEXTMETRIC tm; GetTextMetrics(hdc, &tm);
            SelectObject(hdc, oldf);
            ReleaseDC(hwnd, hdc);
            int rowH = tm.tmHeight + 8;
            int headerH = rowH + 8;
            int contentH = headerH + (int)drives.size() * rowH + 8;
            int maxScroll = 0;
            if ((rc.bottom - rc.top) < contentH) maxScroll = contentH - (rc.bottom - rc.top);
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE;
            si.nMin = 0; si.nMax = maxScroll; si.nPage = rc.bottom - rc.top;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            ShowScrollBar(hwnd, SB_VERT, FALSE);
            // clamp stored offset
            LONG_PTR cur = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (cur > maxScroll) SetWindowLongPtrW(hwnd, GWLP_USERDATA, maxScroll);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            RECT rc; GetClientRect(hwnd, &rc);
            int sbWidth = 8;
            RECT sbArea = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
            int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
            // compute sizes like in paint
            HDC hdc = GetDC(hwnd);
            HFONT hf = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
            if (!hf) hf = (HFONT)SendMessageW(GetParent(hwnd), WM_GETFONT, 0, 0);
            HFONT oldf = (HFONT)SelectObject(hdc, hf);
            TEXTMETRIC tm; GetTextMetrics(hdc, &tm);
            SelectObject(hdc, oldf);
            ReleaseDC(hwnd, hdc);
            int rowH = tm.tmHeight + 8;
            int headerH = rowH + 8;
            int totalH = headerH + (int)drives.size() * rowH + 8;
            int clientH = rc.bottom - rc.top;
            int maxScroll = 0; if (clientH < totalH) maxScroll = totalH - clientH;
            if (mx >= sbArea.left && mx <= sbArea.right) {
                if (maxScroll <= 0) break;
                int trackH = sbArea.bottom - sbArea.top - 2;
                float pageRatio = (float)clientH / (float)totalH;
                int thumbH = std::max(20, (int)(pageRatio * trackH));
                int thumbRange = trackH - thumbH;
                int thumbTop = sbArea.top + 1 + (int)((float)GetWindowLongPtrW(hwnd, GWLP_USERDATA) / maxScroll * thumbRange);
                RECT thumb = { sbArea.left + 1, thumbTop, sbArea.right - 1, thumbTop + thumbH };
                if (my >= thumb.top && my <= thumb.bottom) {
                    g_dragging = true; g_dragStartY = my; g_thumbStart = thumbTop; SetCapture(hwnd);
                } else {
                    if (my < thumb.top) {
                        int newPos = std::max(0, (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA) - clientH);
                        SetWindowLongPtrW(hwnd, GWLP_USERDATA, newPos);
                    } else {
                        int newPos = std::min(maxScroll, (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA) + clientH);
                        SetWindowLongPtrW(hwnd, GWLP_USERDATA, newPos);
                    }
                    SCROLLINFO si2 = {}; si2.cbSize = sizeof(si2); si2.fMask = SIF_POS; si2.nPos = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA); SetScrollInfo(hwnd, SB_VERT, &si2, TRUE);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (g_dragging) {
                int mx = GET_X_LPARAM(lParam), my = GET_Y_LPARAM(lParam);
                RECT rc; GetClientRect(hwnd, &rc);
                // recompute sizes
                HDC hdc = GetDC(hwnd);
                HFONT hf = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
                if (!hf) hf = (HFONT)SendMessageW(GetParent(hwnd), WM_GETFONT, 0, 0);
                HFONT oldf = (HFONT)SelectObject(hdc, hf);
                TEXTMETRIC tm; GetTextMetrics(hdc, &tm);
                SelectObject(hdc, oldf);
                ReleaseDC(hwnd, hdc);
                int rowH = tm.tmHeight + 8;
                int headerH = rowH + 8;
                int totalH = headerH + (int)drives.size() * rowH + 8;
                int clientH = rc.bottom - rc.top;
                int maxScroll = 0; if (clientH < totalH) maxScroll = totalH - clientH;
                int sbWidth = 8; RECT sbArea = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
                int trackH = sbArea.bottom - sbArea.top - 2;
                float pageRatio = (float)clientH / (float)totalH;
                int thumbH = std::max(20, (int)(pageRatio * trackH));
                int thumbRange = trackH - thumbH;
                int dy = my - g_dragStartY;
                int newThumbTop = g_thumbStart + dy;
                int sbTop = sbArea.top + 1;
                newThumbTop = std::max(sbTop, std::min(sbTop + thumbRange, newThumbTop));
                if (thumbRange > 0) {
                    float t = (float)(newThumbTop - sbTop) / (float)thumbRange;
                    int newPos = (int)(t * maxScroll);
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, newPos);
                    SCROLLINFO si2 = {}; si2.cbSize = sizeof(si2); si2.fMask = SIF_POS; si2.nPos = newPos; SetScrollInfo(hwnd, SB_VERT, &si2, TRUE);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (g_dragging) { g_dragging = false; ReleaseCapture(); return 0; }
            break;
        }
        case WM_VSCROLL: {
            int code = (int)LOWORD(wParam);
            int thumb = (int)HIWORD(wParam);
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int cur = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            int line = 24;
            switch (code) {
                case SB_LINEUP: cur = std::max(0, cur - line); break;
                case SB_LINEDOWN: cur = std::min((int)si.nMax, cur + line); break;
                case SB_PAGEUP: cur = std::max(0, cur - (int)si.nPage); break;
                case SB_PAGEDOWN: cur = std::min((int)si.nMax, cur + (int)si.nPage); break;
                case SB_THUMBTRACK: cur = thumb; break;
            }
            si.fMask = SIF_POS; si.nPos = cur; SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, cur);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int lines = (delta / WHEEL_DELTA) * 3;
            SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL; GetScrollInfo(hwnd, SB_VERT, &si);
            int cur = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            cur = std::max(0, std::min((int)si.nMax, cur - lines * 24));
            si.fMask = SIF_POS; si.nPos = cur; SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, cur);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc; GetClientRect(hwnd, &rc);
            static HBRUSH s_br = CreateSolidBrush(RGB(38,52,68));
            FillRect(hdc, &rc, s_br);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc; GetClientRect(hwnd, &rc);
            // Prefer the font assigned to this child; fallback to parent's font
            HFONT hFont = (HFONT)SendMessageW(hwnd, WM_GETFONT, 0, 0);
            if (!hFont) hFont = (HFONT)SendMessageW(GetParent(hwnd), WM_GETFONT, 0, 0);
            HFONT oldf = (HFONT)SelectObject(hdc, hFont);
            SetBkMode(hdc, TRANSPARENT);

            // compute sizes
            TEXTMETRIC tm; GetTextMetrics(hdc, &tm);
            int rowH = tm.tmHeight + 8;
            int headerH = rowH + 8;

            // apply vertical scroll offset via viewport
            int scrollY = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (scrollY) SetViewportOrgEx(hdc, 0, -scrollY, NULL);

            // draw header background
            RECT hdr = { 0, 0, rc.right - rc.left, headerH };
            HBRUSH hHdr = CreateSolidBrush(RGB(33,48,62));
            FillRect(hdc, &hdr, hHdr);
            DeleteObject(hHdr);

            // dynamic column layout
            int cw = rc.right - rc.left;
            int colDrive = 8;
            int colType = colDrive + 100;
            int colFs = colType + 120;
            int widthFree = 120; int widthTotal = 120;
            int colFree = cw - 8 - widthFree;
            int colTotal = colFree - widthTotal;
            int colLabel = colFs + 120;
            if (colLabel + 80 > colTotal) colLabel = colFs + 40;

            // draw headers
            SetTextColor(hdc, RGB(200,220,240));
            RECT rtmp;
            rtmp = { colDrive, 6, colType-4, 6 + rowH };
            DrawTextW(hdc, L"Drive", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            rtmp = { colType, 6, colFs-4, 6 + rowH };
            DrawTextW(hdc, L"Type", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            rtmp = { colFs, 6, colLabel-4, 6 + rowH };
            DrawTextW(hdc, L"FS", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            rtmp = { colLabel, 6, colTotal-4, 6 + rowH };
            DrawTextW(hdc, L"Label", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            rtmp = { colTotal, 6, colFree-4, 6 + rowH };
            DrawTextW(hdc, L"Total", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_RIGHT);
            rtmp = { colFree, 6, cw - 8, 6 + rowH };
            DrawTextW(hdc, L"Free", -1, &rtmp, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_RIGHT);

            // draw rows
            SetTextColor(hdc, RGB(220,230,240));
            int y = headerH + 4;
            for (size_t i = 0; i < drives.size(); ++i) {
                const DriveInfo &d = drives[i];
                // alternating row bg
                if (i % 2 == 1) {
                    RECT rr = { 0, y - 2, cw, y + rowH };
                    HBRUSH rb = CreateSolidBrush(RGB(40,56,68));
                    FillRect(hdc, &rr, rb);
                    DeleteObject(rb);
                }
                RECT rcDrive = { colDrive, y, colType-4, y + rowH };
                RECT rcType = { colType, y, colFs-4, y + rowH };
                RECT rcFs = { colFs, y, colLabel-4, y + rowH };
                RECT rcLabel = { colLabel, y, colTotal-4, y + rowH };
                RECT rcTotal = { colTotal, y, colFree-4, y + rowH };
                RECT rcFree = { colFree, y, cw - 8, y + rowH };

                DrawTextW(hdc, d.name.c_str(), -1, &rcDrive, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                DrawTextW(hdc, d.type.c_str(), -1, &rcType, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                DrawTextW(hdc, d.fs.c_str(), -1, &rcFs, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                DrawTextW(hdc, d.label.c_str(), -1, &rcLabel, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                std::wstring total = FormatBytes(d.totalBytes);
                std::wstring free = FormatBytes(d.freeBytes);
                DrawTextW(hdc, total.c_str(), -1, &rcTotal, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_RIGHT);
                DrawTextW(hdc, free.c_str(), -1, &rcFree, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_RIGHT);

                y += rowH;
            }

            // cleanup
            if (scrollY) SetViewportOrgEx(hdc, 0, 0, NULL);
            SelectObject(hdc, oldf);

            // draw custom narrow scrollbar at right edge
            {
                int sbWidth = 8;
                RECT track = { rc.right - sbWidth - 6, rc.top + 6, rc.right - 6, rc.bottom - 6 };
                RECT inner = track; InflateRect(&inner, -1, -1);
                static HBRUSH hTrackBrush = CreateSolidBrush(RGB(38,52,68));
                static HBRUSH hThumbBrush = CreateSolidBrush(RGB(110,170,90));
                FillRect(hdc, &inner, hTrackBrush);
                // compute content height and scroll range
                int contentH = headerH + (int)drives.size() * rowH + 8;
                int clientH = rc.bottom - rc.top;
                int maxScroll = std::max(0, contentH - clientH);
                if (maxScroll > 0) {
                    int trackH = inner.bottom - inner.top;
                    float pageRatio = (float)clientH / (float)contentH;
                    int thumbH = std::max(20, (int)(pageRatio * trackH));
                    int thumbRange = trackH - thumbH;
                    int thumbTop = inner.top + (int)((float)GetWindowLongPtrW(hwnd, GWLP_USERDATA) / maxScroll * thumbRange);
                    RECT thumb = { inner.left + 1, thumbTop, inner.right - 1, thumbTop + thumbH };
                    FillRect(hdc, &thumb, hThumbBrush);
                }
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void RegisterStoragePageClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = StoragePageProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = STORAGE_PAGE_CLASS;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
}

HWND CreateStoragePage(HWND parent, HINSTANCE hInst) {
    RegisterStoragePageClass(hInst);
    RECT rc; GetClientRect(parent, &rc);
    HWND h = CreateWindowExW(0, STORAGE_PAGE_CLASS, L"",
        WS_CHILD,
        10, 10, rc.right - rc.left - 20, rc.bottom - rc.top - 20,
        parent, NULL, hInst, NULL);
    return h;
}
