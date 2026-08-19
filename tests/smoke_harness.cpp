#include <windows.h>

#include <algorithm>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr UINT kStartPauseId = 1008;
constexpr UINT kMouseButtonSelectId = 1001;
constexpr UINT kClickTypeSelectId = 1002;
constexpr UINT kIntervalId = 1004;
constexpr UINT kMouseDown = WM_LBUTTONDOWN;
constexpr UINT kMouseUp = WM_LBUTTONUP;

std::atomic<unsigned long> g_down{0};
std::atomic<unsigned long> g_up{0};

LRESULT CALLBACK LowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && lParam != 0) {
        const auto* event = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        if ((event->flags & LLMHF_INJECTED) != 0) {
            if (wParam == kMouseDown) {
                ++g_down;
            } else if (wParam == kMouseUp) {
                ++g_up;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

std::wstring QuoteCommandLine(const std::wstring& path) {
    std::wstring quoted = L"\"";
    quoted += path;
    quoted += L"\"";
    return quoted;
}

bool WriteTestSettings(const std::wstring& path, unsigned int clickType) {
    const std::wstring clickTypeText = std::to_wstring(clickType);
    const bool written =
        WritePrivateProfileStringW(L"click", L"button", L"0", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"click", L"intervalMs", L"50", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"click", L"clickType", clickTypeText.c_str(), path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"hotkey", L"modifiers", L"3", path.c_str()) != FALSE &&
        WritePrivateProfileStringW(L"hotkey", L"virtualKey", L"117", path.c_str()) != FALSE;
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
    return written;
}

struct WindowSearch {
    DWORD processId = 0;
    HWND window = nullptr;
};

BOOL CALLBACK FindProcessWindowCallback(HWND window, LPARAM parameter) {
    auto* search = reinterpret_cast<WindowSearch*>(parameter);
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(window, &windowProcessId);
    if (windowProcessId == search->processId) {
        search->window = window;
        return FALSE;
    }
    return TRUE;
}

HWND FindProcessWindow(DWORD processId) {
    for (int attempt = 0; attempt < 50; ++attempt) {
        WindowSearch search{processId, nullptr};
        EnumWindows(&FindProcessWindowCallback, reinterpret_cast<LPARAM>(&search));
        if (search.window != nullptr) {
            return search.window;
        }
        Sleep(100);
    }
    return nullptr;
}

bool RunCase(const std::wstring& executable, const std::wstring& settingsPath,
             unsigned int clickType, unsigned long pairsPerAction,
             const wchar_t* label) {
    DeleteFileW(settingsPath.c_str());
    if (!WriteTestSettings(settingsPath, clickType)) {
        std::wcerr << L"failed to create " << label << L" test settings\n";
        return false;
    }

    g_down = 0;
    g_up = 0;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring commandLine = QuoteCommandLine(executable);
    const std::wstring workingDirectory = executable.substr(0, executable.find_last_of(L"\\/"));
    const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                                        0, nullptr, workingDirectory.c_str(), &startup, &process);
    if (!created) {
        std::wcerr << L"failed to launch MouseClick.exe for " << label << L"\n";
        return false;
    }
    CloseHandle(process.hThread);

    std::atomic<bool> controlled{false};
    std::atomic<bool> tabOrderValid{false};
    std::atomic<bool> controlsDisabled{false};
    std::atomic<bool> controlsReenabled{false};
    const DWORD messageThread = GetCurrentThreadId();
    std::thread controller([messageThread, processId = process.dwProcessId, &controlled,
                            &tabOrderValid, &controlsDisabled, &controlsReenabled]() {
        const HWND window = FindProcessWindow(processId);
        if (window != nullptr) {
            controlled = true;
            const DWORD windowThread = GetWindowThreadProcessId(window, nullptr);
            const DWORD controllerThread = GetCurrentThreadId();
            const bool attached = AttachThreadInput(controllerThread, windowThread, TRUE) != FALSE;
            const HWND interval = GetDlgItem(window, kIntervalId);
            const HWND buttonSelect = GetDlgItem(window, kMouseButtonSelectId);
            SetForegroundWindow(window);
            SetFocus(interval);
            PostMessageW(interval, WM_KEYDOWN, VK_TAB, 0);
            PostMessageW(interval, WM_KEYUP, VK_TAB, 0);
            Sleep(100);
            tabOrderValid = GetFocus() == buttonSelect;
            if (attached) {
                AttachThreadInput(controllerThread, windowThread, FALSE);
            }
            PostMessageW(window, WM_COMMAND, MAKEWPARAM(kStartPauseId, BN_CLICKED), 0);
            Sleep(250);
            controlsDisabled =
                IsWindowEnabled(GetDlgItem(window, kMouseButtonSelectId)) == FALSE &&
                IsWindowEnabled(GetDlgItem(window, kClickTypeSelectId)) == FALSE;
            Sleep(4750);
            PostMessageW(window, WM_COMMAND, MAKEWPARAM(kStartPauseId, BN_CLICKED), 0);
            Sleep(250);
            controlsReenabled =
                IsWindowEnabled(GetDlgItem(window, kMouseButtonSelectId)) != FALSE &&
                IsWindowEnabled(GetDlgItem(window, kClickTypeSelectId)) != FALSE;
            Sleep(250);
            PostMessageW(window, WM_CLOSE, 0, 0);
        }
        Sleep(500);
        PostThreadMessageW(messageThread, WM_QUIT, 0, 0);
    });

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    controller.join();

    if (WaitForSingleObject(process.hProcess, 5000) == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 1);
    }
    CloseHandle(process.hProcess);

    const unsigned long down = g_down.load();
    const unsigned long up = g_up.load();
    const unsigned long completePairs = std::min(down, up);
    const bool completeActions = pairsPerAction != 0 &&
                                 completePairs % pairsPerAction == 0;
    const unsigned long actions = pairsPerAction == 0 ? 0 : completePairs / pairsPerAction;
    std::wcout << label << L": down=" << down << L" up=" << up
               << L" pairs=" << completePairs << L" actions=" << actions
               << L" tabOrder=" << tabOrderValid
               << L" disabled=" << controlsDisabled
               << L" reenabled=" << controlsReenabled << L"\n";
    return controlled && tabOrderValid && controlsDisabled && controlsReenabled && down == up &&
           completeActions && actions >= 90 && actions <= 110;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        std::wcerr << L"usage: MouseClickSmokeHarness.exe <MouseClick.exe> <MouseClick.ini>\n";
        return 2;
    }

    const std::wstring executable = argv[1];
    const std::wstring settingsPath = argv[2];

    POINT originalCursor{};
    GetCursorPos(&originalCursor);
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int targetWidth = 240;
    const int targetHeight = 100;
    const int targetX = std::max(workArea.left, workArea.right - targetWidth - 20);
    const int targetY = std::max(workArea.top, workArea.bottom - targetHeight - 20);
    const HWND target = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC",
        L"MouseClick smoke test target", WS_POPUP | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        targetX, targetY, targetWidth, targetHeight, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (target == nullptr) {
        std::wcerr << L"failed to create safe test target\n";
        return 2;
    }
    SetCursorPos(targetX + targetWidth / 2, targetY + targetHeight / 2);

    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, &LowLevelMouseProc,
                                  GetModuleHandleW(nullptr), 0);
    if (hook == nullptr) {
        DestroyWindow(target);
        SetCursorPos(originalCursor.x, originalCursor.y);
        DeleteFileW(settingsPath.c_str());
        std::wcerr << L"failed to install low-level mouse hook\n";
        return 2;
    }

    const bool singlePassed = RunCase(executable, settingsPath, 0, 1, L"single");
    const bool doublePassed = RunCase(executable, settingsPath, 1, 2, L"double");
    UnhookWindowsHookEx(hook);
    DestroyWindow(target);
    SetCursorPos(originalCursor.x, originalCursor.y);
    DeleteFileW(settingsPath.c_str());
    return singlePassed && doublePassed ? 0 : 1;
}
