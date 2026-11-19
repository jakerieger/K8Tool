#pragma once

#include <functional>
#include <optional>
#include <string>

namespace K8 {
    /// @brief Where a long operation is up to.
    struct ProgressUpdate {
        /// @brief Completed share, 0 to 1; empty while unknown.
        std::optional<float> Fraction;
        /// @brief What is happening now, in UTF-8.
        std::string Message;

        bool operator==(const ProgressUpdate&) const = default;
    };

    /// @brief Receives progress on the worker thread that reports it.
    using ProgressSink = std::function<void(const ProgressUpdate& Update)>;
}  // namespace K8
