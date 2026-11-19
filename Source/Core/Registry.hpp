#pragma once

#include "Core/Library.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace K8::Registry {
    /// @brief Names of the subkeys of Root\Path, or nothing when the key cannot be opened.
    [[nodiscard]] std::optional<std::vector<std::wstring>> SubkeyNames(HKEY Root, const std::wstring& Path);

    /// @brief A REG_SZ or REG_EXPAND_SZ value of any length, expanded. Empty when missing or of another type.
    [[nodiscard]] std::optional<std::wstring> ReadString(const RegistryKey& Key, const wchar_t* Name);

    bool WriteString(const RegistryKey& Key, const wchar_t* Name, const std::wstring& Value, std::wstring& Error);

    [[nodiscard]] bool Exists(const RegistryKey& Key);

    /// @brief Deletes the key and everything under it. A key that is already gone counts as deleted.
    bool DeleteTree(const RegistryKey& Key, std::wstring& Error);

    /// @brief Writes the key and its subkeys as a Unicode .reg file that Registry Editor or `reg import` can restore.
    bool ExportToFile(const RegistryKey& Key, const std::filesystem::path& File, std::wstring& Error);

    /// @brief "HKEY_LOCAL_MACHINE\Path", as .reg files and Registry Editor show it.
    [[nodiscard]] std::wstring FullName(const RegistryKey& Key);

    /// @brief Products under the Native Instruments key that are instruments, effects or apps rather than
    /// libraries, so they are never listed.
    [[nodiscard]] bool IsExcludedProduct(std::wstring_view Name);

    /// @brief Text of a Win32 error code.
    [[nodiscard]] std::wstring ErrorText(LSTATUS Code);
}  // namespace K8::Registry
