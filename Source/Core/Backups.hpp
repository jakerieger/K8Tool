#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace K8 {
    struct BackupArchive {
        std::filesystem::path File;
        size_t Entries = 0;
    };

    /// @brief Zips every registry (.reg) and database (.bak) backup in Backups into Archive.
    /// Fails, with Error set, when there is nothing to collect or the archive cannot be written.
    [[nodiscard]] std::optional<BackupArchive>
    CollectBackups(const std::filesystem::path& Backups, const std::filesystem::path& Archive, std::string& Error);
}  // namespace K8
