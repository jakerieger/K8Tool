#include "Core/Text.hpp"

#include <Windows.h>

namespace K8 {
    std::string ToUtf8(std::wstring_view Text) {
        if (Text.empty()) {
            return {};
        }
        const int Length =
          WideCharToMultiByte(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), nullptr, 0, nullptr, nullptr);
        std::string Result(static_cast<size_t>(Length), '\0');
        WideCharToMultiByte(CP_UTF8,
                            0,
                            Text.data(),
                            static_cast<int>(Text.size()),
                            Result.data(),
                            Length,
                            nullptr,
                            nullptr);
        return Result;
    }

    std::wstring FromUtf8(std::string_view Text) {
        if (Text.empty()) {
            return {};
        }
        const int Length = MultiByteToWideChar(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), nullptr, 0);
        std::wstring Result(static_cast<size_t>(Length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, Text.data(), static_cast<int>(Text.size()), Result.data(), Length);
        return Result;
    }

    bool EqualsNoCase(std::wstring_view Left, std::wstring_view Right) {
        return CompareStringOrdinal(Left.data(),
                                    static_cast<int>(Left.size()),
                                    Right.data(),
                                    static_cast<int>(Right.size()),
                                    TRUE) == CSTR_EQUAL;
    }

    bool StartsWithNoCase(std::wstring_view Text, std::wstring_view Prefix) {
        return Text.size() >= Prefix.size() && EqualsNoCase(Text.substr(0, Prefix.size()), Prefix);
    }

    std::wstring_view Trim(std::wstring_view Text) {
        constexpr std::wstring_view kSpace = L" \t\r\n";
        const size_t First                 = Text.find_first_not_of(kSpace);
        if (First == std::wstring_view::npos) {
            return {};
        }
        return Text.substr(First, Text.find_last_not_of(kSpace) - First + 1);
    }

    std::string_view Trim(std::string_view Text) {
        constexpr std::string_view kSpace = " \t\r\n";
        const size_t First                = Text.find_first_not_of(kSpace);
        if (First == std::string_view::npos) {
            return {};
        }
        return Text.substr(First, Text.find_last_not_of(kSpace) - First + 1);
    }
}  // namespace K8
