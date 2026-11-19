#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace K8 {
    /// @brief A product's entry in Native Instruments' Service Center product hints.
    struct ProductHint {
        /// @brief Product ID, such as "K23". Cache files and license tokens are named after it.
        std::wstring Snpid;
        /// @brief The XML file the entry was found in.
        std::filesystem::path Source;
        /// @brief Whether Source is a file for this product alone, as opposed to the shared NativeAccess.xml.
        bool OwnFile = false;
    };

    /// @brief Looks for Name in <ServiceCenter>\<Name>.xml, then in NativeAccess.xml.
    [[nodiscard]] std::optional<ProductHint> FindProduct(const std::filesystem::path& ServiceCenter,
                                                         std::wstring_view Name);

    /// @brief The SNPID of the <Product> named Name in a ProductHints document. Tolerates missing elements.
    [[nodiscard]] std::optional<std::wstring> ParseSnpid(std::string_view Xml, std::wstring_view Name);
}  // namespace K8
