#pragma once

#include "App/AppModel.hpp"

#include <XenUI/XenUI.hpp>

#include <optional>

namespace K8::Ui {
    /// @brief Shows the log window, creating it the first time and after it was closed.
    void ShowLogWindow(std::optional<XenUI::Window>& Slot, AppModel* Model);
}  // namespace K8::Ui
