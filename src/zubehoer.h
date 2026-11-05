// Takt – Anzeige auf Grove-Zubehoer (Unit OLED oder Unit Hex) ueber die Bibliothek Beiwerk
//
// Erkannt wird beim Start und mit Fn+E. Immer nur ein Geraet am Grove-Port.
//   Hex,  Ansicht 1 "Lichtorgel": jede Spur blitzt in ihrer Farbe und Zone, Laufposition aussen
//   Hex,  Ansicht 2 "Spur": Steps der aktiven Spur im aeusseren Ring, Mitte blitzt bei Schlaegen
//   OLED, Ansicht 1 "Info": Pattern / Song-Platz, Tempo, Laufposition gross
//   OLED, Ansicht 2 "Pegel": je Spur ein Balken, der bei Schlaegen hochspringt
// Fn+R wechselt die Ansicht, Fn+T: Hex-Helligkeit (3 Stufen) bzw. OLED drehen.
// Wird in main.cpp nach curTrack, P() und FONT eingebunden.
#pragma once

#include <Beiwerk.h>
#include <Preferences.h>

namespace zub {

using namespace beiwerk;

static const uint32_t SPUR_FARBE[NT] = {
  0xFF1800,   // BD rot
  0xFF7000,   // SD orange
  0xFFD000,   // CP gelb
  0x05D9E8,   // CH cyan
  0x2050FF,   // OH blau
  0x20FF40,   // LT gruen
  0xA0FF20,   // HT hellgruen
  0xFF2A6D,   // CB magenta
};
static const uint8_t HELL_STUFEN[3] = {5, 12, 25};   // Prozent; Obergrenze der Bibliothek ist 40

static uint8_t ansichtHex = 0, ansichtOled = 0, stufe = 1;
static bool gedreht = false;
static float env[NT];                                // Abklingen je Spur, 0..1
static M5Canvas* bild = nullptr;                     // 128x64, 8 bit grau; wird nur bei Aenderung gesendet
static uint8_t letztes[128 * 64];

// ---------------------------------------------------------------- Einstellungen
// Eigener kleiner Block "zub" im Namespace "takt" (4 Bytes), sofort gespeichert.
static void speichern() {
  Preferences p;
  if (!p.begin("takt", false)) return;
  uint8_t b[4] = {ansichtHex, ansichtOled, stufe, (uint8_t)gedreht};
  p.putBytes("zub", b, sizeof(b));
  p.end();
}

static void laden() {
  Preferences p;
  if (!p.begin("takt", true)) return;
  uint8_t b[4];
  if (p.getBytes("zub", b, sizeof(b)) == sizeof(b)) {
    ansichtHex = b[0] % 2; ansichtOled = b[1] % 2; stufe = b[2] % 3; gedreht = b[3];
  }
  p.end();
}

// ---------------------------------------------------------------- Erkennung
static const char* erkenne() {
  Geraet g = erkennen();
  memset(letztes, 0xAA, sizeof(letztes));            // naechstes OLED-Bild auf jeden Fall senden
  if (g == Geraet::Oled) {
    if (!bild) {
      bild = new M5Canvas(&oled());
      bild->setColorDepth(8);
      bild->createSprite(128, 64);
    }
    return "OLED erkannt";
  }
  return "Hex aktiv (kein OLED gefunden)";
}

static const char* begin() {
  laden();
  hex::helligkeit(HELL_STUFEN[stufe]);
  oledDrehen(gedreht);
  return erkenne();
}

static const char* kurzname() { return geraet() == Geraet::Oled ? "OLED" : "Hex"; }

// Fn+E / Fn+R / Fn+T. Rueckgabe: Meldung fuer die Fusszeile, nullptr = nicht zustaendig
static const char* fnTaste(char c) {
  static char b[40];
  switch (c) {
    case 'e': return erkenne();
    case 'r':
      if (geraet() == Geraet::Oled) {
        ansichtOled = (ansichtOled + 1) % 2;
        snprintf(b, sizeof(b), "OLED: %s", ansichtOled ? "Pegel" : "Info");
      } else {
        ansichtHex = (ansichtHex + 1) % 2;
        snprintf(b, sizeof(b), "Hex: %s", ansichtHex ? "Spur" : "Lichtorgel");
      }
      speichern();
      return b;
    case 't':
      if (geraet() == Geraet::Oled) {
        gedreht = !gedreht;
        oledDrehen(gedreht);
        memset(letztes, 0xAA, sizeof(letztes));
        snprintf(b, sizeof(b), "OLED %s", gedreht ? "gedreht" : "normal");
      } else {
        stufe = (stufe + 1) % 3;
        hex::helligkeit(HELL_STUFEN[stufe]);
        snprintf(b, sizeof(b), "Hex-Helligkeit %d/3", stufe + 1);
      }
      speichern();
      return b;
  }
  return nullptr;
}

// ---------------------------------------------------------------- Schlaege einsammeln
static void schlaege() {
  uint32_t h = __atomic_exchange_n(&hitMask, 0u, __ATOMIC_RELAXED);
  for (int t = 0; t < NT; t++) {
    env[t] *= (t == CH ? 0.65f : 0.80f);             // pro Bild (~33 ms); Hats klingen schneller ab
    if (h & (1u << t)) env[t] = (h & (1u << (8 + t))) ? 1.0f : 0.7f;
  }
}

static uint32_t mal(uint32_t rgb, float f) { return skaliere(rgb, (uint8_t)(constrain(f, 0.0f, 1.0f) * 255)); }

// Step 0-15 auf die 18 LEDs des aeusseren Rings verteilen (alle Positionen verschieden)
static int aussen(int s) { return hex::ring(3, s * 18 / NS); }

// ---------------------------------------------------------------- Hex
static void hexLichtorgel() {
  hex::clear();
  hex::set(hex::ring(0, 0), mal(SPUR_FARBE[BD], env[BD]));
  hex::ringFill(1, mal(SPUR_FARBE[BD], env[BD] * 0.4f), true);
  hex::ringFill(1, mal(SPUR_FARBE[CB], env[CB]), true);
  hex::ringFill(2, mal(SPUR_FARBE[SD], env[SD]), true);
  hex::ringFill(2, mal(SPUR_FARBE[CP], env[CP]), true);
  for (int k = 0; k < 6; k++) {                      // Toms: rechte Haelfte hoch, linke tief
    hex::add(hex::ring(2, k), mal(SPUR_FARBE[HT], env[HT]));
    hex::add(hex::ring(2, k + 6), mal(SPUR_FARBE[LT], env[LT]));
  }
  hex::ringFill(3, mal(SPUR_FARBE[CH], env[CH] * 0.6f), true);
  hex::ringFill(3, mal(SPUR_FARBE[OH], env[OH]), true);
  if (playing) {
    int g = uiGstep;
    hex::add(aussen(g), g % 4 == 0 ? 0xC0C0C0 : 0x606060);
  } else {
    hex::add(hex::ring(0, 0), 0x022428);             // gestoppt: Mitte glimmt cyan
  }
}

static void hexSpur() {
  hex::clear();
  const Pattern& Pt = song.pat[curPat];
  int t = curTrack;
  uint32_t farbe = song.tp[t].mute ? 0x303030 : SPUR_FARBE[t];
  for (int s = 0; s < Pt.len[t]; s++) {
    const Step& st = Pt.s[t][s];
    if (st.on) hex::set(aussen(s), mal(farbe, st.on == 2 ? 0.9f : 0.3f));
  }
  if (playing) hex::add(aussen(uiStep[t]), 0x909090);
  hex::set(hex::ring(0, 0), mal(farbe, 0.15f + env[t] * 0.85f));
  hex::ringFill(1, mal(farbe, env[t] * 0.5f));
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
    snprintf(b, sizeof(b), "Takt %d/%d  SW%d%%", chainBar + 1, song.chainRep[cp], song.swing);
  } else {
    snprintf(b, sizeof(b), "%s  SW%d%%", TRACK_NAMES[curTrack], song.swing);
  }
  c.drawString(b, 0, 38);

  int g = uiGstep;                                   // 16 Kaestchen, Viertel hoeher
  for (int s = 0; s < NS; s++) {
    int x = s * 8, h = (s % 4 == 0) ? 10 : 6;
    if (playing && s == g) c.fillRect(x, 64 - h, 7, h, TFT_WHITE);
    else c.drawRect(x, 64 - h, 7, h, TFT_WHITE);
  }
}

static void oledPegel(M5Canvas& c) {
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

// ---------------------------------------------------------------- Takt
// Aus loop() aufrufen. Hex ~30 Bilder/s. Das OLED nur bei Aenderung und hoechstens
// ~12 Bilder/s: ein volles Bild ueber I2C (400 kHz) dauert ~25 ms und haelt die UI-Schleife auf.
static void tick() {
  static uint32_t lastHex = 0, lastOled = 0;
  uint32_t now = millis();
  if (now - lastHex < 33) return;
  lastHex = now;
  schlaege();

  if (geraet() == Geraet::Hex) {
    if (ansichtHex == 0) hexLichtorgel();
    else hexSpur();
    hex::show();
  } else if (geraet() == Geraet::Oled && bild && now - lastOled >= 80) {
    M5Canvas& c = *bild;
    c.fillSprite(TFT_BLACK);
    c.setTextColor(TFT_WHITE);
    if (ansichtOled == 0) oledInfo(c);
    else oledPegel(c);
    uint8_t* buf = (uint8_t*)c.getBuffer();
    if (memcmp(buf, letztes, sizeof(letztes)) != 0) {
      memcpy(letztes, buf, sizeof(letztes));
      c.pushSprite(0, 0);
      lastOled = now;
    }
  }
}

}  // namespace zub
