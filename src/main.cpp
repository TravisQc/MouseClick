#include "ui.h"

#if defined(MOUSECLICK_CUSTOM_ENTRY)

extern "C" void __cdecl __security_init_cookie();

extern "C" __declspec(noreturn) void CustomEntry() {
    __security_init_cookie();

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    GetStartupInfoW(&startup);
    const int showCommand = (startup.dwFlags & STARTF_USESHOWWINDOW) != 0
                                ? startup.wShowWindow
                                : SW_SHOWDEFAULT;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const int exitCode = RunApplication(instance, showCommand);
    ExitProcess(static_cast<UINT>(exitCode));
}

#else

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    return RunApplication(instance, showCommand);
}

#endif
