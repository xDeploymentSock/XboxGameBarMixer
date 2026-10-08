#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <winrt/Windows.Storage.h>

namespace fuser::widget {
// Local development evidence only. Never log credentials, PINs or stream URLs.
inline void log(std::wstring_view message) noexcept {
    try {
        static std::mutex lock;
        const std::lock_guard guard{lock};
        const auto folder =
            winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
        const auto path = std::filesystem::path{folder.c_str()} / L"runtime.log";
        const bool truncate =
            std::filesystem::exists(path) && std::filesystem::file_size(path) > 256 * 1024;
        std::ofstream output{path, truncate ? std::ios::trunc : std::ios::app};
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
        output << now << ' ' << winrt::to_string(message) << '\n';
    } catch (...) {
        // Logging must not change application behavior when storage is unavailable.
    }
}
}
