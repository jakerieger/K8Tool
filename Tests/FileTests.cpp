#include "TestEnvironment.hpp"

#include "Core/Backups.hpp"
#include "Core/Files.hpp"
#include "Core/Registry.hpp"
#include "Core/Relocation.hpp"
#include "Core/Removal.hpp"
#include "Core/Scanner.hpp"

#include <doctest/doctest.h>

#include <algorithm>

using namespace K8;
using K8::Testing::FakeInstall;
namespace fs = std::filesystem;

namespace {
    Library Find(const Environment& Where, std::wstring_view Name) {
        for (Library& Entry : ScanLibraries(Where)) {
            if (Entry.Name == Name) {
                return Entry;
            }
        }
        FAIL("Library not found");
        return {};
    }

    size_t CountFiles(const fs::path& Directory, std::wstring_view Extension) {
        size_t Count = 0;
        std::error_code Error;
        for (const auto& Entry : fs::directory_iterator(Directory, Error)) {
            if (Entry.path().extension() == Extension) {
                ++Count;
            }
        }
        return Count;
    }
}  // namespace

TEST_CASE("Removing libraries deletes every trace, backs up the registry and resets the database once") {
    FakeInstall Install;
    const fs::path Strings   = Install.AddLibrary(L"Action Strings 2", "K23");
    const fs::path Orchestra = Install.AddLibrary(L"The Orchestra Complete", "Q92");
    const fs::path Keeper    = Install.AddLibrary(L"Keeper", "123");
    FakeInstall::WriteFile(Install.Where.ServiceCenter / L"NativeAccess.xml",
                           FakeInstall::ProductHints(L"Other", "777"));

    const std::vector<Library> Targets {Find(Install.Where, L"Action Strings 2"),
                                        Find(Install.Where, L"The Orchestra Complete")};
    std::vector<ProgressUpdate> Updates;
    const RemovalReport Report = RemoveLibraries(Install.Where,
                                                 Targets,
                                                 {.BackupRegistry = true, .DeleteContent = true},
                                                 [&](const ProgressUpdate& Update) { Updates.push_back(Update); });

    REQUIRE(Report.Libraries.size() == 2);
    for (const LibraryReport& Library : Report.Libraries) {
        CHECK(Library.Removed);
        CHECK_FALSE(Library.HasFailures());
    }
    CHECK_FALSE(Report.Cancelled);
    CHECK_FALSE(fs::exists(Strings));
    CHECK_FALSE(fs::exists(Orchestra));
    CHECK_FALSE(fs::exists(Install.Where.ServiceCenter / L"Action Strings 2.xml"));
    CHECK(fs::exists(Install.Where.ServiceCenter / L"NativeAccess.xml"));
    CHECK_FALSE(fs::exists(Install.Where.Ras3 / L"K23.jwt"));
    CHECK_FALSE(fs::exists(Install.Where.KompleteDatabase));
    CHECK(CountFiles(Install.Where.Backups, L".reg") == 2);
    CHECK(CountFiles(Install.Where.Backups, L".bak") == 1);

    // Everything belonging to the untouched library is still there.
    CHECK(fs::exists(Keeper));
    CHECK(fs::exists(Install.Where.ServiceCenter / L"Keeper.xml"));
    CHECK(fs::exists(Install.Where.Ras3 / L"123.jwt"));
    CHECK(fs::exists(Install.Where.LibrariesCache / L"K1231784788114.cache"));
    CHECK(CountFiles(Install.Where.LibrariesCache, L".cache") == 1);

    const std::vector<Library> Remaining = ScanLibraries(Install.Where);
    REQUIRE(Remaining.size() == 1);
    CHECK(Remaining[0].Name == L"Keeper");

    REQUIRE_FALSE(Updates.empty());
    CHECK(Updates.back().Fraction == 1.0f);
}

TEST_CASE("Libraries whose folder is gone and that only appear in NativeAccess.xml can still be removed") {
    FakeInstall Install;
    Install.AddKey(L"Orphan", (Install.Root / L"Gone").wstring());
    FakeInstall::WriteFile(Install.Where.ServiceCenter / L"NativeAccess.xml",
                           FakeInstall::ProductHints(L"Orphan", "555"));
    FakeInstall::WriteFile(Install.Where.LibrariesCache / L"K5550001.cache", "cache");

    const Library Orphan       = Find(Install.Where, L"Orphan");
    const RemovalReport Report = RemoveLibraries(Install.Where, std::span(&Orphan, 1), {.BackupRegistry = false}, {});
    REQUIRE(Report.Libraries.size() == 1);
    CHECK(Report.Libraries[0].Removed);
    CHECK(ScanLibraries(Install.Where).empty());
    CHECK_FALSE(fs::exists(Install.Where.LibrariesCache / L"K5550001.cache"));
    CHECK(fs::exists(Install.Where.ServiceCenter / L"NativeAccess.xml"));
}

TEST_CASE("Content is kept when not asked to delete it, and stopping before the start removes nothing") {
    FakeInstall Install;
    const fs::path Content = Install.AddLibrary(L"Keep Content", "K77");
    const Library Entry    = Find(Install.Where, L"Keep Content");

    std::stop_source Stopped;
    Stopped.request_stop();
    const RemovalReport Cancelled = RemoveLibraries(Install.Where, std::span(&Entry, 1), {}, {}, Stopped.get_token());
    CHECK(Cancelled.Cancelled);
    CHECK(Cancelled.Libraries.empty());
    CHECK(ScanLibraries(Install.Where).size() == 1);

    const RemovalReport Report =
      RemoveLibraries(Install.Where, std::span(&Entry, 1), {.BackupRegistry = false, .DeleteContent = false}, {});
    CHECK(Report.Libraries[0].Removed);
    CHECK(fs::exists(Content / L"Samples" / L"Sample.ncw"));
}

TEST_CASE("Relocation is refused for destinations that would lose or nest the library") {
    FakeInstall Install;
    const fs::path Content = Install.AddLibrary(L"Movable", "M01");
    const Library Entry    = Find(Install.Where, L"Movable");

    CHECK(ValidateRelocation(Entry, L""));
    CHECK(ValidateRelocation(Entry, L"relative\\path"));
    CHECK(ValidateRelocation(Entry, Install.Root / L"Missing"));
    CHECK(ValidateRelocation(Entry, Content));
    CHECK(ValidateRelocation(Entry, Content / L"Samples"));
    CHECK(ValidateRelocation(Entry, Content.parent_path()));

    const fs::path Occupied = Install.Root / L"Occupied";
    fs::create_directories(Occupied / L"Movable");
    CHECK(ValidateRelocation(Entry, Occupied));

    fs::create_directories(Install.Root / L"Free");
    CHECK_FALSE(ValidateRelocation(Entry, Install.Root / L"Free"));

    Library Orphan       = Entry;
    Orphan.ContentExists = false;
    CHECK(ValidateRelocation(Orphan, Install.Root / L"Free"));
}

TEST_CASE("Relocating moves the folder and updates every ContentDir") {
    FakeInstall Install;
    const fs::path Content = Install.AddLibrary(L"Movable", "M01");
    Install.AddKey(L"Movable", Content.wstring(), 1);
    const fs::path Destination = Install.Root / L"New Home";
    fs::create_directories(Destination);

    const RelocationResult Result = RelocateLibrary(Find(Install.Where, L"Movable"), Destination, {});
    REQUIRE(Result.Succeeded);
    CHECK(Result.NewContentDir == Destination / L"Movable");
    CHECK_FALSE(fs::exists(Content));
    CHECK(fs::exists(Destination / L"Movable" / L"Samples" / L"Sample.ncw"));

    const Library Moved = Find(Install.Where, L"Movable");
    CHECK(Moved.ContentDir == Destination / L"Movable");
    CHECK(Moved.ContentExists);
    for (const RegistryKey& Key : Moved.Keys) {
        CHECK(Registry::ReadString(Key, L"ContentDir") == (Destination / L"Movable").wstring());
    }
}

TEST_CASE("Copying a tree checks the result and cleans up when stopped") {
    FakeInstall Install;
    const fs::path Source = Install.AddLibrary(L"Copied", "C01");
    const fs::path Copy   = Install.Root / L"Copy";
    std::wstring Error;
    float Last = 0.0f;
    REQUIRE(Files::CopyTree(
      Source,
      Copy,
      [&](const ProgressUpdate& Update) { Last = Update.Fraction.value_or(Last); },
      {},
      Error));
    CHECK(Last == doctest::Approx(1.0f));
    CHECK(ComputeSize(Copy) == ComputeSize(Source));
    CHECK_FALSE(Files::CopyTree(Source, Copy, {}, {}, Error));

    std::stop_source Stopped;
    Stopped.request_stop();
    CHECK_FALSE(Files::CopyTree(Source, Install.Root / L"Stopped", {}, Stopped.get_token(), Error));
    CHECK_FALSE(fs::exists(Install.Root / L"Stopped"));
}

TEST_CASE("Paths compare by location, not spelling") {
    CHECK(Files::IsSameOrInside(L"C:\\Libs\\Strings\\Samples", L"c:\\libs\\strings"));
    CHECK(Files::IsSameOrInside(L"C:\\Libs\\Strings\\", L"C:\\Libs\\Strings"));
    CHECK_FALSE(Files::IsSameOrInside(L"C:\\Libs\\Strings 2", L"C:\\Libs\\Strings"));
    CHECK(Files::IsSameOrInside(L"C:\\Anything", L"C:\\"));
}

TEST_CASE("Backups are collected into a zip, and an empty folder is reported") {
    FakeInstall Install;
    std::string Error;
    CHECK_FALSE(CollectBackups(Install.Where.Backups, Install.Root / L"empty.zip", Error));
    CHECK_FALSE(Error.empty());

    FakeInstall::WriteFile(Install.Where.Backups / L"Strings-1.reg", "reg");
    FakeInstall::WriteFile(Install.Where.Backups / L"komplete-1.db3.bak", "db");
    FakeInstall::WriteFile(Install.Where.Backups / L"notes.txt", "ignored");
    const std::optional<BackupArchive> Archive =
      CollectBackups(Install.Where.Backups, Install.Root / L"Out" / L"b.zip", Error);
    REQUIRE(Archive);
    CHECK(Archive->Entries == 2);
    CHECK(fs::file_size(Archive->File) > 0);
}
