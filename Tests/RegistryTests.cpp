#include "TestEnvironment.hpp"

#include "Core/Registry.hpp"
#include "Core/Scanner.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <sstream>

using namespace K8;
using K8::Testing::FakeInstall;

TEST_CASE("Scanning lists libraries, merges both registry views and keeps entries whose folder is gone") {
    FakeInstall Install;
    const auto Strings = Install.AddLibrary(L"Action Strings 2", "K23");
    Install.AddKey(L"Action Strings 2", L"", 1);
    Install.AddKey(L"Orphaned Library", (Install.Root / L"Nowhere").wstring());
    Install.AddKey(L"Kontakt 8", (Install.Root / L"Libraries").wstring());
    Install.AddKey(L"No Content Dir", L"");

    const std::vector<Library> Libraries = ScanLibraries(Install.Where);
    REQUIRE(Libraries.size() == 3);
    CHECK(Libraries[0].Name == L"Action Strings 2");
    CHECK(Libraries[0].Keys.size() == 2);
    CHECK(Libraries[0].ContentExists);
    CHECK(Libraries[0].ContentDir == Strings);
    CHECK(Libraries[1].Name == L"No Content Dir");
    CHECK(Libraries[1].ContentDir.empty());
    CHECK(Libraries[2].Name == L"Orphaned Library");
    CHECK_FALSE(Libraries[2].ContentExists);

    CHECK(ComputeSize(Strings) == 4096 + 3);
}

TEST_CASE("Long and expandable ContentDir values are read in full") {
    FakeInstall Install;
    const RegistryKey Key = Install.AddKey(L"Long", L"");
    const std::wstring Long(700, L'x');
    std::wstring Error;
    REQUIRE(Registry::WriteString(Key, L"ContentDir", Long, Error));
    CHECK(Registry::ReadString(Key, L"ContentDir") == Long);

    HKEY Handle = nullptr;
    RegOpenKeyExW(Key.Root, Key.Path.c_str(), 0, KEY_SET_VALUE, &Handle);
    const std::wstring Expandable = L"%SystemRoot%\\Libs";
    RegSetValueExW(Handle,
                   L"Expand",
                   0,
                   REG_EXPAND_SZ,
                   reinterpret_cast<const BYTE*>(Expandable.c_str()),
                   static_cast<DWORD>((Expandable.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(Handle);
    const std::optional<std::wstring> Expanded = Registry::ReadString(Key, L"Expand");
    REQUIRE(Expanded);
    CHECK(Expanded->find(L'%') == std::wstring::npos);
    CHECK(Expanded->ends_with(L"\\Libs"));
}

TEST_CASE("Registry exports are .reg files that reg import restores") {
    FakeInstall Install;
    const RegistryKey Key            = Install.AddKey(L"Quote \"Lib\"", L"C:\\Libs\\Quote \"Lib\"");
    const std::filesystem::path File = Install.Root / L"export.reg";
    std::wstring Error;
    REQUIRE(Registry::ExportToFile(Key, File, Error));

    std::ifstream In(File, std::ios::binary);
    std::ostringstream Raw;
    Raw << In.rdbuf();
    const std::string Bytes = Raw.str();
    REQUIRE(Bytes.size() > 2);
    CHECK(static_cast<unsigned char>(Bytes[0]) == 0xFF);
    const std::wstring Text(reinterpret_cast<const wchar_t*>(Bytes.data() + 2), (Bytes.size() - 2) / 2);
    CHECK(Text.starts_with(L"Windows Registry Editor Version 5.00"));
    CHECK(Text.find(L"\"ContentDir\"=\"C:\\\\Libs\\\\Quote \\\"Lib\\\"\"") != std::wstring::npos);
    CHECK(Text.find(L"\"Number\"=dword:0000002a") != std::wstring::npos);
    CHECK(Text.find(L"\\Settings]") != std::wstring::npos);

    REQUIRE(Registry::DeleteTree(Key, Error));
    CHECK_FALSE(Registry::Exists(Key));

    STARTUPINFOW Startup {.cb = sizeof(STARTUPINFOW)};
    PROCESS_INFORMATION Process {};
    std::wstring Command = L"reg.exe import \"" + File.wstring() + L"\"";
    REQUIRE(CreateProcessW(nullptr,
                           Command.data(),
                           nullptr,
                           nullptr,
                           FALSE,
                           CREATE_NO_WINDOW,
                           nullptr,
                           nullptr,
                           &Startup,
                           &Process));
    WaitForSingleObject(Process.hProcess, 30000);
    DWORD Exit = 1;
    GetExitCodeProcess(Process.hProcess, &Exit);
    CloseHandle(Process.hThread);
    CloseHandle(Process.hProcess);
    CHECK(Exit == 0);
    CHECK(Registry::ReadString(Key, L"ContentDir") == L"C:\\Libs\\Quote \"Lib\"");
    CHECK(Registry::Exists({Key.Root, Key.Path + L"\\Settings"}));
}

TEST_CASE("Deleting a key that is already gone succeeds") {
    FakeInstall Install;
    std::wstring Error;
    CHECK(Registry::DeleteTree({HKEY_CURRENT_USER, Install.KeyRoot + L"\\Native Instruments\\Never Existed"}, Error));
    CHECK(Error.empty());
}
