// GroveUnits – Grove accessories for the M5Stack Cardputer ADV (see GroveUnits.h)
#include "GroveUnits.h"

#include <driver/rmt.h>
#include <math.h>

namespace grove {

// I2C port 0 is free on the Cardputer ADV: M5Unified puts the keyboard and audio codec on port 1.
static constexpr int I2C_PORT = 0;
static constexpr uint8_t OLED_ADDR = 0x3C;

static Device active = Device::None;

// ---------------------------------------------------------------- OLED
static M5UnitOLED oledObj(PIN_YELLOW, PIN_WHITE, 400000, I2C_PORT, OLED_ADDR);
static bool oledReady = false;    // init() has run once (M5GFX does not initialise again after that)
static bool oledFlipped = false;

M5UnitOLED& oled() { return oledObj; }

static void oledRotation() { oledObj.setRotation(oledFlipped ? 3 : 1); }

void oledFlip(bool flipped) {
  oledFlipped = flipped;
  if (active == Device::Oled) {
    oledRotation();
    oledObj.clear();
  }
}

static bool oledStart() {
  bool ok;
  if (!oledReady) ok = oledReady = oledObj.init();
  else ok = oledObj.getPanel()->init(false);   // after switching from the Hex: set up bus and controller again
  if (!ok) return false;
  oledRotation();
  oledObj.clear();
  return true;
}

static void oledStop() {
  oledObj.clear();
  oledObj.getPanel()->getBus()->release();
}

// ---------------------------------------------------------------- Hex (SK6812 through RMT)
namespace hex {

static constexpr rmt_channel_t CHANNEL = RMT_CHANNEL_3;  // S3: TX channels 0-3, the Arduino HAL allocates from the front
static uint8_t frame[N][3];                              // R, G, B at full scale
static rmt_item32_t items[N * 24];
static uint8_t level = 10;
static bool on = false;

// 40 MHz (clk_div 2) -> 25 ns per tick. SK6812: 0 = 0.3/0.9 us, 1 = 0.6/0.6 us
static constexpr rmt_item32_t BIT_ZERO = {{{12, 1, 36, 0}}};
static constexpr rmt_item32_t BIT_ONE = {{{24, 1, 24, 0}}};

static bool start() {
  rmt_config_t c = RMT_DEFAULT_CONFIG_TX((gpio_num_t)PIN_YELLOW, CHANNEL);
  c.clk_div = 2;
  if (rmt_config(&c) != ESP_OK) return false;
  if (rmt_driver_install(CHANNEL, 0, 0) != ESP_OK) return false;
  on = true;
  return true;
}

static void stop() {
  if (!on) return;
  rmt_driver_uninstall(CHANNEL);
  gpio_reset_pin((gpio_num_t)PIN_YELLOW);
  on = false;
}

void brightness(int percent) { level = percent > 40 ? 40 : (percent < 1 ? 1 : percent); }
uint8_t brightness() { return level; }

void clear() { memset(frame, 0, sizeof(frame)); }

void set(int i, uint32_t rgb) {
  if (i < 0 || i >= N) return;
  frame[i][0] = rgb >> 16;
  frame[i][1] = rgb >> 8;
  frame[i][2] = rgb;
}

void add(int i, uint32_t rgb) {
  if (i < 0 || i >= N) return;
  uint32_t old = ((uint32_t)frame[i][0] << 16) | ((uint32_t)frame[i][1] << 8) | frame[i][2];
  set(i, blend(old, rgb));
}

void fill(uint32_t rgb) {
  for (int i = 0; i < N; i++) set(i, rgb);
}

bool show() {
  if (!on) return false;
  uint32_t f = (uint32_t)level * 255 / 100;   // percent -> factor 0..102
  rmt_item32_t* it = items;
  for (int i = 0; i < N; i++) {
    const uint8_t grb[3] = {frame[i][1], frame[i][0], frame[i][2]};   // SK6812 expects G, R, B
    for (uint8_t v : grb) {
      uint8_t s = (v * f + 254) / 255;   // round up so faint colours are not lost entirely
      for (int b = 7; b >= 0; b--) *it++ = (s >> b) & 1 ? BIT_ONE : BIT_ZERO;
    }
  }
  return rmt_write_items(CHANNEL, items, N * 24, true) == ESP_OK;
}

// Rings: distance from the centre in the hexagonal grid, sorted clockwise from the top
static const int ROW_LEN[7] = {4, 5, 6, 7, 6, 5, 4};
static const int RING_LEN[RINGS] = {1, 6, 12, 18};
static int8_t ringIdx[RINGS][18];
static bool ringsBuilt = false;

static void buildRings() {
  float angle[RINGS][18];
  int n[RINGS] = {0};
  int i = 0;
  for (int r = 0; r < 7; r++) {
    int rr = r - 3;                          // axial coordinates: rr row, q column
    int qmin = rr < 0 ? -3 - rr : -3;
    for (int c = 0; c < ROW_LEN[r]; c++, i++) {
      int q = qmin + c;
      int d = (abs(q) + abs(rr) + abs(q + rr)) / 2;
      float x = q + rr * 0.5f, y = rr * 0.8660254f;
      float w = atan2f(x, -y);               // 0 = top, positive clockwise (y points down)
      if (w < -1e-4f) w += 2 * PI;
      int k = n[d]++;
      ringIdx[d][k] = i;
      angle[d][k] = w;
    }
  }
  for (int d = 1; d < RINGS; d++)            // insertion sort by angle
    for (int a = 1; a < n[d]; a++)
      for (int b = a; b > 0 && angle[d][b] < angle[d][b - 1]; b--) {
        std::swap(angle[d][b], angle[d][b - 1]);
        std::swap(ringIdx[d][b], ringIdx[d][b - 1]);
      }
  ringsBuilt = true;
}

int ringLength(int r) { return (r >= 0 && r < RINGS) ? RING_LEN[r] : 0; }

int ring(int r, int k) {
  if (r < 0 || r >= RINGS) return -1;
  if (!ringsBuilt) buildRings();
  int len = RING_LEN[r];
  k %= len;
  if (k < 0) k += len;
  return ringIdx[r][k];
}

void ringFill(int r, uint32_t rgb, bool additive) {
  for (int k = 0; k < ringLength(r); k++) {
    if (additive) add(ring(r, k), rgb);
    else set(ring(r, k), rgb);
  }
}

}  // namespace hex

// ---------------------------------------------------------------- Detection
static bool oledAnswers() {
  if (!lgfx::i2c::init(I2C_PORT, PIN_YELLOW, PIN_WHITE).has_value()) return false;
  bool found = false;
  for (int v = 0; v < 3 && !found; v++) {
    bool ack = lgfx::i2c::beginTransaction(I2C_PORT, OLED_ADDR, 100000, false).has_value();
    found = lgfx::i2c::endTransaction(I2C_PORT).has_value() && ack;
    if (!found) delay(2);
  }
  lgfx::i2c::release(I2C_PORT);
  return found;
}

Device detect() {
  if (active == Device::Hex) {
    hex::clear();
    hex::show();
    hex::stop();
  } else if (active == Device::Oled) {
    oledStop();
  }
  active = Device::None;

  if (oledAnswers()) {
    if (oledStart()) active = Device::Oled;
  } else if (hex::start()) {
    active = Device::Hex;
    hex::clear();
    hex::show();   // overwrite what the I2C scan left on the data line
  }
  return active;
}

Device device() { return active; }

const char* name(Device d) {
  switch (d) {
    case Device::Oled: return "OLED";
    case Device::Hex: return "Hex";
    default: return "no device";
  }
}

// ---------------------------------------------------------------- Colours
uint32_t scale(uint32_t rgb, uint8_t f) {
  uint32_t r = ((rgb >> 16) & 255) * f / 255, g = ((rgb >> 8) & 255) * f / 255, b = (rgb & 255) * f / 255;
  return (r << 16) | (g << 8) | b;
}

uint32_t blend(uint32_t a, uint32_t b) {
  uint32_t r = ((a >> 16) & 255) + ((b >> 16) & 255);
  uint32_t g = ((a >> 8) & 255) + ((b >> 8) & 255);
  uint32_t bl = (a & 255) + (b & 255);
  return ((r > 255 ? 255 : r) << 16) | ((g > 255 ? 255 : g) << 8) | (bl > 255 ? 255 : bl);
}

}  // namespace grove
