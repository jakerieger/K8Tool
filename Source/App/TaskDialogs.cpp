#include "App/TaskDialogs.hpp"
#include "Core/Text.hpp"

#include <algorithm>
#include <format>

using namespace XenUI;

namespace K8::Ui {
    namespace {
        size_t CountRemoved(const RemovalReport& Report) {
            return static_cast<size_t>(std::count_if(Report.Libraries.begin(),
                                                     Report.Libraries.end(),
                                                     [](const LibraryReport& Entry) { return Entry.Removed; }));
        }

        bool AnyFailures(const RemovalReport& Report) {
            return std::any_of(Report.Libraries.begin(),
                               Report.Libraries.end(),
                               [](const LibraryReport& Entry) { return Entry.HasFailures(); }) ||
                   std::any_of(Report.Database.begin(), Report.Database.end(), [](const StepResult& Step) {
                       return !Step.Succeeded;
                   });
        }
    }  // namespace

    std::string RemovalTitle(const RemovalReport& Report) {
        if (Report.Cancelled) {
            return "Removal stopped";
        }
        return AnyFailures(Report) ? "Removal finished with problems" : "Removal finished";
    }

    Node TaskProgressView::Build(BuildContext& Context) const {
        const Theme& Style            = Theme::Of(Context);
        const ProgressUpdate& Current = Runner->Progress();
        const bool Cancelling         = Runner->Running() && Runner->IsCancelling();
        return Column {
          .Spacing        = 10.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .MainSize       = MainAxisSize::Min,
          .Children =
            {
              ProgressBar {.Value = Current.Fraction, .Label = Runner->Title.Peek()},
              Text {.Content  = Cancelling                ? std::string("Stopping after the current step…")
                                : Current.Message.empty() ? std::string("Starting…")
                                                          : Current.Message,
                    .Color    = Style.TextMuted,
                    .Wrapping = TextWrapping::NoWrap,
                    .Trimming = TextTrimming::Ellipsis},
            },
        };
    }

    Node RemovalReportView::Build(BuildContext& Context) const {
        const Theme& Style   = Theme::Of(Context);
        const size_t Total   = Report.Libraries.size();
        const size_t Removed = CountRemoved(Report);

        NodeList Lines;
        for (const LibraryReport& Entry : Report.Libraries) {
            const bool Clean = Entry.Removed && !Entry.HasFailures();
            Lines.push_back(Row {.Spacing  = 8.0f,
                                 .Children = {Text {.Content = Clean           ? "✓"
                                                               : Entry.Removed ? "!"
                                                                               : "✕",
                                                    .Color   = Clean           ? Style.Success
                                                               : Entry.Removed ? Style.Warning
                                                                               : Style.Danger},
                                              Expanded {.Child = Text {.Content  = ToUtf8(Entry.Name),
                                                                       .Wrapping = TextWrapping::NoWrap,
                                                                       .Trimming = TextTrimming::Ellipsis}}}});
            for (const StepResult& Step : Entry.Steps) {
                if (Step.Succeeded) {
                    continue;
                }
                Lines.push_back(Padding {.Insets = {.Left = 20.0f},
                                         .Child = Text {.Content = std::format("{}: {}", Step.Description, Step.Detail),
                                                        .Color   = Style.TextMuted}});
            }
        }
        for (const StepResult& Step : Report.Database) {
            if (!Step.Succeeded) {
                Lines.push_back(
                  Text {.Content = std::format("Kontakt database — {}: {}", Step.Description, Step.Detail),
                        .Color   = Style.Warning});
            }
        }

        std::string Summary = std::format("Removed {} of {} {}.", Removed, Total, Total == 1 ? "library" : "libraries");
        if (Report.Cancelled) {
            Summary += " The rest were left as they were.";
        }
        if (Removed > 0) {
            Summary += " Kontakt rebuilds its library list the next time it starts.";
        }

        return Column {
          .Spacing        = 10.0f,
          .CrossAlignment = CrossAxisAlignment::Stretch,
          .MainSize       = MainAxisSize::Min,
          .Children =
            {
              Text {.Content = Summary},
              ConstrainedBox {.Limits = BoxConstraints {.MaxHeight = 220.0f},
                              .Child  = ScrollView {.Child = Column {.Spacing        = 4.0f,
                                                                     .CrossAlignment = CrossAxisAlignment::Stretch,
                                                                     .MainSize       = MainAxisSize::Min,
                                                                     .Children       = std::move(Lines)}}},
            },
        };
    }
}  // namespace K8::Ui
