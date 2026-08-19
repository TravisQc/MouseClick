#include "click_worker.h"

#include <cstdint>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace {

std::uint64_t CounterNow() {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>(counter.QuadPart);
}

std::int64_t RelativeDueTime(std::uint64_t deltaTicks, std::uint64_t counterFrequency) {
    if (counterFrequency == 0) {
        return -1;
    }
    std::uint64_t hundredNanoseconds = (deltaTicks * 10000000ULL) / counterFrequency;
    if (hundredNanoseconds == 0) {
        hundredNanoseconds = 1;
    }
    constexpr std::uint64_t maxRelative = 0x7fffffffffffffffULL;
    if (hundredNanoseconds > maxRelative) {
        hundredNanoseconds = maxRelative;
    }
    return -static_cast<std::int64_t>(hundredNanoseconds);
}

bool SendClickAction(ClickButton button, ClickType clickType, INPUT* inputs,
                     UINT inputCapacity, DWORD* error) {
    const UINT inputCount = BuildClickInputs(button, clickType, inputs, inputCapacity);
    if (inputCount == 0) {
        if (error != nullptr) {
            *error = ERROR_INVALID_PARAMETER;
        }
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    const UINT sent = SendInput(inputCount, inputs, sizeof(INPUT));
    const DWORD inputError = GetLastError();
    if (sent == inputCount) {
        return true;
    }

    if (PartialInputNeedsRelease(sent, inputCount)) {
        INPUT release{};
        release.type = INPUT_MOUSE;
        release.mi.dwFlags = ButtonUpFlag(button);
        SendInput(1, &release, sizeof(INPUT));
    }
    if (error != nullptr) {
        *error = inputError == ERROR_SUCCESS ? ERROR_GEN_FAILURE : inputError;
    }
    return false;
}

} // namespace

std::uint64_t ComputeDeadlineTicks(std::uint64_t startTicks, std::uint64_t clickIndex,
                                   std::uint64_t counterFrequency,
                                   std::uint32_t intervalMilliseconds) {
    if (counterFrequency == 0 || intervalMilliseconds == 0) {
        return startTicks;
    }
    const std::uint64_t elapsedMilliseconds = clickIndex * intervalMilliseconds;
    return startTicks + (elapsedMilliseconds / 1000) * counterFrequency +
           ((elapsedMilliseconds % 1000) * counterFrequency) / 1000;
}

std::uint64_t SkipMissedDeadlines(std::uint64_t deadlineTicks, std::uint64_t nowTicks,
                                  std::uint64_t counterFrequency,
                                  std::uint32_t intervalMilliseconds) {
    if (counterFrequency == 0 || intervalMilliseconds == 0 || deadlineTicks > nowTicks) {
        return deadlineTicks;
    }
    std::uint64_t interval = (counterFrequency * intervalMilliseconds) / 1000;
    if (interval == 0) {
        interval = 1;
    }
    const std::uint64_t missed = (nowTicks - deadlineTicks) / interval + 1;
    return deadlineTicks + missed * interval;
}

DWORD ButtonDownFlag(ClickButton button) {
    switch (button) {
    case ClickButton::Left:
        return MOUSEEVENTF_LEFTDOWN;
    case ClickButton::Middle:
        return MOUSEEVENTF_MIDDLEDOWN;
    case ClickButton::Right:
        return MOUSEEVENTF_RIGHTDOWN;
    default:
        return 0;
    }
}

DWORD ButtonUpFlag(ClickButton button) {
    switch (button) {
    case ClickButton::Left:
        return MOUSEEVENTF_LEFTUP;
    case ClickButton::Middle:
        return MOUSEEVENTF_MIDDLEUP;
    case ClickButton::Right:
        return MOUSEEVENTF_RIGHTUP;
    default:
        return 0;
    }
}

UINT ClickInputCount(ClickType clickType) {
    if (clickType == ClickType::Single) {
        return 2;
    }
    if (clickType == ClickType::Double) {
        return 4;
    }
    return 0;
}

UINT BuildClickInputs(ClickButton button, ClickType clickType, INPUT* inputs,
                      UINT inputCapacity) {
    const UINT inputCount = ClickInputCount(clickType);
    const DWORD downFlag = ButtonDownFlag(button);
    const DWORD upFlag = ButtonUpFlag(button);
    if (inputs == nullptr || inputCount == 0 || inputCapacity < inputCount ||
        downFlag == 0 || upFlag == 0) {
        return 0;
    }
    for (UINT index = 0; index < inputCount; index += 2) {
        inputs[index].type = INPUT_MOUSE;
        inputs[index].mi.dwFlags = downFlag;
        inputs[index + 1].type = INPUT_MOUSE;
        inputs[index + 1].mi.dwFlags = upFlag;
    }
    return inputCount;
}

bool PartialInputNeedsRelease(UINT sentInputCount, UINT expectedInputCount) {
    return sentInputCount < expectedInputCount && (sentInputCount % 2) != 0;
}

bool ShouldSendClick(bool stopRequested) {
    return !stopRequested;
}

ClickWorker::~ClickWorker() {
    Stop();
    Reap();
}

bool ClickWorker::Start(HWND notifyWindow, UINT notifyMessage, const WorkerConfig& config) {
    if (thread_ != nullptr || notifyWindow == nullptr || notifyMessage == 0 ||
        !IsValidClickButton(config.button) || !IsValidClickType(config.clickType) ||
        config.intervalMilliseconds < kMinimumIntervalMilliseconds ||
        config.intervalMilliseconds > kMaximumIntervalMilliseconds) {
        return false;
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stopEvent_ == nullptr) {
        return false;
    }
    waitHandles_[0] = stopEvent_;
    waitHandles_[1] = nullptr;
    notifyWindow_ = notifyWindow;
    notifyMessage_ = notifyMessage;
    config_ = config;
    thread_ = CreateThread(nullptr, 0, &ClickWorker::ThreadEntry, this, 0, nullptr);
    if (thread_ == nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
        waitHandles_[0] = nullptr;
        notifyWindow_ = nullptr;
        notifyMessage_ = 0;
        return false;
    }
    return true;
}

void ClickWorker::Stop() {
    if (stopEvent_ != nullptr) {
        SetEvent(stopEvent_);
    }
    if (thread_ != nullptr && GetCurrentThreadId() != GetThreadId(thread_)) {
        WaitForSingleObject(thread_, INFINITE);
    }
}

void ClickWorker::Reap() {
    if (thread_ != nullptr) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (stopEvent_ != nullptr) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    waitHandles_[0] = nullptr;
    waitHandles_[1] = nullptr;
    notifyWindow_ = nullptr;
    notifyMessage_ = 0;
}

bool ClickWorker::IsRunning() const {
    if (thread_ == nullptr) {
        return false;
    }
    return WaitForSingleObject(thread_, 0) == WAIT_TIMEOUT;
}

DWORD WINAPI ClickWorker::ThreadEntry(void* parameter) {
    return static_cast<ClickWorker*>(parameter)->Run();
}

DWORD ClickWorker::Run() {
    Notify(WorkerEventKind::Running);

    LARGE_INTEGER counterFrequencyValue{};
    if (!QueryPerformanceFrequency(&counterFrequencyValue) || counterFrequencyValue.QuadPart <= 0) {
        Notify(WorkerEventKind::InputError, ERROR_NOT_SUPPORTED);
        Notify(WorkerEventKind::Stopped);
        return ERROR_NOT_SUPPORTED;
    }

    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (timer == nullptr) {
        timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    }
    waitHandles_[1] = timer;
    if (timer == nullptr) {
        Notify(WorkerEventKind::InputError, GetLastError());
        Notify(WorkerEventKind::Stopped);
        return GetLastError();
    }

    const std::uint64_t counterFrequency =
        static_cast<std::uint64_t>(counterFrequencyValue.QuadPart);
    const std::uint64_t startTicks = CounterNow();
    std::uint64_t clickIndex = 0;
    DWORD result = ERROR_SUCCESS;

    for (;;) {
        const std::uint64_t deadline = ComputeDeadlineTicks(
            startTicks, clickIndex, counterFrequency, config_.intervalMilliseconds);
        const std::uint64_t now = CounterNow();
        if (deadline > now) {
            LARGE_INTEGER due{};
            due.QuadPart = RelativeDueTime(deadline - now, counterFrequency);
            if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                result = GetLastError();
                Notify(WorkerEventKind::InputError, result);
                break;
            }
            const DWORD waitResult = WaitForMultipleObjects(2, waitHandles_, FALSE, INFINITE);
            if (waitResult == WAIT_OBJECT_0) {
                break;
            }
            if (waitResult != WAIT_OBJECT_0 + 1) {
                result = GetLastError();
                Notify(WorkerEventKind::InputError, result);
                break;
            }
        if (CounterNow() < deadline) {
                continue;
            }
        }

        if (!ShouldSendClick(WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0)) {
            break;
        }

        DWORD inputError = ERROR_SUCCESS;
        if (!SendClickAction(config_.button, config_.clickType, clickInputs_, 4, &inputError)) {
            result = inputError == ERROR_SUCCESS ? ERROR_GEN_FAILURE : inputError;
            Notify(WorkerEventKind::InputError, result);
            break;
        }
        ++clickIndex;
        const std::uint64_t afterClick = CounterNow();
        const std::uint64_t nextDeadline = ComputeDeadlineTicks(
            startTicks, clickIndex, counterFrequency, config_.intervalMilliseconds);
        if (nextDeadline <= afterClick) {
            clickIndex = 1 + ((afterClick - startTicks) * 1000) /
                                 (counterFrequency * config_.intervalMilliseconds);
        }
    }

    CancelWaitableTimer(timer);
    CloseHandle(timer);
    waitHandles_[1] = nullptr;
    Notify(WorkerEventKind::Stopped, result);
    return result;
}

void ClickWorker::Notify(WorkerEventKind kind, DWORD error) const {
    if (notifyWindow_ != nullptr && notifyMessage_ != 0) {
        PostMessageW(notifyWindow_, notifyMessage_, static_cast<WPARAM>(kind),
                     static_cast<LPARAM>(error));
    }
}
