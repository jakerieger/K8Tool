#include "Core/Files.hpp"
#include "Core/Format.hpp"
#include "Core/Text.hpp"

#include <Windows.h>

#include <cwctype>
#include <format>

namespace K8::Files {
    namespace fs = std::filesystem;

    namespace {
        std::wstring Describe(const std::error_code& Code) {
            return FromUtf8(Code.message());
        }

        /// @brief The path as Windows resolves it, for comparisons.
        std::wstring Resolved(const fs::path& Path) {
            std::error_code Error;
            fs::path Result = fs::weakly_canonical(fs::absolute(Path, Error), Error);
            if (Error) {
                Result = Path.lexically_normal();
            }
            std::wstring Text = Result.wstring();
            while (Text.size() > 3 && (Text.back() == L'\\' || Text.back() == L'/')) {
                Text.pop_back();
            }
            return Text;
        }

        std::wstring VolumeOf(const fs::path& Path) {
            wchar_t Volume[MAX_PATH + 1] = {};
            if (!GetVolumePathNameW(Resolved(Path).c_str(), Volume, MAX_PATH)) {
                return {};
            }
            return Volume;
        }

        struct TreeSize {
            std::uintmax_t Files = 0;
            std::uintmax_t Bytes = 0;
        };

        bool Measure(const fs::path& Root, TreeSize& Size, std::wstring& Error) {
            std::error_code Code;
            for (fs::recursive_directory_iterator It(Root, Code), End; It != End; It.increment(Code)) {
                if (Code) {
                    break;
                }
                if (It->is_regular_file(Code)) {
                    ++Size.Files;
                    Size.Bytes += It->file_size(Code);
                }
                if (Code) {
                    break;
                }
            }
            if (Code) {
                Error = std::format(L"Could not read {}: {}", Root.wstring(), Describe(Code));
                return false;
            }
            return true;
        }
    }  // namespace

    bool IsCacheFileFor(const fs::path& File, std::wstring_view Snpid) {
        if (Snpid.empty()) {
            return false;
        }
        return StartsWithNoCase(File.filename().wstring(), L"K" + std::wstring(Snpid));
    }

    bool IsLicenseFileFor(const fs::path& File, std::wstring_view Snpid) {
        if (Snpid.empty() || !EqualsNoCase(File.extension().wstring(), L".jwt")) {
            return false;
        }
        const std::wstring Stem = File.stem().wstring();
        size_t Start            = 0;
        while (Start <= Stem.size()) {
            size_t End = Start;
            while (End < Stem.size() && std::iswalnum(Stem[End])) {
                ++End;
            }
            if (EqualsNoCase(std::wstring_view(Stem).substr(Start, End - Start), Snpid)) {
                return true;
            }
            Start = End + 1;
        }
        return false;
    }

    bool Delete(const fs::path& Target, std::wstring& Error) {
        std::error_code Code;
        if (!fs::exists(fs::symlink_status(Target, Code))) {
            return true;
        }
        fs::remove_all(Target, Code);
        if (Code) {
            Error = std::format(L"Could not delete {}: {}", Target.wstring(), Describe(Code));
            return false;
        }
        return true;
    }

    bool IsSameOrInside(const fs::path& Inner, const fs::path& Outer) {
        const std::wstring InnerText = Resolved(Inner);
        const std::wstring OuterText = Resolved(Outer);
        if (EqualsNoCase(InnerText, OuterText)) {
            return true;
        }
        return InnerText.size() > OuterText.size() && StartsWithNoCase(InnerText, OuterText) &&
               (InnerText[OuterText.size()] == L'\\' || OuterText.back() == L'\\');
    }

    bool OnSameVolume(const fs::path& Left, const fs::path& Right) {
        const std::wstring LeftVolume = VolumeOf(Left);
        return !LeftVolume.empty() && EqualsNoCase(LeftVolume, VolumeOf(Right));
    }

    bool CopyTree(const fs::path& Source,
                  const fs::path& Destination,
                  const ProgressSink& Progress,
                  std::stop_token Stop,
                  std::wstring& Error) {
        TreeSize Expected;
        if (!Measure(Source, Expected, Error)) {
            return false;
        }

        std::error_code Code;
        if (fs::exists(Destination, Code)) {
            Error = std::format(L"{} already exists", Destination.wstring());
            return false;
        }
        const auto Fail = [&](std::wstring Message) {
            Error = std::move(Message);
            std::error_code Ignored;
            fs::remove_all(Destination, Ignored);
            return false;
        };

        fs::create_directories(Destination, Code);
        if (Code) {
            return Fail(std::format(L"Could not create {}: {}", Destination.wstring(), Describe(Code)));
        }

        std::uintmax_t Copied = 0;
        for (fs::recursive_directory_iterator It(Source, Code), End; It != End; It.increment(Code)) {
            if (Code) {
                break;
            }
            if (Stop.stop_requested()) {
                return Fail(L"Cancelled");
            }
            const fs::path Target = Destination / fs::relative(It->path(), Source, Code);
            if (Code) {
                break;
            }
            if (It->is_directory(Code)) {
                fs::create_directories(Target, Code);
            } else if (It->is_regular_file(Code)) {
                fs::copy_file(It->path(), Target, fs::copy_options::none, Code);
                if (!Code) {
                    Copied += It->file_size(Code);
                    if (Progress && Expected.Bytes > 0) {
                        Progress(
                          {.Fraction =
                             static_cast<float>(static_cast<double>(Copied) / static_cast<double>(Expected.Bytes)),
                           .Message = std::format("Copying {} of {}", FormatSize(Copied), FormatSize(Expected.Bytes))});
                    }
                }
            }
            if (Code) {
                return Fail(std::format(L"Could not copy {}: {}", It->path().wstring(), Describe(Code)));
            }
        }
        if (Code) {
            return Fail(std::format(L"Could not read {}: {}", Source.wstring(), Describe(Code)));
        }

        TreeSize Actual;
        if (!Measure(Destination, Actual, Error)) {
            return Fail(Error);
        }
        if (Actual.Files != Expected.Files || Actual.Bytes != Expected.Bytes) {
            return Fail(std::format(L"The copy is incomplete: {} of {} files, {} of {} bytes",
                                    Actual.Files,
                                    Expected.Files,
                                    Actual.Bytes,
                                    Expected.Bytes));
        }
        return true;
    }
}  // namespace K8::Files
