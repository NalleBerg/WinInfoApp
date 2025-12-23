#include <windows.h>
#include <iostream>

typedef double (*GetCpuTemperatureFunc)();

int main(int argc, char** argv) {
    const char* path = "libs\\LibreHardwareMonitorWrapper.dll";
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        std::cout << "LoadLibraryW failed, GetLastError=" << GetLastError() << "\n";
        return 1;
    }
    GetCpuTemperatureFunc f = (GetCpuTemperatureFunc)GetProcAddress(h, "GetCpuTemperature");
    if (!f) {
        std::cout << "GetProcAddress failed\n";
        FreeLibrary(h);
        return 2;
    }
    double t = 0.0;
    t = f();
    std::cout << "GetCpuTemperature returned: " << t << "\n";
    FreeLibrary(h);
    return 0;
}
