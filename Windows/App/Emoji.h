// Emoji picker popup: opened by hotkey without stealing focus from the active app; keys arrive via the LL hook.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace vikey { namespace emoji {

void install(HINSTANCE inst);
void toggle();
void open();
void close();
bool isOpen();
void languageChanged(int lang);
void settingsChanged();
void themeChanged();

/// Converts Telex key sequences to accented Vietnamese for search queries ("nguwaj" -> "ngựa").
std::wstring telexConvert(const std::wstring& s);

}}
