#include "domain.h"

namespace {

bool IsModifierKey(UINT virtualKey) {
    return virtualKey == VK_SHIFT || virtualKey == VK_CONTROL ||
           virtualKey == VK_MENU || virtualKey == VK_LSHIFT ||
           virtualKey == VK_RSHIFT || virtualKey == VK_LCONTROL ||
           virtualKey == VK_RCONTROL || virtualKey == VK_LMENU ||
           virtualKey == VK_RMENU || virtualKey == VK_LWIN ||
           virtualKey == VK_RWIN;
}

bool KeyName(UINT virtualKey, HotkeyText* name) {
    if (name == nullptr) {
        return false;
    }
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        BoundedWideString<2> number;
        return FormatUnsigned(virtualKey - VK_F1 + 1, &number) &&
               name->Assign(L"F") && name->Append(number.view());
    }

    switch (virtualKey) {
    case VK_SPACE:
        return name->Assign(L"Space");
    case VK_RETURN:
        return name->Assign(L"Enter");
    case VK_ESCAPE:
        return name->Assign(L"Esc");
    case VK_TAB:
        return name->Assign(L"Tab");
    case VK_BACK:
        return name->Assign(L"Backspace");
    case VK_DELETE:
        return name->Assign(L"Delete");
    case VK_INSERT:
        return name->Assign(L"Insert");
    case VK_HOME:
        return name->Assign(L"Home");
    case VK_END:
        return name->Assign(L"End");
    case VK_PRIOR:
        return name->Assign(L"PageUp");
    case VK_NEXT:
        return name->Assign(L"PageDown");
    case VK_LEFT:
        return name->Assign(L"Left");
    case VK_RIGHT:
        return name->Assign(L"Right");
    case VK_UP:
        return name->Assign(L"Up");
    case VK_DOWN:
        return name->Assign(L"Down");
    default:
        break;
    }

    const UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    if (scanCode != 0) {
        wchar_t keyName[64]{};
        const LONG keyData = static_cast<LONG>(scanCode << 16);
        const int length = GetKeyNameTextW(
            keyData, keyName, static_cast<int>(sizeof(keyName) / sizeof(keyName[0])));
        if (length > 0) {
            return name->Assign(WideTextView{keyName, static_cast<std::size_t>(length)});
        }
    }

    return name->Assign(L"Key");
}

} // namespace

Settings DefaultSettings() {
    return Settings{};
}

bool IsValidClickButton(ClickButton button) {
    return button == ClickButton::Left || button == ClickButton::Middle ||
           button == ClickButton::Right;
}

bool IsValidClickType(ClickType clickType) {
    return clickType == ClickType::Single || clickType == ClickType::Double;
}

bool IsValidThemeMode(ThemeMode mode) {
    return mode == ThemeMode::System || mode == ThemeMode::Light || mode == ThemeMode::Dark;
}

bool IsValidHotkey(const Hotkey& hotkey, ProductMessage* error) {
    constexpr UINT supportedModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT;
    if ((hotkey.modifiers & supportedModifiers) == 0 ||
        (hotkey.modifiers & ~(supportedModifiers | MOD_NOREPEAT)) != 0) {
        if (error != nullptr) {
            error->Assign(L"快捷键必须包含 Ctrl、Alt 或 Shift 修饰键。");
        }
        return false;
    }
    if (hotkey.virtualKey == 0 || IsModifierKey(hotkey.virtualKey) ||
        hotkey.virtualKey == VK_LWIN || hotkey.virtualKey == VK_RWIN) {
        if (error != nullptr) {
            error->Assign(L"快捷键必须包含一个非修饰键，且不能使用 Windows 键。");
        }
        return false;
    }
    return true;
}

bool ValidateSettings(const Settings& settings, ProductMessage* error) {
    if (!IsValidClickButton(settings.button)) {
        if (error != nullptr) {
            error->Assign(L"请选择左键、中键或右键。");
        }
        return false;
    }
    if (!IsValidClickType(settings.clickType)) {
        if (error != nullptr) {
            error->Assign(L"请选择单次点击或双击。");
        }
        return false;
    }
    if (settings.intervalMilliseconds < kMinimumIntervalMilliseconds ||
        settings.intervalMilliseconds > kMaximumIntervalMilliseconds) {
        if (error != nullptr) {
            error->Assign(L"间隔必须是 10 至 1000 毫秒之间的整数。");
        }
        return false;
    }
    if (!IsValidThemeMode(settings.theme)) {
        if (error != nullptr) {
            error->Assign(L"请选择有效的主题模式。");
        }
        return false;
    }
    return IsValidHotkey(settings.hotkey, error);
}

bool FormatHotkey(const Hotkey& hotkey, HotkeyText* text) {
    if (text == nullptr) {
        return false;
    }
    text->Clear();
    if ((hotkey.modifiers & MOD_CONTROL) != 0) {
        if (!text->Append(L"Ctrl+")) {
            return false;
        }
    }
    if ((hotkey.modifiers & MOD_ALT) != 0) {
        if (!text->Append(L"Alt+")) {
            return false;
        }
    }
    if ((hotkey.modifiers & MOD_SHIFT) != 0) {
        if (!text->Append(L"Shift+")) {
            return false;
        }
    }
    HotkeyText keyName;
    return KeyName(hotkey.virtualKey, &keyName) && text->Append(keyName.view());
}

const wchar_t* StateLabel(AppState state) {
    switch (state) {
    case AppState::Paused:
        return L"已暂停";
    case AppState::Starting:
        return L"启动中...";
    case AppState::Running:
        return L"运行中";
    case AppState::Stopping:
        return L"停止中...";
    case AppState::Error:
        return L"错误";
    default:
        return L"未知状态";
    }
}
