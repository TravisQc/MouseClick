#include "click_worker.h"
#include "domain.h"
#include "settings.h"

#include <windows.h>

#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::wstring TestPath() {
    wchar_t directory[MAX_PATH]{};
    const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(std::size(directory)), directory);
    return std::wstring(directory, length) + L"\\MouseClickCoreTests-" +
           std::to_wstring(GetCurrentProcessId()) + L".ini";
}

void TestDomain() {
    const Settings defaults = DefaultSettings();
    Expect(defaults.button == ClickButton::Left, "default button is left");
    Expect(defaults.clickType == ClickType::Single, "default click type is single");
    Expect(defaults.intervalMilliseconds == 100, "default interval is 100 ms");
    Expect(defaults.hotkey.modifiers == (MOD_CONTROL | MOD_ALT), "default modifiers");
    Expect(defaults.hotkey.virtualKey == VK_F6, "default virtual key");
    Expect(defaults.theme == ThemeMode::System, "default theme follows system");
    Expect(defaults.clientWidth == kDefaultWindowClientWidth, "default window width");
    Expect(defaults.clientHeight == kDefaultWindowClientHeight, "default window height");

    Settings valid = defaults;
    ProductMessage error;
    Expect(ValidateSettings(valid, &error), "default settings validate");
    valid.clickType = static_cast<ClickType>(2);
    Expect(!ValidateSettings(valid, &error), "invalid click type rejected");
    valid.clickType = ClickType::Double;
    Expect(ValidateSettings(valid, &error), "double click type accepted");
    valid.intervalMilliseconds = 9;
    Expect(!ValidateSettings(valid, &error), "interval below 10 ms rejected");
    valid.intervalMilliseconds = 1001;
    Expect(!ValidateSettings(valid, &error), "interval over 1000 ms rejected");
    valid.intervalMilliseconds = 10;
    Expect(ValidateSettings(valid, &error), "minimum interval accepted");
    valid.intervalMilliseconds = 1000;
    Expect(ValidateSettings(valid, &error), "maximum interval accepted");
    valid.hotkey.modifiers = 0;
    Expect(!ValidateSettings(valid, &error), "modifier-less hotkey rejected");
    valid.hotkey.modifiers = MOD_CONTROL | MOD_ALT;
    valid.hotkey.virtualKey = VK_LWIN;
    Expect(!ValidateSettings(valid, &error), "windows-key hotkey rejected");
    valid.hotkey.virtualKey = VK_F6;
    valid.clientWidth = kMinimumWindowClientWidth - 1;
    Expect(!ValidateSettings(valid, &error), "window width below minimum rejected");
    valid.clientWidth = kDefaultWindowClientWidth;
    valid.clientHeight = kMaximumWindowClientHeight + 1;
    Expect(!ValidateSettings(valid, &error), "window height over maximum rejected");

    HotkeyText hotkeyText;
    Expect(FormatHotkey(defaults.hotkey, &hotkeyText), "default hotkey formats");
    Expect(std::wstring(hotkeyText.c_str()) == L"Ctrl+Alt+F6",
           "default hotkey text is unchanged");
}

void TestBoundedText() {
    BoundedWideString<4> text;
    Expect(text.empty() && text.length() == 0 && text.c_str()[0] == L'\0',
           "bounded text starts empty and terminated");
    Expect(text.Assign(L"test"), "exact-capacity assign succeeds");
    Expect(text.length() == 4 && text.c_str()[4] == L'\0',
           "exact-capacity assign is terminated");
    Expect(!text.Assign(L"tests"), "overflow assign fails");
    Expect(std::wstring(text.c_str()) == L"test",
           "overflow assign preserves the previous value");
    text.Clear();
    Expect(text.empty() && text.c_str()[0] == L'\0', "clear restores empty termination");
    Expect(text.Append(L"ab") && text.Append(L"cd"), "append reaches exact capacity");
    Expect(!text.Append(L"e") && text.c_str()[4] == L'\0',
           "overflow append fails without losing termination");
    BoundedWideString<4> copy = text;
    Expect(std::wstring(copy.c_str()) == L"abcd", "bounded text copy preserves content");
}

void TestUnsignedConversion() {
    std::uint32_t value = 7;
    Expect(ParseUnsigned(WideTextView{L"0"}, &value) && value == 0,
           "unsigned zero parses");
    Expect(ParseUnsigned(WideTextView{L"4294967295"}, &value) && value == 0xffffffffu,
           "maximum unsigned value parses");
    Expect(ParseUnsigned(WideTextView{L"00042"}, &value) && value == 42,
           "leading zeroes parse");
    Expect(!ParseUnsigned(WideTextView{}, &value), "empty unsigned text is rejected");
    Expect(!ParseUnsigned(WideTextView{L"-1"}, &value), "signed text is rejected");
    Expect(!ParseUnsigned(WideTextView{L"12x"}, &value), "malformed unsigned text is rejected");
    Expect(!ParseUnsigned(WideTextView{L" 12"}, &value), "whitespace is rejected");
    Expect(!ParseUnsigned(WideTextView{L"4294967296"}, &value),
           "unsigned overflow is rejected");

    BoundedWideString<10> formatted;
    Expect(FormatUnsigned(0, &formatted) && std::wstring(formatted.c_str()) == L"0",
           "unsigned zero formats");
    Expect(FormatUnsigned(0xffffffffu, &formatted) &&
               std::wstring(formatted.c_str()) == L"4294967295",
           "maximum unsigned value formats");
    BoundedWideString<9> tooSmall;
    tooSmall.Assign(L"keep");
    Expect(!FormatUnsigned(0xffffffffu, &tooSmall) &&
               std::wstring(tooSmall.c_str()) == L"keep",
           "format overflow preserves the previous value");
}

void TestSettings() {
    const std::wstring path = TestPath();
    DeleteFileW(path.c_str());

    Settings expected = DefaultSettings();
    expected.button = ClickButton::Right;
    expected.clickType = ClickType::Double;
    expected.intervalMilliseconds = 420;
    expected.hotkey.modifiers = MOD_CONTROL | MOD_SHIFT;
    expected.hotkey.virtualKey = VK_F8;
    expected.theme = ThemeMode::Dark;
    expected.clientWidth = 560;
    expected.clientHeight = 480;

    ProductMessage error;
    const bool saveSucceeded = SaveSettingsAtPath(path.c_str(), expected, &error);
    Expect(saveSucceeded, "settings save succeeds");
    Settings loaded;
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "settings load succeeds");
    Expect(loaded.button == expected.button, "button round trips");
    Expect(loaded.clickType == expected.clickType, "click type round trips");
    Expect(loaded.intervalMilliseconds == expected.intervalMilliseconds,
           "interval round trips");
    Expect(loaded.hotkey.modifiers == expected.hotkey.modifiers, "modifiers round trip");
    Expect(loaded.hotkey.virtualKey == expected.hotkey.virtualKey, "virtual key round trips");
    Expect(loaded.theme == expected.theme, "theme round trips");
    Expect(loaded.clientWidth == expected.clientWidth, "window width round trips");
    Expect(loaded.clientHeight == expected.clientHeight, "window height round trips");

    WritePrivateProfileStringW(L"click", L"intervalMs", L"invalid", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "corrupt settings still load safely");
    Expect(loaded.button == expected.button, "valid button survives corrupt field");
    Expect(loaded.intervalMilliseconds == 100, "corrupt interval uses default");

    WritePrivateProfileStringW(L"click", L"intervalMs", L"420", path.c_str());
    WritePrivateProfileStringW(L"click", L"clickType", L"invalid", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "corrupt click type still loads safely");
    Expect(loaded.clickType == ClickType::Single, "corrupt click type uses single");
    Expect(loaded.button == expected.button, "valid button survives corrupt click type");
    Expect(loaded.intervalMilliseconds == expected.intervalMilliseconds,
           "valid interval survives corrupt click type");
    Expect(loaded.hotkey.modifiers == expected.hotkey.modifiers,
           "valid modifiers survive corrupt click type");
    Expect(loaded.hotkey.virtualKey == expected.hotkey.virtualKey,
           "valid virtual key survives corrupt click type");
    Expect(loaded.theme == expected.theme, "valid theme survives corrupt click type");
    WritePrivateProfileStringW(L"click", L"clickType", L"9", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "out-of-range click type still loads safely");
    Expect(loaded.clickType == ClickType::Single, "out-of-range click type uses single");
    WritePrivateProfileStringW(L"click", L"clickType", nullptr, path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "old settings without click type load");
    Expect(loaded.clickType == ClickType::Single, "old settings default to single click");
    Expect(loaded.button == expected.button, "old settings preserve the button");
    Expect(loaded.intervalMilliseconds == expected.intervalMilliseconds,
           "old settings preserve the interval");

    WritePrivateProfileStringW(L"ui", L"theme", L"invalid", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "corrupt theme still loads safely");
    Expect(loaded.theme == ThemeMode::System, "corrupt theme follows system");
    WritePrivateProfileStringW(L"ui", L"theme", L"2", path.c_str());

    WritePrivateProfileStringW(L"window", L"clientWidth", L"invalid", path.c_str());
    WritePrivateProfileStringW(L"window", L"clientHeight", L"501", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "corrupt window size still loads safely");
    Expect(loaded.clientWidth == kDefaultWindowClientWidth,
           "corrupt window width uses default");
    Expect(loaded.clientHeight == kDefaultWindowClientHeight,
           "out-of-range window height uses default");
    WritePrivateProfileStringW(L"window", L"clientWidth", L"560", path.c_str());
    WritePrivateProfileStringW(L"window", L"clientHeight", L"480", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "valid window size loads safely");
    Expect(loaded.clientWidth == 560 && loaded.clientHeight == 480,
           "valid window size survives independent field recovery");
    WritePrivateProfileStringW(L"window", L"clientWidth", nullptr, path.c_str());
    WritePrivateProfileStringW(L"window", L"clientHeight", nullptr, path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "old settings without window size load");
    Expect(loaded.clientWidth == kDefaultWindowClientWidth &&
               loaded.clientHeight == kDefaultWindowClientHeight,
           "missing window size uses defaults");

    WritePrivateProfileStringW(L"click", L"frequency", L"20", path.c_str());
    WritePrivateProfileStringW(L"click", L"intervalMs", L"invalid", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "settings with both interval formats load");
    Expect(loaded.intervalMilliseconds == 100,
           "invalid new interval does not fall back to legacy frequency");
    WritePrivateProfileStringW(L"click", L"intervalMs", nullptr, path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "legacy frequency loads when interval is absent");
    Expect(loaded.intervalMilliseconds == 50, "legacy 20 CPS migrates to 50 ms");
    Expect(SaveSettingsAtPath(path.c_str(), loaded, &error), "migrated settings save succeeds");
    wchar_t legacyValue[16]{};
    Expect(GetPrivateProfileStringW(L"click", L"frequency", L"", legacyValue,
                                    static_cast<DWORD>(std::size(legacyValue)), path.c_str()) == 0,
           "saving migrated settings removes the legacy frequency field");
    WritePrivateProfileStringW(L"hotkey", L"virtualKey", nullptr, path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "missing hotkey field still loads safely");
    Expect(loaded.button == expected.button, "button survives missing hotkey field");
    Expect(loaded.hotkey.modifiers == expected.hotkey.modifiers,
           "valid hotkey modifiers survive missing virtual key");
    Expect(loaded.hotkey.virtualKey == VK_F6, "missing virtual key uses default");

    WritePrivateProfileStringW(L"click", L"button", L"invalid", path.c_str());
    Expect(LoadSettingsAtPath(path.c_str(), &loaded), "invalid button still loads safely");
    Expect(loaded.button == ClickButton::Left, "invalid button uses default");
    const std::wstring missingDirectoryPath = path + L"-missing-directory\\MouseClick.ini";
    Expect(!SaveSettingsAtPath(missingDirectoryPath.c_str(), expected, &error),
           "save failure is reported for an unavailable directory");
    DeleteFileW(path.c_str());
}

void TestWindowGeometry() {
    const WindowClientSize defaults = DefaultWindowClientSize();
    Expect(defaults.width == static_cast<int>(kDefaultWindowClientWidth) &&
               defaults.height == static_cast<int>(kDefaultWindowClientHeight),
           "default geometry is 600 by 500");
    const WindowClientSize clamped = ClampWindowClientSize(100, 900);
    Expect(clamped.width == static_cast<int>(kMinimumWindowClientWidth) &&
               clamped.height == static_cast<int>(kMaximumWindowClientHeight),
           "geometry clamps both axes");
    Expect(IsValidWindowClientSize(480, 440), "minimum geometry is valid");
    Expect(IsValidWindowClientSize(600, 500), "maximum geometry is valid");
    Expect(!IsValidWindowClientSize(479, 440), "width below minimum is invalid");
    Expect(!IsValidWindowClientSize(600, 501), "height over maximum is invalid");

    const WindowPixelSize scaled = LogicalClientSizeToPixels(WindowClientSize{600, 500}, 144);
    Expect(scaled.width == 900 && scaled.height == 750, "geometry scales at 150 percent");
    const WindowClientSize unscaled = PixelsToLogicalClientSize(scaled, 144);
    Expect(unscaled.width == 600 && unscaled.height == 500,
           "geometry unscales at 150 percent");

    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                             WS_MINIMIZEBOX | WS_THICKFRAME;
    RECT outer{};
    Expect(AdjustedWindowRectForClientSize(style, 0, FALSE, 96,
                                           WindowClientSize{600, 500}, &outer),
           "window frame adjusts for client size");
    Expect(outer.right - outer.left > 600 && outer.bottom - outer.top > 500,
           "outer window includes non-client frame");
    WindowPixelSize outerSize{};
    Expect(WindowRectSizeForClientSize(style, 0, FALSE, 144,
                                       WindowClientSize{480, 440}, &outerSize),
           "DPI-aware outer size calculates");
    Expect(outerSize.width > 720 && outerSize.height > 660,
           "DPI-aware outer size scales the client area");

    const RECT inside{10, 10, 100, 100};
    const RECT outside{10, 10, 500, 100};
    const RECT overlapping{90, 90, 120, 120};
    Expect(RectWithinClient(inside, 480, 440), "inside rectangle is valid");
    Expect(!RectWithinClient(outside, 480, 440), "outside rectangle is rejected");
    Expect(RectsOverlap(inside, overlapping), "overlapping rectangles are detected");
    Expect(!RectsOverlap(inside, RECT{100, 100, 120, 120}),
           "touching rectangles are not considered overlapping");
}

void TestSchedulingAndInputMapping() {
    Expect(ComputeDeadlineTicks(1000, 20, 1000000, 50) == 1001000,
           "deadline uses fixed timeline");
    Expect(SkipMissedDeadlines(100, 250, 1000, 100) == 300,
           "missed deadlines are skipped");
    Expect(ButtonDownFlag(ClickButton::Left) == MOUSEEVENTF_LEFTDOWN,
           "left down mapping");
    Expect(ButtonUpFlag(ClickButton::Middle) == MOUSEEVENTF_MIDDLEUP,
           "middle up mapping");
    Expect(ButtonDownFlag(ClickButton::Right) == MOUSEEVENTF_RIGHTDOWN,
           "right down mapping");
    Expect(ButtonUpFlag(ClickButton::Left) == MOUSEEVENTF_LEFTUP,
           "left up mapping");
    Expect(ButtonDownFlag(ClickButton::Middle) == MOUSEEVENTF_MIDDLEDOWN,
           "middle down mapping");
    Expect(ButtonUpFlag(ClickButton::Right) == MOUSEEVENTF_RIGHTUP,
           "right up mapping");
    Expect(ClickInputCount(ClickType::Single) == 2,
           "single click action contains one pair");
    Expect(ClickInputCount(ClickType::Double) == 4,
           "double click action contains two pairs");
    Expect(ClickInputCount(static_cast<ClickType>(2)) == 0,
           "invalid click type has no input events");

    const ClickButton buttons[] = {ClickButton::Left, ClickButton::Middle,
                                   ClickButton::Right};
    for (ClickButton button : buttons) {
        INPUT inputs[4]{};
        Expect(BuildClickInputs(button, ClickType::Double, inputs,
                                static_cast<UINT>(std::size(inputs))) == 4,
               "double click input batch is built");
        Expect(inputs[0].mi.dwFlags == ButtonDownFlag(button),
               "double click first event is button down");
        Expect(inputs[1].mi.dwFlags == ButtonUpFlag(button),
               "double click second event is button up");
        Expect(inputs[2].mi.dwFlags == ButtonDownFlag(button),
               "double click third event is button down");
        Expect(inputs[3].mi.dwFlags == ButtonUpFlag(button),
               "double click fourth event is button up");
    }
    INPUT singleInputs[2]{};
    Expect(BuildClickInputs(ClickButton::Left, ClickType::Single, singleInputs,
                            static_cast<UINT>(std::size(singleInputs))) == 2,
           "single click input batch is built");
    Expect(PartialInputNeedsRelease(1, 4), "partial batch ending down needs release");
    Expect(PartialInputNeedsRelease(3, 4), "second partial down needs release");
    Expect(!PartialInputNeedsRelease(0, 4), "empty partial batch needs no release");
    Expect(!PartialInputNeedsRelease(2, 4), "complete first pair needs no release");
    Expect(!PartialInputNeedsRelease(4, 4), "complete batch needs no release");
    Expect(ShouldSendClick(false), "click proceeds when stop is not requested");
    Expect(!ShouldSendClick(true), "stop request prevents the next click");

    std::uint64_t previous = ComputeDeadlineTicks(0, 0, 1000000, 50);
    for (std::uint64_t index = 1; index <= 200; ++index) {
        const std::uint64_t current = ComputeDeadlineTicks(0, index, 1000000, 50);
        Expect(current > previous, "50 ms timeline remains monotonic");
        previous = current;
    }
    Expect(ComputeDeadlineTicks(0, 200, 1000000, 50) == 10000000,
           "50 ms timeline spans ten seconds");
    Expect(ComputeDeadlineTicks(0, 1, 1000, 50) == 50,
           "action interval is independent of double-click event count");
    Expect(SkipMissedDeadlines(50, 276, 1000, 50) == 300,
           "late double-click action skips missed deadlines without a burst");
}

} // namespace

int main() {
    TestBoundedText();
    TestUnsignedConversion();
    TestDomain();
    TestSettings();
    TestWindowGeometry();
    TestSchedulingAndInputMapping();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "MouseClick core tests passed\n";
    return 0;
}
