#include <M5StickCPlus.h>
#include <Preferences.h>
#include "Free_Fonts.h"

// Colors
#define GREY  0x0020    //   8  8  8
#define GREEN 0x0200    //   0 64  0
#define RED   0xF800    // 255  0  0

extern Preferences preferences;
void logger(String strLog, String strType);