#include "App/LogWindow.hpp"
#include "Core/Log.hpp"
#include "Core/Text.hpp"

using namespace XenUI;

namespace K8::Ui {
    namespace {
        /// @brief The whole log, selectable and copyable, with the file it is saved to.
        struct LogView {
            AppModel* Model = nullptr;

            Node Build(BuildContext& Context) const {
                (void)Context;
                const Theme Style                = Theme::FromSystem();
                const std::filesystem::path File = Log::GetFile();
                return Provide<Theme> {
                  .Value = Style,
                  .Child =
                    Provide<TextStyle> {
                      .Value = {.Font = Style.BodyFont, .Color = Style.Text},
                      .Child =
                        Container {
                          .Padding    = EdgeInsets::All(12.0f),
                          .Background = Style.Background,
                          .Child =
                            Column {
                              .Spacing        = 8.0f,
                              .CrossAlignment = CrossAxisAlignment::Stretch,
                              .Children =
                                {
                                  Row {.Spacing  = 8.0f,
                                       .Children = {Expanded {.Child = Text {.Content  = ToUtf8(File.wstring()),
                                                                             .Color    = Style.TextMuted,
                                                                             .Wrapping = TextWrapping::NoWrap,
                                                                             .Trimming = TextTrimming::Ellipsis}},
                                                    Button {.Label     = "Copy all",
                                                            .OnPressed = [Model = Model] { Clipboard::SetText(Model->LogText.Peek()); }},
                                                    Button {.Label     = "Show in Explorer",
                                                            .OnPressed = [File] { Shell::Reveal(File); }}}},
                                  Expanded {.Child = TextArea {.Bind     = &Model->LogText,
                                                               .TextFont = Font {.Family = "Cascadia Mono", .Size = 12.0f},
                                                               .ReadOnly = true,
                                                               .Lines    = 30,
                                                               .Label    = "Log"}},
                                },
                            },
                        },
                    },
                };
            }
        };
    }  // namespace

    void ShowLogWindow(std::optional<Window>& Slot, AppModel* Model) {
        if (Slot && Slot->IsOpen()) {
            Slot->Activate();
            return;
        }
        Slot.emplace(WindowDesc {.Title = "K8Tool Log", .ClientSize = {900.0f, 560.0f}});
        Slot->SetContent(LogView {Model});
        Slot->Show();
    }
}  // namespace K8::Ui
