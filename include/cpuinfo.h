#pragma once
#include <string>
#include <vector>

// Full CPU info text
std::wstring GetCpuInfoText();

// CPUInfo string
struct CpuInfoRow {
    std::wstring label;
    std::wstring value;
    bool isDynamic;
};

std::vector<CpuInfoRow> getCpuInfoRows();
std::wstring getCpuInfoText();

void EnsureElevated();

void ShowCpuInfoWindow(HWND hParent = NULL);

// Individual info functions (optional)
std::wstring getProcessorName();
std::wstring getProcessorIdentifier();
int getPhysicalCores();
int getTotalCores();
int getMaxFrequencyMHz();
int getCurrentFrequencyMHz();
int getCpuUsagePercent();
int getCpuTemperature();

bool IsProcessElevated();
