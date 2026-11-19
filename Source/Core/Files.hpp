#pragma once

#include "Core/Progress.hpp"

#include <filesystem>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace K8::Files {
    /// @brief Kontakt cache files for a product: names that start with "K" followed by the SNPID.
    [[nodiscard]] bool IsCacheFileFor(const std::filesystem::path& File, std::wstring_view Snpid);

    /// @brief License tokens for a product: .jwt files whose name has the SNPID as a whole part, delimited by
    /// anything other than a letter or digit. "K23.jwt" and "K23_user.jwt" match "K23"; "AK231.jwt" does not.
    [[nodiscard]] bool IsLicenseFileFor(const std::filesystem::path& File, std::wstring_view Snpid);

    /// @brief Regular files directly inside Directory that Match accepts. Empty when the folder is missing.
    template<class Predicate>
    [[nodiscard]] std::vector<std::filesystem::path> FilesIn(const std::filesystem::path& Directory, Predicate Match) {
        std::vector<std::filesystem::path> Found;
        std::error_code Error;
        for (std::filesystem::directory_iterator It(Directory, Error), End; !Error && It != End; It.increment(Error)) {
            std::error_code EntryError;
            if (It->is_regular_file(EntryError) && Match(It->path())) {
                Found.push_back(It->path());
            }
        }
        return Found;
    }

    /// @brief Deletes a file or a whole folder. Something already missing counts as deleted.
    bool Delete(const std::filesystem::path& Target, std::wstring& Error);

    /// @brief Whether Inner is Outer or somewhere inside it, comparing resolved paths without regard to case.
    [[nodiscard]] bool IsSameOrInside(const std::filesystem::path& Inner, const std::filesystem::path& Outer);

    [[nodiscard]] bool OnSameVolume(const std::filesystem::path& Left, const std::filesystem::path& Right);

    /// @brief Copies a folder tree to Destination, which must not exist, reporting progress by bytes. Checks the
    /// copy holds as many files and bytes as the source. Removes the partial copy on failure or when stopped.
    bool CopyTree(const std::filesystem::path& Source,
                  const std::filesystem::path& Destination,
                  const ProgressSink& Progress,
                  std::stop_token Stop,
                  std::wstring& Error);
}  // namespace K8::Files
