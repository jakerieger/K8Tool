#include "App/RemoveDialog.hpp"

#include <algorithm>
#include <format>

using namespace XenUI;

namespace K8::Ui {
    Node RemoveSummary::Build(BuildContext& Context) const {
        const Theme& Style = Theme::Of(Context);
        const bool Backup  = BackupRegistry->Get();
        const bool Content = AnyContent && DeleteContent->Get();

        NodeList NameRows;
        for (const std::string& Name : Names) {
            NameRows.push_back(Text {.Content  = Name,
                                     .Font     = Font {.Size = 13.0f, .Weight = FontWeight::SemiBold},
                                     .Wrapping = TextWrapping::NoWrap,
                                     .Trimming = TextTrimming::Ellipsis});
        }
        const float ListHeight = std::min(160.0f, 20.0f * static_cast<float>(Names.size()) + 12.0f);

        NodeList Effects {
          Text {.Content = "• Delete its registry entries", .Color = Style.TextMuted},
          Text {.Content = "• Remove its Service Center XML, cache files and license tokens", .Color = Style.TextMuted},
          Text {.Content = "• Reset the Kontakt library database (Kontakt rebuilds it)", .Color = Style.TextMuted}};
        if (Content) {
            Effects.push_back(Text {.Content = "• Delete its content folder", .Color = Style.TextMuted});
        }

        NodeList Children {
          Text {.Content = Names.size() == 1 ? std::string("This library will be removed:")
                                             : std::format("These {} libraries will be removed:", Names.size())},
          Container {.Height       = ListHeight,
                     .Padding      = EdgeInsets::Symmetric(10.0f, 6.0f),
                     .Background   = Style.Track,
                     .CornerRadius = Style.CornerRadius,
                     .Child        = ScrollView {.Child = Column {.Spacing        = 4.0f,
                                                                  .CrossAlignment = CrossAxisAlignment::Stretch,
                                                                  .MainSize       = MainAxisSize::Min,
                                                                  .Children       = std::move(NameRows)}}},
          Column {.Spacing        = 2.0f,
                  .CrossAlignment = CrossAxisAlignment::Stretch,
                  .MainSize       = MainAxisSize::Min,
                  .Children       = std::move(Effects)},
          Checkbox {.Value     = Backup,
                    .OnChanged = [Signal = BackupRegistry](bool Value) { *Signal = Value; },
                    .Label     = "Back up registry entries first (recommended)"},
          Checkbox {.Value     = Content,
                    .OnChanged = [Signal = DeleteContent](bool Value) { *Signal = Value; },
                    .Label     = "Delete content folders",
                    .Enabled   = AnyContent},
        };
        if (Content) {
            Children.push_back(
              Text {.Content = "Content folders are deleted permanently. They do not go to the Recycle Bin.",
                    .Color   = Style.Danger});
        } else if (!Backup) {
            Children.push_back(Text {.Content = "Without a backup, removed registry entries cannot be restored.",
                                     .Color   = Style.Warning});
        }
        return Column {.Spacing        = 10.0f,
                       .CrossAlignment = CrossAxisAlignment::Stretch,
                       .MainSize       = MainAxisSize::Min,
                       .Children       = std::move(Children)};
    }
}  // namespace K8::Ui
