#pragma once

#include "domain.h"

using SettingsPathText = BoundedWideString<MAX_PATH + 32>;

SettingsPathText SettingsPath();
bool LoadSettingsAtPath(const wchar_t* path, Settings* settings,
                        ProductMessage* warning = nullptr);
bool SaveSettingsAtPath(const wchar_t* path, const Settings& settings,
                        ProductMessage* error = nullptr);
bool LoadSettings(Settings* settings, ProductMessage* warning = nullptr);
bool SaveSettings(const Settings& settings, ProductMessage* error = nullptr);
