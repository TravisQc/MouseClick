#pragma once

#include "domain.h"

#include <windows.h>

#include <cstdint>

enum class WorkerEventKind : UINT {
    Running = 1,
    Stopped = 2,
    InputError = 3,
};

struct WorkerConfig {
    ClickButton button = ClickButton::Left;
    ClickType clickType = ClickType::Single;
    std::uint32_t intervalMilliseconds = 100;
};

std::uint64_t ComputeDeadlineTicks(std::uint64_t startTicks, std::uint64_t clickIndex,
                                   std::uint64_t counterFrequency,
                                   std::uint32_t intervalMilliseconds);
std::uint64_t SkipMissedDeadlines(std::uint64_t deadlineTicks, std::uint64_t nowTicks,
                                  std::uint64_t counterFrequency,
                                  std::uint32_t intervalMilliseconds);
DWORD ButtonDownFlag(ClickButton button);
DWORD ButtonUpFlag(ClickButton button);
UINT ClickInputCount(ClickType clickType);
UINT BuildClickInputs(ClickButton button, ClickType clickType, INPUT* inputs,
                      UINT inputCapacity);
bool PartialInputNeedsRelease(UINT sentInputCount, UINT expectedInputCount);
bool ShouldSendClick(bool stopRequested);

class ClickWorker final {
public:
    ClickWorker() = default;
    ClickWorker(const ClickWorker&) = delete;
    ClickWorker& operator=(const ClickWorker&) = delete;
    ~ClickWorker();

    bool Start(HWND notifyWindow, UINT notifyMessage, const WorkerConfig& config);
    void Stop();
    void Reap();
    bool IsRunning() const;

private:
    static DWORD WINAPI ThreadEntry(void* parameter);
    DWORD Run();
    void Notify(WorkerEventKind kind, DWORD error = ERROR_SUCCESS) const;

    HANDLE thread_ = nullptr;
    HANDLE stopEvent_ = nullptr;
    HANDLE waitHandles_[2]{};
    INPUT clickInputs_[4]{};
    HWND notifyWindow_ = nullptr;
    UINT notifyMessage_ = 0;
    WorkerConfig config_{};
};
