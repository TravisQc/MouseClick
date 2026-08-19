#include "hotkey.h"

HotkeyManager::~HotkeyManager() {
    Unregister();
}

bool HotkeyManager::Register(HWND window, const Hotkey& hotkey, DWORD* error) {
    if (window == nullptr || !IsValidHotkey(hotkey)) {
        if (error != nullptr) {
            *error = ERROR_INVALID_PARAMETER;
        }
        return false;
    }
    if (registered_) {
        if (error != nullptr) {
            *error = ERROR_ALREADY_REGISTERED;
        }
        return false;
    }

    const UINT modifiers = (hotkey.modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT)) | MOD_NOREPEAT;
    if (!RegisterHotKey(window, kHotkeyCommandId, modifiers, hotkey.virtualKey)) {
        if (error != nullptr) {
            *error = GetLastError();
        }
        return false;
    }

    window_ = window;
    current_ = hotkey;
    registered_ = true;
    return true;
}

bool HotkeyManager::Replace(const Hotkey& hotkey, DWORD* error) {
    if (!IsValidHotkey(hotkey)) {
        if (error != nullptr) {
            *error = ERROR_INVALID_PARAMETER;
        }
        return false;
    }
    if (registered_ && current_.modifiers == hotkey.modifiers &&
        current_.virtualKey == hotkey.virtualKey) {
        return true;
    }

    const Hotkey previous = current_;
    const bool hadPrevious = registered_;
    if (hadPrevious) {
        Unregister();
    }

    DWORD registrationError = ERROR_SUCCESS;
    if (Register(window_, hotkey, &registrationError)) {
        return true;
    }

    if (hadPrevious) {
        DWORD restoreError = ERROR_SUCCESS;
        Register(window_, previous, &restoreError);
    }
    if (error != nullptr) {
        *error = registrationError;
    }
    return false;
}

void HotkeyManager::Unregister() {
    if (registered_ && window_ != nullptr) {
        UnregisterHotKey(window_, kHotkeyCommandId);
    }
    registered_ = false;
}

bool HotkeyManager::IsRegistered() const {
    return registered_;
}

const Hotkey& HotkeyManager::Current() const {
    return current_;
}
