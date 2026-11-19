#pragma once

#include <XenUI/XenUI.hpp>

#include <functional>
#include <string>

namespace K8::Ui {
    /// @brief The body of the About dialog: name, version, project link and the update check.
    struct AboutView {
        std::string Version;
        std::function<void()> OnCheckForUpdates;
        /// @brief The last update check's outcome, shown under the button.
        XenUI::Signal<std::string>* UpdateStatus = nullptr;
        XenUI::Signal<bool>* Checking            = nullptr;

        [[nodiscard]] XenUI::Node Build(XenUI::BuildContext& Context) const;
    };
}  // namespace K8::Ui
