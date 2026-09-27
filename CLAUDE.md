# Takt – Drum Machine für M5Stack Cardputer-Adv

## Ziel
Eigenständige Firmware, die den Cardputer-Adv zu einer erweiterten Drum Machine macht:
8 synthetisierte Drum-Spuren, 16-Step-Sequencer, 8 Patterns, spielbar komplett über die eingebaute Tastatur,
Ausgabe über Lautsprecher und 3,5-mm-Klinke. Kein WLAN, keine Cloud, keine Samples nötig (alles wird in Echtzeit synthetisiert).

## Hardware (Cardputer-Adv, NICHT der originale Cardputer v1.1)
- MCU: Stamp-S3A, ESP32-S3FN8, 8 MB Flash, **kein PSRAM**
- Display: 1,14" ST7789V2, 240×135 (Rotation 1 = Querformat)
- Tastatur: 56 Tasten (4×14) über **TCA8418** I²C-Controller (Adresse 0x34). Wird erst ab **M5Cardputer ≥ 1.1.1** unterstützt – ältere Versionen lesen keine Tasten.
- Audio: **ES8311**-Codec (I²C 0x18) + NS4150B-Verstärker, 1-W-Lautsprecher, 3,5-mm-Klinke; MEMS-Mikrofon über ES8311
- Sensoren/IO: BMI270 IMU (0x68/0x69), IR-Sender, microSD, Grove HY2.0-4P, EXT 2.54-14P
- Akku 1750 mAh, seitlicher Power-Schalter
- Flashen: Power OFF → G0 gedrückt halten → Power ON → loslassen = Download-Modus

## Toolchain
- PlatformIO, Arduino-Framework – gleiche Basis wie Krähenauge (auf der Hardware bewährt)
- `platform = espressif32@6.7.0`, `board = esp32-s3-devkitc-1`, 8 MB Flash, `default_8MB.csv`
- Library: `m5stack/M5Cardputer@^1.1.1` (zieht M5Unified/M5GFX nach; Cardputer ADV + ES8311 werden unterstützt)
- Build lokal: `~/.platformio/penv/bin/pio run` · Log: `pio device monitor` (115200)
- `FW_VERSION` setzt der Workflow (`1.0.<run_number>`), lokal `dev`

## Verteilung (GitHub + M5Launcher)
- Öffentliches Repo **GhostBeacon/takt**, Lizenz MIT (Spleen-Schrift BSD-2, siehe `docs/`)
- `.github/workflows/firmware.yml`: jeder Push auf `main` → Release `v1.0.<n>` mit fester Datei `takt.bin`
- Nutzer laden per M5Launcher **OTA → Favorite** von `https://github.com/GhostBeacon/takt/releases/latest/download/takt.bin`; Ablage auf SD macht der Launcher
- Getestet wird nur über diese Releases auf dem Gerät (kein USB-Flashen im Arbeitsablauf)
- **Größe klein halten** (aktuell ~0,53 MB): Der Launcher hält mehrere Apps gleichzeitig und legt die App-Partition passend zur Dateigröße an. Daher **kein WLAN/BT** in der Firmware (OTA macht der Launcher), keine eigene SPIFFS/FAT-Partition (Launcher würde ≥448 KB reservieren). Größe steht in der Workflow-Zusammenfassung.
- NVS (20 KB) wird mit Launcher, WLAN-Daten und anderen Apps geteilt: eigener Namespace `takt`, gepackt (1 Byte/Step) in kleinen Blobs `cfg` + `p0`…`p7` (~1,2 KB). Ein einzelner 3,2-KB-Blob schlug auf dem Gerät fehl (v1.0.1). Fehlermeldung zeigt freie Bytes (`nvs_get_stats`).

## Projektstruktur
```
platformio.ini
src/
  main.cpp      – Setup, UI-Loop, Tastatur, Display, Speichern
  engine.h      – Datenmodell, DSP (Stimmen), Sequencer, Audio-Render
  help.h        – Text der eingebauten Hilfe
  fonts.c/.h    – Pixelschrift Spleen 6x12 (u8g2-Format, aus Krähenauge)
.github/workflows/firmware.yml
docs/LICENSE-spleen.txt
CLAUDE.md
README.md       – Überblick, Installation, Kurzanleitung
ANLEITUNG.md    – ausführliche Anleitung (Inhalt deckungsgleich mit help.h halten)
LICENSE
```
Hardware-Test v1.0.1: Darstellung, Demo-Beat, Klang (keine Verzerrung), 250 BPM ohne Aussetzer, Tastatur, Parallelbetrieb mit 2 weiteren Apps ok. Speichern schlug fehl → Format umgestellt (s. o.).

## Ansichten
- **Startbildschirm** (2,6 s, jede Taste überspringt): „TAKT“ groß (Spleen ×4, Magenta-Schatten), eine Step-Reihe läuft durch, Version unten
- **Raster** (Hauptansicht)
- **Hilfe**: `?` (Shift+/) oder Esc (`` ` ``) öffnet; `;` `.` Zeile, `,` `/` Seite, Space Play/Stop, Esc/Del/Enter zurück

## Architektur
- **Audio-Task** auf Core 0 (Priorität hoch): rendert Blöcke à 256 Samples mono, 44,1 kHz, int16, und reicht sie per `M5.Speaker.playRaw(..., stop_current_sound=false)` in eine Warteschlange (3 Puffer rotierend). Der Sequencer-Takt ist **sample-genau** im Render-Loop, nicht über `millis()`.
- **UI-Loop** auf Core 1: Tastatur pollen, Display ~30 fps über einen `M5Canvas`-Sprite (240×135, 16 bit) zeichnen.
- Kommunikation UI → Audio nur über `volatile`-Flags / atomare Bitmasken (Play/Stop-Befehl, Pad-Trigger, Pattern-Wechsel). Keine Mutexe im Audio-Pfad, keine Allokationen, kein `Serial.print` im Audio-Task.
- Falls `M5.Speaker`-Streaming Knackser/Latenz zeigt: auf direktes I2S (ESP-IDF `i2s_std`) umstellen, ES8311 wird von M5Unified initialisiert.

## Klangerzeugung (8 Spuren, 808/909-artig)
| # | Spur | Synthese |
|---|------|----------|
| 1 | BD Kick | Sinus mit Pitch-Hüllkurve + kurzer Noise-Klick |
| 2 | SD Snare | Sinus-Body + hochpassgefiltertes Rauschen, zwei Hüllkurven |
| 3 | CP Clap | bandpassgefiltertes Rauschen, 3 Klatscher im 10-ms-Abstand + Fahne |
| 4 | CH Closed Hat | 6 Rechteck-Oszillatoren (808-Frequenzen) + Rauschen, 2× Hochpass |
| 5 | OH Open Hat | wie CH, lange Decay; **wird von CH abgewürgt (Choke)** |
| 6 | LT Low Tom | Sinus mit Pitch-Hüllkurve |
| 7 | HT High Tom | Sinus mit Pitch-Hüllkurve |
| 8 | CB Cowbell | 2 Rechtecke (540/800 Hz) durch Bandpass |

Pro Spur einstellbar: **Tune** (−12…+12 Halbtöne), **Decay** (0,1…3,0×), **Level** (0…10), **Mute**.
Master: Softclip-Sättigung, Lautstärke.

## Sequencer-Funktionen
- 8 Patterns × 8 Spuren × 16 Steps
- Pro Step: **An/Aus**, **Accent** (lauter), **Probability** (100/75/50/25 %), **Ratchet** (1–4 Schläge pro Step)
- **Polymeter**: eigene Länge 1–16 je Spur
- **Swing** 50–75 %
- **Tempo** 40–250 BPM
- Pattern-Wechsel während des Spielens wird **am Taktanfang** übernommen (gequeued, im Display als `P1>2`)
- **Euklid-Generator**: k Schläge gleichmäßig auf die Spurlänge verteilen
- **Zufallsgenerator** je Spur mit spurspezifischer Dichte
- Kopieren / Einfügen eines ganzen Patterns
- Speichern des gesamten Songs im NVS (`Preferences`, Namespace `takt`), Laden beim Start; Magic-Nummer prüfen, sonst Default-Song mit 2 Demo-Patterns

## Tastenbelegung
| Taste | Funktion |
|-------|----------|
| `1`–`8`, `Q`–`I` | Steps 1–16 der aktiven Spur an/aus |
| Shift + Step | Accent umschalten |
| Alt/Opt + Step | Probability durchschalten |
| Ctrl + Step | Ratchet 1→2→3→4 |
| `A S D F G H J K` | Spur wählen + vorhören |
| Fn + Spurtaste | Mute |
| Shift + Spurtaste | Solo (nochmal = Solo aus) |
| Fn + `1`–`8` | Pattern direkt wählen |
| `9` / `0` | Pattern − / + |
| Space | Play / Stop |
| `M` | aktive Spur vorhören |
| `Z` / `X` | Tune − / + |
| `C` / `V` | Decay − / + |
| `B` / `N` | Level − / + |
| `,` / `.` | BPM − / + (mit Shift ±10) |
| `[` / `]` | Swing − / + |
| `;` / `'` | Spurlänge − / + |
| `-` / `=` | Master-Lautstärke |
| `L` / Shift+`L` | Euklid: einen Schlag mehr / weniger |
| Tab | Spur zufällig füllen |
| `O` / `P` | Pattern kopieren / einfügen |
| Del / Fn+Del | Spur leeren / ganzes Pattern leeren |
| Enter | Song speichern |
| `?` / Esc | Hilfe |

Hinweis: Mit Shift liefert die Tastatur andere Zeichen (`!` statt `1` usw.) – diese auf die Grundtaste zurückmappen. Tastendrücke nur bei **Flanke** (neu gedrückt) auslösen, nicht bei gehaltener Taste.

## Display-Stil „A1 Kopflinie“ (Graphit + Neon-Noir-Akzente)
- Grundlage schwarz/grau: Steps grau `#8C8C8C`, Accent hell `#EDEDED`, Zellen `#171717`, Viertel `#262626`
- Akzente nur sparsam, Farben wie Krähenauge: Cyan `#05D9E8` (Linie unter Kopf- und über Fußzeile, Play-Symbol, Playhead), Magenta `#FF2A6D` (Pattern-Nummer, Kante an der aktiven Spur, Spurname in der Fußzeile)
- Schrift: Spleen 6x12

## Display-Layout (240×135)
- Kopfzeile (y 0–11, Linie y 12): Play/Stop-Symbol, Pattern, BPM, Swing, Lautstärke
- 8 Zeilen à 13 px: Spurname links, 16 Step-Zellen à 12×12 px
  - Viertel-Raster durch hellere Hintergrundzellen
  - Step an = grau, Accent = hell
  - Probability = kleineres Kästchen, Ratchets = schwarze Punkte im Step
  - Steps jenseits der Spurlänge nur als Umriss, Playhead als cyaner Rahmen (pro Spur, wegen Polymeter)
  - Gemutete Spuren grau
- Fußzeile (Linie y 119, Text ab y 122): Parameter der aktiven Spur oder kurze Statusmeldung (~1,4 s)

## Qualitätskriterien / Abnahme
1. `pio run` (lokal und im Workflow) baut ohne Fehler und Warnungen, die auf echte Bugs hinweisen.
2. Nach dem Flashen startet das Gerät, zeigt das Raster und spielt mit Space den Demo-Beat.
3. Kein hörbares Knacksen oder Aussetzer bei 8 gleichzeitig laufenden Spuren, 250 BPM und Ratchets.
4. Tastatur reagiert ohne spürbare Latenz, auch während der Wiedergabe; keine doppelten Auslösungen.
5. Pattern-Wechsel im Spiel exakt auf der Eins.
6. Speichern → Aus-/Einschalten → Song ist wieder da.
7. Flash/RAM-Nutzung im Build-Output prüfen (kein PSRAM!); Binary deutlich unter 1 MB halten.
8. Installation per Launcher-Favorit funktioniert, Takt läuft parallel installiert neben Krähenauge.

## Arbeitsweise für Claude Code
- Zuerst den Entwurf kompilieren und Fehler beheben, dann Features einzeln prüfen.
- API-Details von M5Unified/M5Cardputer im installierten Library-Quellcode (`.pio/libdeps/`) nachsehen statt zu raten – insbesondere `Keyboard_Class::KeysState` (Felder `word`, `fn`, `shift`, `ctrl`, `alt`/`opt`, `del`, `enter`, `tab`, `space`) und `Speaker_Class::playRaw` / `isPlaying`.
- DSP-Code im Audio-Pfad performant halten: Sinus über Lookup-Tabelle, keine `powf/expf` pro Sample (nur beim Triggern).
- Kleine, nachvollziehbare Commits. Kommentare und UI-Texte auf Deutsch. Umlaute in der UI sind erlaubt (Spleen enthält Latin-1, Quelltext UTF-8).
- README.md, ANLEITUNG.md und help.h bei jeder Änderung der Bedienung gemeinsam pflegen.

## Mögliche Erweiterungen (später)
- Song-Modus (Pattern-Kette)
- Pro-Step-Parameter-Locks (Tune/Decay je Step)
- Master-Effekte: Delay, Bitcrusher, Filter mit Resonanz
- IMU: Kippen steuert Filter oder Stutter/Beat-Repeat
- Sampler-Spur über das Mikrofon, Samples auf SD speichern
- Export/Import von Patterns auf microSD (JSON)
- MIDI-Clock-Sync über USB-MIDI oder Grove-UART
- Pattern-Fill-Taste (gehalten = Fill-Variante)
