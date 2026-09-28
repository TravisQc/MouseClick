#pragma once

#include "app_context.h"

#include <windows.h>

void CreateUiResources(AppContext* context);
void DestroyUiResources(AppContext* context);
void CreateControls(AppContext* context);
void LayoutControls(AppContext* context);
void ApplyControlTheme(AppContext* context);
void SetInitialSettings(AppContext* context);
bool ReadSettingsFromControls(AppContext* context, Settings* settings);
void UpdateHotkeyText(HWND control, const Hotkey& hotkey);
ThemeMode ThemeModeFromControlId(int id);
