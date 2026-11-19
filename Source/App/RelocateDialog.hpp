#pragma once

#include "Core/Library.hpp"

#include <XenUI/XenUI.hpp>

#include <filesystem>
#include <functional>

namespace K8::Ui {
    /// @brief The body of the relocate dialog: the destination, a folder picker and the buttons. Relocate stays
    /// disabled until the destination is valid, and the reason is shown under the field.
    struct RelocateForm {
        Library Entry;
        std::function<void(const std::filesystem::path& Destination)> OnRelocate;
        std::function<void()> OnCancel;

        class State : public XenUI::ComponentState<RelocateForm> {
        protected:
            XenUI::Node Build(XenUI::BuildContext& Context) override;

        private:
            void Browse();

            XenUI::Signal<std::string> _Destination;
        };
    };
}  // namespace K8::Ui
