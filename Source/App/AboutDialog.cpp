#include "App/AboutDialog.hpp"
#include "Core/Constants.hpp"

#include <format>

using namespace XenUI;

namespace K8::Ui {
    Node AboutView::Build(BuildContext& Context) const {
        const Theme& Style        = Theme::Of(Context);
        const bool IsChecking     = Checking->Get();
        const std::string& Status = UpdateStatus->Get();

        NodeList Children {
          Text {.Content = std::string(kProductName), .Font = Font {.Size = 20.0f, .Weight = FontWeight::SemiBold}},
          Text {.Content = std::format("Version {}", Version), .Color = Style.TextMuted},
          Text {.Content = "Removes Kontakt 8 libraries completely: registry entries, Service Center files, caches, "
                           "license tokens and, optionally, the library content."},
          Hyperlink {.Label = std::string(kRepoUrl), .Url = std::string(kRepoUrl)},
          Row {.Spacing  = 10.0f,
               .Children = {Button {.Label     = IsChecking ? "Checking…" : "Check for updates",
                                    .OnPressed = OnCheckForUpdates,
                                    .Enabled   = !IsChecking},
                            Expanded {.Child = Text {.Content = Status, .Color = Style.TextMuted}}}},
          Text {.Content = "Copyright © 2026 Jake Rieger. Built with XenUI.",
                .Font    = Style.SmallFont,
                .Color   = Style.TextMuted},
        };
        return Column {.Spacing        = 8.0f,
                       .CrossAlignment = CrossAxisAlignment::Stretch,
                       .MainSize       = MainAxisSize::Min,
                       .Children       = std::move(Children)};
    }
}  // namespace K8::Ui
