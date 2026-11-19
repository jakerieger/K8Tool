#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace K8 {
    /// @brief A registry key under which Native Instruments lists installed products, one subkey each.
    struct RegistrySource {
        HKEY Root = HKEY_LOCAL_MACHINE;
        std::wstring Path;
    };

    /// @brief Every location K8Tool reads or changes. Default() describes the real machine; tests point it at
    /// scratch folders and an HKCU key so they run without administrator rights or a Kontakt install.
    struct Environment {
        std::vector<RegistrySource> Registry;
        /// @brief Holds <Library>.xml product hints and NativeAccess.xml.
        std::filesystem::path ServiceCenter;
        /// @brief Kontakt's per-library cache files, named K<SNPID>....cache.
        std::filesystem::path LibrariesCache;
        /// @brief Kontakt's library database, rebuilt by Kontakt when missing.
        std::filesystem::path KompleteDatabase;
        /// @brief Native Access license tokens (.jwt).
        std::filesystem::path Ras3;
        /// @brief Where registry and database backups are written.
        std::filesystem::path Backups;

        [[nodiscard]] static Environment Default();
    };

    /// @brief %LOCALAPPDATA%\K8Tool, holding Logs, Backups and Exports. Created on first use.
    [[nodiscard]] std::filesystem::path AppDataDirectory();
    [[nodiscard]] std::filesystem::path LogDirectory();
    [[nodiscard]] std::filesystem::path BackupDirectory();
    [[nodiscard]] std::filesystem::path ExportDirectory();
}  // namespace K8
