# Takt

Eigene Firmware für den **M5Stack Cardputer ADV** (ESP32-S3FN8, 8 MB Flash, Tastatur-Chip TCA8418), gestartet über den [M5Launcher](https://github.com/bmorcelli/Launcher): eine **Drum Machine** mit 8 Spuren und 16-Step-Sequencer, komplett über die Tastatur spielbar. Alle Klänge werden in Echtzeit erzeugt – keine Samples, kein WLAN, keine Cloud. Ausgabe über Lautsprecher und 3,5-mm-Klinke.

Die Firmware ist bewusst klein (~0,5 MB), damit sie im Launcher neben anderen Apps installiert bleiben kann.

## Funktionen

- 8 Spuren im Stil von 808/909: **BD** Kick, **SD** Snare, **CP** Clap, **CH** Closed Hat, **OH** Open Hat (wird von CH abgewürgt), **LT** / **HT** Toms, **CB** Cowbell
- pro Spur: Tune (±12 Halbtöne), Decay, Level, Mute, Solo
- 8 Patterns × 16 Steps; pro Step: an/aus, **Accent**, **Wahrscheinlichkeit** (100/75/50/25 %), **Ratchet** (1–4 Schläge)
- **Polymeter**: eigene Länge 1–16 je Spur
- Tempo 40–250 BPM, Swing 50–75 %
- Pattern-Wechsel im Spiel wird **auf der Eins** übernommen (Anzeige `P1>2`)
- **Song-Modus**: Kette aus bis zu 16 Plätzen (Pattern × 1–8 Takte) läuft automatisch ab
- **Euklid-Generator**, **Zufallsgenerator**, Pattern kopieren/einfügen
- Song im internen Speicher sichern (Enter); beim Start wird er wieder geladen
- Startbildschirm und **eingebaute Hilfe** mit Tastenbelegung (`?` oder Esc)
- **Zubehör am Grove-Port:** Unit OLED als Zweitanzeige oder Unit Hex als Lichtorgel

Ausführliche Erklärung aller Funktionen: **[ANLEITUNG.md](ANLEITUNG.md)**

## Installieren

**Über WLAN mit dem M5Launcher:** Bei jedem Push auf `main` baut GitHub Actions die Firmware und legt ein Release an. Der Link auf die neueste Version bleibt immer gleich:

```
https://github.com/GhostBeacon/takt/releases/latest/download/takt.bin
```

Einmalig als Favorit eintragen: SD-Karte in den Rechner, `/config.conf` vorher sichern, dann in der Liste `"favorite": [ … ]` ergänzen (vom vorigen Eintrag mit Komma trennen):

```json
{
  "name": "Takt",
  "fid": "",
  "link": "https://github.com/GhostBeacon/takt/releases/latest/download/takt.bin"
}
```

`fid` muss leer bleiben, sonst sucht der Launcher im M5Burner-Katalog statt über den Link. Danach im Launcher: **OTA → Favorite List → Takt → Install**, künftige Updates genauso. Der Launcher kann die Datei dabei auch auf der SD-Karte ablegen.

**Von der SD-Karte:** `takt.bin` aus einem [Release](https://github.com/GhostBeacon/takt/releases) auf die Karte kopieren, im Launcher **SD** → Datei → **Install**.

Sind mehrere Apps installiert, startet eine Ziffer auf dem Launcher-Startbildschirm die jeweilige App direkt.

## Bedienung

| Taste | Funktion |
|---|---|
| Space | Play / Stop |
| `?` (Shift + `/`) oder Esc | Hilfe öffnen (`;` `.` blättern, Esc zurück) |
| `1`–`8`, `Q`–`I` | Steps 1–16 der aktiven Spur an/aus |
| Shift + Step | Accent |
| Opt/Alt + Step | Wahrscheinlichkeit 100 → 75 → 50 → 25 % |
| Ctrl + Step | Ratchet 1 → 2 → 3 → 4 |
| `A S D F G H J K` | Spur wählen und vorhören |
| Fn + Spurtaste | Mute |
| Shift + Spurtaste | Solo (nochmal = Solo aus) |
| `M` | aktive Spur vorhören |
| Fn + `1`–`8` | Pattern direkt wählen |
| `9` / `0` | Pattern − / + |
| `Z` / `X` | Tune − / + |
| `C` / `V` | Decay − / + |
| `B` / `N` | Level − / + |
| `,` / `.` | Tempo − / + (mit Shift ±10) |
| `[` / `]` | Swing − / + |
| `;` / `'` | Spurlänge − / + |
| `-` / `=` | Lautstärke − / + |
| `L` / Shift + `L` | Euklid: ein Schlag mehr / weniger |
| Tab | Spur zufällig füllen |
| `O` / `P` | Pattern kopieren / einfügen |
| Del / Fn + Del | Spur leeren / ganzes Pattern leeren |
| `\` | Song-Ansicht: Pattern-Kette bearbeiten, Tab = Song-Modus an/aus |
| Enter | Song speichern |

## Anzeige

- **Kopfzeile:** Play-Symbol, Pattern (Magenta, `P1>2` = Wechsel vorgemerkt; im Song-Modus `S2 P3 1/4` = Platz, Pattern, Takt), Tempo, Swing, Lautstärke
- **Raster:** 8 Spuren × 16 Steps; hellere Zellen markieren die Viertel. Grauer Step = an, heller Step = Accent, kleineres Kästchen = geringere Wahrscheinlichkeit, Punkte = Ratchets. Nur umrandete Zellen liegen jenseits der Spurlänge. Der cyane Rahmen ist die Abspielposition – je Spur, wegen Polymeter.
- **Aktive Spur:** Magenta-Kante links; gemutete Spuren dunkelgrau
- **Fußzeile:** Parameter der aktiven Spur oder eine kurze Meldung

Beim Einschalten erscheint ein kurzer Startbildschirm (~2,6 s, jede Taste überspringt ihn). Die Version steht dort und in der Hilfe (`1.0.<Build-Nr.>`, lokal gebaut: `dev`).

## Zubehör am Grove-Port

Am Grove-Port kann **eine** von zwei Erweiterungen stecken (immer nur eine):

- **Unit OLED** (SH1107, 1,3", 128×64): *Info* zeigt Pattern bzw. Song-Platz, Tempo und die Laufposition groß, *Pegel* je Spur einen Balken, der bei jedem Schlag hochspringt.
- **Unit Hex** (37 RGB-LEDs): *Lichtorgel* lässt jede Spur in ihrer Farbe und Zone aufblitzen (Kick in der Mitte, Hats außen), die Laufposition kreist am Rand; *Spur* zeigt die Steps der aktiven Spur im äußeren Ring.

| Taste | Funktion |
|---|---|
| Fn + `E` | Gerät neu erkennen (nach dem Umstecken) |
| Fn + `R` | Ansicht wechseln |
| Fn + `T` | Hex: Helligkeit (3 Stufen) · OLED: um 180° drehen |

Erkannt wird beim Start: Antwortet am Port ein OLED, wird es genutzt, sonst wird das Hex angesteuert. Ansicht, Helligkeit und Drehung bleiben gespeichert. Die Helligkeit des Hex ist bewusst begrenzt (höchstens 25 %), damit Akku und Grove-Port nicht überlastet werden. Die Ansteuerung steckt in [`lib/beiwerk`](lib/beiwerk) (Erkennung, OLED, Hex-Treiber).

## Bauen

```bash
python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/pip install platformio
~/.platformio/penv/bin/pio run
```

Ergebnis: `.pio/build/cardputer-adv/firmware.bin`. Serielle Ausgabe über USB: `~/.platformio/penv/bin/pio device monitor`.

## Lizenz

MIT, siehe [LICENSE](LICENSE). Die eingebettete Schrift Spleen 6x12 steht unter BSD-2-Clause, siehe [docs/LICENSE-spleen.txt](docs/LICENSE-spleen.txt).
