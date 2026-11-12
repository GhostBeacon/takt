# Takt

Custom firmware for the **M5Stack Cardputer ADV** (ESP32-S3FN8, 8 MB flash, TCA8418 keyboard chip), started through the [M5Launcher](https://github.com/bmorcelli/Launcher): a **drum machine** with 8 tracks and a 16-step sequencer, played entirely from the keyboard. All sounds are synthesised in real time – no samples, no Wi-Fi, no cloud. Output through the speaker and the 3.5 mm jack.

The firmware is deliberately small (~0.5 MB) so that it can stay installed in the launcher next to other apps.

*Takt* is German for the beat or bar in music.

## Features

- 8 tracks in the style of the 808/909: **BD** kick, **SD** snare, **CP** clap, **CH** closed hat, **OH** open hat (choked by CH), **LT** / **HT** toms, **CB** cowbell
- per track: tune (±12 semitones), decay, level, mute, solo
- 8 patterns × 16 steps; per step: on/off, **accent**, **probability** (100/75/50/25 %), **ratchet** (1–4 hits)
- **Polymeter**: individual length of 1–16 steps per track
- Tempo 40–250 BPM, swing 50–75 %
- A pattern change while playing takes effect **on the downbeat** (shown as `P1>2`)
- **Song mode**: a chain of up to 16 slots (pattern × 1–8 bars) plays automatically
- **Euclidean generator**, **random generator**, copy/paste of patterns
- Save the song to internal storage (Enter); it is loaded again at start-up
- Splash screen and **built-in help** with the key map (`?` or Esc)
- **Grove port accessories:** Unit OLED as a second display or Unit Hex as a light show

A detailed explanation of all features: **[MANUAL.md](MANUAL.md)**

## Installing

**Over Wi-Fi with the M5Launcher:** on every push to `main`, GitHub Actions builds the firmware and creates a release. The link to the latest version always stays the same:

```
https://github.com/GhostBeacon/takt/releases/latest/download/takt.bin
```

Add it once as a favourite: put the SD card into your computer, back up `/config.conf` first, then add an entry to the list `"favorite": [ … ]` (separated from the previous entry by a comma):

```json
{
  "name": "Takt",
  "fid": "",
  "link": "https://github.com/GhostBeacon/takt/releases/latest/download/takt.bin"
}
```

`fid` must stay empty, otherwise the launcher searches the M5Burner catalogue instead of using the link. Then in the launcher: **OTA → Favorite List → Takt → Install**; future updates work the same way. The launcher can also store the file on the SD card while doing so.

**From the SD card:** copy `takt.bin` from a [release](https://github.com/GhostBeacon/takt/releases) onto the card, then in the launcher **SD** → file → **Install**.

If several apps are installed, a digit on the launcher's start screen starts the corresponding app directly.

## Controls

| Key | Function |
|---|---|
| Space | Play / Stop |
| `?` (Shift + `/`) or Esc | Open help (`;` `.` scroll, Esc back) |
| `1`–`8`, `Q`–`I` | Steps 1–16 of the active track on/off |
| Shift + step | Accent |
| Opt/Alt + step | Probability 100 → 75 → 50 → 25 % |
| Ctrl + step | Ratchet 1 → 2 → 3 → 4 |
| `A S D F G H J K` | Select and audition a track |
| Fn + track key | Mute |
| Shift + track key | Solo (again = solo off) |
| `M` | Audition the active track |
| Fn + `1`–`8` | Select a pattern directly |
| `9` / `0` | Pattern − / + |
| `Z` / `X` | Tune − / + |
| `C` / `V` | Decay − / + |
| `B` / `N` | Level − / + |
| `,` / `.` | Tempo − / + (with Shift ±10) |
| `[` / `]` | Swing − / + |
| `;` / `'` | Track length − / + |
| `-` / `=` | Volume − / + |
| `L` / Shift + `L` | Euclid: one hit more / fewer |
| Tab | Fill the track at random |
| `O` / `P` | Copy / paste pattern |
| Del / Fn + Del | Clear track / clear the whole pattern |
| `\` | Song view: edit the pattern chain, Tab = song mode on/off |
| Enter | Save song |

## Display

- **Header:** play symbol, pattern (magenta, `P1>2` = change queued; in song mode `S2 P3 1/4` = slot, pattern, bar), tempo, swing, volume
- **Grid:** 8 tracks × 16 steps; brighter cells mark the quarter notes. Grey step = on, bright step = accent, smaller box = lower probability, dots = ratchets. Cells drawn as outlines only lie beyond the track length. The cyan frame is the play position – per track, because of polymeter.
- **Active track:** magenta edge on the left; muted tracks are dark grey
- **Footer:** parameters of the active track or a short message

A short splash screen appears at power-on (~2.6 s, any key skips it). The version is shown there and in the help (`1.0.<build number>`, local builds: `dev`).

## Grove port accessories

**One** of two extensions can be plugged into the Grove port (only one at a time):

- **Unit OLED** (SH1107, 1.3", 128×64): *Info* shows the pattern or song slot, the tempo and the play position in large type, *Levels* shows one bar per track that jumps up on every hit.
- **Unit Hex** (37 RGB LEDs): *Lights* makes every track flash in its own colour and zone (kick in the centre, hats on the outside) while the play position circles around the rim; *Track* shows the steps of the active track on the outer ring.

| Key | Function |
|---|---|
| Fn + `E` | Detect the device again (after swapping) |
| Fn + `R` | Switch view |
| Fn + `T` | Hex: brightness (3 levels) · OLED: rotate by 180° |

Detection happens at start-up: if an OLED answers on the port it is used, otherwise the Hex is driven. View, brightness and rotation are saved. The brightness of the Hex is deliberately limited (25 % at most) so that the battery and the Grove port are not overloaded. The driver code lives in [`lib/grove_units`](lib/grove_units) (detection, OLED, Hex driver).

## Building

```bash
python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/pip install platformio
~/.platformio/penv/bin/pio run
```

Result: `.pio/build/cardputer-adv/firmware.bin`. Serial output over USB: `~/.platformio/penv/bin/pio device monitor`.

## License

MIT, see [LICENSE](LICENSE). The embedded font Spleen 6x12 is licensed under BSD-2-Clause, see [docs/LICENSE-spleen.txt](docs/LICENSE-spleen.txt).
