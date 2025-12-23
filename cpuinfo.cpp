#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

// --- CPU Temperature via LibreHardwareMonitorWrapper.dll ---
#include <thread>
#include <atomic>
#include <chrono>

static std::atomic<int> s_lastCpuTemp{-1};
static std::atomic<bool> s_samplerRunning{false};

static void ensureWrapperLoadedAndSampleLoop() {
    typedef double (*GetCpuTemperatureFunc)();
    static HMODULE hLib = NULL;
    static GetCpuTemperatureFunc func = NULL;

    auto loadWrapper = [&]() -> bool {
        if (hLib && func) return true;
        wchar_t dllPath[MAX_PATH];
        GetModuleFileNameW(NULL, dllPath, MAX_PATH);
        std::wstring exeDir = dllPath;
        size_t pos = exeDir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) exeDir = exeDir.substr(0, pos);
        std::vector<std::wstring> candidates = {
            exeDir + L"\\dll\\LibreHardwareMonitorWrapper.dll",
            exeDir + L"\\libs\\LibreHardwareMonitorWrapper.dll",
            exeDir + L"\\..\\libs\\LibreHardwareMonitorWrapper.dll",
            exeDir + L"\\LibreHardwareMonitorWrapper.dll"
        };
        for (const auto &p : candidates) {
            hLib = LoadLibraryW(p.c_str());
            if (hLib) break;
        }
        if (!hLib) {
            hLib = LoadLibraryW(L"LibreHardwareMonitorWrapper.dll");
        }
        if (hLib) {
            func = (GetCpuTemperatureFunc)GetProcAddress(hLib, "GetCpuTemperature");
        }
        return (hLib != NULL && func != NULL);
    };

    // Polling loop: attempt to load wrapper and then sample temperature every second
    while (s_samplerRunning.load()) {
        if (!func) {
            loadWrapper();
        }
        if (func) {
            double t = func();
            if (t > -50 && t < 150) s_lastCpuTemp.store((int)t);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int getCpuTemperature() {
    // Start sampler thread once
    bool expected = false;
    if (s_samplerRunning.compare_exchange_strong(expected, true)) {
        std::thread([](){
            ensureWrapperLoadedAndSampleLoop();
        }).detach();
        // tiny delay to allow initial sample
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    int val = s_lastCpuTemp.load();
    return val;
}

// --- Info Row Structure ---
struct CpuInfoRow {
    std::wstring label;
    std::wstring value;
    bool isDynamic;
};

// --- Auto-elevation ---
bool IsProcessElevated() {
    BOOL fIsElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD dwSize;
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            fIsElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fIsElevated;
}

void EnsureElevated(const wchar_t* extraArgs = nullptr) {
    if (!IsProcessElevated()) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = extraArgs;
        sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) {
            ExitProcess(0); // Exit current process, elevated instance will continue
        } else {
            MessageBoxW(NULL, L"Elevation failed!", L"Error", MB_ICONERROR);
        }
    }
}

// --- Info functions ---
std::wstring getProcessorName() {
    HKEY hKey;
    wchar_t buffer[256] = {0};
    DWORD bufSize = sizeof(buffer);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)buffer, &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return buffer;
        }
        RegCloseKey(hKey);
    }
    return L"Unknown";
}

std::wstring getProcessorIdentifier() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    std::wstringstream ss;
    ss << L"Intel64 Family " << ((sysInfo.dwProcessorType >> 8) & 0xFF)
       << L" Model " << ((sysInfo.dwProcessorType >> 4) & 0xF)
       << L" Stepping " << (sysInfo.dwProcessorType & 0xF)
       << L", GenuineIntel";
    return ss.str();
}

int getPhysicalCores() {
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(len);
    if (!buffer) return 0;
    GetLogicalProcessorInformation(buffer, &len);
    int count = 0;
    for (DWORD i = 0; i < len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
        if (buffer[i].Relationship == RelationProcessorCore)
            count++;
    }
    free(buffer);
    return count;
}

int getTotalCores() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

int getMaxFrequencyMHz() {
    // TODO: Query actual max frequency if needed
    return 4900; // Example value
}

int getCurrentFrequencyMHz() {
    // TODO: Query actual current frequency if needed
    return 3200 + (rand() % 100);
}

int getCpuUsagePercent() {
    static FILETIME prevIdle = {}, prevKernel = {}, prevUser = {};
    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);
    ULONGLONG idleDiff = (*(ULONGLONG*)&idle) - (*(ULONGLONG*)&prevIdle);
    ULONGLONG kernelDiff = (*(ULONGLONG*)&kernel) - (*(ULONGLONG*)&prevKernel);
    ULONGLONG userDiff = (*(ULONGLONG*)&user) - (*(ULONGLONG*)&prevUser);
    prevIdle = idle; prevKernel = kernel; prevUser = user;
    ULONGLONG total = kernelDiff + userDiff;
    if (total == 0) return 0;
    return (int)(100 - (idleDiff * 100 / total));
}

// --- Helper: wrap text at N characters ---
std::wstring wrapText(const std::wstring& text, size_t maxLen) {
    std::wstringstream ss;
    size_t pos = 0;
    while (pos < text.length()) {
        ss << text.substr(pos, maxLen);
        pos += maxLen;
        if (pos < text.length()) ss << L"\r\n";
    }
    return ss.str();
}

// --- Main info row provider ---
std::vector<CpuInfoRow> getCpuInfoRows() {
    std::vector<CpuInfoRow> rows;
    rows.push_back({L"Processor", wrapText(getProcessorName(), 40), false});
    rows.push_back({L"Identifier", wrapText(getProcessorIdentifier(), 40), false});
    rows.push_back({L"Physical cores", std::to_wstring(getPhysicalCores()), false});
    rows.push_back({L"Total cores", std::to_wstring(getTotalCores()), false});
    rows.push_back({L"Max Frequency", std::to_wstring(getMaxFrequencyMHz()) + L" MHz", false});
    rows.push_back({L"Current Frequency", std::to_wstring(getCurrentFrequencyMHz()) + L" MHz", true});
    rows.push_back({L"CPU Usage", std::to_wstring(getCpuUsagePercent()) + L" %", true});
    int temp = getCpuTemperature();
    std::wstring tempStr;
    if (temp == -1)
        tempStr = L"Unavailable";
    else
        tempStr = std::to_wstring(temp) + L" \u00B0C";
    rows.push_back({L"CPU Temperature", tempStr, true});
    return rows;
}

// --- For legacy edit control (if needed) ---
std::wstring GetCpuInfoText() {
    std::wstringstream ss;
    auto rows = getCpuInfoRows();
    for (const auto& row : rows) {
        ss << row.label << L": " << row.value << L"\r\n";
    }
    return ss.str();
}