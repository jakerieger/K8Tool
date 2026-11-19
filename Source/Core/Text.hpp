#pragma once

#include <string>
#include <string_view>

namespace K8 {
    /// @brief UTF-16 to UTF-8, for text that crosses into XenUI or the log.
    [[nodiscard]] std::string ToUtf8(std::wstring_view Text);
    /// @brief UTF-8 to UTF-16, for text that crosses into Win32 or std::filesystem.
    [[nodiscard]] std::wstring FromUtf8(std::string_view Text);

    /// @brief Ordinal, case-insensitive comparison, as NTFS and the registry compare names.
    [[nodiscard]] bool EqualsNoCase(std::wstring_view Left, std::wstring_view Right);
    [[nodiscard]] bool StartsWithNoCase(std::wstring_view Text, std::wstring_view Prefix);

    /// @brief Removes leading and trailing whitespace.
    [[nodiscard]] std::wstring_view Trim(std::wstring_view Text);
    [[nodiscard]] std::string_view Trim(std::string_view Text);
}  // namespace K8
