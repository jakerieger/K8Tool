#include "App/AppModel.hpp"
#include "App/MainView.hpp"
#include "Core/Environment.hpp"
#include "Core/Log.hpp"
#include "Core/Text.hpp"

#include "K8ToolVersion.h"
#include "Resource.h"

#include <XenUI/XenUI.hpp>

#include <Windows.h>

#include <format>

using namespace XenUI;

namespace {
    /// @brief Follows the Windows light or dark setting and gives every widget its colors and default text style.
    struct Root {
        K8::Ui::AppModel* Model = nullptr;

        Node Build(BuildContext& Context) const {
            (void)Context;
            const Theme Style = Theme::FromSystem();
            return Provide<Theme> {
              .Value = Style,
              .Child =
                Provide<TextStyle> {
                  .Value = {.Font = Style.BodyFont, .Color = Style.Text},
#ifndef NDEBUG
                  .Child = DeveloperTools {.Child = K8::Ui::MainView {Model}},
#else
                  .Child = K8::Ui::MainView {Model},
#endif
                },
            };
        }
    };

    /// @brief Appends each log line to the log window's text, keeping the most recent part of a long log.
    void ForwardLog(K8::Ui::AppModel& Model) {
        std::string Text;
        for (const std::string& Line : K8::Log::Lines()) {
            Text += Line + "\n";
        }
        Model.LogText = Text;
        K8::Log::SetListener([&Model](const std::string& Line) {
            App::Post([&Model, Line] {
                Model.LogText.Modify([&Line](std::string& All) {
                    constexpr size_t kLimit = 512 * 1024;
                    All += Line;
                    All += '\n';
                    if (All.size() > kLimit) {
                        All.erase(0, All.find('\n', All.size() - kLimit) + 1);
                    }
                });
            });
        });
    }
}  // namespace

int main() {
    K8::Log::Open(K8::LogDirectory() / L"K8Tool.log");
    K8::Log::Info("K8Tool {} starting", K8TOOL_VERSION_STRING);

    // The model outlives the App, so work it posts during shutdown never finds it gone.
    K8::Ui::AppModel Model;
    int ExitCode = 0;
    try {
        App Application;
        ForwardLog(Model);

        Window MainWindow({
          .Title          = std::format("K8Tool {}", K8TOOL_VERSION_STRING),
          .ClientSize     = {1000.0f, 620.0f},
          .CustomTitleBar = true,
          .Icon           = IDI_K8TOOL,
        });
        MainWindow.SetOnClosed([&Application] { Application.Quit(); });
        Model.MainWindow = &MainWindow;
        MainWindow.SetContent(Root {&Model});
        MainWindow.Show();
        ExitCode = Application.Run();
        K8::Log::SetListener({});
    } catch (const std::exception& Exception) {
        K8::Log::SetListener({});
        K8::Log::Error("Fatal error: {}", Exception.what());
        MessageBoxW(nullptr,
                    K8::FromUtf8(std::format("K8Tool could not continue:\n\n{}", Exception.what())).c_str(),
                    L"K8Tool",
                    MB_ICONERROR | MB_OK);
        ExitCode = 1;
    }
    K8::Log::Info("K8Tool exiting");
    return ExitCode;
}
