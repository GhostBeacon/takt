// Takt – display on Grove accessories (Unit OLED or Unit Hex) through the GroveUnits library
//
// Detection runs at start-up and with Fn+E. Only one device on the Grove port at a time.
//   Hex,  view 1 "Lights": every track flashes in its colour and zone, play position on the rim
//   Hex,  view 2 "Track": steps of the active track on the outer ring, centre flashes on hits
//   OLED, view 1 "Info": pattern / song slot, tempo, play position in large type
//   OLED, view 2 "Levels": one bar per track that jumps up on hits
// Fn+R switches the view, Fn+T: Hex brightness (3 levels) or flip the OLED.
// Included in main.cpp after curTrack, P() and FONT.
#pragma once

#include <GroveUnits.h>
#include <Preferences.h>

namespace acc {

using namespace grove;

static const uint32_t TRACK_COLOR[NT] = {
  0xFF1800,   // BD red
  0xFF7000,   // SD orange
  0xFFD000,   // CP yellow
  0x05D9E8,   // CH cyan
  0x2050FF,   // OH blue
  0x20FF40,   // LT green
  0xA0FF20,   // HT light green
  0xFF2A6D,   // CB magenta
};
static const uint8_t BRIGHT_LEVELS[3] = {5, 12, 25};  // percent; the library caps it at 40

static uint8_t viewHex = 0, viewOled = 0, level = 1;
static bool flipped = false;
static float env[NT];                                // decay per track, 0..1
static M5Canvas* frame = nullptr;                    // 128x64, 8 bit grey; only sent when it changes
static uint8_t lastFrame[128 * 64];

// ---------------------------------------------------------------- Settings
// Own small blob "acc" in the namespace "takt" (4 bytes), saved at once.
static void save() {
  Preferences p;
  if (!p.begin("takt", false)) return;
  uint8_t b[4] = {viewHex, viewOled, level, (uint8_t)flipped};
  p.putBytes("acc", b, sizeof(b));
  p.end();
}

static void load() {
  Preferences p;
  if (!p.begin("takt", true)) return;
  uint8_t b[4];
  if (p.getBytes("acc", b, sizeof(b)) == sizeof(b)) {
    viewHex = b[0] % 2; viewOled = b[1] % 2; level = b[2] % 3; flipped = b[3];
  }
  p.end();
}

// ---------------------------------------------------------------- Detection
static const char* detectDevice() {
  Device d = detect();
  memset(lastFrame, 0xAA, sizeof(lastFrame));        // make sure the next OLED frame is sent
  if (d == Device::Oled) {
    if (!frame) {
      frame = new M5Canvas(&oled());
      frame->setColorDepth(8);
      frame->createSprite(128, 64);
    }
    return "OLED detected";
  }
  return "Hex active (no OLED found)";
}

static const char* begin() {
  load();
  hex::brightness(BRIGHT_LEVELS[level]);
  oledFlip(flipped);
  return detectDevice();
}

static const char* shortName() { return device() == Device::Oled ? "OLED" : "Hex"; }

// Fn+E / Fn+R / Fn+T. Returns the message for the footer, nullptr = not handled here
static const char* fnKey(char c) {
  static char b[40];
  switch (c) {
    case 'e': return detectDevice();
    case 'r':
      if (device() == Device::Oled) {
        viewOled = (viewOled + 1) % 2;
        snprintf(b, sizeof(b), "OLED: %s", viewOled ? "Levels" : "Info");
      } else {
        viewHex = (viewHex + 1) % 2;
        snprintf(b, sizeof(b), "Hex: %s", viewHex ? "Track" : "Lights");
      }
      save();
      return b;
    case 't':
      if (device() == Device::Oled) {
        flipped = !flipped;
        oledFlip(flipped);
        memset(lastFrame, 0xAA, sizeof(lastFrame));
        snprintf(b, sizeof(b), "OLED %s", flipped ? "flipped" : "normal");
      } else {
        level = (level + 1) % 3;
        hex::brightness(BRIGHT_LEVELS[level]);
        snprintf(b, sizeof(b), "Hex brightness %d/3", level + 1);
      }
      save();
      return b;
  }
  return nullptr;
}

// ---------------------------------------------------------------- Collecting hits
static void collectHits() {
  uint32_t h = __atomic_exchange_n(&hitMask, 0u, __ATOMIC_RELAXED);
  for (int t = 0; t < NT; t++) {
    env[t] *= (t == CH ? 0.65f : 0.80f);             // per frame (~33 ms); hats decay faster
    if (h & (1u << t)) env[t] = (h & (1u << (8 + t))) ? 1.0f : 0.7f;
  }
}

static uint32_t dim(uint32_t rgb, float f) { return scale(rgb, (uint8_t)(constrain(f, 0.0f, 1.0f) * 255)); }

// Map step 0-15 onto the 18 LEDs of the outer ring (all positions distinct)
static int outer(int s) { return hex::ring(3, s * 18 / NS); }

// ---------------------------------------------------------------- Hex
static void hexLights() {
  hex::clear();
  hex::set(hex::ring(0, 0), dim(TRACK_COLOR[BD], env[BD]));
  hex::ringFill(1, dim(TRACK_COLOR[BD], env[BD] * 0.4f), true);
  hex::ringFill(1, dim(TRACK_COLOR[CB], env[CB]), true);
  hex::ringFill(2, dim(TRACK_COLOR[SD], env[SD]), true);
  hex::ringFill(2, dim(TRACK_COLOR[CP], env[CP]), true);
  for (int k = 0; k < 6; k++) {                      // toms: high on the right half, low on the left
    hex::add(hex::ring(2, k), dim(TRACK_COLOR[HT], env[HT]));
    hex::add(hex::ring(2, k + 6), dim(TRACK_COLOR[LT], env[LT]));
  }
  hex::ringFill(3, dim(TRACK_COLOR[CH], env[CH] * 0.6f), true);
  hex::ringFill(3, dim(TRACK_COLOR[OH], env[OH]), true);
  if (playing) {
    int g = uiGstep;
    hex::add(outer(g), g % 4 == 0 ? 0xC0C0C0 : 0x606060);
  } else {
    hex::add(hex::ring(0, 0), 0x022428);             // stopped: the centre glows cyan
  }
}

static void hexTrack() {
  hex::clear();
  const Pattern& Pt = song.pat[curPat];
  int t = curTrack;
  uint32_t color = song.tp[t].mute ? 0x303030 : TRACK_COLOR[t];
  for (int s = 0; s < Pt.len[t]; s++) {
    const Step& st = Pt.s[t][s];
    if (st.on) hex::set(outer(s), dim(color, st.on == 2 ? 0.9f : 0.3f));
  }
  if (playing) hex::add(outer(uiStep[t]), 0x909090);
  hex::set(hex::ring(0, 0), dim(color, 0.15f + env[t] * 0.85f));
  hex::ringFill(1, dim(color, env[t] * 0.5f));
}

// ---------------------------------------------------------------- OLED
static void oledInfo(M5Canvas& c) {
  char b[24];
  c.setFont(&fonts::Font0);
  c.setTextSize(2);
  if (songMode && song.chainLen > 0) {
    int cp = chainPos < song.chainLen ? chainPos : 0;
    snprintf(b, sizeof(b), "S%d P%d", cp + 1, curPat + 1);
  } else if (pendingPat >= 0) snprintf(b, sizeof(b), "P%d>%d", curPat + 1, pendingPat + 1);
  else snprintf(b, sizeof(b), "P%d", curPat + 1);
  c.drawString(b, 0, 0);
  if (playing) c.fillTriangle(118, 1, 118, 13, 127, 7, TFT_WHITE);
  else c.fillRect(117, 2, 10, 10, TFT_WHITE);

  snprintf(b, sizeof(b), "%d BPM", song.bpm);
  c.drawString(b, 0, 20);

  c.setFont(&FONT);
  c.setTextSize(1);
  if (songMode && song.chainLen > 0) {
    int cp = chainPos < song.chainLen ? chainPos : 0;
    snprintf(b, sizeof(b), "Bar %d/%d  SW%d%%", chainBar + 1, song.chainRep[cp], song.swing);
  } else {
    snprintf(b, sizeof(b), "%s  SW%d%%", TRACK_NAMES[curTrack], song.swing);
  }
  c.drawString(b, 0, 38);

  int g = uiGstep;                                   // 16 boxes, quarter notes taller
  for (int s = 0; s < NS; s++) {
    int x = s * 8, h = (s % 4 == 0) ? 10 : 6;
    if (playing && s == g) c.fillRect(x, 64 - h, 7, h, TFT_WHITE);
    else c.drawRect(x, 64 - h, 7, h, TFT_WHITE);
  }
}

static void oledLevels(M5Canvas& c) {
  c.setFont(&FONT);
  c.setTextSize(1);
  for (int t = 0; t < NT; t++) {
    int x = t * 16;
    int h = (int)(env[t] * 48);
    if (h > 0) c.fillRect(x + 2, 50 - h, 12, h, TFT_WHITE);
    if (t == curTrack) {
      c.fillRect(x, 52, 16, 12, TFT_WHITE);
      c.setTextColor(TFT_BLACK);
    } else {
      c.setTextColor(song.tp[t].mute ? (uint16_t)0x4208 : (uint16_t)TFT_WHITE);
    }
    c.drawCenterString(TRACK_NAMES[t], x + 8, 52);
  }
  c.setTextColor(TFT_WHITE);
}

// ---------------------------------------------------------------- Tick
// Call from loop(). Hex ~30 frames/s. The OLED only when the frame changes and at most
// ~12 frames/s: a full frame over I2C (400 kHz) takes ~25 ms and holds up the UI loop.
static void tick() {
  static uint32_t lastHex = 0, lastOled = 0;
  uint32_t now = millis();
  if (now - lastHex < 33) return;
  lastHex = now;
  collectHits();

  if (device() == Device::Hex) {
    if (viewHex == 0) hexLights();
    else hexTrack();
    hex::show();
  } else if (device() == Device::Oled && frame && now - lastOled >= 80) {
    M5Canvas& c = *frame;
    c.fillSprite(TFT_BLACK);
    c.setTextColor(TFT_WHITE);
    if (viewOled == 0) oledInfo(c);
    else oledLevels(c);
    uint8_t* buf = (uint8_t*)c.getBuffer();
    if (memcmp(buf, lastFrame, sizeof(lastFrame)) != 0) {
      memcpy(lastFrame, buf, sizeof(lastFrame));
      c.pushSprite(0, 0);
      lastOled = now;
    }
  }
}

}  // namespace acc
