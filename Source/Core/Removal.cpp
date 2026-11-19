#include "Core/Removal.hpp"
#include "Core/Files.hpp"
#include "Core/Format.hpp"
#include "Core/Log.hpp"
#include "Core/Registry.hpp"
#include "Core/ServiceCenter.hpp"
#include "Core/Text.hpp"

#include <algorithm>
#include <format>

namespace K8 {
    namespace fs = std::filesystem;

    namespace {
        /// @brief Characters Windows does not allow in file names, replaced so any library name makes a valid one.
        std::wstring SafeFileName(std::wstring Name) {
            for (wchar_t& Character : Name) {
                if (Character < 32 || std::wstring_view(L"<>:\"/\\|?*").find(Character) != std::wstring_view::npos) {
                    Character = L'_';
                }
            }
            return Name;
        }

        class Recorder {
        public:
            explicit Recorder(std::vector<StepResult>& Steps, std::wstring_view Library)
                : _Steps(Steps), _Library(Library) {}

            bool Record(std::string Description, bool Succeeded, const std::wstring& Detail = {}) {
                const std::string DetailText = ToUtf8(Detail);
                if (Succeeded) {
                    Log::Info("{}: {}{}",
                              ToUtf8(_Library),
                              Description,
                              DetailText.empty() ? "" : " (" + DetailText + ")");
                } else {
                    Log::Error("{}: {} failed: {}", ToUtf8(_Library), Description, DetailText);
                }
                _Steps.push_back({std::move(Description), Succeeded, DetailText});
                return Succeeded;
            }

            void Remove(const std::string& Description, const fs::path& File) {
                std::wstring Error;
                const bool Deleted = Files::Delete(File, Error);
                Record(Description, Deleted, Deleted ? File.wstring() : Error);
            }

        private:
            std::vector<StepResult>& _Steps;
            std::wstring_view _Library;
        };

        void RemoveOne(const Environment& Where,
                       const Library& Entry,
                       const RemovalOptions& Options,
                       const std::wstring& Timestamp,
                       const std::function<void(std::string_view)>& Step,
                       std::stop_token Stop,
                       LibraryReport& Report) {
            Recorder Steps(Report.Steps, Entry.Name);

            if (Options.BackupRegistry) {
                Step("Backing up registry");
                for (size_t i = 0; i < Entry.Keys.size(); ++i) {
                    const std::wstring Suffix = Entry.Keys.size() > 1 ? std::format(L"-{}", i + 1) : L"";
                    const fs::path File =
                      Where.Backups / std::format(L"{}-{}{}.reg", SafeFileName(Entry.Name), Timestamp, Suffix);
                    std::wstring Error;
                    if (!Steps.Record("Back up registry key",
                                      Registry::ExportToFile(Entry.Keys[i], File, Error),
                                      Error.empty() ? File.wstring() : Error)) {
                        Steps.Record("Skipped", false, L"The registry backup failed, so nothing was removed");
                        return;
                    }
                }
            }
            if (Stop.stop_requested()) {
                return;
            }

            Step("Removing Service Center entry");
            const fs::path OwnXml                 = Where.ServiceCenter / (Entry.Name + L".xml");
            const std::optional<ProductHint> Hint = FindProduct(Where.ServiceCenter, Entry.Name);
            std::error_code Error;
            if (fs::is_regular_file(OwnXml, Error)) {
                Steps.Remove("Delete Service Center XML", OwnXml);
            }
            if (!Hint) {
                Steps.Record("Find product ID",
                             false,
                             L"No SNPID was found, so cache files and license tokens were left");
            }
            if (Stop.stop_requested()) {
                return;
            }

            if (Hint) {
                Step("Removing cache files");
                for (const fs::path& File : Files::FilesIn(Where.LibrariesCache, [&](const fs::path& Path) {
                         return Files::IsCacheFileFor(Path, Hint->Snpid);
                     })) {
                    Steps.Remove("Delete cache file", File);
                }
                if (Stop.stop_requested()) {
                    return;
                }

                Step("Removing license tokens");
                for (const fs::path& File : Files::FilesIn(Where.Ras3, [&](const fs::path& Path) {
                         return Files::IsLicenseFileFor(Path, Hint->Snpid);
                     })) {
                    Steps.Remove("Delete license token", File);
                }
                if (Stop.stop_requested()) {
                    return;
                }
            }

            if (Options.DeleteContent && Entry.ContentExists) {
                Step("Deleting content folder");
                Steps.Remove("Delete content folder", Entry.ContentDir);
                if (Stop.stop_requested()) {
                    return;
                }
            }

            Step("Removing registry keys");
            bool AllDeleted = true;
            for (const RegistryKey& Key : Entry.Keys) {
                std::wstring KeyError;
                AllDeleted &= Steps.Record("Delete registry key",
                                           Registry::DeleteTree(Key, KeyError),
                                           KeyError.empty() ? Registry::FullName(Key) : KeyError);
            }
            Report.Removed = AllDeleted;
        }
    }  // namespace

    bool LibraryReport::HasFailures() const {
        return std::any_of(Steps.begin(), Steps.end(), [](const StepResult& Step) { return !Step.Succeeded; });
    }

    RemovalReport RemoveLibraries(const Environment& Where,
                                  std::span<const Library> Libraries,
                                  const RemovalOptions& Options,
                                  const ProgressSink& Progress,
                                  std::stop_token Stop) {
        RemovalReport Report;
        const std::wstring Timestamp = FileTimestamp();
        const size_t Total           = Libraries.size();
        Log::Info("Removing {} libraries (backup registry: {}, delete content: {})",
                  Total,
                  Options.BackupRegistry,
                  Options.DeleteContent);

        for (size_t i = 0; i < Total; ++i) {
            if (Stop.stop_requested()) {
                Report.Cancelled = true;
                break;
            }
            const Library& Entry   = Libraries[i];
            LibraryReport& Result  = Report.Libraries.emplace_back(LibraryReport {.Name = Entry.Name});
            const std::string Name = ToUtf8(Entry.Name);
            const auto Step        = [&](std::string_view What) {
                if (Progress) {
                    Progress({.Fraction = static_cast<float>(i) / static_cast<float>(Total + 1),
                              .Message  = std::format("{} ({} of {}): {}", Name, i + 1, Total, What)});
                }
            };
            RemoveOne(Where, Entry, Options, Timestamp, Step, Stop, Result);
            if (Stop.stop_requested() && !Result.Removed) {
                Report.Cancelled = true;
            }
        }

        const bool AnyRemoved = std::any_of(Report.Libraries.begin(),
                                            Report.Libraries.end(),
                                            [](const LibraryReport& Entry) { return Entry.Removed; });
        std::error_code Error;
        if (AnyRemoved && fs::is_regular_file(Where.KompleteDatabase, Error)) {
            if (Progress) {
                Progress({.Fraction = static_cast<float>(Total) / static_cast<float>(Total + 1),
                          .Message  = "Resetting the Kontakt library database"});
            }
            Recorder Steps(Report.Database, L"komplete.db3");
            const fs::path Backup = Where.Backups / std::format(L"komplete-{}.db3.bak", Timestamp);
            fs::create_directories(Where.Backups, Error);
            fs::copy_file(Where.KompleteDatabase, Backup, fs::copy_options::overwrite_existing, Error);
            if (Steps.Record("Back up database", !Error, Error ? FromUtf8(Error.message()) : Backup.wstring())) {
                Steps.Remove("Delete database (Kontakt rebuilds it on launch)", Where.KompleteDatabase);
            }
        }
        if (Progress) {
            Progress({.Fraction = 1.0f, .Message = "Done"});
        }
        return Report;
    }
}  // namespace K8
