// Vikey windows on Windows: main settings, onboarding guide, About, Acknowledgements, and typing log.
#pragma once
#include <windows.h>
namespace vikey { namespace ui {
void showSettings(HINSTANCE inst, bool firstRun);
void showOnboarding(HINSTANCE inst);
void showAbout(HINSTANCE inst);
void showAcknowledgements(HINSTANCE inst);
void showTypingLog(HINSTANCE inst);
void languageChanged(int lang);
void toggleTheme();
bool isDarkTheme();
}}
