#include "App/RelocateDialog.hpp"
#include "Core/Relocation.hpp"
#include "Core/Text.hpp"

#include <format>

using namespace XenUI;

namespace K8::Ui {
    void RelocateForm::State::Browse() {
        const Library& Target                             = GetWidget().Entry;
        const std::optional<std::filesystem::path> Folder = FileDialog::PickFolder({
          .Title         = std::format("Choose a new location for {}", ToUtf8(Target.Name)),
          .InitialFolder = Target.ContentDir.parent_path(),
        });
        if (Folder) {
            _Destination = ToUtf8(Folder->wstring());
        }
    }

    Node RelocateForm::State::Build(BuildContext& Context) {
        const Theme& Style        = Theme::Of(Context);
        const RelocateForm& Props = GetWidget();
        const std::string Typed   = _Destination();
        const std::filesystem::path Destination(FromUtf8(Typed));
        const std::optional<std::string> Problem = ValidateRelocation(Props.Entry, Destination);
        const bool Valid                         = !Problem;

        const std::string Hint =
          Typed.empty() ? std::string("The library's folder will be moved into the folder you choose.")
          : Valid
            ? std::format("The library will move to {}", ToUtf8(RelocationTarget(Props.Entry, Destination).wstring()))
            : *Problem;

        return Column {
          .Spacing        = 10.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .MainSize       = MainAxisSize::Min,
          .Children =
            {
              Text {.Content = ToUtf8(Props.Entry.Name), .Font = Font {.Size = 14.0f, .Weight = FontWeight::SemiBold}},
              Text {.Content  = ToUtf8(Props.Entry.ContentDir.wstring()),
                    .Color    = Style.TextMuted,
                    .Wrapping = TextWrapping::NoWrap,
                    .Trimming = TextTrimming::Ellipsis},
              Text {.Content = "New location"},
              Row {.Spacing  = 8.0f,
                   .Children = {Expanded {.Child = TextField {.Bind = &_Destination,
                                                              .OnSubmit =
                                                                [this, Valid](const std::string& Text) {
                                                                    if (Valid) {
                                                                        GetWidget().OnRelocate(
                                                                          std::filesystem::path(FromUtf8(Text)));
                                                                    }
                                                                },
                                                              .Placeholder = "D:\\Kontakt Libraries",
                                                              .AutoFocus   = true,
                                                              .Label       = "New location"}},
                                Button {.Label = "Browse…", .OnPressed = [this] { Browse(); }}}},
              Text {.Content = Hint, .Color = Typed.empty() || Valid ? Style.TextMuted : Style.Warning},
              Row {.Spacing       = 8.0f,
                   .MainAlignment = MainAxisAlignment::End,
                   .Children      = {Button {.Label = "Cancel", .OnPressed = [this] { GetWidget().OnCancel(); }},
                                     Button {.Label     = "Relocate",
                                             .OnPressed = [this, Destination] { GetWidget().OnRelocate(Destination); },
                                             .Variant   = ButtonVariant::Primary,
                                             .Enabled   = Valid}}},
            },
        };
    }
}  // namespace K8::Ui
