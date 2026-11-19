#include "Core/Export.hpp"
#include "Core/Format.hpp"
#include "Core/Log.hpp"
#include "Core/Text.hpp"

#include <fstream>

namespace K8 {
    namespace {
        std::string Field(std::string_view Text) {
            if (Text.find_first_of(",\"\r\n") == std::string_view::npos) {
                return std::string(Text);
            }
            std::string Result = "\"";
            for (const char Character : Text) {
                if (Character == '"') {
                    Result += '"';
                }
                Result += Character;
            }
            return Result + "\"";
        }

        std::string StatusOf(const Library& Entry) {
            if (Entry.ContentDir.empty()) {
                return "No content folder";
            }
            return Entry.ContentExists ? "OK" : "Folder missing";
        }
    }  // namespace

    std::string LibrariesToCsv(std::span<const Library> Libraries) {
        std::string Csv = "Name,Content Directory,Status,Size,Size (bytes)\r\n";
        for (const Library& Entry : Libraries) {
            Csv += Field(ToUtf8(Entry.Name)) + ",";
            Csv += Field(ToUtf8(Entry.ContentDir.wstring())) + ",";
            Csv += Field(StatusOf(Entry)) + ",";
            Csv += (Entry.Size ? Field(FormatSize(*Entry.Size)) : std::string()) + ",";
            Csv += (Entry.Size ? std::to_string(*Entry.Size) : std::string()) + "\r\n";
        }
        return Csv;
    }

    bool ExportLibraries(std::span<const Library> Libraries, const std::filesystem::path& File, std::string& Error) {
        std::ofstream Out(File, std::ios::binary | std::ios::trunc);
        const std::string Csv = LibrariesToCsv(Libraries);
        Out << "\xEF\xBB\xBF" << Csv;
        if (!Out) {
            Error = "Could not write " + ToUtf8(File.wstring());
            return false;
        }
        Log::Info("Exported {} libraries to {}", Libraries.size(), ToUtf8(File.wstring()));
        return true;
    }
}  // namespace K8
