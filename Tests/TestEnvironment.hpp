#pragma once

#include "Core/Environment.hpp"
#include "Core/Registry.hpp"
#include "Core/Text.hpp"

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace K8::Testing {
    /// @brief A throwaway Native Instruments install: folders under %TEMP% and registry keys under HKCU, both
    /// deleted afterwards. Lets removal and relocation run for real without administrator rights.
    class FakeInstall {
    public:
        FakeInstall() {
            static std::atomic<int> Counter = 0;
            const std::wstring Id           = std::format(L"K8ToolTests-{}-{}", GetCurrentProcessId(), Counter++);
            Root                            = std::filesystem::temp_directory_path() / Id;
            KeyRoot                         = L"Software\\" + Id;
            Where                           = Environment {
              .Registry         = {{HKEY_CURRENT_USER, KeyRoot + L"\\Native Instruments"},
                                   {HKEY_CURRENT_USER, KeyRoot + L"\\WOW6432Node\\Native Instruments"}},
              .ServiceCenter    = Root / L"Service Center",
              .LibrariesCache   = Root / L"LibrariesCache",
              .KompleteDatabase = Root / L"komplete.db3",
              .Ras3             = Root / L"ras3",
              .Backups          = Root / L"Backups",
            };
            for (const auto& Folder : {Where.ServiceCenter, Where.LibrariesCache, Where.Ras3, Root / L"Libraries"}) {
                std::filesystem::create_directories(Folder);
            }
            WriteFile(Where.KompleteDatabase, "database");
        }

        ~FakeInstall() {
            std::error_code Error;
            std::filesystem::remove_all(Root, Error);
            RegDeleteTreeW(HKEY_CURRENT_USER, KeyRoot.c_str());
        }

        FakeInstall(const FakeInstall&)            = delete;
        FakeInstall& operator=(const FakeInstall&) = delete;

        static void WriteFile(const std::filesystem::path& File, std::string_view Contents) {
            std::filesystem::create_directories(File.parent_path());
            std::ofstream(File, std::ios::binary) << Contents;
        }

        /// @brief Adds a library key and, unless ContentDir is empty, its ContentDir value. Source 1 is the
        /// WOW6432Node key.
        RegistryKey AddKey(const std::wstring& Name, const std::wstring& ContentDir, size_t Source = 0) {
            const RegistryKey Key {HKEY_CURRENT_USER, Where.Registry[Source].Path + L"\\" + Name};
            HKEY Handle = nullptr;
            RegCreateKeyExW(Key.Root, Key.Path.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &Handle, nullptr);
            if (!ContentDir.empty()) {
                RegSetValueExW(Handle,
                               L"ContentDir",
                               0,
                               REG_SZ,
                               reinterpret_cast<const BYTE*>(ContentDir.c_str()),
                               static_cast<DWORD>((ContentDir.size() + 1) * sizeof(wchar_t)));
            }
            const DWORD Number = 42;
            RegSetValueExW(Handle, L"Number", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&Number), sizeof(Number));
            HKEY Child = nullptr;
            RegCreateKeyExW(Handle, L"Settings", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &Child, nullptr);
            RegCloseKey(Child);
            RegCloseKey(Handle);
            return Key;
        }

        /// @brief A complete library: content folder with files, registry key, own product hint XML, a cache file
        /// and a license token.
        std::filesystem::path AddLibrary(const std::wstring& Name, const std::string& Snpid) {
            const std::filesystem::path Content = Root / L"Libraries" / Name;
            WriteFile(Content / L"Instruments" / L"Patch.nki", "nki");
            WriteFile(Content / L"Samples" / L"Sample.ncw", std::string(4096, 's'));
            AddKey(Name, Content.wstring());
            WriteFile(Where.ServiceCenter / (Name + L".xml"), ProductHints(Name, Snpid));
            const std::wstring WideId = FromUtf8(Snpid);
            WriteFile(Where.LibrariesCache / (L"K" + WideId + L"1784788114.cache"), "cache");
            WriteFile(Where.Ras3 / (WideId + L".jwt"), "token");
            return Content;
        }

        static std::string ProductHints(const std::wstring& Name, const std::string& Snpid) {
            const std::string Narrow = ToUtf8(Name);
            return std::format(R"(<?xml version="1.0" encoding="UTF-8"?>
<ProductHints spec="1.0"><Product version="1"><Name>{}</Name><SNPID>{}</SNPID></Product></ProductHints>)",
                               Narrow,
                               Snpid);
        }

        std::filesystem::path Root;
        std::wstring KeyRoot;
        Environment Where;
    };
}  // namespace K8::Testing
