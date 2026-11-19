#pragma once

#include "Core/Environment.hpp"
#include "Core/Library.hpp"
#include "Core/Progress.hpp"

#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace K8 {
    struct RemovalOptions {
        /// @brief Export each library's registry keys to a .reg file first, and skip the library if that fails.
        bool BackupRegistry = true;
        /// @brief Also delete the library's content folder.
        bool DeleteContent = true;
    };

    /// @brief One thing done, or attempted, during removal.
    struct StepResult {
        std::string Description;
        bool Succeeded = true;
        /// @brief Why it failed, or extra detail such as a backup path.
        std::string Detail;
    };

    struct LibraryReport {
        std::wstring Name;
        std::vector<StepResult> Steps;
        /// @brief Whether every registry key is gone, so the library no longer appears in Kontakt.
        bool Removed = false;

        [[nodiscard]] bool HasFailures() const;
    };

    struct RemovalReport {
        std::vector<LibraryReport> Libraries;
        /// @brief Backing up and deleting komplete.db3, done once for the batch.
        std::vector<StepResult> Database;
        bool Cancelled = false;
    };

    /// @brief Removes each library: registry backup, Service Center XML, cache files, license tokens, content
    /// folder and registry keys, in that order. Then backs up and deletes komplete.db3 once if anything was
    /// removed. Stopping takes effect between steps. Never throws.
    [[nodiscard]] RemovalReport RemoveLibraries(const Environment& Where,
                                                std::span<const Library> Libraries,
                                                const RemovalOptions& Options,
                                                const ProgressSink& Progress,
                                                std::stop_token Stop = {});
}  // namespace K8
