
#ifndef FUNC_PROT_H
#define FUNC_PROT_H

#include "launcher_typedef.h"


void drawHeader(bool animated);
void transitionToScreen(void (*drawFunc)());
void drawScreen();
void loadFirmwareList();
bool startWiFiServer();
void handleTouch();
bool executeFirmware(const String& path);
void showFirmwareOptions(int index);
bool confirmDelete();
void deleteFirmware(int index);
void deleteAllFirmware();
void showPopup(const char* message, uint16_t color, int duration);
void drawButton(int x, int y, int w, int h, const char* text, uint16_t color, bool rounded);

#endif