// Takt – Drum Machine fuer den M5Stack Cardputer ADV
#include <M5Cardputer.h>
#include <Preferences.h>
#include "engine.h"
#include "fonts.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static M5Canvas canvas(&M5Cardputer.Display);
static Preferences prefs;

static int curTrack = 0;
static Pattern clip;
static bool hasClip = false;
static char msg[48] = "";
static uint32_t msgUntil = 0;

static const lgfx::U8g2font FONT(spleen6x12);

// Stil "Graphit" mit Neon-Noir-Akzenten (Cyan/Magenta)
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
static constexpr uint16_t C_CYAN    = rgb(0x05, 0xd9, 0xe8);
static constexpr uint16_t C_MAGENTA = rgb(0xff, 0x2a, 0x6d);
static constexpr uint16_t C_BRIGHT  = rgb(0xed, 0xed, 0xed);   // Accent-Steps, aktive Spur
static constexpr uint16_t C_STEP    = rgb(0x8c, 0x8c, 0x8c);   // normale Steps
static constexpr uint16_t C_TEXT    = rgb(0xb8, 0xb8, 0xb8);
static constexpr uint16_t C_MUTED   = rgb(0x3a, 0x3a, 0x3a);   // gemutete Spur
static constexpr uint16_t C_CELL    = rgb(0x17, 0x17, 0x17);
static constexpr uint16_t C_CELL_Q  = rgb(0x26, 0x26, 0x26);   // Viertel-Raster
static constexpr uint16_t C_OUTSIDE = rgb(0x1c, 0x1c, 0x1c);   // jenseits der Spurlaenge
static constexpr uint16_t C_SEL     = rgb(0x1e, 0x1e, 0x1e);   // Hintergrund aktive Spur
static const char* STEP_KEYS = "12345678qwertyui";
static const char* TRACK_KEYS = "asdfghjk";

// ---------------------------------------------------------------- Audio-Task
static constexpr int ABUF = 256;   // ≈ 5,8 ms pro Puffer
static int16_t abuf[3][ABUF];

static void audioTask(void*) {
  int bi = 0;
  for (;;) {
    if (M5.Speaker.isPlaying(0) < 2) {       // Warteschlange hat Platz
      render(abuf[bi], ABUF);
      M5.Speaker.playRaw(abuf[bi], ABUF, SR, false, 1, 0, false);
      bi = (bi + 1) % 3;
    } else {
      vTaskDelay(1);
    }
  }
}

// ---------------------------------------------------------------- Speichern
static void showMsg(const char* m) { strncpy(msg, m, sizeof(msg) - 1); msgUntil = millis() + 1400; }

static void saveSong() {
  prefs.begin("takt", false);
  size_t n = prefs.putBytes("song", &song, sizeof(Song));
  prefs.end();
  showMsg(n == sizeof(Song) ? "Gespeichert" : "Speichern fehlgeschlagen");
}

static void loadSong() {
  prefs.begin("takt", true);
  bool ok = prefs.getBytesLength("song") == sizeof(Song) && prefs.getBytes("song", &song, sizeof(Song)) == sizeof(Song);
  prefs.end();
  if (!ok || song.magic != MAGIC) defaultSong();
}

// ---------------------------------------------------------------- Pattern-Werkzeuge
static Pattern& P() { return song.pat[curPat]; }

static int hitCount(int t) {
  int n = 0;
  for (int s = 0; s < P().len[t]; s++) if (P().s[t][s].on) n++;
  return n;
}

static void clearTrack(int t) { for (int s = 0; s < NS; s++) P().s[t][s] = {0, 0, 1}; }

static void euclid(int t, int k) {            // k Schläge gleichmäßig auf die Spurlänge verteilen
  int n = P().len[t];
  clearTrack(t);
  for (int s = 0; s < n; s++) if (((s * k) % n) < k) P().s[t][s].on = 1;
}

static void randomTrack(int t) {
  static const uint8_t DENS[NT] = {30, 20, 12, 55, 12, 12, 12, 15};
  clearTrack(t);
  for (int s = 0; s < P().len[t]; s++) {
    if ((int)random(100) < DENS[t]) {
      P().s[t][s].on = random(5) == 0 ? 2 : 1;
      if (random(6) == 0) P().s[t][s].prob = 1 + random(3);
      if ((t == CH || t == SD) && random(8) == 0) P().s[t][s].rat = 2 + random(2);
    }
  }
}

static void selectPattern(int p) {
  p = (p + NP) % NP;
  pendingPat = p;              // Audio-Task übernimmt am Taktanfang (oder sofort, wenn gestoppt)
  char b[24]; snprintf(b, sizeof(b), "Pattern %d", p + 1); showMsg(b);
}

// ---------------------------------------------------------------- Tastatur
struct Mods { bool fn, shift, ctrl, alt; };

static char norm(char c) {
  static const char* from = "!@#$%^&*()_+{}:\"<>?~|";
  static const char* to   = "1234567890-=[];',./`\\";
  if (c >= 'A' && c <= 'Z') return c + 32;
  const char* p = c ? strchr(from, c) : nullptr;
  return p ? to[p - from] : c;
}

template <typename T> static T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void onKey(char c, const Mods& m) {
  TrackParams& tp = song.tp[curTrack];
  const char* sk = strchr(STEP_KEYS, c);
  const char* tk = strchr(TRACK_KEYS, c);

  if (c && sk) {                                   // Steps
    int i = sk - STEP_KEYS;
    if (m.fn) { if (i < NP) selectPattern(i); return; }
    Step& st = P().s[curTrack][i];
    if (m.shift)      st.on = (st.on == 2) ? 1 : 2;
    else if (m.alt)   { if (st.on) st.prob = (st.prob + 1) % 4; }
    else if (m.ctrl)  { if (st.on) st.rat = st.rat % 4 + 1; }
    else              { st.on = st.on ? 0 : 1; if (!st.on) { st.prob = 0; st.rat = 1; } }
    return;
  }
  if (c && tk) {                                   // Spuren
    int t = tk - TRACK_KEYS;
    if (m.fn) { song.tp[t].mute ^= 1; return; }
    if (m.shift) {                                 // Solo
      bool solo = true;
      for (int i = 0; i < NT; i++) if ((i == t) == (bool)song.tp[i].mute) solo = false;
      for (int i = 0; i < NT; i++) song.tp[i].mute = solo ? 0 : (i != t);
      showMsg(solo ? "Solo aus" : "Solo");
      return;
    }
    curTrack = t;
    padMask |= 1u << t;
    return;
  }

  switch (c) {
    case ' ': cmd = playing ? 2 : 1; break;
    case 'm': padMask |= 1u << curTrack; break;
    case '9': selectPattern(curPat - 1); break;
    case '0': selectPattern(curPat + 1); break;
    case 'z': tp.tune  = clampv<int>(tp.tune - 1, -12, 12); break;
    case 'x': tp.tune  = clampv<int>(tp.tune + 1, -12, 12); break;
    case 'c': tp.decay = clampv<int>(tp.decay - 1, 1, 30); break;
    case 'v': tp.decay = clampv<int>(tp.decay + 1, 1, 30); break;
    case 'b': tp.level = clampv<int>(tp.level - 1, 0, 10); break;
    case 'n': tp.level = clampv<int>(tp.level + 1, 0, 10); break;
    case ',': song.bpm = clampv<int>(song.bpm - (m.shift ? 10 : 1), 40, 250); break;
    case '.': song.bpm = clampv<int>(song.bpm + (m.shift ? 10 : 1), 40, 250); break;
    case '[': song.swing = clampv<int>(song.swing - 1, 50, 75); break;
    case ']': song.swing = clampv<int>(song.swing + 1, 50, 75); break;
    case ';': P().len[curTrack] = clampv<int>(P().len[curTrack] - 1, 1, NS); break;
    case '\'': P().len[curTrack] = clampv<int>(P().len[curTrack] + 1, 1, NS); break;
    case '-': song.vol = clampv<int>(song.vol - 16, 0, 255); M5.Speaker.setVolume(song.vol); break;
    case '=': song.vol = clampv<int>(song.vol + 16, 0, 255); M5.Speaker.setVolume(song.vol); break;
    case 'l': {
      int k = clampv<int>(hitCount(curTrack) + (m.shift ? -1 : 1), 0, P().len[curTrack]);
      euclid(curTrack, k);
      char b[24]; snprintf(b, sizeof(b), "Euklid %d/%d", k, P().len[curTrack]); showMsg(b);
      break;
    }
    case 9:  randomTrack(curTrack); showMsg("Zufall"); break;           // Tab
    case 'o': clip = P(); hasClip = true; showMsg("Pattern kopiert"); break;
    case 'p': if (hasClip) { P() = clip; showMsg("Eingefuegt"); } break;
    case 8:                                                              // Del
      if (m.fn) { for (int t = 0; t < NT; t++) clearTrack(t); showMsg("Pattern geleert"); }
      else clearTrack(curTrack);
      break;
    case 13: saveSong(); break;                                          // Enter
  }
}

static uint8_t prevKeys[16];

static void handleKeys() {
  M5Cardputer.update();
  if (!M5Cardputer.Keyboard.isChange()) return;
  auto& st = M5Cardputer.Keyboard.keysState();
  uint8_t now[16] = {0};
  auto set = [&](uint8_t c) { now[(c >> 3) & 15] |= 1 << (c & 7); };
  for (char c : st.word) set((uint8_t)norm(c));
  if (st.space) set(' ');
  if (st.del) set(8);
  if (st.enter) set(13);
  if (st.tab) set(9);
  Mods m{st.fn, st.shift, st.ctrl, (bool)(st.alt || st.opt)};
  for (int c = 1; c < 128; c++) {
    bool isNow = now[c >> 3] & (1 << (c & 7));
    bool wasPrev = prevKeys[c >> 3] & (1 << (c & 7));
    if (isNow && !wasPrev) onKey((char)c, m);   // nur neu gedrückte Tasten auslösen
  }
  memcpy(prevKeys, now, sizeof(now));
}

// ---------------------------------------------------------------- Anzeige
static void draw() {
  canvas.fillSprite(TFT_BLACK);
  char b[48];

  // Kopfzeile: Play-Symbol, Pattern (Magenta), Tempo, Swing, Lautstaerke; cyane Linie darunter
  if (playing) canvas.fillTriangle(2, 2, 2, 10, 8, 6, C_CYAN);
  else canvas.fillRect(2, 3, 6, 6, C_STEP);
  int pp = pendingPat;
  if (pp >= 0) snprintf(b, sizeof(b), "P%d>%d", curPat + 1, pp + 1);
  else         snprintf(b, sizeof(b), "P%d", curPat + 1);
  canvas.setTextColor(C_MAGENTA);
  canvas.drawString(b, 12, 0);
  int x = 12 + canvas.textWidth(b) + 8;
  snprintf(b, sizeof(b), "%d BPM  SW%d%%", song.bpm, song.swing);
  canvas.setTextColor(C_TEXT);
  canvas.drawString(b, x, 0);
  snprintf(b, sizeof(b), "VOL%d", song.vol * 100 / 255);
  canvas.drawRightString(b, 238, 0);
  canvas.drawFastHLine(0, 12, 240, C_CYAN);

  // Raster: 8 Spuren a 13 px
  Pattern& Pt = P();
  for (int t = 0; t < NT; t++) {
    int y = 13 + t * 13;
    bool muted = song.tp[t].mute;
    if (t == curTrack) {                                   // aktive Spur: Magenta-Kante links
      canvas.fillRect(0, y, 20, 12, C_SEL);
      canvas.fillRect(0, y, 2, 12, C_MAGENTA);
    }
    canvas.setTextColor(muted ? C_MUTED : (t == curTrack ? C_BRIGHT : C_TEXT));
    canvas.drawString(TRACK_NAMES[t], 4, y);
    int len = Pt.len[t];
    for (int s = 0; s < NS; s++) {
      int sx = 22 + s * 13;
      if (s >= len) { canvas.drawRect(sx, y, 12, 12, C_OUTSIDE); continue; }
      canvas.fillRect(sx, y, 12, 12, (s % 4 == 0) ? C_CELL_Q : C_CELL);
      const Step& st = Pt.s[t][s];
      if (st.on) {
        uint16_t col = muted ? C_MUTED : (st.on == 2 ? C_BRIGHT : C_STEP);
        int in = st.prob & 3;                                // kleineres Kaestchen = geringere Wahrscheinlichkeit
        canvas.fillRect(sx + in, y + in, 12 - 2 * in, 12 - 2 * in, col);
        for (int r = 1; r < st.rat; r++) canvas.fillRect(sx + 2 + (r - 1) * 3, y + 9, 2, 2, TFT_BLACK);
      }
      if (playing && uiStep[t] == s) canvas.drawRect(sx, y, 12, 12, C_CYAN);
    }
  }

  // Fusszeile: cyane Linie, Spurname Magenta + Parameter, oder kurze Meldung
  canvas.drawFastHLine(0, 119, 240, C_CYAN);
  if (millis() < msgUntil) {
    canvas.setTextColor(C_BRIGHT);
    canvas.drawString(msg, 3, 122);
  } else {
    const TrackParams& tp = song.tp[curTrack];
    canvas.setTextColor(C_MAGENTA);
    canvas.drawString(TRACK_NAMES[curTrack], 3, 122);
    snprintf(b, sizeof(b), "Tune%+d  Dec%d.%d  Lvl%d  Len%d", tp.tune, tp.decay / 10, tp.decay % 10, tp.level, Pt.len[curTrack]);
    canvas.setTextColor(C_TEXT);
    canvas.drawString(b, 21, 122);
  }
  canvas.pushSprite(0, 0);
}

// ---------------------------------------------------------------- Setup / Loop
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  canvas.setColorDepth(16);
  canvas.createSprite(240, 135);
  canvas.setFont(&FONT);
  canvas.setTextSize(1);

  engineInit();
  loadSong();

  M5.Speaker.begin();
  M5.Speaker.setVolume(song.vol);
  M5.Speaker.setChannelVolume(0, 255);

  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0);
  showMsg("Takt " FW_VERSION " - Space = Play");
}

void loop() {
  handleKeys();
  static uint32_t last = 0;
  if (millis() - last >= 33) { last = millis(); draw(); }
  delay(1);
}
