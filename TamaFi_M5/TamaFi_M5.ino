#include <Arduino.h>
#include <WiFi.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>

#include "ui_m5.h"
#include "ui_anim_m5.h"

// ============ Sprite data (shared from original) ============
#include "StoneGolem.h"
#include "egg_hatch.h"

// --- Hatching state ---
bool hasHatchedOnce = false;
bool hatchTriggered = false;

// --------- Global objects ---------
LGFX_Sprite fb(&M5.Display);
LGFX_Sprite petSprite(&M5.Display);
LGFX_Sprite effectSprite(&M5.Display);

int petPosX = 120;
int petPosY = 90;

Preferences prefs;

// --------- Shared game state ---------
Screen    currentScreen = SCREEN_BOOT;
Activity  currentActivity = ACT_NONE;
RestPhase restPhase = REST_NONE;

Pet       pet;
WifiStats wifiStats;

Mood      currentMood = MOOD_CALM;
Stage     petStage    = STAGE_BABY;

bool      hungerEffectActive = false;
int       hungerEffectFrame  = 0;

bool      wifiScanInProgress = false;
unsigned long lastWifiScanTime = 0;
unsigned long lastSaveTime     = 0;

bool      soundEnabled     = true;
uint8_t   tftBrightnessIndex = 1;
bool      autoSleep          = true;
uint16_t  autoSaveMs         = 30000;

uint8_t   traitCuriosity = 70;
uint8_t   traitActivity  = 60;
uint8_t   traitStress    = 40;

unsigned long lastDecisionTime      = 0;
uint32_t      currentDecisionInterval = 10000;

int       restFrameIndex = 0;

// Haptic feedback
bool      hapticEnabled = true;

// Mood ring state (replaces NeoPixel)
uint16_t  moodRingColor    = 0;
bool      moodRingOverride = false;
bool      moodRingPulse    = false;
unsigned long moodRingFlashEnd = 0;

// --------- Internal logic timers ---------
unsigned long hungerTimer    = 0;
unsigned long happinessTimer = 0;
unsigned long healthTimer    = 0;
unsigned long ageTimer       = 0;
unsigned long lastLogicTick  = 0;

// Rest
unsigned long lastRestAnimTime = 0;
unsigned long restPhaseStart   = 0;
unsigned long restDurationMs   = 0;
bool          restStatsApplied = false;

// Hunger overlay
unsigned long lastHungerFrameTime = 0;

// Death
unsigned long lastDeadFrameTime = 0;

// Menus
int mainMenuIndex     = 0;
int controlsIndex     = 0;
int settingsMenuIndex = 0;

// Touch tracking
static int  touchStartX = -1, touchStartY = -1;
static bool touchWasDown = false;
static unsigned long touchDownTime = 0;
static const unsigned long LONG_PRESS_MS = 500;
static const int SWIPE_THRESHOLD = 40;

// Vibration
unsigned long vibratorEndTime = 0;

// Wifi decision randomness
const uint32_t DECISION_INTERVAL_MIN = 8000;
const uint32_t DECISION_INTERVAL_MAX = 15000;

// ------- Forward declarations -------
void sndClick();
void sndGoodFeed();
void sndBadFeed();
void sndDiscover();
void sndRestStart();
void sndRestEnd();

// ---------- Mood ring helpers (replace NeoPixel functions) ----------
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void setMoodRing(uint16_t color, unsigned long durationMs) {
  moodRingColor = color;
  moodRingOverride = true;
  moodRingFlashEnd = (durationMs > 0) ? millis() + durationMs : 0;
}

void clearMoodRing() {
  moodRingOverride = false;
  moodRingFlashEnd = 0;
}

static void ringHappy()   { setMoodRing(rgb565(120, 40, 200), 800); }
static void ringSad()     { setMoodRing(rgb565(200, 0, 0), 800); }
static void ringWifi()    { setMoodRing(rgb565(0, 90, 255), 600); }
static void ringRest()    { setMoodRing(rgb565(0, 25, 90), 0); }
static void ringOff()     { clearMoodRing(); }

// ---------- Vibration ----------
void vibratorPulse(uint8_t intensity, unsigned long ms) {
  if (!hapticEnabled) return;
  M5.Power.setVibration(intensity);
  vibratorEndTime = millis() + ms;
}

// ---------- Sound sequencer ----------

int sndIndex = -1;
int sndStep  = 0;
unsigned long sndNext = 0;

const int CLICK_FREQS[] = { 2100, 1600, 900 };
const int CLICK_TIMES[] = {  20,   20,   20 };
RetroSound SND_CLICK = { CLICK_FREQS, CLICK_TIMES, 3 };

const int GOOD_FREQS[] = { 600, 900, 1200, 1500 };
const int GOOD_TIMES[] = { 40,  40,  40,   60   };
RetroSound SND_GOOD = { GOOD_FREQS, GOOD_TIMES, 4 };

const int BAD_FREQS[] = { 900, 700, 500, 300 };
const int BAD_TIMES[] = { 50,  50,  60,  80   };
RetroSound SND_BAD = { BAD_FREQS, BAD_TIMES, 4 };

const int DISC_FREQS[] = { 400, 650, 900, 1200, 1500 };
const int DISC_TIMES[] = { 40,  40,  40,  40,   60    };
RetroSound SND_DISC = { DISC_FREQS, DISC_TIMES, 5 };

const int REST_START_FREQS[] = { 600, 400, 300 };
const int REST_START_TIMES[] = { 60,  70,  90  };
RetroSound SND_REST_START = { REST_START_FREQS, REST_START_TIMES, 3 };

const int REST_END_FREQS[] = { 300, 500, 700 };
const int REST_END_TIMES[] = { 60,  60,  80  };
RetroSound SND_REST_END = { REST_END_FREQS, REST_END_TIMES, 3 };

const int HATCH_FREQS[] = { 500, 800, 1200, 1600, 2000 };
const int HATCH_TIMES[] = {  60,  60,   60,   80,  100 };
RetroSound SND_HATCH = { HATCH_FREQS, HATCH_TIMES, 5 };

static RetroSound* getSoundByIndex(int idx) {
  switch (idx) {
    case 0: return &SND_CLICK;
    case 1: return &SND_GOOD;
    case 2: return &SND_BAD;
    case 3: return &SND_DISC;
    case 4: return &SND_REST_START;
    case 5: return &SND_REST_END;
    case 6: return &SND_HATCH;
    default: return nullptr;
  }
}

void sndUpdate() {
  if (!soundEnabled) {
    M5.Speaker.stop();
    sndIndex = -1;
    sndStep = 0;
    return;
  }

  if (sndIndex < 0) return;

  unsigned long now = millis();
  if (now >= sndNext) {
    RetroSound *snd = getSoundByIndex(sndIndex);
    if (!snd || sndStep >= snd->length) {
      M5.Speaker.stop();
      sndIndex = -1;
      sndStep = 0;
      return;
    }

    M5.Speaker.tone(snd->freqs[sndStep], snd->times[sndStep], false);
    sndNext = now + snd->times[sndStep];
    sndStep++;
  }
}

void sndClick()    { if (!soundEnabled) return; sndIndex = 0; sndStep = 0; vibratorPulse(50, 20); }
void sndGoodFeed() { if (!soundEnabled) return; sndIndex = 1; sndStep = 0; ringHappy(); vibratorPulse(70, 60); }
void sndBadFeed()  { if (!soundEnabled) return; sndIndex = 2; sndStep = 0; ringSad(); }
void sndDiscover() { if (!soundEnabled) return; sndIndex = 3; sndStep = 0; ringWifi(); }
void sndRestStart(){ if (!soundEnabled) return; sndIndex = 4; sndStep = 0; }
void sndRestEnd()  { if (!soundEnabled) return; sndIndex = 5; sndStep = 0; }
void sndHatch()    { if (!soundEnabled) return; sndIndex = 6; sndStep = 0; }

// ---------- Display brightness ----------
void applyTftBrightness() {
  uint8_t val = (tftBrightnessIndex == 0) ? 60 :
                (tftBrightnessIndex == 1) ? 150 : 255;
  M5.Display.setBrightness(val);
}

// ---------- WiFi scan ----------
void startWifiScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.scanNetworks(true);
  wifiScanInProgress = true;
}

bool checkWifiScanDone() {
  if (!wifiScanInProgress) return false;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;

  wifiScanInProgress = false;
  lastWifiScanTime = millis();

  if (n < 0) {
    wifiStats = WifiStats();
    WiFi.scanDelete();
    return true;
  }

  WifiStats s;
  s.netCount    = n;
  s.strongCount = 0;
  s.hiddenCount = 0;
  s.openCount   = 0;
  s.wpaCount    = 0;
  int totalRSSI = 0;

  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    totalRSSI += rssi;
    if (rssi > -60) s.strongCount++;
    if (WiFi.SSID(i).length() == 0) s.hiddenCount++;
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) s.openCount++;
    else s.wpaCount++;
  }

  s.avgRSSI = (n > 0) ? (totalRSSI / n) : -100;
  wifiStats = s;

  WiFi.scanDelete();
  return true;
}

// ---------- Persistence ----------
void saveState() {
  prefs.putInt("hunger", pet.hunger);
  prefs.putInt("happy",  pet.happiness);
  prefs.putInt("health", pet.health);

  prefs.putULong("ageMin", pet.ageMinutes);
  prefs.putULong("ageHr",  pet.ageHours);
  prefs.putULong("ageDay", pet.ageDays);

  prefs.putUChar("stage",  (uint8_t)petStage);
  prefs.putBool("hatched", hasHatchedOnce);

  prefs.putBool("sound", soundEnabled);
  prefs.putUChar("tftBri", tftBrightnessIndex);
  prefs.putBool("haptic", hapticEnabled);

  prefs.putUChar("tCur", traitCuriosity);
  prefs.putUChar("tAct", traitActivity);
  prefs.putUChar("tStr", traitStress);
}

void loadState() {
  int h = prefs.getInt("hunger", -1);
  if (h == -1) {
    pet.hunger     = 70;
    pet.happiness  = 70;
    pet.health     = 70;
    pet.ageMinutes = 0;

    petStage       = STAGE_BABY;
    hasHatchedOnce = false;

    soundEnabled     = true;
    tftBrightnessIndex = 1;
    hapticEnabled    = true;

    traitCuriosity = random(40, 90);
    traitActivity  = random(30, 90);
    traitStress    = random(20, 80);

    saveState();
    return;
  }

  pet.hunger     = prefs.getInt("hunger", 70);
  pet.happiness  = prefs.getInt("happy",  70);
  pet.health     = prefs.getInt("health", 70);

  pet.ageMinutes = prefs.getULong("ageMin", 0);
  pet.ageHours   = prefs.getULong("ageHr", 0);
  pet.ageDays    = prefs.getULong("ageDay", 0);

  petStage       = (Stage)prefs.getUChar("stage", (uint8_t)STAGE_BABY);
  hasHatchedOnce = prefs.getBool("hatched", false);

  soundEnabled     = prefs.getBool("sound", true);
  tftBrightnessIndex = prefs.getUChar("tftBri", 1);
  hapticEnabled    = prefs.getBool("haptic", true);

  traitCuriosity = prefs.getUChar("tCur", 70);
  traitActivity  = prefs.getUChar("tAct", 60);
  traitStress    = prefs.getUChar("tStr", 40);
}

// ---------- Mood & evolution ----------
void updateMood() {
  if (pet.health < 25 || (wifiStats.netCount == 0 && lastWifiScanTime > 0 &&
                          millis() - lastWifiScanTime > 60000)) {
    currentMood = MOOD_SICK;
    return;
  }

  if (pet.hunger < 25) {
    currentMood = MOOD_HUNGRY;
    return;
  }

  if (pet.happiness > 80 && wifiStats.netCount > 8) {
    currentMood = MOOD_EXCITED;
    return;
  }

  if (pet.happiness > 60 && wifiStats.netCount > 0) {
    currentMood = MOOD_HAPPY;
    return;
  }

  if (wifiStats.netCount == 0 && millis() - lastWifiScanTime > 30000) {
    currentMood = MOOD_BORED;
    return;
  }

  if (wifiStats.hiddenCount > 0 || wifiStats.openCount > 0) {
    currentMood = MOOD_CURIOUS;
    return;
  }

  currentMood = MOOD_CALM;
}

void updateEvolution() {
  unsigned long a = pet.ageMinutes;
  int avg = (pet.hunger + pet.happiness + pet.health) / 3;

  if (a >= 180 && avg > 40 && petStage < STAGE_ELDER) {
    petStage = STAGE_ELDER;
    sndDiscover();
  } else if (a >= 60 && avg > 45 && petStage < STAGE_ADULT) {
    petStage = STAGE_ADULT;
    sndDiscover();
  } else if (a >= 20 && avg > 35 && petStage < STAGE_TEEN) {
    petStage = STAGE_TEEN;
    sndDiscover();
  }
}

// ---------- Rest state machine ----------
void stepRest() {
  if (currentActivity != ACT_REST || restPhase == REST_NONE) return;

  unsigned long now = millis();

  switch (restPhase) {
    case REST_ENTER:
      if (now - lastRestAnimTime >= REST_ENTER_DELAY) {
        lastRestAnimTime = now;
        if (restFrameIndex > 0) {
          restFrameIndex--;
        } else {
          restFrameIndex   = 0;
          restPhase        = REST_DEEP;
          restPhaseStart   = now;
          restStatsApplied = false;
        }
      }
      break;

    case REST_DEEP:
      moodRingPulse = true;
      if (!restStatsApplied && now - restPhaseStart > restDurationMs / 2) {
        pet.hunger    = constrain(pet.hunger - 3, 0, 100);
        pet.happiness = constrain(pet.happiness + 10, 0, 100);
        pet.health    = constrain(pet.health + 15, 0, 100);
        restStatsApplied = true;
      }
      if (now - restPhaseStart >= restDurationMs) {
        restPhase        = REST_WAKE;
        restPhaseStart   = now;
        lastRestAnimTime = now;
        sndRestEnd();
        ringOff();
        moodRingPulse = false;
        restFrameIndex = 0;
      }
      break;

    case REST_WAKE:
      if (now - lastRestAnimTime >= REST_WAKE_DELAY) {
        lastRestAnimTime = now;
        if (restFrameIndex < 4) {
          restFrameIndex++;
        } else {
          restFrameIndex   = 4;
          restPhase        = REST_NONE;
          currentActivity  = ACT_NONE;
          moodRingPulse    = false;
        }
      }
      break;

    default:
      break;
  }
}

// ---------- WiFi-based activities ----------
void resolveHunt() {
  int n = wifiStats.netCount;
  int hungerDelta = 0;
  int happyDelta  = 0;
  int healthDelta = 0;

  if (n == 0) {
    hungerDelta = -15;
    happyDelta  = -10;
    healthDelta = -5;
    sndBadFeed();
  } else {
    hungerDelta = min(35, n * 2 + wifiStats.strongCount * 3);
    int varietyScore = wifiStats.hiddenCount * 2 + wifiStats.openCount;
    happyDelta = min(30, varietyScore * 3 + (wifiStats.avgRSSI + 100) / 3);

    if (wifiStats.avgRSSI > -75) healthDelta += 5;
    if (wifiStats.avgRSSI > -65) healthDelta += 5;
    if (wifiStats.strongCount > 5) healthDelta += 3;

    sndGoodFeed();
  }

  pet.hunger    = constrain(pet.hunger + hungerDelta, 0, 100);
  pet.happiness = constrain(pet.happiness + happyDelta, 0, 100);
  pet.health    = constrain(pet.health + healthDelta, 0, 100);

  hungerEffectActive    = true;
  hungerEffectFrame     = 0;
  lastHungerFrameTime   = millis();
}

void resolveDiscover() {
  int n = wifiStats.netCount;
  int happyDelta  = 0;
  int hungerDelta = 0;

  if (n == 0) {
    happyDelta  = -5;
    hungerDelta = -3;
    sndBadFeed();
  } else {
    int curiosity = wifiStats.hiddenCount * 4 + wifiStats.openCount * 3;
    curiosity += wifiStats.netCount;
    happyDelta  = min(35, curiosity / 2);
    hungerDelta = -5;
    sndDiscover();
  }

  pet.happiness = constrain(pet.happiness + happyDelta, 0, 100);
  pet.hunger    = constrain(pet.hunger + hungerDelta, 0, 100);
}

// ---------- Autonomous decisions ----------
void decideNextActivity() {
  if (currentActivity != ACT_NONE || restPhase != REST_NONE) return;

  unsigned long now = millis();
  if (now - lastDecisionTime < currentDecisionInterval) return;

  lastDecisionTime = now;
  currentDecisionInterval = random(DECISION_INTERVAL_MIN, DECISION_INTERVAL_MAX);

  int desireHunt = 0;
  int desireDisc = 0;
  int desireRest = 0;
  int desireIdle = 10;

  desireHunt = (100 - pet.hunger) + traitCuriosity / 2;
  if (wifiStats.netCount == 0) desireHunt /= 2;

  desireDisc = traitCuriosity + wifiStats.hiddenCount * 10 + wifiStats.openCount * 6 +
               wifiStats.netCount * 2 + random(0, 20);
  if (wifiStats.netCount == 0) desireDisc /= 2;

  desireRest = (100 - pet.health) + traitStress / 2;
  if (pet.hunger < 20) desireRest -= 10;

  if (currentMood == MOOD_HUNGRY) {
    desireHunt += 20;
    desireRest -= 10;
  }
  if (currentMood == MOOD_CURIOUS) {
    desireDisc += 15;
  }
  if (currentMood == MOOD_SICK) {
    desireRest += 20;
    desireDisc -= 10;
  }
  if (currentMood == MOOD_EXCITED) {
    desireDisc += 10;
    desireHunt += 5;
  }
  if (currentMood == MOOD_BORED) {
    desireDisc += 10;
    desireHunt += 5;
  }

  desireHunt = max(desireHunt, 0);
  desireDisc = max(desireDisc, 0);
  desireRest = max(desireRest, 0);
  desireIdle = max(desireIdle, 0);

  int best = desireIdle;
  Activity chosen = ACT_NONE;

  if (desireHunt > best) { best = desireHunt; chosen = ACT_HUNT; }
  if (desireDisc > best) { best = desireDisc; chosen = ACT_DISCOVER; }
  if (desireRest > best) { best = desireRest; chosen = ACT_REST; }

  if (chosen == ACT_NONE) return;

  if (chosen == ACT_HUNT || chosen == ACT_DISCOVER) {
    currentActivity = chosen;
    ringWifi();
    startWifiScan();
  } else if (chosen == ACT_REST) {
    currentActivity  = ACT_REST;
    restPhase        = REST_ENTER;
    restFrameIndex   = 4;
    lastRestAnimTime = millis();
    restPhaseStart   = millis();
    restDurationMs   = random(REST_MIN_DURATION, REST_MAX_DURATION);
    restStatsApplied = false;
    sndRestStart();
    ringRest();
  }
}

// ---------- Reset pet ----------
void resetPet(bool fullReset) {
  pet.hunger    = 70;
  pet.happiness = 70;
  pet.health    = 70;
  if (fullReset) pet.ageMinutes = 0;

  wifiStats = WifiStats();
  lastWifiScanTime = 0;

  unsigned long now = millis();
  hungerTimer    = now;
  happinessTimer = now;
  healthTimer    = now;
  ageTimer       = now;

  currentActivity    = ACT_NONE;
  restPhase          = REST_NONE;
  hungerEffectActive = false;
  wifiScanInProgress = false;
  ringOff();
}

// ---------- Logic tick ----------
void logicTick() {
  unsigned long now = millis();

  if (now - hungerTimer >= 5000) {
    pet.hunger = max(0, pet.hunger - 2);
    hungerTimer = now;
  }
  if (now - happinessTimer >= 7000) {
    if (wifiStats.netCount == 0 && (now - lastWifiScanTime) > 30000) {
      pet.happiness = max(0, pet.happiness - 3);
    } else {
      pet.happiness = max(0, pet.happiness - 1);
    }
    happinessTimer = now;
  }
  if (now - healthTimer >= 10000) {
    if (pet.hunger < 20 || pet.happiness < 20) {
      pet.health = max(0, pet.health - 2);
    } else {
      pet.health = max(0, pet.health - 1);
    }
    healthTimer = now;
  }

  if (now - ageTimer >= 60000) {
    pet.ageMinutes++;
    if (pet.ageMinutes >= 60) {
      pet.ageMinutes -= 60;
      pet.ageHours++;
    }
    if (pet.ageHours >= 24) {
      pet.ageHours -= 24;
      pet.ageDays++;
    }
    ageTimer = now;
  }

  // Hunger effect
  if (hungerEffectActive && now - lastHungerFrameTime >= HUNGER_EFFECT_DELAY) {
    lastHungerFrameTime = now;
    hungerEffectFrame++;
    if (hungerEffectFrame >= HUNGER_FRAME_COUNT) {
      hungerEffectActive = false;
      ringOff();
    }
  }

  // Mood ring flash timeout
  if (moodRingOverride && moodRingFlashEnd > 0 && now > moodRingFlashEnd) {
    clearMoodRing();
  }

  // WiFi-based activity
  if (currentActivity == ACT_HUNT || currentActivity == ACT_DISCOVER) {
    if (checkWifiScanDone()) {
      if (currentActivity == ACT_HUNT)      resolveHunt();
      else if (currentActivity == ACT_DISCOVER) resolveDiscover();
      currentActivity = ACT_NONE;
      ringOff();
    }
  }

  // Rest phases
  stepRest();

  // Mood & evolution
  updateMood();
  updateEvolution();

  // Death
  if (pet.hunger <= 0 && pet.happiness <= 0 && pet.health <= 0 &&
      currentScreen != SCREEN_GAMEOVER) {
    currentScreen  = SCREEN_GAMEOVER;
    uiOnScreenChange(currentScreen);
    currentActivity = ACT_NONE;
    restPhase = REST_NONE;
    ringSad();
  }

  // Autosave
  if (now - lastSaveTime >= autoSaveMs) {
    lastSaveTime = now;
    saveState();
  }

  // Autonomous in HOME
  if (currentScreen == SCREEN_HOME &&
      currentActivity == ACT_NONE &&
      restPhase == REST_NONE) {
    decideNextActivity();
  }
}

// ---------- Touch & button handling ----------

// Map touch (tx,ty) on 466x466 display to virtual button
// Returns: 0=UP, 1=OK, 2=DOWN, 3=R1, 4=R2, 5=R3, -1=none
static int mapTouchToButton(int tx, int ty) {
  // Inside the 240x240 box → OK
  if (tx >= BOX_X && tx <= BOX_X + BOX_W &&
      ty >= BOX_Y && ty <= BOX_Y + BOX_H) {
    return 1; // OK
  }

  int thirdH = DISP_H / 3; // ~155

  // Left of box
  if (tx < BOX_X) {
    if (ty < thirdH)             return 0; // UP
    else if (ty < thirdH * 2)    return 1; // OK
    else                         return 2; // DOWN
  }

  // Right of box
  if (tx > BOX_X + BOX_W) {
    if (ty < thirdH)             return 3; // R1
    else if (ty < thirdH * 2)    return 4; // R2
    else                         return 5; // R3
  }

  // Above or below box (but within x range)
  if (ty < BOX_Y)                return 0; // UP
  if (ty > BOX_Y + BOX_H)        return 2; // DOWN

  return -1;
}

void handleButtons() {
  M5.update();

  bool okPressed    = M5.BtnA.wasPressed();   // KEYA = OK
  bool backPressed  = M5.BtnB.wasPressed();   // KEYB = Back

  // Touch tap detection
  int touchBtn = -1;
  auto t = M5.Touch.getDetail();

  if (t.wasPressed()) {
    touchStartX = t.x;
    touchStartY = t.y;
    touchDownTime = millis();
  }

  if (t.wasReleased()) {
    int dx = t.x - touchStartX;
    int dy = t.y - touchStartY;

    // Only count as tap if minimal movement
    if (abs(dx) < 30 && abs(dy) < 30) {
      touchBtn = mapTouchToButton(touchStartX, touchStartY);
    }
  }

  // Shortcut names for readability
  bool up   = (touchBtn == 0);
  bool ok   = okPressed || (touchBtn == 1);
  bool down = (touchBtn == 2);
  bool r1   = (touchBtn == 3);
  bool r2   = (touchBtn == 4);
  bool r3   = (touchBtn == 5);

  // ===== DIRECT QUICK-ACCESS PAGES (matching original R1/R2/R3) =====
  if (currentScreen == SCREEN_HOME) {
    if (r1) {
      sndClick();
      currentScreen = SCREEN_PET_STATUS;
      uiOnScreenChange(currentScreen);
      return;
    }
    if (r2) {
      sndClick();
      currentScreen = SCREEN_ENVIRONMENT;
      uiOnScreenChange(currentScreen);
      return;
    }
    if (r3) {
      sndClick();
      currentScreen = SCREEN_DIAGNOSTICS;
      uiOnScreenChange(currentScreen);
      return;
    }
  }

  // ===== RETURN TO HOME FROM QUICK-ACCESS PAGES =====
  if (currentScreen == SCREEN_PET_STATUS ||
      currentScreen == SCREEN_ENVIRONMENT ||
      currentScreen == SCREEN_DIAGNOSTICS) {
    if (r1 || r2 || r3) {
      sndClick();
      currentScreen = SCREEN_HOME;
      uiOnScreenChange(currentScreen);
      return;
    }
  }

  // ===== BOOT =====
  if (currentScreen == SCREEN_BOOT) {
    if (up || ok || down || backPressed) {
      sndClick();
      currentScreen = hasHatchedOnce ? SCREEN_HOME : SCREEN_HATCH;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== HATCH =====
  if (currentScreen == SCREEN_HATCH) {
    if (ok && !hasHatchedOnce) {
      sndClick();
      hatchTriggered = true;
    }
    return;
  }

  // ===== HOME =====
  if (currentScreen == SCREEN_HOME) {
    if (ok) {
      sndClick();
      currentScreen = SCREEN_MENU;
      mainMenuIndex = 0;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== MAIN MENU =====
  if (currentScreen == SCREEN_MENU) {
    if (up) {
      sndClick();
      mainMenuIndex = (mainMenuIndex - 1 + 7) % 7;
    }
    if (down) {
      sndClick();
      mainMenuIndex = (mainMenuIndex + 1) % 7;
    }
    if (ok) {
      sndClick();
      switch (mainMenuIndex) {
        case 0: currentScreen = SCREEN_PET_STATUS;   break;
        case 1: currentScreen = SCREEN_ENVIRONMENT;  break;
        case 2: currentScreen = SCREEN_SYSINFO;      break;
        case 3: currentScreen = SCREEN_CONTROLS;     break;
        case 4: currentScreen = SCREEN_SETTINGS;     break;
        case 5: currentScreen = SCREEN_DIAGNOSTICS;  break;
        case 6: currentScreen = SCREEN_HOME;         break;
      }
      uiOnScreenChange(currentScreen);
    }
    if (backPressed) {
      sndClick();
      currentScreen = SCREEN_HOME;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== SIMPLE OK-BACK PAGES =====
  if (currentScreen == SCREEN_PET_STATUS ||
      currentScreen == SCREEN_ENVIRONMENT ||
      currentScreen == SCREEN_SYSINFO ||
      currentScreen == SCREEN_DIAGNOSTICS) {
    if (ok || backPressed) {
      sndClick();
      currentScreen = SCREEN_MENU;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== CONTROLS =====
  if (currentScreen == SCREEN_CONTROLS) {
    if (up) {
      sndClick();
      controlsIndex = (controlsIndex - 1 + 4) % 4;
    }
    if (down) {
      sndClick();
      controlsIndex = (controlsIndex + 1) % 4;
    }
    if (ok) {
      sndClick();
      switch (controlsIndex) {
        case 0:
          tftBrightnessIndex = (tftBrightnessIndex + 1) % 3;
          applyTftBrightness();
          break;
        case 1:
          soundEnabled = !soundEnabled;
          if (!soundEnabled) M5.Speaker.stop();
          break;
        case 2:
          hapticEnabled = !hapticEnabled;
          break;
        case 3:
          currentScreen = SCREEN_MENU;
          uiOnScreenChange(currentScreen);
          break;
      }
    }
    if (backPressed) {
      sndClick();
      currentScreen = SCREEN_MENU;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== SETTINGS =====
  if (currentScreen == SCREEN_SETTINGS) {
    if (up) {
      sndClick();
      settingsMenuIndex = (settingsMenuIndex - 1 + 6) % 6;
    }
    if (down) {
      sndClick();
      settingsMenuIndex = (settingsMenuIndex + 1) % 6;
    }
    if (ok) {
      sndClick();
      switch (settingsMenuIndex) {
        case 0: break;
        case 1: autoSleep = !autoSleep; break;
        case 2:
          if (autoSaveMs == 15000) autoSaveMs = 30000;
          else if (autoSaveMs == 30000) autoSaveMs = 60000;
          else autoSaveMs = 15000;
          break;
        case 3: resetPet(false); break;
        case 4:
          resetPet(true);
          petStage = STAGE_BABY;
          hasHatchedOnce = false;
          saveState();
          currentScreen = SCREEN_HATCH;
          uiOnScreenChange(currentScreen);
          return;
        case 5:
          currentScreen = SCREEN_MENU;
          uiOnScreenChange(currentScreen);
          break;
      }
    }
    if (backPressed) {
      sndClick();
      currentScreen = SCREEN_MENU;
      uiOnScreenChange(currentScreen);
    }
    return;
  }

  // ===== GAME OVER =====
  if (currentScreen == SCREEN_GAMEOVER) {
    if (ok) {
      sndClick();
      resetPet(true);
      petStage = STAGE_BABY;
      hasHatchedOnce = false;
      saveState();
      currentScreen = SCREEN_HATCH;
      uiOnScreenChange(currentScreen);
    }
    return;
  }
}

// ---------- setup & loop ----------
void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  M5.begin(cfg);

  delay(100);

  M5.Display.setBrightness(255);
  M5.Speaker.setVolume(128);

  randomSeed(esp_random());

  fb.setColorDepth(16);
  fb.createSprite(240, 240);
  fb.setSwapBytes(true);

  petSprite.setColorDepth(16);
  petSprite.createSprite(PET_W, PET_H);
  petSprite.setSwapBytes(true);

  effectSprite.setColorDepth(16);
  effectSprite.createSprite(100, 95);
  effectSprite.setSwapBytes(true);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);

  prefs.begin("tamafi2", false);
  loadState();
  applyTftBrightness();

  unsigned long now = millis();
  hungerTimer      = now;
  happinessTimer   = now;
  healthTimer      = now;
  ageTimer         = now;
  lastLogicTick    = now;
  lastSaveTime     = now;
  lastDecisionTime = now;
  lastRestAnimTime = now;
  lastHungerFrameTime = now;
  lastDeadFrameTime   = now;

  currentScreen = SCREEN_BOOT;
  uiInit();
  uiOnScreenChange(currentScreen);
}

void loop() {
  unsigned long now = millis();

  sndUpdate();

  // Vibration timeout
  if (vibratorEndTime > 0 && now > vibratorEndTime) {
    M5.Power.setVibration(0);
    vibratorEndTime = 0;
  }

  if (now - lastLogicTick >= 100) {
    lastLogicTick = now;
    if (currentScreen != SCREEN_BOOT && currentScreen != SCREEN_HATCH) {
      logicTick();
    }
  }

  handleButtons();
  uiDrawScreen(currentScreen, mainMenuIndex, controlsIndex, settingsMenuIndex);
}
