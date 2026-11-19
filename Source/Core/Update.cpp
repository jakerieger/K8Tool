#include "Core/Update.hpp"
#include "Core/Constants.hpp"
#include "Core/Text.hpp"

#include <Windows.h>
#include <winhttp.h>

#include <charconv>
#include <format>

namespace K8 {
    namespace {
        class Handle {
        public:
            explicit Handle(HINTERNET Value) : _Value(Value) {}

            ~Handle() {
                if (_Value != nullptr) {
                    WinHttpCloseHandle(_Value);
                }
            }

            Handle(const Handle&)            = delete;
            Handle& operator=(const Handle&) = delete;

            [[nodiscard]] HINTERNET Get() const {
                return _Value;
            }

            explicit operator bool() const {
                return _Value != nullptr;
            }

        private:
            HINTERNET _Value;
        };

        std::string LastError(std::string_view What) {
            return std::format("{} failed (error {})", What, GetLastError());
        }
    }  // namespace

    std::optional<Version> Version::Parse(std::string_view Text) {
        Text = Trim(Text);
        if (!Text.empty() && (Text.front() == 'v' || Text.front() == 'V')) {
            Text.remove_prefix(1);
        }
        Version Result;
        int* Parts[]       = {&Result.Major, &Result.Minor, &Result.Patch};
        const char* Cursor = Text.data();
        const char* End    = Text.data() + Text.size();
        for (size_t i = 0; i < 3; ++i) {
            const auto [Next, Error] = std::from_chars(Cursor, End, *Parts[i]);
            if (Error != std::errc {}) {
                return i == 0 ? std::nullopt : std::optional(Result);
            }
            Cursor = Next;
            if (Cursor == End || *Cursor != '.') {
                break;
            }
            ++Cursor;
        }
        return Result;
    }

    std::string Version::ToString() const {
        return std::format("{}.{}.{}", Major, Minor, Patch);
    }

    std::optional<std::string> ParseReleaseTag(std::string_view Json) {
        constexpr std::string_view kKey = "\"tag_name\"";
        size_t At                       = Json.find(kKey);
        if (At == std::string_view::npos) {
            return std::nullopt;
        }
        At = Json.find(':', At + kKey.size());
        if (At == std::string_view::npos) {
            return std::nullopt;
        }
        const size_t Open = Json.find('"', At + 1);
        if (Open == std::string_view::npos) {
            return std::nullopt;
        }
        const size_t Close = Json.find('"', Open + 1);
        if (Close == std::string_view::npos) {
            return std::nullopt;
        }
        return std::string(Json.substr(Open + 1, Close - Open - 1));
    }

    std::optional<UpdateInfo> FetchLatestRelease(std::string& Error) {
        const Handle Session(WinHttpOpen(L"K8Tool",
                                         WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0));
        if (!Session) {
            Error = LastError("WinHttpOpen");
            return std::nullopt;
        }
        WinHttpSetTimeouts(Session.Get(), 5000, 5000, 10000, 10000);
        const Handle Connection(WinHttpConnect(Session.Get(), L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!Connection) {
            Error = LastError("WinHttpConnect");
            return std::nullopt;
        }
        const std::wstring Path = FromUtf8(std::format("/repos/{}/{}/releases/latest", kRepoOwner, kRepoName));
        const Handle Request(WinHttpOpenRequest(Connection.Get(),
                                                L"GET",
                                                Path.c_str(),
                                                nullptr,
                                                WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE));
        if (!Request) {
            Error = LastError("WinHttpOpenRequest");
            return std::nullopt;
        }
        constexpr wchar_t kHeaders[] = L"Accept: application/vnd.github+json\r\n";
        if (!WinHttpSendRequest(Request.Get(), kHeaders, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(Request.Get(), nullptr)) {
            Error = LastError("Contacting GitHub");
            return std::nullopt;
        }

        DWORD Status = 0;
        DWORD Size   = sizeof(Status);
        WinHttpQueryHeaders(Request.Get(),
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &Status,
                            &Size,
                            WINHTTP_NO_HEADER_INDEX);
        if (Status != 200) {
            Error = std::format("GitHub answered with HTTP {}", Status);
            return std::nullopt;
        }

        std::string Body;
        for (;;) {
            DWORD Available = 0;
            if (!WinHttpQueryDataAvailable(Request.Get(), &Available)) {
                Error = LastError("Reading the response");
                return std::nullopt;
            }
            if (Available == 0) {
                break;
            }
            const size_t Offset = Body.size();
            Body.resize(Offset + Available);
            DWORD Read = 0;
            if (!WinHttpReadData(Request.Get(), Body.data() + Offset, Available, &Read)) {
                Error = LastError("Reading the response");
                return std::nullopt;
            }
            Body.resize(Offset + Read);
        }

        const std::optional<std::string> Tag = ParseReleaseTag(Body);
        const std::optional<Version> Latest  = Tag ? Version::Parse(*Tag) : std::nullopt;
        if (!Latest) {
            Error = "The release information from GitHub could not be read";
            return std::nullopt;
        }
        return UpdateInfo {
          .Latest = *Latest,
          .Tag    = *Tag,
          .Url    = std::format("https://github.com/{}/{}/releases/tag/{}", kRepoOwner, kRepoName, *Tag),
        };
    }
}  // namespace K8
