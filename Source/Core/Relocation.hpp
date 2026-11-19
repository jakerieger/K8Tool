#pragma once

#include "Core/Library.hpp"
#include "Core/Progress.hpp"

#include <filesystem>
#include <optional>
#include <stop_token>
#include <string>

namespace K8 {
    /// @brief The folder a library would end up in: Destination joined with the content folder's own name.
    [[nodiscard]] std::filesystem::path RelocationTarget(const Library& Entry,
                                                         const std::filesystem::path& Destination);

    /// @brief Why the library cannot be moved into Destination, or nothing when it can.
    [[nodiscard]] std::optional<std::string> ValidateRelocation(const Library& Entry,
                                                                const std::filesystem::path& Destination);

    struct RelocationResult {
        bool Succeeded = false;
        std::filesystem::path NewContentDir;
        /// @brief Why it failed, in UTF-8.
        std::string Error;
        /// @brief A problem that did not stop the move, such as an original folder that could not be deleted.
        std::string Warning;
    };

    /// @brief Moves the content folder into Destination and points ContentDir at the new place. On the same
    /// volume the folder is renamed; otherwise it is copied, checked, and the original deleted only after the
    /// registry is updated. On failure everything is put back as it was. Never throws.
    [[nodiscard]] RelocationResult RelocateLibrary(const Library& Entry,
                                                   const std::filesystem::path& Destination,
                                                   const ProgressSink& Progress,
                                                   std::stop_token Stop = {});
}  // namespace K8
