#include <Arduino.h>
#include "ui_m5.h"
#include "ui_anim_m5.h"

#include "StoneGolem.h"
#include "egg_hatch.h"
#include "background.h"
#include "effect.h"

static const uint16_t* HUNGER_FRAMES[4] = {
    hunger1, hunger2, hunger3, hunger4
};

// Match original TamaFi layout exactly
static const int TFT_W = 240;
static const int TFT_H = 240;

// Hunting animation
static int huntFrame = 0;
static unsigned long lastHuntFrameTime = 0;
static const int HUNT_FRAME_DELAY = 300;

// Idle sprite sets per stage
static const uint16_t* BABY_IDLE_FRAMES[4]  = { idle_1, idle_2, idle_3, idle_4 };
static const uint16_t* TEEN_IDLE_FRAMES[4]  = { idle_1, idle_2, idle_3, idle_4 };
static const uint16_t* ADULT_IDLE_FRAMES[4] = { idle_1, idle_2, idle_3, idle_4 };
static const uint16_t* ELDER_IDLE_FRAMES[4] = { idle_1, idle_2, idle_3, idle_4 };

static const uint16_t* EGG_FRAMES[5] = {
    egg_hatch_1, egg_hatch_2, egg_hatch_3, egg_hatch_4, egg_hatch_5
};
static const uint16_t* EGG_IDLE_FRAMES[4] = {
    egg_hatch_11, egg_hatch_21, egg_hatch_31, egg_hatch_41
};
static const uint16_t* ATTACK_FRAMES[3] = {
    attack_0, attack_1, attack_2
};
static const uint16_t* DEAD_FRAMES[3] = {
    dead_1, dead_2, dead_3
};

// Local UI state
static int idleFrameUi = 0;
static unsigned long lastIdleFrameUi = 0;
static int eggIdleFrameUi = 0;
static unsigned long lastEggIdleTimeUi = 0;
static int hatchFrameUi = 0;
static unsigned long lastHatchFrameUi = 0;
static int deadFrameUi = 0;
static unsigned long lastDeadFrameUi = 0;

// ---------- Helpers matching original ui.cpp ----------
static const char* moodTextLocal(Mood m) {
  switch (m) {
    case MOOD_HUNGRY:  return "HUNGRY";
    case MOOD_HAPPY:   return "HAPPY";
    case MOOD_CURIOUS: return "CURIOUS";
    case MOOD_BORED:   return "BORED";
    case MOOD_SICK:    return "SICK";
    case MOOD_EXCITED: return "EXCITED";
    case MOOD_CALM:    return "CALM";
  }
  return "?";
}

static const char* stageTextLocal(Stage s) {
  switch (s) {
    case STAGE_BABY:  return "BABY";
    case STAGE_TEEN:  return "TEEN";
    case STAGE_ADULT: return "ADULT";
    case STAGE_ELDER: return "ELDER";
  }
  return "?";
}

static const char* activityTextLocal(Activity a) {
  switch (a) {
    case ACT_HUNT:     return "HUNTING WIFI...";
    case ACT_DISCOVER: return "DISCOVERING...";
    case ACT_REST:     return "RESTING...";
    default:           return "";
  }
}

static void drawHeader(const char* title) {
  fb.fillRect(0, 0, TFT_W, 18, TFT_BLACK);
  fb.drawLine(0, 18, TFT_W, 18, TFT_CYAN);
  fb.drawLine(0, 19, TFT_W, 19, TFT_MAGENTA);
  fb.fillRect(5, 6, 6, 6, TFT_WHITE);
  fb.fillRect(6, 7, 4, 4, TFT_BLACK);
  fb.setTextColor(TFT_WHITE);
  fb.setCursor(18, 5);
  fb.print(title);
}

static void drawBar(int x, int y, int w, int h, int value, uint16_t color) {
  fb.drawRect(x, y, w, h, TFT_WHITE);
  int fillWidth = (w - 2) * value / 100;
  if (fillWidth > 0) fb.fillRect(x + 1, y + 1, fillWidth, h - 2, color);
}

static void drawBubble(int x, int y, bool selected) {
  if (selected) {
    fb.fillCircle(x, y, 4, TFT_WHITE);
    fb.fillCircle(x, y, 2, TFT_BLACK);
  } else {
    fb.drawCircle(x, y, 4, TFT_WHITE);
  }
}

static const uint16_t** currentIdleSet() {
  switch (petStage) {
    case STAGE_BABY:  return BABY_IDLE_FRAMES;
    case STAGE_TEEN:  return TEEN_IDLE_FRAMES;
    case STAGE_ADULT: return ADULT_IDLE_FRAMES;
    case STAGE_ELDER: return ELDER_IDLE_FRAMES;
  }
  return BABY_IDLE_FRAMES;
}

// ---------- Virtual button rendering (outside 240x240 box) ----------
// ---------- Mood color helper ----------
static inline uint16_t moodColor() {
  switch (currentMood) {
    case MOOD_HUNGRY:  return TFT_RED;
    case MOOD_HAPPY:   return ((120>>3)<<11)|((40>>2)<<5)|(200>>3);
    case MOOD_CURIOUS: return TFT_ORANGE;
    case MOOD_BORED:   return ((80>>3)<<11)|((80>>2)<<5)|(100>>3);
    case MOOD_SICK:    return ((200>>3)<<11)|((0>>2)<<5)|(0>>3);
    case MOOD_EXCITED: return TFT_YELLOW;
    case MOOD_CALM:    return TFT_GREEN;
  }
  return TFT_WHITE;
}

// =========================================================================
// SCREENS — matching original ui.cpp layout exactly
// =========================================================================

// -------------------------------------------------------
// BOOT
// -------------------------------------------------------
static void screenBoot() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("TamaFi v2");

  fb.setTextColor(TFT_WHITE);
  fb.setCursor(20, 60);
  fb.print("WiFi-fed Virtual Pet");

  fb.setCursor(20, 100);
  fb.print("Press any button...");
}

// -------------------------------------------------------
// HATCH
// -------------------------------------------------------
static void screenHatch() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Hatching...");

  fb.pushImage(0, 18, TFT_W, TFT_H - 18, backgroundImage2);

  unsigned long now = millis();

  if (!hasHatchedOnce && !hatchTriggered) {
    if (now - lastEggIdleTimeUi >= EGG_IDLE_DELAY) {
      lastEggIdleTimeUi = now;
      eggIdleFrameUi = (eggIdleFrameUi + 1) % 4;
    }

    petSprite.pushImage(0, 0, 115, 110, EGG_IDLE_FRAMES[eggIdleFrameUi]);
    petSprite.pushSprite(&fb, 70, 80, TFT_WHITE);

    fb.setCursor(10, 200);
    fb.setTextColor(TFT_WHITE);
    fb.print("Press OK to hatch");

    return;
  }

  if (!hasHatchedOnce && hatchTriggered) {
    if (hatchFrameUi == 0) {
      sndHatch();
    }
    if (now - lastHatchFrameUi >= HATCH_DELAY) {
      lastHatchFrameUi = now;
      if (hatchFrameUi < 4) hatchFrameUi++;
      else {
        hasHatchedOnce = true;
        hatchTriggered = false;
        hatchFrameUi = 0;
        currentScreen = SCREEN_HOME;
        uiOnScreenChange(currentScreen);
        return;
      }
    }

    petSprite.pushImage(0, 0, 115, 110, EGG_FRAMES[hatchFrameUi]);
    petSprite.pushSprite(&fb, 70, 80, TFT_WHITE);

    fb.setCursor(10, 200);
    fb.setTextColor(TFT_WHITE);
    fb.print("Hatching...");

    return;
  }

  currentScreen = SCREEN_HOME;
  uiOnScreenChange(currentScreen);
}

// -------------------------------------------------------
// HOME
// -------------------------------------------------------
static void drawStatsBlock() {
  int x = 20, y = 100, w = 80, h = 8;

  drawBar(x, y,       w, h, pet.hunger,    TFT_RED);
  drawBar(x, y + 28,  w, h, pet.happiness, TFT_YELLOW);
  drawBar(x, y + 56,  w, h, pet.health,    TFT_GREEN);

  fb.setTextColor(TFT_BLACK);
  fb.setCursor(x + 3, y + 75);
  fb.print("Mood:  ");
  fb.print(moodTextLocal(currentMood));

  fb.setCursor(x + 3, y + 89);
  fb.print("Stage: ");
  fb.print(stageTextLocal(petStage));
}

static void screenHome() {
  fb.fillSprite(TFT_BLACK);

  if (currentActivity != ACT_NONE)
    drawHeader(activityTextLocal(currentActivity));
  else
    drawHeader("Idle");

  fb.pushImage(0, 18, TFT_W, TFT_H - 18, backgroundImage);

  unsigned long now = millis();

  // Rest animation
  if (currentActivity == ACT_REST && restPhase != REST_NONE) {
    int frameIdx = 0;
    if (restPhase == REST_ENTER) {
      frameIdx = 4 - constrain(restFrameIndex, 0, 4);
    } else if (restPhase == REST_DEEP) {
      frameIdx = 0;
    } else if (restPhase == REST_WAKE) {
      frameIdx = constrain(restFrameIndex, 0, 4);
    }

    petSprite.pushImage(0, 0, 115, 110, EGG_FRAMES[frameIdx]);
    petSprite.pushSprite(&fb, petPosX, petPosY, TFT_WHITE);

    drawStatsBlock();

    if (hungerEffectActive) {
      effectSprite.pushImage(0, 0, 100, 95, HUNGER_FRAMES[hungerEffectFrame]);
      effectSprite.pushSprite(&fb, 120, 90, TFT_WHITE);
    }
    return;
  }

  // Hunting animation
  if (currentActivity == ACT_HUNT) {
    if (now - lastHuntFrameTime >= HUNT_FRAME_DELAY) {
      lastHuntFrameTime = now;
      huntFrame = (huntFrame + 1) % 3;
    }

    petSprite.pushImage(0, 0, 115, 110, ATTACK_FRAMES[huntFrame]);
    petSprite.pushSprite(&fb, petPosX, petPosY, TFT_WHITE);

    drawStatsBlock();

    if (hungerEffectActive) {
      effectSprite.pushImage(0, 0, 100, 95, HUNGER_FRAMES[hungerEffectFrame]);
      effectSprite.pushSprite(&fb, 120, 90, TFT_WHITE);
    }
    return;
  }

  // Idle animation
  int idleSpeed = IDLE_BASE_DELAY;
  if (currentMood == MOOD_EXCITED) idleSpeed = IDLE_FAST_DELAY;
  if (currentMood == MOOD_BORED || currentMood == MOOD_SICK) idleSpeed = IDLE_SLOW_DELAY;

  if (now - lastIdleFrameUi >= (unsigned long)idleSpeed) {
    lastIdleFrameUi = now;
    idleFrameUi = (idleFrameUi + 1) % 4;
  }

  const uint16_t** idleSet = currentIdleSet();
  petSprite.pushImage(0, 0, 115, 110, idleSet[idleFrameUi]);
  petSprite.pushSprite(&fb, petPosX, petPosY, TFT_WHITE);

  drawStatsBlock();

  if (hungerEffectActive) {
    effectSprite.pushImage(0, 0, 100, 95, HUNGER_FRAMES[hungerEffectFrame]);
    effectSprite.pushSprite(&fb, 120, 90, TFT_WHITE);
  }
}

// -------------------------------------------------------
// MAIN MENU
// -------------------------------------------------------
static void screenMenu(int mainMenuIdx) {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Main Menu");

  int baseY = 30;
  int step  = 20;
  int highlightY = baseY + mainMenuIdx * step - 5;

  fb.fillRect(8, highlightY, 224, 18, TFT_DARKGREY);
  fb.drawRect(8, highlightY, 224, 18, TFT_CYAN);

  const char* items[] = {
    "Pet Status", "Environment", "System Info",
    "Controls", "Settings", "Diagnostics", "Back"
  };

  for (int i = 0; i < 7; i++) {
    int y = baseY + i * step;
    fb.setCursor(40, y);
    fb.setTextColor(i == mainMenuIdx ? TFT_YELLOW : TFT_WHITE);
    fb.print(items[i]);

    // Simple icon
    if (i == mainMenuIdx) {
      fb.fillRect(16, y + 2, 4, 4, TFT_YELLOW);
    }
  }

  fb.setCursor(10, 200);
  fb.setTextColor(TFT_WHITE);
  fb.print("UP/DOWN = move | OK = select");
}

// -------------------------------------------------------
// PET STATUS
// -------------------------------------------------------
static void screenPetStatus() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Pet Status");

  fb.setTextColor(TFT_WHITE);

  fb.setCursor(10, 26);
  fb.print("Stage: "); fb.print(stageTextLocal(petStage));

  fb.setCursor(10, 38);
  fb.print("Age:   ");
  fb.print(pet.ageDays); fb.print("d ");
  fb.print(pet.ageHours); fb.print("h ");
  fb.print(pet.ageMinutes); fb.print("m");

  fb.setCursor(10, 56);
  fb.print("Hunger: "); fb.print(pet.hunger); fb.print("%");

  fb.setCursor(10, 68);
  fb.print("Happy:  "); fb.print(pet.happiness); fb.print("%");

  fb.setCursor(10, 80);
  fb.print("Health: "); fb.print(pet.health); fb.print("%");

  fb.setCursor(10, 98);
  fb.print("Mood:   "); fb.print(moodTextLocal(currentMood));

  fb.setCursor(10, 116);
  fb.print("Personality:");

  fb.setCursor(16, 130);
  fb.print("Curiosity: "); fb.print((int)traitCuriosity);

  fb.setCursor(16, 142);
  fb.print("Activity : "); fb.print((int)traitActivity);

  fb.setCursor(16, 154);
  fb.print("Stress   : "); fb.print((int)traitStress);

  fb.setCursor(10, 200);
  fb.print("OK = Back");
}

// -------------------------------------------------------
// ENVIRONMENT
// -------------------------------------------------------
static void screenEnvironment() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Environment");

  fb.setTextColor(TFT_WHITE);

  fb.setCursor(10, 30);
  fb.print("Networks : "); fb.print(wifiStats.netCount);

  fb.setCursor(10, 42);
  fb.print("Strong   : "); fb.print(wifiStats.strongCount);

  fb.setCursor(10, 54);
  fb.print("Hidden   : "); fb.print(wifiStats.hiddenCount);

  fb.setCursor(10, 66);
  fb.print("Open     : "); fb.print(wifiStats.openCount);

  fb.setCursor(10, 78);
  fb.print("WPA/etc  : "); fb.print(wifiStats.wpaCount);

  fb.setCursor(10, 94);
  fb.print("Avg RSSI : "); fb.print(wifiStats.avgRSSI);

  fb.setCursor(10, 200);
  fb.print("OK = Back");
}

// -------------------------------------------------------
// SYSTEM INFO
// -------------------------------------------------------
static void screenSysInfo() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("System Info");

  fb.setTextColor(TFT_WHITE);

  fb.setCursor(10, 30);
  fb.print("Firmware: 2.0");

  fb.setCursor(10, 42);
  fb.print("MCU:      ESP32-S3");

  fb.setCursor(10, 54);
  fb.print("Heap Free: ");
  fb.print(ESP.getFreeHeap() / 1024); fb.print(" KB");

  unsigned long s = millis() / 1000;
  unsigned long m = s / 60;
  unsigned long h = m / 60;
  s %= 60; m %= 60;

  fb.setCursor(10, 72);
  fb.print("Uptime: ");
  fb.printf("%02lu:%02lu:%02lu", h, m, s);

  fb.setCursor(10, 90);
  fb.print("WiFi Scan: ");
  fb.print(wifiScanInProgress ? "Running" : "Idle");

  fb.setCursor(10, 200);
  fb.print("OK = Back");
}

// -------------------------------------------------------
// CONTROLS
// -------------------------------------------------------
static void screenControls(int controlsIdx) {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Controls");

  int baseY = 30;
  int step  = 20;
  int highlightY = baseY + controlsIdx * step - 5;

  fb.fillRect(8, highlightY, 224, 18, TFT_DARKGREY);
  fb.drawRect(8, highlightY, 224, 18, TFT_CYAN);

  const char* labels[] = {
    "Screen Brightness",
    "Sound",
    "Haptic Feedback",
    "Back"
  };

  for (int i = 0; i < 4; i++) {
    int y = baseY + i * step;

    drawBubble(14, y - 2, i == controlsIdx);

    fb.setCursor(30, y - 4);
    fb.setTextColor(i == controlsIdx ? TFT_YELLOW : TFT_WHITE);
    fb.print(labels[i]);

    fb.setCursor(150, y - 4);
    fb.setTextColor(TFT_CYAN);

    switch (i) {
      case 0:
        fb.print(tftBrightnessIndex == 0 ? "Low" : tftBrightnessIndex == 1 ? "Mid" : "High");
        break;
      case 1:
        fb.print(soundEnabled ? "On" : "Off");
        break;
      case 2:
        fb.print(hapticEnabled ? "On" : "Off");
        break;
    }
  }

  fb.setCursor(10, 200);
  fb.print("OK = Select/Back");
}

// -------------------------------------------------------
// SETTINGS
// -------------------------------------------------------
static void screenSettings(int settingsIdx) {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Settings");

  int baseY = 30;
  int step  = 18;
  int highlightY = baseY + settingsIdx * step - 5;

  fb.fillRect(8, highlightY, 224, 18, TFT_DARKGREY);
  fb.drawRect(8, highlightY, 224, 18, TFT_CYAN);

  const char* labels[] = {
    "Theme", "Auto Sleep", "Auto Save",
    "Reset Pet", "Reset All", "Back"
  };

  for (int i = 0; i < 6; i++) {
    int y = baseY + i * step;

    drawBubble(14, y - 2, i == settingsIdx);

    fb.setCursor(30, y - 4);
    fb.setTextColor(i == settingsIdx ? TFT_YELLOW : TFT_WHITE);
    fb.print(labels[i]);

    fb.setCursor(150, y - 4);
    fb.setTextColor(TFT_CYAN);

    switch (i) {
      case 0: fb.print("Pixel"); break;
      case 1: fb.print(autoSleep ? "On" : "Off"); break;
      case 2: fb.print(autoSaveMs / 1000); fb.print("s"); break;
    }
  }

  fb.setCursor(10, 200);
  fb.print("OK = Select");
}

// -------------------------------------------------------
// DIAGNOSTICS
// -------------------------------------------------------
static void screenDiagnostics() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Diagnostics");

  fb.setTextColor(TFT_WHITE);

  fb.setCursor(10, 30);
  fb.print("Activity: ");
  fb.print(activityTextLocal(currentActivity));

  fb.setCursor(10, 42);
  fb.print("Mood: ");
  fb.print(moodTextLocal(currentMood));

  fb.setCursor(10, 54);
  fb.print("RestPhase: ");
  fb.print(restPhase == REST_ENTER ? "ENTER" :
           restPhase == REST_DEEP  ? "DEEP" :
           restPhase == REST_WAKE  ? "WAKE" : "NONE");

  fb.setCursor(10, 66);
  fb.print("WiFi Scan: ");
  fb.print(wifiScanInProgress ? "Running" : "Idle");

  fb.setCursor(10, 200);
  fb.print("OK = Back");
}

// -------------------------------------------------------
// GAME OVER
// -------------------------------------------------------
static void screenGameOver() {
  fb.fillSprite(TFT_BLACK);
  drawHeader("Game Over");

  fb.pushImage(0, 18, TFT_W, TFT_H - 18, backgroundImage);

  unsigned long now = millis();
  if (now - lastDeadFrameUi >= DEAD_DELAY) {
    lastDeadFrameUi = now;
    deadFrameUi++;
    if (deadFrameUi > 2) deadFrameUi = 2;
  }

  petSprite.pushImage(0, 0, 115, 110, DEAD_FRAMES[deadFrameUi]);
  petSprite.pushSprite(&fb, petPosX, petPosY, TFT_WHITE);

  fb.setCursor(10, 200);
  fb.setTextColor(TFT_WHITE);
  fb.print("OK = Restart");
}

// =========================================================================
// PUBLIC UI API
// =========================================================================
void uiInit() {
  idleFrameUi = 0;
  lastIdleFrameUi = millis();
  eggIdleFrameUi = 0;
  lastEggIdleTimeUi = millis();
  hatchFrameUi = 0;
  lastHatchFrameUi = millis();
  deadFrameUi = 0;
  lastDeadFrameUi = millis();
}

void uiOnScreenChange(Screen newScreen) {
  if (newScreen == SCREEN_HATCH) {
    eggIdleFrameUi = hatchFrameUi = 0;
  }
}

void uiDrawScreen(Screen screen,
                  int mainMenuIdx,
                  int controlsIdx,
                  int settingsIdx)
{
  switch (screen) {
    case SCREEN_BOOT:        screenBoot(); break;
    case SCREEN_HATCH:       screenHatch(); break;
    case SCREEN_HOME:        screenHome(); break;
    case SCREEN_MENU:        screenMenu(mainMenuIdx); break;
    case SCREEN_PET_STATUS:  screenPetStatus(); break;
    case SCREEN_ENVIRONMENT: screenEnvironment(); break;
    case SCREEN_SYSINFO:     screenSysInfo(); break;
    case SCREEN_CONTROLS:    screenControls(controlsIdx); break;
    case SCREEN_SETTINGS:    screenSettings(settingsIdx); break;
    case SCREEN_DIAGNOSTICS: screenDiagnostics(); break;
    case SCREEN_GAMEOVER:    screenGameOver(); break;
  }

  // Push the 240x240 game UI centered onto the round display
  int xOff = (DISP_W - TFT_W) / 2;
  int yOff = (DISP_H - TFT_H) / 2;
  fb.pushSprite(&M5.Display, xOff, yOff);

  // 2px #666666 border around the analog screen
  uint16_t frameColor = M5.Display.color565(0x66, 0x66, 0x66);
  M5.Display.drawRect(xOff - 1, yOff - 1, TFT_W + 2, TFT_H + 2, frameColor);
  M5.Display.drawRect(xOff - 2, yOff - 2, TFT_W + 4, TFT_H + 4, frameColor);

  // Mood-colored corner accents
  uint16_t moodC = moodRingOverride ? moodRingColor : moodColor();
  if (currentActivity == ACT_REST && restPhase == REST_DEEP) {
    float phase = (millis() - restPhaseStart) / (float)REST_BREATHE_MS;
    int breathe = constrain((int)(sin(phase * 2.0f * PI) * 50.0f + 60.0f), 20, 200);
    moodC = M5.Display.color565(0, 0, breathe);
  }
  M5.Display.drawPixel(xOff - 2, yOff - 2, moodC);
  M5.Display.drawPixel(xOff + TFT_W + 1, yOff - 2, moodC);
  M5.Display.drawPixel(xOff - 2, yOff + TFT_H + 1, moodC);
  M5.Display.drawPixel(xOff + TFT_W + 1, yOff + TFT_H + 1, moodC);

  // WiFi scan dots — 4 big dots spanning the full 240px width above the screen
  if (wifiScanInProgress) {
    int dot = (millis() / 150) % 4;
    int gap = 12;  // distance above the screen border
    int dotY = yOff - gap - 3;
    int dotSpacing = TFT_W / 4;
    for (int i = 0; i < 4; i++) {
      int dotX = xOff + dotSpacing / 2 + i * dotSpacing;
      if (i == dot) {
        M5.Display.fillCircle(dotX, dotY, 3, TFT_BLUE);
      } else {
        M5.Display.fillCircle(dotX, dotY, 2, TFT_DARKGREY);
      }
    }
  }

  // "TamaFi v2" label centered below the screen
  M5.Display.setTextColor(M5.Display.color565(0x44, 0x44, 0x44));
  M5.Display.drawString("TamaFi v2", DISP_CX - 30, yOff + TFT_H + 8);

  // Virtual button labels (in outer ring area)
  M5.Display.setTextColor(TFT_DARKGREY);

  // LEFT side buttons
  M5.Display.drawString("UP", 62, yOff + 5);
  M5.Display.drawString("OK", 54, DISP_CY - 6);
  M5.Display.drawString("DN", 62, yOff + TFT_H - 15);

  // RIGHT side buttons
  M5.Display.drawString("R1", DISP_W - 72, yOff + 30);
  M5.Display.drawString("R2", DISP_W - 72, DISP_CY + 40);
  M5.Display.drawString("R3", DISP_W - 72, yOff + TFT_H - 55);
}
