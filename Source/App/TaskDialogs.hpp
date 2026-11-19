#pragma once

#include "App/TaskRunner.hpp"
#include "Core/Removal.hpp"

#include <XenUI/XenUI.hpp>

namespace K8::Ui {
    /// @brief The body of the progress dialog shown while a task runs.
    struct TaskProgressView {
        TaskRunner* Runner = nullptr;

        [[nodiscard]] XenUI::Node Build(XenUI::BuildContext& Context) const;
    };

    /// @brief What a removal did, library by library, with the steps that failed.
    struct RemovalReportView {
        RemovalReport Report;

        [[nodiscard]] XenUI::Node Build(XenUI::BuildContext& Context) const;
    };

    /// @brief A title for the dialog that shows Report.
    [[nodiscard]] std::string RemovalTitle(const RemovalReport& Report);
}  // namespace K8::Ui
