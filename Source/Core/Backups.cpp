#include "Core/Backups.hpp"
#include "Core/Log.hpp"
#include "Core/Text.hpp"

#include <zip_file.hpp>

#include <fstream>
#include <sstream>

namespace K8 {
    namespace fs = std::filesystem;

    std::optional<BackupArchive> CollectBackups(const fs::path& Backups, const fs::path& Archive, std::string& Error) {
        miniz_cpp::zip_file Zip;
        size_t Entries = 0;
        std::error_code Code;
        for (fs::directory_iterator It(Backups, Code), End; !Code && It != End; It.increment(Code)) {
            std::error_code EntryError;
            if (!It->is_regular_file(EntryError)) {
                continue;
            }
            const std::wstring Extension = It->path().extension().wstring();
            if (!EqualsNoCase(Extension, L".reg") && !EqualsNoCase(Extension, L".bak")) {
                continue;
            }
            std::ifstream In(It->path(), std::ios::binary);
            std::ostringstream Bytes;
            Bytes << In.rdbuf();
            if (!In) {
                Log::Warn("Skipped unreadable backup {}", ToUtf8(It->path().wstring()));
                continue;
            }
            // Archive names are stored as UTF-8 and read back as such by Explorer and 7-Zip.
            Zip.writestr(ToUtf8(It->path().filename().wstring()), Bytes.str());
            ++Entries;
        }
        if (Entries == 0) {
            Error = "There are no backups to collect yet. Backups are made when you remove a library.";
            return std::nullopt;
        }

        fs::create_directories(Archive.parent_path(), Code);
        std::ofstream Out(Archive, std::ios::binary | std::ios::trunc);
        Zip.save(Out);
        if (!Out) {
            Error = "Could not write " + ToUtf8(Archive.wstring());
            return std::nullopt;
        }
        Log::Info("Collected {} backups into {}", Entries, ToUtf8(Archive.wstring()));
        return BackupArchive {.File = Archive, .Entries = Entries};
    }
}  // namespace K8
