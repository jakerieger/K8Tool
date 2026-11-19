#pragma once

#include "Core/Environment.hpp"
#include "Core/Library.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <vector>

namespace K8 {
    /// @brief Lists every library under the environment's registry sources, sorted by name. Fast: sizes are left
    /// empty for ComputeSize to fill in.
    [[nodiscard]] std::vector<Library> ScanLibraries(const Environment& Where);

    /// @brief Total size of the files under Directory. Unreadable entries are skipped. Empty when stopped.
    [[nodiscard]] std::optional<std::uintmax_t> ComputeSize(const std::filesystem::path& Directory,
                                                            std::stop_token Stop = {});
}  // namespace K8
