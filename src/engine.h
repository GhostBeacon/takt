// Takt – Klangerzeugung + Sequencer (laeuft im Audio-Task)
#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>

constexpr int SR = 44100;          // Samplerate
constexpr int NT = 8;              // Spuren
constexpr int NS = 16;             // Steps
constexpr int NP = 8;              // Patterns
constexpr uint32_t MAGIC = 0xD5A70003;
constexpr int CHAIN_MAX = 16;      // Song-Modus: Plaetze in der Pattern-Kette

enum { BD, SD, CP, CH, OH, LT, HT, CB };
static const char* TRACK_NAMES[NT] = {"BD", "SD", "CP", "CH", "OH", "LT", "HT", "CB"};
static const uint8_t PROB_PCT[4] = {100, 75, 50, 25};

// ---------------------------------------------------------------- Daten
struct Step { uint8_t on, prob, rat; };          // on: 0 aus, 1 an, 2 Accent | prob: Index in PROB_PCT | rat: 1..4
struct Pattern { Step s[NT][NS]; uint8_t len[NT]; };
struct TrackParams { int8_t tune; uint8_t decay, level, mute; }; // tune -12..12 HT, decay 1..30 (/10), level 0..10
struct Song {
  uint32_t magic;
  Pattern pat[NP];
  TrackParams tp[NT];
  uint16_t bpm;
  uint8_t swing;   // 50..75 %
  uint8_t vol;     // 0..255
  uint8_t chainLen;                 // Song-Modus: Anzahl belegter Plaetze
  uint8_t chainPat[CHAIN_MAX];      // Pattern 0..7 je Platz
  uint8_t chainRep[CHAIN_MAX];      // Wiederholungen 1..8 je Platz
};

static Song song;

// Zustand, den UI und Audio-Task teilen
static volatile bool playing = false;
static volatile int cmd = 0;              // 1 = Start, 2 = Stop
static volatile int curPat = 0;
static volatile int pendingPat = -1;
static volatile uint32_t padMask = 0;     // Vorhör-Trigger aus der UI
static volatile uint8_t uiStep[NT];
static volatile bool songMode = false;    // Kette statt Einzel-Pattern abspielen
static volatile int chainPos = 0;         // aktueller Platz in der Kette
static volatile int chainBar = 0;         // gespielte Takte auf diesem Platz
static volatile int songJump = -1;        // UI: an der naechsten Eins zu diesem Platz springen

// ---------------------------------------------------------------- DSP-Helfer
static float sineLUT[1024];
static inline float fsin(float ph) { return sineLUT[(int)(ph * 1024.0f) & 1023]; }

static uint32_t nrng = 0x13572468;
static inline uint32_t xr() { nrng ^= nrng << 13; nrng ^= nrng >> 17; nrng ^= nrng << 5; return nrng; }
static inline float noise() { return (int32_t)xr() * (1.0f / 2147483648.0f); }
static inline float coef(float sec) { return expf(-1.0f / (sec * SR)); }
static inline float hp1(float& y, float& xp, float x, float a) { y = a * (y + x - xp); xp = x; return y; }
static inline float wrap(float p) { return p >= 1.0f ? p - 1.0f : p; }
static inline float softclip(float x) {
  if (x > 3.0f) x = 3.0f;
  if (x < -3.0f) x = -3.0f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static const float A_HP_SNARE = 1.0f / (1.0f + 2.0f * PI * 1500.0f / SR);
static const float A_HP_HAT   = 1.0f / (1.0f + 2.0f * PI * 7000.0f / SR);
static const float HAT_F[6]   = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f}; // 808-Metallic-Oszillatoren

// ---------------------------------------------------------------- Stimmen
struct Voice {
  bool active; uint32_t t; float vel;
  float ph[6];
  float env, envC, env2, env2C, penv, penvC, pamt, base;
  float y1, x1, y2, x2;   // Hochpass-Zustände
  float lp, bp, f, q;     // State-Variable-Filter
};
static Voice voices[NT];

static inline void setSVF(Voice& v, float fc, float q) { v.f = 2.0f * sinf(PI * fc / SR); v.q = q; }
static inline float svfBP(Voice& v, float x) {
  v.lp += v.f * v.bp;
  float h = x - v.lp - v.q * v.bp;
  v.bp += v.f * h;
  return v.bp;
}

static void trigger(int tr, float vel) {
  Voice& v = voices[tr];
  const TrackParams& p = song.tp[tr];
  float tm = powf(2.0f, p.tune / 12.0f);
  float dm = p.decay / 10.0f;
  memset(&v, 0, sizeof(Voice));
  v.active = true;
  v.vel = vel * p.level / 10.0f;
  v.env = 1.0f;
  switch (tr) {
    case BD: v.base = 50 * tm;  v.envC = coef(0.35f * dm); v.penv = 1; v.penvC = coef(0.03f); v.pamt = 3.0f; break;
    case SD: v.base = 185 * tm; v.envC = coef(0.06f * dm); v.env2 = 1; v.env2C = coef(0.14f * dm);
             v.penv = 1; v.penvC = coef(0.01f); v.pamt = 0.6f; break;
    case CP: v.envC = coef(0.005f); v.env2C = coef(0.16f * dm); setSVF(v, 1100 * tm, 0.7f); break;
    case CH: v.base = 2.0f * tm; v.envC = coef(0.035f * dm); break;
    case OH: v.base = 2.0f * tm; v.envC = coef(0.30f * dm); break;
    case LT: v.base = 90 * tm;  v.envC = coef(0.30f * dm); v.penv = 1; v.penvC = coef(0.05f); v.pamt = 0.7f; break;
    case HT: v.base = 140 * tm; v.envC = coef(0.25f * dm); v.penv = 1; v.penvC = coef(0.05f); v.pamt = 0.7f; break;
    case CB: v.base = tm; v.envC = coef(0.012f); v.env2 = 1; v.env2C = coef(0.10f * dm); setSVF(v, 1700 * tm, 0.9f); break;
  }
  if (tr == CH && voices[OH].active) voices[OH].envC = coef(0.005f);  // Closed HH würgt Open HH ab
}

static float renderVoice(int tr, Voice& v) {
  float s = 0;
  switch (tr) {
    case BD: case LT: case HT: {
      v.ph[0] = wrap(v.ph[0] + v.base * (1.0f + v.pamt * v.penv) / SR);
      s = fsin(v.ph[0]) * v.env;
      if (tr == BD && v.t < 90) s += noise() * 0.25f * (1.0f - v.t / 90.0f);   // Klick
      v.penv *= v.penvC;
      break;
    }
    case SD: {
      v.ph[0] = wrap(v.ph[0] + v.base * (1.0f + v.pamt * v.penv) / SR);
      float n = hp1(v.y1, v.x1, noise(), A_HP_SNARE);
      s = 0.55f * fsin(v.ph[0]) * v.env + 0.75f * n * v.env2;
      v.penv *= v.penvC; v.env2 *= v.env2C;
      break;
    }
    case CP: {
      if (v.t == 441 || v.t == 882) v.env = 1.0f;    // drei Klatscher im 10-ms-Abstand
      if (v.t == 1323) v.env2 = 1.0f;                // danach Hallfahne
      float n = svfBP(v, noise());
      s = n * (v.t < 1323 ? v.env : v.env2) * 2.0f;
      v.env2 *= v.env2C;
      break;
    }
    case CH: case OH: {
      float m = 0;
      for (int k = 0; k < 6; k++) {
        v.ph[k] = wrap(v.ph[k] + HAT_F[k] * v.base / SR);
        m += v.ph[k] < 0.5f ? 1.0f : -1.0f;
      }
      m = m * (1.0f / 6.0f) * 0.7f + noise() * 0.3f;
      float h = hp1(v.y1, v.x1, m, A_HP_HAT);
      h = hp1(v.y2, v.x2, h, A_HP_HAT);
      s = h * v.env * 1.6f;
      break;
    }
    case CB: {
      v.ph[0] = wrap(v.ph[0] + 540.0f * v.base / SR);
      v.ph[1] = wrap(v.ph[1] + 800.0f * v.base / SR);
      float m = (v.ph[0] < 0.5f ? 1.0f : -1.0f) + (v.ph[1] < 0.5f ? 1.0f : -1.0f);
      s = svfBP(v, m * 0.5f) * (0.6f * v.env + v.env2) * 1.2f;
      v.env2 *= v.env2C;
      break;
    }
  }
  v.env *= v.envC;
  v.t++;
  if ((v.t > 1500 && v.env < 0.0005f && v.env2 < 0.0005f) || v.t > (uint32_t)SR * 4) v.active = false;
  return s;
}

// ---------------------------------------------------------------- Sequencer
struct Ratchet { int left, interval, cnt; float vel; };
static Ratchet rat[NT];
static int gstep = 0;
static int tpos[NT];
static double toNext = 0;
static bool firstStep = true;             // erster Step nach Start zaehlt nicht als Taktende

static void resetPositions() {
  gstep = 0;
  for (int t = 0; t < NT; t++) { tpos[t] = 0; rat[t].left = 0; }
}

// Song-Modus: am Taktanfang Kette weiterschalten (oder an einen Platz springen)
static void songBar() {
  int n = song.chainLen;
  if (n <= 0) return;
  int prev = curPat;
  if (songJump >= 0) {
    chainPos = songJump < n ? songJump : 0;
    chainBar = 0;
    songJump = -1;
  } else if (!firstStep) {
    if (chainPos >= n) chainPos = 0;
    if (++chainBar >= song.chainRep[chainPos]) {
      chainBar = 0;
      chainPos = (chainPos + 1) % n;             // am Ende von vorn
    }
  }
  if (chainPos >= n) chainPos = 0;
  curPat = song.chainPat[chainPos] & 7;
  pendingPat = -1;
  if (curPat != prev || chainBar == 0) for (int t = 0; t < NT; t++) tpos[t] = 0;
}

static void doStep() {
  if (gstep == 0) {
    if (songMode && song.chainLen > 0) songBar();
    else if (pendingPat >= 0) {                  // Patternwechsel am Taktanfang
      curPat = pendingPat; pendingPat = -1;
      for (int t = 0; t < NT; t++) tpos[t] = 0;
    }
    firstStep = false;
  }
  Pattern& P = song.pat[curPat];
  double sl = (double)SR * 60.0 / song.bpm / 4.0;
  double sw = song.swing / 100.0;
  double thisLen = (gstep % 2 == 0) ? 2.0 * sl * sw : 2.0 * sl * (1.0 - sw);

  for (int t = 0; t < NT; t++) {
    int p = tpos[t];
    const Step& st = P.s[t][p];
    uiStep[t] = p;
    if (st.on && !song.tp[t].mute && (int)(xr() % 100) < PROB_PCT[st.prob & 3]) {
      float vel = st.on == 2 ? 1.0f : 0.7f;
      trigger(t, vel);
      if (st.rat > 1) {
        rat[t].left = st.rat - 1;
        rat[t].interval = (int)(thisLen / st.rat);
        rat[t].cnt = rat[t].interval;
        rat[t].vel = vel * 0.8f;
      } else {
        rat[t].left = 0;
      }
    }
    int len = P.len[t] < 1 ? 1 : P.len[t];
    tpos[t] = (p + 1) % len;                     // eigene Länge je Spur = Polymeter
  }
  gstep = (gstep + 1) % NS;
  toNext += thisLen;
}

static void engineInit() {
  for (int i = 0; i < 1024; i++) sineLUT[i] = sinf(2.0f * PI * i / 1024.0f);
  memset(voices, 0, sizeof(voices));
  resetPositions();
}

static void render(int16_t* out, int n) {
  int c = cmd;
  if (c) {
    cmd = 0;
    if (c == 1) { resetPositions(); toNext = 0; firstStep = true; playing = true; }
    else { playing = false; for (int t = 0; t < NT; t++) rat[t].left = 0; }
  }
  if (!playing && pendingPat >= 0) { curPat = pendingPat; pendingPat = -1; }
  if (!playing && songJump >= 0 && songMode && song.chainLen > 0) {  // gestoppt: Platz sofort anzeigen
    chainPos = songJump < song.chainLen ? songJump : 0;
    chainBar = 0;
    curPat = song.chainPat[chainPos] & 7;
  }

  uint32_t pads = __atomic_exchange_n(&padMask, 0u, __ATOMIC_SEQ_CST);
  for (int t = 0; t < NT; t++) if (pads & (1u << t)) trigger(t, 0.9f);

  for (int i = 0; i < n; i++) {
    if (playing) {
      toNext -= 1.0;
      if (toNext <= 0) doStep();
      for (int t = 0; t < NT; t++) {
        if (rat[t].left > 0 && --rat[t].cnt <= 0) {
          trigger(t, rat[t].vel);
          rat[t].left--;
          rat[t].cnt = rat[t].interval;
        }
      }
    }
    float mix = 0;
    for (int t = 0; t < NT; t++) if (voices[t].active) mix += renderVoice(t, voices[t]) * voices[t].vel;
    out[i] = (int16_t)(softclip(mix * 0.6f) * 30000.0f);
  }
}

// ---------------------------------------------------------------- Default-Song
static void defaultSong() {
  memset(&song, 0, sizeof(song));
  song.magic = MAGIC;
  song.bpm = 120; song.swing = 54; song.vol = 160;
  for (int t = 0; t < NT; t++) song.tp[t] = {0, 10, 8, 0};
  song.tp[BD].level = 10;
  for (int p = 0; p < NP; p++)
    for (int t = 0; t < NT; t++) {
      song.pat[p].len[t] = NS;
      for (int s = 0; s < NS; s++) song.pat[p].s[t][s] = {0, 0, 1};
    }
  // Pattern 1: Grundbeat
  Pattern& a = song.pat[0];
  for (int s : {0, 4, 8, 12}) a.s[BD][s].on = 1;
  a.s[BD][0].on = 2;
  for (int s : {4, 12}) a.s[SD][s].on = 1;
  for (int s = 0; s < NS; s += 2) a.s[CH][s].on = (s % 4 == 0) ? 2 : 1;
  a.s[OH][14].on = 1;
  // Pattern 2: Variation mit Ratchets, Probability und Polymeter
  Pattern& b = song.pat[1];
  memcpy(&b, &a, sizeof(Pattern));
  b.s[BD][10].on = 1; b.s[BD][10].prob = 2;
  b.s[CP][12].on = 2;
  b.s[CH][14].rat = 3;
  b.s[CB][0].on = 1; b.s[CB][3].on = 1; b.s[CB][6].on = 1; b.len[CB] = 7;
  b.s[LT][13].on = 1; b.s[HT][15].on = 1;
  // Demo-Kette: 3x Grundbeat, 1x Variation
  song.chainLen = 2;
  song.chainPat[0] = 0; song.chainRep[0] = 3;
  song.chainPat[1] = 1; song.chainRep[1] = 1;
}
