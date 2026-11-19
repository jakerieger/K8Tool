#pragma once

#include "Core/Environment.hpp"
#include "Core/Library.hpp"
#include "Core/Update.hpp"

#include <XenUI/XenUI.hpp>

#include <optional>
#include <string>
#include <vector>

namespace K8::Ui {
    /// @brief The library table's columns, in order.
    enum LibraryColumn : int { NameColumn, FolderColumn, SizeColumn, StatusColumn };

    /// @brief Everything the main window shows. Lives on the UI thread; workers change it through App::Post.
    ///
    /// The selection is kept by library name, not by row: rows move when sizes arrive or the sort or filter
    /// changes, and a removal must act on the libraries that were picked, not on whatever now sits in those rows.
    struct AppModel {
        XenUI::Window* MainWindow = nullptr;
        Environment Where         = Environment::Default();

        XenUI::Signal<std::vector<Library>> Libraries;
        XenUI::Signal<bool> Scanning = true;
        XenUI::Signal<std::string> Filter;
        XenUI::Signal<XenUI::TableSort> Sort = XenUI::TableSort {NameColumn, true};
        /// @brief The table's selected rows, which index Visible. Derived from the selected names.
        XenUI::Signal<std::vector<int>> Selection;
        XenUI::Signal<int> Lead = -1;
        /// @brief A newer release, once a check finds one.
        XenUI::Signal<std::optional<UpdateInfo>> Update;
        XenUI::Signal<bool> UpdateDismissed = false;
        /// @brief The log, for the log window.
        XenUI::Signal<std::string> LogText;

        /// @brief Indexes into Libraries of the rows shown, after the filter and sort.
        XenUI::Computed<std::vector<size_t>> Visible {[this] { return ComputeVisible(); }};

        /// @brief The library shown in a table row.
        [[nodiscard]] const Library* RowLibrary(int Row) const;
        [[nodiscard]] std::vector<Library> SelectedLibraries() const;

        /// @brief Records the table's new selection.
        void SelectRows(const std::vector<int>& Rows);
        void SelectLead(int Row);
        void ClearSelection();
        /// @brief Points Selection and Lead at the selected libraries' current rows. Call after anything that
        /// changes Visible.
        void Reselect();

        void SetLibraries(std::vector<Library> All);
        void SetFilter(std::string Text);
        void SetSize(const std::wstring& Name, std::uintmax_t Size);

    private:
        [[nodiscard]] std::vector<size_t> ComputeVisible() const;

        std::vector<std::wstring> _SelectedNames;
        std::wstring _LeadName;
    };

    /// @brief What the Size column shows for a library.
    [[nodiscard]] std::string SizeText(const Library& Entry);
    /// @brief What the Status column shows for a library.
    [[nodiscard]] std::string StatusText(const Library& Entry);
}  // namespace K8::Ui
