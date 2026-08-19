#include "settings.h"

namespace {

using ProfileValueText = BoundedWideString<63>;

void SetWriteError(ProductMessage* error) {
    if (error != nullptr) {
        error->Assign(L"无法写入程序目录中的 MouseClick.ini。");
    }
}

void ReadProfileValue(const wchar_t* section, const wchar_t* key,
                      const wchar_t* path, ProfileValueText* value) {
    value->Clear();
    const DWORD length = GetPrivateProfileStringW(
        section, key, L"", value->data(),
        static_cast<DWORD>(ProfileValueText::capacity() + 1), path);
    value->SetLength(static_cast<std::size_t>(length));
}

bool WriteProfileValue(const wchar_t* section, const wchar_t* key,
                       std::uint32_t value, const wchar_t* path) {
    BoundedWideString<10> number;
    return FormatUnsigned(value, &number) &&
           WritePrivateProfileStringW(section, key, number.c_str(), path) != FALSE;
}

bool WriteAllProfileValues(const wchar_t* path, const Settings& settings,
                           ProductMessage* error) {
    const bool written =
        WriteProfileValue(L"click", L"button", static_cast<std::uint32_t>(settings.button), path) &&
        WriteProfileValue(L"click", L"clickType",
                          static_cast<std::uint32_t>(settings.clickType), path) &&
        WriteProfileValue(L"click", L"intervalMs", settings.intervalMilliseconds, path) &&
        WriteProfileValue(L"hotkey", L"modifiers", settings.hotkey.modifiers, path) &&
        WriteProfileValue(L"hotkey", L"virtualKey", settings.hotkey.virtualKey, path) &&
        WriteProfileValue(L"ui", L"theme", static_cast<std::uint32_t>(settings.theme), path) &&
        WriteProfileValue(L"window", L"clientWidth", settings.clientWidth, path) &&
        WriteProfileValue(L"window", L"clientHeight", settings.clientHeight, path);
    if (!written) {
        SetWriteError(error);
        return false;
    }

    // Profile APIs cache INI files. Flush the temporary file before replacing the target.
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path);
    return true;
}

bool BuildSettingsPath(SettingsPathText* path) {
    path->Clear();
    const DWORD length = GetModuleFileNameW(
        nullptr, path->data(), static_cast<DWORD>(SettingsPathText::capacity() + 1));
    if (length == 0 || length >= SettingsPathText::capacity() + 1) {
        path->Clear();
        return false;
    }
    path->SetLength(static_cast<std::size_t>(length));

    std::size_t separator = path->length();
    while (separator != 0 && path->c_str()[separator - 1] != L'\\' &&
           path->c_str()[separator - 1] != L'/') {
        --separator;
    }
    if (separator == 0) {
        path->Clear();
        return false;
    }
    path->SetLength(separator);
    if (!path->Append(L"MouseClick.ini")) {
        path->Clear();
        return false;
    }
    return true;
}

} // namespace

SettingsPathText SettingsPath() {
    SettingsPathText path;
    BuildSettingsPath(&path);
    return path;
}

bool LoadSettingsAtPath(const wchar_t* path, Settings* settings, ProductMessage* warning) {
    if (settings == nullptr) {
        return false;
    }

    if (warning != nullptr) {
        warning->Clear();
    }

    *settings = DefaultSettings();
    if (path == nullptr || path[0] == L'\0') {
        if (warning != nullptr) {
            warning->Assign(L"无法确定程序目录，已使用默认设置。");
        }
        return false;
    }

    const DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD fileError = GetLastError();
        if (fileError == ERROR_FILE_NOT_FOUND || fileError == ERROR_PATH_NOT_FOUND) {
            return true;
        }
        if (warning != nullptr) {
            warning->Assign(L"无法读取设置文件，已使用默认设置。");
        }
        return false;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        if (warning != nullptr) {
            warning->Assign(L"设置路径是目录，已使用默认设置。");
        }
        return false;
    }

    std::uint32_t value = 0;
    ProfileValueText text;
    ReadProfileValue(L"click", L"button", path, &text);
    if (ParseUnsigned(text.view(), &value) &&
        value <= static_cast<std::uint32_t>(ClickButton::Right)) {
        settings->button = static_cast<ClickButton>(value);
    }

    ReadProfileValue(L"click", L"clickType", path, &text);
    if (ParseUnsigned(text.view(), &value) &&
        value <= static_cast<std::uint32_t>(ClickType::Double)) {
        settings->clickType = static_cast<ClickType>(value);
    }

    ReadProfileValue(L"click", L"intervalMs", path, &text);
    if (ParseUnsigned(text.view(), &value) && value >= kMinimumIntervalMilliseconds &&
        value <= kMaximumIntervalMilliseconds) {
        settings->intervalMilliseconds = value;
    } else if (text.empty()) {
        ReadProfileValue(L"click", L"frequency", path, &text);
        if (ParseUnsigned(text.view(), &value) && value >= 1 && value <= 100) {
            settings->intervalMilliseconds = 1000 / value;
        }
    }

    std::uint32_t modifiers = 0;
    std::uint32_t virtualKey = 0;
    Hotkey candidate = settings->hotkey;
    ReadProfileValue(L"hotkey", L"modifiers", path, &text);
    if (ParseUnsigned(text.view(), &modifiers)) {
        candidate.modifiers = modifiers;
    }
    ReadProfileValue(L"hotkey", L"virtualKey", path, &text);
    if (ParseUnsigned(text.view(), &virtualKey)) {
        candidate.virtualKey = virtualKey;
    }
    if (IsValidHotkey(candidate)) {
        settings->hotkey = candidate;
    }

    ReadProfileValue(L"ui", L"theme", path, &text);
    if (ParseUnsigned(text.view(), &value) &&
        value <= static_cast<std::uint32_t>(ThemeMode::Dark)) {
        settings->theme = static_cast<ThemeMode>(value);
    }

    ReadProfileValue(L"window", L"clientWidth", path, &text);
    if (ParseUnsigned(text.view(), &value) &&
        value >= kMinimumWindowClientWidth && value <= kMaximumWindowClientWidth) {
        settings->clientWidth = value;
    }
    ReadProfileValue(L"window", L"clientHeight", path, &text);
    if (ParseUnsigned(text.view(), &value) &&
        value >= kMinimumWindowClientHeight && value <= kMaximumWindowClientHeight) {
        settings->clientHeight = value;
    }

    return true;
}

bool SaveSettingsAtPath(const wchar_t* path, const Settings& settings, ProductMessage* error) {
    if (error != nullptr) {
        error->Clear();
    }
    if (path == nullptr || path[0] == L'\0' || !ValidateSettings(settings, error)) {
        if (error != nullptr && error->empty()) {
            error->Assign(L"设置无效，无法保存。");
        }
        return false;
    }

    SettingsPathText temporaryPath;
    BoundedWideString<10> processId;
    if (!temporaryPath.Assign(path) || !temporaryPath.Append(L".tmp.") ||
        !FormatUnsigned(GetCurrentProcessId(), &processId) ||
        !temporaryPath.Append(processId.view())) {
        SetWriteError(error);
        return false;
    }

    // Replace the target only after every value has reached the temporary file.
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path);
    DeleteFileW(temporaryPath.c_str());
    if (!WriteAllProfileValues(temporaryPath.c_str(), settings, error)) {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    if (!MoveFileExW(temporaryPath.c_str(), path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporaryPath.c_str());
        SetWriteError(error);
        return false;
    }
    return true;
}

bool LoadSettings(Settings* settings, ProductMessage* warning) {
    const SettingsPathText path = SettingsPath();
    return LoadSettingsAtPath(path.c_str(), settings, warning);
}

bool SaveSettings(const Settings& settings, ProductMessage* error) {
    const SettingsPathText path = SettingsPath();
    return SaveSettingsAtPath(path.c_str(), settings, error);
}
