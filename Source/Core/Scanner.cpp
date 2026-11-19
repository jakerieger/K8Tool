#include "Core/Scanner.hpp"
#include "Core/Log.hpp"
#include "Core/Registry.hpp"
#include "Core/Text.hpp"

#include <algorithm>

namespace K8 {
    std::vector<Library> ScanLibraries(const Environment& Where) {
        std::vector<Library> Libraries;
        for (const RegistrySource& Source : Where.Registry) {
            const std::optional<std::vector<std::wstring>> Names = Registry::SubkeyNames(Source.Root, Source.Path);
            if (!Names) {
                continue;
            }
            for (const std::wstring& Name : *Names) {
                if (Registry::IsExcludedProduct(Name)) {
                    continue;
                }
                const RegistryKey Key {Source.Root, Source.Path + L"\\" + Name};

                const auto Existing = std::find_if(Libraries.begin(), Libraries.end(), [&](const Library& Entry) {
                    return EqualsNoCase(Entry.Name, Name);
                });
                Library& Entry =
                  Existing != Libraries.end() ? *Existing : Libraries.emplace_back(Library {.Name = Name});
                Entry.Keys.push_back(Key);

                if (!Entry.ContentDir.empty()) {
                    continue;
                }
                const std::optional<std::wstring> ContentDir = Registry::ReadString(Key, L"ContentDir");
                if (!ContentDir || Trim(*ContentDir).empty()) {
                    continue;
                }
                Entry.ContentDir = std::filesystem::path(std::wstring(Trim(*ContentDir))).lexically_normal();
                std::error_code Error;
                Entry.ContentExists = std::filesystem::is_directory(Entry.ContentDir, Error);
                if (!Entry.ContentExists) {
                    Log::Warn("'{}' points to a missing folder: {}", ToUtf8(Name), ToUtf8(Entry.ContentDir.wstring()));
                }
            }
        }
        std::sort(Libraries.begin(), Libraries.end(), [](const Library& Left, const Library& Right) {
            return CompareStringOrdinal(Left.Name.c_str(), -1, Right.Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
        Log::Info("Found {} libraries", Libraries.size());
        return Libraries;
    }

    std::optional<std::uintmax_t> ComputeSize(const std::filesystem::path& Directory, std::stop_token Stop) {
        namespace fs         = std::filesystem;
        std::uintmax_t Total = 0;
        std::error_code Error;
        fs::recursive_directory_iterator It(Directory, fs::directory_options::skip_permission_denied, Error);
        for (const fs::recursive_directory_iterator End; !Error && It != End; It.increment(Error)) {
            if (Stop.stop_requested()) {
                return std::nullopt;
            }
            std::error_code EntryError;
            if (It->is_regular_file(EntryError)) {
                const std::uintmax_t Size = It->file_size(EntryError);
                if (!EntryError) {
                    Total += Size;
                }
            }
        }
        if (Error) {
            Log::Warn("Size of {} may be incomplete: {}", ToUtf8(Directory.wstring()), Error.message());
        }
        return Total;
    }
}  // namespace K8
