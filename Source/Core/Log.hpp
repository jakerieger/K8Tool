#pragma once

#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace K8 {
    enum class LogLevel { Debug, Info, Warning, Error };

    /// @brief Thread-safe application log. Lines go to a file and to an in-memory list that the log viewer reads.
    /// Messages are UTF-8 and are never used as format strings, so paths containing '%' or '{' are safe.
    class Log {
    public:
        /// @brief Opens the log file, starting a fresh one when the existing file passes a size limit.
        static void Open(const std::filesystem::path& File);

        static void Write(LogLevel Level, std::string_view Message);

        template<class... Args>
        static void Info(std::format_string<Args...> Format, Args&&... Arguments) {
            Write(LogLevel::Info, std::format(Format, std::forward<Args>(Arguments)...));
        }

        template<class... Args>
        static void Warn(std::format_string<Args...> Format, Args&&... Arguments) {
            Write(LogLevel::Warning, std::format(Format, std::forward<Args>(Arguments)...));
        }

        template<class... Args>
        static void Error(std::format_string<Args...> Format, Args&&... Arguments) {
            Write(LogLevel::Error, std::format(Format, std::forward<Args>(Arguments)...));
        }

        template<class... Args>
        static void Debug(std::format_string<Args...> Format, Args&&... Arguments) {
            Write(LogLevel::Debug, std::format(Format, std::forward<Args>(Arguments)...));
        }

        /// @brief Every line written since the log was opened.
        [[nodiscard]] static std::vector<std::string> Lines();

        /// @brief Called, on the writing thread, with each new line. Replaces any earlier listener.
        static void SetListener(std::function<void(const std::string& Line)> Listener);

        [[nodiscard]] static std::filesystem::path GetFile();
    };
}  // namespace K8
