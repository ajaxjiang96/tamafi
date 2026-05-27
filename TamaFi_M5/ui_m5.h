#pragma once
#include <Arduino.h>
#include <M5Unified.h>

// ============ Enums & structs ============

void sndHatch();

enum Screen {
  SCREEN_BOOT,
  SCREEN_HATCH,
  SCREEN_HOME,
  SCREEN_MENU,
  SCREEN_PET_STATUS,
  SCREEN_ENVIRONMENT,
  SCREEN_SYSINFO,
  SCREEN_CONTROLS,
  SCREEN_SETTINGS,
  SCREEN_DIAGNOSTICS,
  SCREEN_GAMEOVER
};

enum Activity {
  ACT_NONE,
  ACT_HUNT,
  ACT_DISCOVER,
  ACT_REST
};

enum Stage {
  STAGE_BABY = 0,
  STAGE_TEEN = 1,
  STAGE_ADULT = 2,
  STAGE_ELDER = 3
};

enum Mood {
  MOOD_HUNGRY,
  MOOD_HAPPY,
  MOOD_CURIOUS,
  MOOD_BORED,
  MOOD_SICK,
  MOOD_EXCITED,
  MOOD_CALM
};

enum RestPhase {
  REST_NONE,
  REST_ENTER,
  REST_DEEP,
  REST_WAKE
};

struct Pet {
  int hunger;
  int happiness;
  int health;
  unsigned long ageMinutes;
  unsigned long ageHours;
  unsigned long ageDays;
};

struct WifiStats {
  int netCount    = 0;
  int strongCount = 0;
  int hiddenCount = 0;
  int avgRSSI     = -100;
  int openCount   = 0;
  int wpaCount    = 0;
};

// Sound sequencer note pattern
struct RetroSound {
  const int *freqs;
  const int *times;
  int length;
};

// ============ Display constants ============

#define DISP_W 466
#define DISP_H 466
#define DISP_CX 233
#define DISP_CY 233
#define DISP_R  233

#define PET_W 115
#define PET_H 110

// 240x240 box centered on 466x466 display
#define BOX_X 113
#define BOX_Y 113
#define BOX_W 240
#define BOX_H 240

// Touch zone: inside or outside the 240x240 box
#define TOUCH_LEFT_EDGE   113
#define TOUCH_RIGHT_EDGE  353
#define TOUCH_TOP_EDGE    113
#define TOUCH_BOTTOM_EDGE 353

// Pet sprite position (matches original)
extern int petPosX;
extern int petPosY;

// ============ Extern objects ============

extern LGFX_Sprite fb;
extern LGFX_Sprite petSprite;
extern LGFX_Sprite effectSprite;

// ============ Shared game state ============

extern Screen    currentScreen;
extern Activity  currentActivity;
extern RestPhase restPhase;

extern Pet       pet;
extern WifiStats wifiStats;

extern Mood      currentMood;
extern Stage     petStage;

extern bool      hungerEffectActive;
extern int       hungerEffectFrame;
extern bool      hasHatchedOnce;

extern bool      wifiScanInProgress;
extern unsigned long lastWifiScanTime;
extern unsigned long lastSaveTime;

extern bool      soundEnabled;
extern uint8_t   tftBrightnessIndex;
extern bool      autoSleep;
extern uint16_t  autoSaveMs;

extern uint8_t   traitCuriosity;
extern uint8_t   traitActivity;
extern uint8_t   traitStress;

extern unsigned long lastDecisionTime;
extern uint32_t      currentDecisionInterval;

extern int       restFrameIndex;

extern int       mainMenuIndex;
extern int       controlsIndex;
extern int       settingsMenuIndex;

extern bool      hatchTriggered;
extern bool      hapticEnabled;

// Mood ring state (replaces NeoPixel effects)
extern uint16_t  moodRingColor;
extern bool      moodRingOverride;
extern bool      moodRingPulse;
extern unsigned long moodRingFlashEnd;

// Rest phase tracking (used by UI for breathing animation)
extern unsigned long restPhaseStart;

// ============ UI API ============

void uiInit();
void uiOnScreenChange(Screen newScreen);
void uiDrawScreen(Screen screen,
                  int mainMenuIndex,
                  int controlsIndex,
                  int settingsMenuIndex);
