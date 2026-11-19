#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace K8 {
    struct Version {
        int Major = 0;
        int Minor = 0;
        int Patch = 0;

        /// @brief Reads "1.2.3" or "v1.2.3"; a missing minor or patch number counts as 0.
        [[nodiscard]] static std::optional<Version> Parse(std::string_view Text);
        [[nodiscard]] std::string ToString() const;

        auto operator<=>(const Version&) const = default;
    };

    struct UpdateInfo {
        Version Latest;
        /// @brief The release's tag as published, such as "v3.1.0".
        std::string Tag;
        /// @brief The release page.
        std::string Url;
    };

    /// @brief The value of "tag_name" in a GitHub release JSON document.
    [[nodiscard]] std::optional<std::string> ParseReleaseTag(std::string_view Json);

    /// @brief Asks GitHub for the latest release. Blocks; call it from a worker thread. Empty, with Error set,
    /// when offline or the response cannot be read.
    [[nodiscard]] std::optional<UpdateInfo> FetchLatestRelease(std::string& Error);
}  // namespace K8
