#pragma once

#include "Core/Progress.hpp"

#include <XenUI/XenUI.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

namespace K8::Ui {
    /// @brief Runs one long operation at a time on a worker thread. Progress and the result come back to the UI
    /// thread through App::Post; nothing posted runs once the runner is gone.
    class TaskRunner {
    public:
        TaskRunner() = default;

        ~TaskRunner() {
            _Alive.reset();
            _Worker = {};
        }

        TaskRunner(const TaskRunner&)            = delete;
        TaskRunner& operator=(const TaskRunner&) = delete;

        /// @brief Starts Work unless a task is already running. Done receives its result on the UI thread.
        template<class Result>
        bool Start(std::string Name,
                   std::function<Result(const ProgressSink& Progress, std::stop_token Stop)> Work,
                   std::function<void(Result)> Done) {
            if (Running.Peek()) {
                return false;
            }
            _Worker  = {};
            Title    = std::move(Name);
            Progress = ProgressUpdate {};
            Running  = true;
            _Worker =
              std::jthread([this, Alive = std::weak_ptr(_Alive), Work = std::move(Work), Done = std::move(Done)](
                             std::stop_token Stop) mutable {
                  std::chrono::steady_clock::time_point Last;
                  const ProgressSink Sink = [this, Alive, &Last](const ProgressUpdate& Update) {
                      // Copying thousands of files would otherwise flood the UI thread with updates.
                      const auto Now = std::chrono::steady_clock::now();
                      if (Update.Fraction != 1.0f && Now - Last < std::chrono::milliseconds(50)) {
                          return;
                      }
                      Last = Now;
                      Post(Alive, [this, Update] { Progress = Update; });
                  };
                  Result Value = Work(Sink, Stop);
                  Post(Alive, [this, Value = std::move(Value), Done = std::move(Done)]() mutable {
                      Running = false;
                      Done(std::move(Value));
                  });
              });
            return true;
        }

        /// @brief Asks the running task to stop at its next checkpoint.
        void Cancel() {
            _Worker.request_stop();
        }

        [[nodiscard]] bool IsCancelling() const {
            return _Worker.get_stop_token().stop_requested();
        }

        XenUI::Signal<bool> Running = false;
        XenUI::Signal<std::string> Title;
        XenUI::Signal<ProgressUpdate> Progress;

    private:
        static void Post(const std::weak_ptr<bool>& Alive, std::function<void()> Work) {
            XenUI::App::Post([Alive, Work = std::move(Work)] {
                if (Alive.lock()) {
                    Work();
                }
            });
        }

        std::shared_ptr<bool> _Alive = std::make_shared<bool>(true);
        // Declared last, so it is stopped and joined before the signals above are destroyed.
        std::jthread _Worker;
    };
}  // namespace K8::Ui
