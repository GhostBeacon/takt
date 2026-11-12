// GroveUnits – Grove accessories for the M5Stack Cardputer ADV
//
// Detects what is plugged into the Grove port and drives it:
//   - Unit OLED (SH1107, 128x64, I2C 0x3C) through M5GFX
//   - Unit Hex (37 x SK6812) through the RMT peripheral, data on the yellow wire (G2)
// Only one of the two devices is connected at a time.
//
// Detection runs on request only (at start-up and on a key press), never continuously:
// an I2C scan would show up on the Hex as random flicker.
#pragma once

#include <Arduino.h>
#include <M5UnitOLED.h>

namespace grove {

enum class Device : uint8_t { None, Oled, Hex };

// Grove port of the Cardputer ADV: G2 = SDA (yellow), G1 = SCL (white)
constexpr uint8_t PIN_YELLOW = 2;
constexpr uint8_t PIN_WHITE = 1;

// Probes the Grove port and sets up the device it finds. If something answers on 0x3C
// it is the OLED, otherwise the Hex is assumed (it cannot answer).
Device detect();
Device device();
const char* name(Device d);

// ---------------------------------------------------------------- OLED
// 128 x 64 in landscape. Only valid while device() == Device::Oled.
M5UnitOLED& oled();
void oledFlip(bool flipped);     // rotate the display by 180 degrees (cable at the top or bottom)

// ---------------------------------------------------------------- Hex
namespace hex {

constexpr int N = 37;            // rows 4-5-6-7-6-5-4, each row left to right, 0 = top left
constexpr int RINGS = 4;         // ring 0 = centre (1 LED), then 6, 12, 18 LEDs

// Brightness in percent, hard-limited to 40 %: 37 LEDs draw up to ~2 A at full white.
void brightness(int percent);
uint8_t brightness();

void clear();
void set(int i, uint32_t rgb);   // colour 0xRRGGBB (full scale, brightness is applied when sending)
void add(int i, uint32_t rgb);   // add, each channel capped at 255
void fill(uint32_t rgb);
bool show();                     // sends the frame (~1.2 ms), false if no Hex is active

int ringLength(int ring);        // 1, 6, 12, 18
int ring(int ring, int k);       // LED index: k-th LED of the ring, clockwise from the top
void ringFill(int ring, uint32_t rgb, bool additive = false);

}  // namespace hex

// ---------------------------------------------------------------- Colours
uint32_t scale(uint32_t rgb, uint8_t factor);      // factor 0..255
uint32_t blend(uint32_t a, uint32_t b);            // add per channel, capped

}  // namespace grove
