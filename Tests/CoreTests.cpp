#include "Core/Export.hpp"
#include "Core/Files.hpp"
#include "Core/Format.hpp"
#include "Core/ServiceCenter.hpp"
#include "Core/Text.hpp"
#include "Core/Update.hpp"

#include <doctest/doctest.h>

using namespace K8;

TEST_CASE("Versions parse with or without a v prefix and compare numerically") {
    CHECK(Version::Parse("v3.0.0") == Version {3, 0, 0});
    CHECK(Version::Parse("2.10.1") == Version {2, 10, 1});
    CHECK(Version::Parse("4") == Version {4, 0, 0});
    CHECK(Version::Parse(" v1.2 ") == Version {1, 2, 0});
    CHECK_FALSE(Version::Parse("latest"));
    CHECK_FALSE(Version::Parse(""));
    CHECK(Version {2, 10, 0} > Version {2, 9, 9});
    CHECK(Version {3, 0, 0} == *Version::Parse("v3.0.0"));
}

TEST_CASE("Release tags are read from GitHub's JSON") {
    CHECK(ParseReleaseTag(R"({"url":"x","tag_name": "v2.0.0","name":"K8Tool 2"})") == "v2.0.0");
    CHECK_FALSE(ParseReleaseTag(R"({"message":"Not Found"})"));
}

TEST_CASE("Product hints give the SNPID of the named product and tolerate malformed entries") {
    const std::string Xml = R"(<?xml version="1.0"?>
<ProductHints>
  <Product><Name>Broken Without SNPID</Name></Product>
  <Product><SNPID>999</SNPID></Product>
  <Product><Name> Action Strings 2 </Name><SNPID>K23</SNPID></Product>
  <Product><Name>The Orchestra Complete</Name><SNPID>Q92</SNPID></Product>
</ProductHints>)";
    CHECK(ParseSnpid(Xml, L"Action Strings 2") == L"K23");
    CHECK(ParseSnpid(Xml, L"the orchestra complete") == L"Q92");
    CHECK_FALSE(ParseSnpid(Xml, L"Broken Without SNPID"));
    CHECK_FALSE(ParseSnpid(Xml, L"Missing"));
    CHECK_FALSE(ParseSnpid("<not xml", L"Action Strings 2"));
    CHECK_FALSE(ParseSnpid("<Other><Product/></Other>", L"Action Strings 2"));
}

TEST_CASE("Cache files match on the K-prefixed SNPID only") {
    CHECK(Files::IsCacheFileFor(L"KK231784788114.cache", L"K23"));
    CHECK(Files::IsCacheFileFor(L"K4371315386310.cache", L"437"));
    CHECK_FALSE(Files::IsCacheFileFor(L"K4371315386310.cache", L"371"));
    CHECK_FALSE(Files::IsCacheFileFor(L"KQ921776982068.cache", L"K23"));
    CHECK_FALSE(Files::IsCacheFileFor(L"K4371315386310.cache", L""));
}

TEST_CASE("License tokens match the SNPID as a whole part of the name") {
    CHECK(Files::IsLicenseFileFor(L"K23.jwt", L"K23"));
    CHECK(Files::IsLicenseFileFor(L"k23_user.JWT", L"K23"));
    CHECK(Files::IsLicenseFileFor(L"user-012-token.jwt", L"012"));
    CHECK_FALSE(Files::IsLicenseFileFor(L"AK231.jwt", L"K23"));
    CHECK_FALSE(Files::IsLicenseFileFor(L"1012.jwt", L"012"));
    CHECK_FALSE(Files::IsLicenseFileFor(L"K23.txt", L"K23"));
}

TEST_CASE("CSV export quotes fields that need it and keeps raw sizes") {
    const std::vector<Library> Libraries {
      Library {.Name = L"Plain", .ContentDir = L"D:\\Libs\\Plain", .ContentExists = true, .Size = 2048},
      Library {.Name = L"Comma, \"Quoted\"", .ContentDir = L"D:\\Libs\\Gone"},
    };
    const std::string Csv = LibrariesToCsv(Libraries);
    CHECK(Csv == "Name,Content Directory,Status,Size,Size (bytes)\r\n"
                 "Plain,D:\\Libs\\Plain,OK,2.0 KB,2048\r\n"
                 "\"Comma, \"\"Quoted\"\"\",D:\\Libs\\Gone,Folder missing,,\r\n");
}

TEST_CASE("Sizes use binary units with one decimal") {
    CHECK(FormatSize(0) == "0 B");
    CHECK(FormatSize(1023) == "1023 B");
    CHECK(FormatSize(1536) == "1.5 KB");
    CHECK(FormatSize(5ull * 1024 * 1024 * 1024) == "5.0 GB");
}

TEST_CASE("Text helpers round-trip UTF-8 and compare without case") {
    CHECK(FromUtf8(ToUtf8(L"Café Ström")) == L"Café Ström");
    CHECK(EqualsNoCase(L"Kontakt 8", L"KONTAKT 8"));
    CHECK(StartsWithNoCase(L"kk231.cache", L"KK23"));
    CHECK(Trim(std::wstring_view(L"  name \r\n")) == L"name");
}
