#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace K8 {
    /// @brief One registry key describing a library, such as HKLM\SOFTWARE\Native Instruments\<Name>.
    struct RegistryKey {
        HKEY Root = HKEY_LOCAL_MACHINE;
        std::wstring Path;

        bool operator==(const RegistryKey&) const = default;
    };

    /// @brief A Kontakt library found in the registry. A library listed under both the native and the WOW6432Node
    /// key is one entry with two keys, and removing it removes both.
    struct Library {
        std::wstring Name;
        std::vector<RegistryKey> Keys;
        /// @brief The ContentDir value; empty when the registry has none.
        std::filesystem::path ContentDir;
        /// @brief Whether ContentDir is an existing folder. Libraries whose folder is gone are still listed, so
        /// their leftover entries can be removed.
        bool ContentExists = false;
        /// @brief Size of ContentDir on disk, once computed.
        std::optional<std::uintmax_t> Size;

        bool operator==(const Library&) const = default;
    };
}  // namespace K8
