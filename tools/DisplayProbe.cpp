// Read-only display inventory. This tool never calls SetDisplayConfig or changes modes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <set>
#include <vector>

int main() {
    UINT32 path_count{}, mode_count{};
    LONG result{};
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    do {
        result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
        if (result != ERROR_SUCCESS) { break; }
        paths.resize(path_count);
        modes.resize(mode_count);
        result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(),
                                    &mode_count, modes.data(), nullptr);
    } while (result == ERROR_INSUFFICIENT_BUFFER);
    if (result != ERROR_SUCCESS) {
        std::cerr << "Display query failed: " << result << '\n';
        return 1;
    }
    paths.resize(path_count);
    modes.resize(mode_count);
    for (const auto& path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        result = DisplayConfigGetDeviceInfo(&source.header);
        if (result != ERROR_SUCCESS) { return 1; }
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size = sizeof(target);
        target.header.adapterId = path.targetInfo.adapterId;
        target.header.id = path.targetInfo.id;
        result = DisplayConfigGetDeviceInfo(&target.header);
        if (result != ERROR_SUCCESS) { return 1; }
        DISPLAYCONFIG_ADAPTER_NAME adapter{};
        adapter.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADAPTER_NAME;
        adapter.header.size = sizeof(adapter);
        adapter.header.adapterId = path.sourceInfo.adapterId;
        result = DisplayConfigGetDeviceInfo(&adapter.header);
        if (result != ERROR_SUCCESS) { return 1; }
        const auto& refresh = path.targetInfo.refreshRate;
        std::wcout << source.viewGdiDeviceName << L" monitor=" << target.monitorFriendlyDeviceName
                   << L" source_id=" << path.sourceInfo.id << L" target_id=" << path.targetInfo.id
                   << L" adapter_luid=" << std::hex
                   << static_cast<UINT32>(path.sourceInfo.adapterId.HighPart) << L':'
                   << path.sourceInfo.adapterId.LowPart << std::dec;
        if (path.sourceInfo.modeInfoIdx < modes.size() &&
            modes[path.sourceInfo.modeInfoIdx].infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE) {
            const auto& mode = modes[path.sourceInfo.modeInfoIdx].sourceMode;
            std::wcout << L" current=" << mode.width << L'x' << mode.height
                       << L" position=" << mode.position.x << L',' << mode.position.y;
        }
        std::wcout << L" refresh=" << refresh.Numerator << L'/' << refresh.Denominator;
        if (refresh.Denominator) {
            std::wcout << L" (" << static_cast<double>(refresh.Numerator) / refresh.Denominator << L" Hz)";
        }
        std::set<DWORD> rates;
        for (DWORD index = 0;; ++index) {
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            if (!EnumDisplaySettingsW(source.viewGdiDeviceName, index, &mode)) { break; }
            if (mode.dmPelsWidth == 2560 && mode.dmPelsHeight == 1440 && mode.dmBitsPerPel == 32) {
                rates.insert(mode.dmDisplayFrequency);
            }
        }
        std::wcout << L" available_2560x1440_Hz=";
        for (const auto rate : rates) { std::wcout << rate << L' '; }
        std::wcout << L"\n  adapter_path=" << adapter.adapterDevicePath
                   << L"\n  monitor_path=" << target.monitorDevicePath << L'\n';
    }
    return 0;
}
