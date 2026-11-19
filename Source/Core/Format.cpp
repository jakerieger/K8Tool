#include "Core/Format.hpp"

#include <array>
#include <chrono>
#include <format>

namespace K8 {
    std::string FormatSize(std::uintmax_t Bytes) {
        constexpr std::array kUnits = {"B", "KB", "MB", "GB", "TB", "PB"};
        if (Bytes < 1024) {
            return std::format("{} B", Bytes);
        }
        auto Value  = static_cast<double>(Bytes);
        size_t Unit = 0;
        while (Value >= 1024.0 && Unit + 1 < kUnits.size()) {
            Value /= 1024.0;
            ++Unit;
        }
        return std::format("{:.1f} {}", Value, kUnits[Unit]);
    }

    std::wstring FileTimestamp() {
        const auto Now =
          std::chrono::zoned_time(std::chrono::current_zone(),
                                  std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
        return std::format(L"{:%Y%m%d-%H%M%S}", Now);
    }
}  // namespace K8
