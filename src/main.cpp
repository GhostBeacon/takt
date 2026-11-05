// Takt – Drum Machine fuer den M5Stack Cardputer ADV
#include <M5Cardputer.h>
#include <Preferences.h>
#include <nvs.h>
#include "engine.h"
#include "fonts.h"
#include "help.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static M5Canvas canvas(&M5Cardputer.Display);
static Preferences prefs;

static int curTrack = 0;
static Pattern clip;
static bool hasClip = false;
static char msg[48] = "";

enum class Mode { Boot, Grid, Help, Song };
static Mode helpReturn = Mode::Grid;     // wohin die Hilfe zurueckkehrt
static int songCursor = 0;              // ausgewaehlter Platz in der Song-Ansicht
static Mode mode = Mode::Boot;
static uint32_t bootStart = 0;
static constexpr uint32_t BOOT_MS = 2600;
static int helpTop = 0;
static constexpr int HELP_ROWS = 8;
static uint32_t msgUntil = 0;

static const lgfx::U8g2font FONT(spleen6x12);

#include "zubehoer.h"   // OLED oder Hex am Grove-Port (braucht FONT, curTrack)

// Stil "Graphit" mit Neon-Noir-Akzenten (Cyan/Magenta wie Kraehenauge)
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
static void showMsg(const char* m, uint32_t ms = 1400) { strncpy(msg, m, sizeof(msg) - 1); msgUntil = millis() + ms; }

// Der NVS-Bereich (20 KB) wird mit dem Launcher und anderen Apps geteilt. Deshalb gepackt
// (1 Byte pro Step) und in kleinen Bloecken: "cfg" + je Pattern "p0".."p7" (~1,2 KB gesamt).
struct SavedCfg { uint32_t magic; TrackParams tp[NT]; uint16_t bpm; uint8_t swing, vol; };
struct SavedPat { uint8_t s[NT][NS]; uint8_t len[NT]; };           // Bit 0-1 on, 2-3 prob, 4-5 rat-1
struct SavedChain { uint8_t len, mode; uint8_t e[CHAIN_MAX]; };    // e: Bit 0-2 Pattern, 3-5 Wiederholungen-1
static constexpr uint32_t SAVE_MAGIC = 0x54414B01;                // "TAK" + Format 1

static void packPattern(const Pattern& p, SavedPat& o) {
  for (int t = 0; t < NT; t++) {
    o.len[t] = p.len[t];
    for (int s = 0; s < NS; s++) {
      const Step& st = p.s[t][s];
      o.s[t][s] = (st.on & 3) | ((st.prob & 3) << 2) | (((st.rat - 1) & 3) << 4);
    }
  }
}

static void unpackPattern(const SavedPat& o, Pattern& p) {
  for (int t = 0; t < NT; t++) {
    p.len[t] = (o.len[t] >= 1 && o.len[t] <= NS) ? o.len[t] : NS;
    for (int s = 0; s < NS; s++) {
      uint8_t v = o.s[t][s];
      p.s[t][s] = {(uint8_t)((v & 3) > 2 ? 0 : (v & 3)), (uint8_t)((v >> 2) & 3), (uint8_t)(((v >> 4) & 3) + 1)};
    }
  }
}

static void saveSong() {
  char b[48];
  if (!prefs.begin("takt", false)) {
    nvs_stats_t st{};
    nvs_get_stats(nullptr, &st);
    snprintf(b, sizeof(b), "Fehler: Speicher voll (frei %u B)", (unsigned)(st.free_entries * 32));
    showMsg(b, 5000);
    return;
  }
  prefs.remove("song");                                           // alter 3,2-KB-Block aus v1.0.1, falls vorhanden
  SavedCfg c{SAVE_MAGIC, {}, song.bpm, song.swing, song.vol};
  memcpy(c.tp, song.tp, sizeof(c.tp));
  bool ok = true;
  for (int i = 0; ok && i < NP; i++) {
    SavedPat sp;
    packPattern(song.pat[i], sp);
    char key[4] = {'p', (char)('0' + i), 0};
    ok = prefs.putBytes(key, &sp, sizeof(sp)) == sizeof(sp);
  }
  if (ok) {
    SavedChain ch{song.chainLen, (uint8_t)songMode, {}};
    for (int i = 0; i < CHAIN_MAX; i++) ch.e[i] = (song.chainPat[i] & 7) | (((song.chainRep[i] - 1) & 7) << 3);
    ok = prefs.putBytes("chn", &ch, sizeof(ch)) == sizeof(ch);
  }
  if (ok) ok = prefs.putBytes("cfg", &c, sizeof(c)) == sizeof(c);   // zuletzt: cfg markiert einen vollstaendigen Song
  prefs.end();
  if (ok) { showMsg("Gespeichert"); return; }
  nvs_stats_t st{};
  nvs_get_stats(nullptr, &st);
  snprintf(b, sizeof(b), "Fehler beim Speichern (frei %u B)", (unsigned)(st.free_entries * 32));
  showMsg(b, 5000);
}

static void loadSong() {
  defaultSong();
  if (!prefs.begin("takt", true)) return;
  SavedCfg c;
  if (prefs.getBytes("cfg", &c, sizeof(c)) == sizeof(c) && c.magic == SAVE_MAGIC) {
    memcpy(song.tp, c.tp, sizeof(c.tp));
    song.bpm = constrain(c.bpm, 40, 250);
    song.swing = constrain(c.swing, 50, 75);
    song.vol = c.vol;
    for (int i = 0; i < NP; i++) {
      SavedPat sp;
      char key[4] = {'p', (char)('0' + i), 0};
      if (prefs.getBytes(key, &sp, sizeof(sp)) == sizeof(sp)) unpackPattern(sp, song.pat[i]);
    }
    SavedChain ch;
    if (prefs.getBytes("chn", &ch, sizeof(ch)) == sizeof(ch)) {
      song.chainLen = min<int>(ch.len, CHAIN_MAX);
      for (int i = 0; i < CHAIN_MAX; i++) { song.chainPat[i] = ch.e[i] & 7; song.chainRep[i] = ((ch.e[i] >> 3) & 7) + 1; }
      songMode = ch.mode && song.chainLen > 0;
    }
  }
  prefs.end();
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
  if (songMode) { songMode = false; showMsg("Song-Modus aus"); }
  pendingPat = p;              // Audio-Task übernimmt am Taktanfang (oder sofort, wenn gestoppt)
  char b[24]; snprintf(b, sizeof(b), "Pattern %d", p + 1);
  if (millis() >= msgUntil) showMsg(b);
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
    if (m.fn) {
      if (i < NP) selectPattern(i);
      else if (const char* z = zub::fnTaste(c)) showMsg(z, 2000);   // Fn+E/R/T: Grove-Zubehoer
      return;
    }
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
    case ' ':
      if (!playing && songMode) songJump = 0;          // Song startet am Anfang der Kette
      cmd = playing ? 2 : 1;
      break;
    case '\\': mode = Mode::Song; break;              // Song-Ansicht
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

static void leaveBoot() {
  mode = Mode::Grid;
  char b[48];
  snprintf(b, sizeof(b), "Takt " FW_VERSION "  ·  %s  ·  ? = Hilfe", zub::kurzname());
  showMsg(b, 3000);
}

static void helpKey(char c) {
  int maxTop = HELP_LINES > HELP_ROWS ? HELP_LINES - HELP_ROWS : 0;
  switch (c) {
    case ';': helpTop--; break;                        // Pfeil hoch
    case '.': helpTop++; break;                        // Pfeil runter
    case ',': helpTop -= HELP_ROWS; break;             // Seite zurueck
    case '/': helpTop += HELP_ROWS; break;             // Seite vor
    case ' ': cmd = playing ? 2 : 1; break;            // Play/Stop geht auch hier
    case '`': case 8: case 13: mode = helpReturn; break;
  }
  helpTop = constrain(helpTop, 0, maxTop);
}

// Song-Ansicht: Kette bearbeiten. Pfeiltasten ; . , / bewegen den Cursor.
static void songKey(char c, const Mods& m) {
  int n = song.chainLen;
  int& k = songCursor;
  if (c >= '1' && c <= '8') {
    int pat = c - '1';
    if (m.shift && n < CHAIN_MAX && k < n) {             // Shift + 1-8: vor dem Cursor einfuegen
      for (int i = n; i > k; i--) { song.chainPat[i] = song.chainPat[i - 1]; song.chainRep[i] = song.chainRep[i - 1]; }
      song.chainPat[k] = pat; song.chainRep[k] = 1;
      song.chainLen = n + 1;
    } else if (k < n) {                                  // Platz belegt: Pattern ersetzen
      song.chainPat[k] = pat;
    } else if (n < CHAIN_MAX) {                          // "+"-Platz: anhaengen, Cursor weiter
      song.chainPat[n] = pat; song.chainRep[n] = 1;
      song.chainLen = n + 1;
      if (k < CHAIN_MAX - 1) k++;
    }
    return;
  }
  switch (c) {
    case ',': k--; break;                                // Pfeil links
    case '/':
      if (m.shift) { helpReturn = Mode::Song; mode = Mode::Help; return; }   // ?
      k++; break;                                        // Pfeil rechts
    case ';': k -= 8; break;                             // Pfeil hoch
    case '.': k += 8; break;                             // Pfeil runter
    case '-': if (k < n && song.chainRep[k] > 1) song.chainRep[k]--; break;
    case '=': if (k < n && song.chainRep[k] < 8) song.chainRep[k]++; break;
    case 8:                                              // Del: Platz loeschen
      if (k < n) {
        for (int i = k; i < n - 1; i++) { song.chainPat[i] = song.chainPat[i + 1]; song.chainRep[i] = song.chainRep[i + 1]; }
        song.chainLen = n - 1;
        if (song.chainLen == 0) songMode = false;
      }
      break;
    case 9:                                              // Tab: Song-Modus an/aus
      if (!songMode && n == 0) { showMsg("Kette ist leer"); break; }
      songMode = !songMode;
      if (songMode) songJump = k < n ? k : 0;            // ab Cursor, an der naechsten Eins
      showMsg(songMode ? "Song-Modus an" : "Song-Modus aus");
      break;
    case ' ':                                            // Play ab Cursor
      if (!playing && songMode) songJump = k < n ? k : 0;
      cmd = playing ? 2 : 1;
      break;
    case 13: saveSong(); break;
    case '`': case '\\': mode = Mode::Grid; break;
  }
  int maxK = min<int>(song.chainLen, CHAIN_MAX - 1);   // Cursor bis einschliesslich "+"-Platz
  k = constrain(k, 0, maxK);
}

static void dispatchKey(char c, const Mods& m) {
  switch (mode) {
    case Mode::Boot: leaveBoot(); return;              // jede Taste ueberspringt den Startbildschirm
    case Mode::Help: helpKey(c); return;
    case Mode::Song: songKey(c, m); return;
    case Mode::Grid:
      if (c == '/' || c == '`') { helpReturn = Mode::Grid; mode = Mode::Help; return; }   // ? (Shift+/) oder Esc
      onKey(c, m);
      return;
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
    if (isNow && !wasPrev) dispatchKey((char)c, m);   // nur neu gedrückte Tasten auslösen
  }
  memcpy(prevKeys, now, sizeof(now));
}

// ---------------------------------------------------------------- Anzeige
// Startbildschirm: Name, eine Step-Reihe laeuft einmal durch, Version
static void drawBoot() {
  uint32_t t = millis() - bootStart;
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextSize(4);                               // Pixelschrift vergroessert, keine Extra-Schrift im Flash
  canvas.setTextColor(C_MAGENTA);
  canvas.drawCenterString("TAKT", 122, 20);
  canvas.setTextColor(C_BRIGHT);
  canvas.drawCenterString("TAKT", 120, 18);
  canvas.setTextSize(1);

  int cur = min<int>(t / 120, NS);                     // 16 Steps in ~1,9 s
  for (int s = 0; s < NS; s++) {
    int x = 16 + s * 13;
    canvas.fillRect(x, 80, 12, 8, (s % 4 == 0) ? C_CELL_Q : C_CELL);
    if (s < cur) canvas.fillRect(x, 80, 12, 8, (s % 4 == 0) ? C_BRIGHT : C_STEP);
    if (s == cur) canvas.drawRect(x, 80, 12, 8, C_CYAN);
  }
  canvas.setTextColor(C_TEXT);
  canvas.drawCenterString("Drum Machine", 120, 96);

  canvas.drawFastHLine(0, 119, 240, C_CYAN);
  canvas.setTextColor(C_STEP);
  canvas.drawString(FW_VERSION, 3, 122);
  canvas.drawRightString("Taste = weiter", 237, 122);
  canvas.pushSprite(0, 0);
}

// Hilfe: Titel wie Kraehenauge (Magenta, cyane Linie), Abschnitte Magenta, Tasten hell
static void drawHelp() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(C_MAGENTA);
  canvas.drawString("Hilfe", 3, 0);
  canvas.setTextColor(C_STEP);
  canvas.drawRightString("Takt " FW_VERSION, 237, 0);
  canvas.drawFastHLine(0, 12, 240, C_CYAN);

  char key[16];
  for (int i = 0; i < HELP_ROWS && helpTop + i < HELP_LINES; i++) {
    const char* line = HELP[helpTop + i];
    int y = 14 + i * 13;
    if (line[0] == '#') {
      canvas.setTextColor(C_MAGENTA);
      canvas.drawString(line + 1, 3, y);
      continue;
    }
    const char* tab = strchr(line, '\t');
    if (tab) {
      int n = min<int>(tab - line, sizeof(key) - 1);
      memcpy(key, line, n); key[n] = 0;
      canvas.setTextColor(C_BRIGHT);
      canvas.drawString(key, 3, y);
      canvas.setTextColor(C_TEXT);
      canvas.drawString(tab + 1, 69, y);
    } else {
      canvas.setTextColor(C_TEXT);
      canvas.drawString(line, 3, y);
    }
  }
  if (HELP_LINES > HELP_ROWS) {                        // Scrollbalken rechts
    int h = 104 * HELP_ROWS / HELP_LINES;
    int y = 14 + (104 - h) * helpTop / (HELP_LINES - HELP_ROWS);
    canvas.fillRect(238, y, 2, h, C_CYAN);
  }
  canvas.drawFastHLine(0, 119, 240, C_CYAN);
  canvas.setTextColor(C_STEP);
  canvas.drawString("; . blättern  , / Seite  Esc zurück", 3, 122);
  canvas.pushSprite(0, 0);
}

// Song-Ansicht: 16 Plaetze in 2 Reihen, je Platz Pattern und Wiederholungen
static void drawSong() {
  canvas.fillSprite(TFT_BLACK);
  char b[48];
  int n = song.chainLen;
  int bars = 0;
  for (int i = 0; i < n; i++) bars += song.chainRep[i];

  canvas.setTextColor(C_MAGENTA);
  canvas.drawString("Song", 3, 0);
  canvas.setTextColor(songMode ? C_CYAN : C_STEP);
  canvas.drawString(songMode ? "Modus an" : "Modus aus", 39, 0);
  canvas.setTextColor(C_STEP);
  snprintf(b, sizeof(b), "%d Takte", bars);
  canvas.drawRightString(b, 237, 0);
  canvas.drawFastHLine(0, 12, 240, C_CYAN);

  int cp = chainPos;
  for (int i = 0; i < CHAIN_MAX; i++) {
    int x = 5 + (i % 8) * 29, y = 18 + (i / 8) * 46;
    if (i < n) {
      bool now = songMode && playing && i == cp;
      canvas.fillRect(x, y, 27, 40, now ? C_CELL_Q : C_CELL);
      if (now) {                                           // laeuft gerade: Magenta oben, Fortschritt unten
        canvas.fillRect(x, y, 27, 2, C_MAGENTA);
        canvas.fillRect(x, y + 38, 27 * (chainBar + 1) / song.chainRep[i], 2, C_CYAN);
      }
      snprintf(b, sizeof(b), "P%d", song.chainPat[i] + 1);
      canvas.setTextColor(C_BRIGHT);
      canvas.drawCenterString(b, x + 14, y + 7);
      snprintf(b, sizeof(b), "×%d", song.chainRep[i]);
      canvas.setTextColor(C_TEXT);
      canvas.drawCenterString(b, x + 14, y + 22);
    } else if (i == n) {                                   // freier Platz zum Anhaengen
      canvas.drawRect(x, y, 27, 40, C_OUTSIDE);
      canvas.setTextColor(C_STEP);
      canvas.drawCenterString("+", x + 14, y + 14);
    }
    if (i == songCursor) canvas.drawRect(x - 2, y - 2, 31, 44, C_CYAN);
  }

  canvas.setTextColor(C_TEXT);
  if (songCursor < n) snprintf(b, sizeof(b), "Platz %d: Pattern %d, %d Takt%s", songCursor + 1,
                               song.chainPat[songCursor] + 1, song.chainRep[songCursor], song.chainRep[songCursor] > 1 ? "e" : "");
  else if (n < CHAIN_MAX) snprintf(b, sizeof(b), "1-8 = Pattern anhängen");
  else snprintf(b, sizeof(b), "Kette voll (16 Plätze)");
  canvas.drawString(b, 3, 106);

  canvas.drawFastHLine(0, 119, 240, C_CYAN);
  if (millis() < msgUntil) {
    canvas.setTextColor(C_BRIGHT);
    canvas.drawString(msg, 3, 122);
  } else {
    canvas.setTextColor(C_STEP);
    canvas.drawString("-/= Takte  Del  Tab an/aus  Esc", 3, 122);
  }
  canvas.pushSprite(0, 0);
}

static void drawGrid() {
  canvas.fillSprite(TFT_BLACK);
  char b[48];

  // Kopfzeile: Play-Symbol, Pattern (Magenta), Tempo, Swing, Lautstaerke; cyane Linie darunter
  if (playing) canvas.fillTriangle(2, 2, 2, 10, 8, 6, C_CYAN);
  else canvas.fillRect(2, 3, 6, 6, C_STEP);
  int pp = pendingPat;
  if (songMode && song.chainLen > 0) {
    int cp = chainPos < song.chainLen ? chainPos : 0;
    snprintf(b, sizeof(b), "S%d P%d %d/%d", cp + 1, curPat + 1, chainBar + 1, song.chainRep[cp]);
  }
  else if (pp >= 0) snprintf(b, sizeof(b), "P%d>%d", curPat + 1, pp + 1);
  else              snprintf(b, sizeof(b), "P%d", curPat + 1);
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
  zub::begin();

  M5.Speaker.begin();
  M5.Speaker.setVolume(song.vol);
  M5.Speaker.setChannelVolume(0, 255);

  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0);
  bootStart = millis();
}

static void draw() {
  switch (mode) {
    case Mode::Boot:
      if (millis() - bootStart >= BOOT_MS) { leaveBoot(); drawGrid(); }
      else drawBoot();
      break;
    case Mode::Grid: drawGrid(); break;
    case Mode::Help: drawHelp(); break;
    case Mode::Song: drawSong(); break;
  }
}

void loop() {
  handleKeys();
  static uint32_t last = 0;
  if (millis() - last >= 33) { last = millis(); draw(); }
  zub::tick();
  delay(1);
}
