#pragma once

#include "App/AppModel.hpp"
#include "App/TaskRunner.hpp"
#include "Core/Library.hpp"
#include "Core/Removal.hpp"

#include <XenUI/XenUI.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace K8::Ui {
    /// @brief The main window's content: title bar with menus, the library table and the actions on it. Owns the
    /// background work: scanning, size calculation, removal, relocation and update checks.
    struct MainView {
        AppModel* Model = nullptr;

        class State : public XenUI::ComponentState<MainView> {
        protected:
            void Init() override;
            void Dispose() override;
            XenUI::Node Build(XenUI::BuildContext& Context) override;

        private:
            void Rescan();
            void ConfirmRemove();
            void StartRemove(std::vector<Library> Libraries, RemovalOptions Options);
            void ConfirmRelocate();
            void StartRelocate(Library Entry, std::filesystem::path Destination);
            void ExportList();
            void CollectBackupsToZip();
            void ShowAbout();
            void CheckForUpdates(bool Quietly);
            void ShowProgress();
            void ShowMessage(std::string Title, std::string Body);
            void RequestClose();
            bool Finished();

            /// @brief Runs Work on the UI thread later, after the current input and any closing menu, unless this
            /// view is gone by then. Safe from any thread.
            void Post(std::function<void()> Work);

            [[nodiscard]] std::vector<XenUI::Menu> Menus();
            [[nodiscard]] std::vector<XenUI::MenuItem> RowMenu();
            [[nodiscard]] bool Busy() const { return _Task.Running.Peek(); }
            [[nodiscard]] bool CanRelocate(const std::vector<Library>& Selected) const;
            [[nodiscard]] XenUI::Node Header(const XenUI::Theme& Style);
            [[nodiscard]] XenUI::Node Body(const XenUI::Theme& Style);
            [[nodiscard]] XenUI::Node Footer(const XenUI::Theme& Style);
            [[nodiscard]] XenUI::Node UpdateBanner(const XenUI::Theme& Style);

            XenUI::OverlayController* _Overlay = nullptr;
            XenUI::PopupStyle _Style;
            XenUI::OverlayEntry _Dialog;
            XenUI::OverlayEntry _Message;

            XenUI::Signal<bool> _BackupRegistry = true;
            XenUI::Signal<bool> _DeleteContent  = true;
            XenUI::Signal<std::string> _UpdateStatus;
            XenUI::Signal<bool> _CheckingUpdates = false;
            bool _QuitWhenDone                   = false;

            std::optional<XenUI::Window> _LogWindow;
            TaskRunner _Task;
            std::shared_ptr<bool> _Alive = std::make_shared<bool>(true);
            // Declared last, so they are stopped and joined before the state above is destroyed.
            std::jthread _Scanner;
            std::jthread _UpdateCheck;
        };
    };
}  // namespace K8::Ui
