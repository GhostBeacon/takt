# Takt – Anleitung

Takt ist eine Drum Machine mit 8 Spuren und 16 Steps. Alle Klänge werden im Gerät erzeugt. Die Kurzfassung dieser Anleitung ist auch auf dem Cardputer eingebaut: **`?`** (Shift + `/`) oder **Esc** öffnet sie.

## Erste Schritte

1. Takt im M5Launcher starten. Der Startbildschirm verschwindet nach gut zwei Sekunden oder mit einer beliebigen Taste.
2. **Space** drücken – der Demo-Beat läuft.
3. Mit **`A`** … **`K`** eine Spur wählen (z. B. `S` für die Snare). Die Spur ist links mit einer Magenta-Kante markiert und klingt einmal an.
4. Mit **`1`–`8`** und **`Q`–`I`** die 16 Steps dieser Spur an- und ausschalten.
5. **Enter** speichert alles dauerhaft.

## Die Anzeige

```
 ▶ P1   120 BPM  SW54%          VOL63    ← Kopfzeile
 ─────────────────────────────────────
 BD  ■ · · · ■ · · · ■ · ▪ · ■ · · ·      ← 8 Spuren × 16 Steps
▌SD  · · · · ■ · · · · · · · ■ · · ·      ← aktive Spur (Magenta-Kante)
 …
 ─────────────────────────────────────
 SD  Tune+0  Dec1.0  Lvl8  Len16          ← Fußzeile
```

| Element | Bedeutung |
|---|---|
| ▶ (cyan) / ■ (grau) | läuft / gestoppt |
| `P1` | aktuelles Pattern; `P1>2` = Wechsel zu Pattern 2 ist vorgemerkt |
| `SW54%` | Swing |
| `VOL63` | Lautstärke in Prozent |
| hellere Zellen | Viertel (Step 1, 5, 9, 13) |
| grauer Step | Step spielt |
| heller Step | Step spielt mit **Accent** (lauter) |
| kleineres Kästchen | Step spielt nur mit einer **Wahrscheinlichkeit** |
| schwarze Punkte im Step | **Ratchet**: mehrere schnelle Schläge |
| nur Umriss | liegt hinter dem Ende der Spur (Spurlänge < 16) |
| cyaner Rahmen | aktuelle Abspielposition, je Spur |
| dunkelgraue Spur | gemutet |
| Fußzeile | Werte der aktiven Spur oder eine kurze Meldung |

## Die 8 Spuren

| Taste | Spur | Klang |
|---|---|---|
| `A` | **BD** Bass Drum | Sinus mit fallender Tonhöhe und Klick |
| `S` | **SD** Snare | Ton plus gefiltertes Rauschen |
| `D` | **CP** Clap | drei kurze Klatscher und eine Fahne |
| `F` | **CH** Closed Hat | metallisch, kurz |
| `G` | **OH** Open Hat | metallisch, lang – wird von der Closed Hat abgewürgt |
| `H` | **LT** Low Tom | tiefe Tom |
| `J` | **HT** High Tom | hohe Tom |
| `K` | **CB** Cowbell | zwei Töne durch einen Bandpass |

Jede Spur hat eigene Werte, die in der Fußzeile stehen:

- **Tune** (`Z` / `X`): Tonhöhe in Halbtönen, −12 bis +12
- **Decay** (`C` / `V`): Ausklinglänge, 0,1× bis 3,0×
- **Level** (`B` / `N`): Lautstärke der Spur, 0 bis 10
- **Len** (`;` / `'`): Spurlänge, 1 bis 16 Steps

## Steps bearbeiten

| Taste | Wirkung |
|---|---|
| Step-Taste | an / aus |
| Shift + Step | Accent an / aus (schaltet einen leeren Step direkt mit Accent ein) |
| Opt + Step | Wahrscheinlichkeit 100 → 75 → 50 → 25 % |
| Ctrl + Step | Ratchet 1 → 2 → 3 → 4 Schläge |

Wahrscheinlichkeit und Ratchet wirken nur auf Steps, die an sind. Ein ausgeschalteter Step verliert beide Einstellungen.

## Spuren steuern

| Taste | Wirkung |
|---|---|
| `A`–`K` | Spur wählen und anhören |
| `M` | aktive Spur nochmal anhören |
| Fn + Spurtaste | Mute an / aus |
| Shift + Spurtaste | Solo: nur diese Spur hören; nochmal = alle wieder an |
| Del | aktive Spur leeren |
| Tab | aktive Spur zufällig füllen (Dichte passend zur Spur, z. B. viele Hats, wenige Toms) |
| `L` | Euklid: einen Schlag mehr |
| Shift + `L` | Euklid: einen Schlag weniger |

**Euklid** verteilt die Schläge so gleichmäßig wie möglich über die Spurlänge. 3 Schläge auf 8 Steps ergeben z. B. den bekannten Tresillo-Rhythmus. Die bisherigen Steps der Spur werden dabei ersetzt.

## Polymeter

Jede Spur kann eine eigene Länge haben (`;` / `'`). Ist die Cowbell z. B. 7 Steps lang und der Rest 16, wiederholt sich die Cowbell früher und verschiebt sich gegen die anderen Spuren – der Rhythmus wiederholt sich erst nach 112 Steps. Der cyane Rahmen zeigt die Position jeder Spur einzeln.

Bei einem Pattern-Wechsel starten alle Spuren wieder gemeinsam auf Step 1.

## Patterns

Takt hat 8 Patterns. Gespielt wird immer eines.

| Taste | Wirkung |
|---|---|
| Fn + `1`–`8` | Pattern direkt wählen |
| `9` / `0` | vorheriges / nächstes Pattern |
| `O` | Pattern kopieren |
| `P` | kopiertes Pattern ins aktuelle einfügen |
| Fn + Del | ganzes Pattern leeren |

Läuft die Wiedergabe, wird der Wechsel erst **auf der nächsten Eins** übernommen – so bleibt der Groove im Takt. Bis dahin steht oben z. B. `P1>2`.

Tipp: Pattern kopieren (`O`), zum nächsten Pattern wechseln (`0`), einfügen (`P`) und dort eine Variation bauen.

## Tempo, Swing, Lautstärke

| Taste | Wirkung |
|---|---|
| `,` / `.` | Tempo − / + (40–250 BPM); mit Shift in 10er-Schritten |
| `[` / `]` | Swing − / + (50–75 %) |
| `-` / `=` | Lautstärke − / + |

**Swing** verzögert jeden zweiten Sechzehntel-Step. 50 % ist gerade, um 58 % klingt es leicht shuffelnd, 66 % ist eine Triole.

## Speichern

**Enter** speichert alle 8 Patterns, die Spurwerte, Tempo, Swing und Lautstärke im internen Speicher des Cardputers. Beim nächsten Start wird der Song automatisch geladen. Nicht gespeicherte Änderungen gehen beim Ausschalten verloren.

Der interne Speicher wird mit dem M5Launcher und anderen Apps geteilt. Takt braucht davon nur gut 1 KB. Erscheint trotzdem **„Fehler: Speicher voll“** oder **„Fehler beim Speichern“**, zeigt die Meldung, wie viel Platz noch frei war – bitte diesen Wert mit einem Issue melden.

## Die eingebaute Hilfe

`?` (Shift + `/`) oder **Esc** öffnet die Hilfe.

| Taste | Wirkung |
|---|---|
| `;` / `.` | eine Zeile hoch / runter |
| `,` / `/` | eine Seite zurück / vor |
| Space | Play / Stop (läuft weiter, während die Hilfe offen ist) |
| Esc, Del oder Enter | zurück zum Raster |
