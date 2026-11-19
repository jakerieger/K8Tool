#include "Core/Registry.hpp"
#include "Core/Text.hpp"

#include <array>
#include <format>
#include <fstream>
#include <sstream>

namespace K8::Registry {
    namespace {
        constexpr REGSAM kView = KEY_WOW64_64KEY;

        constexpr std::array<std::wstring_view, 42> kExcludedProducts = {
          L"Massive",
          L"Massive X",
          L"Reaktor 6",
          L"Battery 4",
          L"FM8",
          L"Absynth 5",
          L"Absynth 6",
          L"Guitar Rig 7 Pro",
          L"Traktor Pro 4",
          L"Maschine 3",
          L"Komplete Kontrol",
          L"Kontakt 5",
          L"Kontakt 6",
          L"Kontakt 7",
          L"Kontakt 8",
          L"Native Access",
          L"Monark",
          L"Super 8",
          L"TRK-01",
          L"Form",
          L"Rounds",
          L"Molekular",
          L"Raum",
          L"Replika XT",
          L"Choral",
          L"Flair",
          L"Phasis",
          L"Bite",
          L"Dirt",
          L"Freak",
          L"Driver",
          L"Solid EQ",
          L"Solid Bus Comp",
          L"Solid Dynamics",
          L"VC 2A",
          L"VC 76",
          L"VC 160",
          L"Vari Comp",
          L"Enhanced EQ",
          L"Passive EQ",
          L"RC 24",
          L"RC 48",
        };

        /// @brief Closes the key when it goes out of scope.
        class KeyHandle {
        public:
            KeyHandle() = default;

            ~KeyHandle() {
                if (_Key != nullptr) {
                    RegCloseKey(_Key);
                }
            }

            KeyHandle(const KeyHandle&)            = delete;
            KeyHandle& operator=(const KeyHandle&) = delete;

            LSTATUS Open(HKEY Root, const std::wstring& Path, REGSAM Access) {
                return RegOpenKeyExW(Root, Path.c_str(), 0, Access | kView, &_Key);
            }

            [[nodiscard]] HKEY Get() const {
                return _Key;
            }

        private:
            HKEY _Key = nullptr;
        };

        std::wstring_view RootName(HKEY Root) {
            if (Root == HKEY_LOCAL_MACHINE) {
                return L"HKEY_LOCAL_MACHINE";
            }
            if (Root == HKEY_CURRENT_USER) {
                return L"HKEY_CURRENT_USER";
            }
            if (Root == HKEY_CLASSES_ROOT) {
                return L"HKEY_CLASSES_ROOT";
            }
            if (Root == HKEY_USERS) {
                return L"HKEY_USERS";
            }
            return L"HKEY_UNKNOWN";
        }

        std::wstring Quote(std::wstring_view Text) {
            std::wstring Result = L"\"";
            for (const wchar_t Character : Text) {
                if (Character == L'\\' || Character == L'"') {
                    Result += L'\\';
                }
                Result += Character;
            }
            return Result + L"\"";
        }

        std::wstring HexBytes(const std::vector<BYTE>& Data) {
            std::wstring Result;
            for (size_t i = 0; i < Data.size(); ++i) {
                if (i > 0) {
                    Result += L',';
                }
                Result += std::format(L"{:02x}", Data[i]);
            }
            return Result;
        }

        std::wstring FormatValue(DWORD Type, const std::vector<BYTE>& Data) {
            switch (Type) {
                case REG_SZ: {
                    std::wstring Text(reinterpret_cast<const wchar_t*>(Data.data()), Data.size() / sizeof(wchar_t));
                    while (!Text.empty() && Text.back() == L'\0') {
                        Text.pop_back();
                    }
                    return Quote(Text);
                }
                case REG_DWORD:
                    if (Data.size() == sizeof(DWORD)) {
                        return std::format(L"dword:{:08x}", *reinterpret_cast<const DWORD*>(Data.data()));
                    }
                    break;
                case REG_BINARY:
                    return L"hex:" + HexBytes(Data);
                default:
                    break;
            }
            return std::format(L"hex({:x}):", Type) + HexBytes(Data);
        }

        bool ExportKey(HKEY Key, const std::wstring& Name, std::wostream& Out, std::wstring& Error) {
            Out << L"\r\n[" << Name << L"]\r\n";

            DWORD ValueCount = 0, MaxName = 0, MaxData = 0, SubkeyCount = 0, MaxSubkey = 0;
            LSTATUS Status = RegQueryInfoKeyW(Key,
                                              nullptr,
                                              nullptr,
                                              nullptr,
                                              &SubkeyCount,
                                              &MaxSubkey,
                                              nullptr,
                                              &ValueCount,
                                              &MaxName,
                                              &MaxData,
                                              nullptr,
                                              nullptr);
            if (Status != ERROR_SUCCESS) {
                Error = std::format(L"Could not read {}: {}", Name, ErrorText(Status));
                return false;
            }

            std::wstring ValueName(MaxName + 1, L'\0');
            std::vector<BYTE> Data(MaxData);
            for (DWORD i = 0; i < ValueCount; ++i) {
                DWORD NameLength = MaxName + 1;
                DWORD DataLength = MaxData;
                DWORD Type       = 0;
                Status = RegEnumValueW(Key, i, ValueName.data(), &NameLength, nullptr, &Type, Data.data(), &DataLength);
                if (Status != ERROR_SUCCESS) {
                    continue;
                }
                const std::wstring_view Shown(ValueName.data(), NameLength);
                Out << (Shown.empty() ? std::wstring(L"@") : Quote(Shown)) << L'='
                    << FormatValue(Type, std::vector<BYTE>(Data.begin(), Data.begin() + DataLength)) << L"\r\n";
            }

            std::wstring SubkeyName(MaxSubkey + 1, L'\0');
            for (DWORD i = 0; i < SubkeyCount; ++i) {
                DWORD Length = MaxSubkey + 1;
                if (RegEnumKeyExW(Key, i, SubkeyName.data(), &Length, nullptr, nullptr, nullptr, nullptr) !=
                    ERROR_SUCCESS) {
                    continue;
                }
                const std::wstring Child(SubkeyName.data(), Length);
                HKEY ChildKey = nullptr;
                if (RegOpenKeyExW(Key, Child.c_str(), 0, KEY_READ | kView, &ChildKey) != ERROR_SUCCESS) {
                    continue;
                }
                const bool Exported = ExportKey(ChildKey, Name + L"\\" + Child, Out, Error);
                RegCloseKey(ChildKey);
                if (!Exported) {
                    return false;
                }
            }
            return true;
        }
    }  // namespace

    std::wstring ErrorText(LSTATUS Code) {
        wchar_t* Buffer = nullptr;
        const DWORD Count =
          FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                         nullptr,
                         static_cast<DWORD>(Code),
                         0,
                         reinterpret_cast<wchar_t*>(&Buffer),
                         0,
                         nullptr);
        std::wstring Result = Count > 0 ? std::wstring(Trim(std::wstring_view(Buffer, Count)))
                                        : std::format(L"error {}", static_cast<long>(Code));
        LocalFree(Buffer);
        return Result;
    }

    std::wstring FullName(const RegistryKey& Key) {
        return std::wstring(RootName(Key.Root)) + L"\\" + Key.Path;
    }

    bool IsExcludedProduct(std::wstring_view Name) {
        for (const std::wstring_view Excluded : kExcludedProducts) {
            if (EqualsNoCase(Trim(Name), Excluded)) {
                return true;
            }
        }
        return false;
    }

    std::optional<std::vector<std::wstring>> SubkeyNames(HKEY Root, const std::wstring& Path) {
        KeyHandle Key;
        if (Key.Open(Root, Path, KEY_READ) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        DWORD Count = 0, MaxLength = 0;
        if (RegQueryInfoKeyW(Key.Get(),
                             nullptr,
                             nullptr,
                             nullptr,
                             &Count,
                             &MaxLength,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        std::vector<std::wstring> Names;
        std::wstring Buffer(MaxLength + 1, L'\0');
        for (DWORD i = 0; i < Count; ++i) {
            DWORD Length = MaxLength + 1;
            if (RegEnumKeyExW(Key.Get(), i, Buffer.data(), &Length, nullptr, nullptr, nullptr, nullptr) ==
                ERROR_SUCCESS) {
                Names.emplace_back(Buffer.data(), Length);
            }
        }
        return Names;
    }

    std::optional<std::wstring> ReadString(const RegistryKey& Key, const wchar_t* Name) {
        constexpr DWORD kFlags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_SUBKEY_WOW6464KEY;
        DWORD Size             = 0;
        if (RegGetValueW(Key.Root, Key.Path.c_str(), Name, kFlags, nullptr, nullptr, &Size) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        std::wstring Value(Size / sizeof(wchar_t) + 1, L'\0');
        Size = static_cast<DWORD>(Value.size() * sizeof(wchar_t));
        if (RegGetValueW(Key.Root, Key.Path.c_str(), Name, kFlags, nullptr, Value.data(), &Size) != ERROR_SUCCESS) {
            return std::nullopt;
        }
        Value.resize(wcsnlen(Value.c_str(), Value.size()));
        return Value;
    }

    bool WriteString(const RegistryKey& Key, const wchar_t* Name, const std::wstring& Value, std::wstring& Error) {
        KeyHandle Handle;
        LSTATUS Status = Handle.Open(Key.Root, Key.Path, KEY_SET_VALUE);
        if (Status == ERROR_SUCCESS) {
            Status = RegSetValueExW(Handle.Get(),
                                    Name,
                                    0,
                                    REG_SZ,
                                    reinterpret_cast<const BYTE*>(Value.c_str()),
                                    static_cast<DWORD>((Value.size() + 1) * sizeof(wchar_t)));
        }
        if (Status != ERROR_SUCCESS) {
            Error = std::format(L"Could not write {} in {}: {}", Name, FullName(Key), ErrorText(Status));
            return false;
        }
        return true;
    }

    bool Exists(const RegistryKey& Key) {
        KeyHandle Handle;
        return Handle.Open(Key.Root, Key.Path, KEY_READ) == ERROR_SUCCESS;
    }

    bool DeleteTree(const RegistryKey& Key, std::wstring& Error) {
        const size_t Split = Key.Path.rfind(L'\\');
        if (Split == std::wstring::npos) {
            Error = L"Refusing to delete a top-level key";
            return false;
        }
        KeyHandle Parent;
        LSTATUS Status = Parent.Open(Key.Root, Key.Path.substr(0, Split), KEY_READ | KEY_WRITE | DELETE);
        if (Status == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        if (Status == ERROR_SUCCESS) {
            const std::wstring Child = Key.Path.substr(Split + 1);
            Status                   = RegDeleteTreeW(Parent.Get(), Child.c_str());
        }
        if (Status != ERROR_SUCCESS && Status != ERROR_FILE_NOT_FOUND) {
            Error = std::format(L"Could not delete {}: {}", FullName(Key), ErrorText(Status));
            return false;
        }
        return true;
    }

    bool ExportToFile(const RegistryKey& Key, const std::filesystem::path& File, std::wstring& Error) {
        KeyHandle Handle;
        const LSTATUS Status = Handle.Open(Key.Root, Key.Path, KEY_READ);
        if (Status != ERROR_SUCCESS) {
            Error = std::format(L"Could not open {}: {}", FullName(Key), ErrorText(Status));
            return false;
        }

        std::wostringstream Text;
        Text << L"Windows Registry Editor Version 5.00\r\n";
        if (!ExportKey(Handle.Get(), FullName(Key), Text, Error)) {
            return false;
        }
        Text << L"\r\n";

        std::error_code FileError;
        std::filesystem::create_directories(File.parent_path(), FileError);
        std::ofstream Out(File, std::ios::binary | std::ios::trunc);
        const std::wstring Body        = Text.str();
        constexpr unsigned char kBom[] = {0xFF, 0xFE};
        Out.write(reinterpret_cast<const char*>(kBom), sizeof(kBom));
        Out.write(reinterpret_cast<const char*>(Body.data()),
                  static_cast<std::streamsize>(Body.size() * sizeof(wchar_t)));
        if (!Out) {
            Error = std::format(L"Could not write {}", File.wstring());
            return false;
        }
        return true;
    }
}  // namespace K8::Registry
