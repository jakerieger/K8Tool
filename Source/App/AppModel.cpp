#include "App/AppModel.hpp"
#include "Core/Format.hpp"
#include "Core/Text.hpp"

#include <Windows.h>

#include <algorithm>

namespace K8::Ui {
    namespace {
        bool Contains(std::wstring_view Text, std::wstring_view Needle) {
            return FindNLSStringEx(LOCALE_NAME_USER_DEFAULT,
                                   FIND_FROMSTART | LINGUISTIC_IGNORECASE,
                                   Text.data(),
                                   static_cast<int>(Text.size()),
                                   Needle.data(),
                                   static_cast<int>(Needle.size()),
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   0) >= 0;
        }

        int CompareText(const std::wstring& Left, const std::wstring& Right) {
            return CompareStringEx(LOCALE_NAME_USER_DEFAULT,
                                   LINGUISTIC_IGNORECASE | SORT_DIGITSASNUMBERS,
                                   Left.c_str(),
                                   -1,
                                   Right.c_str(),
                                   -1,
                                   nullptr,
                                   nullptr,
                                   0) -
                   CSTR_EQUAL;
        }

        int Rank(const Library& Entry) {
            if (Entry.ContentDir.empty()) {
                return 2;
            }
            return Entry.ContentExists ? 0 : 1;
        }
    }  // namespace

    std::string SizeText(const Library& Entry) {
        if (!Entry.ContentExists) {
            return "—";
        }
        return Entry.Size ? FormatSize(*Entry.Size) : std::string("Calculating…");
    }

    std::string StatusText(const Library& Entry) {
        if (Entry.ContentDir.empty()) {
            return "No content folder";
        }
        return Entry.ContentExists ? std::string() : std::string("Folder missing");
    }

    std::vector<size_t> AppModel::ComputeVisible() const {
        const std::vector<Library>& All = Libraries.Get();
        const std::wstring Needle       = std::wstring(Trim(FromUtf8(Filter.Get())));
        const XenUI::TableSort Order    = Sort.Get();

        std::vector<size_t> Rows;
        for (size_t i = 0; i < All.size(); ++i) {
            const Library& Entry = All[i];
            if (Needle.empty() || Contains(Entry.Name, Needle) || Contains(Entry.ContentDir.wstring(), Needle)) {
                Rows.push_back(i);
            }
        }

        const auto Less = [&](size_t LeftIndex, size_t RightIndex) {
            const Library& Left  = All[LeftIndex];
            const Library& Right = All[RightIndex];
            int Result           = 0;
            switch (Order.Column) {
                case FolderColumn:
                    Result = CompareText(Left.ContentDir.wstring(), Right.ContentDir.wstring());
                    break;
                case SizeColumn: {
                    const std::uintmax_t LeftSize  = Left.Size.value_or(0);
                    const std::uintmax_t RightSize = Right.Size.value_or(0);
                    Result                         = LeftSize < RightSize ? -1 : LeftSize > RightSize ? 1 : 0;
                    break;
                }
                case StatusColumn:
                    Result = Rank(Left) - Rank(Right);
                    break;
                default:
                    break;
            }
            if (Result == 0) {
                Result = CompareText(Left.Name, Right.Name);
            }
            return Order.Ascending ? Result < 0 : Result > 0;
        };
        std::stable_sort(Rows.begin(), Rows.end(), Less);
        return Rows;
    }

    const Library* AppModel::RowLibrary(int Row) const {
        const std::vector<size_t>& Rows = Visible.Get();
        if (Row < 0 || static_cast<size_t>(Row) >= Rows.size()) {
            return nullptr;
        }
        return &Libraries.Get()[Rows[static_cast<size_t>(Row)]];
    }

    std::vector<Library> AppModel::SelectedLibraries() const {
        std::vector<Library> Result;
        for (const Library& Entry : Libraries.Peek()) {
            if (std::find(_SelectedNames.begin(), _SelectedNames.end(), Entry.Name) != _SelectedNames.end()) {
                Result.push_back(Entry);
            }
        }
        return Result;
    }

    void AppModel::SelectRows(const std::vector<int>& Rows) {
        const std::vector<size_t>& Shown = Visible.Peek();
        const std::vector<Library>& All  = Libraries.Peek();
        _SelectedNames.clear();
        for (const int Row : Rows) {
            if (Row >= 0 && static_cast<size_t>(Row) < Shown.size()) {
                _SelectedNames.push_back(All[Shown[static_cast<size_t>(Row)]].Name);
            }
        }
        Selection = Rows;
    }

    void AppModel::SelectLead(int Row) {
        const std::vector<size_t>& Shown = Visible.Peek();
        _LeadName                        = Row >= 0 && static_cast<size_t>(Row) < Shown.size()
                                             ? Libraries.Peek()[Shown[static_cast<size_t>(Row)]].Name
                                             : std::wstring();
        Lead                             = Row;
    }

    void AppModel::ClearSelection() {
        _SelectedNames.clear();
        _LeadName.clear();
        XenUI::Batch([this] {
            Selection = std::vector<int> {};
            Lead      = -1;
        });
    }

    void AppModel::Reselect() {
        const std::vector<size_t>& Shown = Visible.Peek();
        const std::vector<Library>& All  = Libraries.Peek();
        std::vector<int> Rows;
        std::vector<std::wstring> StillShown;
        int NewLead = -1;
        for (size_t Row = 0; Row < Shown.size(); ++Row) {
            const std::wstring& Name = All[Shown[Row]].Name;
            if (std::find(_SelectedNames.begin(), _SelectedNames.end(), Name) != _SelectedNames.end()) {
                Rows.push_back(static_cast<int>(Row));
                StillShown.push_back(Name);
            }
            if (Name == _LeadName) {
                NewLead = static_cast<int>(Row);
            }
        }
        // Libraries hidden by the filter or gone after a rescan drop out of the selection, so actions only ever
        // apply to rows the user can see.
        _SelectedNames = std::move(StillShown);
        if (NewLead < 0) {
            _LeadName.clear();
        }
        XenUI::Batch([&] {
            Selection = std::move(Rows);
            Lead      = NewLead;
        });
    }

    void AppModel::SetLibraries(std::vector<Library> All) {
        Libraries = std::move(All);
        Reselect();
    }

    void AppModel::SetFilter(std::string Text) {
        Filter = std::move(Text);
        Reselect();
    }

    void AppModel::SetSize(const std::wstring& Name, std::uintmax_t Size) {
        Libraries.Modify([&](std::vector<Library>& All) {
            for (Library& Entry : All) {
                if (Entry.Name == Name) {
                    Entry.Size = Size;
                }
            }
        });
        if (Sort.Peek().Column == SizeColumn) {
            Reselect();
        }
    }
}  // namespace K8::Ui
