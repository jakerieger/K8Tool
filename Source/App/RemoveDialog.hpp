#pragma once

#include <XenUI/XenUI.hpp>

#include <string>
#include <vector>

namespace K8::Ui {
    /// @brief The body of the removal confirmation: what will be removed, and the backup and content options.
    struct RemoveSummary {
        /// @brief Library names, in UTF-8.
        std::vector<std::string> Names;
        /// @brief Whether any of the libraries has a content folder that could be deleted.
        bool AnyContent                     = false;
        XenUI::Signal<bool>* BackupRegistry = nullptr;
        XenUI::Signal<bool>* DeleteContent  = nullptr;

        [[nodiscard]] XenUI::Node Build(XenUI::BuildContext& Context) const;
    };
}  // namespace K8::Ui
