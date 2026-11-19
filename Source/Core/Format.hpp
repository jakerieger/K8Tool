#pragma once

#include <cstdint>
#include <string>

namespace K8 {
    /// @brief Human-readable size in binary units, such as "12.3 GB".
    [[nodiscard]] std::string FormatSize(std::uintmax_t Bytes);
    /// @brief Local time as yyyyMMdd-HHmmss, safe for file names.
    [[nodiscard]] std::wstring FileTimestamp();
}  // namespace K8
