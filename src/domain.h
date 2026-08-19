#pragma once

#include "bounded_text.h"

#include <windows.h>

#include <cstdint>

enum class ClickButton : std::uint32_t {
    Left = 0,
    Middle = 1,
    Right = 2,
};

enum class ClickType : std::uint32_t {
    Single = 0,
    Double = 1,
};

enum class AppState : std::uint32_t {
    Paused = 0,
    Starting = 1,
    Running = 2,
    Stopping = 3,
    Error = 4,
};

enum class ThemeMode : std::uint32_t {
    System = 0,
    Light = 1,
    Dark = 2,
};

struct Hotkey {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT virtualKey = VK_F6;
};

struct Settings {
    ClickButton button = ClickButton::Left;
    ClickType clickType = ClickType::Single;
    std::uint32_t intervalMilliseconds = 100;
    Hotkey hotkey{};
    ThemeMode theme = ThemeMode::System;
};

constexpr std::uint32_t kMinimumIntervalMilliseconds = 10;
constexpr std::uint32_t kMaximumIntervalMilliseconds = 1000;

using ProductMessage = BoundedWideString<128>;
using HotkeyText = BoundedWideString<64>;

Settings DefaultSettings();
bool IsValidClickButton(ClickButton button);
bool IsValidClickType(ClickType clickType);
bool IsValidThemeMode(ThemeMode mode);
bool IsValidHotkey(const Hotkey& hotkey, ProductMessage* error = nullptr);
bool ValidateSettings(const Settings& settings, ProductMessage* error = nullptr);
bool FormatHotkey(const Hotkey& hotkey, HotkeyText* text);
const wchar_t* StateLabel(AppState state);
