#include "Core/Log.hpp"

#include <Windows.h>

#include <chrono>
#include <fstream>

namespace K8 {
    namespace {
        constexpr std::uintmax_t kMaxFileSize = 1024 * 1024;
        constexpr size_t kMaxLines            = 20000;

        struct LogState {
            std::mutex Lock;
            std::ofstream File;
            std::filesystem::path Path;
            std::vector<std::string> Lines;
            std::function<void(const std::string&)> Listener;
        };

        LogState& State() {
            static LogState Instance;
            return Instance;
        }

        std::string_view LevelName(LogLevel Level) {
            switch (Level) {
                case LogLevel::Debug:
                    return "DEBUG";
                case LogLevel::Info:
                    return "INFO";
                case LogLevel::Warning:
                    return "WARN";
                case LogLevel::Error:
                    return "ERROR";
            }
            return "?";
        }
    }  // namespace

    void Log::Open(const std::filesystem::path& File) {
        LogState& Log = State();
        std::lock_guard Guard(Log.Lock);
        std::error_code Error;
        if (std::filesystem::file_size(File, Error) > kMaxFileSize && !Error) {
            std::filesystem::path Previous = File;
            Previous.replace_extension(L".old.log");
            std::filesystem::rename(File, Previous, Error);
        }
        Log.Path = File;
        Log.File.open(File, std::ios::app | std::ios::binary);
    }

    void Log::Write(LogLevel Level, std::string_view Message) {
#ifdef NDEBUG
        if (Level == LogLevel::Debug) {
            return;
        }
#endif
        const auto Now =
          std::chrono::zoned_time(std::chrono::current_zone(),
                                  std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
        std::string Line = std::format("[{:%Y-%m-%d %H:%M:%S}] [{}] {}", Now, LevelName(Level), Message);

        std::function<void(const std::string&)> Listener;
        {
            LogState& Log = State();
            std::lock_guard Guard(Log.Lock);
            if (Log.File.is_open()) {
                Log.File << Line << "\r\n";
                Log.File.flush();
            }
            if (Log.Lines.size() >= kMaxLines) {
                Log.Lines.erase(Log.Lines.begin(), Log.Lines.begin() + kMaxLines / 10);
            }
            Log.Lines.push_back(Line);
            Listener = Log.Listener;
        }
#ifndef NDEBUG
        OutputDebugStringA((Line + "\n").c_str());
#endif
        if (Listener) {
            Listener(Line);
        }
    }

    std::vector<std::string> Log::Lines() {
        LogState& Log = State();
        std::lock_guard Guard(Log.Lock);
        return Log.Lines;
    }

    void Log::SetListener(std::function<void(const std::string& Line)> Listener) {
        LogState& Log = State();
        std::lock_guard Guard(Log.Lock);
        Log.Listener = std::move(Listener);
    }

    std::filesystem::path Log::GetFile() {
        LogState& Log = State();
        std::lock_guard Guard(Log.Lock);
        return Log.Path;
    }
}  // namespace K8
