#include "Core/Environment.hpp"

#include <KnownFolders.h>
#include <ShlObj.h>

namespace K8 {
    namespace {
        std::filesystem::path KnownFolder(REFKNOWNFOLDERID Id) {
            PWSTR Raw = nullptr;
            std::filesystem::path Result;
            if (SUCCEEDED(SHGetKnownFolderPath(Id, KF_FLAG_DEFAULT, nullptr, &Raw))) {
                Result = Raw;
            }
            CoTaskMemFree(Raw);
            return Result;
        }

        std::filesystem::path Ensure(std::filesystem::path Directory) {
            std::error_code Error;
            std::filesystem::create_directories(Directory, Error);
            return Directory;
        }
    }  // namespace

    Environment Environment::Default() {
        const std::filesystem::path LocalAppData = KnownFolder(FOLDERID_LocalAppData);
        const std::filesystem::path Kontakt      = LocalAppData / L"Native Instruments" / L"Kontakt 8";
        return Environment {
          .Registry =
            {
              {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Native Instruments"},
              {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Native Instruments"},
            },
          .ServiceCenter    = KnownFolder(FOLDERID_ProgramFilesCommonX64) / L"Native Instruments" / L"Service Center",
          .LibrariesCache   = Kontakt / L"LibrariesCache",
          .KompleteDatabase = Kontakt / L"komplete.db3",
          .Ras3    = KnownFolder(FOLDERID_PublicDocuments) / L"Native Instruments" / L"Native Access" / L"ras3",
          .Backups = BackupDirectory(),
        };
    }

    std::filesystem::path AppDataDirectory() {
        return Ensure(KnownFolder(FOLDERID_LocalAppData) / L"K8Tool");
    }

    std::filesystem::path LogDirectory() {
        return Ensure(AppDataDirectory() / L"Logs");
    }

    std::filesystem::path BackupDirectory() {
        return Ensure(AppDataDirectory() / L"Backups");
    }

    std::filesystem::path ExportDirectory() {
        return Ensure(AppDataDirectory() / L"Exports");
    }
}  // namespace K8
