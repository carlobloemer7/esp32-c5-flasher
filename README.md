# ESP32-C5 Flasher (Flipper Zero FAP)

Flasht einen ESP32-C5 (getestet gegen WROOM-1U-Module) über die GPIO-Pins
des Flipper Zero, per selbst implementiertem ESP32-ROM-Bootloader-Protokoll
(SLIP-Framing über UART, kein Stub-Loader, kein `esptool.py` nötig).

**Erfolgreich gebaut** gegen die RogueMaster-Firmware `RM0722-1811-ff9f4feb`
(API-Version 88.0, Ziel `f7`/Flipper Zero) — Build lief fehler- und
warnungsfrei durch. Die fertige `esp32_c5_flasher.fap` liegt in `dist/` und
wurde bereits per USB auf einen angeschlossenen Flipper Zero installiert
und gestartet. Die vier Firmware-Presets (GhostESP/Marauder/FlipperHTTP/
Wardriver) sind ebenfalls schon direkt auf die SD-Karte kopiert (siehe
"Firmware-Presets" unten) — die App ist also sofort einsatzbereit.
**Ein echter Flash-Vorgang gegen einen ESP32-C5 wurde noch nicht getestet**
— das kann ich hier nicht, dafür fehlt der ESP32-C5 selbst. Die unten
gelisteten Laufzeit-Risiken (Protokoll-Timing, kein Chip-ID-Check, GPIO27)
bleiben also offen, bis das jemand am echten ESP32-C5 ausprobiert.

## Installieren

Am einfachsten per qFlipper oder dem Flipper-Mobile-App-Uploader:
`dist/esp32_c5_flasher.fap` auf den Flipper übertragen (Apps-Ordner, z.B.
`apps/GPIO/`), dort öffnen.

Alternativ per USB direkt aus dem Firmware-Repo heraus (falls du selbst
weiterbauen willst):

```sh
cd flipper_build/fw
./fbt fap_esp32_c5_flasher   # baut die App neu
./fbt launch APPSRC=applications_user/esp32_c5_flasher   # baut, installiert per USB, startet
```

Hinweis: `ufbt` wird von RogueMaster offiziell nicht unterstützt — für
dieses Projekt wurde daher das volle Firmware-Repo bei genau diesem Tag
geklont und mit `./fbt` gebaut (siehe "Build-Details" unten).

## Verkabelung

| Flipper-Pin | Funktion | ESP32-C5-Pin |
|---|---|---|
| Pin 13 (TX) | UART TX | RX (Chip-Empfang) |
| Pin 14 (RX) | UART RX | TX (Chip-Sendet) |
| Pin 7 (PC3) | EN-Steuerung (nur Auto-Reset-Modus) | EN / CHIP_PU |
| Pin 6 (PB2) | BOOT-Steuerung (nur Auto-Reset-Modus) | **GPIO28** (nicht GPIO0!) |
| GND | Masse | GND |

**Wichtig:** Der ESP32-C5 nutzt GPIO28 als Boot-Select-Strap, nicht GPIO0
wie klassische ESP32-Chips. GPIO27 muss beim Reset HIGH sein — die meisten
WROOM-1U-Module ziehen das über einen internen Pull-Up automatisch hoch;
falls der Auto-Reset unzuverlässig ist, in den Einstellungen den
"GPIO27-Assist"-Pin (Flipper Pin 4) aktivieren und zusätzlich mit GPIO27
verbinden.

- **Manueller Modus** (Standard): Nur TX/RX/GND verbinden, ESP32 von Hand in
  den Bootloader versetzen (BOOT halten, kurz RESET drücken, BOOT loslassen),
  dann in der App "Flashen starten" wählen.
- **Auto-Reset-Modus** (in Einstellungen aktivierbar): Zusätzlich EN und BOOT
  verbinden, die App übernimmt das Reset/Boot-Strapping selbst.

## Nutzung

**Flashen starten** öffnet die Firmware-Auswahl: **GhostESP**, **Marauder**,
**FlipperHTTP**, **Wardriver** oder **Eigene Dateien**. Die vier
vordefinierten Firmwares stecken **direkt in der `.fap`-Datei** (via
`fap_file_assets` im Manifest, siehe "Firmware-Presets" unten) und werden
vom Flipper-Betriebssystem **automatisch beim ersten App-Start** nach
`/ext/apps_assets/esp32_c5_flasher/firmwares/<preset>/` entpackt — du musst
dafür nichts tun, auch keine Internetverbindung (die der Flipper ohnehin
nicht hat). Bei einem Update der App (neue `.fap` mit geänderten
Firmware-Dateien) wird automatisch neu entpackt (Abgleich per
MD5-Signatur), bei unveränderten Dateien übersprungen.

**Eigene Dateien** nutzt stattdessen die manuelle Konfiguration unter
**Firmware verwalten** — dort landen deine eigenen `.bin`s ganz normal
beschreibbar unter `/ext/apps_data/esp32_c5_flasher/<kategorie>/`:

1. Für Bootloader, Partitionstabelle und Firmware/App jeweils eine
   `.bin`-Datei auswählen (aus `/ext/apps_data/esp32_c5_flasher/<kategorie>/`
   oder von woanders auf der SD-Karte — wird automatisch in den richtigen
   Ordner kopiert). OTA-Data ist optional.
2. Optional: **Eigene Binaries hinzufügen** für zusätzliche Dateien mit
   frei wählbarem Ziel-Offset (Hex-Eingabe).

**Einstellungen**: Auto-Reset an/aus, Baudrate, GPIO27-Assist.

Am Ende steht immer die Zusammenfassung (Confirm) mit allen aufgelösten
Dateien + Offsets, erst danach wird tatsächlich geflasht.

## Firmware-Presets (Quellen & Versionsstand)

Diese exakten Dateien wurden am 2026-07-27 heruntergeladen, anhand ihrer
ESP-Image-Magic-Bytes und Chip-ID (`0x17` = 23 = ESP32-C5, bei allen vier
Bootloadern identisch) verifiziert und liegen unter
`resources/firmwares/<preset>/` im App-Quellcode — der Build bettet sie via
`fap_file_assets="resources"` direkt in die `.fap` ein (ELF-Sektion
`.fapassets`, gepackt von `scripts/fbt/fapassets.py`). Verifiziert per
`storage.py list` auf dem Gerät: alle 12 Dateien wurden beim ersten
App-Start tatsächlich automatisch nach `apps_assets` entpackt.

| Preset | Quelle | Version |
|---|---|---|
| GhostESP | [GhostESP-Revival/GhostESP](https://github.com/GhostESP-Revival/GhostESP), Asset `esp32c5-generic-v01.zip` | `v2.1-pre7` |
| Marauder | [justcallmekoko/ESP32Marauder](https://github.com/justcallmekoko/ESP32Marauder), Bootloader/Partitionen aus `C5_Py_Flasher_for_adapter/bins/` | App `v1.14.0` (2026-07-21) |
| FlipperHTTP | [jblanked/FlipperHTTP](https://github.com/jblanked/FlipperHTTP), Ordner `ESP32-C5/` | `main` (Stand 2026-06-04, `v2.2.0`) |
| Wardriver | [justcallmekoko/ESP32DualBandWardriver](https://github.com/justcallmekoko/ESP32DualBandWardriver) | `v2.3.0` (2026-07-09) |

**Wichtig — unterschiedliche Bootloader-Offsets:** GhostESP und FlipperHTTP
flashen ihren Bootloader auf `0x0`, Marauder und Wardriver auf das
ESP-IDF-Standard-`0x2000`. Die App berücksichtigt das automatisch pro
Preset (`esp32/esp32_firmware_preset.c`, `preset_bootloader_offsets[]`) —
falls du die Dateien manuell aktualisierst, nicht versehentlich Bootloader
zwischen den Projekten mischen oder den Offset "vereinheitlichen", sonst
bootet der Chip nicht mehr.

**Zum Aktualisieren** auf neuere Releases: neue `.bin`-Dateien in
`resources/firmwares/<preset>/` im Projektordner ersetzen und die App neu
bauen (`./fbt fap_esp32_c5_flasher`) — beim nächsten Start auf dem Flipper
wird automatisch neu entpackt (MD5-Signatur-Abgleich erkennt die Änderung).

## Bekannte Risiken / worauf beim Testen achten

Alle reinen Compile-Risiken (Funktionssignaturen von `furi_hal_serial`,
`dialog_file_browser_show`, `widget_add_text_box_element`,
`byte_input_set_result_callback`, `storage_file_size`,
`variable_item_list_add` usw.) sind durch den erfolgreichen Build gegen die
echte SDK bereits ausgeräumt — das war vorher die größte Unsicherheit.
Offen sind nur noch **Laufzeit**-Risiken, die sich erst am echten Gerät zeigen:

1. **Keine Chip-ID-Prüfung**: Die App verifiziert nicht, dass wirklich ein
   C5 antwortet — sie vertraut auf ein erfolgreiches SYNC und flasht dann.
2. **Reset-Timing** (100ms/50ms) ist von esptool.py übernommen, nicht C5-
   spezifisch verifiziert — bei unzuverlässigem Auto-Reset zuerst hier
   ansetzen (`esp32/esp32_gpio_strap.c`, `ESP32_STRAP_*_HOLD_MS`).
3. **GPIO27** wird standardmäßig nicht aktiv angesteuert (siehe Verkabelung
   oben) — potenzielle Fehlerquelle bei Auto-Reset, falls das Modul es nicht
   zuverlässig selbst hochzieht.
4. SLIP-Timeouts/Blockgrößen sind nach Spezifikation implementiert, aber nie
   gegen reale Latenzen der Flipper-UART getestet worden.

## Testreihenfolge

1. `dist/esp32_c5_flasher.fap` auf den Flipper kopieren und öffnen
2. Nur TX/RX/GND, manueller Modus: eine kleine Test-`.bin` (z.B. nur
   Bootloader) flashen, SYNC/Schreib-Erfolg prüfen
3. Kompletten Satz (Bootloader+Partition+App) eines echten ESP-IDF-Builds
   flashen, danach normalen Boot verifizieren
4. Auto-Reset-Modus mit allen 4 Datenpins verbunden testen

## Build-Details

Gebaut am 2026-07-27 gegen
[RogueMaster/flipperzero-firmware-wPlugins](https://github.com/RogueMaster/flipperzero-firmware-wPlugins)
Tag `RM0722-1811-ff9f4feb` (Branch `420`, Commit `fe40ed2`), API-Version 88.0,
Zielplattform `f7` (Flipper Zero). Quelle liegt zusätzlich unter
`applications_user/esp32_c5_flasher/` im vollständigen Firmware-Repo unter
`~/flipper_build/fw/`, falls du von dort aus weiterbauen/debuggen willst.

Bei Laufzeitfehlern gerne die genaue Fehlermeldung/das Verhalten am Gerät
zurückmelden — dann fixe ich gezielt.
