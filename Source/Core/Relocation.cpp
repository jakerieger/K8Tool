#include "Core/Relocation.hpp"
#include "Core/Files.hpp"
#include "Core/Log.hpp"
#include "Core/Registry.hpp"
#include "Core/Text.hpp"

#include <format>

namespace K8 {
    namespace fs = std::filesystem;

    namespace {
        RelocationResult Failure(const Library& Entry, std::string Error) {
            Log::Error("Relocating '{}' failed: {}", ToUtf8(Entry.Name), Error);
            return {.Error = std::move(Error)};
        }

        /// @brief Points every key that has a ContentDir at Target. Restores the keys already changed on failure.
        bool UpdateRegistry(const Library& Entry, const fs::path& Target, std::wstring& Error) {
            std::vector<RegistryKey> Changed;
            for (const RegistryKey& Key : Entry.Keys) {
                if (!Registry::ReadString(Key, L"ContentDir")) {
                    continue;
                }
                if (!Registry::WriteString(Key, L"ContentDir", Target.wstring(), Error)) {
                    std::wstring Ignored;
                    for (const RegistryKey& Restore : Changed) {
                        Registry::WriteString(Restore, L"ContentDir", Entry.ContentDir.wstring(), Ignored);
                    }
                    return false;
                }
                Changed.push_back(Key);
            }
            return true;
        }
    }  // namespace

    fs::path RelocationTarget(const Library& Entry, const fs::path& Destination) {
        fs::path Name = Entry.ContentDir.filename();
        if (Name.empty()) {
            Name = Entry.ContentDir.parent_path().filename();
        }
        return Destination / Name;
    }

    std::optional<std::string> ValidateRelocation(const Library& Entry, const fs::path& Destination) {
        std::error_code Error;
        if (Entry.ContentDir.empty() || !Entry.ContentExists || !fs::is_directory(Entry.ContentDir, Error)) {
            return "The library's content folder doesn't exist, so there is nothing to move.";
        }
        if (Destination.empty()) {
            return "Choose a destination folder.";
        }
        if (!Destination.is_absolute()) {
            return "Enter a full path, such as D:\\Kontakt Libraries.";
        }
        if (!fs::is_directory(Destination, Error)) {
            return "The destination folder doesn't exist.";
        }
        if (Files::IsSameOrInside(Destination, Entry.ContentDir)) {
            return "The destination is inside the library's own folder.";
        }
        const fs::path Target = RelocationTarget(Entry, Destination);
        if (Files::IsSameOrInside(Target, Entry.ContentDir)) {
            return "The library is already in that folder.";
        }
        if (fs::exists(Target, Error)) {
            return std::format("{} already exists.", ToUtf8(Target.wstring()));
        }
        return std::nullopt;
    }

    RelocationResult RelocateLibrary(const Library& Entry,
                                     const fs::path& Destination,
                                     const ProgressSink& Progress,
                                     std::stop_token Stop) {
        if (const std::optional<std::string> Problem = ValidateRelocation(Entry, Destination)) {
            return Failure(Entry, *Problem);
        }
        const fs::path Source = Entry.ContentDir;
        const fs::path Target = RelocationTarget(Entry, Destination);
        Log::Info("Relocating '{}' from {} to {}",
                  ToUtf8(Entry.Name),
                  ToUtf8(Source.wstring()),
                  ToUtf8(Target.wstring()));

        std::wstring Error;
        std::string Warning;
        std::error_code Code;
        if (Files::OnSameVolume(Source, Destination)) {
            if (Progress) {
                Progress({.Message = "Moving the folder"});
            }
            fs::rename(Source, Target, Code);
            if (Code) {
                return Failure(Entry, std::format("Could not move the folder: {}", Code.message()));
            }
            if (!UpdateRegistry(Entry, Target, Error)) {
                fs::rename(Target, Source, Code);
                return Failure(Entry, ToUtf8(Error));
            }
        } else {
            if (!Files::CopyTree(Source, Target, Progress, Stop, Error)) {
                return Failure(Entry, ToUtf8(Error));
            }
            if (Stop.stop_requested() || !UpdateRegistry(Entry, Target, Error)) {
                fs::remove_all(Target, Code);
                return Failure(Entry, Stop.stop_requested() ? std::string("Cancelled") : ToUtf8(Error));
            }
            if (Progress) {
                Progress({.Message = "Removing the original folder"});
            }
            if (!Files::Delete(Source, Error)) {
                // The library works from its new place; only the old copy is left behind.
                Warning = std::format("The original folder could not be deleted: {}", ToUtf8(Error));
                Log::Warn("'{}' moved. {}", ToUtf8(Entry.Name), Warning);
            }
        }
        Log::Info("Relocated '{}' to {}", ToUtf8(Entry.Name), ToUtf8(Target.wstring()));
        if (Progress) {
            Progress({.Fraction = 1.0f, .Message = "Done"});
        }
        return {.Succeeded = true, .NewContentDir = Target, .Warning = std::move(Warning)};
    }
}  // namespace K8
