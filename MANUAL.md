# Takt – Manual

Takt is a drum machine with 8 tracks and 16 steps. All sounds are generated on the device. A short version of this manual is built into the Cardputer as well: **`?`** (Shift + `/`) or **Esc** opens it.

## Getting started

1. Start Takt in the M5Launcher. The splash screen disappears after a good two seconds or on any key press.
2. Press **Space** – the demo beat plays.
3. Select a track with **`A`** … **`K`** (e.g. `S` for the snare). The track is marked with a magenta edge on the left and sounds once.
4. Switch the 16 steps of this track on and off with **`1`–`8`** and **`Q`–`I`**.
5. **Enter** saves everything permanently.

## The display

```
 ▶ P1   120 BPM  SW54%          VOL63    ← header
 ─────────────────────────────────────
 BD  ■ · · · ■ · · · ■ · ▪ · ■ · · ·      ← 8 tracks × 16 steps
▌SD  · · · · ■ · · · · · · · ■ · · ·      ← active track (magenta edge)
 …
 ─────────────────────────────────────
 SD  Tune+0  Dec1.0  Lvl8  Len16          ← footer
```

| Element | Meaning |
|---|---|
| ▶ (cyan) / ■ (grey) | playing / stopped |
| `P1` | current pattern; `P1>2` = a change to pattern 2 is queued |
| `SW54%` | swing |
| `VOL63` | volume in percent |
| brighter cells | quarter notes (steps 1, 5, 9, 13) |
| grey step | the step plays |
| bright step | the step plays with an **accent** (louder) |
| smaller box | the step only plays with a certain **probability** |
| black dots in a step | **ratchet**: several fast hits |
| outline only | lies beyond the end of the track (track length < 16) |
| cyan frame | current play position, per track |
| dark grey track | muted |
| footer | values of the active track or a short message |

## The 8 tracks

| Key | Track | Sound |
|---|---|---|
| `A` | **BD** bass drum | sine with falling pitch and a click |
| `S` | **SD** snare | tone plus filtered noise |
| `D` | **CP** clap | three short claps and a tail |
| `F` | **CH** closed hat | metallic, short |
| `G` | **OH** open hat | metallic, long – choked by the closed hat |
| `H` | **LT** low tom | low tom |
| `J` | **HT** high tom | high tom |
| `K` | **CB** cowbell | two tones through a band-pass filter |

Each track has its own values, shown in the footer:

- **Tune** (`Z` / `X`): pitch in semitones, −12 to +12
- **Decay** (`C` / `V`): decay time, 0.1× to 3.0×
- **Level** (`B` / `N`): volume of the track, 0 to 10
- **Len** (`;` / `'`): track length, 1 to 16 steps

## Editing steps

| Key | Effect |
|---|---|
| Step key | on / off |
| Shift + step | accent on / off (switches an empty step on with an accent right away) |
| Opt + step | probability 100 → 75 → 50 → 25 % |
| Ctrl + step | ratchet 1 → 2 → 3 → 4 hits |

Probability and ratchet only affect steps that are on. A step that is switched off loses both settings.

## Controlling tracks

| Key | Effect |
|---|---|
| `A`–`K` | select and audition a track |
| `M` | audition the active track again |
| Fn + track key | mute on / off |
| Shift + track key | solo: hear only this track; again = all back on |
| Del | clear the active track |
| Tab | fill the active track at random (density suited to the track, e.g. many hats, few toms) |
| `L` | Euclid: one hit more |
| Shift + `L` | Euclid: one hit fewer |

**Euclid** spreads the hits as evenly as possible over the track length. 3 hits on 8 steps, for example, give the well-known tresillo rhythm. The existing steps of the track are replaced.

## Polymeter

Each track can have its own length (`;` / `'`). If the cowbell is 7 steps long, for example, and the rest 16, the cowbell repeats earlier and shifts against the other tracks – the rhythm only repeats after 112 steps. The cyan frame shows the position of each track individually.

On a pattern change all tracks start together on step 1 again.

## Patterns

Takt has 8 patterns. Exactly one of them plays at a time.

| Key | Effect |
|---|---|
| Fn + `1`–`8` | select a pattern directly |
| `9` / `0` | previous / next pattern |
| `O` | copy the pattern |
| `P` | paste the copied pattern into the current one |
| Fn + Del | clear the whole pattern |

While playing, the change only takes effect **on the next downbeat** – this keeps the groove in time. Until then the header shows e.g. `P1>2`.

Tip: copy a pattern (`O`), switch to the next pattern (`0`), paste (`P`) and build a variation there.

## Song mode (automatic sequence)

In song mode Takt plays a **chain** of patterns automatically one after the other, e.g. 4× basic beat, 4× verse, 1× fill, 4× chorus. The chain has up to 16 slots. Each slot consists of a pattern and a number of bars (1–8). At the end the chain starts over.

**`\`** (backslash, to the right of `]`) opens the song view:

```
 Song  mode on                   13 bars
 ─────────────────────────────────────
 ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌┄┄┐
 │P1│ │P2│ │P4│ │P3│ │ +│               ← slots 1–8
 │×4│ │×4│ │×1│ │×4│ └┄┄┘
 └──┘ └──┘ └──┘ └──┘
                                        ← slots 9–16
 Slot 3: pattern 4, 1 bar
 ─────────────────────────────────────
```

| Key | Effect |
|---|---|
| `;` `.` `,` `/` (arrows) | select a slot (cyan frame) |
| `1`–`8` | set the pattern for the selected slot; on the **+** slot: append |
| Shift + `1`–`8` | insert a new slot before the selected one |
| `-` / `=` | bars for this slot − / + |
| Del | delete the slot |
| Tab | song mode on / off (starts on the next downbeat from the selected slot) |
| Space | Play / Stop – starts from the selected slot |
| Enter | save |
| Esc or `\` | back to the grid |

The slot that is currently playing has a magenta edge at the top; the cyan bar at the bottom shows how many of its bars have been played.

With song mode active, the grid header shows e.g. **`S2 P3 1/4`**: slot 2 of the chain, pattern 3, bar 1 of 4. Space in the grid starts the chain from the beginning. You can edit steps in the meantime – the changes go into the pattern that is currently playing.

If you select a pattern by hand (Fn + `1`–`8`, `9`, `0`), song mode ends and the selected pattern keeps playing as a loop.

## Tempo, swing, volume

| Key | Effect |
|---|---|
| `,` / `.` | tempo − / + (40–250 BPM); with Shift in steps of 10 |
| `[` / `]` | swing − / + (50–75 %) |
| `-` / `=` | volume − / + |

**Swing** delays every second sixteenth step. 50 % is straight, around 58 % sounds slightly shuffled, 66 % is a triplet feel.

## Accessories: OLED or Hex

A **Unit OLED** or a **Unit Hex** can be plugged into the Grove port (the four-pin socket on the side), only one of the two at a time. Takt detects at start-up what is connected: if an OLED answers it is used, otherwise the Hex is driven. After swapping devices while Takt is running, press **Fn + E**; the footer reports what was detected.

**Fn + R** switches the view:

- **Hex – Lights:** every track has a colour and a zone. Kick red in the centre, cowbell magenta on the inner ring, snare orange and clap yellow on the middle ring, the toms green on one half of it each, closed and open hat cyan and blue on the outer ring. Accents shine brighter. A white dot circles around the rim in time, brighter on the quarter notes. When stopped, the centre glows faintly cyan.
- **Hex – Track:** the 16 steps of the active track lie on the outer ring, starting at the top and running clockwise. Accents are bright, normal steps dimmed, the play position white. The centre flashes on every hit of the track.
- **OLED – Info:** pattern (or song slot), tempo, swing and active track in large type, below them 16 boxes with the play position.
- **OLED – Levels:** eight bars, one per track; they jump up on every hit and then fall off. The active track is highlighted.

**Fn + T** changes the brightness on the Hex (3 levels) and rotates the display by 180° on the OLED, depending on which way up the display is mounted. View, brightness and rotation are saved immediately.

## Saving

**Enter** saves all 8 patterns, the song chain (including on/off), the track values, tempo, swing and volume to the internal storage of the Cardputer. The song is loaded automatically at the next start. Unsaved changes are lost when the device is switched off.

The internal storage is shared with the M5Launcher and other apps. Takt needs only a good 1 KB of it. Should **“Error: storage full”** or **“Error while saving”** appear nevertheless, the message shows how much space was still free – please report this value in an issue.

## The built-in help

`?` (Shift + `/`) or **Esc** opens the help.

| Key | Effect |
|---|---|
| `;` / `.` | one line up / down |
| `,` / `/` | one page back / forward |
| Space | Play / Stop (keeps running while the help is open) |
| Esc, Del or Enter | back (to the grid or the song view) |
