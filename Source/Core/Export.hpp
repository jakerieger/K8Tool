#pragma once

#include "Core/Library.hpp"

#include <filesystem>
#include <span>
#include <string>

namespace K8 {
    /// @brief The library list as CSV (RFC 4180): Name, Content Directory, Status, Size, Size (bytes).
    [[nodiscard]] std::string LibrariesToCsv(std::span<const Library> Libraries);

    /// @brief Writes LibrariesToCsv to File as UTF-8 with a byte-order mark, so Excel reads accents correctly.
    bool ExportLibraries(std::span<const Library> Libraries, const std::filesystem::path& File, std::string& Error);
}  // namespace K8
