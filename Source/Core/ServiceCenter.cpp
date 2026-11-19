#include "Core/ServiceCenter.hpp"
#include "Core/Log.hpp"
#include "Core/Text.hpp"

#include <tinyxml2.h>

#include <fstream>
#include <sstream>

namespace K8 {
    namespace {
        std::optional<std::string> ReadFile(const std::filesystem::path& File) {
            std::ifstream In(File, std::ios::binary);
            if (!In) {
                return std::nullopt;
            }
            std::ostringstream Contents;
            Contents << In.rdbuf();
            return std::move(Contents).str();
        }

        std::wstring_view TextOf(const tinyxml2::XMLElement* Parent, const char* Child, std::wstring& Storage) {
            const tinyxml2::XMLElement* Element = Parent->FirstChildElement(Child);
            const char* Text                    = Element != nullptr ? Element->GetText() : nullptr;
            Storage                             = Text != nullptr ? FromUtf8(Text) : std::wstring();
            return Trim(Storage);
        }

        std::optional<ProductHint> Search(const std::filesystem::path& File, std::wstring_view Name, bool OwnFile) {
            std::error_code Error;
            if (!std::filesystem::is_regular_file(File, Error)) {
                return std::nullopt;
            }
            const std::optional<std::string> Xml = ReadFile(File);
            if (!Xml) {
                Log::Warn("Could not read {}", ToUtf8(File.wstring()));
                return std::nullopt;
            }
            std::optional<std::wstring> Snpid = ParseSnpid(*Xml, Name);
            if (!Snpid) {
                return std::nullopt;
            }
            return ProductHint {.Snpid = std::move(*Snpid), .Source = File, .OwnFile = OwnFile};
        }
    }  // namespace

    std::optional<std::wstring> ParseSnpid(std::string_view Xml, std::wstring_view Name) {
        tinyxml2::XMLDocument Document;
        if (Document.Parse(Xml.data(), Xml.size()) != tinyxml2::XML_SUCCESS) {
            return std::nullopt;
        }
        const tinyxml2::XMLElement* Root = Document.FirstChildElement("ProductHints");
        if (Root == nullptr) {
            return std::nullopt;
        }
        const std::wstring_view Wanted = Trim(Name);
        for (const tinyxml2::XMLElement* Product = Root->FirstChildElement("Product"); Product != nullptr;
             Product                             = Product->NextSiblingElement("Product")) {
            std::wstring NameText, SnpidText;
            if (!EqualsNoCase(TextOf(Product, "Name", NameText), Wanted)) {
                continue;
            }
            const std::wstring_view Snpid = TextOf(Product, "SNPID", SnpidText);
            if (!Snpid.empty()) {
                return std::wstring(Snpid);
            }
        }
        return std::nullopt;
    }

    std::optional<ProductHint> FindProduct(const std::filesystem::path& ServiceCenter, std::wstring_view Name) {
        if (std::optional<ProductHint> Own = Search(ServiceCenter / (std::wstring(Name) + L".xml"), Name, true)) {
            return Own;
        }
        return Search(ServiceCenter / L"NativeAccess.xml", Name, false);
    }
}  // namespace K8
