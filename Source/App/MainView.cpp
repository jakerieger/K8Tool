#include "App/MainView.hpp"
#include "App/AboutDialog.hpp"
#include "App/LogWindow.hpp"
#include "App/RelocateDialog.hpp"
#include "App/RemoveDialog.hpp"
#include "App/TaskDialogs.hpp"
#include "Core/Backups.hpp"
#include "Core/Constants.hpp"
#include "Core/Environment.hpp"
#include "Core/Export.hpp"
#include "Core/Format.hpp"
#include "Core/Log.hpp"
#include "Core/Relocation.hpp"
#include "Core/Scanner.hpp"
#include "Core/Text.hpp"
#include "Core/Update.hpp"

#include "K8ToolVersion.h"

#include <algorithm>
#include <format>
#include <numeric>

using namespace XenUI;

namespace K8::Ui {
    namespace {
        constexpr Version kCurrentVersion {K8TOOL_VERSION_MAJOR, K8TOOL_VERSION_MINOR, K8TOOL_VERSION_PATCH};

        struct CollectResult {
            std::optional<BackupArchive> Archive;
            std::string Error;
        };
    }  // namespace

    void MainView::State::Post(std::function<void()> Work) {
        App::Post([Alive = std::weak_ptr(_Alive), Work = std::move(Work)] {
            if (Alive.lock()) {
                Work();
            }
        });
    }

    void MainView::State::Init() {
        AppModel* Shared = GetWidget().Model;
        Shared->MainWindow->SetShortcuts({
          Shortcut {.Keys = {KeyModifiers::None + KeyCode::F5}, .Action = [this] { Rescan(); }},
          Shortcut {.Keys = {KeyModifiers::Control + KeyCode::E}, .Action = [this] { Post([this] { ExportList(); }); }},
          Shortcut {.Keys   = {KeyModifiers::Control + KeyCode::L},
                    .Action = [this] { ShowLogWindow(_LogWindow, GetWidget().Model); }},
          Shortcut {.Keys = {KeyModifiers::None + KeyCode::Delete}, .Action = [this] { ConfirmRemove(); }},
        });
        Shared->MainWindow->SetOnCloseRequested([this] {
            if (!Busy()) {
                return true;
            }
            RequestClose();
            return false;
        });
        Rescan();
        CheckForUpdates(true);
    }

    void MainView::State::Dispose() {
        _Alive.reset();
        _Scanner     = {};
        _UpdateCheck = {};
    }

    // ---- Scanning ---------------------------------------------------------------------------------------------

    void MainView::State::Rescan() {
        if (Busy()) {
            return;
        }
        AppModel* Shared = GetWidget().Model;
        _Scanner         = {};
        Shared->Scanning = true;
        _Scanner         = std::jthread([this, Where = Shared->Where](std::stop_token Stop) {
            std::vector<Library> Found = ScanLibraries(Where);
            Post([this, Found] {
                AppModel* Current = GetWidget().Model;
                Current->SetLibraries(Found);
                Current->Scanning = false;
            });
            // Sizes take a while for large libraries, so they fill in one by one after the list appears.
            for (const Library& Entry : Found) {
                if (!Entry.ContentExists) {
                    continue;
                }
                const std::optional<std::uintmax_t> Size = ComputeSize(Entry.ContentDir, Stop);
                if (!Size) {
                    return;
                }
                Post([this, Name = Entry.Name, Bytes = *Size] { GetWidget().Model->SetSize(Name, Bytes); });
            }
        });
    }

    // ---- Removal ----------------------------------------------------------------------------------------------

    void MainView::State::ConfirmRemove() {
        if (Busy() || _Overlay == nullptr || _Dialog.IsShown()) {
            return;
        }
        std::vector<Library> Selected = GetWidget().Model->SelectedLibraries();
        if (Selected.empty()) {
            return;
        }

        std::vector<std::string> Names;
        bool AnyContent = false;
        for (const Library& Entry : Selected) {
            Names.push_back(ToUtf8(Entry.Name));
            AnyContent |= Entry.ContentExists;
        }
        const std::string Title =
          Selected.size() == 1 ? std::string("Remove library?") : std::format("Remove {} libraries?", Selected.size());
        _Dialog =
          ShowDialog(*_Overlay,
                     _Style,
                     Dialog {
                       .Title   = Title,
                       .Content = RemoveSummary {.Names          = std::move(Names),
                                                 .AnyContent     = AnyContent,
                                                 .BackupRegistry = &_BackupRegistry,
                                                 .DeleteContent  = &_DeleteContent},
                       .Actions =
                         {
                           DialogAction {.Label = "Cancel", .OnPressed = [this] { _Dialog.Reset(); }},
                           DialogAction {.Label = "Remove",
                                         .OnPressed =
                                           [this, Selected] {
                                               _Dialog.Reset();
                                               const RemovalOptions Options {
                                                 .BackupRegistry = _BackupRegistry.Peek(),
                                                 .DeleteContent  = _DeleteContent.Peek(),
                                               };
                                               Post([this, Selected, Options] { StartRemove(Selected, Options); });
                                           },
                                         .Variant = ButtonVariant::Primary},
                         },
                       .OnDismiss = [this] { _Dialog.Reset(); },
                       .Width     = 480.0f,
                     });
    }

    void MainView::State::StartRemove(std::vector<Library> Libraries, RemovalOptions Options) {
        _Scanner           = {};
        const bool Started = _Task.Start<RemovalReport>(
          Libraries.size() == 1 ? "Removing library" : std::format("Removing {} libraries", Libraries.size()),
          [Where = GetWidget().Model->Where, Libraries = std::move(Libraries), Options](const ProgressSink& Progress,
                                                                                        std::stop_token Stop) {
              return RemoveLibraries(Where, Libraries, Options, Progress, Stop);
          },
          [this](RemovalReport Report) {
              _Dialog.Reset();
              if (Finished()) {
                  return;
              }
              const std::string Title = RemovalTitle(Report);
              _Dialog =
                ShowDialog(*_Overlay,
                           _Style,
                           Dialog {
                             .Title   = Title,
                             .Content = RemovalReportView {std::move(Report)},
                             .Actions =
                               {
                                 DialogAction {.Label     = "View log",
                                               .OnPressed = [this] { ShowLogWindow(_LogWindow, GetWidget().Model); }},
                                 DialogAction {.Label     = "Close",
                                               .OnPressed = [this] { _Dialog.Reset(); },
                                               .Variant   = ButtonVariant::Primary},
                               },
                             .OnDismiss = [this] { _Dialog.Reset(); },
                             .Width     = 520.0f,
                           });
          });
        if (Started) {
            ShowProgress();
        }
    }

    // ---- Relocation -------------------------------------------------------------------------------------------

    bool MainView::State::CanRelocate(const std::vector<Library>& Selected) const {
        return !Busy() && Selected.size() == 1 && Selected.front().ContentExists;
    }

    void MainView::State::ConfirmRelocate() {
        const std::vector<Library> Selected = GetWidget().Model->SelectedLibraries();
        if (!CanRelocate(Selected) || _Overlay == nullptr || _Dialog.IsShown()) {
            return;
        }
        _Dialog = ShowDialog(
          *_Overlay,
          _Style,
          Dialog {
            .Title     = "Relocate library",
            .Content   = RelocateForm {.Entry = Selected.front(),
                                       .OnRelocate =
                                         [this, Entry = Selected.front()](const std::filesystem::path& Destination) {
                                           _Dialog.Reset();
                                           Post([this, Entry, Destination] { StartRelocate(Entry, Destination); });
                                         },
                                       .OnCancel = [this] { _Dialog.Reset(); }},
            .OnDismiss = [this] { _Dialog.Reset(); },
            .Width     = 520.0f,
          });
    }

    void MainView::State::StartRelocate(Library Entry, std::filesystem::path Destination) {
        _Scanner           = {};
        const bool Started = _Task.Start<RelocationResult>(
          std::format("Relocating {}", ToUtf8(Entry.Name)),
          [Entry, Destination](const ProgressSink& Progress, std::stop_token Stop) {
              return RelocateLibrary(Entry, Destination, Progress, Stop);
          },
          [this, Name = ToUtf8(Entry.Name)](RelocationResult Result) {
              _Dialog.Reset();
              if (Finished()) {
                  return;
              }
              if (Result.Succeeded) {
                  std::string Body = std::format("{} is now in {}.", Name, ToUtf8(Result.NewContentDir.wstring()));
                  if (!Result.Warning.empty()) {
                      Body += "\n\n" + Result.Warning;
                  }
                  ShowMessage("Library relocated", Body);
              } else {
                  ShowMessage("Relocation failed", Result.Error + "\n\nThe library was left where it was.");
              }
          });
        if (Started) {
            ShowProgress();
        }
    }

    void MainView::State::ShowProgress() {
        _Dialog = ShowDialog(*_Overlay,
                             _Style,
                             Dialog {
                               .Title     = _Task.Title.Peek(),
                               .Content   = TaskProgressView {&_Task},
                               .Actions   = {DialogAction {.Label = "Stop", .OnPressed = [this] { _Task.Cancel(); }}},
                               .OnDismiss = [] {},
                               .Width     = 460.0f,
                             },
                             false);
    }

    /// @brief After a task: rescan, so the list shows what is really there now, or close the window if the user
    /// asked to quit. The window closes once the current callback has returned, since closing it destroys this view.
    /// @return Whether the window is closing.
    bool MainView::State::Finished() {
        if (_QuitWhenDone) {
            App::Post([Target = GetWidget().Model->MainWindow] { Target->Destroy(); });
            return true;
        }
        Rescan();
        return false;
    }

    void MainView::State::RequestClose() {
        if (_Message.IsShown()) {
            return;
        }
        _Message = ShowDialog(*_Overlay,
                              _Style,
                              Dialog {
                                .Title   = "K8Tool is still working",
                                .Content = Text {.Content = "Stopping now finishes the current step and leaves the "
                                                            "remaining libraries untouched."},
                                .Actions =
                                  {
                                    DialogAction {.Label = "Keep working", .OnPressed = [this] { _Message.Reset(); }},
                                    DialogAction {.Label = "Stop and quit",
                                                  .OnPressed =
                                                    [this] {
                                                        _Message.Reset();
                                                        _QuitWhenDone = true;
                                                        _Task.Cancel();
                                                    },
                                                  .Variant = ButtonVariant::Primary},
                                  },
                                .OnDismiss = [this] { _Message.Reset(); },
                              });
    }

    // ---- Files ------------------------------------------------------------------------------------------------

    void MainView::State::ExportList() {
        AppModel* Shared                                = GetWidget().Model;
        const std::optional<std::filesystem::path> File = FileDialog::SaveFile({
          .Title            = "Export library list",
          .InitialFolder    = ExportDirectory(),
          .FileName         = ToUtf8(std::format(L"K8Tool Libraries {}.csv", FileTimestamp())),
          .Filters          = {{"CSV files", "*.csv"}},
          .DefaultExtension = "csv",
          .Owner            = Shared->MainWindow,
        });
        if (!File) {
            return;
        }
        std::vector<Library> Rows;
        for (const size_t Index : Shared->Visible.Peek()) {
            Rows.push_back(Shared->Libraries.Peek()[Index]);
        }
        std::string Error;
        if (ExportLibraries(Rows, *File, Error)) {
            Shell::Reveal(*File);
        } else {
            ShowMessage("Export failed", Error);
        }
    }

    void MainView::State::CollectBackupsToZip() {
        if (Busy()) {
            return;
        }
        AppModel* Shared                                = GetWidget().Model;
        const std::optional<std::filesystem::path> File = FileDialog::SaveFile({
          .Title            = "Save backups as",
          .InitialFolder    = ExportDirectory(),
          .FileName         = ToUtf8(std::format(L"K8Tool Backups {}.zip", FileTimestamp())),
          .Filters          = {{"Zip archives", "*.zip"}},
          .DefaultExtension = "zip",
          .Owner            = Shared->MainWindow,
        });
        if (!File) {
            return;
        }
        const bool Started = _Task.Start<CollectResult>(
          "Collecting backups",
          [Backups = Shared->Where.Backups, Archive = *File](const ProgressSink& Progress, std::stop_token) {
              Progress({.Message = "Compressing backups"});
              CollectResult Result;
              Result.Archive = CollectBackups(Backups, Archive, Result.Error);
              return Result;
          },
          [this](CollectResult Result) {
              _Dialog.Reset();
              if (_QuitWhenDone) {
                  Finished();
              } else if (Result.Archive) {
                  Shell::Reveal(Result.Archive->File);
              } else {
                  ShowMessage("No backups collected", Result.Error);
              }
          });
        if (Started) {
            ShowProgress();
        }
    }

    // ---- About and updates ------------------------------------------------------------------------------------

    void MainView::State::CheckForUpdates(bool Quietly) {
        if (_CheckingUpdates.Peek()) {
            return;
        }
        _CheckingUpdates = true;
        if (!Quietly) {
            _UpdateStatus = std::string();
        }
        _UpdateCheck = std::jthread([this, Quietly] {
            std::string Error;
            std::optional<UpdateInfo> Latest = FetchLatestRelease(Error);
            Post([this, Quietly, Latest, Error] {
                _CheckingUpdates  = false;
                AppModel* Current = GetWidget().Model;
                if (!Latest) {
                    Log::Warn("Update check failed: {}", Error);
                    if (!Quietly) {
                        _UpdateStatus = "Couldn't check for updates: " + Error;
                    }
                    return;
                }
                if (Latest->Latest > kCurrentVersion) {
                    Log::Info("K8Tool {} is available", Latest->Latest.ToString());
                    Current->Update          = Latest;
                    Current->UpdateDismissed = false;
                    _UpdateStatus            = std::format("Version {} is available.", Latest->Latest.ToString());
                } else {
                    _UpdateStatus = std::format("You're up to date (latest release: {}).", Latest->Latest.ToString());
                }
            });
        });
    }

    void MainView::State::ShowAbout() {
        if (_Overlay == nullptr || _Dialog.IsShown()) {
            return;
        }
        _Dialog = ShowDialog(*_Overlay,
                             _Style,
                             Dialog {
                               .Title     = "About K8Tool",
                               .Content   = AboutView {.Version           = kCurrentVersion.ToString(),
                                                       .OnCheckForUpdates = [this] { CheckForUpdates(false); },
                                                       .UpdateStatus      = &_UpdateStatus,
                                                       .Checking          = &_CheckingUpdates},
                               .Actions   = {DialogAction {.Label     = "Close",
                                                           .OnPressed = [this] { _Dialog.Reset(); },
                                                           .Variant   = ButtonVariant::Primary}},
                               .OnDismiss = [this] { _Dialog.Reset(); },
                               .Width     = 460.0f,
                             });
    }

    void MainView::State::ShowMessage(std::string Title, std::string Body) {
        if (_Overlay == nullptr) {
            return;
        }
        _Message = ShowDialog(*_Overlay,
                              _Style,
                              Dialog {
                                .Title     = std::move(Title),
                                .Content   = Text {.Content = std::move(Body)},
                                .Actions   = {DialogAction {.Label     = "OK",
                                                            .OnPressed = [this] { _Message.Reset(); },
                                                            .Variant   = ButtonVariant::Primary}},
                                .OnDismiss = [this] { _Message.Reset(); },
                              });
    }

    // ---- Layout -----------------------------------------------------------------------------------------------

    std::vector<MenuItem> MainView::State::RowMenu() {
        const std::vector<Library> Selected = GetWidget().Model->SelectedLibraries();
        const bool Single                   = Selected.size() == 1;
        const bool HasFolder                = Single && Selected.front().ContentExists;
        const std::filesystem::path Folder  = Single ? Selected.front().ContentDir : std::filesystem::path();
        return {
          MenuItem {.Label = Selected.size() > 1 ? std::format("Remove {} libraries…", Selected.size()) : "Remove…",
                    .ShortcutText = "Del",
                    .OnSelect     = [this] { Post([this] { ConfirmRemove(); }); },
                    .Enabled      = !Busy() && !Selected.empty()},
          MenuItem {.Label    = "Relocate…",
                    .OnSelect = [this] { Post([this] { ConfirmRelocate(); }); },
                    .Enabled  = CanRelocate(Selected)},
          MenuItem::Divider(),
          MenuItem {.Label    = "Open content folder",
                    .OnSelect = [Folder] { Shell::Open(ToUtf8(Folder.wstring())); },
                    .Enabled  = HasFolder},
          MenuItem {.Label    = "Copy content folder path",
                    .OnSelect = [Folder] { Clipboard::SetText(ToUtf8(Folder.wstring())); },
                    .Enabled  = Single && !Folder.empty()},
        };
    }

    std::vector<Menu> MainView::State::Menus() {
        AppModel* Shared   = GetWidget().Model;
        const bool Working = _Task.Running();
        return {
          Menu {.Label = "File",
                .Items =
                  {
                    MenuItem {.Label        = "Rescan libraries",
                              .ShortcutText = "F5",
                              .OnSelect     = [this] { Rescan(); },
                              .Enabled      = !Working},
                    MenuItem::Divider(),
                    MenuItem {.Label        = "Export library list…",
                              .ShortcutText = "Ctrl+E",
                              .OnSelect     = [this] { Post([this] { ExportList(); }); }},
                    MenuItem {.Label    = "Collect backups into a zip…",
                              .OnSelect = [this] { Post([this] { CollectBackupsToZip(); }); },
                              .Enabled  = !Working},
                    MenuItem {.Label    = "Open data folder",
                              .OnSelect = [] { Shell::Open(ToUtf8(AppDataDirectory().wstring())); }},
                    MenuItem::Divider(),
                    MenuItem {.Label = "Exit", .OnSelect = [Shared] { Shared->MainWindow->Close(); }},
                  }},
          Menu {.Label = "Library", .Items = RowMenu()},
          Menu {.Label = "View",
                .Items = {MenuItem {
                  .Label        = "Log",
                  .ShortcutText = "Ctrl+L",
                  .OnSelect     = [this] { Post([this] { ShowLogWindow(_LogWindow, GetWidget().Model); }); }}}},
          Menu {.Label = "Help",
                .Items = {MenuItem {.Label = "Check for updates",
                                    .OnSelect =
                                      [this] {
                                          Post([this] {
                                              ShowAbout();
                                              CheckForUpdates(false);
                                          });
                                      }},
                          MenuItem {.Label = "Project page", .OnSelect = [] { Shell::Open(kRepoUrl); }},
                          MenuItem::Divider(),
                          MenuItem {.Label = "About K8Tool", .OnSelect = [this] { Post([this] { ShowAbout(); }); }}}},
        };
    }

    Node MainView::State::UpdateBanner(const Theme& Style) {
        AppModel* Shared                         = GetWidget().Model;
        const std::optional<UpdateInfo>& Release = Shared->Update();
        if (!Release || Shared->UpdateDismissed()) {
            return {};
        }
        return Container {
          .Padding    = EdgeInsets::Symmetric(16.0f, 6.0f),
          .Background = Style.Accent.WithAlpha(0.15f),
          .Child      = Row {.Spacing        = 12.0f,
                             .CrossAlignment = CrossAxisAlignment::Center,
                             .Children =
                               {
                                 Text {.Content = std::format("K8Tool {} is available.", Release->Latest.ToString())},
                                 Hyperlink {.Label = "Download", .Url = Release->Url},
                                 Spacer {},
                                 Button {.Label     = "Dismiss",
                                         .OnPressed = [Shared] { Shared->UpdateDismissed = true; },
                                         .Variant   = ButtonVariant::Ghost},
                               }},
        };
    }

    Node MainView::State::Header(const Theme& Style) {
        AppModel* Shared                = GetWidget().Model;
        const std::vector<Library>& All = Shared->Libraries();
        const std::uintmax_t Total =
          std::accumulate(All.begin(), All.end(), std::uintmax_t {0}, [](std::uintmax_t Sum, const Library& Entry) {
              return Sum + Entry.Size.value_or(0);
          });
        const bool Pending =
          std::any_of(All.begin(), All.end(), [](const Library& Entry) { return Entry.ContentExists && !Entry.Size; });
        std::string Summary = Shared->Scanning()
                                ? std::string("Scanning the registry…")
                                : std::format("{} {}", All.size(), All.size() == 1 ? "library" : "libraries");
        if (!Shared->Scanning() && Total > 0) {
            Summary += std::format(" · {}{}", FormatSize(Total), Pending ? " so far" : "");
        }

        return Container {
          .Padding = EdgeInsets {.Left = 16.0f, .Top = 12.0f, .Right = 16.0f, .Bottom = 8.0f},
          .Child   = Row {.Spacing        = 8.0f,
                          .CrossAlignment = CrossAxisAlignment::Center,
                          .Children =
                            {
                              Text {.Content = "Kontakt libraries",
                                    .Font    = Font {.Size = 16.0f, .Weight = FontWeight::SemiBold}},
                              Text {.Content = Summary, .Color = Style.TextMuted},
                              Spacer {},
                              TextField {.InitialText = Shared->Filter.Peek(),
                                         .OnChanged   = [Shared](const std::string& Text) { Shared->SetFilter(Text); },
                                         .Placeholder = "Filter by name or folder",
                                         .Width       = 240.0f,
                                         .Label       = "Filter libraries"},
                              Button {.Label     = "Rescan",
                                      .OnPressed = [this] { Rescan(); },
                                      .Enabled   = !Shared->Scanning() && !_Task.Running()},
                            }},
        };
    }

    Node MainView::State::Body(const Theme& Style) {
        AppModel* Shared                = GetWidget().Model;
        const std::vector<size_t>& Rows = Shared->Visible();
        if (Rows.empty()) {
            const std::string Message = Shared->Scanning() ? std::string()
                                        : Shared->Libraries().empty()
                                          ? std::string("No Kontakt libraries were found in the registry.")
                                          : std::string("No libraries match the filter.");
            return Center {.Child = Text {.Content = Message, .Color = Style.TextMuted}};
        }
        return Container {
          .Margin       = EdgeInsets::Symmetric(16.0f, 0.0f),
          .CornerRadius = Style.CornerRadius,
          .BorderColor  = Style.Border,
          .BorderWidth  = 1.0f,
          .Child        = Table {
            .Columns =
              {
                TableColumn {.Title = "Name", .Width = 260.0f, .MinWidth = 120.0f},
                TableColumn {.Title = "Content folder", .Width = 330.0f, .MinWidth = 120.0f},
                TableColumn {.Title = "Size", .Width = 96.0f, .MinWidth = 70.0f, .Alignment = TextAlignment::Trailing},
                TableColumn {.Title = "Status", .MinWidth = 80.0f},
              },
            .RowCount = static_cast<int>(Rows.size()),
            .CellText = [Shared](int Row, int Column) -> std::string {
                const Library* Entry = Shared->RowLibrary(Row);
                if (Entry == nullptr) {
                    return {};
                }
                switch (Column) {
                    case NameColumn:
                        return ToUtf8(Entry->Name);
                    case FolderColumn:
                        return ToUtf8(Entry->ContentDir.wstring());
                    case SizeColumn:
                        return SizeText(*Entry);
                    default:
                        return StatusText(*Entry);
                }
            },
            .Selected           = &Shared->Lead,
            .OnSelected         = [Shared](int Row) { Shared->SelectLead(Row); },
            .SelectionMode      = TableSelectionMode::Multiple,
            .SelectedRows       = &Shared->Selection,
            .OnSelectionChanged = [Shared](const std::vector<int>& Selected) { Shared->SelectRows(Selected); },
            .RowMenu            = [this](int) { return RowMenu(); },
            .Sort               = &Shared->Sort,
            .OnSort             = [Shared](const TableSort&) { Shared->Reselect(); },
            .Label              = "Libraries",
          },
        };
    }

    Node MainView::State::Footer(const Theme& Style) {
        AppModel* Shared = GetWidget().Model;
        (void)Shared->Selection();
        const std::vector<Library> Selected = Shared->SelectedLibraries();
        const bool Working                  = _Task.Running();
        std::uintmax_t Bytes                = 0;
        for (const Library& Entry : Selected) {
            Bytes += Entry.Size.value_or(0);
        }

        const std::string Summary =
          Selected.empty() ? std::string("Select libraries to remove or relocate. Ctrl and Shift select several.")
          : Selected.size() == 1
            ? ToUtf8(Selected.front().Name) + (Bytes > 0 ? " · " + FormatSize(Bytes) : std::string())
            : std::format("{} selected{}", Selected.size(), Bytes > 0 ? " · " + FormatSize(Bytes) : std::string());
        return Container {
          .Padding = EdgeInsets::Symmetric(16.0f, 12.0f),
          .Child =
            Row {.Spacing        = 8.0f,
                 .CrossAlignment = CrossAxisAlignment::Center,
                 .Children =
                   {
                     Expanded {.Child = Text {.Content  = Summary,
                                              .Color    = Style.TextMuted,
                                              .Wrapping = TextWrapping::NoWrap,
                                              .Trimming = TextTrimming::Ellipsis}},
                     Button {.Label     = "Relocate…",
                             .OnPressed = [this] { ConfirmRelocate(); },
                             .Enabled   = !Working && CanRelocate(Selected)},
                     Button {.Label     = Selected.size() > 1 ? std::format("Remove {}…", Selected.size()) : "Remove…",
                             .OnPressed = [this] { ConfirmRemove(); },
                             .Variant   = ButtonVariant::Primary,
                             .Enabled   = !Working && !Selected.empty()},
                   }},
        };
    }

    Node MainView::State::Build(BuildContext& Context) {
        _Overlay           = OverlayController::Of(Context);
        _Style             = PopupStyle::Capture(Context);
        const Theme& Style = Theme::Of(Context);

        NodeList Children {TitleBar {.Title = "K8Tool", .Menus = Menus()}};
        if (Node Banner = UpdateBanner(Style); !Banner.IsEmpty()) {
            Children.push_back(std::move(Banner));
        }
        Children.push_back(Header(Style));
        Children.push_back(Expanded {.Child = Body(Style)});
        Children.push_back(Footer(Style));

        return Container {
          .Background = Style.Background,
          .Child      = Column {.CrossAlignment = CrossAxisAlignment::Stretch, .Children = std::move(Children)},
        };
    }
}  // namespace K8::Ui
