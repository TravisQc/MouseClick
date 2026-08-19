#pragma once

#include "domain.h"

#include <windows.h>

constexpr int kHotkeyCommandId = 1;

class HotkeyManager final {
public:
    HotkeyManager() = default;
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;
    ~HotkeyManager();

    bool Register(HWND window, const Hotkey& hotkey, DWORD* error = nullptr);
    bool Replace(const Hotkey& hotkey, DWORD* error = nullptr);
    void Unregister();
    bool IsRegistered() const;
    const Hotkey& Current() const;

private:
    HWND window_ = nullptr;
    Hotkey current_{};
    bool registered_ = false;
};

